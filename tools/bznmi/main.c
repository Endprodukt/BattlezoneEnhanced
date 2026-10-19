/* bznmi - check the interrupt routine in C against the original's.
 *
 *   bznmi CAPTURE.bzcap [--every N] [--nmis N] [--max N]
 *
 * bzcompare leaves out what the interrupt owns (coins and credits, the
 * tunes, the sound latch, the start of a game, the display buffers): the
 * interrupt came at other moments in MAME than it does here. This holds
 * the interrupt itself, routine against routine: from the memory of every
 * Nth frame of a capture (default 25) two machines run N interrupts each
 * (default 400), one the C routine (bz_nmi), one the original's NMI code
 * on the 6502 core, with the same switches (coins dropped, the slam switch
 * hit, start pressed, by a fixed pattern), the same random numbers, and
 * the main line's part played the same for both (SYNC taken, the buffer
 * handed back now and then). After every interrupt both must hold the
 * same memory and math box and have written the same to the POKEY, the
 * sound latch, the coin counters and the vector generator.
 *
 * Exit status 0 when all are the same.
 */
#include "capture/capture.h"
#include "core/emu.h"
#include "core/machine.h"
#include "game/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct side {
    bz_machine m;
    uint32_t   rnd;             /* the random numbers' place */
    uint32_t   writes;          /* a running checksum of the sound writes */
    uint32_t   nwrites;
} side;

static uint8_t next_rnd(void *u)
{
    side *s = (side *)u;
    s->rnd = s->rnd * 1103515245u + 12345u;
    return (uint8_t)(s->rnd >> 16);
}

static void on_sound(void *u, uint16_t addr, uint8_t v)
{
    side *s = (side *)u;
    s->writes = s->writes * 31u + addr * 257u + v;
    s->nwrites++;
}

static void load(side *s, const bzcap_reader *cap, const bzcap_frame *f)
{
    bz_machine_init(&s->m);
    bz_machine_set_rom(&s->m, cap->rom);
    memcpy(s->m.mem, f->ram, BZCAP_RAM_SIZE);
    memcpy(s->m.mem + 0x2000, f->vgram, BZCAP_VGRAM_SIZE);
    memcpy(s->m.mb.reg, f->mb_reg, sizeof s->m.mb.reg);
    s->m.mb.result = f->mb_result;
    s->m.prng = next_rnd;
    s->m.prng_user = s;
    s->m.sound_write = on_sound;
    s->m.sound_user = s;
    s->rnd = f->frame * 2654435761u;
    s->writes = s->nwrites = 0;
}

/* The switches of interrupt i of the run that starts at frame f. */
static void switches(bz_machine *m, uint32_t f, int i)
{
    uint8_t in0 = 0x3f, in3 = 0;
    unsigned kind = f / 25u;
    /* a coin in one of the three mechs: closed 30 interrupts (120 ms), now and then too short or too long */
    int mech = (int)(kind % 3u);
    int len = (kind % 7u) == 3 ? 3 : (kind % 11u) == 5 ? 260 : 30;
    if (i >= 20 && i < 20 + len) in0 &= (uint8_t)~(1u << mech);
    if ((kind % 4u) == 1 && i >= 120 && i < 150) in0 &= (uint8_t)~(1u << ((mech + 1) % 3));
    /* a second coin into the same mech right behind the first: its credit comes early */
    if ((kind % 6u) == 4 && i >= 70 && i < 100) in0 &= (uint8_t)~(1u << mech);
    /* the slam switch, sometimes right after the coin */
    if ((kind % 5u) == 2 && i >= 60 && i < 64) in0 &= (uint8_t)~BZ_IN0_SLAM;
    /* start, late enough for the coin to have become a credit */
    if ((kind % 2u) == 0 && i >= 250 && i < 262) in3 |= BZ_IN3_START;
    if (i & 64) in3 |= BZ_IN3_FIRE | BZ_IN3_LEFT_FWD;
    m->in.in0 = in0;
    m->in.in3 = in3;
    m->in.dsw1 = (uint8_t)((kind * 37u) & 0xff);        /* every coin mode in turn */
    m->in.dsw0 = (uint8_t)((kind * 29u) & 0xff);
}

/* The main line's part, the same for both. */
static void main_line(bz_machine *m, int i)
{
    m->mem[BZ_SYNC] = 0;
    if (i % 23 == 0) m->mem[BZ_BUFRDY] = 0;
    if (i % 50 == 0) m->mem[BZ_FRAME]++;
}

static uint64_t g_nmi_cycles;      /* what the interrupts took on the core, all together */

static int emu_nmi(bz_emu *e)
{
    m6502 *c = &e->cpu;
    uint64_t before = c->cycles;
    c->pc = BZ_MAIN;
    c->s = 0x7f;
    c->p = M6502_U;
    c->a = 0x11; c->x = 0x22; c->y = 0x33;
    c->jammed = 0;
    m6502_nmi(c);
    for (int n = 0; n < 200000; n++) {
        m6502_step(c);
        if (c->s == 0x7f && c->pc == BZ_MAIN) g_nmi_cycles += c->cycles - before;
        if (c->s == 0x7f && c->pc == BZ_MAIN) return c->a == 0x11 && c->x == 0x22 && c->y == 0x33 ? 0 : -2;
    }
    return -1;
}

/* PWRON in C against the original's: from reset to MAIN. Returns the
 * number of bytes that differ. */
static int pwron_check(const bzcap_reader *cap)
{
    static bz_machine a, b;
    static bz_emu e;
    bz_machine_init(&a);
    bz_machine_set_rom(&a, cap->rom);
    bz_pwron(&a);
    bz_machine_init(&b);
    bz_machine_set_rom(&b, cap->rom);
    bz_emu_init(&e, &b);
    int n;
    for (n = 0; n < 2000000 && e.cpu.pc != BZ_MAIN; n++) m6502_step(&e.cpu);
    if (e.cpu.pc != BZ_MAIN) return -1;
    int nd = 0;
    for (unsigned i = 0; i < 0x3000; i++) {
        if (i >= 0x100 && i < 0x200) continue;      /* the stack */
        if (i >= 0x400 && i < 0x2000) continue;
        if (a.mem[i] != b.mem[i]) {
            if (nd < 8) printf("    power on: %04X here %02X  original %02X\n", i, a.mem[i], b.mem[i]);
            nd++;
        }
    }
    nd += memcmp(a.pokey, b.pokey, sizeof a.pokey) != 0;
    return nd;
}

static const char *arg(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], name)) return argv[i + 1];
    return NULL;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') { i++; continue; }
        path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: bznmi CAPTURE.bzcap [--every N] [--nmis N] [--max N]\n");
        return 2;
    }
    uint32_t every = (uint32_t)atoi(arg(argc, argv, "--every") ? arg(argc, argv, "--every") : "25");
    int nmis = atoi(arg(argc, argv, "--nmis") ? arg(argc, argv, "--nmis") : "400");
    int max_report = atoi(arg(argc, argv, "--max") ? arg(argc, argv, "--max") : "5");

    bzcap_reader cap;
    if (bzcap_open(&cap, path) != 0) {
        fprintf(stderr, "bznmi: cannot read %s\n", path);
        return 2;
    }
    static side nat, org;
    static bz_emu emu;
    static bzcap_frame fr;
    static uint8_t payload[0x40000];
    char tag[4];
    uint32_t len, runs = 0, bad = 0, total = 0, started = 0, credits = 0;
    int rc, reported = 0, pwron_done = 0;

    while ((rc = bzcap_next_chunk(&cap, tag, payload, sizeof payload, &len)) == 1) {
        if (memcmp(tag, "FRAM", 4) != 0 || len != BZCAP_FRAM_SIZE || !cap.have_rom) continue;
        bzcap_parse_frame(payload, &fr);
        if (!pwron_done) {
            int nd = pwron_check(&cap);
            printf("%s: power on (PWRON to MAIN): %s\n", path, nd == 0 ? "the same" : nd < 0 ? "the original did not reach MAIN" : "DIFFERS");
            if (nd) bad++;
            pwron_done = 1;
        }
        if (every == 0 || fr.frame % every != 0) continue;
        load(&nat, &cap, &fr);
        load(&org, &cap, &fr);
        bz_emu_init(&emu, &org.m);
        runs++;
        int was_playing = nat.m.mem[BZ_ATRACT] != 0;
        uint8_t credit0 = nat.m.mem[BZ_CRDT];
        for (int i = 0; i < nmis; i++) {
            switches(&nat.m, fr.frame, i);
            switches(&org.m, fr.frame, i);
            bz_nmi(&nat.m);
            int er = emu_nmi(&emu);
            total++;
            int nd = memcmp(nat.m.mem, org.m.mem, 0x100) != 0 ||
                     memcmp(nat.m.mem + 0x200, org.m.mem + 0x200, 0x200) != 0 ||
                     memcmp(nat.m.mem + 0x2000, org.m.mem + 0x2000, 0x1000) != 0 ||
                     memcmp(&nat.m.mb, &org.m.mb, sizeof nat.m.mb) != 0 ||
                     nat.writes != org.writes || nat.nwrites != org.nwrites ||
                     nat.m.vg_starts != org.m.vg_starts || nat.m.coin_counter != org.m.coin_counter ||
                     nat.m.sound_latch != org.m.sound_latch || nat.rnd != org.rnd;
            if (nd || er) {
                bad++;
                if (reported++ < max_report) {
                    printf("frame %u, interrupt %d: differs%s\n", fr.frame, i,
                           er == -1 ? " (the original's did not return)" : er == -2 ? " (the original's did not restore the registers)" : "");
                    int shown = 0;
                    for (unsigned a = 0; a < 0x400 && shown < 10; a++) {
                        if (a >= 0x100 && a < 0x200) continue;
                        if (nat.m.mem[a] != org.m.mem[a]) {
                            printf("    %04X here %02X  original %02X\n", a, nat.m.mem[a], org.m.mem[a]);
                            shown++;
                        }
                    }
                    if (nat.nwrites != org.nwrites || nat.writes != org.writes)
                        printf("    sound writes: here %u, original %u\n", nat.nwrites, org.nwrites);
                    if (nat.m.sound_latch != org.m.sound_latch)
                        printf("    sound latch: here %02X, original %02X\n", nat.m.sound_latch, org.m.sound_latch);
                    if (nat.m.vg_starts != org.m.vg_starts)
                        printf("    VG starts: here %u, original %u\n", nat.m.vg_starts, org.m.vg_starts);
                    if (nat.rnd != org.rnd) printf("    random numbers read differ\n");
                }
                break;      /* (what follows a difference differs too) */
            }
            main_line(&nat.m, i);
            main_line(&org.m, i);
        }
        if (!was_playing && nat.m.mem[BZ_ATRACT] != 0) started++;
        if (nat.m.mem[BZ_CRDT] != credit0) credits++;
    }
    bzcap_close(&cap);
    printf("%s: the interrupt, %u runs, %u interrupts: %u run(s) differ (%u started a game, %u changed the credits)\n",
           path, runs, total, bad, started, credits);
    if (total) printf("  an interrupt takes %.1f cycles of the 6502 on average\n", (double)g_nmi_cycles / total);
    return bad ? 1 : 0;
}
