/* play.c - BZONE.MAC: what happens. Shells and what they hit (QWIKCK,
 * SHRTCK, COLCHK, SAUCHK, SHUPDT), scores and bonus tanks (NEWLIF), the
 * explosion's start (EXINIT), the player's levers (MOTION), the enemy
 * tank (ROBOT, FIREIT, REACT), the missile (BUZBOM), the saucer (SAUCMV),
 * where the enemy is (TRACK), running into things (OBJOBJ), the next
 * enemy (ROBCHK, TANKCK, R2D3CK) and the radar (DRADAR).
 *
 * Everything of the player's is at index 0, of the enemy's at index 2:
 * TPOSX/TPOSY (16 bits each), TANGLE, FIRECT (the shell's range counter;
 * negative: it is exploding), SHELLX/SHELLY, COLFLG, HITS, SNDWRD. */
#include "game/game.h"
#include "game/m6502.h"

#define LOX     0x02    /* SNDWRD: the loud explosion */
#define HIX     0xfd
#define LOUDSH  0x08    /* SNDWRD: the loud shell */
#define SOFTSH  0xf7
#define HIDLE   0x10    /* SNDWRD: the engine revs */
#define LIDLE   0x00
#define PL1FIR  0x10

static void m_set(bz_machine *m);
static void m_for(bz_machine *m);
static void m_for1(bz_machine *m);
static void m_rev(bz_machine *m);
static void itangl(bz_machine *m);
static void dtangl(bz_machine *m);
static void saveit(bz_machine *m);
static void bakout(bz_machine *m);
static void react(bz_machine *m);
static void fireit(bz_machine *m);
static void buzbom(bz_machine *m);
static void newlif(bz_machine *m);
static void exinit(bz_machine *m);
static void abstmp(bz_machine *m);
static void shrtck(bz_machine *m);
static void rob1(bz_machine *m);

/* --- shells --------------------------------------------------------------------- */

/* QWIKCK DOES A QUICK COLLISION CHECK FOR PLAYER'S SHELL VS. ROBOT ONLY.
 * CC - NO COLLISION. CS - COLLISION: APPROPRIATE FLAGS & COUNTERS ARE SET */
void bz_qwikck(bz_machine *m)
{
    LDA(RD(BZ_FIRECT));         /* IF NO SHELL */
    if (BMI) goto none;
    if (BNE) goto l10;
none:
    CLC();
    return;
l10:
    LDX(0);                     /* SET UP FOR SHRTCK */
    LDY(2);
    shrtck(m);
}

/* SHRTCK DOES THE SAME CHECK, BUT FOR EITHER PLAYER'S SHELL VS. ROBOT, OR
 * FOR ROBOT'S SHELL VS. PLAYER. X: whose shell; Y: who may be hit. */
static void shrtck(bz_machine *m)
{
    LDA(RD(BZ_SHELLX + X));     /* SET UP FOR DISTANCE */
    STA(BZ_ACOS);
    LDA(RD(BZ_SHELLX + 1 + X));
    STA(BZ_ACOS + 1);
    LDA(RD(BZ_SHELLY + X));
    STA(BZ_BSIN);
    LDA(RD(BZ_SHELLY + 1 + X));
    STA(BZ_BSIN + 1);
    LDA(RD(BZ_COLFLG + Y));
    if (BNE) goto l16;
    LDA(RD(BZ_TPOSX + Y));      /* GET PROXIMITY TO SHELL */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_TPOSX + 1 + Y));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_TPOSY + Y));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_TPOSY + 1 + Y));
    STA(BZ_DSTNCE);
    CMP(RD(IX(0)));             /* WASTE TIME */
    LDA(RD(IY(0)));
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    if (BMI) goto none;
    LSRA();
    ROR(BZ_TEMP3);
    LSRA();
l16:
    if (BNE) goto none;
    ROR(BZ_TEMP3);
    BIT(RD(BZ_R2D3FL));         /* IF NOT R2D3 */
    if (BPL) goto l17;
    LDA(RD(BZ_EXPOSZ + 0x0c));  /* IF TOO HIGH UP */
    CMP(0x00);                  /* TOP&0FF */
    LDA(RD(BZ_EXPOSZ + 0x0d));
    SBC(0x02);                  /* TOP/100 */
    if (BCS) goto none;
l17:
    LDA(RD(BZ_TANGLE));
    SEC();
    SBC(RD(BZ_TANGLE + 2));
    ASLA();
    if (BPL) goto l30;
    EOR(0xff);
    CLC();
    ADC(1);
l30:
    LSRA();
    LSRA();
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l35;
    LSRA();
    goto l36;                   /* (BPL, always) */
l35:
    CLC();
    ADC(0x18);
l36:
    STA(BZ_TEMP1);
    LSRA();
    CLC();
    ADC(RD(BZ_TEMP1));
    ADC(0x38);
    STA(BZ_TEMP1);
    CMP(RD(BZ_TEMP3));
    if (BCS) goto l38;
none:
    CLC();                      /* Q.20$: TELL 'EM NO - NO - NO , NO-NO-NO-NO-NO! */
    return;
l38:
    LDA(0x20);                  /* INDICATE COLLISION */
    STA(BZ_COLFLG + Y);
    LDA(0);                     /* RESET TIMER */
    STA(BZ_TIMOUT);
    TYA();                      /* IF NOT PLAYER */
    if (BNE) goto l18;
    LDA(2);                     /* CRACK WINDSHIELD */
    STA(BZ_CRACK);
    LDA(0xff);
    STA(BZ_BOUNCE);
    DEC(BZ_LIVES);              /* IF GAME NOT OVER */
    if (BNE) goto l18;
    INC(BZ_GOVER);              /* INDICATE GAME OVER */
l18:
    LDA(RD(BZ_HITS + X));       /* SAVE CURRENT SCORE */
    STA(BZ_TEMP1);
    LDA(RD(BZ_HITS + 1 + X));
    STA(BZ_TEMP1 + 1);
    TXA();                      /* IF ROBOT */
    if (BNE) goto l4;
    LDA(2);
    BIT(RD(BZ_R2D3FL));         /* IF NOT R2D3 */
    if (BMI) goto l90;
    bz_tr7chk(m);               /* IF TR7 */
    LDA(3);
    if (BCS) goto l90;
l4:
    LDA(1);                     /* ADD 1 OTHERWISE */
l90:
    CLC();
    SED();
    ADC(RD(BZ_HITS + X));
    STA(BZ_HITS + X);
    LDA(RD(BZ_HITS + 1 + X));
    ADC(0);
    CLD();
    STA(BZ_HITS + 1 + X);
    TXA();                      /* IF ROBOT */
    if (BNE) goto l19;
    newlif(m);                  /* GIVE-A-LIFE  --  SAVE-A-FRIEND */
l19:
    LDA(0x80);                  /* EXPLODE SHELL */
    STA(BZ_FIRECT + X);
    LDA(0xff);                  /* BIG EXPLOSION IN TANK HIT */
    STA(BZ_EXPCNT + Y);
    LDA(LOX);
    ORA(RD(BZ_SNDWRD + Y));
    STA(BZ_SNDWRD + Y);
    LDA(RD(BZ_SNDWRD + X));
    AND(HIX);
    STA(BZ_SNDWRD + X);
    exinit(m);
    LDA(0x70);                  /* SMALL EXPLOSION IN OTHER TANK */
    STA(BZ_EXPCNT + X);
    SEC();                      /* TELL THEM COLLISION */
}

/* COLCHK: WILL CHECK FOR COLLISION BETWEEN SHELLS AND OBJECTS: the other
 * tank (SHRTCK), the saucer (SAUCHK), the things on the playfield. */
void bz_colchk(bz_machine *m)
{
    LDX(2);                     /* START WITH 2ND SHELL */
c10:
    TXA();                      /* INDEX TO OTHER TANK */
    EOR(2);
    TAY();
    LDA(RD(BZ_FIRECT + X));     /* IF NO SHELL FIRED */
    if (BMI) goto c60;
    if (BEQ) goto c60;
    LDA(RD(BZ_CRACK));
    if (BNE) goto l5;
    LDA(RD(BZ_GOVER));
    if (BNE) goto c60;
l5:
    shrtck(m);                  /* SETUP MATH BOX AND DO TANK-SHELL COLCHK */
    if (BCS) goto c60;          /* IF COLLISION */

    /* SAUCHK */
    LDA(RD(BZ_SAUCER));
    if (BEQ) goto c21;
    LDA(RD(BZ_SCOLFG));
    if (BNE) goto c21;
    LDA(RD(BZ_SAPOSX));         /* GET DISTANCE TO SAUCER */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_SAPOSX + 1));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_SAPOSY));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_SAPOSY + 1));
    STA(BZ_TPOSYT + 1);
    STA(BZ_DSTNCE);
    CMP(RD(IX(0)));
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP3);
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP3);
    CMP(0);                     /* IF TOO FAR */
    if (BNE) goto c21;
    LDA(0x90);
    CMP(RD(BZ_TEMP3));
    if (BCC) goto c21;
    LDA(0x40);                  /* INDICATE COLLISION */
    STA(BZ_SCOLFG);
    LDA(BZ_S_DISINT);
    bz_sndon(m);
    LDA(0xa0);                  /* EXPLODE SHELL */
    STA(BZ_FIRECT + X);
    STA(BZ_EXPCNT + X);
    LDA(LOX);
    ORA(RD(BZ_SNDWRD + Y));
    STA(BZ_SNDWRD + Y);
    TXA();                      /* IF ROBOT BLEW IT UP */
    if (BNE) goto c60;
    LDA(RD(BZ_HITS));
    STA(BZ_TEMP1);
    LDA(RD(BZ_HITS + 1));
    STA(BZ_TEMP1 + 1);
    LDA(5);
    CLC();
    SED();
    ADC(RD(BZ_HITS));
    STA(BZ_HITS);
    LDA(RD(BZ_HITS + 1));
    ADC(0);
    CLD();
    STA(BZ_HITS + 1);
    newlif(m);                  /* GIVE NEW LIFE */
    goto c60;

c21:
    LDY(0);
l21:
    LDA(RD(BZ_PTBLO1 + Y));     /* IF DONE */
    if (BMI) goto c60;
    LDA(RD(BZ_PTBLX1 + Y));     /* GET PROXIMITY TO OBJECT */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_PTBLX1 + 1 + Y));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_PTBLY1 + Y));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_PTBLY1 + 1 + Y));
    STA(BZ_DSTNCE);
    CMP(RD(IX(0)));             /* WASTE TIME */
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP3);
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP3);
    CMP(0);                     /* IF TOO FAR */
    if (BNE) goto l40;
    STX(BZ_GARBAG);
    LDX(RD(BZ_PTBLO1 + Y));
    LDA(RD(BZ_PRXTBL + X));
    LDX(RD(BZ_GARBAG));
    CMP(RD(BZ_TEMP3));
    if (BCC) goto l40;
    LDA(0xa0);                  /* EXPLODE SHELL */
    STA(BZ_FIRECT + X);
    LDA(0x70);                  /* TURN ON SOUND SHORT */
    STA(BZ_EXPCNT);
    LDA(RD(BZ_SNDWRD));
    AND(HIX);
    STA(BZ_SNDWRD);
    goto c60;                   /* (BCS, always) */
l40:
    INY();
    INY();
    if (BNE) goto l21;
c60:
    DEX();
    DEX();
    if (BEQ) goto c10;
}

/* SHUPDT: UPDATES SHELLS POSITION */
void bz_shupdt(bz_machine *m)
{
    LDX(2);                     /* START WITH SECOND SHELL */
l10:
    LDA(RD(BZ_FIRECT + X));     /* IF SHELL NOT IN AIR */
    if (BEQ) goto l20;
    if (BMI) goto l20;
    DEC(BZ_FIRECT + X);         /* RANGE SHELL */
    CLC();
    LDA(RD(BZ_SHELLX + X));     /* UPDATE SHELL'S X POS */
    ADC(RD(BZ_SINCX + X));
    STA(BZ_SHELLX + X);
    LDA(RD(BZ_SHELLX + 1 + X));
    ADC(RD(BZ_SINCX + 1 + X));
    STA(BZ_SHELLX + 1 + X);
    LDA(RD(BZ_SHELLY + X));     /* UPDATE SHELL'S Y POS */
    CLC();
    ADC(RD(BZ_SINCY + X));
    STA(BZ_SHELLY + X);
    LDA(RD(BZ_SHELLY + 1 + X));
    ADC(RD(BZ_SINCY + 1 + X));
    STA(BZ_SHELLY + 1 + X);
l20:
    DEX();
    DEX();
    if (BEQ) goto l10;
}

/* ABSTMP: WILL RETURN ABSOLUTE VALUE OF TEMP1 IN TEMP1 */
static void abstmp(bz_machine *m)
{
    BIT(RD(BZ_TEMP1 + 1));      /* IF POSITIVE */
    if (BPL) return;
    LDA(RD(BZ_TEMP1));          /* ABS VALUE */
    CLC();
    EOR(0xff);
    ADC(1);
    STA(BZ_TEMP1);
    LDA(RD(BZ_TEMP1 + 1));      /* HIGH BYTE */
    EOR(0xff);
    ADC(0);
    STA(BZ_TEMP1 + 1);
}

/* NEWLIF: GIVE NEW LIFE TO PLAYER IF WARRANTED. TEMP1 MUST HAVE SCORE
 * PRIOR TO CURRENT INCREMENT. A bonus tank at the level the option
 * switches name, and one more at 100000 (the SUPER BONUS, with the 1812
 * overture). */
static void newlif(bz_machine *m)
{
    LDA(RD(BZ_OPTION));         /* GET BONUS LEVEL */
    LSRA();
    LSRA();
    LSRA();
    LSRA();
    AND(3);
    TAX();
    LDA(RD(BZ_BONTBL + X));
    if (BEQ) goto l20;
    LDX(RD(BZ_HITS + 1));       /* IF NOT IN PROPER RANGE */
    if (BNE) goto l40;
    CMP(RD(BZ_TEMP1));
    if (BCC) goto l20;
    CMP(RD(BZ_HITS));
    if (BCS) goto l20;
    INC(BZ_LIVES);
    LDA(0);                     /* MAKE SURE GAME NOT OVER */
    STA(BZ_GOVER);
    LDA(BZ_S_BONER);            /* RING THE BELL */
l10:
    bz_sndon(m);
l20:
    LDX(0);
    return;
l40:
    LDA(RD(BZ_TEMP1 + 1));      /* IF NOT SUPER BONUS */
    if (BNE) goto l20;
    STA(BZ_GOVER);              /* SUPER BONUS */
    INC(BZ_LIVES);
    LDA(0xff);
    STA(BZ_SUPER);
    LDA(BZ_S_SUPBON);
    goto l10;
}

/* EXINIT: INITIALIZES POSITIONS AND Z VELOCITIES OF TANK PIECES IF ROBOT
 * EXPLODED. (X)=2 MEANS PLAYER EXPLODED, (X)=0 MEANS ROBOT EXPLODED */
static void exinit(bz_machine *m)
{
    TXA();                      /* IF NOT ROBOT EXPLODED */
    if (BNE) return;
    LDX(5);
    LDY(10);
    do {
        LDA(RD(BZ_IZVEL + X));  /* INITIALIZE Z VELOCITIES */
        STA(BZ_ZVELOC + Y);
        LDA(RD(BZ_TPOSX + 2));  /* INIT ALL PIECES POS */
        STA(BZ_EXPOSX + Y);
        LDA(RD(BZ_TPOSX + 3));
        STA(BZ_EXPOSX + 1 + Y);
        LDA(RD(BZ_TPOSY + 2));
        STA(BZ_EXPOSY + Y);
        LDA(RD(BZ_TPOSY + 3));
        STA(BZ_EXPOSY + 1 + Y);
        LDA(RD(BZ_PRAND));      /* INITIALIZE ORIENTATION */
        STA(BZ_EXPOSO + Y);
        LDA(0);
        STA(BZ_EXPOSZ + Y);
        STA(BZ_EXPOSZ + 1 + Y);
        DEY();
        DEY();
        DEX();
    } while (BPL);
    LDX(0);                     /* RESTORE X */
}

/* --- the player ------------------------------------------------------------------ */

/* MOTION OF TANK: the two levers (forward, back, pivot, the four turns),
 * running into things, the engine's sound, and the fire button. In the
 * attract mode the tank drives about by itself. */
/* An addition, for a yoke or two analog levers (platform/controls.h): the
 * player's motion with each lever anywhere between full back and full
 * forward. The cabinet's levers are switches, and its eight motions are
 * the corners of this:
 *
 *   both forward        two steps (M.FOR twice)         no turn
 *   one forward         one step                        half a unit (ITANGL / DTANGL once)
 *   one each way        no step                         a whole unit (twice)
 *
 * so with l and r as -1..1: l + r steps along the heading, and (r - l) / 2
 * units of turn; the tank is never faster than the cabinet's. Returns 2 if
 * it drove (the caller then checks for objects in the way as after any
 * step), 1 if it only turned, 0 if it stands.
 */
static int analog_motion(bz_machine *m)
{
    int l = m->analog.left, r = m->analog.right;
    if (l > 256) l = 256;
    if (l < -256) l = -256;
    if (r > 256) r = 256;
    if (r < -256) r = -256;
    int steps = l + r;                  /* 256ths of a step, -512..512 */
    int turn = (r - l) / 2;             /* 256ths of a unit, left positive */
    if (!steps && !turn) return 0;

    unsigned angle = (unsigned)(RD(BZ_TANGLE + X) << 8 | RD(BZ_LANGLE + X));
    angle = (angle + (unsigned)turn) & 0xffff;
    WR(BZ_LANGLE + X, angle & 0xff);
    WR(BZ_TANGLE + X, angle >> 8);
    if (!steps) return 1;

    m_set(m);                           /* (X1, Y1: one step along the heading) */
    int sx = (int16_t)(RD(BZ_X1) | RD(BZ_X1 + 1) << 8), sy = (int16_t)(RD(BZ_Y1) | RD(BZ_Y1 + 1) << 8);
    int dx = sx * steps + m->analog.rem_x, dy = sy * steps + m->analog.rem_y;
    int qx = (dx - (dx & 0xff)) / 256, qy = (dy - (dy & 0xff)) / 256;
    m->analog.rem_x = (int16_t)(dx & 0xff);
    m->analog.rem_y = (int16_t)(dy & 0xff);
    unsigned px = (unsigned)(RD(BZ_TPOSX + X) | RD(BZ_TPOSX + 1 + X) << 8) + (unsigned)qx;
    unsigned py = (unsigned)(RD(BZ_TPOSY + X) | RD(BZ_TPOSY + 1 + X) << 8) + (unsigned)qy;
    WR(BZ_TPOSX + X, px & 0xff);
    WR(BZ_TPOSX + 1 + X, (px >> 8) & 0xff);
    WR(BZ_TPOSY + X, py & 0xff);
    WR(BZ_TPOSY + 1 + X, (py >> 8) & 0xff);
    if (steps > 0) INC(BZ_TRDCTR + X);
    else DEC(BZ_TRDCTR + X);
    return 2;
}

void bz_motion(bz_machine *m)
{
    uint8_t t;

    BIT(RD(BZ_ATRACT));         /* IF IN GAME */
    if (BMI) goto l10;
    BIT(RD(BZ_LOGO));           /* IF LOGO */
    if (BMI) return;
    LDA(RD(BZ_CRACK));
    if (BNE) return;
    LDX(0);                     /* MOVE IN ATTRACT */
    BIT(RD(BZ_FRAME));
    if (BVS) goto l3;
    m_for(m);
    goto l4;
l3:
    m_rev(m);
l4:
    BIT(RD(BZ_TPOSX + 3));
    if (BPL) { dtangl(m); return; }
    if (BVC) return;
    itangl(m);
    return;
l10:
    LDX(0);
    LDA(RD(BZ_COLFLG + X));
    if (BNE) return;
    /* M.TOP */
    saveit(m);
    STX(BZ_POTGO);              /* READ POKEY */
    LDA(RD(BZ_ALLPOT));
    STA(BZ_PL1MOV);
    if (m->analog.on) {         /* (an addition: the levers anywhere between their ends) */
        switch (analog_motion(m)) {
        case 2:  goto m_next;
        case 1:  goto m_6;
        default: goto m_stop;
        }
    }
    EOR(0x0f);
    AND(0x0f);
    CMP(5);                     /* < 4 IS ILLEGAL */
    if (BCC) goto m_stop;
    ASLA();
    TAY();
    /* (LDA AY,MTAB-9, PHA, LDA AY,MTAB-0A, PHA, RTS) */
    switch (Y >> 1) {
    case 5:                     /* M.FF: FULL FORWARD */
        m_for(m);
        goto m_f;
    case 6:                     /* M.PR: PIVOT RIGHT */
        dtangl(m);
        dtangl(m);
        goto m_6;
    case 7:                     /* M.RTF: RIGHT TURN FORWARD */
        dtangl(m);
        goto m_f;
    case 9:                     /* M.PL: PIVOT LEFT */
        itangl(m);
        itangl(m);
        goto m_6;
    case 10:                    /* M.FR: FULL REVERSE */
        m_rev(m);
        goto m_r;
    case 11:                    /* M.LTR: LEFT TURN REVERSE */
        itangl(m);
        goto m_r;
    case 13:                    /* M.LTF: LEFT TURN FORWARD */
        itangl(m);
        goto m_f;
    case 14:                    /* M.RTR: RIGHT TURN REVERSE */
        dtangl(m);
        goto m_r;
    default:                    /* 8, 12, 15: M.STOP */
        goto m_stop;
    }
m_f:
    m_for(m);                   /* FORWARD */
    INC(BZ_TRDCTR + X);
    goto m_next;
m_r:
    m_rev(m);                   /* REVERSE */
    DEC(BZ_TRDCTR + X);
m_next:
    bz_objobj(m);               /* IF NO COLLISION */
    if (BCC) goto m_6;
    bakout(m);
    LDA(RD(BZ_OBJCOL));         /* IF ALREADY IN COLLISION */
    if (BNE) goto m_10;
    LDA(BZ_S_BOING);            /* SOUND OF RUNNING INTO OBJECT */
    bz_sndon(m);
    LDA(0x3f);
    STA(BZ_BOUNCE);
    INC(BZ_OBJCOL);
    goto m_10;                  /* (BNE, always) */
m_6:
    LDA(0);
    STA(BZ_OBJCOL);
m_10:
    LDA(HIDLE);
    goto m_nex0;
m_stop:
    LDA(LIDLE);
m_nex0:
    STA(BZ_TEMP1);              /* SET ENGINE SPEED SOUND */
    LDA(RD(BZ_SNDWRD + X));
    AND(0xef);
    ORA(RD(BZ_TEMP1));
    STA(BZ_SNDWRD + X);
    LDA(RD(BZ_FIRECT + X));     /* IF SHELL HAS BEEN FIRED */
    if (BNE) return;
    LDA(RD(BZ_PL1MOV));         /* IF FIRE BUTTON HAS NOT BEEN PUSHED */
    AND(PL1FIR);
    if (BEQ) return;
    LDA(0x7f);                  /* INDICATE SHELL FIRED */
    STA(BZ_FIRECT + X);
    LDA(RD(BZ_SNDWRD + X));     /* SET TO HI EXPLODE SOUND */
    ORA(LOUDSH);
    STA(BZ_SNDWRD + X);
    LDA(5);                     /* TURN ON EXPLOSION */
    STA(BZ_SXPCNT);
    LDA(RD(BZ_LANGLE + X));
    CLC();
    ROLA();
    ROLA();
    t = A;
    LDA(RD(BZ_TANGLE + X));
    STX(BZ_SAVE1);
    TAX();
    PLA(t);
    bz_cosxtr(m);
    STA(BZ_TEMP1);
    TXA();
    {
        FLAGS(p);
        LDX(RD(BZ_SAVE1));
        STA(BZ_SINCX + X);
        RESTORE(p);
    }
    if (BMI) goto l15;
    LDA(0);
    goto l16;
l15:
    LDA(0xff);
l16:
    ASL(BZ_TEMP1);
    ROL(BZ_SINCX + X);
    ROLA();
    STA(BZ_SINCX + 1 + X);
    LDA(RD(BZ_TANGLE + X));
    TAX();
    LDA(RD(BZ_GARBAG));
    bz_sinxtr(m);
    STA(BZ_TEMP1);
    TXA();
    {
        FLAGS(p);
        LDX(RD(BZ_SAVE1));
        STA(BZ_SINCY + X);
        RESTORE(p);
    }
    if (BMI) goto l20;
    LDA(0);
    goto l21;
l20:
    LDA(0xff);
l21:
    ASL(BZ_TEMP1);
    ROL(BZ_SINCY + X);
    ROLA();
    STA(BZ_SINCY + 1 + X);
    LDA(RD(BZ_TPOSX + X));      /* INIT SHELL POSITION (LOAD SHELL) */
    STA(BZ_SHELLX + X);
    LDA(RD(BZ_TPOSX + 1 + X));
    STA(BZ_SHELLX + 1 + X);
    LDA(RD(BZ_TPOSY + X));
    STA(BZ_SHELLY + X);
    LDA(RD(BZ_TPOSY + 1 + X));
    STA(BZ_SHELLY + 1 + X);
}

/* M.SET: the step of the tank at X: X1 and Y1 = 3/4 of the high byte of
 * the cosine and sine of its angle. */
static void m_set(bz_machine *m)
{
    LDA(RD(BZ_TANGLE + X));     /* GET ANGLE */
    STX(BZ_TEMP1);              /* SAVE X */
    bz_sin(m);                  /* SIN (ANGLE)/2 */
    TXA();
    CMP(0x80);
    RORA();
    STA(BZ_Y1);
    CMP(0x80);
    RORA();
    CLC();
    ADC(RD(BZ_Y1));
    STA(BZ_Y1);
    if (BPL) goto l10;          /* SIGN EXTEND */
    LDA(0xff);
    goto l11;
l10:
    LDA(0);
l11:
    STA(BZ_Y1 + 1);
    LDX(RD(BZ_TEMP1));
    LDA(RD(BZ_TANGLE + X));     /* GET ANGLE */
    bz_cos(m);                  /* COS (ANGLE)/2 */
    TXA();
    CMP(0x80);
    RORA();
    STA(BZ_X1);
    CMP(0x80);
    RORA();
    CLC();
    ADC(RD(BZ_X1));
    STA(BZ_X1);
    if (BPL) goto l20;
    LDA(0xff);
    goto l21;
l20:
    LDA(0);
l21:
    STA(BZ_X1 + 1);
    LDX(RD(BZ_TEMP1));          /* RESTORE INDEX */
}

static void m_for(bz_machine *m)
{
    m_set(m);
    m_for1(m);
}

static void m_for1(bz_machine *m)
{
    LDA(RD(BZ_TPOSX + X));      /* UPDATE X POSITION */
    CLC();
    ADC(RD(BZ_X1));
    STA(BZ_TPOSX + X);
    LDA(RD(BZ_TPOSX + 1 + X));
    ADC(RD(BZ_X1 + 1));
    STA(BZ_TPOSX + 1 + X);
    LDA(RD(BZ_TPOSY + X));      /* UPDATE Y POS */
    CLC();
    ADC(RD(BZ_Y1));
    STA(BZ_TPOSY + X);
    LDA(RD(BZ_TPOSY + 1 + X));
    ADC(RD(BZ_Y1 + 1));
    STA(BZ_TPOSY + 1 + X);
}

static void m_rev(bz_machine *m)
{
    m_set(m);
    LDA(RD(BZ_TPOSX + X));      /* UPDATE X POS */
    SEC();
    SBC(RD(BZ_X1));
    STA(BZ_TPOSX + X);
    LDA(RD(BZ_TPOSX + 1 + X));
    SBC(RD(BZ_X1 + 1));
    STA(BZ_TPOSX + 1 + X);
    SEC();                      /* UPDATE Y POS */
    LDA(RD(BZ_TPOSY + X));
    SBC(RD(BZ_Y1));
    STA(BZ_TPOSY + X);
    LDA(RD(BZ_TPOSY + 1 + X));
    SBC(RD(BZ_Y1 + 1));
    STA(BZ_TPOSY + 1 + X);
}

/* ITANGL: INCREMENT TANK'S ANGLE (half a step of 256: the 9th bit is LANGLE D7) */
static void itangl(bz_machine *m)
{
    LDA(0x80);
    CLC();
    ADC(RD(BZ_LANGLE + X));
    STA(BZ_LANGLE + X);
    LDA(0);
    ADC(RD(BZ_TANGLE + X));
    STA(BZ_TANGLE + X);
}

/* DTANGL: DECREMENT TANK'S ANGLE */
static void dtangl(bz_machine *m)
{
    LDA(RD(BZ_LANGLE + X));
    SEC();
    SBC(0x80);
    STA(BZ_LANGLE + X);
    LDA(RD(BZ_TANGLE + X));
    SBC(0);
    STA(BZ_TANGLE + X);
}

/* SAVEIT: SAVES POSITION OF TANK OR ROBOT (X=0 MEANS TANK, X=2 ROBOT) */
static void saveit(bz_machine *m)
{
    LDA(RD(BZ_TPOSX + X));
    STA(BZ_OTPOSX);
    LDA(RD(BZ_TPOSX + 1 + X));
    STA(BZ_OTPOSX + 1);
    LDA(RD(BZ_TPOSY + X));
    STA(BZ_OTPOSY);
    LDA(RD(BZ_TPOSY + 1 + X));
    STA(BZ_OTPOSY + 1);
}

/* BAKOUT: UNDOES SAVEIT: DEAD STOP */
static void bakout(bz_machine *m)
{
    LDA(RD(BZ_OTPOSX));
    STA(BZ_TPOSX + X);
    LDA(RD(BZ_OTPOSX + 1));
    STA(BZ_TPOSX + 1 + X);
    LDA(RD(BZ_OTPOSY));
    STA(BZ_TPOSY + X);
    LDA(RD(BZ_OTPOSY + 1));
    STA(BZ_TPOSY + 1 + X);
}

/* --- the enemy tank ---------------------------------------------------------------- */

/* REACT: SETS UP REACTION TIME FOR ROBOT (ACTION) and how exactly he has
 * to face his goal before driving on (SKILL). */
static void react(bz_machine *m)
{
    uint8_t t = A;              /* PHA: SAVE REGISTERS */
    BIT(RD(BZ_ATRACT));
    if (BMI) goto l10;
    LDA(0x30);
    STA(BZ_ACTION);
    goto l35;                   /* (BNE, always; the code after it in the source is never reached) */
l10:
    CMP(5);
    if (BCS) goto l20;
    STA(BZ_ACTION);
    LDA(5);
    SBC(RD(BZ_ACTION));
    ASLA();
    ASLA();
    ASLA();
    ASLA();
    if (BNE) goto l30;
l20:
    LDA(4);
l30:
    STA(BZ_ACTION);
    BIT(RD(BZ_SKILL));          /* SET UP ACCURACY FOR ROBOT */
    if (BMI) goto l35;
    LDA(10);
    SEC();
    SBC(RD(BZ_SKILL));
    if (BCS) goto l40;
    LDA(1);
    goto l40;
l35:
    LDA(0x0a);
l40:
    ASLA();
    STA(BZ_SKILL);
    PLA(t);
}

/* ROBOT: CONTROL MOTION & FIRING OF ROBOT TANK. He turns towards his goal
 * (RGOAL) and drives; when ACTION has counted down he decides anew: to
 * attack, to evade or to wander. After running into something he backs
 * away, turning. */
void bz_robot(bz_machine *m)
{
    LDA(RD(BZ_COLFLG + 2));     /* IF NOT BLOWN UP */
    if (BNE) return;
    BIT(RD(BZ_R2D3FL));
    if (BPL) goto l2;
    buzbom(m);
    return;
l2:
    LDX(2);
    saveit(m);                  /* SAVE POS */
    CLC();                      /* UPDATE RADAR ANGLE */
    LDA(RD(BZ_RANGLE));
    ADC(0x0b);
    STA(BZ_RANGLE);
    LDA(RD(BZ_STATE));          /* IF NOT COLLISION RETREAT */
    LSRA();
    if (BCC) goto l10;
    LSRA();                     /* BACK AWAY */
    {
        FLAGS(p);
        m_rev(m);
        RESTORE(p);
    }
    if (BCC) goto l7;
    itangl(m);
    goto l8;
l7:
    dtangl(m);
l8:
    DEC(BZ_TRDCTR + 2);
    LDA(RD(BZ_ACTION));         /* IF DONE BACKING */
    if (BNE) return;
    LDA(RD(BZ_STATE));          /* RESET COLLISION RETREAT */
    AND(0xfc);
    STA(BZ_STATE);
    LDA(RD(BZ_TANGLE + 2));     /* SET UP NEW GOAL */
    STA(BZ_RGOAL);
    LDA(0x34);
    STA(BZ_ACTION);
    return;
l10:
    LDA(RD(BZ_ACTION));         /* IF DECISION TIME */
    if (BEQ) goto r40;
    LDA(RD(BZ_TANGLE + 2));     /* IF ABS THETA < 30 DEG */
    SEC();
    SBC(RD(BZ_RGOAL));
    TAY();                      /* SAVE */
    if (BPL) goto l12;
    EOR(0xff);
    CLC();
    ADC(1);
l12:
    CMP(RD(BZ_SKILL));
    if (BCC) goto l20;
    TYA();                      /* IF PIVOT RIGHT */
    if (BPL) goto l15;
    itangl(m);                  /* PIVOT LEFT */
    fireit(m);
    itangl(m);
    fireit(m);
    bz_tr7chk(m);
    if (BCC) return;
    itangl(m);
    fireit(m);
    itangl(m);
    fireit(m);
    return;
l15:
    dtangl(m);                  /* PIVOT RIGHT */
    fireit(m);
    dtangl(m);
    fireit(m);
    bz_tr7chk(m);
    if (BCC) return;
    dtangl(m);
    fireit(m);
    dtangl(m);
    fireit(m);
    return;
l20:
    CMP(0);                     /* IF ON TARGET */
    if (BEQ) goto l30;
    TYA();
    if (BPL) goto l25;          /* IF TURN RIGHT */
    itangl(m);                  /* TURN LEFT */
    goto l30;
l25:
    dtangl(m);                  /* TURN RIGHT */
l30:
    fireit(m);
    LDA(RD(BZ_TEMP1));          /* CHECK DISTANCE */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_TEMP1 + 1));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_TEMP2));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_TEMP2 + 1));
    STA(BZ_TPOSYT + 1);
    STA(BZ_DST2);
    bz_tr7chk(m);
    LDA(RD(BZ_MOUTHI));
    if (BCC) goto l32;          /* IF NOT TR7 */
    CMP(8);
    if (BCS) goto r37;
    return;
l32:
    CMP(5);                     /* IF NOT TOO CLOSE */
    if (BCS) goto r37;
    return;
r37:
    LDX(2);                     /* MOVE FORWARD */
    m_set(m);
    bz_tr7chk(m);
    if (BCC) goto s20;
    ASL(BZ_Y1);
    ROL(BZ_Y1 + 1);
    ASL(BZ_X1);
    ROL(BZ_X1 + 1);
s20:
    m_for1(m);
    bz_objobj(m);
    if (BCS) goto l39;          /* IF COLLISION */
    INC(BZ_TRDCTR + 2);         /* MOVE TREAD */
    LDA(RD(BZ_TANGLE + 2));
    CMP(RD(BZ_RGOAL));
    if (BNE) return;
    m_for1(m);
    bz_objobj(m);
    if (BCS) goto l39;
    return;
l39:
    BIT(RD(BZ_ATRACT));
    if (BPL) goto l42;
    ORA(0);                     /* IF COLLIDED WITH PLAYER */
    if (BMI) goto l41;
    LDA(RD(BZ_PRAND));          /* INDICATE COLLISION */
    AND(2);
    ORA(1);
l42:
    ORA(RD(BZ_STATE));
    STA(BZ_STATE);
    LDA(0x30);                  /* SET UP REACTION TIME */
    STA(BZ_ACTION);
l41:
    bakout(m);                  /* UNDO DAMAGE */
    return;

r40:
    BIT(RD(BZ_ATRACT));
    if (BPL) goto r_rand;
    LDA(RD(BZ_FTIMER));         /* IF WE'VE BEEN NICE TOO LONG */
    CMP(0xff);
    if (BEQ) goto r_atck;
    LDA(RD(BZ_PRAND));          /* IF RANDOM ATTACK */
    LSRA();
    if (BCC) goto r_atck;
    LDA(RD(BZ_HITS + 1));       /* IF HE'S GOOD */
    if (BNE) goto r_atck;
    LDA(RD(BZ_HITS));           /* IF EVADE */
    SEC();
    SBC(RD(BZ_HITS + 2));
    if (BEQ) goto r_evad;
    if (BCC) goto r_rand;
    goto r_atck;
r_evad:
    LDA(RD(BZ_INTCT));
    AND(7);
    if (BNE) goto e10;
    LDA(1);
    ORA(RD(BZ_STATE));
    STA(BZ_STATE);
    goto e20;                   /* (BNE, always) */
e10:
    LDA(0);
    STA(BZ_STATE);
    bz_track(m);                /* BEAT FEET */
    EOR(0x40);
    STA(BZ_RGOAL);
e20:
    LDA(0x40);
    STA(BZ_ACTION);
    return;
r_rand:
    LDA(RD(BZ_PRAND));          /* RANDOM */
    AND(0x1f);
    BIT(RD(BZ_INTCT));
    if (BMI) goto n10;
    SBC(RD(BZ_RGOAL));
    if (BNE) goto n20;
n10:
    ADC(RD(BZ_RGOAL));
n20:
    STA(BZ_RGOAL);
    LDA(0);
    STA(BZ_STATE);
    react(m);
    return;
r_atck:
    bz_track(m);                /* ATTACK */
    STA(BZ_RGOAL);
    LDA(0x80);                  /* INDICATE ATTACK */
    STA(BZ_STATE);
    react(m);
}

/* FIREIT: the enemy fires if he may: not too soon after appearing, at a
 * beginner only when in view and near, facing the player, his shell
 * free, the player alive. */
static void fireit(bz_machine *m)
{
    LDA(RD(BZ_FTIMER));         /* IF TOO SOON AFTER APPEARING */
    CMP(0x20);
    if (BCC) return;
    CMP(0xff);
    if (BEQ) goto l5;
    LDA(RD(BZ_HITS + 1));       /* IF HE'S GOOD */
    if (BNE) goto l5;
    LDA(RD(BZ_HITS));           /* IF NOT ROOKIE */
    LSRA();
    if (BNE) goto l5;
    LDA(RD(BZ_PTURN));          /* IF NOT IN VIEW */
    CMP(0x20);
    if (BCS) return;
    LDA(RD(BZ_TDIST));          /* IF TOO FAR */
    CMP(0x24);
    if (BCS) return;
l5:
    bz_track(m);                /* IF NOT ON TARGET */
    LDX(2);
    SEC();
    SBC(RD(BZ_TANGLE + 2));
    if (BPL) goto l10;
    CLC();
    EOR(0xff);
    ADC(2);
l10:
    CMP(2);
    if (BCS) return;
    LDA(RD(BZ_FIRECT + 2));     /* IF SHELL NOT AVAILABLE */
    if (BNE) return;
    LDA(RD(BZ_COLFLG));         /* IF PLAYER IS HIT */
    if (BNE) return;
    LDA(0x7f);                  /* FIRE!! */
    STA(BZ_FIRECT + 2);
    LDA(5);                     /* SHELL SOUND */
    STA(BZ_SXPCNT);
    LDA(RD(BZ_SNDWRD));
    AND(SOFTSH);
    STA(BZ_SNDWRD);
    LDA(RD(BZ_TANGLE + 2));     /* SET UP SHELLS MOTION */
    bz_cos(m);
    STA(BZ_TEMP1);
    TXA();
    STA(BZ_SINCX + 2);
    if (BMI) goto l31;
    LDA(0);
    goto l32;
l31:
    LDA(0xff);
l32:
    ASL(BZ_TEMP1);
    ROL(BZ_SINCX + 2);
    ROLA();
    STA(BZ_SINCX + 3);
    LDA(RD(BZ_TANGLE + 2));
    bz_sin(m);
    STA(BZ_TEMP1);
    TXA();
    STA(BZ_SINCY + 2);
    if (BMI) goto l33;
    LDA(0);
    goto l34;
l33:
    LDA(0xff);
l34:
    ASL(BZ_TEMP1);
    ROL(BZ_SINCY + 2);
    ROLA();
    STA(BZ_SINCY + 3);
    LDA(RD(BZ_TPOSX + 2));      /* INIT SHELL POSITION */
    STA(BZ_SHELLX + 2);
    LDA(RD(BZ_TPOSX + 3));
    STA(BZ_SHELLX + 3);
    LDA(RD(BZ_TPOSY + 2));
    STA(BZ_SHELLY + 2);
    LDA(RD(BZ_TPOSY + 3));
    STA(BZ_SHELLY + 3);
    LDX(2);                     /* RESTORE X */
}

/* BUZBOM: THIS ROUTINE CONTROLS MOVEMENT OF R2D3-BUZZ-BOMB. IT IS CALLED
 * IN LIEU OF ROBOT AND FLIES THE BUZZ BOMB DIRECTLY AT PLAYER, AT HIGH
 * SPEED, SWOOPING IN ON HIM. IF ANOTHER OBJECT IS ENCOUNTERED, BUZZ-BOMB
 * LEVITATES AND RESTARTS SWOOPING. Its height is EXPOSZ+0C. */
static void buzbom(bz_machine *m)
{
    uint8_t t;

    INC(BZ_TRDCTR + 2);         /* ALWAYS CHANGE THRUST PICTURE */
    LDA(0x00);                  /* IF TOP > Z HEIGHT AND OBJCOL */
    CMP(RD(BZ_EXPOSZ + 0x0c));
    LDA(0x02);
    SBC(RD(BZ_EXPOSZ + 0x0d));
    LDA(0);
    ROLA();
    STA(BZ_GARBAG);
    AND(RD(BZ_OBJCOL + 2));
    if (BEQ) goto l2;
    INC(BZ_EXPOSZ + 0x0d);      /* 30$ */
    return;
l2:
    LDA(RD(BZ_TANGLE));         /* INSURE NO BACK ATTACKS */
    SEC();
    SBC(RD(BZ_RGOAL));
    STA(BZ_TEMP2);
    if (BPL) goto l3;
    CLC();
    EOR(0xff);
    ADC(1);
l3:
    CMP(0x40);
    if (BCS) goto l4;
    LDX(2);
    BIT(RD(BZ_TEMP2));
    if (BPL) goto l12;
    goto l6;
l4:
    bz_track(m);                /* ADJUST DIRECTION */
    LDX(2);
    STA(BZ_TEMP2);
    LDA(RD(BZ_RGOAL));
    SEC();
    SBC(RD(BZ_TEMP2));
    if (BEQ) goto l20;
    if (BPL) goto l10;          /* IF DECREMENT NEEDED */
    CMP(0xfd);
    if (BCC) goto l5;
l6:
    INC(BZ_RGOAL);
l5:
    INC(BZ_RGOAL);
    goto l20;
l10:
    CMP(3);
    if (BCC) goto l15;
l12:
    DEC(BZ_RGOAL);
l15:
    DEC(BZ_RGOAL);
l20:
    LDA(RD(BZ_NOR2D3));         /* IF FIRST R2D3 */
    if (BEQ) goto l37;
    LDA(RD(BZ_HITS + 1));
    if (BNE) goto l33;
    LDA(RD(BZ_OPTION));
    LSRA();
    LSRA();
    AND(3);
    TAX();
    LDA(RD(BZ_MISLVL + X));
    CLC();
    SED();
    ADC(0x25);
    CLD();
    SEC();
    SBC(RD(BZ_HITS));
    if (BMI) goto l33;
    CMP(8);
    if (BCS) goto l35;
l33:
    LDA(8);
l35:
    CMP(RD(BZ_TDIST));
    if (BCC) goto l40;
l37:
    LDA(RD(BZ_RGOAL));
    goto l19;
l40:
    LDA(RD(BZ_FRAME));          /* SET UP X1 & Y1 FOR M.FOR1 */
    LSRA();
    LSRA();
    LSRA();
    LSRA();
    LDA(RD(BZ_FRAME));
    AND(0x1f);
    STA(BZ_TEMP1);
    LDA(RD(BZ_RGOAL));
    if (BCS) goto l18;
    ADC(RD(BZ_TEMP1));
    goto l19;
l18:
    SBC(RD(BZ_TEMP1));
l19:
    STA(BZ_TANGLE + 2);
    bz_sin(m);
    STX(BZ_Y1);
    t = A;
    LDA(0);
    BIT(RD(BZ_Y1));
    if (BPL) goto l22;
    LDA(0xff);
l22:
    STA(BZ_Y1 + 1);
    PLA(t);
    ASLA();
    ROL(BZ_Y1);
    ROL(BZ_Y1 + 1);
    ASLA();
    ROL(BZ_Y1);
    ROL(BZ_Y1 + 1);
    LDA(RD(BZ_TANGLE + 2));
    bz_cos(m);
    STX(BZ_X1);
    t = A;
    LDA(0);
    BIT(RD(BZ_X1));
    if (BPL) goto l24;
    LDA(0xff);
l24:
    STA(BZ_X1 + 1);
    PLA(t);
    ASLA();
    ROL(BZ_X1);
    ROL(BZ_X1 + 1);
    ASLA();
    ROL(BZ_X1);
    ROL(BZ_X1 + 1);
    LDA(RD(BZ_TPOSX + 2));      /* SAVE OLD POSITION */
    STA(BZ_OTPOSX);
    LDA(RD(BZ_TPOSX + 3));
    STA(BZ_OTPOSX + 1);
    LDA(RD(BZ_TPOSY + 2));
    STA(BZ_OTPOSY);
    LDA(RD(BZ_TPOSY + 3));
    STA(BZ_OTPOSY + 1);
    LDX(2);
    m_for1(m);                  /* MOVE FORWARD */
    bz_qwikck(m);               /* IF WE'RE BLOWN UP */
    if (BCS) return;
    LDX(2);
    bz_objobj(m);               /* IF NO COLLISION */
    if (BCC) goto l25;
    LDX(RD(BZ_GARBAG));         /* IF ON GROUND */
    if (BNE) goto l28;
    return;
l25:
    LDA(0);
    STA(BZ_OBJCOL + 2);
    LDA(RD(BZ_EXPOSZ + 0x0c));
    ORA(RD(BZ_EXPOSZ + 0x0d));
    if (BEQ) return;
    DEC(BZ_EXPOSZ + 0x0d);
    return;
l28:
    ORA(0);
    if (BMI) goto l60;          /* IF WE GOT HIM */
    LDA(RD(BZ_OTPOSX));         /* BACK OUT POSITION CHANGE */
    STA(BZ_TPOSX + 2);
    LDA(RD(BZ_OTPOSX + 1));
    STA(BZ_TPOSX + 3);
    LDA(RD(BZ_OTPOSY));
    STA(BZ_TPOSY + 2);
    LDA(RD(BZ_OTPOSY + 1));
    STA(BZ_TPOSY + 3);
    INC(BZ_OBJCOL + 2);         /* INDICATE COLLISION */
    return;
l60:
    LDA(0x20);                  /* BLOW'M BOTH UP */
    STA(BZ_COLFLG);
    STA(BZ_COLFLG + 2);
    LDA(2);
    STA(BZ_CRACK);
    LDA(0xff);
    STA(BZ_BOUNCE);
    LDA(1);                     /* ONLY POINTS FOR ENEMY */
    CLC();
    SED();
    ADC(RD(BZ_HITS + 2));
    CLD();
    STA(BZ_HITS + 2);
    LDX(0);
    STX(BZ_CHAN3V);
    STX(BZ_CHAN4V);
    exinit(m);
    LDA(0xff);
    STA(BZ_EXPCNT);
    LDA(RD(BZ_SNDWRD));
    AND(HIX);
    STA(BZ_SNDWRD);
    DEC(BZ_LIVES);
    if (BNE) return;
    INC(BZ_GOVER);
}

/* --- the saucer -------------------------------------------------------------------- */

/* SAUCMV: THIS ROUTINE CONTROL ENTRANCE, EXIT, & MOTION OF SAUCER */
void bz_saucmv(bz_machine *m)
{
    LDA(RD(BZ_SCOLFG));         /* IF NOT BLOWING UP */
    if (BEQ) goto l20;
    CMP(0x30);                  /* IF < 30 */
    if (BCC) goto l10;
    LDA(0x60);
    SEC();
    SBC(RD(BZ_SCOLFG));
l10:
    CLC();
    ADC(8);                     /* SET INTENSITY FOR EXPLOSION */
    ASLA();
    ASLA();
    AND(0xf0);
    STA(BZ_SINT);
    DEC(BZ_SCOLFG);             /* IF EXPLOSION NOT OVER */
    DEC(BZ_SCOLFG);
    if (BNE) return;
    LDA(RD(BZ_PRAND));          /* RESET TIMER TILL NEXT SAUCER */
    STA(BZ_STIMER);
    LDA(0);
    STA(BZ_SAUCER);
    return;
l20:
    LDA(RD(BZ_SAUCER));         /* IF NOT SAUCER */
    if (BEQ) goto l40;
    LDA(RD(BZ_SCANGL));         /* ROTATE SAUCER */
    CLC();
    ADC(8);
    STA(BZ_SCANGL);
    LDA(RD(BZ_STIMER));         /* IF DECISION TIMER */
    if (BEQ) goto l30;
    CLC();                      /* MOVE SAUCER */
    LDA(RD(BZ_SAINCX));
    ADC(RD(BZ_SAPOSX));
    STA(BZ_SAPOSX);
    LDA(RD(BZ_SAPOSX + 1));
    ADC(RD(BZ_SAINCX + 1));
    STA(BZ_SAPOSX + 1);
    LDA(RD(BZ_SAPOSY));
    CLC();
    ADC(RD(BZ_SAINCY));
    STA(BZ_SAPOSY);
    LDA(RD(BZ_SAPOSY + 1));
    ADC(RD(BZ_SAINCY + 1));
    STA(BZ_SAPOSY + 1);
l28:
    DEC(BZ_STIMER);
    return;
l30:
    LDA(RD(BZ_PRAND));          /* SET NEW MOTION VALUES */
    STA(BZ_SAINCX);
    ORA(0);
    if (BMI) goto l31;
    LDA(0);
    goto l32;
l31:
    LDA(0xff);
l32:
    STA(BZ_SAINCX + 1);
    LDA(RD(BZ_PRAND));
    STA(BZ_SAINCY);
    ORA(0);
    if (BMI) goto l35;
    LDA(0);
    goto l36;
l35:
    LDA(0xff);
l36:
    STA(BZ_SAINCY + 1);
    LDA(RD(BZ_PRAND));          /* RESET TIMER */
    LSRA();
    STA(BZ_STIMER);
    return;
l40:
    LDA(RD(BZ_HITS));           /* IF SCORE NOT TOO LOW */
    LSRA();
    if (BEQ) return;
    LDA(RD(BZ_STIMER));         /* IF NOT TIME YET */
    if (BNE) goto l28;
    LDA(RD(BZ_PRAND));          /* BRING OUT SAUCER */
    STA(BZ_SAPOSX + 1);
    LDA(RD(BZ_PRAND));
    STA(BZ_SAPOSY + 1);
    INC(BZ_SAUCER);
    goto l30;                   /* SET UP MOTION */
}

/* --- where the enemy is ------------------------------------------------------------ */

/* TRACK: THIS ROUTINE COMPUTE THE VECTOR BETWEEN THE ROBOT TANK AND THE
 * PLAYER CONTROLLED TANK, AND USING THE ARCTANGENT FUNCTION, RETURNS THE
 * ANGLE TO DIRECT THE ROBOT TANK IN. (A)=NEW ANGLE; TEMP1 = ABS VALUE(Y),
 * TEMP2 = ABS VALUE(X). */
void bz_track(bz_machine *m)
{
    bz_flags p = { 0, 0, 0, 0, 0 };

    LDY(0);                     /* INITIALIZE QUADRANT */
    SEC();                      /* HIS XPOS MINUS ROBOT XPOS */
    LDA(RD(BZ_TPOSX));
    SBC(RD(BZ_TPOSX + 2));
    STA(BZ_TEMP1);
    LDA(RD(BZ_TPOSX + 1));
    SBC(RD(BZ_TPOSX + 3));
    STA(BZ_TEMP1 + 1);
    if (BPL) goto l10;          /* IF POSITIVE */
    abstmp(m);                  /* TAKE ABSOLUTE VALUE */
    INY();                      /* X < 0 MEANS QUAD 2 OR 3 */
l10:
    LDA(RD(BZ_TEMP1));          /* MOVE ABSOLUTE VALUE */
    STA(BZ_TEMP2);
    LDA(RD(BZ_TEMP1 + 1));
    STA(BZ_TEMP2 + 1);
    LDA(RD(BZ_TEMP1 + 1));
    SEC();                      /* HIS YPOS MINUS ROBOT YPOS */
    LDA(RD(BZ_TPOSY));
    SBC(RD(BZ_TPOSY + 2));
    STA(BZ_TEMP1);
    LDA(RD(BZ_TPOSY + 1));
    SBC(RD(BZ_TPOSY + 3));
    STA(BZ_TEMP1 + 1);
    if (BPL) goto l20;          /* IF POSITIVE */
    abstmp(m);                  /* ABSOLUTE VALUE */
    TYA();                      /* Y < 0 MEANS QUAD 2 OR 3 */
    if (BEQ) goto l12;
    INY();
    goto l20;                   /* (BNE, always) */
l12:
    LDY(3);
l20:
    LDA(RD(BZ_TEMP1));          /* IF X > Y */
    CMP(RD(BZ_TEMP2));
    if (BNE) goto l25;          /* (IF NOT EQUAL) */
    LDA(RD(BZ_TEMP1 + 1));
    CMP(RD(BZ_TEMP2 + 1));
    if (BEQ) goto l22;          /* IF X=Y */
    SEC();
    goto l25;
l22:
    LDA(0x20);                  /* USE 45 DEG */
    goto l45;
l25:
    LDA(RD(BZ_TEMP1 + 1));
    SBC(RD(BZ_TEMP2 + 1));
    p.c = m->r.c; p.z = m->r.z; p.n = m->r.n; p.v = m->r.v; p.d = m->r.d;   /* PHP (SAVE STATUS) */
    if (BCC) goto l30;
    LDA(RD(BZ_TEMP1));          /* COMPUTE X/Y */
    STA(BZ_XPL);
    LDA(RD(BZ_TEMP1 + 1));
    STA(BZ_XPH);
    LDA(RD(BZ_TEMP2));
    STA(BZ_DIVOUT);
    LDA(RD(BZ_TEMP2 + 1));
    STA(BZ_DIVOUT + 1);
    STA(BZ_ZDIV);
    goto l40;
l30:
    LDA(RD(BZ_TEMP2));          /* COMPUTE Y/X */
    STA(BZ_XPL);
    LDA(RD(BZ_TEMP2 + 1));
    STA(BZ_XPH);
    LDA(RD(BZ_TEMP1));
    STA(BZ_DIVOUT);
    LDA(RD(BZ_TEMP1 + 1));
    STA(BZ_DIVOUT + 1);
    STA(BZ_ZDIV);
l40:
    bz_wait(m);
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    LSRA();                     /* DOUBLE SHIFT RIGHT */
    ROR(BZ_TEMP3);
    LSRA();
    ROR(BZ_TEMP3);
    LDX(RD(BZ_TEMP3));
    LDA(RD(BZ_ARCTAN + X));     /* GET ARCTANGENT */
    RESTORE(p);                 /* PLP: IF Y/X */
    if (BCC) goto l45;
    STA(BZ_TEMP3);              /* SUBTRACT FROM 90 DEGREES */
    LDA(0x40);
    SBC(RD(BZ_TEMP3));
l45:
    TAX();                      /* SET UP FOR DO CASE */
    TYA();
    ASLA();
    TAY();
    /* (LDA AY,TTAB+1, PHA, LDA AY,TTAB, PHA, TXA, RTS) */
    TXA();
    switch (Y >> 1) {
    case 1:                     /* T.1 */
        CLC();                  /* PFM */
        EOR(0xff);
        ADC(1);
        EOR(0x80);              /* T.2 */
        break;
    case 2:                     /* T.2 */
        EOR(0x80);
        break;
    case 3:                     /* T.3 */
        CLC();
        EOR(0xff);
        ADC(1);
        break;
    default:                    /* T.0 */
        break;
    }
}

/* OBJOBJ: THIS ROUTINE WILL CHECK FOR COLLISION BETWEEN TANKS AND
 * PLAYFIELD OBJECTS. (X)=0 FOR PLAYER TANK, (X)=2 FOR ROBOT TANK.
 * CARRY SET COLLISION, 0=NONTANK COLLISION, -1=TANK COLLISION IN (A) */
void bz_objobj(bz_machine *m)
{
    CLC();
    STX(BZ_TEMP1);              /* SAVE X */
    LDA(RD(BZ_TPOSX + X));      /* SET UP MATH BOX */
    STA(BZ_ACOS);
    LDA(RD(BZ_TPOSX + 1 + X));
    STA(BZ_ACOS + 1);
    LDA(RD(BZ_TPOSY + X));
    STA(BZ_BSIN);
    LDA(RD(BZ_TPOSY + 1 + X));
    STA(BZ_BSIN + 1);
    LDY(0);
l10:
    LDA(RD(BZ_PTBLO1 + Y));     /* IF DONE */
    if (BMI) goto l50;
    LDA(RD(BZ_PTBLX1 + Y));     /* GET DISTANCE TO OBJECT */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_PTBLX1 + 1 + Y));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_PTBLY1 + Y));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_PTBLY1 + 1 + Y));
    STA(BZ_DSTNCE);
    CMP(RD(IX(0)));
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    STA(BZ_TEMP3 + 1);
    TXA();                      /* IF ROBOT */
    if (BNE) goto l20;          /* COLLIDE SO WE CAN SEE OBJECT */
    LDA(0x80);
    CMP(RD(BZ_TEMP3));
    LDA(4);
    goto l30;                   /* (BNE, always) */
l20:
    LDA(RD(BZ_PTBLO1 + Y));
    ASLA();
    TAX();
    BIT(RD(BZ_R2D3FL));
    if (BPL) goto l25;
    LDA(RD(BZ_TEMP3 + 1));      /* 3/4 TANK RADIUS */
    CMP(0x80);
    RORA();
    STA(BZ_TEMP2 + 1);
    LDA(RD(BZ_TEMP3));
    RORA();
    CLC();
    ADC(RD(BZ_TEMP3));
    STA(BZ_TEMP3);
    LDA(RD(BZ_TEMP2 + 1));
    ADC(RD(BZ_TEMP3 + 1));
    STA(BZ_TEMP3 + 1);
    if (BCC) goto l25;
    LDX(RD(BZ_TEMP1));
    goto l40;                   /* (BCS, always) */
l25:
    LDA(RD(BZ_PROXTB + X));     /* IF COLLISION */
    CMP(RD(BZ_TEMP3));
    LDA(RD(BZ_PROXTB + 1 + X));
    LDX(RD(BZ_TEMP1));
l30:
    SBC(RD(BZ_TEMP3 + 1));
    LDA(0);                     /* SET FOR NONTANK COLLISION */
    if (BCS) goto l60;
l40:
    INY();                      /* NEXT OBJECT */
    INY();
    if (BNE) goto l10;
l50:
    LDA(RD(BZ_COLFLG));         /* IF EITHER IN COLLISION */
    ORA(RD(BZ_COLFLG + 2));
    if (BNE) goto l60;
    TXA();
    EOR(2);
    TAX();
    LDA(RD(BZ_TPOSX + X));
    STA(BZ_TPOSXT);
    LDA(RD(BZ_TPOSX + 1 + X));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_TPOSY + X));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_TPOSY + 1 + X));
    STA(BZ_DSTNCE);
    CMP(RD(IX(0)));             /* WASTE TIME */
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    STA(BZ_TEMP3 + 1);
    LDA(5);
    BIT(RD(BZ_R2D3FL));         /* SMALLER COLL. RADIUS FOR R2D3 */
    if (BPL) goto l55;
    LDA(3);
l55:
    SEC();
    SBC(RD(BZ_TEMP3 + 1));      /* CARRY SET IF COLL. */
    LDA(0xff);                  /* INDICATE TANKS */
l60:
    LDX(RD(BZ_TEMP1));          /* RESTORE X */
}

/* TR7CHK: DETERMINES IF WE HAVE OR SHOULD HAVE THE TR7 (the super tank)
 * INSTEAD OF THE OLD CLUNKER. CARRY SET=TR7 */
void bz_tr7chk(bz_machine *m)
{
    CLC();                      /* ASSUME REGULAR TANK */
    LDA(RD(BZ_NOR2D3));
    if (BMI) return;
    CMP(5);                     /* CLEAR CARRY FOR TANK */
}

/* --- the next enemy ---------------------------------------------------------------- */

/* ROBCHK: WILL RANDOMLY RESET HIS GOAL AND TAKE HIM OUT OF ATTACK MODE
 * FOR A SHORT TIME, AS WELL AS MOVING HIM TO A NEW POSITION. A tank, or,
 * past the missile level of the option switches, at random the missile. */
void bz_robchk(bz_machine *m)
{
    LDA(RD(BZ_GOVER));
    if (BNE) { bz_tankck(m); return; }
    LDA(RD(BZ_HITS + 1));
    if (BNE) goto l10;
    LDA(RD(BZ_OPTION));
    LSRA();
    LSRA();
    AND(3);
    TAX();
    LDA(RD(BZ_HITS));           /* IF NOT BUZZ BOMB */
    CMP(RD(BZ_MISLVL + X));
    if (BCC) { bz_tankck(m); return; }
l10:
    LDY(RD(BZ_PRAND));          /* FUDGE POKEY RANDOMNESS */
    TYA();
    EOR(RD(BZ_OLDRND));
    STY(BZ_OLDRND);
    LSRA();
    if (BCC) { bz_tankck(m); return; }
    LDA(2);                     /* SET SHORTER WAIT TIME */
    STA(BZ_TIMOUT);
    bz_r2d3ck(m);
}

/* TANKCK: a tank appears, somewhere in front of the player; the better
 * the player does, the wider the arc it may appear in. */
void bz_tankck(bz_machine *m)
{
    LDA(RD(BZ_PRAND));          /* RESET GOAL */
    STA(BZ_RGOAL);
    LDA(0);
    STA(BZ_R2D3FL);             /* RESET TO TANK */
    STA(BZ_COLFLG + 2);         /* OUT OF COLLISION */
    LDA(1);                     /* SET REACTION TIME SHORT */
    STA(BZ_ACTION);
    STA(BZ_TIMOUT);
    BIT(RD(BZ_ATRACT));         /* IF ATRACT PUT TANK IN FRONT */
    if (BPL) goto l10;
    LDA(RD(BZ_HITS + 1));
    if (BNE) goto l6;
    SEC();
    LDA(RD(BZ_HITS));           /* DETERMINE WINDOW FOR TANK */
    SBC(RD(BZ_HITS + 2));
    if (BCC) goto l10;
    if (BEQ) goto l10;
    CMP(7);
    if (BCC) goto l8;
l6:
    LDA(7);
l8:
    LSRA();
    if (BEQ) goto l10;
    TAX();
    LDA(0x0f);
    do {
        SEC();
        ROLA();
        DEX();
    } while (BNE);
    rob1(m);
    return;
l10:
    LDA(0x0f);
    rob1(m);
}

/* R2D3CK: the missile appears, high up, somewhere in front of the player. */
void bz_r2d3ck(bz_machine *m)
{
    INC(BZ_NOR2D3);             /* THIS WILL SET NOR2D3 TO 0 FOR FIRST */
    LDA(0x18);                  /* STARTZ/100 */
    STA(BZ_EXPOSZ + 0x0d);
    LDA(0x00);
    STA(BZ_EXPOSZ + 0x0c);
    LDA(0x80);
    STA(BZ_STATE);
    LDX(0xff);
    STX(BZ_CHAN3F);
    DEX();
    STX(BZ_CHAN4F);
    LDA(0xff);
    STA(BZ_R2D3FL);             /* PUT BUZZ BOMB IN FRONT TO HIM */
    LDA(RD(BZ_PRAND));
    AND(0x0f);
    rob1(m);
}

/* ROB1: the enemy's place: at a random angle within the window in A off
 * the player's heading, 3/4 of the unit circle's radius away (a tank at
 * random half as far). */
static void rob1(bz_machine *m)
{
    bz_flags p = { 0, 0, 0, 0, 0 };

    STA(BZ_TEMP1);
    LDA(RD(BZ_PRAND));
    AND(RD(BZ_TEMP1));
    BIT(RD(BZ_INTCT));
    if (BVS) goto l20;
    EOR(0xff);
l20:
    CLC();
    ADC(RD(BZ_TANGLE));
    STA(BZ_TEMP1);
    bz_cos(m);
    STA(BZ_TEMP2);              /* X=X-1/4*X */
    STA(BZ_X1);
    STX(BZ_X1 + 1);
    TXA();
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP2);
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP2);
    STA(BZ_TEMP2 + 1);
    LDA(RD(BZ_X1));
    SEC();
    SBC(RD(BZ_TEMP2));
    STA(BZ_TEMP2);
    LDA(RD(BZ_X1 + 1));
    SBC(RD(BZ_TEMP2 + 1));
    TAX();
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l45;
    LDA(RD(BZ_PRAND));
    LSRA();
    p.c = m->r.c; p.z = m->r.z; p.n = m->r.n; p.v = m->r.v; p.d = m->r.d;   /* PHP */
    TXA();
    if (BCC) goto l45;
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP2);
l45:
    STA(BZ_TEMP2 + 1);
    LDA(RD(BZ_TEMP1));
    bz_sin(m);
    STA(BZ_TEMP1);              /* Y=Y-1/4*Y */
    STA(BZ_Y1);
    STX(BZ_Y1 + 1);
    TXA();
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP1);
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP1);
    STA(BZ_TEMP1 + 1);
    SEC();
    LDA(RD(BZ_Y1));
    SBC(RD(BZ_TEMP1));
    STA(BZ_TEMP1);
    LDA(RD(BZ_Y1 + 1));
    SBC(RD(BZ_TEMP1 + 1));
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l49;
    RESTORE(p);                 /* PLP */
    if (BCC) goto l49;
    CMP(0x80);
    RORA();
    ROR(BZ_TEMP1);
l49:
    STA(BZ_TEMP1 + 1);          /* SET UP STARTING POS */
    CLC();
    LDA(RD(BZ_TPOSX));
    ADC(RD(BZ_TEMP2));
    STA(BZ_TPOSX + 2);
    LDA(RD(BZ_TPOSX + 1));
    ADC(RD(BZ_TEMP2 + 1));
    STA(BZ_TPOSX + 3);
    LDA(RD(BZ_TPOSY));
    CLC();
    ADC(RD(BZ_TEMP1));
    STA(BZ_TPOSY + 2);
    LDA(RD(BZ_TPOSY + 1));
    ADC(RD(BZ_TEMP1 + 1));
    STA(BZ_TPOSY + 3);
    BIT(RD(BZ_R2D3FL));
    if (BPL) goto l50;
    bz_track(m);                /* SET UP STARTING ANGLE */
    STA(BZ_TANGLE + 2);
    STA(BZ_RGOAL);
l50:
    LDA(0);
    STA(BZ_EIRNGE);
    STA(BZ_FTIMER);
}

/* --- the radar ---------------------------------------------------------------------- */

/* DRADAR: THIS ROUTINE WILL DISPLAY RADAR SWEEP AND BLIP OF OTHER TANK, IF
 * WITHIN FIRING RANGE. Also the distance to him (TDIST), the missile's
 * sound by it, the "ENEMY IN RANGE" message and warning, and the next
 * enemy when this one has got too far away. */
void bz_dradar(bz_machine *m)
{
    m->tag = BZ_TAG(BZ_TAG_RADAR, 0);
    LDX(BZ_RDRING & 0xff);
    LDA(BZ_RDRING >> 8);
    bz_vgjsrl(m);
    LDA(0);
    STA(BZ_XCOMP + 1);
    STA(BZ_YCOMP + 1);
    STA(BZ_CURNTX + 1);
    LDA(0x0b);                  /* INCREMENT SWEEP ANGLE */
    CLC();
    ADC(RD(BZ_SANGLE));
    STA(BZ_SANGLE);
    bz_sin(m);                  /* SIN/2 */
    TXA();
    CMP(0x80);
    RORA();
    STA(BZ_XCOMP);
    STA(BZ_CURNTX);
    if (BPL) goto l10;
    LDA(0xff);
    STA(BZ_XCOMP + 1);
    STA(BZ_CURNTX + 1);
l10:
    LDA(RD(BZ_SANGLE));         /* COS/2 */
    bz_cos(m);
    TXA();
    CMP(0x80);
    RORA();
    STA(BZ_XCOMP + 2);
    if (BPL) goto l20;
    LDA(0xff);
    STA(BZ_XCOMP + 3);
l20:
    LDA(0xa0);                  /* DRAW BRIGHT VECTOR */
    STA(BZ_VGBRIT);
    LDA(0x3c);                  /* CENTRY&0FF */
    CLC();
    ADC(RD(BZ_XCOMP + 2));
    STA(BZ_CURNTY);
    LDA(0x01);                  /* CENTRY/100 */
    ADC(RD(BZ_XCOMP + 3));
    STA(BZ_CURNTY + 1);
    m->tag = BZ_TAG(BZ_TAG_SWEEP, 0);
    bz_vgvtr2(m);
    m->tag = BZ_TAG(BZ_TAG_BLIP, 0);
    LDA(RD(BZ_COLFLG + 2));     /* IF NOT EXPLODING */
    if (BEQ) goto l21;
    LDA(0);
    STA(BZ_CHAN3V);
    STA(BZ_CHAN4V);
    return;
l21:
    bz_track(m);                /* TRACK THE ROBOT */
    EOR(0x80);
    SEC();
    STA(BZ_GARBAG);             /* RELATIVE ANGLE OF ROBOT */
    LDA(RD(BZ_TANGLE));
    SBC(RD(BZ_GARBAG));
    STA(BZ_GARBAG);
    LDA(RD(BZ_SANGLE));         /* IF NOT ON SWEEP LINE */
    SEC();
    SBC(RD(BZ_GARBAG));
    if (BMI) goto l23;
    CMP(0x0c);
    if (BCS) goto l23;
    LDA(0xf0);                  /* SET BRIGHTNESS OF BLIP */
    STA(BZ_BLIP);
l23:
    LDA(RD(BZ_TEMP1));          /* COMPUTE DISTANCE */
    STA(BZ_TPOSXT);
    LDA(RD(BZ_TEMP1 + 1));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_TEMP2));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_TEMP2 + 1));
    STA(BZ_TPOSYT + 1);
    STA(BZ_DST2);
    CMP(RD(IX(0)));
    LDA(RD(BZ_MOUTLO));
    STA(BZ_TEMP3);
    LDA(RD(BZ_MOUTHI));
    STA(BZ_TDIST);
    TAX();
    if (BMI) goto l24;
    LSRA();                     /* SET VOLUME */
    LSRA();
    LSRA();
    AND(0x0f);
    EOR(0xaf);
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l25;
l24:
    LDA(0);
l25:
    STA(BZ_CHAN3V);
    STA(BZ_CHAN4V);
    TXA();
    STA(BZ_XMATRX);
    CMP(0x80);                  /* IF OUT OF RANGE */
    if (BCC) goto l35;
    LDA(0);
    STA(BZ_EIRNGE);             /* OUT OF RANGE */
    BIT(RD(BZ_R2D3FL));
    if (BPL) goto dr_blp;
    LDA(RD(BZ_TIMOUT));         /* IF NOT TIMED OUT ON R2D3'S */
    CMP(4);
    if (BCC) goto l27;
    bz_tankck(m);
    goto dr_blp;
l27:
    bz_r2d3ck(m);
    goto dr_blp;
l35:
    LDA(RD(BZ_BLIP));           /* IF SWEEP TOO LATE */
    if (BEQ) return;
    CMP(0xf0);
    if (BCC) goto l40;
    LDA(BZ_S_RBEEP);
    bz_sndon(m);
l40:
    LDA(RD(BZ_GARBAG));         /* SETUP SIN FOR MATRIX MULT */
    bz_sin(m);
    STA(BZ_ACOS);
    STX(BZ_ACOS + 1);
    LDA(RD(BZ_GARBAG));
    bz_cos(m);                  /* SETUP COS FOR MATRIX MULT */
    STA(BZ_BSIN);
    STX(BZ_BSIN + 1);
    LDA(0x00);                  /* CENTRX&0FF */
    STA(BZ_TPOSXT);
    LDA(0x00);                  /* CENTRX/100 */
    STA(BZ_TPOSXT + 1);
    LDA(0x3c);                  /* CENTRY&0FF */
    STA(BZ_TPOSYT);
    LDA(0x01);                  /* CENTRY/100 */
    STA(BZ_TPOSYT + 1);
    LDA(0);
    STA(BZ_XMATRX + 1);
    STA(BZ_VGBRIT);             /* SET ZERO BRITE */
    STA(BZ_YMATRX);             /* START MATRIX MULT */
    STA(BZ_YSTRT2);
    bz_wait(m);                 /* SCALE RESULT */
    STA(BZ_DXP);
    LDA(RD(BZ_MOUTLO));
    SEC();
    SBC(RD(BZ_CURNTX));
    STA(BZ_XCOMP);
    LDA(RD(BZ_MOUTHI));
    STA(BZ_DYPH);               /* RESTART MULTIPLIER */
    SBC(RD(BZ_CURNTX + 1));
    STA(BZ_XCOMP + 1);
    LDA(RD(BZ_MOUTLO));
    SEC();
    SBC(RD(BZ_CURNTY));
    STA(BZ_YCOMP);
    LDA(RD(BZ_MOUTHI));
    SBC(RD(BZ_CURNTY + 1));
    STA(BZ_YCOMP + 1);
    bz_vgvtr2(m);
    LDA(RD(BZ_BLIP));           /* DRAW BLIP */
    AND(0xe0);
    bz_vgdot(m);
    LDA(RD(BZ_VGBRIT));
    bz_vgdot(m);
    BIT(RD(BZ_ATRACT));         /* IF IN GAME */
    if (BMI) goto d5;
    BIT(RD(BZ_LOGO));           /* IF LOGO IS BEING DRAWN */
    if (BMI) goto d10;
d5:
    LDA(2);
    AND(RD(BZ_FRAME));
    if (BEQ) goto d10;
    LDX(0x10);                  /* ERANGE: INDICATE IN RANGE */
    bz_msgs(m);
d10:
    LDA(RD(BZ_EIRNGE));         /* IF ALREADY IN RANGE */
    if (BNE) goto dr_blp;       /* DRAW BLIP */
    INC(BZ_EIRNGE);             /* INDICATE IN RANGE */
    LDA(BZ_S_WARNG);            /* OUTPUT WARNING SOUND */
    bz_sndon(m);
dr_blp:
    LDA(RD(BZ_BLIP));           /* IF SWEEP WAY PAST */
    if (BEQ) return;
    SEC();
    SBC(8);
    STA(BZ_BLIP);
}
