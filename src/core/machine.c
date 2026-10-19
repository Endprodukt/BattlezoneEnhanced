#include "core/machine.h"

#include "avg/avg.h"

#include <string.h>

void bz_machine_init(bz_machine *m)
{
    memset(m, 0, sizeof *m);
    m->in.in0 = 0x3f;       /* nothing closed (active low) */
    m->in.dsw0 = 0x15;      /* MAME's defaults: 3 tanks, missile at 10000, bonus at 15k and 100k, English */
    m->in.dsw1 = 0x02;      /* 1 coin 1 play */
    m->in.in3 = 0x00;
    m->lfsr = 0x1ffff;
    bz_mathbox_reset(&m->mb);
}

void bz_machine_set_rom(bz_machine *m, const uint8_t *rom3000)
{
    memcpy(m->mem + 0x3000, rom3000, 0x5000);
    m->have_rom = 1;
}

/* The POKEY's random number: its 17-bit polynomial counter runs at the
 * chip's clock, so what a read sees depends on when it comes. Here it is
 * stepped a varying amount per read. */
static uint8_t lfsr_read(bz_machine *m)
{
    unsigned steps = 9 + (m->lfsr & 0x3f) + (m->nmis & 7);
    for (unsigned i = 0; i < steps; i++) {
        uint32_t fb = ((m->lfsr >> 16) ^ (m->lfsr >> 11)) & 1u;     /* x^17 + x^12 + 1 */
        m->lfsr = ((m->lfsr << 1) | fb) & 0x1ffff;
    }
    return (uint8_t)(m->lfsr >> 8);
}

static uint8_t io_read(bz_machine *m, uint16_t a)
{
    switch (a) {
    case BZ_HALT:
        return (uint8_t)((m->in.in0 & 0x3f) | (m->vg_left == 0 ? BZ_IN0_HALT : 0) |
                         ((m->cpu_cycles & 0x100) ? BZ_IN0_3KHZ : 0));
    case BZ_OPTION: return m->in.dsw0;
    case BZ_OPTON2: return m->in.dsw1;
    case BZ_MSTAT:  return 0x00;                    /* the math box is always done */
    case BZ_MOUTLO: return bz_mathbox_lo(&m->mb);
    case BZ_MOUTHI: return bz_mathbox_hi(&m->mb);
    case BZ_ALLPOT: return m->in.in3;
    case BZ_PRAND:
        m->n_rnd++;
        return m->prng ? m->prng(m->prng_user) : lfsr_read(m);
    default:
        return 0x00;
    }
}

uint8_t bz_rd8(bz_machine *m, uint16_t a)
{
    a &= 0x7fff;
    m->n_rd++;
    if (a >= 0x2000) return m->mem[a];
    uint8_t v = a < 0x400 ? m->mem[a] : io_read(m, a);
    if (m->read_hook && m->watch[a]) {
        v = m->read_hook(m->read_user, a, v);
        if (a < 0x400) m->mem[a] = v;
    }
    return v;
}

void bz_wr8(bz_machine *m, uint16_t a, uint8_t v)
{
    a &= 0x7fff;
    m->n_wr++;
    if (a < 0x400) { m->mem[a] = v; return; }
    if (a >= 0x2000) {
        if (a < 0x3000) {                   /* vector RAM; the ROMs do not take writes */
            m->mem[a] = v;
            m->vgtag[(a - 0x2000) >> 1] = m->tag;
            m->vgnote[(a - 0x2000) >> 1] = m->note;
            m->vgqx[(a - 0x2000) >> 1] = m->note_qx;
            m->vgqy[(a - 0x2000) >> 1] = m->note_qy;
        }
        return;
    }
    if (a >= BZ_MBOXIN && a < BZ_MBOXIN + 0x20) {
        m->n_mbox++;
        bz_mathbox_go(&m->mb, a - BZ_MBOXIN, v);
        return;
    }
    if (a >= BZ_POKEY && a < BZ_POKEY + 0x10) {
        m->pokey[a - BZ_POKEY] = v;
        if (m->sound_write) m->sound_write(m->sound_user, a, v);
        return;
    }
    switch (a) {
    case BZ_SOUND:
        m->sound_latch = v;
        if (m->sound_write) m->sound_write(m->sound_user, a, v);
        return;
    case BZ_EMCNT:  m->coin_counter = v; return;
    case BZ_GOADD:
        m->vg_starts++;
        m->vg_left = bz_avg_cycles(m->mem, 0x2000);
        return;
    case BZ_VSTOP:  m->vg_left = 0; return;
    case BZ_WTCHDG: m->watchdog++; return;
    default: return;
    }
}
