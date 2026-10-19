/* ground.c - see ground.h. */
#include "render/ground.h"

#include "core/tags.h"

#include <math.h>
#include <string.h>

#define SPACING   2048.0f
#define SCREEN    509.5f        /* the screen's units per unit of side (or height) over distance */
#define EYE       320.0f        /* the eye above the ground */
#define NEAR      700.0f        /* nearer than this is below the screen */
#define FAR       24000.0f      /* (15000 until 0.1.16; the owner saw no flicker and wanted it further. 30000 cost half again the lit length for little more to see) */
#define SIDE      0.62f         /* to the side as a part of the distance ahead: the screen's edge and a little */
#define TWO_PI    6.2831853f

static float wrap16(float v)
{
    v = fmodf(v, 65536.0f);
    if (v >= 32768.0f) v -= 65536.0f;
    if (v < -32768.0f) v += 65536.0f;
    return v;
}

bz_pose bz_pose_between(bz_pose a, bz_pose b, float f)
{
    float dx = wrap16(b.x - a.x), dy = wrap16(b.y - a.y), da = wrap16(b.angle - a.angle);
    if (fabsf(dx) > 1500.0f || fabsf(dy) > 1500.0f || fabsf(da) > 4000.0f) return f < 0.5f ? a : b;
    bz_pose p = { a.x + dx * f, a.y + dy * f, a.angle + da * f, a.bounce + (b.bounce - a.bounce) * f };
    return p;
}

/* A stretch in the view (d ahead, l to the side at each end) cut to where
 * g(d, l) = gd * d + gl * l + g0 is not below 0. Returns 0 if nothing is left. */
static int cut(float *d0, float *l0, float *d1, float *l1, float gd, float gl, float g0)
{
    float a = gd * *d0 + gl * *l0 + g0, b = gd * *d1 + gl * *l1 + g0;
    if (a < 0.0f && b < 0.0f) return 0;
    if (a < 0.0f) {
        float t = a / (a - b);
        *d0 += (*d1 - *d0) * t; *l0 += (*l1 - *l0) * t;
    } else if (b < 0.0f) {
        float t = b / (b - a);
        *d1 += (*d0 - *d1) * t; *l1 += (*l0 - *l1) * t;
    }
    return 1;
}

static void put(float a0, float b0, float a1, float b1, float eye, int intensity, float gain, unsigned tag,
                const float win[4], bz_avg_list *out)
{
    if (bz_avg_list_reserve(out, out->count + 1) != 0) return;
    bz_line *l = &out->lines[out->count++];
    memset(l, 0, sizeof *l);
    l->x0 = -SCREEN * b0 / a0; l->y0 = -SCREEN * eye / a0;
    l->x1 = -SCREEN * b1 / a1; l->y1 = -SCREEN * eye / a1;
    l->w0 = 1.0f / a0; l->w1 = 1.0f / a1;
    l->intensity = (uint8_t)intensity;
    l->gain = gain;
    l->z = 1;
    l->tag = (uint16_t)tag;
    memcpy(l->win, win, sizeof l->win);
}

#define BRIGHT_TO 5000.0f   /* the ground is lit in full up to here ... */
#define FADE_BANDS 8        /* ... and comes out of the dark beyond, in bands small on the screen */

/* How bright the ground is at distance d: in full up to BRIGHT_TO, then
 * down to nothing at FAR, without a step. */
static float lit_at(float d)
{
    if (d <= BRIGHT_TO) return 1.0f;
    float f = logf(d / BRIGHT_TO) / logf(FAR / BRIGHT_TO);
    return f >= 1.0f ? 0.0f : powf(1.0f - f, 1.4f);
}

/* The line is drawn in one bright stretch near the player and in
 * stretches that dim towards the horizon beyond that, each as bright as
 * its own middle is far. (Until 0.1.19 each band had a fixed brightness:
 * a line across the view, coming nearer, got brighter in steps as it
 * crossed into the next band, and the grid seemed alive.) A stretch along
 * the view stays where it is to the player, a line across it brightens
 * smoothly as it comes. */
static void stretch(float d0, float l0, float d1, float l1, float eye, const float win[4], bz_avg_list *out)
{
    if (!cut(&d0, &l0, &d1, &l1, SIDE, -1.0f, 0.0f)) return;       /* l <= SIDE * d */
    if (!cut(&d0, &l0, &d1, &l1, SIDE, 1.0f, 0.0f)) return;        /* l >= -SIDE * d */
    if (d0 > d1) {                      /* (near end first: the stretches then join) */
        float t = d0; d0 = d1; d1 = t;
        t = l0; l0 = l1; l1 = t;
    }
    float step = powf(FAR / BRIGHT_TO, 1.0f / FADE_BANDS);
    float from = NEAR, to = BRIGHT_TO;
    for (int i = 0; i <= FADE_BANDS; i++, from = to, to *= step) {
        float a0 = d0, b0 = l0, a1 = d1, b1 = l1;
        if (!cut(&a0, &b0, &a1, &b1, 1.0f, 0.0f, -from)) continue;
        if (!cut(&a0, &b0, &a1, &b1, -1.0f, 0.0f, to)) continue;
        if (fabsf(a1 - a0) + fabsf(b1 - b0) < 1.0f) continue;
        float gain = i == 0 ? 1.0f : lit_at(sqrtf(a0 * a1));      /* (its middle as the screen sees it) */
        if (gain < 0.03f) continue;
        put(a0, b0, a1, b1, eye, 8, gain, BZ_TAG(BZ_TAG_GROUND, 0), win, out);
    }
}

int bz_ground_point(bz_pose p, float wx, float wy, float z, float *sx, float *sy, float *ahead, float *side)
{
    float a = p.angle / 65536.0f * TWO_PI, c = cosf(a), s = sinf(a);
    float dx = wrap16(wx - p.x), dy = wrap16(wy - p.y);
    *ahead = dx * c + dy * s;
    *side = -dx * s + dy * c;
    if (*ahead < 200.0f) return 0;
    *sx = -SCREEN * *side / *ahead;
    *sy = SCREEN * (z - EYE - p.bounce) / *ahead;
    return 1;
}

void bz_ground_lines(bz_pose p, const float win[4], bz_avg_list *out)
{
    float spacing = SPACING, eye = EYE + p.bounce;
    float a = p.angle / 65536.0f * TWO_PI, c = cosf(a), s = sinf(a);
    float reach = FAR * 1.25f;
    /* The lines of one X each, then those of one Y: both ends in the view
     * (ahead: dx c + dy s, to the side: -dx s + dy c). */
    for (int along_y = 0; along_y < 2; along_y++) {
        float here = along_y ? p.y : p.x;
        float first = ceilf((here - reach) / spacing) * spacing;
        for (float v = first; v <= here + reach; v += spacing) {
            float k = v - here;                 /* the line's own coordinate from the player */
            float d0, l0, d1, l1;
            if (!along_y) {                     /* X = v, Y from -reach to reach */
                d0 = k * c - reach * s; l0 = -k * s - reach * c;
                d1 = k * c + reach * s; l1 = -k * s + reach * c;
            } else {                            /* Y = v */
                d0 = -reach * c + k * s; l0 = reach * s + k * c;
                d1 = reach * c + k * s;  l1 = -reach * s + k * c;
            }
            int before = out->count;
            stretch(d0, l0, d1, l1, eye, win, out);
            /* Every other line the other way round: the beam goes up one
             * and down the next instead of jumping back across the screen.
             * (By the line's own number, so that a line is drawn the same
             * way in every picture.) */
            if (lroundf(v / spacing) & 1) {
                for (int a = before, b = out->count - 1; a < b; a++, b--) {
                    bz_line t = out->lines[a];
                    out->lines[a] = out->lines[b];
                    out->lines[b] = t;
                }
                for (int a = before; a < out->count; a++) {
                    bz_line *l = &out->lines[a];
                    float t;
                    t = l->x0; l->x0 = l->x1; l->x1 = t;
                    t = l->y0; l->y0 = l->y1; l->y1 = t;
                    t = l->w0; l->w0 = l->w1; l->w1 = t;
                }
            }
        }
    }
}
