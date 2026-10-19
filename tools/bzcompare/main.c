/* bzcompare - check the game frame by frame against MAME.
 *
 *   bzcompare CAPTURE.bzcap [--emu] [--syms bz_symbols.h]
 *             [--show N] [--max N] [--from F] [--to F] [--quiet]
 *
 * The capture (tools/mame/bzcapture.lua, MAME set bzonea) holds the
 * original's memory at the start of every game frame (FRAM). For each pair
 * of consecutive frames the machine is loaded with the first, fed the
 * random numbers and the switches the original read, runs one frame, and
 * must then hold what the second has: RAM, the math box and the display
 * list just written. Every frame is checked on its own, so one difference
 * does not hide the next.
 *
 *   default  the C routines of src/game
 *   --emu    the original program on the 6502 core (checks the core, the
 *            machine and the math box; needs the capture's own ROMs only)
 *
 * Not compared, because the interrupt owns it and ran at other moments
 * than here: credits and coins, the sync and buffer handshakes, the
 * explosion timers it counts down, the tunes. A frame during which the
 * interrupt started a game (it then sets up half the game's variables) is
 * passed over.
 *
 * Exit status 0 when every frame compared matches.
 */
#include "capture/capture.h"
#include "core/emu.h"
#include "core/machine.h"
#include "game/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- what is left out ------------------------------------------------------ */

typedef struct range { uint16_t lo, hi; const char *why; } range;

static const range SKIP[] = {
    { BZ_CRDT, BZ_CRDT, "NMI: credits" },
    { BZ_EXPCNT, BZ_EXPCNT, "NMI: counts the explosion down" },
    { BZ_CNCT, BZ_CNCT, "NMI: coin count" },
    { BZ_BUFRDY, BZ_BUFRDY + 1, "NMI: buffer handshake" },
    { BZ_SYNC, BZ_INTCT, "NMI: frame sync, interrupt count" },
    { BZ_SXPCNT, BZ_SXPCNT, "NMI: counts the shell's sound down" },
    { BZ_CCTIM, BZ_VTIMER, "NMI: coins, VG timer" },
    { BZ_SOUNDN, 0x00ff, "NMI: the tunes" },
    { 0x0100, 0x01ff, "6502 stack" },
    { 0x2001, 0x2001, "NMI: which buffer is shown" },
};
#define NSKIP ((int)(sizeof SKIP / sizeof SKIP[0]))

static int skipped(uint16_t a)
{
    for (int i = 0; i < NSKIP; i++)
        if (a >= SKIP[i].lo && a <= SKIP[i].hi) return 1;
    return 0;
}

/* --- symbol names for the report --------------------------------------------- */

typedef struct sym { uint16_t addr; char name[32]; } sym;
static sym  *syms;
static int   nsyms;

static int sym_cmp(const void *a, const void *b)
{
    return (int)((const sym *)a)->addr - (int)((const sym *)b)->addr;
}

static void load_syms(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256], name[64];
    unsigned addr;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "#define BZ_%63s 0x%x", name, &addr) != 2) continue;
        if (addr >= 0x400) continue;
        syms = (sym *)realloc(syms, (size_t)(nsyms + 1) * sizeof *syms);
        syms[nsyms].addr = (uint16_t)addr;
        snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%.31s", name);
        nsyms++;
    }
    fclose(f);
    qsort(syms, (size_t)nsyms, sizeof *syms, sym_cmp);
}

static const char *sym_at(uint16_t a, char *buf, int size)
{
    const sym *best = NULL;
    for (int i = 0; i < nsyms && syms[i].addr <= a; i++) best = &syms[i];
    if (a >= 0x2000) snprintf(buf, (size_t)size, "VG RAM");
    else if (!best) snprintf(buf, (size_t)size, "?");
    else if (best->addr == a) snprintf(buf, (size_t)size, "%s", best->name);
    else snprintf(buf, (size_t)size, "%s+%X", best->name, (unsigned)(a - best->addr));
    return buf;
}

/* --- what the original read ---------------------------------------------------- */

typedef struct feed { const uint8_t *v; uint32_t n, used, over; } feed;

static uint8_t feed_rnd(void *u)
{
    feed *fd = (feed *)u;
    if (fd->used < fd->n) return fd->v[fd->used++];
    fd->over++;
    return 0;
}

typedef struct rdfeed { const uint8_t *r; uint32_t n, used, bad; int strict; } rdfeed;

/* A read of a watched address: the value the original read at this point
 * of the frame. The original program makes exactly the reads recorded;
 * the C routines may leave one out or fetch a value once where the
 * original fetched it twice, so for them the record is looked for a
 * little ahead. */
static int g_trace;

static uint8_t feed_read(void *u, uint16_t a, uint8_t cur)
{
    rdfeed *fd = (rdfeed *)u;
    if (g_trace) printf("    read %04X (record %u of %u)\n", a, fd->used, fd->n);
    uint32_t look = fd->strict ? 1 : 8;
    for (uint32_t k = 0; k < look && fd->used + k < fd->n; k++) {
        const uint8_t *rec = fd->r + 3 * (fd->used + k);
        if ((uint16_t)(rec[0] | (rec[1] << 8)) == a) {
            fd->used += k + 1;
            return rec[2];
        }
    }
    fd->bad++;
    return cur;
}

/* --- one frame ------------------------------------------------------------------ */

static void load(bz_machine *m, const bzcap_reader *cap, const bzcap_frame *s)
{
    bz_machine_init(m);
    bz_machine_set_rom(m, cap->rom);
    memcpy(m->mem, s->ram, BZCAP_RAM_SIZE);
    memcpy(m->mem + 0x2000, s->vgram, BZCAP_VGRAM_SIZE);
    memcpy(m->mb.reg, s->mb_reg, sizeof m->mb.reg);
    m->mb.result = s->mb_result;
    m->mem[BZ_SYNC] >>= 1;          /* the LSR the record was taken in */
}

static const char *arg(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], name)) return argv[i + 1];
    return NULL;
}

static int flag(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], name)) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--emu") || !strcmp(argv[i], "--quiet")) continue;
        if (!strcmp(argv[i], "--csv")) { i++; continue; }
        if (argv[i][0] == '-') { i++; continue; }
        path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: bzcompare CAPTURE.bzcap [--emu] [--syms FILE] [--show N] [--max N] [--from F] [--to F]\n");
        return 2;
    }
    int use_emu = flag(argc, argv, "--emu");
    int quiet = flag(argc, argv, "--quiet");
    /* --csv FILE: for every frame the cycles the original takes on the 6502
     * core next to what the C routines count (to fit game/pace.c by). */
    FILE *csv = arg(argc, argv, "--csv") ? fopen(arg(argc, argv, "--csv"), "a") : NULL;
    int show = atoi(arg(argc, argv, "--show") ? arg(argc, argv, "--show") : "12");
    int max_report = atoi(arg(argc, argv, "--max") ? arg(argc, argv, "--max") : "10");
    uint32_t from = (uint32_t)atoi(arg(argc, argv, "--from") ? arg(argc, argv, "--from") : "0");
    uint32_t to = (uint32_t)strtoul(arg(argc, argv, "--to") ? arg(argc, argv, "--to") : "4000000000", NULL, 10);
    load_syms(arg(argc, argv, "--syms") ? arg(argc, argv, "--syms") : "src/core/bz_symbols.h");

    bzcap_reader cap;
    if (bzcap_open(&cap, path) != 0) {
        fprintf(stderr, "bzcompare: cannot read %s\n", path);
        return 2;
    }

    static bz_machine m;
    static bz_emu emu;
    static bzcap_frame prev, next;
    static uint8_t payload[0x40000];
    static uint8_t rnd[0x10000], irds[0x30000];
    uint32_t nrnd = 0, nirds = 0, len;
    int have_prev = 0, poked = 0;
    char tag[4];
    uint32_t compared = 0, differing = 0, started = 0, reported = 0, stuck = 0;
    uint32_t rnd_wrong = 0, read_wrong = 0;
    uint64_t cycles_sum = 0;
    uint32_t cycles_max = 0;
    double t_first = -1.0, t_last = 0.0;
    int rc;

    while ((rc = bzcap_next_chunk(&cap, tag, payload, sizeof payload, &len)) == 1) {
        if (!memcmp(tag, "RNDM", 4)) {
            /* (the chunk was read into payload: only as much as fits there) */
            nrnd = len < sizeof payload ? len : (uint32_t)sizeof payload;
            if (nrnd > sizeof rnd) nrnd = sizeof rnd;
            memcpy(rnd, payload, nrnd);
            continue;
        }
        if (!memcmp(tag, "IRDS", 4)) {
            nirds = len < sizeof payload ? len : (uint32_t)sizeof payload;
            if (nirds > sizeof irds) nirds = sizeof irds;
            memcpy(irds, payload, nirds);
            nirds /= 3;
            continue;
        }
        if (!memcmp(tag, "TIME", 4) && len == 8) {
            double t;
            memcpy(&t, payload, 8);
            if (t_first < 0.0) t_first = t;
            t_last = t;
            continue;
        }
        if (!memcmp(tag, "POKE", 4)) { poked = 1; continue; }
        if (memcmp(tag, "FRAM", 4) != 0 || len != BZCAP_FRAM_SIZE) continue;

        bzcap_parse_frame(payload, &next);
        if (have_prev && !poked && prev.frame >= from && prev.frame <= to && cap.have_rom) {
            /* The interrupt started a game during the frame: not the main line's doing. */
            int start = prev.ram[BZ_ATRACT] == 0 && next.ram[BZ_ATRACT] != 0;
            load(&m, &cap, &prev);
            feed fr = { rnd, nrnd, 0, 0 };
            rdfeed fd = { irds, nirds, 0, 0, use_emu };
            m.prng = feed_rnd;
            m.prng_user = &fr;
            m.read_hook = feed_read;
            m.read_user = &fd;
            bzcap_watch_set(m.watch);

            g_trace = arg(argc, argv, "--trace") && (uint32_t)atoi(arg(argc, argv, "--trace")) == prev.frame;
            if (g_trace) {
                printf("frame %u, the original read:", prev.frame);
                for (uint32_t i = 0; i < nirds; i++)
                    printf(" %04X=%02X", irds[3 * i] | (irds[3 * i + 1] << 8), irds[3 * i + 2]);
                printf("\n");
            }
            int cy = 0;
            if (use_emu) {
                bz_emu_init(&emu, &m);
                cy = bz_emu_frame(&emu, 3000000);
            } else {
                bz_frame(&m);
            }
            if (csv && !use_emu && !start) {
                static bz_machine m2;
                static bz_emu e2;
                load(&m2, &cap, &prev);
                feed fr2 = { rnd, nrnd, 0, 0 };
                rdfeed fd2 = { irds, nirds, 0, 0, 1 };
                m2.prng = feed_rnd;
                m2.prng_user = &fr2;
                m2.read_hook = feed_read;
                m2.read_user = &fd2;
                bzcap_watch_set(m2.watch);
                bz_emu_init(&e2, &m2);
                int c2 = bz_emu_frame(&e2, 3000000);
                if (c2 > 0) fprintf(csv, "%d,%u,%u,%u\n", c2, (unsigned)m.n_rd, (unsigned)m.n_wr, (unsigned)m.n_mbox);
            }
            if (start) {
                started++;
            } else if (cy < 0) {
                stuck++;
                if (!quiet && reported++ < (uint32_t)max_report)
                    printf("frame %u: the program did not come back to MAIN (pc %04X)\n", prev.frame, emu.cpu.pc);
            } else {
                compared++;
                cycles_sum += (uint32_t)cy;
                if ((uint32_t)cy > cycles_max) cycles_max = (uint32_t)cy;
                int nd = 0;
                for (unsigned a = 0; a < 0x400; a++)
                    if (!skipped((uint16_t)a) && m.mem[a] != next.ram[a]) nd++;
                for (unsigned a = 0; a < 0x1000; a++)
                    if (!skipped((uint16_t)(0x2000 + a)) && m.mem[0x2000 + a] != next.vgram[a]) nd++;
                int mbd = 0;
                for (int i = 0; i < 16; i++) mbd += m.mb.reg[i] != next.mb_reg[i];
                mbd += m.mb.result != next.mb_result;
                int feed_bad = fr.over || fr.used != fr.n || fd.bad;
                if (fr.over || fr.used != fr.n) rnd_wrong++;
                if (fd.bad) read_wrong++;
                if (nd || mbd || feed_bad) {
                    differing++;
                    if (!quiet && reported++ < (uint32_t)max_report) {
                        printf("frame %u: %d byte(s) differ, math box %d; random numbers %u of %u%s, switch reads %u of %u, %u not found\n",
                               prev.frame, nd, mbd, fr.used, fr.n, fr.over ? " (ran out)" : "", fd.used, fd.n, fd.bad);
                        int shown = 0;
                        char nm[64];
                        for (unsigned a = 0; a < 0x400 && shown < show; a++) {
                            if (skipped((uint16_t)a) || m.mem[a] == next.ram[a]) continue;
                            printf("    %04X %-12s here %02X  MAME %02X  (was %02X)\n", a, sym_at((uint16_t)a, nm, sizeof nm),
                                   m.mem[a], next.ram[a], prev.ram[a]);
                            shown++;
                        }
                        for (unsigned a = 0; a < 0x1000 && shown < show; a++) {
                            if (skipped((uint16_t)(0x2000 + a)) || m.mem[0x2000 + a] == next.vgram[a]) continue;
                            printf("    %04X %-12s here %02X  MAME %02X  (was %02X)\n", 0x2000 + a, "VG RAM",
                                   m.mem[0x2000 + a], next.vgram[a], prev.vgram[a]);
                            shown++;
                        }
                        for (int i = 0; i < 16 && mbd; i++)
                            if (m.mb.reg[i] != next.mb_reg[i])
                                printf("    math box R%X here %04X  MAME %04X\n", i, (uint16_t)m.mb.reg[i], (uint16_t)next.mb_reg[i]);
                        if (mbd && m.mb.result != next.mb_result)
                            printf("    math box result here %04X  MAME %04X\n", (uint16_t)m.mb.result, (uint16_t)next.mb_result);
                    }
                }
            }
        }
        prev = next;
        have_prev = 1;
        poked = 0;
        nrnd = nirds = 0;
    }
    bzcap_close(&cap);
    if (rc < 0) fprintf(stderr, "bzcompare: %s is damaged at its end\n", path);

    printf("%s, %s: %u frames compared, %u differ", path, use_emu ? "the original on the 6502 core" : "the C routines",
           compared, differing);
    if (started) printf(", %u passed over (game started by the interrupt)", started);
    if (stuck) printf(", %u stuck", stuck);
    if (rnd_wrong || read_wrong) printf("; random numbers off in %u, switch reads in %u", rnd_wrong, read_wrong);
    printf("\n");
    if (use_emu && compared)
        printf("  6502 cycles a frame: average %u, most %u (an interrupt period is %d; 16 of them make a frame)\n",
               (unsigned)(cycles_sum / compared), cycles_max, BZ_CPU_CYCLES_PER_NMI);
    if (t_last > t_first && compared)
        printf("  MAME: %.2f frames a second over %.1f s\n", (double)compared / (t_last - t_first), t_last - t_first);
    return differing || stuck ? 1 : 0;
}
