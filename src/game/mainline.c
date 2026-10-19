/* mainline.c - BZONE.MAC: the main loop (MAIN), the cracked windshield
 * (WNSHLD), the logo (BATTLE), the high score pages (CKSCOR, HISCRE,
 * LETCHK, DISTBL), messages (MSGS) and the score line (INFO).
 *
 * A frame: wait for the interrupt's sync (every 16th interrupt, 15.4 times
 * a second), move the volcano's rocks, wait for a display buffer, then
 * write the whole picture into it (window, mountains, every object in
 * view through the math box, radar, sights, messages), hand the buffer
 * back (FINISH) and move the game one step (shells, collisions, the enemy,
 * the saucer, the player). */
#include "game/game.h"
#include "game/m6502.h"

/* Messages, by their number (X for MSGS). */
enum {
    ETO = 0x00, LEFT = 0x02, RIGHT = 0x04, REAR = 0x06, LINE2 = 0x08, LINE3 = 0x0a, LINE4 = 0x0c, CHISCR = 0x0e,
    ERANGE = 0x10, BLOCKD = 0x12, GAMOVR = 0x14, PRSTRT = 0x16, YSCORE = 0x18, HISCOR = 0x1a, ZEROS = 0x1c,
    LINE1 = 0x1e, MODE1 = 0x20, MODE2 = 0x22, MODE3 = 0x24, CONPLY = 0x26, INSCON = 0x28, BONPLN = 0x2a,
    BONPL1 = 0x2c, COPYRT = 0x2e
};

#define PL1FIR 0x10     /* PLAYER 1 FIRE, in ALLPOT */
#define LOX    0x02     /* the loud explosion, in SNDWRD */

static void main_body(bz_machine *m);
static void wnshld(bz_machine *m);
static void ckscor(bz_machine *m);
static void hiscre(bz_machine *m);
static void letchk(bz_machine *m);
static void distbl(bz_machine *m);
static void dspalp(bz_machine *m);
static void finish(bz_machine *m);
static void bigwnd(bz_machine *m);
static void info(bz_machine *m);

/* --- the frame ---------------------------------------------------------------- */

void bz_frame(bz_machine *m)
{
    SEC();                      /* (the carry MAIN's LSR SYNC shifted out) */
    bz_volcno(m);
    LDX(RD(BZ_BUFRDY));         /* USE AVAILABLE BUFFER */
    main_body(m);
    m->frames++;
}

void bz_picture(bz_machine *m, uint8_t buffer)
{
    m->draw_only = 1;
    SEC();
    bz_volcno(m);
    LDX(buffer);
    main_body(m);
    m->draw_only = 0;
}

int bz_main_ready(bz_machine *m)
{
    if (m->main_state == BZ_MAIN_SYNC) {
        for (;;) {
            if (RD(BZ_SYNC) == 0) return 0;
            LSR(BZ_SYNC);       /* SYNC UP */
            if (BCS) break;
        }
        bz_volcno(m);
        m->main_state = BZ_MAIN_BUFFER;
    }
    return RD(BZ_BUFRDY) != 0;
}

void bz_main_poll(bz_machine *m)
{
    if (m->main_state == BZ_MAIN_SYNC) {
        /* MAIN: LSR SYNC, BCC MAIN */
        for (;;) {
            if (RD(BZ_SYNC) == 0) return;
            LSR(BZ_SYNC);       /* SYNC UP */
            if (BCS) break;
        }
        bz_volcno(m);
        m->main_state = BZ_MAIN_BUFFER;
    }
    LDX(RD(BZ_BUFRDY));         /* USE AVAILABLE BUFFER */
    if (BEQ) return;
    main_body(m);
    m->frames++;
    m->main_state = BZ_MAIN_SYNC;
}

static void main_body(bz_machine *m)
{
    LDA((BZ_VECRAM & 0xff) + 2);
    STA(BZ_VGLIST);
    STX(BZ_VGLIST + 1);
    m->tag = BZ_TAG(BZ_TAG_WINDOW, 0);
    LDA(1);
    bz_vgsca1(m);               /* SET SCALING */
    bz_vgcntr(m);
    LDA(0x81);                  /* -127: SET LOWER WINDOW */
    TAX();
    LDY(0);
    bz_vgvtr(m);
    LDA(0);
    bz_vgstat(m);
    INC(BZ_FRAME);
    if (BNE) goto l6;
    INC(BZ_TIMOUT);
    LDA(RD(BZ_TIMOUT));
    CMP(4);
l6:
    BIT(RD(BZ_GOVER));
    if (BPL) goto l1;
    bigwnd(m);
    hiscre(m);
    return;
l1:
    BIT(RD(BZ_ATRACT));
    if (BMI) goto l3;
    LDA(RD(BZ_CRACK));          /* ALLOW ENDING SEQUENCE TO FINISH */
    if (BNE) goto l3;
    if (BCC) goto l10;
    bz_tankck(m);
    LDX(3);
    LDA(RD(BZ_DSPLAY));
    EOR(0x80);
    STA(BZ_DSPLAY);
    STX(BZ_TIMOUT);
    STA(BZ_FRAME);
    if (BMI) goto l10;          /* IF GOING INTO DISPLAY */
    BIT(RD(BZ_ATRACT));         /* CLOSE WINDOW */
    if (BMI) goto l15;
    LDA(0xff);                  /* SET UP FOR LOGO */
    STA(BZ_LOGO);
l10:
    BIT(RD(BZ_DSPLAY));
    if (BPL) goto l15;
    bigwnd(m);
    distbl(m);
    return;
l3:
    if (BCC) goto l15;
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l15;
    bz_r2d3ck(m);
l15:
    bz_vgcntr(m);               /* SET UPPER WINDOW */
    LDA(127);
    LDX(48);
    LDY(0);
    bz_vgvtr(m);
    LDA(2);
    bz_vgstat(m);
    BIT(RD(BZ_ATRACT));         /* IF GAME */
    if (BMI) goto l16;
    LDX(COPYRT);                /* COPYRIGHT 1980 */
    bz_msgs(m);
l16:
    bz_mounts(m);               /* DRAW MOUNTAINS & HORIZON */
    DEC(BZ_ACTION);
    LDA(RD(BZ_FTIMER));
    CMP(0xff);
    if (BEQ) goto l18;
    INC(BZ_FTIMER);
l18:
    LDA(10);                    /* INIT DIVIDER */
    STA(BZ_DIVCYC);
    LDA(0);                     /* INIT INDEX */
    STA(BZ_SAVE0);
    bz_rotate(m);               /* CHOOSE OBJECTS IN VIEW */
    bz_battle(m);
l20:
    LDX(RD(BZ_SAVE0));          /* NEXT TABLE VALUE */
    LDA(RD(BZ_PTBLO2 + X));     /* IF END OF TABLE */
    if (BMI) goto l30;
    m->tag = BZ_TAG(BZ_TAG_OBJECT, m->vis_id[(X >> 1) & 31]);
    bz_pntput(m);
    LDX(RD(BZ_SAVE0));          /* GET OBJECT # IN A */
    LDA(RD(BZ_PTBLO2 + X));
    bz_draw(m);                 /* GENERATE VECTOR INSTRUCTIONS */
    INC(BZ_SAVE0);              /* NEXT OBJECT */
    INC(BZ_SAVE0);
    if (BNE) goto l20;
l30:
    LDA(RD(BZ_CRACK));
    if (BEQ) goto l31;
    wnshld(m);
    goto b_end;
l31:
    bigwnd(m);                  /* OPEN WINDOW */
    bz_dradar(m);
    bz_track(m);                /* IF ROBOT IN SIGHTS */
    EOR(0x80);
    SEC();
    SBC(RD(BZ_TANGLE));
    STA(BZ_SAVE2);
    if (BPL) goto l41;
    CLC();
    EOR(0xff);
    ADC(1);
l41:
    STA(BZ_PTURN);
    BIT(RD(BZ_LOGO));
    if (BMI) goto l50;
    CMP(2);
    LDA(RD(BZ_FIRECT));
    if (BEQ) goto l42;
    LDA(0x20);
    AND(RD(BZ_INTCT));
    if (BEQ) goto l50;
l42:
    if (BCC) goto l45;
    m->tag = BZ_TAG(BZ_TAG_CROSS, 0);
    LDX(BZ_XCROSS & 0xff);
    LDA(BZ_XCROSS >> 8);
    goto l48;
l45:
    m->tag = BZ_TAG(BZ_TAG_CROSS, 1);
    LDX(BZ_XCROS1 & 0xff);
    LDA(BZ_XCROS1 >> 8);
l48:
    bz_vgjsrl(m);
l50:
    LDA(RD(BZ_COLFLG + 2));
    if (BNE) goto l53;
    BIT(RD(BZ_ATRACT));         /* IF NOT ATTRACT */
    if (BMI) goto l51;
    LDA(RD(BZ_VGLIST + 1));     /* SECURITY */
    AND(0xf8);
    STA(BZ_TBLPTR + 1);
    LDA(2);
    STA(BZ_TBLPTR);
    LDY(0x35);
    LDA(0);
    do {
        EOR(RD(IY(BZ_TBLPTR)));
        DEY();
    } while (BPL);
    LSRA();
    STA(BZ_TRAP2);
    BIT(RD(BZ_LOGO));           /* IF SHOWING LOGO */
    if (BMI) goto b_end;
l51:
    LDA(2);                     /* FLASH MSG */
    AND(RD(BZ_FRAME));
    if (BEQ) goto l53;
    LDA(RD(BZ_PTURN));          /* IF ROBOT IN VIEW */
    CMP(22);
    if (BCC) goto l53;
    LDX(ETO);                   /* POINT DIRECTION */
    bz_msgs(m);
    LDX(REAR);
    LDA(0x6b);
    CMP(RD(BZ_PTURN));
    if (BCC) goto l52;
    LDX(LEFT);
    BIT(RD(BZ_SAVE2));
    if (BPL) goto l52;
    LDX(RIGHT);
l52:
    bz_msgs(m);
l53:
    LDA(RD(BZ_OBJCOL));
    if (BEQ) goto b_end;
    LDA(RD(BZ_FRAME));
    AND(4);
    if (BNE) goto b_end;
    LDX(BLOCKD);
    bz_msgs(m);

b_end:
    info(m);                    /* DISPLAY STATS */
    LDA(RD(BZ_GOVER));          /* IF IN GAME */
    if (BEQ) goto e15;
    LDA(RD(BZ_CRACK));
    if (BNE) goto e15;
    BIT(RD(BZ_LOGO));           /* IF LOGO */
    if (BMI) goto e15;
    LDX(GAMOVR);                /* OUTPUT GAME OVER */
    bz_msgs(m);
    LDA(RD(BZ_CRDT));           /* IF NO COINS NEEDED */
    if (BNE) goto e10;
    LDA(RD(BZ_CMODE));
    AND(3);
    if (BEQ) goto e10;
    ASLA();                     /* OUTPUT COIN MODE */
    ADC(MODE1 - 2);
    TAX();
    bz_msgs(m);
    LDX(CONPLY);
    bz_msgs(m);
    LDX(INSCON);                /* INSERT COIN */
    goto e13;
e10:
    LDX(PRSTRT);
e13:
    BIT(RD(BZ_INTCT));
    if (BVC) goto e15;
    bz_msgs(m);
e15:
    finish(m);
    if (m->draw_only) return;   /* (the picture only: render/smooth.h) */
    BIT(RD(BZ_SUPER));          /* IF SUPER BONUS */
    if (BMI) goto e30;
    LDA(4);
    STA(BZ_SAVE3);
    do {
        bz_colchk(m);           /* CHECK COLLISIONS */
        bz_shupdt(m);
        DEC(BZ_SAVE3);
    } while (BNE);
    BIT(RD(BZ_R2D3FL));         /* IF SLOW TANK, DON'T DO QWIKCK */
    if (BMI) goto e17;
    bz_tr7chk(m);
    if (BCC) goto e19;
e17:
    bz_qwikck(m);
e19:
    bz_robot(m);
    bz_saucmv(m);
    bz_bound(m);
    bz_motion(m);               /* UPDATE TANK POSITION */
    return;                     /* NEXT MOVE */
e30:
    LDA(RD(BZ_TRAP2));
    LSRA();
    ORA(RD(BZ_POINT));
    if (BNE) return;
    STA(BZ_SUPER);
    LDA(RD(BZ_SNDWRD));
    ORA(LOX);
    STA(BZ_SNDWRD);
    LDA(0xff);
    STA(BZ_EXPCNT);
}

/* BIGWND: SET UPPER WINDOW, the whole screen. */
static void bigwnd(bz_machine *m)
{
    m->tag = BZ_TAG(BZ_TAG_WINDOW, 0);
    bz_vgcntr(m);
    LDY(0);
    LDA(127);
    TAX();
    bz_vgvtr(m);
    LDA(2);
    bz_vgstat(m);
}

/* WNSHLD: the windshield cracks, a piece more each frame; when it is all
 * there and the shell has gone, the game goes on: over (to the high
 * scores), or with the player put somewhere else. */
static void wnshld(bz_machine *m)
{
    m->tag = BZ_TAG(BZ_TAG_CRACK, 0);
    LDY(0);                     /* INIT INDEX */
    STY(BZ_CHAN4V);
l10:
    LDA(RD(BZ_CRACKS + Y));     /* OUTPUT ONE SECTION */
    LDX(RD(BZ_CRACKS + 1 + Y));
    STY(BZ_TEMP1);
    bz_vgadd2(m);
    LDY(RD(BZ_TEMP1));
    INY();
    INY();
    CPY(RD(BZ_CRACK));          /* IF DONE */
    if (BCS) goto l20;
    CPY(0x10);                  /* IF NOT FULL PICTURE */
    if (BCC) goto l10;
    LDA(0x2c);                  /* IF NOT DONE COMPLETELY */
    CMP(RD(BZ_CRACK));
    if (BCS) goto l20;
    LDA(RD(BZ_FIRECT));
    if (BNE) return;
    STA(BZ_CRACK);              /* RESET CRACK */
    STA(BZ_STATE);
    STA(BZ_COLFLG);
    STA(BZ_FTIMER);
    LDX(RD(BZ_GOVER));
    if (BEQ) goto l13;
    STA(BZ_ATRACT);             /* SET TO ATTRACT */
    STA(BZ_OBJCOL);
    ckscor(m);
    return;
l13:
    LDA(0x30);
    STA(BZ_ACTION);
    LDA(RD(BZ_PRAND));          /* RESET POSITION */
    STA(BZ_RGOAL);
    STA(BZ_TPOSX);
    LDA(RD(BZ_PRAND));
    STA(BZ_TPOSY);
    do {
        LDA(RD(BZ_PRAND));
        AND(0x3f);
        STA(BZ_TPOSX + 1);
        LDA(RD(BZ_PRAND));
        STA(BZ_TPOSY + 1);
        LDX(0);
        bz_objobj(m);           /* IF IN COLLISION */
    } while (BCS);
    STX(BZ_OBJCOL);             /* MAKE SURE NOT IN OBJECT COLLISION */
    STX(BZ_EIRNGE);
    LDA(RD(BZ_PRAND));          /* RESET ANGLE */
    STA(BZ_TANGLE);
    BIT(RD(BZ_R2D3FL));         /* IF BLOWN UP BY TANK */
    if (BPL) goto l17;
    bz_tankck(m);
    return;                     /* INSURE TANK */
l17:
    LDA(RD(BZ_COLFLG + 2));     /* IF OTHER TANK NOT BLOWN UP */
    if (BNE) return;
    bz_robchk(m);               /* MOVE TANK IN FRONT OF HIM */
    return;
l20:
    INC(BZ_CRACK);              /* INCREMENT CRACK */
    INC(BZ_CRACK);
}

/* --- the logo ------------------------------------------------------------------- */

/* BATTLE: HANDLE LOGO DISPLAY. In the attract mode the three parts of the
 * BATTLE ZONE logo are added to the table of objects in view, rising from
 * below and receding. */
void bz_battle(bz_machine *m)
{
    BIT(RD(BZ_ATRACT));
    if (BMI) return;
    LDA(RD(BZ_VGLIST + 1));
    AND(0xf8);
    STA(BZ_TBLPTR + 1);
    LDA(2);
    STA(BZ_TBLPTR);
    LDY(0x35);
    LDA(0);
    SEC();
    do {
        SBC(RD(IY(BZ_TBLPTR)));
        DEY();
    } while (BPL);
    SEC();
    SBC(RD(BZ_OBJTBL + 0x21));
    STA(BZ_TRAP1);
    BIT(RD(BZ_LOGO));           /* IF LOGO */
    if (BPL) return;
    LDA(RD(BZ_LOGOZ + 1));      /* IF DONE WITH SEQUENCE */
    if (BMI) goto l13;
    CMP(2);
    if (BCC) goto l13;
    bz_batint(m);
    return;
l13:
    LDX(0);                     /* FIND NEXT SPOT IN TABLE */
    for (;;) {
        LDA(RD(BZ_PTBLO2 + X));
        if (BMI) break;
        INX();
        INX();
        if (BEQ) break;
    }
    LDA(0xf0);                  /* SET BRITENESS */
    STA(BZ_SINT);
    LDY(0);
l22:
    LDA(RD(BZ_LOGOBJ + Y));     /* SET UP OBJ #,X,Y */
    STA(BZ_PTBLO2 + X);
    m->vis_id[(X >> 1) & 31] = (uint8_t)(BZ_ID_LOGO + (Y >> 1));
    LDA(RD(BZ_LOGOX));
    STA(BZ_PTBLX2 + X);
    LDA(0);
    STA(BZ_PTBLY2 + X);
    INX();
    INY();
    LDA(RD(BZ_TANGLE));         /* USE TANKS ANGLE */
    STA(BZ_PTBLO2 + X);
    LDA(RD(BZ_LOGOX + 1));
    STA(BZ_PTBLX2 + X);
    LDA(0);
    STA(BZ_PTBLY2 + X);
    INX();
    INY();
    LDA(RD(BZ_LOGOZ + 1));      /* THE ULTIMATE KLUDGE, IF LOGO HIGH ENOUGH TO BRING OUT "ZONE" */
    if (BPL) goto l90;
    CMP(0xfd);
    if (BCS) goto l90;
    LDA(RD(BZ_LOGOZ));
    CMP(0xb0);
    if (BCS) goto l90;
    CPY(3);                     /* DON'T DRAW "ZONE" UNTIL WE HAVE ENOUGH TIME */
    goto l95;
l90:
    CPY(5);
l95:
    if (BCC) goto l22;
    LDA(RD(BZ_LOGOZ));          /* SET UP Z */
    LDY(RD(BZ_LOGOZ + 1));
    STA(BZ_EXPOSZ + 0x0c);
    STY(BZ_EXPOSZ + 0x0d);
    STA(BZ_EXPOSZ + 0x0e);
    STY(BZ_EXPOSZ + 0x0f);
    CLC();                      /* UPDATE Z */
    ADC(8);
    STA(BZ_LOGOZ);
    if (BCS) INY();
    STY(BZ_LOGOZ + 1);
    CLC();
    LDY(RD(BZ_LOGOX + 1));      /* UPDATE DISTANCE */
    LDA(RD(BZ_LOGOX));
    ADC(0x40);
    STA(BZ_LOGOX);
    if (BCS) INY();
    STY(BZ_LOGOX + 1);
    LDA(0xff);                  /* END TABLE */
    STA(BZ_PTBLO2 + X);
}

/* BATINT: the logo's start. */
void bz_batint(bz_machine *m)
{
    LDA(0xfc);                  /* INIT POSITION */
    STA(BZ_LOGOZ + 1);
    LDA(0);
    STA(BZ_LOGOX);
    STA(BZ_LOGOZ);
    STA(BZ_LOGO);
    LDA(4);
    STA(BZ_LOGOX + 1);
    LDA(3);                     /* TEMPORARY */
    STA(BZ_TIMOUT);
    LDA(0);
    STA(BZ_DSPLAY);
    STA(BZ_FRAME);
}

/* --- high scores ---------------------------------------------------------------- */

/* CKSCOR: the game is over: IF GOOD ENOUGH TO MAKE TABLE, the others move
 * down and the entry of initials begins. Then on with the frame (B.END). */
static void ckscor(bz_machine *m)
{
    LDX(0);
    STX(BZ_TIMOUT);             /* RESET TIMER */
    STX(BZ_DSPLAY);
    do {
        LDY(RD(BZ_HSCTBL + X)); /* IF GOOD ENOUGH TO MAKE TABLE */
        CPY(RD(BZ_HITS));
        LDA(RD(BZ_HSCTBL + 1 + X));
        SBC(RD(BZ_HITS + 1));
        if (BCC) goto l20;
        INX();
        INX();
        INX();
        CPX(BZ_HSCNUM * 3);     /* IF NOT DONE */
    } while (BCC);
    return;                     /* NOT GOOD ENOUGH */
l20:
    LDA(0);                     /* RESET TIME OUT */
    STA(BZ_FRAME);
    STA(BZ_LETR);
    STX(BZ_SAVX);
    STA(BZ_OBJCOL);             /* KILL ANY CONTINUOUS SOUNDS */
    STA(BZ_SCOLFG);
    STA(BZ_SAUCER);
    LDA(BZ_S_SUPBON);           /* PLAY THE TUNE FOR THE TURKEY */
    bz_sndon(m);
    LDA(RD(BZ_GOVER));          /* SET HI SCORE MODE */
    ORA(0x80);
    STA(BZ_GOVER);
    LDX((BZ_HSCNUM - 1) * 3);
    for (;;) {
        CPX(RD(BZ_SAVX));       /* MOVE SCORES & INITIALS DOWN */
        if (BEQ) break;         /* IF DONE */
        LDA(RD(BZ_HSCTBL - 3 + X));
        STA(BZ_HSCTBL + X);
        LDA(RD(BZ_HSCTBL - 2 + X));
        STA(BZ_HSCTBL + 1 + X);
        LDA(RD(BZ_INITLS - 3 + X));
        STA(BZ_INITLS + X);
        LDA(RD(BZ_INITLS - 2 + X));
        STA(BZ_INITLS + 1 + X);
        LDA(RD(BZ_INITLS - 1 + X));
        STA(BZ_INITLS + 2 + X);
        DEX();
        DEX();
        DEX();
        if (BMI) break;
    }
    LDA(RD(BZ_HITS));           /* INSERT NEW SCORE IN TABLE */
    STA(BZ_HSCTBL + X);
    LDA(RD(BZ_HITS + 1));
    STA(BZ_HSCTBL + 1 + X);
    LDA(4);
    STA(BZ_LTIMER);
    LDA(0x16);                  /* SET UP INITIALS */
    STA(BZ_STINTL);
    LDA(0x4c);
    STA(BZ_STINTL + 1);
    STA(BZ_STINTL + 2);
}

/* HISCRE: the page on which the initials are entered. Ends the frame. */
static void hiscre(bz_machine *m)
{
    LDA(RD(BZ_TIMOUT));
    CMP(4);
    if (BCS) goto l60;          /* IF TIMOUT */
    LDA(RD(BZ_POINT));          /* SEE IF OVERTURE OVER (TURE) */
    if (BNE) goto l10;
    LDX(RD(BZ_HITS + 2));       /* IF EPLO DONE ALREADY ( CHEAP TRICK ) */
    if (BEQ) goto l10;
    STA(BZ_HITS + 2);           /* MAKE CHEAP TRICK VIABLE */
    LDA(2);
    STA(BZ_SNDWRD);             /* CANNON EXPLOSION */
    LDA(0xff);
    STA(BZ_EXPCNT);
l10:
    info(m);
    LDX(LINE1);                 /* OUTPUT INSTRUCTIONS */
    bz_msgs(m);
    LDX(LINE2);
    bz_msgs(m);
    LDX(LINE3);
    bz_msgs(m);
    LDX(LINE4);
    bz_msgs(m);
    letchk(m);                  /* LETTER CHANGE */
    LDA(RD(BZ_ALLPOT));         /* IF BUTTON NOT PUSHED ( AND DEBOUNCE ) */
    STA(BZ_NWSTAT);
    EOR(RD(BZ_OLSTAT));
    AND(RD(BZ_NWSTAT));
    LDX(RD(BZ_NWSTAT));
    STX(BZ_OLSTAT);
    AND(PL1FIR);
    if (BEQ) goto l20;
    LDX(RD(BZ_LETR));           /* IF LAST LETTER */
    CPX(2);
    if (BCS) goto l60;
    INX();                      /* NEXT LETTER */
    STX(BZ_LETR);
    LDA(0x16);                  /* BLANK NEXT LETTER */
    STA(BZ_STINTL + X);
l20:
    m->tag = BZ_TAG(BZ_TAG_INITIALS, 0);
    bz_vgcntr(m);
    LDY(0);                     /* OUTPUT INITIALS */
    STY(BZ_VGSIZE);
    LDA(0xee);                  /* -18 */
    LDX(0xee);
    bz_vgvtr(m);
    LDX(0);
    do {
        STX(BZ_TEMP1);
        LDA(RD(BZ_STINTL + X));
        dspalp(m);
        LDX(RD(BZ_TEMP1));
        INX();
        CPX(3);
    } while (BCC);
    finish(m);
    return;
l60:
    LDX(RD(BZ_SAVX));           /* MOVE INITIALS */
    LDY(0);
    for (;;) {
        LDA(RD(BZ_STINTL + Y));
        STA(BZ_INITLS + X);
        CPY(2);
        if (BCS) break;
        INX();
        INY();
    }
    LDA(3);
    STA(BZ_TIMOUT);
    STA(BZ_GOVER);
    LDA(0x81);
    STA(BZ_DSPLAY);
    STA(BZ_FRAME);
}

/* LETCHK: the right hand lever steps the letter being entered. */
static void letchk(bz_machine *m)
{
    STA(BZ_POTGO);              /* READ CONTROLLER */
    LDA(RD(BZ_ALLPOT));
    AND(3);                     /* IF PUSHED */
    if (BNE) goto l10;
l3:
    LDA(4);                     /* RESET TIMER */
    STA(BZ_LTIMER);
    return;
l10:
    DEC(BZ_LTIMER);             /* DELAY */
    if (BNE) return;
    LDX(RD(BZ_LETR));           /* GET LETTER */
    LDY(RD(BZ_STINTL + X));
    LSRA();                     /* IF INCREMENT LETTER */
    if (BCC) goto l30;
    DEY();                      /* DECREMENT LETTER */
    DEY();
    CPY(0x16);
    if (BCS) goto l40;
    LDY(0x4a);
    goto l40;
l30:
    INY();                      /* INCREMENT LETTER */
    INY();
    CPY(0x4c);
    if (BCC) goto l40;
    LDY(0x16);
l40:
    TYA();
    STA(BZ_STINTL + X);
    goto l3;                    /* (BNE, always) */
}

/* DISTBL: the page of high scores, in the attract mode. Ends the frame. */
static void distbl(bz_machine *m)
{
    info(m);                    /* OUTPUT CURRENT SCORE */
    LDX(HISCOR);
    bz_msgs(m);
    m->tag = BZ_TAG(BZ_TAG_HISCORE, 0);
    bz_vgcntr(m);
    LDY(0);
    STY(BZ_SAVX);
    LDA(0xe0);                  /* -32 */
    LDX(30);
    STX(BZ_YDISP);
l10:
    m->tag = BZ_TAG(BZ_TAG_HISCORE, m->mem[BZ_SAVX] / 3);
    bz_vgvtr(m);
    LDX(RD(BZ_SAVX));
    LDA(RD(BZ_HSCTBL + X));
    STA(BZ_TEMP2);
    ORA(RD(BZ_HSCTBL + 1 + X)); /* IF DONE */
    if (BEQ) goto l40;
    LDA(RD(BZ_HSCTBL + 1 + X)); /* OUTPUT SCORE */
    STA(BZ_TEMP2 + 1);
    LDA(BZ_TEMP2);
    bz_digt2s(m);
    LDX(ZEROS);
    bz_msgs(m);
    LDX(RD(BZ_SAVX));           /* OUTPUT INITIALS */
    LDA(RD(BZ_INITLS + X));
    dspalp(m);
    LDX(RD(BZ_SAVX));
    LDA(RD(BZ_INITLS + 1 + X));
    dspalp(m);
    LDX(RD(BZ_SAVX));
    LDA(RD(BZ_INITLS + 2 + X));
    dspalp(m);
    LDY(0);
    LDX(RD(BZ_SAVX));
    LDA(RD(BZ_HSCTBL + 1 + X));
    if (BEQ) goto l25;
    TAX();
    LDA(RD(BZ_VGMSGA));         /* BLANK BEFORE TANK */
    STA(IY(BZ_VGLIST));
    INY();
    LDA(RD(BZ_VGMSGA + 1));
    STA(IY(BZ_VGLIST));
    CPX(10);
    if (BCC) goto l23;
    LDX(10);
l23:
    do {
        INY();
        LDA(RD(BZ_TSYMBL));
        STA(IY(BZ_VGLIST));
        INY();
        LDA(RD(BZ_TSYMBL + 1));
        STA(IY(BZ_VGLIST));
        DEX();
    } while (BNE);
    bz_vgadd(m);
l25:
    LDX(RD(BZ_SAVX));           /* IF DONE */
    CPX((BZ_HSCNUM - 1) * 3);
    if (BCS) goto l40;
    INX();                      /* NEXT */
    INX();
    INX();
    STX(BZ_SAVX);
    bz_vgcntr(m);               /* MOVE BEAM TO NEXT LINE */
    LDA(RD(BZ_YDISP));
    SEC();
    SBC(10);
    STA(BZ_YDISP);
    TAX();
    LDA(0xe0);
    LDY(0);
    goto l10;
l40:
    LDA(RD(BZ_OPTION));
    LSRA();                     /* SET TEMP2 W/BONUS LEVEL */
    LSRA();
    LSRA();
    LSRA();
    AND(3);
    if (BEQ) goto l45;
    TAX();
    LDA(RD(BZ_BONTBL + X));
    CLC();
    SED();
    ADC(1);
    CLD();
    STA(BZ_SAVE1);
    m->tag = BZ_TAG(BZ_TAG_TEXT, BONPLN);   /* (its numbers belong to the line) */
    LDX(BONPLN);
    bz_msgs(m);                 /* OUTPUT BONUS MESSAGE */
    LDA(BZ_SAVE1);              /* OUTPUT BONUS LEVEL */
    LDY(1);
    bz_digtys(m);
    LDX(BONPL1);                /* OUTPUT REMAINING MESSAGE */
    bz_msgs(m);
l45:
    finish(m);
}

/* DSPALP: OUTPUT ALPHA NUM CHAR. */
static void dspalp(bz_machine *m)
{
    TAY();
    LDX(RD(BZ_VGMSGA + 1 + Y));
    LDA(RD(BZ_VGMSGA + Y));
    bz_vgadd2(m);
}

/* FINISH: the list is done; the interrupt may show it. */
static void finish(bz_machine *m)
{
    m->tag = BZ_TAG(BZ_TAG_END, 0);
    bz_vghalt(m);               /* STOP VG */
    LDA(0);                     /* INDICATE BUFFER READY */
    STA(BZ_BUFRDY);
    BIT(RD(BZ_ATRACT));         /* SECURITY */
    if (BPL) return;
    LDA(RD(BZ_NOR2D3));
    if (BMI) return;
    CMP(4);
    if (BCC) return;
    LDA(RD(BZ_TRAP1));
    STA(BZ_VECRAM);
}

/* --- messages --------------------------------------------------------------------- */

/* MSGS: WILL OUTPUT SPECIFIED MESSAGE TO SPECIFIED LOCATION ON SCREEN.
 * (X)=MESSAGE # * 2. In the language of the option switches. A message is
 * two bytes of position (X/4, Y/4; 0 0: where the beam is), then the
 * characters as their places in VGMSGA, the last one with bit 7 set. */
void bz_msgs(bz_machine *m)
{
    static const uint16_t tables[4] = { BZ_ENGMSG, BZ_GRMMSG, BZ_FRNMSG, BZ_SPNMSG };
    uint16_t tag_before = m->tag;
    if (X != ZEROS) m->tag = BZ_TAG(BZ_TAG_TEXT, X);    /* (the zeros behind a score belong to its row) */

    LDA(RD(BZ_OPTION));         /* DO CASE LANGUAGE */
    ROLA();
    ROLA();
    ROLA();
    ROLA();
    AND(6);
    TAY();
    {
        uint16_t tbl = tables[Y >> 1];
        LDA(RD(tbl + X));       /* SET UP POINTER */
        STA(BZ_TBLPTR);
        LDA(RD(tbl + 1 + X));
    }
    STA(BZ_TBLPTR + 1);
    LDY(0);
    TXA();
    CMP(GAMOVR);
    LDA(2);                     /* SET SCALING */
    if (BCC) goto l5;
    LDA(1);
l5:
    STA(BZ_TEMP3 + 1);
    LDA(RD(IY(BZ_TBLPTR)));
    INY();                      /* IF NO POSITIONING NEEDED */
    ORA(RD(IY(BZ_TBLPTR)));
    if (BEQ) goto l10;
    bz_vgcntr(m);               /* POSITION FOR MSG */
    LDY(1);
    LDA(RD(IY(BZ_TBLPTR)));
    TAX();
    DEY();
    LDA(RD(IY(BZ_TBLPTR)));
    bz_vgvtr(m);
l10:
    LDA(RD(BZ_TEMP3 + 1));
    bz_vgsca1(m);
    LDY(2);                     /* INIT LOOP */
    LDA(0);                     /* INIT VGLIST OFFSET */
    STA(BZ_TEMP1);
    do {
        LDA(RD(IY(BZ_TBLPTR))); /* GET CHARACTER REPRESENTATION */
        STA(BZ_TEMP2);
        AND(0x7f);
        INY();
        STY(BZ_TEMP3);          /* SAVE Y */
        TAX();
        LDA(RD(BZ_VGMSGA + X)); /* GET CORRECT JSRL */
        LDY(RD(BZ_TEMP1));
        STA(IY(BZ_VGLIST));
        INY();
        LDA(RD(BZ_VGMSGA + 1 + X));
        STA(IY(BZ_VGLIST));
        INY();
        STY(BZ_TEMP1);          /* SAVE Y */
        LDY(RD(BZ_TEMP3));      /* GET CHARACTER PTR */
        BIT(RD(BZ_TEMP2));      /* IF NOT END OF STRING */
    } while (BPL);
    LDY(RD(BZ_TEMP1));          /* UPDATE VGLIST */
    DEY();
    bz_vgadd(m);
    LDA(1);                     /* RESET SCALE */
    bz_vgsca1(m);
    m->tag = tag_before;
}

/* INFO: the tanks left, the score and the high score at the top right. */
static void info(bz_machine *m)
{
    m->tag = BZ_TAG(BZ_TAG_INFO, 0);
    bz_vgcntr(m);
    LDY(0);
    STY(BZ_VGSIZE);
    LDX(90);                    /* Y/4 */
    LDA(32);                    /* X/4 */
    bz_vgvtr(m);
    LDA(RD(BZ_LIVES));          /* IF DEAD */
    if (BEQ) goto l20;
    STA(BZ_TEMP1);
    m->tag = BZ_TAG(BZ_TAG_LIVES, 0);
    LDY(0);
    LDX(RD(BZ_TSYMBL + 1));
    do {
        LDA(RD(BZ_TSYMBL));     /* OUTPUT 1 TANK PER LIFE */
        STA(IY(BZ_VGLIST));
        TXA();
        INY();
        STA(IY(BZ_VGLIST));
        INY();
        DEC(BZ_TEMP1);
    } while (BNE);
    DEY();
    bz_vgadd(m);
l20:
    m->tag = BZ_TAG(BZ_TAG_INFO, 0);
    LDX(YSCORE);
    bz_msgs(m);
    LDY(0);
    LDX(0);                     /* LABS SHORT FORM (Y/4) */
    LDA(0xd6);                  /* -42 (X/4) */
    bz_vgvtr(m);
    m->tag = BZ_TAG(BZ_TAG_SCORE, 0);
    LDA(BZ_HITS);
    bz_digt2s(m);
    m->tag = BZ_TAG(BZ_TAG_INFO, 0);
    LDX(CHISCR);
    bz_msgs(m);
    LDA(2);
    bz_vgsca1(m);
    LDY(0);
    LDX(0);
    LDA(0xd6);
    bz_vgvtr(m);
    LDA(RD(BZ_HSCTBL));
    STA(BZ_TEMP2);
    LDA(RD(BZ_HSCTBL + 1));
    STA(BZ_TEMP2 + 1);
    m->tag = BZ_TAG(BZ_TAG_SCORE, 1);
    LDA(BZ_TEMP2);
    bz_digt2s(m);
    m->tag = BZ_TAG(BZ_TAG_INFO, 0);
    LDA(1);                     /* RESET SCALE INFO */
    bz_vgsca1(m);
}
