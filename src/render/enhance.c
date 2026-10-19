/* enhance.c - see enhance.h. */
#include "render/enhance.h"

#include "core/bz_symbols.h"
#include "core/tags.h"
#include "render/color.h"

#include <math.h>
#include <string.h>

#define TWO_PI      6.2831853f
#define SCREEN      509.5f      /* as render/ground.c */
#define N_STARS     100         /* (150 until 0.1.19, over less of the screen a turn: as many to see) */
#define VIEW_HALF   0.52f       /* the view to each side, as an angle: a little past the screen's edge */
#define MTN0        0x3016      /* the vector ROM's first section of the mountains, the moon after its 16th vector */
/* How much of a turn the far mountains make: half as fast across the
 * screen as the game's own row, which moves 0.996 units every 32 angle
 * steps (2040 units a turn; render/smooth.c, mount_shift), where a true
 * view would move 509.5 * 2 pi / 65536 = 0.0488 units a step. (0.6 until
 * 0.1.16: 0.029 units a step, all but as fast as the first row, and no
 * parallax was seen.) */
#define MOUNT_RATE  (0.996f / 32.0f)     /* the game's own row and its moon: screen units an angle step */
#define PARALLAX    (0.5f * MOUNT_RATE / (509.5f * 6.2831853f / 65536.0f))
#define FAR_POINTS  52          /* the far row's profile: its corners (as far apart on the screen as with 96 at 0.6) */
#define FAR_PERIOD  (65536.0f * PARALLAX)

static const float EVERYWHERE[4] = { -1e6f, -1e6f, 1e6f, 1e6f };

static unsigned w16(const uint8_t *mem, unsigned a) { return mem[a] | (unsigned)mem[a + 1] << 8; }

static float wrap(float v, float period)
{
    v = fmodf(v, period);
    if (v >= period * 0.5f) v -= period;
    if (v < -period * 0.5f) v += period;
    return v;
}

/* Numbers that look random and are the same every time. */
static uint32_t hash(uint32_t n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    return n ^ (n >> 15);
}

static float frand(uint32_t n) { return (float)(hash(n) & 0xffff) / 65535.0f; }

static bz_line *line(bz_avg_list *out, float x0, float y0, float x1, float y1, float w, int intensity, unsigned key,
                     const float win[4])
{
    if (intensity < 1) return NULL;
    if (bz_avg_list_reserve(out, out->count + 1) != 0) return NULL;
    bz_line *l = &out->lines[out->count++];
    memset(l, 0, sizeof *l);
    l->x0 = x0; l->y0 = y0; l->x1 = x1; l->y1 = y1;
    l->w0 = l->w1 = w;
    l->intensity = (uint8_t)(intensity > 15 ? 15 : intensity);
    l->z = 1;
    l->tag = BZ_TAG(BZ_TAG_FX, key);
    memcpy(l->win, win, sizeof l->win);
    return l;
}

void bz_enhance_init(bz_enhance *e, const bz_machine *m)
{
    memset(e, 0, sizeof *e);
    e->m = m;
    e->explode_at = e->fire_at = -1.0;
}

/* --- what happened in a frame ------------------------------------------------------ */

/* Bits that fly from x, y, z: dust (what 0), or sparks of what was hit. */
static void burst(bz_enhance *e, float x, float y, float z, int count, float speed, uint8_t what, double now)
{
    for (int k = 0; k < count; k++) {
        int i = e->n_bit++ % BZ_ENH_BITS;
        uint32_t seed = (uint32_t)e->n_bit * 7919u;
        float a = frand(seed) * TWO_PI, v = speed * (0.25f + 0.75f * frand(seed + 1));
        float up = what ? (0.3f + 1.4f * frand(seed + 2)) : (1.0f + 1.8f * frand(seed + 2));
        e->bit[i].x = x; e->bit[i].y = y; e->bit[i].z = z;
        e->bit[i].vx = cosf(a) * v; e->bit[i].vy = sinf(a) * v;
        e->bit[i].vz = speed * up * 0.5f;
        e->bit[i].born = now;
        e->bit[i].life = (what ? 0.7f : 0.5f) + (what ? 0.9f : 0.5f) * frand(seed + 3);
        e->bit[i].what = what;
    }
}

void bz_enhance_frame(bz_enhance *e, double now)
{
    const uint8_t *mem = e->m->mem;
    e->now = now;
    int playing = (mem[BZ_ATRACT] & 0x80) != 0;

    /* The enemy begins to fly apart: sparks of his color. */
    uint8_t exploding = mem[BZ_COLFLG + 2];
    if (exploding && !e->was_exploding) {
        e->explode_at = now;
        e->boom_id = e->m->enemy_id ? e->m->enemy_id : BZ_ID_TANK;
        burst(e, (float)w16(mem, BZ_TPOSX + 2), (float)w16(mem, BZ_TPOSY + 2), 220.0f, 56, 2600.0f, e->boom_id, now);
    }
    e->was_exploding = exploding;

    /* The saucer is hit: it dissolves, with sparks. */
    uint8_t saucer_hit = mem[BZ_SCOLFG];
    if (saucer_hit && !e->was_saucer_hit)
        burst(e, (float)w16(mem, BZ_SAPOSX), (float)w16(mem, BZ_SAPOSX + 2), 700.0f, 40, 1800.0f, BZ_ID_SAUCER, now);
    e->was_saucer_hit = saucer_hit;

    /* The shells (0 the player's, 2 the enemy's): FIRECT is 7F as one is
     * fired and has its top bit set when it has come down. */
    for (int i = 0; i < 3; i += 2) {
        uint8_t f = mem[BZ_FIRECT + i];
        int flying = f != 0 && !(f & 0x80), flew = e->was_firect[i] != 0 && !(e->was_firect[i] & 0x80);
        if (i == 0 && flying && !flew && playing) e->fire_at = now;
        if ((f & 0x80) && !(e->was_firect[i] & 0x80))
            burst(e, (float)w16(mem, BZ_SHELLX + i), (float)w16(mem, BZ_SHELLY + i), 60.0f, 10, 650.0f, 0, now);
        e->was_firect[i] = f;
    }
}

/* --- the scene --------------------------------------------------------------------- */

void bz_enhance_scene(bz_enhance *e, const bz_line *lines, int n)
{
    for (int i = 0; i < BZ_ENH_SKY; i++) e->sky[i] = 0.0f;
    float mx0 = 1e9f, my0 = 1e9f, mx1 = -1e9f, my1 = -1e9f;
    e->have_moon = 0;
    for (int i = 0; i < n; i++) {
        const bz_line *l = &lines[i];
        unsigned kind = BZ_TAG_KIND(l->tag);
        if (kind == BZ_TAG_MOUNT && l->sub == MTN0 && l->n >= 16) {    /* the moon */
            if (l->x0 < mx0) mx0 = l->x0;
            if (l->x1 < mx0) mx0 = l->x1;
            if (l->x0 > mx1) mx1 = l->x0;
            if (l->x1 > mx1) mx1 = l->x1;
            if (l->y0 < my0) my0 = l->y0;
            if (l->y1 < my0) my0 = l->y1;
            if (l->y0 > my1) my1 = l->y0;
            if (l->y1 > my1) my1 = l->y1;
            e->have_moon = 1;
            continue;
        }
        if (!(kind == BZ_TAG_MOUNT || (kind == BZ_TAG_VOLCANO && BZ_TAG_KEY(l->tag) == 0))) continue;
        /* the line's height at every column it crosses */
        float xa = l->x0, ya = l->y0, xb = l->x1, yb = l->y1;
        if (xa > xb) { float t = xa; xa = xb; xb = t; t = ya; ya = yb; yb = t; }
        int ca = (int)floorf(xa) + 256, cb = (int)ceilf(xb) + 256;
        if (ca < 0) ca = 0;
        if (cb > BZ_ENH_SKY - 1) cb = BZ_ENH_SKY - 1;
        for (int c = ca; c <= cb; c++) {
            float x = (float)(c - 256), y;
            if (xb - xa < 1e-3f) y = ya > yb ? ya : yb;
            else {
                float t = (x - xa) / (xb - xa);
                t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
                y = ya + (yb - ya) * t;
            }
            if (y > e->sky[c]) e->sky[c] = y;
        }
    }
    if (e->have_moon) {
        e->moon_x = 0.5f * (mx0 + mx1);
        e->moon_y = 0.5f * (my0 + my1);
        e->moon_r = 0.5f * (mx1 - mx0 > my1 - my0 ? mx1 - mx0 : my1 - my0) + 2.0f;
    }
}

static float sky_at(const bz_enhance *e, float x)
{
    int c = (int)lroundf(x) + 256;
    if (c < 0) c = 0;
    if (c > BZ_ENH_SKY - 1) c = BZ_ENH_SKY - 1;
    return e->sky[c];
}

/* A line of the far mountains, drawn where it stands over the first row's
 * skyline. */
static void over_skyline(const bz_enhance *e, float x0, float y0, float x1, float y1, int intensity, float gain,
                         const float win[4], bz_avg_list *out)
{
    if (x0 > x1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (x1 < -258.0f || x0 > 258.0f) return;
    float len = x1 - x0;
    int steps = (int)(len / 2.0f) + 1;
    float run_x = 0.0f, run_y = 0.0f;
    int in_run = 0;
    for (int i = 0; i <= steps; i++) {
        float x = x0 + len * (float)i / (float)steps, y = y0 + (y1 - y0) * (float)i / (float)steps;
        int above = y > sky_at(e, x) + 0.6f;
        if (above && !in_run) { run_x = x; run_y = y; in_run = 1; }
        if (!above && in_run) {
            bz_line *l = line(out, run_x, run_y, x, y, 0.0f, intensity, BZ_FX_FAR, win);
            if (l) l->gain = gain;
            in_run = 0;
        }
    }
    if (in_run) {
        bz_line *l = line(out, run_x, run_y, x1, y1, 0.0f, intensity, BZ_FX_FAR, win);
        if (l) l->gain = gain;
    }
}

/* The far row's profile: corner i's place along its circle (0 ..
 * FAR_PERIOD, and on round the circle for i outside 0 .. FAR_POINTS) and
 * its height over the horizon. Ranges of hills with a few tall peaks. */
static void far_corner(int i, float *u, float *h)
{
    int turn = (int)floorf((float)i / (float)FAR_POINTS);
    int k = i - turn * FAR_POINTS;
    *u = ((float)k + 0.35f * frand((uint32_t)k * 5u + 101u)) * FAR_PERIOD / (float)FAR_POINTS + (float)turn * FAR_PERIOD;
    float base = 10.0f + 22.0f * frand((uint32_t)k * 5u + 102u);
    if (k % 2) base *= 0.45f;                           /* every other corner a valley */
    if (hash((uint32_t)k * 5u + 103u) % 9u == 0) base += 24.0f;     /* a peak now and then */
    *h = base;
}

/* --- in the world ------------------------------------------------------------------ */

void bz_enhance_world(const bz_enhance *e, bz_pose pose, double now, const float win[4], bz_avg_list *out)
{
    if (e->on.far) {
        /* Where the view is on the far row's own circle, which turns PARALLAX
         * as fast as the view; the row is periodic over that circle, so a
         * full turn of the view is seamless. */
        float here = fmodf(pose.angle * PARALLAX, FAR_PERIOD);
        if (here < 0.0f) here += FAR_PERIOD;
        float span = VIEW_HALF / TWO_PI * 65536.0f;         /* (the view's half width in angle units) */
        int first = (int)floorf((here - span) / FAR_PERIOD * FAR_POINTS) - 1;
        int last = (int)ceilf((here + span) / FAR_PERIOD * FAR_POINTS) + 1;
        float px = 0.0f, py = 0.0f;
        int have = 0;
        for (int i = first; i <= last; i++) {
            float u, h;
            far_corner(i, &u, &h);
            float rel = (u - here) / 65536.0f * TWO_PI;
            if (fabsf(rel) > VIEW_HALF + 0.1f) { have = 0; continue; }
            float x = -SCREEN * tanf(rel), y = h;
            if (have) over_skyline(e, px, py, x, y, 6, 0.55f, win, out);
            px = x; py = y; have = 1;
        }
    }
    if (e->on.flash || e->on.particles) {
        /* Dust and sparks: each flies up and falls, drawn as a short
         * streak, fading. */
        int n = e->n_bit < BZ_ENH_BITS ? e->n_bit : BZ_ENH_BITS;
        for (int i = 0; i < n; i++) {
            if (e->bit[i].what ? !e->on.particles : !e->on.flash) continue;
            float t = (float)(now - e->bit[i].born);
            if (t < 0.0f || t >= e->bit[i].life) continue;
            float pos[2][3];
            for (int k = 0; k < 2; k++) {
                float u = t - 0.04f * (float)k;
                if (u < 0.0f) u = 0.0f;
                pos[k][0] = e->bit[i].x + e->bit[i].vx * u;
                pos[k][1] = e->bit[i].y + e->bit[i].vy * u;
                pos[k][2] = e->bit[i].z + e->bit[i].vz * u - 1600.0f * u * u;
                if (pos[k][2] < 0.0f) pos[k][2] = 0.0f;
            }
            float x0, y0, x1, y1, d0, d1, side;
            if (!bz_ground_point(pose, pos[0][0], pos[0][1], pos[0][2], &x0, &y0, &d0, &side)) continue;
            if (!bz_ground_point(pose, pos[1][0], pos[1][1], pos[1][2], &x1, &y1, &d1, &side)) continue;
            if (d0 > 30000.0f) continue;
            float left = 1.0f - t / e->bit[i].life;
            bz_line *l = line(out, x0, y0, x1, y1, 1.0f / d0, 2 + (int)(13.0f * left),
                              e->bit[i].what ? BZ_FX_BOOM : BZ_FX_DUST, win);
            if (l) l->model = e->bit[i].what;
        }
    }
    if (e->on.stars) {
        int first_star = out->count;
        /* Each star has its place around the horizon and its height over
         * it, and twinkles at its own pace. The stars are as far as the
         * moon: they turn with it, as fast as the game's mountains go
         * (MOUNT_RATE; until 0.1.19 as a true view would turn, half as
         * fast again as the moon beside them). */
        for (uint32_t i = 0; i < N_STARS; i++) {
            float az = frand(i * 3u + 11u) * 65536.0f;
            float x = -wrap(az - pose.angle, 65536.0f) * MOUNT_RATE;
            if (fabsf(x) > 262.0f) continue;
            float y = 52.0f + 130.0f * frand(i * 3u + 12u);
            if (y < sky_at(e, x) + 3.0f) continue;
            if (e->have_moon && hypotf(x - e->moon_x, y - e->moon_y) < e->moon_r) continue;
            float pace = 0.8f + 2.2f * frand(i * 3u + 14u), phase = frand(i * 3u + 15u) * TWO_PI;
            float tw = 0.5f + 0.5f * sinf((float)now * pace * TWO_PI + phase);
            int base = 2 + (int)(hash(i * 3u + 13u) % 6u);
            int inten = base + (int)(4.0f * tw * tw);
            if (hash(i * 7u + (uint32_t)(now * 2.5)) % 11u == 0) inten += 5;     /* a glint now and then */
            line(out, x, y, x, y, 0.0f, inten, BZ_FX_STAR, win);
        }
        /* from left to right: the beam's jumps between them short, and in
         * the same order every picture */
        for (int a = first_star + 1; a < out->count; a++) {
            bz_line t = out->lines[a];
            int b = a - 1;
            while (b >= first_star && out->lines[b].x0 > t.x0) { out->lines[b + 1] = out->lines[b]; b--; }
            out->lines[b + 1] = t;
        }
    }
}

/* --- over the picture ------------------------------------------------------------------ */

void bz_enhance_overlay(const bz_enhance *e, bz_pose pose, double now, const float win[4], bz_avg_list *out)
{
    (void)pose;
    /* (the window reaches below what the screen shows) */
    float l = win[0] > -250.0f ? win[0] : -250.0f, r = win[2] < 250.0f ? win[2] : 250.0f;
    float b = win[1] > -188.0f ? win[1] : -188.0f;
    if (e->on.flash && e->fire_at >= 0.0 && now - e->fire_at < 0.11 && now >= e->fire_at) {
        /* The gun is below the view: its flash comes up over the lower edge. */
        float t = (float)((now - e->fire_at) / 0.11);
        for (int i = 0; i < 7; i++) {
            float a = (0.25f + 0.5f * (float)i / 6.0f) * 3.14159265f, len = (i & 1 ? 26.0f : 40.0f) * (1.0f - 0.5f * t);
            float r0 = 6.0f + 20.0f * t;
            line(out, cosf(a) * r0, b + sinf(a) * r0, cosf(a) * (r0 + len), b + sinf(a) * (r0 + len), 0.0f,
                 (int)(15.0f * (1.0f - t)) + 1, BZ_FX_FLASH, win);
        }
    }
    if (e->on.levers && e->analog && (e->m->mem[BZ_ATRACT] & 0x80)) {
        /* A scale for each lever in the lower corners, and a mark on it. */
        for (int side = 0; side < 2; side++) {
            float x = side ? r - 14.0f : l + 14.0f, mid = b + 36.0f, half = 26.0f;
            float v = (float)(side ? e->lever_r : e->lever_l) / 256.0f;
            line(out, x, mid - half, x, mid + half, 0.0f, 5, BZ_FX_LEVER, EVERYWHERE);
            line(out, x - 3.0f, mid, x + 3.0f, mid, 0.0f, 5, BZ_FX_LEVER, EVERYWHERE);
            line(out, x - 6.0f, mid + v * half, x + 6.0f, mid + v * half, 0.0f, 13, BZ_FX_LEVER, EVERYWHERE);
        }
    }
}

/* --- colors ------------------------------------------------------------------------ */

static void mix(uint8_t rgb[3], const uint8_t to[3], float f)
{
    for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)lroundf((float)rgb[i] + ((float)to[i] - (float)rgb[i]) * f);
}

/* The color of something that was white hot t seconds ago. */
static void heat(float t, uint8_t rgb[3])
{
    static const struct { float t; uint8_t c[3]; } STEP[] = {
        { 0.0f, { 255, 255, 235 } }, { 0.35f, { 255, 230, 70 } }, { 1.0f, { 255, 135, 30 } },
        { 2.2f, { 235, 60, 25 } }, { 4.0f, { 150, 30, 20 } },
    };
    int n = (int)(sizeof STEP / sizeof STEP[0]);
    if (t <= 0.0f) { memcpy(rgb, STEP[0].c, 3); return; }
    for (int i = 0; i + 1 < n; i++)
        if (t < STEP[i + 1].t) {
            memcpy(rgb, STEP[i].c, 3);
            mix(rgb, STEP[i + 1].c, (t - STEP[i].t) / (STEP[i + 1].t - STEP[i].t));
            return;
        }
    memcpy(rgb, STEP[n - 1].c, 3);
}

/* The palette's color of a thing of the world. */
static void color_of_thing(const uint8_t *mem, unsigned id, int palette, uint8_t rgb[3])
{
    bz_line fake;
    memset(&fake, 0, sizeof fake);
    fake.tag = BZ_TAG(BZ_TAG_OBJECT, id);
    bz_color_of(mem, &fake, palette, rgb);
}

void bz_enhance_color(const bz_enhance *e, const bz_line *l, double now, int palette, const uint8_t horizon[3],
                      uint8_t rgb[3], float *k)
{
    unsigned kind = BZ_TAG_KIND(l->tag), key = BZ_TAG_KEY(l->tag);
    (void)now;
    if (kind == BZ_TAG_FX) {
        static const uint8_t COLOR[][3] = {
            { 255, 255, 255 }, { 225, 235, 255 }, { 150, 120, 90 }, { 255, 250, 200 }, { 255, 220, 40 },
        };
        static const uint8_t pale[3] = { 190, 200, 230 }, lilac[3] = { 175, 135, 255 }, white[3] = { 255, 255, 255 };
        if (key == BZ_FX_FAR) {
            memcpy(rgb, horizon, 3);                    /* the far row: the horizon's hue, pale (lilac with the flyer's) */
            mix(rgb, palette == BZ_PALETTE_FLYER ? lilac : pale, 0.55f);
            return;
        }
        if (key == BZ_FX_BOOM) {
            uint8_t c[3] = { 255, 255, 255 };
            if (e) color_of_thing(e->m->mem, l->model, palette, c);
            memcpy(rgb, c, 3);
            mix(rgb, white, 0.35f * (float)l->intensity / 15.0f);    /* (the brightest, newest, nearly white) */
            return;
        }
        memcpy(rgb, COLOR[key < sizeof COLOR / sizeof COLOR[0] ? key : 0], 3);
        return;
    }
    if (!e) return;
    const uint8_t *mem = e->m->mem;
    if (kind == BZ_TAG_OBJECT && key >= BZ_ID_PIECE && key < BZ_ID_FIELD && e->boom_id)
        color_of_thing(mem, e->boom_id, palette, rgb);  /* the wreck in the color of what it was */
    if (e->on.volcano && kind == BZ_TAG_VOLCANO && key >= 1 && key <= BZ_NOROCK) {
        /* YSPD: how fast the rock goes up (it goes down with a negative one). */
        int v = (int8_t)mem[BZ_YSPD + key - 1];
        heat(v >= 0 ? 0.3f - 0.3f * (float)v / 127.0f : 0.3f + 3.0f * (float)(-v) / 127.0f, rgb);
        if (v >= 0) *k *= 1.3f;
    }
    if (*k > 1.0f) *k = 1.0f;
}
