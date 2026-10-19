#include "core/emu.h"

#include <string.h>

static uint8_t bus_read(void *user, uint16_t a)
{
    return bz_rd8(((bz_emu *)user)->m, a);
}

static void bus_write(void *user, uint16_t a, uint8_t v)
{
    bz_wr8(((bz_emu *)user)->m, a, v);
}

void bz_emu_init(bz_emu *e, bz_machine *m)
{
    memset(e, 0, sizeof *e);
    e->m = m;
    m6502_init(&e->cpu, bus_read, bus_write, e);
    m6502_reset(&e->cpu);
}

static void run_step(bz_emu *e)
{
    bz_machine *m = e->m;
    int cy = m6502_step(&e->cpu);
    e->budget -= cy;
    m->cpu_cycles = e->cpu.cycles;
    uint32_t mc = (uint32_t)cy * 8;
    m->vg_left = m->vg_left > mc ? m->vg_left - mc : 0;
}

void bz_emu_nmi(bz_emu *e)
{
    /* The CPU runs first: the board's first interrupt comes a period after
     * power on, when PWRON has long set the stack pointer (the interrupt
     * routine takes a stack pointer above 7F for a crash and stops). */
    e->budget += BZ_CPU_CYCLES_PER_NMI;
    while (e->budget > 0) run_step(e);
    if (e->m->in.in0 & BZ_IN0_TEST) {       /* (active low: the switch is off) */
        m6502_nmi(&e->cpu);
        e->nmis++;
        e->m->nmis++;
    }
}

int bz_emu_frame(bz_emu *e, uint32_t max_cycles)
{
    m6502 *c = &e->cpu;
    /* At MAIN's BCC, the carry set by the LSR SYNC before it. */
    c->pc = BZ_MAIN + 2;
    c->s = 0x7f;
    c->p = M6502_C | M6502_U | M6502_I;
    c->a = c->x = c->y = 0;
    c->nmi_pending = 0;
    c->jammed = 0;
    uint64_t start = c->cycles;
    e->budget = (int32_t)max_cycles;
    while (e->budget > 0) {
        run_step(e);
        if (c->pc == BZ_MAIN) return (int)(c->cycles - start);
    }
    return -1;
}
