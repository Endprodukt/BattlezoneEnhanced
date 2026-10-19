/* vgut.c - VGUT.MAC, the vector generator utilities (Ed Logg, 1979): they
 * add instructions to the display list at VGLIST. And DIGITS from
 * BZSTST.MAC. Instruction by instruction: A, X, Y and the carry come in
 * and go out as the original's do. */
#include "game/game.h"
#include "game/m6502.h"

/* VGRTSL - ADD RTSL TO VECTOR LIST */
void bz_vgrtsl(bz_machine *m)
{
    LDA(0xc0);                  /* DXXX IS RTSL */
    LDY(0);
    STA(IY(BZ_VGLIST));
    INY();                      /* VGWAI1: ADD LAST BYTE */
    STA(IY(BZ_VGLIST));
    bz_vgadd(m);
}

/* VGHALT - CENTER AND HALT VECTOR GENERATOR */
void bz_vghalt(bz_machine *m)
{
    bz_vgcntr(m);               /* CENTER FIRST */
    LDA(0x20);                  /* BXXX IS HALT */
    LDY(0);
    STA(IY(BZ_VGLIST));
    INY();
    STA(IY(BZ_VGLIST));
    bz_vgadd(m);
}

/* VGHEX1: A is the character's place in VGMSGA. */
static void vghex1(bz_machine *m)
{
    FLAGS(p);                   /* SAVE C FLAG */
    ASLA();
    LDY(0);
    TAX();
    LDA(RD(BZ_VGMSGA + X));
    STA(IY(BZ_VGLIST));
    LDA(RD(BZ_VGMSGA + 1 + X)); /* COPY JSRL TO CHARACTER ROUTINE */
    INY();
    STA(IY(BZ_VGLIST));
    bz_vgadd(m);                /* UPDATE VECTOR LIST POINTER */
    RESTORE(p);                 /* RESTORE C FLAG */
}

/* VGHEX - DISPLAY DIGIT: (A) = LOWER 4 BITS TO BE DISPLAYED; carry clear after. */
void bz_vghex(bz_machine *m)
{
    AND(0x0f);
    CLC();
    ADC(0x01);                  /* CLEARS C BIT */
    vghex1(m);
}

/* VGHEXZ - DISPLAY DIGIT WITH ZERO SUPPRESSION: carry set to suppress a
 * zero (a blank is put out instead and the carry stays set). */
void bz_vghexz(bz_machine *m)
{
    if (BCC) { bz_vghex(m); return; }   /* IF NO ZERO SUPPRESSION */
    AND(0x0f);
    if (BEQ) { vghex1(m); return; }     /* LEAVE C SET */
    bz_vghex(m);
}

/* VGJMP1: the opcode and address high bits in A, the address low byte in X,
 * its lowest bit in the carry. */
static void vgjmp1(bz_machine *m)
{
    LDY(0x01);
    STA(IY(BZ_VGLIST));         /* SAVE MSB + OPCODE */
    DEY();
    TXA();
    RORA();
    STA(IY(BZ_VGLIST));         /* LSB OF ADDRESS */
    INY();
    bz_vgadd(m);                /* UPDATE VECTOR POINTER */
}

/* VGJMPL - ADD JMPL TO VECTOR LIST: (A) = MSB OF ADDRESS, (X) = LSB */
void bz_vgjmpl(bz_machine *m)
{
    LSRA();
    AND(0x0f);                  /* BASE ADDRESS IS RELATIVE TO ZERO */
    ORA(0xe0);
    vgjmp1(m);
}

/* VGJSRL - ADD JSRL TO VECTOR LIST: (A) = MSB OF ADDRESS, (X) = LSB */
void bz_vgjsrl(bz_machine *m)
{
    LSRA();
    AND(0x0f);                  /* BASE ADDRESS IS RELATIVE */
    ORA(0xa0);
    vgjmp1(m);                  /* MOVE INTO VECTOR LIST */
}

/* VGSTAT - SET VECTOR GENERATOR STATUS: (A)=HI/LOW AND IN/OUT FLAGS AND
 * ENABLE (0,1,2,3,OR 4), (Y)=VECTOR BRIGHTNESS (0,10,20,...,F0) */
void bz_vgstat(bz_machine *m)
{
    ORA(0x60);
    TAX();                      /* OPCODE + LIMIT BITS */
    TYA();
    bz_vgadd2(m);               /* ADD 2 BYTES TO VECTOR LIST */
}

/* VGSTA1: the brightness from VGBRIT. */
void bz_vgsta1(bz_machine *m)
{
    LDY(RD(BZ_VGBRIT));
    bz_vgstat(m);
}

/* VGCNTR - CENTER BEAM IN MIDDLE OF SCREEN */
void bz_vgcntr(bz_machine *m)
{
    LDA(0x40);                  /* TIMER + SCALE */
    LDX(0x80);                  /* OPCODE */
    bz_vgadd2(m);
}

/* VGADD2 - ADD 2 WORDS TO VECTOR LIST: (A)=FIRST BYTE, (X)=SECOND BYTE */
void bz_vgadd2(bz_machine *m)
{
    LDY(0);
    STA(IY(BZ_VGLIST));         /* LSB BYTE */
    INY();
    TXA();
    STA(IY(BZ_VGLIST));         /* MSB BYTE */
    bz_vgadd(m);                /* UPDATE VECTOR LIST POINTER */
}

/* VGADD - ADD Y+1 TO VECTOR LIST ADDRESS */
void bz_vgadd(bz_machine *m)
{
    TYA();                      /* ADD 1+(Y) TO VGLIST */
    SEC();
    ADC(RD(BZ_VGLIST));
    STA(BZ_VGLIST);
    if (BCC) return;
    INC(BZ_VGLIST + 1);
}

/* VGSCAL - SET VECTOR GENERATOR SCALE: (Y)=LINEAR SCALE FACTOR (0=FULL
 * SIZE, FF=1/256 SIZE), (A)=POWER OF 2 SCALE FACTOR (0=FULL SIZE,1=1/2
 * SIZE,...) */
void bz_vgscal(bz_machine *m)
{
    ORA(0x70);                  /* OPCODE + SCALE SIZE */
    TAX();
    TYA();
    bz_vgadd2(m);               /* ADD 2 BYTES TO VECTOR LIST */
}

/* VGSCA1: USE FULL SIZE */
void bz_vgsca1(bz_machine *m)
{
    LDY(0);
    bz_vgscal(m);
}

/* VGDOT - DRAW A DOT AT CURRENT POSITION: (A) = INTENSITY */
void bz_vgdot(bz_machine *m)
{
    TAY();
    LDA(0);
    TAX();
    bz_vgvtr(m);
}

/* VGVTR - SHORT FORM VGVCTR CALL: (A) = CHANGE IN X/4 (-80 TO +7F),
 * (X) = CHANGE IN Y/4, (Y) = VECTOR BRIGHTNESS (0,10,20,...,F0) */
void bz_vgvtr(bz_machine *m)
{
    STY(BZ_VGBRIT);
    bz_vgvtr1(m);
}

/* VGVTR1: the same with the brightness already in VGBRIT. */
void bz_vgvtr1(bz_machine *m)
{
    LDY(0);
    ASLA();
    if (BCS) DEY();             /* SIGN EXTEND: Y=-1 */
    STY(BZ_XCOMP + 1);          /* WITH SCALE=0 A=1 MEANS=DOTS ON XY */
    ASLA();
    ROL(BZ_XCOMP + 1);
    STA(BZ_XCOMP);
    TXA();                      /* WRITE Y VALUE TO XCOMP+2,XCOMP+3 */
    ASLA();
    LDY(0);
    if (BCS) DEY();             /* SIGN EXTEND: Y=-1 */
    STY(BZ_XCOMP + 3);
    ASLA();
    ROL(BZ_XCOMP + 3);
    STA(BZ_XCOMP + 2);
    bz_vgvtr2(m);
}

/* VGVTR2: the vector in XCOMP (X LSB, X MSB, Y LSB, Y MSB). */
void bz_vgvtr2(bz_machine *m)
{
    LDX(BZ_XCOMP);
    bz_vgvctr(m);               /* GENERATE VECTOR */
}

/* VGVCTR - ADD VECTOR TO VECTOR LIST: (X) = 4 ZERO PAGE LOCN. CONTAINING
 * (X LSB,X MSB,Y LSB,Y MSB), (VGBRIT)=VECTOR BRIGHTNESS (ONLY THE UPPER 3
 * BITS ARE USED) */
void bz_vgvctr(bz_machine *m)
{
    LDY(0);
    LDA(RD((uint8_t)(X + 2)));  /* Y LSB */
    STA(IY(BZ_VGLIST));
    LDA(RD((uint8_t)(X + 3)));  /* Y MSB */
    AND(0x1f);                  /* CLEAR SIGN EXTENSION */
    INY();
    STA(IY(BZ_VGLIST));
    LDA(RD(X));                 /* X LSB */
    INY();
    STA(IY(BZ_VGLIST));
    LDA(RD((uint8_t)(X + 1)));  /* X MSB */
    EOR(RD(BZ_VGBRIT));
    AND(0x1f);                  /* CLEAR SIGN EXTENSION */
    EOR(RD(BZ_VGBRIT));         /* COMBINE UPPER 3 BITS OF VGBRIT WITH X MSB */
    INY();                      /* VGWAI1 */
    STA(IY(BZ_VGLIST));         /* SET INTENSITY */
    bz_vgadd(m);                /* ALWAYS - UPDATE VGLIST POINTER */
}

/* DIGITS - DISPLAY 2Y DIGIT NUMBERS (BZSTST.MAC)
 * ENTRY (C)=CARRY SET FOR ZERO SUPPRESSION
 *       (A)=ADDRESS OF (Y) ZERO PAGE LOCATIONS CONTAINING NUMBER (LSB TO MSB)
 *       (Y)=NUMBER OF ZERO PAGE LOCATIONS TO USE */
void bz_digits(bz_machine *m)
{
    {
        FLAGS(p);               /* SAVE INPUT PARAMETERS */
        DEY();
        STY(BZ_TEMP1 + 1);
        CLC();
        ADC(RD(BZ_TEMP1 + 1));
        RESTORE(p);
    }
    TAX();                      /* MSB OF DIGITS */
    do {
        {
            FLAGS(p);
            STX(BZ_TEMP1);      /* SAVE DIGIT ZP OFFSET */
            LDA(RD(X));
            LSRA();
            LSRA();
            LSRA();
            LSRA();
            RESTORE(p);
        }
        bz_vghexz(m);           /* FIRST DIGIT */
        LDA(RD(BZ_TEMP1 + 1));
        if (BEQ) CLC();         /* DISPLAY LAST DIGIT (EVEN 0) */
        LDX(RD(BZ_TEMP1));
        LDA(RD(X));
        bz_vghexz(m);           /* SECOND DIGIT */
        LDX(RD(BZ_TEMP1));
        DEX();
        DEC(BZ_TEMP1 + 1);
    } while (BPL);              /* LOOP FOR EACH SET OF DIGITS */
}

void bz_digtys(bz_machine *m)
{
    SEC();
    bz_digits(m);
}

void bz_digt2s(bz_machine *m)
{
    LDY(2);
    bz_digtys(m);
}
