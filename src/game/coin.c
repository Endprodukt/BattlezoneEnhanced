/* coin.c - COIN65.MAC, Atari's "universal" coin routine MOOLAH (Downend &
 * Albaugh), as Battlezone assembles it: three mechs with their switches in
 * D0 (right), D1 (middle), D2 (left) of 0800, low true; the slam switch in
 * D3, low true; mech multipliers; four coin modes; three electro-mechanical
 * counters; the bonus adder. Called four times a frame, from the interrupt.
 *
 * A coin is 16 to 800 ms of switch closure, with 33 ms open before and
 * after; credit is given half a second later (120 calls) unless the slam
 * switch is seen meanwhile. */
#include "game/game.h"
#include "game/m6502.h"

/* THE NUMBER OF UNIT-COINS REQUIRED TO RECEIVE BONUS (7F: no bonus coins) */
static const uint8_t MODLO[8] = { 0x7f, 2, 4, 4, 5, 0x7f, 0x7f, 0x7f };

void bz_moolah(bz_machine *m)
{
    uint8_t t;

    LDX(2);                             /* X IS USED TO INDEX FROM RIGHT TO LEFT COIN MECH */
detct:
    LDA(RD(BZ_HALT));                   /* GET COIN SWITCHES */
    CPX(1);                             /* WHICH MECH ARE WE DOING */
    if (BEQ) goto l11;                  /* MIDDLE: SHIFT TWICE */
    if (BCS) goto l12;                  /* RIGHT: SHIFT ONCE */
    LSRA();                             /* ELSE LEFT, SHIFT THRICE */
l11:
    LSRA();
l12:
    LSRA();
    LDA(RD(BZ_CNSTT + X));
    AND(31);                            /* ISOLATE COIN-ON DOWN-COUNTER, RESET COIN-OFF UP-CTR. */
    if (BCS) goto l5;                   /* BRANCH IF INPUT HIGH (COIN ABSENT) */
    if (BEQ) goto l1;                   /* STICK AT 0 (TERMINAL COUNT) */
    CMP(27);                            /* IN FIRST FIVE SAMPLES? */
    if (BCS) goto l10;                  /* YES, RUN FAST */
    TAY();                              /* ELSE SAVE STATUS */
    LDA(RD(BZ_INTCT));                  /* CHECK INTERUPT CTR */
    AND(7);                             /* ARE D0-D2 ALL ONES? */
    CMP(7);                             /* SET CARRY IF SO */
    TYA();                              /* STATUS BACK INTO ACC */
    if (BCC) goto l1;                   /* SKIP IF NOT ALL ONES */
l10:
    SBC(1);                             /* CARRY SET */
l1:
    STA(BZ_CNSTT + X);                  /* SAVE UPDATED STATUS */
    LDA(RD(BZ_HALT));                   /* CHECK SLAM SWITCH */
    AND(0x08);
    if (BNE) goto l2;                   /* BRANCH IF BIT HI (SWITCH OFF) */
    LDA(0xf0);                          /* ELSE SET PRE-COIN SLAM TIMER */
    STA(BZ_LMTIM);                      /* DECR. 8 TIMES/FRAME=PRST FRAMES */
l2:
    LDA(RD(BZ_LMTIM));                  /* CHECK PRE-COIN SLAM TIMER */
    if (BEQ) goto l3;                   /* O.K. */
    DEC(BZ_LMTIM);                      /* ELSE RUN TIMER */
    LDA(0);
    STA(BZ_CNSTT + X);                  /* CLEAR COIN STATUS */
    STA(BZ_PSTSL + X);                  /* CLEAR POST-COIN SLAM-TIMER */
l3:
    CLC();                              /* DEFAULT "NO COIN DETECTED" */
    LDA(RD(BZ_PSTSL + X));              /* CHECK POST-COIN SLAM-TIMER */
    if (BEQ) goto l8;                   /* EMPTY, PROCEED */
    DEC(BZ_PSTSL + X);                  /* RUN TIMER */
    if (BNE) goto l8;                   /* NOT DONE, PROCEED */
    SEC();                              /* WHEN IT BECOMES ZERO, INDICATE A COIN */
    goto l8;

l5:
    CMP(27);                            /* IS COIN VALID YET (ON FOR >4 SAMPLES) */
    if (BCS) goto l6;                   /* NO, RESET IT */
    LDA(RD(BZ_CNSTT + X));              /* GET STATUS AGAIN */
    ADC(32);                            /* BUMP COIN-OFF UP-CTR. */
    if (BCC) goto l1;                   /* IF IT DIDN'T WRAP, JUST STORE STATUS */
    if (BEQ) goto l6;                   /* IT WRAPPED BUT COIN WAS ON TOO LONG, JUST RESET */
    CLC();                              /* SET "VALIDITY" AGAIN */
l6:
    LDA(31);                            /* RESET DOWN-COUNTER */
    if (BCS) goto l1;                   /* BRANCH IF COIN TOO LONG OR TOO SHORT */
    STA(BZ_CNSTT + X);                  /* SAVE RESET STATUS */
    LDA(RD(BZ_PSTSL + X));              /* CHECK HOWIES ASSUMPTION */
    if (BEQ) goto l7;                   /* BRANCH IF $PSTSL VACANT */
    SEC();                              /* ELSE GIVE CREDIT A LITTLE EARLY */
l7:
    LDA(0x78);                          /* POST*4: DELAY ACCEPTANCE FOR POST/60 SEC. */
    STA(BZ_PSTSL + X);

l8:                                     /* CARRY=1 IF COIN FALLS OUT */
    if (BCC) goto l9;
    /* MECH-MULTIPLIERS */
    LDA(0);                             /* START WITH 0 (TO ADD 1) */
    CPX(1);                             /* CHECK WHICH MECH */
    if (BCC) goto l85;                  /* IF LEFT, ALWAYS ADD 1 */
    if (BEQ) goto l83;                  /* IF CENTER, CHECK HALF-MUL */
    LDA(RD(BZ_CMODE));                  /* GET DOLLAR MUL */
    AND(0x0c);
    LSRA();
    LSRA();
    if (BEQ) goto l85;                  /* 00-ADD 1 */
    ADC(2);                             /* ELSE MAP 1,2,3 TO 3,4,5 */
    if (BNE) goto l85;                  /* (ALWAYS) */
l83:
    LDA(RD(BZ_CMODE));
    AND(0x10);
    if (BEQ) goto l85;
    LDA(1);
l85:
    SEC();
    t = A;                              /* PHA */
    ADC(RD(BZ_BCCNT));                  /* UPDATE BONUS-ADDER COUNTER */
    STA(BZ_BCCNT);
    PLA(t);
    SEC();
    ADC(RD(BZ_CNCT));
    STA(BZ_CNCT);
    INC(BZ_CCTIM + X);                  /* "QUEUE" PULSE FOR E.M. COUNTER */
l9:
    DEX();
    if (BPL) goto detct;

    /* BONUS-ADDER */
    LDA(RD(BZ_CMODE));                  /* GET BONUS ADDER MODE */
    LSRA();
    LSRA();
    LSRA();
    LSRA();
    LSRA();                             /* ISOLATE BONUS-ADDER MODE IN BITS 0-2 */
    TAY();
    LDA(RD(BZ_BCCNT));
    SEC();
    SBC(MODLO[Y]);                      /* SEE IF ENOUGH UNIT-COINS HAVE ACCUMULATED */
    if (BMI) goto extb;                 /* BRANCH IF NOT */
    STA(BZ_BCCNT);                      /* ELSE UPDATE BONUS-ADDER AND... */
    INC(BZ_CNCT);                       /* GIVE ONE OR TWO BONUS UNIT-COINS */
    CPY(3);
    if (BNE) goto extb;
    INC(BZ_CNCT);                       /* MODE 3 YIELDS 2 BONUS COINS FOR 4 INSERTED */
extb:
    /* COINS TO CREDITS */
    LDA(RD(BZ_CMODE));                  /* GET COIN MODE IN ACC 0,1 */
    AND(3);
    TAY();                              /* SAVE IT & TEST IT */
    if (BEQ) goto c2;                   /* IF FREE PLAY, KEEP COIN COUNT CLEAR, LEAVE CREDIT */
    LSRA();
    ADC(0);                             /* FORM PRICE (0,1,1,2) */
    EOR(0xff);
    SEC();                              /* NEGATE */
    ADC(RD(BZ_CNCT));                   /* ACC <- COINCT - PRICE */
    if (BCC) goto ext;                  /* BRANCH ON BORROW */
    CPY(2);                             /* COIN MODE 2 OR 3? */
    if (BCS) goto c1;                   /* YES, JUST ONE CREDIT */
    INC(BZ_CRDT);                       /* ELSE GIVE TWO FOR MODES 0,1 */
c1:
    INC(BZ_CRDT);
c2:
    STA(BZ_CNCT);                       /* STORE REDUCED (OR CLEARED) COINCT */
ext:
    /* ELECTRO-MECH. CTRS */
    LDA(RD(BZ_INTCT));                  /* GET $INTCT */
    LSRA();                             /* USE LSB FOR PULSE=4 FRAMES */
    if (BCS) return;
    LDY(0);                             /* START WITH FLAG OF 0 */
    LDX(2);
    do {
        LDA(RD(BZ_CCTIM + X));          /* CHECK TIMER(X) */
        if (BEQ) goto e3;               /* NEITHER RUNNING NOR PENDING */
        CMP(0x10);                      /* IS IT RUNNING */
        if (BCC) goto e3;               /* NO, SKIP */
        ADC(0xef);                      /* ELSE DEC 4 MSB */
        INY();                          /* SET ON FLAG */
        STA(BZ_CCTIM + X);
e3:
        DEX();
    } while (BPL);
    TYA();                              /* CHECK "ON" FLAG */
    if (BNE) return;                    /* SKIP IF ANY ON */
    /* IF NONE OF THE COUNTERS ARE CURRENTLY ON, WE CHECK TO SEE IF ANY CAN BE STARTED. */
    LDX(2);
    do {
        LDA(RD(BZ_CCTIM + X));          /* NEED WE START THIS ONE */
        if (BEQ) goto e5;               /* NO, NO COUNTS PENDING */
        CLC();
        ADC(0xef);                      /* SET 4 MSB, DEC 4 LSB */
        STA(BZ_CCTIM + X);              /* START TIMER */
        if (BMI) return;                /* EXIT, SO WE DON'T START MORE */
e5:
        DEX();
    } while (BPL);
}
