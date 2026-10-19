/* pace.c - see pace.h. */
#include "game/pace.h"

#include "game/game.h"

uint32_t bz_pace_cycles(uint32_t n_rd, uint32_t n_wr, uint32_t n_mbox)
{
    /* cycles = -705 + 2.941 reads + 10.145 writes - 2.846 math box starts */
    double c = -705.2 + 2.9414 * n_rd + 10.1448 * n_wr - 2.8462 * n_mbox;
    return c < 1000.0 ? 1000u : (uint32_t)c;
}

int bz_pace_poll(bz_machine *m, bz_pace *p)
{
    if (!p->working) {
        if (!bz_main_ready(m)) return 0;
        p->working = 1;
        p->busy = (p->last ? p->last : 56000) + p->owed;
    } else {
        p->busy -= BZ_PACE_CYCLES_PER_NMI;
    }
    if (p->busy > 0) return 0;
    uint32_t frames = m->frames, rd = m->n_rd, wr = m->n_wr, mb = m->n_mbox;
    bz_main_poll(m);
    p->working = 0;
    if (m->frames == frames) return 0;
    int32_t took = (int32_t)bz_pace_cycles(m->n_rd - rd, m->n_wr - wr, m->n_mbox - mb);
    p->owed = took - (p->last ? p->last : 56000);
    p->last = took;
    return 1;
}
