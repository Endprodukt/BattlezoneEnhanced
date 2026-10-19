#include "render/smooth.h"

#include "core/tags.h"
#include "game/game.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 16 interrupts of 246.09 a second. */
#define FRAME_PERIOD (16.0 * 6144.0 / 1512000.0)

/* A line that would have to move further than this between two frames is
 * not the same thing after all (a tank that was put somewhere else). */
#define MAX_MOVE 160.0f

typedef struct entry {
    uint64_t key;
    int      line;
    int      used;
} entry;

void bz_smooth_init(bz_smooth *s)
{
    memset(s, 0, sizeof *s);
    s->period = FRAME_PERIOD;
}

void bz_smooth_free(bz_smooth *s)
{
    bz_avg_list_free(&s->shown.cur);
    bz_avg_list_free(&s->shown.next);
    bz_avg_list_free(&s->pending.cur);
    bz_avg_list_free(&s->pending.next);
    bz_avg_list_free(&s->out);
    bz_avg_list_free(&s->raw);
    free(s->index);
    s->index = NULL;
    s->index_cap = 0;
}

/* The lines that move: the world. */
static int moves(const bz_line *l)
{
    switch (BZ_TAG_KIND(l->tag)) {
    case BZ_TAG_MOUNT: case BZ_TAG_VOLCANO: case BZ_TAG_OBJECT: case BZ_TAG_SWEEP: case BZ_TAG_BLIP:
        return 1;
    default:
        return 0;
    }
}

/* What a line is: its tag, and which vector under it. Lines a vector ROM
 * routine draws (a section of the mountains, the explosion's picture) are
 * known by the routine and their place in it, wherever in the list it is
 * called from. */
static uint64_t key_of(const bz_line *l)
{
    return ((uint64_t)l->tag << 32) | ((uint64_t)l->sub << 16) | (l->sub ? l->n : l->ord);
}

static int entry_cmp(const void *a, const void *b)
{
    uint64_t x = ((const entry *)a)->key, y = ((const entry *)b)->key;
    return x < y ? -1 : x > y;
}

void bz_smooth_frame(bz_smooth *s, const bz_machine *m, double now)
{
    bz_smooth_pic *p;
    if (!s->have_shown) {
        p = &s->shown;
        p->start = now;
        s->have_shown = 1;
    } else {
        /* The frames' pace: what the last six took, on average (the 18,
         * 18, 12 make 16). A pause (the test menu) is no pace. */
        double took = now - s->arrived;
        if (took > 0.0 && took < 4.0 * FRAME_PERIOD) {
            s->took[s->ntook++ % 6] = took;
            int n = s->ntook < 6 ? s->ntook : 6;
            double sum = 0.0;
            for (int i = 0; i < n; i++) sum += s->took[i];
            s->period = sum / n;
        } else {
            s->ntook = 0;
            s->period = FRAME_PERIOD;
        }
        if (s->have_pending) {
            /* (two frames before the first could begin: the older gives way) */
            bz_smooth_pic t = s->shown;
            s->shown = s->pending;
            s->pending = t;
            if (s->shown.start > now) s->shown.start = now;
        }
        p = &s->pending;
        /* A period after the last, so that a picture is done becoming
         * the next exactly when the next begins: with the frames 18, 18
         * and 12 interrupts apart the pictures then begin 16 apart, up to
         * 4 behind their frames, and none has to wait. Only if they fall
         * further behind than that (the pace has changed) is it made up,
         * a little at a time. */
        p->start = s->shown.start + s->period;
        if (p->start < now) p->start = now;
        else if (p->start - now > 0.45 * s->period) p->start -= 0.03 * s->period;
        s->have_pending = 1;
    }
    p->dur = s->period;
    s->arrived = now;

    /* The game before this frame and after it. */
    p->have_ab = s->have_last;
    if (s->have_last) p->a = s->last;
    p->b = *m;
    s->last = *m;
    s->have_last = 1;

    /* The picture the frame has just written: VGLIST stands behind its end. */
    uint8_t buffer = m->mem[BZ_VGLIST + 1] & 0xf8;
    if (s->exact) bz_avg_exact(m->vgqx, m->vgqy);
    bz_avg_run_raw(m->mem, (uint16_t)((buffer << 8) | 2), m->vgtag, m->vgnote, &p->cur, 0);

    /* The next frame's, on a copy, into the other buffer. */
    s->scratch = *m;
    s->scratch.prng = NULL;             /* (its own random numbers: a rock of the volcano may start differently) */
    s->scratch.read_hook = NULL;
    s->scratch.sound_write = NULL;
    s->scratch.sound_request = NULL;
    buffer ^= 0x08;
    bz_picture(&s->scratch, buffer);
    if (s->exact) bz_avg_exact(s->scratch.vgqx, s->scratch.vgqy);
    bz_avg_run_raw(s->scratch.mem, (uint16_t)((buffer << 8) | 2), s->scratch.vgtag, s->scratch.vgnote, &p->next, 0);
    bz_avg_exact(NULL, NULL);
    s->frames++;

    /* For the ground's grid: the frame's picture shows the player where the
     * frame before left him, the next one where he is now. */
    bz_pose now_at = { (float)(m->mem[BZ_TPOSX] | m->mem[BZ_TPOSX + 1] << 8),
                       (float)(m->mem[BZ_TPOSY] | m->mem[BZ_TPOSY + 1] << 8),
                       (float)(m->mem[BZ_TANGLE] << 8 | m->mem[BZ_LANGLE]), (float)m->mem[BZ_BOUNCE] };
    p->pose0 = s->have_pose ? s->pose : now_at;
    p->pose1 = now_at;
    s->pose = now_at;
    s->have_pose = 1;
    p->land = 0;
    for (int i = 0; i < p->cur.count && !p->land; i++)
        if (BZ_TAG_KIND(p->cur.lines[i].tag) == BZ_TAG_MOUNT) {
            p->land = 1;
            memcpy(p->win, p->cur.lines[i].win, sizeof p->win);
        }
}

static int finish(bz_smooth *s, const bz_line **lines);

/* --- the picture of a moment, computed -------------------------------------------- */

static unsigned rd16(const bz_machine *m, unsigned at) { return m->mem[at] | (unsigned)m->mem[at + 1] << 8; }

/* A 16-bit value of the copy on its way from a's to b's (it wraps round).
 * One that jumps (something was put somewhere else) stays a's. */
static void between16(bz_machine *s, const bz_machine *a, const bz_machine *b, unsigned at, float t, int most)
{
    int d = (int16_t)(rd16(b, at) - rd16(a, at));
    if (d > most || d < -most) return;
    unsigned v = (rd16(a, at) + (unsigned)(int)lroundf((float)d * t)) & 0xffff;
    s->mem[at] = (uint8_t)v;
    s->mem[at + 1] = (uint8_t)(v >> 8);
}

/* The same for a value kept as a high and a low byte apart (an angle:
 * TANGLE, LANGLE; a rock's place). */
static void between_hl(bz_machine *s, const bz_machine *a, const bz_machine *b, unsigned hi, unsigned lo, float t, int most)
{
    unsigned va = (unsigned)(a->mem[hi] << 8 | a->mem[lo]), vb = (unsigned)(b->mem[hi] << 8 | b->mem[lo]);
    int d = (int16_t)(vb - va);
    if (d > most || d < -most) return;
    unsigned v = (va + (unsigned)(int)lroundf((float)d * t)) & 0xffff;
    s->mem[hi] = (uint8_t)(v >> 8);
    s->mem[lo] = (uint8_t)v;
}

static void between8(bz_machine *s, const bz_machine *a, const bz_machine *b, unsigned at, float t)
{
    int d = (int8_t)(b->mem[at] - a->mem[at]);
    s->mem[at] = (uint8_t)(a->mem[at] + (int)lroundf((float)d * t));
}

/* An angle the game keeps in 8 bits: its whole part into the copy, what
 * is below that returned (bz_machine.antenna_lo and the like). */
static uint8_t between_angle(bz_machine *s, const bz_machine *a, const bz_machine *b, unsigned at, float t)
{
    int d = (int8_t)(b->mem[at] - a->mem[at]);
    unsigned v = ((unsigned)a->mem[at] << 8) + (unsigned)(int)lroundf((float)d * 256.0f * t);
    s->mem[at] = (uint8_t)(v >> 8);
    return (uint8_t)v;
}

/* The picture t of the way (0..1) from the shown frame's state to the
 * next one's, into s->raw. */
static void compute(bz_smooth *s, float t)
{
    const bz_machine *a = &s->shown.a, *b = &s->shown.b;
    bz_machine *c = &s->scratch;
    *c = *a;
    c->prng = NULL;                     /* (its own random numbers: a rock of the volcano may start differently) */
    c->read_hook = NULL;
    c->sound_write = NULL;
    c->sound_request = NULL;
    c->fine = 1;
    if (t > 0.0f) {
        for (unsigned who = 0; who <= 2; who += 2) {    /* the player, the enemy: place and heading */
            between16(c, a, b, BZ_TPOSX + who, t, 0x1800);
            between16(c, a, b, BZ_TPOSY + who, t, 0x1800);
            between_hl(c, a, b, BZ_TANGLE + who, BZ_LANGLE + who, t, 0x1000);
            /* the shells, while one is in flight in both */
            if (a->mem[BZ_FIRECT + who] && b->mem[BZ_FIRECT + who] &&
                !((a->mem[BZ_FIRECT + who] | b->mem[BZ_FIRECT + who]) & 0x80)) {
                between16(c, a, b, BZ_SHELLX + who, t, 0x1800);
                between16(c, a, b, BZ_SHELLY + who, t, 0x1800);
            }
            /* one that has come down: its burst grows with FIRECT */
            if (a->mem[BZ_FIRECT + who] & b->mem[BZ_FIRECT + who] & 0x80) between8(c, a, b, BZ_FIRECT + who, t);
        }
        between16(c, a, b, BZ_SAPOSX, t, 0x1800);       /* the saucer */
        between16(c, a, b, BZ_SAPOSX + 2, t, 0x1800);
        c->saucer_lo = between_angle(c, a, b, BZ_SCANGL, t);
        c->antenna_lo = between_angle(c, a, b, BZ_RANGLE, t);
        /* The pieces of a wreck: where they are, how they are turned and
         * how fast they rise (EXPLDE moves them a step as it draws: the
         * picture is of this state a step on, at every time alike). */
        for (unsigned i = 0; i < 12; i += 2) {
            if ((a->mem[BZ_EXPOSZ + 1 + i] | b->mem[BZ_EXPOSZ + 1 + i]) & 0x80) continue;      /* (it has come down) */
            between16(c, a, b, BZ_EXPOSX + i, t, 0x1800);
            between16(c, a, b, BZ_EXPOSY + i, t, 0x1800);
            between16(c, a, b, BZ_EXPOSZ + i, t, 0x1800);
            between8(c, a, b, BZ_ZVELOC + i, t);
            c->piece_lo[i >> 1] = between_angle(c, a, b, BZ_EXPOSO + i, t);
        }
        /* (the missile's height over the ground and the logo's are kept with the pieces') */
        between16(c, a, b, BZ_EXPOSZ + 12, t, 0x1800);
        between16(c, a, b, BZ_EXPOSZ + 14, t, 0x1800);
        for (unsigned i = 0; i < BZ_NOROCK; i++) {      /* the volcano's rocks */
            if (!a->mem[BZ_OBJTIM + i] || !b->mem[BZ_OBJTIM + i]) continue;
            between_hl(c, a, b, BZ_XPOSH + i, BZ_XPOSL + i, t, 0x1000);
            between_hl(c, a, b, BZ_YPOSH + i, BZ_YPOSL + i, t, 0x1000);
        }
        between8(c, a, b, BZ_SANGLE, t);                /* the radar's sweep */
        between8(c, a, b, BZ_BOUNCE, t);
        between16(c, a, b, BZ_LOGOX, t, 0x1800);        /* the logo on its way in */
        between16(c, a, b, BZ_LOGOZ, t, 0x1800);
    }
    uint8_t buffer = (uint8_t)((a->mem[BZ_VGLIST + 1] & 0xf8) ^ 0x08);
    bz_picture(c, buffer);
    if (s->exact) bz_avg_exact(c->vgqx, c->vgqy);
    bz_avg_run_raw(c->mem, (uint16_t)((buffer << 8) | 2), c->vgtag, c->vgnote, &s->raw, 0);
    bz_avg_exact(NULL, NULL);
    /* (the mountains stand on whole numbers: the rest of their way) */
    if (c->mount_shift != 0.0f)
        for (int i = 0; i < s->raw.count; i++) {
            unsigned kind = BZ_TAG_KIND(s->raw.lines[i].tag);
            if (kind != BZ_TAG_MOUNT && kind != BZ_TAG_VOLCANO) continue;
            s->raw.lines[i].x0 += c->mount_shift;
            s->raw.lines[i].x1 += c->mount_shift;
        }
}

static void put(bz_avg_list *out, const bz_line *l)
{
    if (bz_avg_list_reserve(out, out->count + 1) != 0) return;
    out->lines[out->count++] = *l;
}

/* The whole lines of the picture in s->raw: cut where something covers
 * them, then to the window. */
static int finish(bz_smooth *s, const bz_line **lines)
{
    /* What the renderer adds to the game's lines: the far mountains, the
     * stars, the sparks, the grid (behind the game's skyline and the moon,
     * which are taken from the lines as they are now). */
    if (s->shown.land) {
        if (s->enh) bz_enhance_scene(s->enh, s->raw.lines, s->raw.count);
        if (s->grid) bz_ground_lines(s->at, s->shown.win, &s->raw);
        if (s->enh) bz_enhance_world(s->enh, s->at, s->at_time, s->shown.win, &s->raw);   /* (its stars last) */
    }
    const bz_line *l = s->raw.lines;
    int n = s->raw.count;
    if (s->solid) n = bz_solid_run(s->solid, l, n, &l);
    s->out.count = 0;
    if (bz_avg_list_reserve(&s->out, n) != 0) return 0;
    for (int i = 0; i < n; i++) {
        bz_line c = l[i];
        if (bz_line_clip(&c)) s->out.lines[s->out.count++] = c;
    }
    if (s->enh && s->shown.land) bz_enhance_overlay(s->enh, s->at, s->at_time, s->shown.win, &s->out);
    if (s->order == 1) bz_order_lines(s->out.lines, s->out.count);
    else if (s->order == 2) bz_order_steady(s->out.lines, s->out.count);
    *lines = s->out.lines;
    return s->out.count;
}

static float lerp(float a, float b, float f) { return a + (b - a) * f; }

int bz_smooth_lines(bz_smooth *s, double now, int smooth, const bz_line **lines)
{
    *lines = NULL;
    s->moved = s->popped = 0;
    if (!s->have_shown) return 0;
    if (s->have_pending && now >= s->pending.start) {
        bz_smooth_pic t = s->shown;
        s->shown = s->pending;
        s->pending = t;
        s->have_pending = 0;
    }
    const bz_avg_list *cur = &s->shown.cur, *next = &s->shown.next;
    float f = (float)((now - s->shown.start) / s->shown.dur);
    if (f < 0.0f || !smooth) f = 0.0f;
    s->shown_count++;
    if (f > 1.02f) s->held_count++;
    if (f > 1.0f) f = 1.0f;
    s->last_f = f;

    s->raw.count = 0;
    s->at = bz_pose_between(s->shown.pose0, s->shown.pose1, f);
    s->at_time = now;
    if (smooth && s->computed && s->shown.have_ab) {
        compute(s, f);
        return finish(s, lines);
    }
    if (f == 0.0f) {
        for (int i = 0; i < cur->count; i++) put(&s->raw, &cur->lines[i]);
        return finish(s, lines);
    }

    /* The next picture's moving lines, by what they are. */
    if (s->index_cap < next->count) {
        free(s->index);
        s->index = malloc((size_t)(next->count + 64) * sizeof(entry));
        s->index_cap = s->index ? next->count + 64 : 0;
        if (!s->index) return 0;
    }
    entry *ix = (entry *)s->index;
    int n = 0;
    for (int i = 0; i < next->count; i++) {
        if (!moves(&next->lines[i])) continue;
        ix[n].key = key_of(&next->lines[i]);
        ix[n].line = i;
        ix[n].used = 0;
        n++;
    }
    qsort(ix, (size_t)n, sizeof *ix, entry_cmp);

    for (int i = 0; i < cur->count; i++) {
        const bz_line *a = &cur->lines[i];
        if (!moves(a)) { put(&s->raw, a); continue; }
        entry want, *e;
        want.key = key_of(a);
        e = n ? (entry *)bsearch(&want, ix, (size_t)n, sizeof *ix, entry_cmp) : NULL;
        const bz_line *b = e ? &next->lines[e->line] : NULL;
        if (b && (fabsf(b->x0 - a->x0) > MAX_MOVE || fabsf(b->y0 - a->y0) > MAX_MOVE ||
                  fabsf(b->x1 - a->x1) > MAX_MOVE || fabsf(b->y1 - a->y1) > MAX_MOVE))
            b = NULL;
        if (!b) {                               /* in this picture only: goes halfway */
            s->popped++;
            if (f < 0.5f) put(&s->raw, a);
            continue;
        }
        e->used = 1;
        bz_line l = *a;
        if (BZ_TAG_KIND(a->tag) == BZ_TAG_SWEEP) {
            /* The radar's sweep turns about its start. */
            float a0 = atan2f(a->y1 - a->y0, a->x1 - a->x0), a1 = atan2f(b->y1 - b->y0, b->x1 - b->x0);
            float r0 = hypotf(a->x1 - a->x0, a->y1 - a->y0), r1 = hypotf(b->x1 - b->x0, b->y1 - b->y0);
            float d = a1 - a0;
            while (d > 3.14159265f) d -= 6.2831853f;
            while (d < -3.14159265f) d += 6.2831853f;
            float ang = a0 + d * f, r = lerp(r0, r1, f);
            l.x0 = lerp(a->x0, b->x0, f); l.y0 = lerp(a->y0, b->y0, f);
            l.x1 = l.x0 + cosf(ang) * r; l.y1 = l.y0 + sinf(ang) * r;
        } else {
            l.x0 = lerp(a->x0, b->x0, f); l.y0 = lerp(a->y0, b->y0, f);
            l.x1 = lerp(a->x1, b->x1, f); l.y1 = lerp(a->y1, b->y1, f);
        }
        if (a->w0 > 0.0f && b->w0 > 0.0f) l.w0 = lerp(a->w0, b->w0, f);
        if (a->w1 > 0.0f && b->w1 > 0.0f) l.w1 = lerp(a->w1, b->w1, f);
        s->moved++;
        put(&s->raw, &l);
    }
    /* In the next picture only: comes halfway. */
    for (int k = 0; k < n; k++) {
        if (ix[k].used) continue;
        s->popped++;
        if (f >= 0.5f) put(&s->raw, &next->lines[ix[k].line]);
    }
    return finish(s, lines);
}

/* --- the beam's way ------------------------------------------------------------------ */

typedef struct stroke { int first, n; float x0, y0, x1, y1; uint8_t used, star; } stroke;

/* What the order may change: the world. (Everything else keeps the game's.) */
static int movable(const bz_line *l)
{
    unsigned kind = BZ_TAG_KIND(l->tag), key = BZ_TAG_KEY(l->tag);
    if (kind == BZ_TAG_FX) return key == BZ_FX_STAR || key == BZ_FX_DUST || key == BZ_FX_FAR || key == BZ_FX_BOOM;
    return kind == BZ_TAG_MOUNT || kind == BZ_TAG_VOLCANO || kind == BZ_TAG_OBJECT || kind == BZ_TAG_GROUND;
}

static int joins(const bz_line *a, const bz_line *b)
{
    return fabsf(a->x1 - b->x0) < 0.01f && fabsf(a->y1 - b->y0) < 0.01f;
}

void bz_order_lines(bz_line *lines, int n)
{
    static stroke *st;
    static bz_line *tmp;
    static int cap;
    if (n < 3) return;
    if (n > cap) {
        stroke *a = (stroke *)realloc(st, (size_t)n * sizeof *a);
        bz_line *b = (bz_line *)realloc(tmp, (size_t)n * sizeof *b);
        if (!a || !b) { free(a); free(b); st = NULL; tmp = NULL; cap = 0; return; }
        st = a; tmp = b; cap = n;
    }
    /* first what keeps its order: as it is */
    int out = 0;
    float bx = 0.0f, by = 0.0f;
    for (int i = 0; i < n; i++)
        if (!movable(&lines[i])) {
            tmp[out++] = lines[i];
            bx = lines[i].x1;
            by = lines[i].y1;
        }
    /* the strokes of the world */
    int ns = 0;
    for (int i = 0; i < n; ) {
        if (!movable(&lines[i])) { i++; continue; }
        int j = i + 1;
        while (j < n && movable(&lines[j]) && joins(&lines[j - 1], &lines[j])) j++;
        stroke *k = &st[ns++];
        k->first = i; k->n = j - i;
        k->x0 = lines[i].x0; k->y0 = lines[i].y0; k->x1 = lines[j - 1].x1; k->y1 = lines[j - 1].y1;
        k->used = 0;
        k->star = (uint8_t)(k->n == 1 && BZ_TAG_KIND(lines[i].tag) == BZ_TAG_FX && BZ_TAG_KEY(lines[i].tag) == BZ_FX_STAR);
        i = j;
    }
    /* nearest first, from where the rest left the beam, the stars after
     * everything else */
    for (int pass = 0; pass < 2; pass++) {
        for (;;) {
            int best = -1, rev = 0;
            float bd = 1e30f;
            for (int i = 0; i < ns; i++) {
                if (st[i].used || st[i].star != pass) continue;
                float d0 = fabsf(st[i].x0 - bx) + fabsf(st[i].y0 - by);
                float d1 = fabsf(st[i].x1 - bx) + fabsf(st[i].y1 - by);
                if (d0 < bd) { bd = d0; best = i; rev = 0; }
                if (d1 < bd) { bd = d1; best = i; rev = 1; }
            }
            if (best < 0) break;
            stroke *k = &st[best];
            k->used = 1;
            for (int q = 0; q < k->n; q++) {
                bz_line l = lines[rev ? k->first + k->n - 1 - q : k->first + q];
                if (rev) {
                    float t;
                    t = l.x0; l.x0 = l.x1; l.x1 = t;
                    t = l.y0; l.y0 = l.y1; l.y1 = t;
                    t = l.w0; l.w0 = l.w1; l.w1 = t;
                    uint8_t p = l.p0; l.p0 = l.p1; l.p1 = p;
                }
                tmp[out++] = l;
            }
            bx = rev ? k->x0 : k->x1;
            by = rev ? k->y0 : k->y1;
        }
    }
    memcpy(lines, tmp, (size_t)n * sizeof *lines);
}

/* 0: what does not move (texts, score, the radar's ring, the sights);
 * 1: what moves over it (the sweep, the blip); 2: the world. */
static int steady_part(const bz_line *l)
{
    unsigned kind = BZ_TAG_KIND(l->tag);
    if (movable(l)) return 2;
    if (kind == BZ_TAG_SWEEP || kind == BZ_TAG_BLIP) return 1;
    if (kind == BZ_TAG_FX) return 1;            /* (the flash, the levers' gauges) */
    return 0;
}

void bz_order_steady(bz_line *lines, int n)
{
    static bz_line *tmp;
    static int cap;
    if (n < 2) return;
    if (n > cap) {
        bz_line *b = (bz_line *)realloc(tmp, (size_t)n * sizeof *b);
        if (!b) return;
        tmp = b;
        cap = n;
    }
    int out = 0;
    for (int part = 0; part < 3; part++)
        for (int i = 0; i < n; i++)
            if (steady_part(&lines[i]) == part) tmp[out++] = lines[i];
    memcpy(lines, tmp, (size_t)n * sizeof *lines);
}
