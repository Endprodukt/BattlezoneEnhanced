/* view.c - BZONE.MAC: what is seen. The mountains and the volcano
 * (MOUNTS, VOLCNO), the objects in view (ROTATE, ROTPNT, EXPLDE), their
 * points through the math box (PNTPUT, KLUDGE) and their lines into the
 * display list (DRAW); sines and cosines (SIN, COS, SINXTR, COSXTR).
 *
 * Angles are a byte, 00..FF for the full circle; the player's has a ninth
 * bit below it (LANGLE D7). Positions are 16 bits and wrap: the world is
 * a torus 65536 units around. The math box turns a point of the world
 * into the player's view (X' ahead, Y' to the side), and divides for the
 * perspective. */
#include <math.h>
#include "game/game.h"
#include "game/m6502.h"

#define NOROCK  BZ_NOROCK
#define GRAVTY  0xfc    /* -4 */

static void rotpnt(bz_machine *m);
static void explde(bz_machine *m);
static void kludge(bz_machine *m);
static void dovect(bz_machine *m);

/* WAIT: WAIT FOR 2901 MATH BOX & RETURN LOW BYTE IN A */
void bz_wait(bz_machine *m)
{
    do {
        BIT(RD(BZ_MSTAT));      /* DO WHILE NOT DONE */
    } while (BMI);
    LDA(RD(BZ_MOUTLO));
}

/* --- sines ---------------------------------------------------------------------- */

/* SIN1: SIN(A) WHEN PI>A>=0; the table has 0..90 degrees. */
static void sin1(bz_machine *m)
{
    uint8_t t;
    CMP(0x41);                  /* IF PI/2 >-ANGLE >-0 */
    if (BCC) goto l10;
    EOR(0x7f);                  /* SIN(PI/2+A)=SIN(PI/2-A) */
    ADC(0);
l10:
    ASLA();
    TAX();
    LDA(RD(BZ_SINTBL + X));     /* LOW ORDER */
    t = A;
    LDA(RD(BZ_SINTBL + 1 + X)); /* HIGH ORDER */
    TAX();
    PLA(t);
}

/* SIN: (A)=ANGLE (0 TO FF REPRESENTS 0 TO 360 DEGREES), the flags set for
 * it. Returns (A)=LOW ORDER BYTE, (X)=HIGH; 7FFF is 1. */
void bz_sin(bz_machine *m)
{
    uint8_t t;
    if (BPL) { sin1(m); return; }   /* IF PI>ANGLE>-0 */
    AND(0x7f);
    sin1(m);
    EOR(0xff);
    CLC();
    ADC(1);
    t = A;
    TXA();
    EOR(0xff);
    ADC(0);
    TAX();
    PLA(t);
}

/* COS(A)=SIN(A+PI/2) */
void bz_cos(bz_machine *m)
{
    CLC();
    ADC(0x40);
    bz_sin(m);
}

/* XTRAP: the value halfway between TEMP1 and (X,A). */
static void xtrap(bz_machine *m)
{
    uint8_t t;
    SEC();                      /* COMPUTE DIFFERENCE */
    SBC(RD(BZ_TEMP1));
    STA(BZ_TEMP2);
    TXA();
    SBC(RD(BZ_TEMP1 + 1));
    CMP(0x80);                  /* DIVIDE BY 2 */
    RORA();
    ROR(BZ_TEMP2);
    STA(BZ_TEMP2 + 1);
    LDA(RD(BZ_TEMP1));          /* ADD IN DIFFERENCE CORRECTLY */
    CLC();
    ADC(RD(BZ_TEMP2));
    t = A;
    LDA(RD(BZ_TEMP1 + 1));
    ADC(RD(BZ_TEMP2 + 1));
    TAX();                      /* GET VALUES IN REGS */
    PLA(t);
}

/* COSXTR: WILL EXTRAPOLATE SIN & COS TO AN EXTRA 2 DIVISIONS BETWEEN EACH
 * ANGLE. (X)=ANGLE, (A)=DIVISION OF ANGLE (0 TO 1). Leaves it in GARBAG. */
void bz_cosxtr(bz_machine *m)
{
    AND(1);                     /* IF DIV D0 */
    STA(BZ_GARBAG);
    if (BNE) goto l10;
    TXA();                      /* GIVE 'EM COS */
    bz_cos(m);
    return;
l10:
    TXA();                      /* GET HI BYTE OF ANGLE */
    STA(BZ_TEMP3);              /* SAVE ANGLE */
    bz_cos(m);                  /* COS OF ANGLE */
    STA(BZ_TEMP1);
    STX(BZ_TEMP1 + 1);
    INC(BZ_TEMP3);              /* COS OF NEXT ANGLE */
    LDA(RD(BZ_TEMP3));
    bz_cos(m);
    xtrap(m);
}

void bz_sinxtr(bz_machine *m)
{
    AND(1);                     /* IF DIV>0 */
    STA(BZ_GARBAG);
    if (BNE) goto l10;
    TXA();                      /* GIVE 'EM SIN */
    bz_sin(m);
    return;
l10:
    TXA();
    STA(BZ_TEMP3);
    bz_sin(m);                  /* SIN OF ANGLE */
    STA(BZ_TEMP1);
    STX(BZ_TEMP1 + 1);
    INC(BZ_TEMP3);              /* SIN OF NEXT ANGLE */
    LDA(RD(BZ_TEMP3));
    bz_sin(m);
    xtrap(m);
}

/* --- the horizon ------------------------------------------------------------------ */

/* 50$ of MOUNTS: one of the eight sections of the mountains. */
static void mtn(bz_machine *m)
{
    AND(0x0e);
    TAY();
    LDA(RD(BZ_MTNS + Y));
    INY();
    LDX(RD(BZ_MTNS + Y));
    INY();
    STY(BZ_TEMP1);
    bz_vgadd2(m);               /* DRAW MOUNTAINS */
    LDA(RD(BZ_TEMP1));
}

/* MOUNTS: DOES A LABS TO CORRECT PLACE TO START FOR MOUNTAINS. SETS UP
 * JSRL TO MOUNTAINS ROUTINE IN ROM. Then the rocks the volcano throws. */
void bz_mounts(bz_machine *m)
{
    uint8_t t;

    /* (the place is the angle's top 12 bits, and the vector generator
     * takes the top ten of a vector's length: on the screen the mountains
     * stand on steps of the angle's top 11 bits, 0.996 units each. What
     * the lowest 5 would add, for a picture between two frames.) */
    m->mount_shift = m->fine ? (float)(m->mem[BZ_LANGLE] & 0x1f) / 32.0f * 0.996f : 0.0f;
    m->tag = BZ_TAG(BZ_TAG_MOUNT, 0);
    LDX(0);                     /* LDA VIEW FOR 1 BIT RESOLUTION */
    STX(BZ_XCOMP + 3);
    STX(BZ_XCOMP + 2);
    LDA(RD(BZ_LANGLE));
    STA(BZ_XCOMP);
    LDA(RD(BZ_TANGLE));         /* X=ANGLE (9 BITS) */
    LSRA();                     /* THIS MEANS IT WRAPS AROUND EVERY 360 DEGRS */
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    t = A;
    AND(0x01);
    ORA(0x02);                  /* 512 TO 1023 RANGE */
    STA(BZ_XCOMP + 1);
    LDA(RD(BZ_BOUNCE));         /* PROVIDE BOUNCE */
    LSRA();
    LSRA();
    LSRA();
    LSRA();
    if (BEQ) goto l20;
    STA(BZ_TEMP1);
    SEC();
    LDA(RD(BZ_XCOMP + 2));
    SBC(RD(BZ_TEMP1));
    STA(BZ_XCOMP + 2);
    LDA(0xff);
    STA(BZ_XCOMP + 3);
l20:
    bz_vgcntr(m);               /* CENTER BEAM */
    bz_vgvtr2(m);               /* POSITION BEAM */
    LDA(BZ_HORIZN >> 8);
    LDX(BZ_HORIZN & 0xff);
    bz_vgjsrl(m);               /* ADD HORIZON LINE */
    PLA(t);
    EOR(0x0f);
    mtn(m);                     /* DRAW 3/2 FULL SCREEN */
    mtn(m);                     /* TO INSURE FULL VIEW */
    mtn(m);
    LDA(RD(BZ_LANGLE));         /* SET UP FOR SIFT */
    STA(BZ_XCOMP);
    LDA(RD(BZ_TANGLE));         /* IF VOLCANO NOT ON SCREEN */
    CMP(0x88);
    if (BCS) return;
    CMP(0x39);
    if (BCC) return;
    LSRA();                     /* DUBLE SIFT 4 TO SCALE */
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    LSRA();
    ROR(BZ_XCOMP);
    STA(BZ_XCOMP + 1);
    LDA(RD(BZ_XCOMP));
    SEC();
    SBC(5);
    STA(BZ_XCOMP);
    LDA(RD(BZ_XCOMP + 1));
    SBC(6);
    STA(BZ_XCOMP + 1);
    CLC();
    LDA(0x5e);
    ADC(RD(BZ_XCOMP + 2));
    STA(BZ_XCOMP + 2);
    if (BCC) goto l25;
    INC(BZ_XCOMP + 3);
l25:
    m->tag = BZ_TAG(BZ_TAG_VOLCANO, 0);
    bz_vgcntr(m);               /* MOVE BEAM TO TOP OF VOLCANO */
    bz_vgvtr2(m);
    LDA(0);
    STA(BZ_CURNTX);
    STA(BZ_CURNTX + 1);
    STA(BZ_CURNTY);
    STA(BZ_CURNTY + 1);
    LDX(NOROCK - 1);
    do {
        LDA(RD(BZ_OBJTIM + X)); /* IF NOT ACTIVE */
        if (BEQ) goto l40;
        m->tag = BZ_TAG(BZ_TAG_VOLCANO, 1 + X);
        LDA(RD(BZ_XPOSL + X));  /* DELTA X */
        SEC();
        TAY();
        SBC(RD(BZ_CURNTX));
        STA(BZ_XCOMP);
        STY(BZ_CURNTX);
        LDA(RD(BZ_XPOSH + X));
        TAY();
        SBC(RD(BZ_CURNTX + 1));
        STA(BZ_XCOMP + 1);
        STY(BZ_CURNTX + 1);
        SEC();                  /* DELTA Y */
        LDA(RD(BZ_YPOSL + X));
        TAY();
        SBC(RD(BZ_CURNTY));
        STA(BZ_XCOMP + 2);
        STY(BZ_CURNTY);
        LDA(RD(BZ_YPOSH + X));
        TAY();
        SBC(RD(BZ_CURNTY + 1));
        STA(BZ_XCOMP + 3);
        STY(BZ_CURNTY + 1);
        LDA(0);                 /* VECTOR TO POINT */
        STA(BZ_VGBRIT);
        STX(BZ_TEMP1);
        bz_vgvtr2(m);
        LDX(RD(BZ_TEMP1));
        LDA(RD(BZ_OBJTIM + X));
        ASLA();
        ASLA();
        ASLA();
        AND(0xe0);
        bz_vgdot(m);
        LDX(RD(BZ_TEMP1));
l40:
        DEX();
    } while (BPL);
}

/* VOLCNO: VOLCANO ROCK MOVEMENT. CALLED ONCE A FRAME. WILL MOVE UP TO 5
 * ROCKS AT A TIME. XPOS(2 BYTES) & YPOS(2 BYTES) ARE RELATIVE TO THE
 * VOLCANO TOP. (THIS ROUTINE MAY BE BLAMED ON OWEN RUBIN) */
void bz_volcno(bz_machine *m)
{
    LDX(NOROCK - 1);            /* 5 ROCKS */
l10:
    LDA(RD(BZ_OBJTIM + X));     /* THIS ROCK ACTIVE? */
    if (BNE) goto l30;          /* YES */
    LDA(RD(BZ_PRAND));          /* NOT ACTIVE, SHOULD WE START ONE? */
    AND(7);                     /* (THIS # MAY CHANGE) */
    if (BNE) goto l50;          /* DON'T START THIS TIME */
    LDA(0x1f);                  /* ROCK ACTIVE TIME */
    STA(BZ_OBJTIM + X);
    LDA(RD(BZ_PRAND));
    AND(3);
    ADC(1);                     /* SOME RANDOM HORIZ POSITION */
    BIT(RD(BZ_PRAND));          /* LEFT OR RIGHT */
    if (BVC) goto l11;
    EOR(0xff);                  /* CHANGE HORIZ SPEED(+-1) */
l11:
    STA(BZ_XSPD + X);
    LDA(RD(BZ_PRAND));
    AND(7);
    ADC(5);                     /* SOME RANDOM VERT VELOCITY */
    STA(BZ_YSPD + X);
l12:
    LDA(0);
    STA(BZ_XPOSL + X);
    STA(BZ_XPOSH + X);
    STA(BZ_YPOSL + X);
    STA(BZ_YPOSH + X);
    goto l50;                   /* ALWAYS */
l30:
    DEC(BZ_OBJTIM + X);
    if (BEQ) goto l12;
    DEC(BZ_YSPD + X);           /* GRAVITY TAKES HOLD */
    LDY(0);                     /* MOVE ROCKS */
    CLC();
    LDA(RD(BZ_YSPD + X));
    if (BPL) goto l31;
    DEY();                      /* A MINUS #, SIGN EXTEND */
l31:
    ADC(RD(BZ_YPOSL + X));
    STA(BZ_YPOSL + X);
    LDA(RD(BZ_YPOSH + X));
    if (BPL) goto l33;
    LDA(RD(BZ_YPOSL + X));      /* LOW ENOUGH */
    CMP(0xa2);
    if (BCS) goto l33;
    LDA(0);
    STA(BZ_OBJTIM + X);
    goto l12;                   /* ALWAYS */
l33:
    TYA();                      /* GET MSB */
    ADC(RD(BZ_YPOSH + X));
    STA(BZ_YPOSH + X);
    LDY(0);                     /* MSB 0(GUESS) */
    CLC();
    LDA(RD(BZ_XSPD + X));
    if (BPL) goto l32;
    DEY();                      /* AS ABOVE, BUT FOR X */
l32:
    ADC(RD(BZ_XPOSL + X));
    STA(BZ_XPOSL + X);
    TYA();
    ADC(RD(BZ_XPOSH + X));
    STA(BZ_XPOSH + X);          /* DONE MOVED THEM! */
l50:
    DEX();
    if (BPL) goto l10;
}

/* --- which objects are in view ------------------------------------------------------- */

/* For a picture between two frames (bz_machine.fine): sine and cosine as
 * the tables have them (32767 times), of an angle of 16 bits a turn. */
static void fine_sincos(unsigned angle, int *s, int *c)
{
    double th = (double)(angle & 0xffff) / 65536.0 * 6.283185307179586;
    *s = (int)lround(32767.0 * sin(th));
    *c = (int)lround(32767.0 * cos(th));
}

/* (BSIN and ACOS are the math box's inputs, not memory) */
static void put16(bz_machine *m, unsigned at, int v)
{
    bz_wr8(m, (uint16_t)at, (uint8_t)v);
    bz_wr8(m, (uint16_t)(at + 1), (uint8_t)((unsigned)v >> 8));
}

/* ROTATE's BSIN and ACOS (minus the sine and the cosine of the view's
 * angle), of all of the angle. */
static void fine_view(bz_machine *m)
{
    int s, c;
    fine_sincos((unsigned)(m->mem[BZ_TANGLE] << 8 | m->mem[BZ_LANGLE]), &s, &c);
    put16(m, BZ_BSIN, -s);
    put16(m, BZ_ACOS, c);
}

/* PNTPUT's (the sine and minus the cosine of the object's angle as the
 * view sees it, turned half round), of all of both angles. X: the
 * object's place in PTBLO2. */
static void fine_object(bz_machine *m)
{
    unsigned x = m->mem[BZ_SAVE1];
    unsigned view = (unsigned)(m->mem[BZ_TANGLE] << 8 | m->mem[BZ_LANGLE]);
    unsigned obj = (unsigned)(m->mem[(BZ_PTBLO2 + 1 + x) & 0x3ff] << 8 | m->vis_lo[(x >> 1) & 31]);
    int s, c;
    fine_sincos((view - obj) ^ 0x8000, &s, &c);
    put16(m, BZ_BSIN, s);
    put16(m, BZ_ACOS, -c);
}

/* ROTATE: WILL SCAN THE PLAYFIELD TABLE FOR OBJECT AND TRANSLATE AND
 * ROTATE THE CENTER POINT OF EACH OBJECT FOUND AND THEN DETERMINE IF IT
 * WILL BE IN THE FIELD OF VIEW. IF SO, THE OBJECT WILL THEN BE MOVED TO A
 * NEW TABLE (PTBLX2, PTBLY2, PTBLO2), THE OBJECTS IN WHICH WILL BE
 * PROJECTED. First the enemy (or the pieces he blew into), his shell and
 * the player's, the saucer; then the playfield. */
void bz_rotate(bz_machine *m)
{
    uint8_t t;

    LDY(0);
    LDA(RD(BZ_TPOSX));
    STA(BZ_TPOSXT);
    LDA(RD(BZ_TPOSX + 1));
    STA(BZ_TPOSXT + 1);
    LDA(RD(BZ_TPOSY));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_TPOSY + 1));
    STA(BZ_TPOSYT + 1);
    LDA(RD(BZ_LANGLE));         /* GET ANGLE DIVISIONS */
    CLC();
    ROLA();
    ROLA();
    LDX(RD(BZ_TANGLE));
    bz_sinxtr(m);
    EOR(0xff);
    ADC(1);
    STA(BZ_BSIN);
    TXA();
    EOR(0xff);
    ADC(0);
    STA(BZ_BSIN + 1);
    LDA(RD(BZ_GARBAG));         /* THIS IS CHEATING */
    LDX(RD(BZ_TANGLE));
    bz_cosxtr(m);
    STA(BZ_ACOS);
    STX(BZ_ACOS + 1);
    if (m->fine) fine_view(m);  /* (a picture between two frames: all 16 bits of the angle) */
    BIT(RD(BZ_LOGO));           /* IF DISPLAYING LOGO */
    if (BPL) goto l33;
    LDX(0);
    goto r10;
l33:
    LDA(RD(BZ_COLFLG + 2));     /* IF IN COLLISION & FLASHING */
    if (BEQ) goto l1;
    explde(m);
    goto r4;
l1:
    LDA(RD(BZ_TANGLE + 2));     /* SET UP ORIENTATION */
    STA(BZ_PTBLO2 + 1);
    LDA(0x16);
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto l3;
    LDX(0x21);
    bz_tr7chk(m);
    if (BCS) goto l15;
    LDX(2);
l15:
    TXA();
l3:
    STA(BZ_PTBLO2);
    m->cur_id = A == 0x16 ? BZ_ID_MISSILE : A == 0x21 ? BZ_ID_SUPERTANK : BZ_ID_TANK;
    m->enemy_id = m->cur_id;
    LDA(RD(BZ_TPOSX + 2));
    STA(BZ_XMATRX);
    LDA(RD(BZ_TPOSX + 3));
    STA(BZ_XMATRX + 1);
    LDA(RD(BZ_TPOSY + 2));
    STA(BZ_YMATRX);
    LDA(RD(BZ_TPOSY + 3));
    STA(BZ_YSTRT1);
    rotpnt(m);                  /* ROTATE & SELECT */
    TYA();                      /* IF TANK NOT IN VIEW */
    if (BEQ) goto r4;
    LDA(RD(BZ_PTBLO2 + 1));     /* SET UP ORIENTATION OF TREADS */
    STA(BZ_PTBLO2 + 1 + Y);
    LDA(RD(BZ_PTBLX2));         /* SETUP CENTER POINTS */
    STA(BZ_PTBLX2 + Y);
    LDA(RD(BZ_PTBLX2 + 1));
    STA(BZ_PTBLX2 + 1 + Y);
    LDA(RD(BZ_PTBLY2));
    STA(BZ_PTBLY2 + Y);
    LDA(RD(BZ_PTBLY2 + 1));
    STA(BZ_PTBLY2 + 1 + Y);
    BIT(RD(BZ_R2D3FL));         /* IF TANK, NOT R2D3 */
    if (BPL) goto l6;
    LDA(RD(BZ_TRDCTR + 2));     /* DETERMINE EGGZAWST PICTURE */
    AND(7);
    CLC();
    ADC(0x24);
    goto l4;                    /* (BNE, always) */
l6:
    bz_tr7chk(m);               /* IF TR7 */
    if (BCS) goto r4;
    LDA(RD(BZ_PTBLO2 + 1));     /* DETERMINE IF WE SEE FRONT OR BACK */
    SEC();
    SBC(RD(BZ_TANGLE));
    CLC();
    ADC(0x40);                  /* 90 DEGREES */
    ASLA();
    LDA(4);
    if (BCC) goto l2;
    ASLA();
l2:
    STA(BZ_TEMP1);              /* COMPUTE OBJECT # */
    LDA(RD(BZ_TRDCTR + 2));
    AND(3);
    CLC();
    ADC(RD(BZ_TEMP1));
l4:
    STA(BZ_PTBLO2 + Y);
    m->vis_id[(Y >> 1) & 31] = A >= 0x24 ? BZ_ID_EXHAUST : BZ_ID_TREAD;
    m->vis_lo[(Y >> 1) & 31] = m->mem[BZ_LANGLE + 2];
    INY();
    INY();
    BIT(RD(BZ_R2D3FL));
    if (BMI) goto r4;
    LDA(0x0d);                  /* OUTPUT RADAR ANT. */
    STA(BZ_PTBLO2 + Y);
    m->vis_id[(Y >> 1) & 31] = BZ_ID_ANTENNA;
    m->vis_lo[(Y >> 1) & 31] = m->antenna_lo;
    LDA(RD(BZ_RANGLE));
    STA(BZ_PTBLO2 + 1 + Y);     /* ORIENTATION */
    LDA(RD(BZ_TANGLE));         /* COMPUTE RELATIVE ORIENTATION */
    SEC();
    SBC(RD(BZ_TANGLE + 2));
    STA(BZ_NTHETA);
    bz_cos(m);                  /* COMPUTE CENTER OF RADAR */
    TXA();
    LDX(0xff);
    EOR(0xff);
    if (BMI) goto l5;
    INX();
l5:
    CLC();
    ADC(1);
    if (BCC) goto l10;
    INX();
l10:
    ASLA();
    t = A;
    TXA();
    ROLA();
    TAX();
    PLA(t);
    CLC();
    ADC(RD(BZ_PTBLX2));
    STA(BZ_PTBLX2 + Y);
    TXA();
    ADC(RD(BZ_PTBLX2 + 1));
    STA(BZ_PTBLX2 + 1 + Y);
    LDA(RD(BZ_NTHETA));
    bz_sin(m);
    TXA();
    ASLA();
    LDX(0);
    if (BCC) goto l21;
    LDX(0xff);
l21:
    CLC();
    ADC(RD(BZ_PTBLY2));
    STA(BZ_PTBLY2 + Y);
    TXA();
    ADC(RD(BZ_PTBLY2 + 1));
    STA(BZ_PTBLY2 + 1 + Y);
    INY();
    INY();
r4:
    LDX(2);
r5:
    LDA(RD(BZ_FIRECT + X));     /* IF SHELL NOT FIRED */
    if (BEQ) goto r8;
    LDA(RD(BZ_TANGLE + X));     /* SET UP ANGLE */
    STA(BZ_PTBLO2 + 1 + Y);
    LDA(3);                     /* SET UP OBJ # */
    STA(BZ_PTBLO2 + Y);
    LDA(RD(BZ_FIRECT + X));     /* IF NOT EXPLODING */
    if (BPL) goto r6;
    CLC();
    ADC(0x0f);
    if (BMI) goto s10;
    LDA(0);                     /* END EXPLOSION */
    STA(BZ_FIRECT + X);
    goto r8;
s10:
    STA(BZ_FIRECT + X);
    EOR(0xff);
    CLC();
    ADC(RD(BZ_PTBLX2 + 1 + Y));
    STA(BZ_PTBLO2 + 1 + Y);
    LDA(0x0e);
    STA(BZ_PTBLO2 + Y);
r6:
    m->cur_id = m->mem[(BZ_PTBLO2 + Y) & 0x3ff] == 0x0e ? (X ? BZ_ID_ESHELL_HIT : BZ_ID_SHELL_HIT)
                                                        : (X ? BZ_ID_ESHELL : BZ_ID_SHELL);
    LDA(RD(BZ_SHELLX + X));
    STA(BZ_XMATRX);
    LDA(RD(BZ_SHELLX + 1 + X));
    STA(BZ_XMATRX + 1);
    LDA(RD(BZ_SHELLY + X));
    STA(BZ_YMATRX);
    LDA(RD(BZ_SHELLY + 1 + X));
    STA(BZ_YSTRT1);
    rotpnt(m);                  /* ROTATE & SELECT */
r8:
    DEX();
    DEX();
    if (BPL) goto r5;
    LDA(RD(BZ_SAUCER));         /* IF NOT SAUCER */
    if (BEQ) goto l40;
    m->cur_id = BZ_ID_SAUCER;
    LDA(0x20);                  /* ROTATE & SELECT */
    STA(BZ_PTBLO2 + Y);
    LDA(RD(BZ_SCANGL));
    STA(BZ_PTBLO2 + 1 + Y);
    LDA(RD(BZ_SAPOSX));
    STA(BZ_XMATRX);
    LDA(RD(BZ_SAPOSX + 1));
    STA(BZ_XMATRX + 1);
    LDA(RD(BZ_SAPOSY));
    STA(BZ_YMATRX);
    LDA(RD(BZ_SAPOSY + 1));
    STA(BZ_YSTRT1);
    rotpnt(m);
    LDA(RD(BZ_PTBLO2 - 2 + Y)); /* IF SAUCER NOT IN VIEW */
    CMP(0x20);
    if (BNE) goto l20;
    LDA(0x81);                  /* SET UP SOUND */
    STA(BZ_SAUCER);
    goto l40;
l20:
    LDA(1);                     /* TURN OFF SOUND */
    STA(BZ_SAUCER);
l40:
    LDX(0);
r10:
    LDA(RD(BZ_PTBLO1 + X));     /* IF END OF TABLE */
    if (BMI) goto rotund;
    STA(BZ_PTBLO2 + Y);         /* MOVE OBJECT # TO TABLE */
    m->cur_id = (uint8_t)(BZ_ID_FIELD + (X >> 1));
    LDA(RD(BZ_PTBLO1 + 1 + X)); /* MOVE ORENTATION TO TABLE */
    STA(BZ_PTBLO2 + 1 + Y);
    LDA(RD(BZ_PTBLX1 + X));
    STA(BZ_XMATRX);
    LDA(RD(BZ_PTBLX1 + 1 + X));
    STA(BZ_XMATRX + 1);
    LDA(RD(BZ_PTBLY1 + X));
    STA(BZ_YMATRX);
    LDA(RD(BZ_PTBLY1 + 1 + X));
    STA(BZ_YSTRT1);
    rotpnt(m);                  /* ROTATE & SELECT */
    INX();                      /* NEXT TABLE LOCATION */
    INX();
    if (BNE) goto r10;
rotund:
    STA(BZ_PTBLO2 + Y);         /* INDICATE END OF TBL */
}

/* ROTPNT: the math box has turned a point into the player's view (X' at
 * its output, Y' to come). PUT IN TABLE IF IN VIEW: ahead, not too near
 * nor too far, and less to the side than ahead. Y=INDEX TO RAM TABLE,
 * moved on if the point is taken. */
static void rotpnt(bz_machine *m)
{
    bz_wait(m);                 /* READ MATH BOX */
    ASLA();
    STA(BZ_X2);
    LDA(RD(BZ_MOUTHI));
    STA(BZ_RSTRT1);
    if (BMI) return;
    ROLA();
    if (BMI) return;            /* IF TOO FAR THROW OUT */
    STA(BZ_X2 + 1);
    LSRA();                     /* IF TOO CLOSE, THROW OUT */
    LSRA();
    if (BEQ) return;
    LDA(RD(BZ_X2 + 1));
    CMP(0x7b);                  /* IF TOO FAR, THROW OUT */
    if (BCS) return;
    bz_wait(m);                 /* READ MATH BOX */
    ASLA();
    STA(BZ_Y2);
    LDA(RD(BZ_MOUTHI));
    ROLA();
    STA(BZ_Y2 + 1);
    LDA(RD(BZ_Y2));             /* ABSOLUTE VALUE OF Y */
    BIT(RD(BZ_Y2 + 1));         /* IF POS, BRANCH */
    if (BPL) goto l10;
    CLC();
    EOR(0xff);
    ADC(1);
l10:
    STA(BZ_Y1);
    LDA(RD(BZ_Y2 + 1));
    if (BPL) goto l15;
    EOR(0xff);
    ADC(0);
l15:
    STA(BZ_Y1 + 1);
    LDA(RD(BZ_Y1));             /* IF Y >= X */
    CMP(RD(BZ_X2));
    LDA(RD(BZ_Y1 + 1));
    SBC(RD(BZ_X2 + 1));
    if (BCS) return;
    m->vis_id[(Y >> 1) & 31] = m->cur_id;
    m->vis_lo[(Y >> 1) & 31] = (m->cur_id == BZ_ID_TANK || m->cur_id == BZ_ID_SUPERTANK || m->cur_id == BZ_ID_MISSILE)
                               ? m->mem[BZ_LANGLE + 2]
                               : m->cur_id == BZ_ID_SAUCER ? m->saucer_lo
                               : (m->cur_id >= BZ_ID_PIECE && m->cur_id < BZ_ID_PIECE + 6) ? m->piece_lo[m->cur_id - BZ_ID_PIECE]
                               : 0;
    LDA(RD(BZ_X2));             /* MOVE TABLE VALUE TO RAM */
    STA(BZ_PTBLX2 + Y);
    LDA(RD(BZ_X2 + 1));
    STA(BZ_PTBLX2 + 1 + Y);
    LDA(RD(BZ_Y2));
    STA(BZ_PTBLY2 + Y);
    LDA(RD(BZ_Y2 + 1));
    STA(BZ_PTBLY2 + 1 + Y);
    INY();
    INY();
}

/* EXPLDE: UPDATES EXPLOSION PIECES, POSITIONS AND ANGLES. ROTATE &
 * TRANSLATES THEIR CENTER POINTS. RESETS COLLISION FLAG AT TERMINATION OF
 * EXPLOSION SEQUENCE (all six pieces below the ground), and brings the
 * next enemy. */
static void explde(bz_machine *m)
{
    LDX(10);                    /* UPDATE X,Y,Z POSITIONS */
l1:
    LDA(RD(BZ_EXPOSZ + 1 + X));
    CLC();
    if (BMI) goto l30;
    LDA(RD(BZ_EXPOSX + X));
    ADC(RD(BZ_EXPTBX + X));
    STA(BZ_EXPOSX + X);
    STA(BZ_XMATRX);
    LDA(RD(BZ_EXPOSX + 1 + X));
    ADC(RD(BZ_EXPTBX + 1 + X));
    STA(BZ_EXPOSX + 1 + X);
    STA(BZ_XMATRX + 1);
    CLC();                      /* Y */
    LDA(RD(BZ_EXPOSY + X));
    ADC(RD(BZ_EXPTBY + X));
    STA(BZ_EXPOSY + X);
    STA(BZ_YMATRX);
    LDA(RD(BZ_EXPOSY + 1 + X));
    ADC(RD(BZ_EXPTBY + 1 + X));
    STA(BZ_EXPOSY + 1 + X);
    LDA(RD(BZ_ZVELOC + X));
    STA(BZ_TEMP1);
    if (BMI) goto l5;
    LDA(0);
l5:
    if (BEQ) goto l6;
    LDA(0xff);
l6:
    ASL(BZ_TEMP1);
    ROLA();
    ASL(BZ_TEMP1);
    ROLA();
    STA(BZ_TEMP1 + 1);
    LDA(RD(BZ_EXPOSZ + X));     /* Z */
    CLC();
    ADC(RD(BZ_TEMP1));
    STA(BZ_EXPOSZ + X);
    LDA(RD(BZ_TEMP1 + 1));
    ADC(RD(BZ_EXPOSZ + 1 + X));
    STA(BZ_EXPOSZ + 1 + X);
    LDA(RD(BZ_ZVELOC + X));     /* IF ZVELOC MAXED OUT */
    if (BPL) goto l8;
    CMP(0x85);                  /* 80-GRAVTY+1 */
    if (BCC) goto l10;
l8:
    ADC(GRAVTY);                /* UPDATE ZVELOC */
    STA(BZ_ZVELOC + X);
l10:
    TXA();                      /* UPDATE ORIENTATION */
    LSRA();
    LSRA();
    {
        FLAGS(p);
        TXA();
        ASLA();
        ASLA();
        ADC(3);
        RESTORE(p);
    }
    if (BCC) goto l20;
    STA(BZ_TEMP1);              /* DECREMENT */
    LDA(RD(BZ_EXPOSO + X));
    SBC(RD(BZ_TEMP1));
    goto l25;
l20:
    ADC(RD(BZ_EXPOSO + X));
l25:
    STA(BZ_EXPOSO + X);
    STA(BZ_PTBLO2 + 1 + Y);     /* SET UP FOR TABLE USE */
    TXA();
    LSRA();                     /* DEVELOP OBJECT # */
    CLC();
    ADC(0x10);
    BIT(RD(BZ_R2D3FL));         /* IF TANK */
    if (BPL) goto l26;          /* DIFFERENT */
    ADC(8);                     /* FOR R2D3 */
    goto l28;                   /* (BNE, always) */
l26:
    CMP(0x13);                  /* IF NOT RADAR */
    if (BNE) goto l28;
    STA(BZ_TEMP1);
    bz_tr7chk(m);
    if (BCC) goto l27;
    INC(BZ_TEMP1);
l27:
    LDA(RD(BZ_TEMP1));
l28:
    STA(BZ_PTBLO2 + Y);
    m->cur_id = (uint8_t)(BZ_ID_PIECE + (X >> 1));
    LDA(RD(BZ_EXPOSY + 1 + X)); /* START MATRIX MULTIPLY */
    STA(BZ_YSTRT1);
    rotpnt(m);                  /* ROTATE & SELECT */
l30:
    DEX();
    DEX();
    if (BPL) goto l1;
    LDX(10);                    /* IF EXPLOSION NOT OVER */
    do {
        LDA(RD(BZ_EXPOSZ + 1 + X));
        if (BPL) return;
        DEX();
        DEX();
    } while (BPL);
    LDA(0);                     /* RESET COLLISION FLAG */
    STA(BZ_COLFLG + 2);
    {
        uint8_t y = Y;          /* TYA PHA */
        bz_robchk(m);           /* RESET ROBOT POSITION */
        PLA(y);
        TAY();
    }
}

/* --- an object's lines --------------------------------------------------------------- */

/* DRAW: WILL BUILD VECTOR LIST FOR ANY OBJECT FROM VALUES IN TABLES
 * PNTTBL AND OBJTBL. A=OBJECT #, SAVE0=OBJECT # POINTER. An object's list
 * is a row of bytes: a command in the low three bits, a point's number
 * (times 8) or a brightness in the others; FF ends it. */
void bz_draw(bz_machine *m)
{
    ASLA();                     /* BUILD INDEX TO OBJ TBL */
    TAY();
    LDA(RD(BZ_OBJTBL + Y));     /* GET POINTER TO OBJ IN MEM */
    STA(BZ_TBLPTR);
    LDA(RD(BZ_OBJTBL + 1 + Y));
    STA(BZ_TBLPTR + 1);
    for (;;) {
        LDY(0);                 /* IF NOT DONE */
        LDA(RD(IY(BZ_TBLPTR))); /* (VALUE FROM TABLE) */
        CMP(0xff);
        if (BEQ) return;
        TAX();                  /* DO COMPUTED GO TO */
        AND(0xf8);              /* (SAVE INDEX OR VALUE) */
        STA(BZ_TEMP1);
        TXA();
        AND(7);
        ASLA();
        TAX();
        switch (X >> 1) {
        case 0:                 /* D.DOT */
            LDA(0);             /* DRAW BLANK VECTOR TO POINT */
            dovect(m);
            LDA(0x20);          /* DRAW DOT AT CURRENT INTENSITY */
            bz_vgdot(m);
            break;
        case 1:                 /* D.SBRT */
            LDX(RD(BZ_SAVE0));  /* IF NOT SAUCER */
            LDA(RD(BZ_PTBLO2 + X));
            CMP(0x17);          /* IF LOGO */
            if (BEQ) goto b5;
            CMP(0x1e);
            if (BEQ) goto b5;
            CMP(0x1f);
            if (BEQ) goto b5;
            CMP(0x20);          /* IF NOT SAUCER */
            if (BNE) goto b10;
            LDA(RD(BZ_SCOLFG)); /* IF NOT IN COLLISION */
            if (BEQ) goto b10;
        b5:
            LDA(RD(BZ_SINT));
            goto b15;
        b10:
            LDA(RD(BZ_TEMP1));  /* GET BRITNESS */
            SEC();
            SBC(RD(BZ_DQUE));
            if (BCC) goto b12;
            CMP(0x30);
            if (BCS) goto b15;
        b12:
            LDA(0x30);
        b15:
            TAY();              /* SET BRITNESS REGISTER */
            LDA(4);             /* NO CLIPPING WINDOW */
            bz_vgstat(m);
            break;
        case 3:                 /* D.LABS */
            LDA(0);             /* RESET CURRENT X&Y */
            STA(BZ_CURNTX);
            STA(BZ_CURNTX + 1);
            STA(BZ_CURNTY);
            STA(BZ_CURNTY + 1);
            bz_vgcntr(m);       /* MOVE BEAM */
            /* fall through */
        case 2:                 /* D.BVCT */
            LDA(0);             /* DRAW BLANK VECTOR */
            dovect(m);
            break;
        case 4:                 /* D.VCTR */
            LDA(0x20);          /* DRAW AT CURRENT BRITHNESS */
            dovect(m);
            break;
        case 5:                 /* D.SPCL */
            LDX(RD(BZ_SAVE0));  /* SPECIAL COUNTER */
            LDA(RD(BZ_PTBLO2 + 1 + X));
            TAX();              /* SET UP SCALING */
            AND(0x3f);
            ASLA();
            TAY();              /* LINEAR */
            TXA();
            AND(0xc0);
            ASLA();
            ROLA();
            ROLA();
            ADC(1);             /* POWER OF 2 */
            bz_vgscal(m);       /* SET SCALE FACTOR */
            LDX(BZ_EXPIC & 0xff);   /* OUTPUT EXPLOSION PICTURE */
            LDA(BZ_EXPIC >> 8);
            bz_vgjsrl(m);
            LDA(1);             /* RESET SCALE */
            bz_vgsca1(m);
            break;
        default:                /* D.SLIM */
            break;
        }
        INC(BZ_TBLPTR);         /* D.NEXT: GET NEXT COMMAND */
        if (BNE) continue;
        INC(BZ_TBLPTR + 1);
    }
}

/* DOVECT: a vector from where the beam is to the point TEMP1 names, at the
 * brightness in A. */
static void dovect(bz_machine *m)
{
    STA(BZ_VGBRIT);             /* SET BRITNESS */
    LDA(RD(BZ_TEMP1));          /* GET INDEX */
    LSRA();
    TAX();
    LDA(RD(BZ_PNTTBL + X));     /* COMPUTE DELTA */
    TAY();
    SEC();
    SBC(RD(BZ_CURNTX));
    STA(BZ_XCOMP);
    STY(BZ_CURNTX);             /* SAVE NEW CURRENT */
    LDA(RD(BZ_PNTTBL + 1 + X));
    TAY();
    SBC(RD(BZ_CURNTX + 1));
    STA(BZ_XCOMP + 1);
    STY(BZ_CURNTX + 1);
    SEC();                      /* COMPUTE DELTA Y */
    LDA(RD(BZ_PNTTBL + 2 + X));
    TAY();
    SBC(RD(BZ_CURNTY));
    STA(BZ_XCOMP + 2);
    STY(BZ_CURNTY);             /* SAVE NEW CURRENT */
    LDA(RD(BZ_PNTTBL + 3 + X));
    TAY();
    SBC(RD(BZ_CURNTY + 1));
    STA(BZ_XCOMP + 3);
    STY(BZ_CURNTY + 1);
    /* (for the renderer: the object, the point this vector goes to, how far ahead it is) */
    m->note = (uint32_t)m->mem[(BZ_PTBLO2 + m->mem[BZ_SAVE0]) & 0x3ff] << 24 |
              (uint32_t)(((X >> 2) & 63) + 1) << 16 | (uint16_t)m->pt_depth[(X >> 2) & 63];
    m->note_qx = m->pt_qx[(X >> 2) & 63];
    m->note_qy = m->pt_qy[(X >> 2) & 63];
    bz_vgvtr2(m);               /* ADD VCTR TO VGLIST */
    m->note = 0;
}

/* PNTPUT: THIS ROUTINE WILL GENERATE THE POINTS USED TO DRAW AN OBJECT
 * AND MOVE THEM TO THE POINT TABLE. X=POINTER TO CENTER POINT OF OBJECT
 * (in PTBLX2, PTBLY2, PTBLO2). Each point of the object (X, Y, Z in the
 * ROM) is turned by the object's angle, moved to its place and divided
 * by its distance: screen X and Y into PNTTBL. */
void bz_pntput(bz_machine *m)
{
    STX(BZ_SAVE1);              /* SAVE INDEX */
    LDA(RD(BZ_TANGLE));         /* COMPUTE RELATIVE ORIENTATION */
    SEC();
    SBC(RD(BZ_PTBLO2 + 1 + X));
    EOR(0x80);
    STA(BZ_NTHETA);
    bz_sin(m);
    STA(BZ_BSIN);
    STX(BZ_BSIN + 1);
    LDA(RD(BZ_NTHETA));
    bz_cos(m);
    CLC();
    EOR(0xff);
    ADC(1);
    STA(BZ_ACOS);
    TXA();
    EOR(0xff);
    ADC(0);
    STA(BZ_ACOS + 1);
    if (m->fine) fine_object(m);
    LDX(RD(BZ_SAVE1));
    LDA(RD(BZ_PTBLO2 + X));     /* SET UP TABLE POINTER */
    ASLA();
    TAY();
    LDA(RD(BZ_OBJPNT + Y));
    STA(BZ_TBLPTR);
    LDA(RD(BZ_OBJPNT + 1 + Y));
    STA(BZ_TBLPTR + 1);
    LDA(RD(BZ_PTBLX2 + X));
    STA(BZ_TPOSXT);
    LDA(RD(BZ_PTBLX2 + 1 + X));
    STA(BZ_TPOSXT + 1);
    AND(0xf0);
    STA(BZ_DQUE);
    LDA(RD(BZ_PTBLY2 + X));
    STA(BZ_TPOSYT);
    LDA(RD(BZ_PTBLY2 + 1 + X));
    STA(BZ_TPOSYT + 1);
    LDX(0);                     /* INITIALIZE INDICES */
    STX(BZ_ZLOW);
    STX(BZ_ZHI);
    STX(BZ_SAVE3);              /* OBJECT POINT TABLE */
    INC(BZ_SAVE3);
    for (;;) {
        LDY(RD(BZ_SAVE3));      /* SET UP DELTA X */
        LDA(RD(IY(BZ_TBLPTR)));
        STA(BZ_XMATRX);
        INY();
        LDA(RD(IY(BZ_TBLPTR)));
        STA(BZ_XMATRX + 1);
        INY();                  /* SET UP DELTA Y */
        LDA(RD(IY(BZ_TBLPTR)));
        STA(BZ_YMATRX);
        INY();
        LDA(RD(IY(BZ_TBLPTR)));
        STA(BZ_YSTRT2);
        INY();
        bz_wait(m);
        CLC();
        EOR(0xff);
        ADC(1);
        STA(BZ_PNTTBL + X);     /* MOVE X COORD TO TABLE */
        LDA(RD(BZ_MOUTHI));
        EOR(0xff);
        ADC(0);
        STA(BZ_PNTTBL + 1 + X);
        LDA(RD(IY(BZ_TBLPTR)));
        STA(BZ_DDEND);
        INY();
        LDA(RD(IY(BZ_TBLPTR)));
        INY();
        STY(BZ_SAVE3);
        STA(BZ_DDEND + 1);
        STX(BZ_SAVE2);
        kludge(m);
        bz_wait(m);
        LDX(RD(BZ_SAVE2));
        STA(BZ_PNTTBL + 2 + X);
        LDA(RD(BZ_MOUTHI));
        STA(BZ_PNTTBL + 3 + X);
        m->pt_depth[(X >> 2) & 63] = m->mb.reg[7];     /* (what the math box divided by: X') */
        if (m->mb.reg[7] > 0) {                         /* (and what it divided: Y' with its fraction, and Z) */
            float ahead = (float)m->mb.reg[7];
            m->pt_qx[(X >> 2) & 63] = ((float)m->mb.reg[8] + (float)(uint16_t)m->mb.reg[9] / 65536.0f) / ahead;
            m->pt_qy[(X >> 2) & 63] = (float)m->mb.reg[0x0b] / ahead;
        }
        TYA();
        LDY(0);
        CMP(RD(IY(BZ_TBLPTR)));
        if (BCS) return;
        INX();
        INX();
        INX();
        INX();
        if (BEQ) return;
    }
}

/* KLUDGE: WILL ADD APPROPRIATE Z OFFSET FOR EXPLOSION OBJECTS (the pieces
 * fly up and fall; the logo and the buzz bomb use the same heights), and
 * the bounce of the view; then starts the divide. */
static void kludge(bz_machine *m)
{
    LDX(RD(BZ_SAVE1));          /* IF NOT EXPLOSION OBJECT */
    LDA(RD(BZ_PTBLO2 + X));
    AND(0x10);
    if (BEQ) goto l20;
    LDA(RD(BZ_PTBLO2 + X));     /* CONVERT TO EXPLOSION PIECE # */
    AND(7);
    ASLA();
    TAX();
    LDA(RD(BZ_EXPOSZ + X));     /* ADD IN Z OFFSET */
    ADC(RD(BZ_DDEND));
    STA(BZ_DDEND);
    LDA(RD(BZ_EXPOSZ + 1 + X));
    ADC(RD(BZ_DDEND + 1));
    STA(BZ_DDEND + 1);
l20:
    LDA(RD(BZ_BOUNCE));
    if (BEQ) goto l30;
    SEC();
    LDA(RD(BZ_DDEND));
    SBC(RD(BZ_BOUNCE));
    STA(BZ_DIVOUT);
    LDA(RD(BZ_DDEND + 1));
    SBC(0);
    goto l40;
l30:
    LDA(RD(BZ_DDEND));
    STA(BZ_DIVOUT);
    LDA(RD(BZ_DDEND + 1));
l40:
    STA(BZ_DIVOUT + 1);
    STA(BZ_ZDIV);               /* START DIVIDE */
}

/* BOUND: THIS ROUTINE WILL CONTROL BOUNCE */
void bz_bound(bz_machine *m)
{
    LDA(RD(BZ_BOUNCE));         /* IF NO BOUNCE */
    if (BEQ) return;
    LSR(BZ_BOUNCE);             /* LOWER BOUNCE */
}
