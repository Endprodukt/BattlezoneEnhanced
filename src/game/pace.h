/* pace.h - how long the 6502 took over a frame (the option `enh_tempo`).
 *
 * The C routines are done with a frame at once. The cabinet's 6502 needs
 * 56,000 cycles for one on average and up to 126,000, and its interrupt
 * takes 690 of every 6144 away from it: a frame that needs more than the
 * 16 interrupts between two SYNCs holds the next one up, and the game
 * slows down in a full scene. MAME shows 14.7 to 15.0 frames a second
 * where the C routines alone always make 15.3.
 *
 * With the option on, a frame's work is done when the 6502 would be done
 * with it: only then does the interrupt see the display buffer finished
 * and hands it to the vector generator at its next sixth turn, and only
 * then can the next frame begin. (How long a frame will take is known
 * only when it is done, so the wait is the frame before's time, and what
 * that was wrong by is made up in the next.) The time is
 * modelled: what a line-by-line port has in common with the original is
 * the memory it reads and writes, so those are counted (bz_machine.n_rd,
 * n_wr, n_mbox) and each has a cost. The costs were fitted to the
 * original program run on the 6502 core (tools/bzcompare --csv, 70,120
 * frames of the 16 captures): within 790 cycles (rms) of the 56,000.
 */
#ifndef BZ_PACE_H
#define BZ_PACE_H

#include "core/machine.h"

/* The cycles of the 6502 left for the main line in an interrupt's period:
 * 6144 less the interrupt's 683 (measured with tools/bznmi) and its entry. */
#define BZ_PACE_CYCLES_PER_NMI 5454

/* The 6502's cycles for a frame in which the C routines read, wrote and
 * started the math box this often. */
uint32_t bz_pace_cycles(uint32_t n_rd, uint32_t n_wr, uint32_t n_mbox);

typedef struct bz_pace {
    int     working;            /* the 6502 would be at a frame's work now */
    int32_t busy;               /* cycles of it left */
    int32_t last;               /* what the frame before took */
    int32_t owed;               /* what the wait for it was short by */
} bz_pace;

/* In place of bz_main_poll, once an interrupt (zero the struct first).
 * Returns 1 if a frame ran. */
int bz_pace_poll(bz_machine *m, bz_pace *p);

#endif
