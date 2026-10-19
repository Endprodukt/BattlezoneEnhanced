/* bzshot - run the game without a screen and write what it shows.
 *
 *   bzshot --roms PATH [--original] [--play "SCRIPT"] [--seconds S]
 *          [--svg OUT.svg] [--at S] [--every S] [--wav OUT.wav] [--overlay]
 *
 * The game runs for S seconds of its own time (default 10). --svg writes
 * the picture at the end (or at --at seconds; with --every a numbered
 * file each so many seconds: OUT-001.svg ...) as the vector generator's
 * lines; --wav the sound. --play is a list of "TIME WHAT [ARG]" separated
 * by ';', TIME in seconds:
 *   coin | start | fire        press the switch for 0.1 s (ARG: seconds)
 *   drive L R                  the two levers: f, b or - each
 *   aim [off]                  from then on the tank turns towards the enemy and fires when he is ahead
 *   yoke L R                   the analog mode: the two levers, -256..256 each ("yoke off": the switches again)
 * At the end it prints what the game's state is (score, tanks left, the
 * frames and interrupts run).
 */
#include "avg/avg.h"
#include "core/emu.h"
#include "core/machine.h"
#include "game/game.h"
#include "game/pace.h"
#include "render/color.h"
#include "render/enhance.h"
#include "render/smooth.h"
#include "rom/romset.h"
#include "sound/sound.h"

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* --- the picture ------------------------------------------------------------- */

static const bz_enhance *g_enh;     /* the enhancements, for their colors */
static double g_enh_now;
static int g_color = 3;         /* 0 white, 1 overlay, 2 color (classic), 3 flyer */

static int write_svg(const char *path, const bz_machine *m, const bz_line *lines, int count)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    /* What MAME shows: -290..290 by -200..200, y up. */
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-290 -200 580 400\" width=\"1160\" height=\"800\">\n");
    fprintf(f, "<rect x=\"-290\" y=\"-200\" width=\"580\" height=\"400\" fill=\"black\"/>\n");
    for (int pass = 0; pass < 2; pass++) {
        /* a wide dim stroke under a thin bright one: the glow of the tube */
        for (int i = 0; i < count; i++) {
            const bz_line *l = &lines[i];
            double k = (0.25 + 0.75 * l->intensity / 15.0) * (l->gain > 0.0f ? l->gain : 1.0);
            uint8_t rgb[3] = { 255, 255, 255 };
            if (g_color >= 2) {
                float kk = (float)k;
                int palette = g_color == 3 ? BZ_PALETTE_FLYER : BZ_PALETTE_CLASSIC;
                bz_color_of(m->mem, l, palette, rgb);
                bz_enhance_color(g_enh, l, g_enh_now, palette, bz_color_horizon(palette), rgb, &kk);
                k = kk;
            }
            else if (g_color == 1) {
                int top = (l->y0 + l->y1) / 2 > 120.0f;
                rgb[0] = top ? 255 : 60; rgb[1] = top ? 60 : 255; rgb[2] = 60;
            }
            double w = pass ? 0.7 : 2.6, op = pass ? k : k * 0.22;
            if (l->x0 == l->x1 && l->y0 == l->y1)
                fprintf(f, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\"/>\n",
                        l->x0, -l->y0, pass ? 0.9 : 2.2, rgb[0], rgb[1], rgb[2], op);
            else
                fprintf(f, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"rgb(%d,%d,%d)\" stroke-opacity=\"%.2f\" "
                           "stroke-width=\"%.1f\" stroke-linecap=\"round\"/>\n",
                        l->x0, -l->y0, l->x1, -l->y1, rgb[0], rgb[1], rgb[2], op, w);
        }
    }
    fprintf(f, "</svg>\n");
    fclose(f);
    return count;
}

/* The picture now: of the C routines through the pictures in between
 * (render/smooth.h), of the original as its display list has it. */
static int shot(const char *path, const bz_machine *m, bz_smooth *sm, int original, double now, int smooth)
{
    static bz_avg_list list;
    if (original) {
        bz_avg_run(m->mem, BZ_VECRAM, &list, 0);
        return write_svg(path, m, list.lines, list.count);
    }
    const bz_line *lines;
    g_enh_now = now;
    int n = bz_smooth_lines(sm, now, smooth, &lines);
    return write_svg(path, m, lines, n);
}

/* How well a frame's picture drawn ahead meets the picture the frame then
 * draws: of the lines that move, how many have their partner, and how far
 * apart the two are. */
typedef struct ahead_stats { long lines, unmatched; double sum, worst; long frames; } ahead_stats;

static void ahead_check(ahead_stats *st, const bz_avg_list *pred, const bz_avg_list *real)
{
    st->frames++;
    for (int i = 0; i < real->count; i++) {
        const bz_line *a = &real->lines[i];
        int kind = BZ_TAG_KIND(a->tag);
        if (kind != BZ_TAG_MOUNT && kind != BZ_TAG_VOLCANO && kind != BZ_TAG_OBJECT && kind != BZ_TAG_SWEEP && kind != BZ_TAG_BLIP)
            continue;
        st->lines++;
        const bz_line *b = NULL;
        for (int k = 0; k < pred->count && !b; k++) {
            const bz_line *c = &pred->lines[k];
            if (c->tag == a->tag && c->sub == a->sub && (a->sub ? c->n == a->n : c->ord == a->ord)) b = c;
        }
        if (!b) { st->unmatched++; continue; }
        double d = fabs(a->x0 - b->x0) + fabs(a->y0 - b->y0) + fabs(a->x1 - b->x1) + fabs(a->y1 - b->y1);
        st->sum += d;
        if (d > st->worst) st->worst = d;
    }
}

/* How evenly the land goes by: the picture as a 60 Hz screen gets it, and
 * from one to the next how far the mountains' lines have gone sideways. A
 * steady turn should move them the same distance every time. */
typedef struct even_stats {
    struct { uint16_t sub, n; float x; uint8_t have; } was[4096];
    long   samples, stalls, jumps, slow, fast;
    double sum, sum2, mean;           /* mean: a running one, to tell a stall or a jump by */
} even_stats;

static void even_check(even_stats *st, const bz_line *lines, int count)
{
    static uint8_t seen[4096];
    double sum = 0.0;
    int n = 0;
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < count; i++) {
        const bz_line *l = &lines[i];
        if (BZ_TAG_KIND(l->tag) != BZ_TAG_MOUNT || !l->sub) continue;
        unsigned k = (l->sub * 31u + (unsigned)l->n) & 4095u;
        if (seen[k]) continue;
        seen[k] = 1;
        if (st->was[k].have && st->was[k].sub == l->sub && st->was[k].n == l->n && fabs(l->x0 - st->was[k].x) < 40.0) {
            sum += l->x0 - st->was[k].x;
            n++;
        }
        st->was[k].sub = l->sub; st->was[k].n = (uint16_t)l->n; st->was[k].x = l->x0; st->was[k].have = 1;
    }
    for (unsigned k = 0; k < 4096; k++)
        if (!seen[k]) st->was[k].have = 0;
    if (n < 4) return;
    double d = fabs(sum / n);
    if (st->mean > 0.5) {       /* (turning) */
        st->samples++;
        st->sum += d;
        st->sum2 += d * d;
        if (d < 0.85 * st->mean) st->slow++;
        if (d > 1.15 * st->mean) st->fast++;
        if (d < 0.5 * st->mean) st->stalls++;
        if (d > 1.5 * st->mean) st->jumps++;
    }
    st->mean = st->mean * 0.9 + d * 0.1;
}

/* How evenly single lines move: every line of the world that can be told
 * apart (its tag, the vector ROM routine, its number: once in the picture)
 * is followed from one picture to the next, and its step compared with
 * the step before. Motion that is even has steps that stay the same; one
 * that stops and goes has steps that change by as much as they are. */
typedef struct jerk_stats {
    struct { uint64_t key; float x, y, dx, dy; uint8_t have, have_step, seen, twice; } was[8192];
    double change, step;        /* summed: how much a step differs from the one before; the steps */
    long   n;
} jerk_stats;

static void jerk_check(jerk_stats *st, const bz_line *lines, int count)
{
    for (unsigned k = 0; k < 8192; k++) st->was[k].seen = st->was[k].twice = 0;
    for (int i = 0; i < count; i++) {
        const bz_line *l = &lines[i];
        int kind = BZ_TAG_KIND(l->tag);
        if (kind != BZ_TAG_MOUNT && kind != BZ_TAG_OBJECT) continue;
        uint64_t key = ((uint64_t)l->tag << 32) | ((uint64_t)l->sub << 16) | (l->sub ? l->n : l->ord);
        unsigned k = (unsigned)((key * 0x9e3779b97f4a7c15ull) >> 51);
        if (st->was[k].seen) { st->was[k].twice = 1; continue; }
        st->was[k].seen = 1;
        if (st->was[k].have && st->was[k].key == key && !st->was[k].twice) {
            float mx = 0.5f * (l->x0 + l->x1), my = 0.5f * (l->y0 + l->y1);     /* (the middle: either way round) */
            float dx = mx - st->was[k].x, dy = my - st->was[k].y;
            if (fabsf(dx) < 30.0f && fabsf(dy) < 30.0f) {
                if (st->was[k].have_step) {
                    double step = hypot(dx, dy) + hypot(st->was[k].dx, st->was[k].dy);
                    if (step > 0.4) {       /* (moving) */
                        st->change += hypot(dx - st->was[k].dx, dy - st->was[k].dy);
                        st->step += 0.5 * step;
                        st->n++;
                    }
                }
                st->was[k].dx = dx; st->was[k].dy = dy; st->was[k].have_step = 1;
            } else {
                st->was[k].have_step = 0;
            }
        } else {
            st->was[k].have_step = 0;
        }
        st->was[k].key = key; st->was[k].x = 0.5f * (l->x0 + l->x1); st->was[k].y = 0.5f * (l->y0 + l->y1); st->was[k].have = 1;
    }
    for (unsigned k = 0; k < 8192; k++)
        if (!st->was[k].seen || st->was[k].twice) st->was[k].have = st->was[k].have_step = 0;
}

/* --- the sound --------------------------------------------------------------- */

static void wav_header(FILE *f, uint32_t samples)
{
    uint32_t bytes = samples * 4, rate = BZ_SOUND_RATE, v;     /* (pairs: left, right) */
    uint16_t w;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    w = 1; fwrite(&w, 2, 1, f); w = 2; fwrite(&w, 2, 1, f);
    fwrite(&rate, 4, 1, f); v = rate * 4; fwrite(&v, 4, 1, f);
    w = 4; fwrite(&w, 2, 1, f); w = 16; fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
}

/* --- the played script --------------------------------------------------------- */

typedef struct event { double t; char what[16]; char a[16], b[16]; } event;

static int parse_play(const char *s, event *ev, int max)
{
    int n = 0;
    char copy[2048];
    if (!s) return 0;
    snprintf(copy, sizeof copy, "%s", s);
    for (char *p = strtok(copy, ";"); p && n < max; p = strtok(NULL, ";")) {
        event *e = &ev[n];
        memset(e, 0, sizeof *e);
        if (sscanf(p, "%lf %15s %15s %15s", &e->t, e->what, e->a, e->b) >= 2) n++;
    }
    return n;
}

int main(int argc, char **argv)
{
    static bz_romset roms;
    const char *paths[32];
    int np = 0;
    if (arg(argc, argv, "--roms")) np = bz_romset_add_dir(paths, np, 32, arg(argc, argv, "--roms"));
    np += bz_romset_default_paths(paths + np, 32 - np);
    if (bz_romset_load(&roms, paths, np) != 0) {
        char missing[512];
        fprintf(stderr, "bzshot: ROMs missing: %s\n", bz_romset_missing(&roms, missing, sizeof missing));
        return 2;
    }

    static bz_machine m;
    static bz_emu emu;
    static bz_sound snd;
    int original = flag(argc, argv, "--original");
    static bz_smooth sm;
    static bz_avg_list predicted;
    ahead_stats ahead = { 0, 0, 0.0, 0.0, 0 };
    static even_stats even;
    static jerk_stats jerk;
    double beam_lit = 0.0, beam_dark = 0.0;
    long beam_pictures = 0;
    double next_even = 0.0, even_hz = arg(argc, argv, "--hz") ? atof(arg(argc, argv, "--hz")) : 60.0;
    int smooth = !flag(argc, argv, "--nosmooth");
    static bz_solid solid;
    const char *color = arg(argc, argv, "--color");
    if (color) g_color = !strcmp(color, "white") ? 0 : !strcmp(color, "overlay") ? 1 : !strcmp(color, "classic") ? 2 : 3;
    if (flag(argc, argv, "--overlay")) g_color = 1;
    bz_smooth_init(&sm);
    int occlusion = !flag(argc, argv, "--nosolid");
    sm.grid = !flag(argc, argv, "--nogrid");
    /* The enhancements: all on, --plain none. */
    static bz_enhance enh;
    int plain = flag(argc, argv, "--plain"), pace = flag(argc, argv, "--pace");
    bz_pace pacer = { 0, 0, 0, 0 };
    bz_enhance_init(&enh, &m);
    if (!plain) {
        enh.on.particles = enh.on.stars = enh.on.far = enh.on.volcano = enh.on.flash = enh.on.levers = 1;
    }
    sm.enh = &enh;
    sm.exact = !flag(argc, argv, "--rounded");
    sm.computed = !flag(argc, argv, "--lines");     /* (--lines: the frame's lines moved, not the picture computed) */
    sm.order = flag(argc, argv, "--unordered") ? 0 : flag(argc, argv, "--nearest") ? 1 : 2;
    g_enh = &enh;
    double seconds = atof(arg(argc, argv, "--seconds") ? arg(argc, argv, "--seconds") : "10");
    double at = arg(argc, argv, "--at") ? atof(arg(argc, argv, "--at")) : -1.0;
    double every = arg(argc, argv, "--every") ? atof(arg(argc, argv, "--every")) : 0.0;
    const char *svg = arg(argc, argv, "--svg");
    const char *wav = arg(argc, argv, "--wav");
    static event ev[64];
    int nev = parse_play(arg(argc, argv, "--play"), ev, 64);

    bz_machine_init(&m);
    if (arg(argc, argv, "--dsw0")) m.in.dsw0 = (uint8_t)strtoul(arg(argc, argv, "--dsw0"), NULL, 16);
    if (arg(argc, argv, "--dsw1")) m.in.dsw1 = (uint8_t)strtoul(arg(argc, argv, "--dsw1"), NULL, 16);
    bz_machine_set_rom(&m, roms.rom);
    bz_solid_init(&solid, m.mem);
    if (flag(argc, argv, "--soundlist")) {
        /* How long each of the POKEY's tunes is: SNDON, then MODSND an
         * interrupt at a time until all four of its sequences have ended. */
        bz_pwron(&m);
        for (int i = 0; i < 8; i++) {
            m.r.a = (uint8_t)(1 << i);
            bz_sndon(&m);
            int n = 0;
            do { bz_modsnd(&m); n++; }
            while ((m.mem[BZ_POINT] | m.mem[BZ_POINT + 1] | m.mem[BZ_POINT + 2] | m.mem[BZ_POINT + 3]) && n < 100000);
            printf("%-8s %5d interrupts, %.2f s\n", bz_sample_name(i), n, n / BZ_NMI_HZ);
        }
        return 0;
    }
    if (occlusion) sm.solid = &solid;
    bz_sound_init(&snd);
    m.sound_write = bz_sound_write;
    m.sound_user = &snd;
    if (original) bz_emu_init(&emu, &m);
    else bz_pwron(&m);
    m.sound_request = bz_sound_request;
    if (arg(argc, argv, "--sounds")) {
        int found = bz_samples_load(&snd.smp, arg(argc, argv, "--sounds"), BZ_SOUND_RATE);
        snd.smp.enabled = 1;
        printf("bzshot: %d sound(s) replaced by recordings\n", found);
    }

    FILE *wf = NULL;
    uint32_t wav_samples = 0;
    if (wav) {
        wf = fopen(wav, "wb");
        if (wf) wav_header(wf, 0);
    }

    long nmis = (long)(seconds * BZ_NMI_HZ);
    double hold[3] = { 0, 0, 0 };           /* coin, start, fire: held until */
    double next_shot = every > 0.0 ? every : -1.0;
    int shots = 0, lines = 0, at_done = 0;
    uint8_t levers = 0, last_frame = 0;
    int aiming = 0;
    unsigned frames = 0;
    for (long i = 0; i < nmis; i++) {
        double t = (double)i / BZ_NMI_HZ;
        for (int k = 0; k < nev; k++) {
            if (ev[k].t < 0.0 || ev[k].t > t) continue;
            double len = ev[k].a[0] ? atof(ev[k].a) : 0.1;
            if (!strcmp(ev[k].what, "coin")) hold[0] = t + len;
            else if (!strcmp(ev[k].what, "start")) hold[1] = t + len;
            else if (!strcmp(ev[k].what, "fire")) hold[2] = t + len;
            else if (!strcmp(ev[k].what, "drive")) {
                levers = 0;
                if (ev[k].a[0] == 'f') levers |= BZ_IN3_LEFT_FWD;
                if (ev[k].a[0] == 'b') levers |= BZ_IN3_LEFT_BACK;
                if (ev[k].b[0] == 'f') levers |= BZ_IN3_RIGHT_FWD;
                if (ev[k].b[0] == 'b') levers |= BZ_IN3_RIGHT_BACK;
            } else if (!strcmp(ev[k].what, "aim")) {
                aiming = strcmp(ev[k].a, "off") != 0;
            } else if (!strcmp(ev[k].what, "yoke")) {
                levers = 0;
                m.analog.on = strcmp(ev[k].a, "off") != 0;
                m.analog.left = (int16_t)atoi(ev[k].a);
                m.analog.right = (int16_t)atoi(ev[k].b);
            }
            ev[k].t = -1.0;
        }
        if (aiming && (m.mem[BZ_ATRACT] & 0x80)) {
            /* where the enemy is from the player: pivot towards him, fire when he is ahead */
            double a = (m.mem[BZ_TANGLE] << 8 | m.mem[BZ_LANGLE]) / 65536.0 * 6.283185307;
            double dx = (int16_t)((m.mem[BZ_TPOSX + 2] | m.mem[BZ_TPOSX + 3] << 8) - (m.mem[BZ_TPOSX] | m.mem[BZ_TPOSX + 1] << 8));
            double dy = (int16_t)((m.mem[BZ_TPOSY + 2] | m.mem[BZ_TPOSY + 3] << 8) - (m.mem[BZ_TPOSY] | m.mem[BZ_TPOSY + 1] << 8));
            double side = -dx * sin(a) + dy * cos(a), ahead = dx * cos(a) + dy * sin(a);
            double rel = atan2(side, ahead);
            levers = (uint8_t)(rel > 0.02 ? (BZ_IN3_LEFT_BACK | BZ_IN3_RIGHT_FWD) : rel < -0.02 ? (BZ_IN3_LEFT_FWD | BZ_IN3_RIGHT_BACK) : 0);
            if (fabs(rel) < 0.03 && m.mem[BZ_FIRECT] == 0 && (long)(t * 10) % 7 == 0) hold[2] = t + 0.1;
        }
        m.in.in0 = (uint8_t)(0x3f & ~(t < hold[0] ? BZ_IN0_COIN_R : 0));
        m.in.in3 = (uint8_t)(levers | (t < hold[1] ? BZ_IN3_START : 0) | (t < hold[2] ? BZ_IN3_FIRE : 0));

        if (original) {
            bz_emu_nmi(&emu);
        } else {
            uint32_t before = m.frames;
            bz_nmi(&m);
            if (pace) bz_pace_poll(&m, &pacer);
            else bz_main_poll(&m);
            enh.analog = m.analog.on;
            enh.lever_l = m.analog.left;
            enh.lever_r = m.analog.right;
            snd.enemy_engine = snd.ducking = (uint8_t)!plain;
            bz_sound_place(&snd, m.mem, !plain, !plain);
            if (m.frames != before) {
                bz_enhance_frame(&enh, t);
                bz_smooth_frame(&sm, &m, t);
                /* (the newest picture: the pending one, or the one shown) */
                const bz_smooth_pic *pic = sm.have_pending ? &sm.pending : &sm.shown;
                if (predicted.count) ahead_check(&ahead, &predicted, &pic->cur);
                bz_avg_list_reserve(&predicted, pic->next.count);
                memcpy(predicted.lines, pic->next.lines, (size_t)pic->next.count * sizeof(bz_line));
                predicted.count = pic->next.count;
            }
        }
        if (!original)
            bz_sound_tune(&snd, m.mem[BZ_POINT] | m.mem[BZ_POINT + 1] | m.mem[BZ_POINT + 2] | m.mem[BZ_POINT + 3]);
        bz_sound_run(&snd, BZ_CPU_CYCLES_PER_NMI);
        if (m.mem[BZ_FRAME] != last_frame) {    /* (MAIN counts its frames there) */
            last_frame = m.mem[BZ_FRAME];
            frames++;
        }
        if (wf) {
            int16_t buf[2048];
            int n;
            while ((n = bz_sound_read(&snd, buf, 1024)) > 0) {
                fwrite(buf, 4, (size_t)n, wf);
                wav_samples += (uint32_t)n;
            }
        } else {
            snd.ring_r = snd.ring_w;
        }
        if (!original && t >= next_even) {
            const bz_line *el;
            int en = bz_smooth_lines(&sm, next_even > 0.0 ? next_even : t, smooth, &el);   /* (the exact time, not the interrupt's) */
            even_check(&even, el, en);
            jerk_check(&jerk, el, en);
            {   /* the beam's way: lit and dark, in screen units (the board's are 7 times these) */
                float bx = 0.0f, by = 0.0f;
                for (int q = 0; q < en; q++) {
                    if (fabsf(el[q].x0 - bx) > 0.01f || fabsf(el[q].y0 - by) > 0.01f)
                        beam_dark += hypot(el[q].x0 - bx, el[q].y0 - by);
                    beam_lit += hypot(el[q].x1 - el[q].x0, el[q].y1 - el[q].y0);
                    bx = el[q].x1; by = el[q].y1;
                }
                beam_pictures++;
            }
            next_even += 1.0 / even_hz;
        }
        if (svg && next_shot > 0.0 && t >= next_shot) {
            char name[1024];
            snprintf(name, sizeof name, "%.*s-%03d.svg", (int)(strlen(svg) - 4), svg, ++shots);
            lines = shot(name, &m, &sm, original, t, smooth);
            next_shot += every;
        }
        if (svg && at >= 0.0 && !at_done && t >= at) {
            lines = shot(svg, &m, &sm, original, t, smooth);
            at_done = 1;
        }
    }
    if (svg && at < 0.0 && every <= 0.0) lines = shot(svg, &m, &sm, original, seconds, smooth);
    if (wf) {
        fseek(wf, 0, SEEK_SET);
        wav_header(wf, wav_samples);
        fclose(wf);
    }

    printf("bzshot: %s, %.1f s: %u interrupts, %u frames (%.2f a second), %u random numbers, %u math box starts\n",
           original ? "the original on the 6502 core" : "the C routines", seconds,
           (unsigned)(original ? emu.nmis : m.nmis), frames, frames / seconds, m.n_rnd, m.n_mbox);
    printf("  playing %s, score %02X%02X000, tanks %u, enemy's score %02X, credits %u, last picture %d lines\n",
           m.mem[BZ_ATRACT] ? "yes" : "no", m.mem[BZ_HITS + 1], m.mem[BZ_HITS], m.mem[BZ_LIVES], m.mem[BZ_HITS + 2],
           m.mem[BZ_CRDT], lines);
    printf("  the tank at %02X%02X, %02X%02X, heading %02X%02X\n", m.mem[BZ_TPOSX + 1], m.mem[BZ_TPOSX],
           m.mem[BZ_TPOSY + 1], m.mem[BZ_TPOSY], m.mem[BZ_TANGLE], m.mem[BZ_LANGLE]);
    if (!original && ahead.lines)
        printf("  pictures drawn ahead: %ld frames, %ld moving lines, %ld without a partner (%.2f %%), "
               "apart %.3f units a line on average, %.1f at most\n",
               ahead.frames, ahead.lines, ahead.unmatched, 100.0 * ahead.unmatched / ahead.lines,
               ahead.sum / 4.0 / (ahead.lines - ahead.unmatched + 1), ahead.worst / 4.0);
    if (!original && even.samples) {
        double mean = even.sum / even.samples, var = even.sum2 / even.samples - mean * mean;
        printf("  the land on a %.0f Hz screen while turning (%s): %ld pictures, %.2f units each on average, "
               "uneven by %.1f %% (standard deviation), %.1f %% of them more than 15 %% slow, %.1f %% fast; "
               "%ld stood nearly still (%.1f %%), %ld jumped (%.1f %%)\n", even_hz, smooth ? "smooth" : "not smooth",
               even.samples, mean, 100.0 * sqrt(var > 0.0 ? var : 0.0) / mean, 100.0 * even.slow / even.samples,
               100.0 * even.fast / even.samples, even.stalls, 100.0 * even.stalls / even.samples,
               even.jumps, 100.0 * even.jumps / even.samples);
    }
    for (int k = 0; k < BZ_SMP_COUNT; k++)
        if (snd.smp.sound[k].played) printf("  recording %s started %u times\n", bz_sample_name(k), (unsigned)snd.smp.sound[k].played);
    if (!original && jerk.n)
        printf("  single lines of the land and the objects on that screen: a step differs from the one before by "
               "%.1f %% of a step on average (%ld steps of %.2f units)\n",
               100.0 * jerk.change / jerk.step, jerk.n, jerk.step / jerk.n);
    if (!original && beam_pictures)
        printf("  the beam's way a picture: %.0f units lit, %.0f dark (screen units)\n",
               beam_lit / beam_pictures, beam_dark / beam_pictures);
    return 0;
}
