/* sound.c - BZSOUN.MAC, the POKEY sound system (Rich Adam): SNDON starts a
 * sound, MODSND carries the sounds on, once every interrupt.
 *
 * A sound is up to four sequences, one per "channel": here the POKEY's
 * registers AUDF1, AUDC1, AUDF2, AUDC2 in this order. A sequence is a row
 * of four bytes: the value to start with, the frames before any change,
 * the amount of change, the number of changes; a row with 0 frames ends
 * it. The tables (PNTRS, SOUND) are read from the ROM. */
#include "game/game.h"
#include "game/m6502.h"

#define PTLEN 0x20

/* SNDON: (A) = the sound, one bit (BZ_SND_xxx). Leaves A, X and Y as they
 * were. */
void bz_sndon(bz_machine *m)
{
    uint8_t a = A, x = X, y = Y;        /* PHA, TXA PHA, TYA PHA */
    if (m->sound_request) m->sound_request(m->sound_user, a);
    LDA(a);                             /* TSX, LDA AX,103 */
    if (BNE) {
        LDX(8);                         /* START SOUND # SEARCH AT TOP */
        do {
            ASLA();                     /* WHICH BIT IS IT? */
            DEX();
        } while (BCC);                  /* FOUND HIM */
    }
    TXA();
    ASLA();                             /* X2 + CLC */
    ASLA();                             /* X4 + CLC */
    ADC(3);                             /* CAUSE IT COUNTS DOWN NOT UP */
    TAY();                              /* USE AS INDEX */
    CPY(PTLEN + 7);                     /* PAST TABLE? */
    if (BCC) {
        LDX(3);
        do {
            LDA(RD(BZ_PNTRS + Y));
            if (BNE) {                  /* IF POINTER 0 DONT TOUCH THIS CHANNEL */
                STA(BZ_POINT + X);      /* IF NOT SET UP POINTER */
                LDA(1);
                STA(BZ_FRAMES + X);     /* DUMMY START, NO SOUND */
                STA(BZ_COUNT + X);      /* TILL MODSND STORES TO POKEY */
            }
            DEY();
            DEX();
        } while (BPL);
    }
    Y = y;                              /* PLA TAY */
    X = x;                              /* PLA TAX */
    PLA(a);
}

/* MODSND: CONTINUES A PREVIOUSLY STARTED SOUND. WHEN CHANNEL 1 GOES IDLE,
 * ALL SOUND ENDS. */
void bz_modsnd(bz_machine *m)
{
    LDX(3);
l10:
    LDA(RD(BZ_FRAMES + X));
    if (BEQ) goto l15;
    DEC(BZ_FRAMES + X);                 /* CHANGE THIS FRAME */
    if (BNE) goto l30;
    LDY(RD(BZ_POINT + X));              /* GET DATA POINTER */
    if (BEQ) goto l30;                  /* NOT ACTIVE CHANNEL */
    DEC(BZ_COUNT + X);                  /* DATA CHANGES LEFT? */
    if (BNE) goto l20;                  /* NO -- NONE */
    LDA(RD(BZ_SNDTAB + Y));             /* GET CURRENT VALUE */
    STA(BZ_CURRENT + X);                /* SAVE */
    LDA(RD(BZ_SNDTAB + 1 + Y));         /* GET FRAMES OF THIS LEVEL */
    STA(BZ_FRAMES + X);                 /* UPDATE FRAME NUMBER */
    if (BNE) goto l17;                  /* IF 0, THIS CHANNEL JUST ENDED */
    STA(BZ_POINT + X);                  /* SET POINTER TO 0 TO IDLE CHANNEL */
    TXA();                              /* A CHEAP WAY TO TEST IF THIS IS CH 1 */
    if (BNE) goto l30;                  /* IF NOT CH 1, JUST IDLE CHANNEL */
l15:
    LDA(RD(BZ_IDLEV));                  /* GET IDLE VALUE */
    STA(BZ_CURRENT + X);                /* SAVE */
    STA(BZ_POKEY + X);                  /* STORE TO POKEY */
    LDA(0);                             /* 0 POINTER */
    STA(BZ_POINT + X);
    DEX();
    if (BPL) goto l10;
    return;                             /* SOUNDS OFF IN SOUND STATE */
l17:
    LDA(RD(BZ_SNDTAB + 3 + Y));
    STA(BZ_COUNT + X);                  /* # OF CHANGES */
    LDA(RD(BZ_POINT + X));
    CLC();
    ADC(4);                             /* POINT TO NEXT SEQUENCE */
    STA(BZ_POINT + X);
    goto l30;
l20:
    LDA(RD(BZ_SNDTAB + 1 - 4 + Y));     /* FRAME RATE (the pointer is already at the next block) */
    STA(BZ_FRAMES + X);
    LDA(RD(BZ_CURRENT + X));
    CLC();
    ADC(RD(BZ_SNDTAB + 2 - 4 + Y));     /* STEP THRU SEQUENCE */
    STA(BZ_CURRENT + X);
l30:
    LDA(RD(BZ_CURRENT + X));            /* MORE THAN 1 PATH HERE SO LOAD CURRENT */
    STA(BZ_POKEY + X);                  /* OUTPUT */
    LDA(RD(BZ_AUDCV));                  /* GET CONTROL BYTE */
    STA(BZ_ALLPOT);                     /* AUDCTL */
    DEX();
    if (BPL) goto l10;
}
