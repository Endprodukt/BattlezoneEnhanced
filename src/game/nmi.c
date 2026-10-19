/* nmi.c - the interrupt (BZONE.MAC, NMI), 246 times a second: counts
 * interrupts and lets the main line go every 16th, handles coins, carries
 * the sounds on, takes the start button, and every 6th time restarts the
 * vector generator, on the buffer the main line has finished if it has.
 * And PWRON (BZSTST.MAC): the state the game starts from. */
#include "game/game.h"
#include "game/m6502.h"

#include <string.h>

#define START  0x20     /* the start button in ALLPOT */

static void nmi(bz_machine *m);

void bz_nmi(bz_machine *m)
{
    bz_regs saved = m->r;       /* PHA, TXA PHA, TYA PHA; the flags come back with RTI */
    m->in_nmi = 1;
    m->nmis++;
    nmi(m);
    m->in_nmi = 0;
    m->r = saved;
}

static void nmi(bz_machine *m)
{
    uint8_t t;

    /* (TSX, BMI: IF STACK BLITZED, hang for the watchdog. No stack here.) */
    INC(BZ_INTCT);              /* UPDATE INTERRUPT COUNT */
    LDA(RD(BZ_INTCT));          /* IF NOT END OF FRAME (64 MS) */
    AND(0x0f);
    if (BEQ) {
        INC(BZ_SYNC);           /* MAINTAIN SYNCHRONIZATION */
        /* (BMI: IF LOST, hang for the watchdog: the main line is 128 frames behind) */
    }
    CLD();                      /* INSURE WE DON'T SCREW UP COINS IN BINARY */
    STA(BZ_WTCHDG);
    LDA(RD(BZ_OPTON2));         /* SET COIN MODE */
    STA(BZ_CMODE);
    bz_moolah(m);               /* LOOK FOR MONEY */
    LDA(RD(BZ_CCTIM));
    ROLA();
    LDA(RD(BZ_CCTIM + 1));
    ROLA();
    ROLA();
    t = A;
    LDA(RD(BZ_CCTIM + 2));
    ROLA();
    PLA(t);
    ROLA();
    STA(BZ_EMCNT);
    bz_modsnd(m);
    BIT(RD(BZ_SUPER));          /* IF SUPER BONUS, DON'T START ANY SOUNDS! */
    if (BMI) goto l15;
    LDA(RD(BZ_POINT));
    if (BNE) goto l15;
    LDA(RD(BZ_SCOLFG));         /* PRIORITIZE SOUNDS */
    if (BEQ) goto l3;
    LDA(BZ_S_DISINT);
    goto l11;
l3:
    LDA(RD(BZ_OBJCOL));
    if (BEQ) goto l6;
    LDA(BZ_S_BLKSND);
    goto l11;
l6:
    BIT(RD(BZ_SAUCER));
    if (BPL) goto l15;
    LDA(BZ_S_SAUSND);
l11:
    bz_sndon(m);
l15:
    LDA(RD(BZ_EXPCNT));         /* IF NOT EXPLOSION */
    if (BEQ) goto l17;
    DEC(BZ_EXPCNT);             /* IF EXPLOSION */
    if (BNE) goto l16;
    LDA(0);                     /* SHUT OFF EXPLOSION SOUND */
    goto l17;
l16:
    LDA(1);                     /* TURN ON EXPLOSION SOUND */
l17:
    TAY();
    LDA(RD(BZ_SXPCNT));         /* IF NOT SHELL FIRED */
    if (BEQ) goto l18;
    DEC(BZ_SXPCNT);
    if (BEQ) goto l18;
    TYA();
    ORA(4);                     /* TURN ON SHELL */
    TAY();
l18:
    TYA();
    ORA(RD(BZ_SNDWRD));
    BIT(RD(BZ_ATRACT));         /* IF ATTRACT MODE */
    if (BPL) goto l19;
    ORA(0xa0);                  /* GAME - ENABLE SOUNDS AND RUMBLE */
    goto l56;
l19:
    BIT(RD(BZ_GOVER));          /* IF ACCEPTING INITIALS */
    if (BMI) goto l54;
    LDA(0);                     /* START WITH NOTHING */
    LDX(RD(BZ_LMTIM));          /* IF NOT SLAM */
    if (BEQ) goto l65;
    ORA(0x20);                  /* SOUND THE ALARM */
    LDX(RD(BZ_POINT));          /* IF SOUND GOING */
    if (BNE) goto l65;
    t = A;
    LDA(BZ_S_DISINT);
    bz_sndon(m);
    PLA(t);
l65:
    t = A;                      /* PHA */
    LDA(RD(BZ_CMODE));          /* IF FREE PLAY */
    AND(3);
    if (BEQ) goto l68;
    LDA(RD(BZ_CRDT));           /* IF NO MONEY */
    if (BEQ) goto l55;
l68:
    LDA(RD(BZ_FRAME));          /* FLASH BUTTON */
    LSRA();
    LSRA();
    PLA(t);
    if (BCC) goto l70;
    ORA(0x40);
l70:
    STA(BZ_POTGO);
    STA(BZ_SOUND);              /* TURN OFF SOUNDS */
    LDA(RD(BZ_ALLPOT));         /* IF NOT START */
    AND(START);
    if (BEQ) goto l40;
    LDA(0xff);
    STA(BZ_NOR2D3);
    STA(BZ_ATRACT);
    LDA(RD(BZ_CRDT));
    SEC();
    SBC(RD(BZ_TRAP1));
    STA(BZ_CRDT);
    LDA(0);
    STA(BZ_SKCTL);
    STA(BZ_BCCNT);              /* RESET BONUS ADDER */
    STA(BZ_LOGO);
    STA(BZ_SOUND);              /* TURN OFF LICONS */
    STA(BZ_HITS);
    STA(BZ_HITS + 1);
    STA(BZ_HITS + 3);
    STA(BZ_HITS + 2);
    STA(BZ_ACTION);
    STA(BZ_FIRECT + 2);
    STA(BZ_GOVER);
    STA(BZ_TIMOUT);
    STA(BZ_SCOLFG);
    STA(BZ_SAUCER);
    STA(BZ_TANGLE);
    STA(BZ_TPOSX + 1);
    STA(BZ_TPOSY + 1);
    LDA(7);                     /* RE-INIT POKEY */
    STA(BZ_SKCTL);
    bz_robchk(m);
    CLC();
    LDA(RD(BZ_OPTION));
    AND(3);
    ADC(2);
    STA(BZ_LIVES);
    goto l40;                   /* (BPL, always) */
l54:
    ORA(0x60);                  /* TURN OFF RUMBLE ONLY */
    goto l56;
l55:
    PLA(t);                     /* RETRIEVE SOUND VALUE */
    ORA(0x40);                  /* TURN OFF LICONS */
l56:
    STA(BZ_SOUND);
l40:
    DEC(BZ_VTIMER);
    if (BNE) return;
    STA(BZ_VSTOP);
    LDA(RD(BZ_BUFRDY));         /* IF BUFFER NOT COMPLETE */
    if (BNE) goto l30;
    LDA(RD(BZ_VECRAM + 1));     /* SWITCH BUFFERS */
    EOR(4);
    STA(BZ_VECRAM + 1);
    AND(4);
    if (BNE) goto l25;          /* IF LOWER BUFFER AVAILABLE */
    LDA(BZ_VECRAM / 0x100 + 8); /* USE UPPER BUFFER */
    goto l26;
l25:
    LDA(BZ_VECRAM / 0x100);     /* USE LOWER BUFFER */
l26:
    STA(BZ_BUFRDY);
l30:
    STA(BZ_GOADD);              /* START UP VECTOR GENERATOR FOR 24 MS. */
    LDA(6);
    STA(BZ_VTIMER);
}

/* PWRON, as far as it leads into the game (the self test switch off):
 * clears the memory and sets up the two display buffers, the game over
 * state, the high score table and the logo. */
void bz_pwron(bz_machine *m)
{
    LDA(0);
    STA(BZ_SKCTL);
    CLD();
    memset(m->mem, 0, 0x400);           /* CLEAR RAM PAGES */
    LDY(7);                             /* RESET POKEY */
    STY(BZ_SKCTL);
    memset(m->mem + BZ_VECRAM, 0, 0x1000);
    STA(BZ_WTCHDG);
    STA(BZ_VSTOP);                      /* RESET VECTOR GEN */
    LDA(1);                             /* INIT VG BUFFER */
    STA(BZ_VECRAM);
    LDA(0xe4);
    STA(BZ_VECRAM + 1);
    LDA(0x20);
    STA(BZ_VECRAM + 2);
    STA(BZ_VECRAM + 3);
    STA(BZ_VECRAM + 0x802);
    STA(BZ_VECRAM + 0x803);
    LDA(1);                             /* INIT GAME STATE */
    STA(BZ_ACTION);
    STA(BZ_GOVER);
    LDA(0xfe);                          /* SET UP BUZZ BOMB SOUND */
    STA(BZ_CHAN3F);
    LDX(1);
    STA(BZ_CHAN4F);
    LDX(BZ_HSCNUM * 3 - 1);
    LDY(0);
    do {
        LDA(RD(BZ_FDGTBL - 2 + X));     /* INIT HIGH SCORE TABLE */
        STA(BZ_INITLS - 2 + X);
        LDA(5);
        STA(BZ_HSCTBL - 2 + X);
        LDA(RD(BZ_FDGTBL - 1 + X));
        STA(BZ_INITLS - 1 + X);
        LDA(RD(BZ_FDGTBL + X));
        STA(BZ_INITLS + X);
        INY();
        DEX();
        DEX();
        DEX();
    } while (BPL);
    bz_batint(m);                       /* INIT FOR LOGO */
    m->main_state = 0;
}
