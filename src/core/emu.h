/* emu.h - the original program on the machine.
 *
 * The 6502 (cpu/m6502.h) runs the game's own code from the ROMs against
 * bz_machine: its RAM, the math box, the switches. Everything the game
 * does is then the original's, at the original's pace (the 6502's cycles
 * are counted; the interrupt comes every 6144 of them, 246 times a
 * second).
 *
 * This is the reference: the C routines of src/game compute the same,
 * frame for frame, and tools/bzcompare checks both against MAME.
 */
#ifndef BZ_EMU_H
#define BZ_EMU_H

#include "core/machine.h"
#include "cpu/m6502.h"

typedef struct bz_emu {
    bz_machine *m;
    m6502      cpu;
    int32_t    budget;      /* CPU cycles to run (negative: ran over into the next slice) */
    uint64_t   nmis;
} bz_emu;

#define BZ_CPU_HZ             1512000.0     /* 12.096 MHz / 8 */
#define BZ_CPU_CYCLES_PER_NMI 6144          /* 246.09 interrupts a second */
#define BZ_NMI_HZ             (BZ_CPU_HZ / BZ_CPU_CYCLES_PER_NMI)

/* The machine must be initialized and have its ROMs. Resets the CPU: the
 * program starts at PWRON. */
void bz_emu_init(bz_emu *e, bz_machine *m);

/* One interrupt period: the CPU runs for 6144 cycles, then the NMI comes
 * (unless the self test switch is on, which gates it off). */
void bz_emu_nmi(bz_emu *e);

/* One game frame without interrupts, for checking against a recording:
 * from MAIN, just after its LSR SYNC has shifted a 1 out (the machine
 * holding the memory of the frame's start), until the main line is back
 * at MAIN. Returns the CPU cycles it took, or -1 if it does not come back
 * within max_cycles. */
int bz_emu_frame(bz_emu *e, uint32_t max_cycles);

#endif
