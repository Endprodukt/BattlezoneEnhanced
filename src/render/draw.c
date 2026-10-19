#include "render/draw.h"

#include "render/color.h"

#include <math.h>

#define BOARD_MAX 4095

void bz_map_point(const bz_config *c, float x, float y, int *bx, int *by)
{
    float x0 = (float)c->margin_left, x1 = (float)(BOARD_MAX - c->margin_right);
    float y0 = (float)c->margin_bottom, y1 = (float)(BOARD_MAX - c->margin_top);
    float u = (x - c->view_left) / (c->view_right - c->view_left);
    float v = (y - c->view_bottom) / (c->view_top - c->view_bottom);
    if (c->flip_x) u = 1.0f - u;
    if (c->flip_y) v = 1.0f - v;
    *bx = (int)lroundf(x0 + u * (x1 - x0));
    *by = (int)lroundf(y0 + v * (y1 - y0));
}

typedef struct pen {
    vp_dvg *d;
    int     have_pos, x, y;
    int     have_col;
    uint8_t col[3];
} pen;

static void stroke(pen *p, const bz_config *c, float ax, float ay, float bx, float by,
                   const uint8_t rgb[3], float k)
{
    uint8_t r = (uint8_t)lroundf(rgb[0] * k), g = (uint8_t)lroundf(rgb[1] * k), b = (uint8_t)lroundf(rgb[2] * k);
    if (!(r | g | b)) return;
    int x0, y0, x1, y1;
    bz_map_point(c, ax, ay, &x0, &y0);
    bz_map_point(c, bx, by, &x1, &y1);
    if (!p->have_col || r != p->col[0] || g != p->col[1] || b != p->col[2]) {
        vp_color(p->d, r, g, b);
        p->col[0] = r; p->col[1] = g; p->col[2] = b;
        p->have_col = 1;
    }
    if (!p->have_pos || x0 != p->x || y0 != p->y) vp_moveto(p->d, x0, y0);
    if (x0 == x1 && y0 == y1) x1 += 1;      /* a dot: the beam has to move to show */
    vp_lineto(p->d, x1, y1);
    p->x = x1; p->y = y1;
    p->have_pos = 1;
}

static const bz_enhance *g_enh;
static double g_now;
static uint16_t g_leave_tag;
static float *g_leave_box;

void bz_draw_leave_out(uint16_t tag, float box[4])
{
    g_leave_tag = tag;
    g_leave_box = box;
    if (box) { box[0] = 1e9f; box[1] = 1e9f; box[2] = -1e9f; box[3] = -1e9f; }
}

void bz_draw_enhance(const bz_enhance *e, double now)
{
    g_enh = e;
    g_now = now;
}

void bz_draw_lines(vp_dvg *d, const bz_config *c, const uint8_t *mem, const bz_line *lines, int count)
{
    pen p = { d, 0, 0, 0, 0, { 0, 0, 0 } };
    /* The overlay's border, in screen units. */
    float split = c->view_top - (c->view_top - c->view_bottom) * c->overlay_split;

    for (int i = 0; i < count; i++) {
        const bz_line *l = &lines[i];
        if (g_leave_tag && l->tag == g_leave_tag) {
            if (g_leave_box) {
                float xa = l->x0 < l->x1 ? l->x0 : l->x1, xb = l->x0 < l->x1 ? l->x1 : l->x0;
                float ya = l->y0 < l->y1 ? l->y0 : l->y1, yb = l->y0 < l->y1 ? l->y1 : l->y0;
                if (xa < g_leave_box[0]) g_leave_box[0] = xa;
                if (ya < g_leave_box[1]) g_leave_box[1] = ya;
                if (xb > g_leave_box[2]) g_leave_box[2] = xb;
                if (yb > g_leave_box[3]) g_leave_box[3] = yb;
            }
            continue;
        }
        float k = c->floor + (1.0f - c->floor) * powf((float)l->intensity / 15.0f, c->gamma);
        k *= c->brightness;
        if (l->gain > 0.0f) k *= l->gain;
        if (k > 1.0f) k = 1.0f;
        if (c->color_mode == BZ_COLOR_COLOR || c->color_mode == BZ_COLOR_FLYER) {
            uint8_t rgb[3];
            int palette = c->color_mode == BZ_COLOR_FLYER ? BZ_PALETTE_FLYER : BZ_PALETTE_CLASSIC;
            bz_color_of(mem, l, palette, rgb);
            bz_enhance_color(g_enh, l, g_now, palette, bz_color_horizon(palette), rgb, &k);
            stroke(&p, c, l->x0, l->y0, l->x1, l->y1, rgb, k);
            continue;
        }
        if (c->color_mode != BZ_COLOR_OVERLAY) {
            stroke(&p, c, l->x0, l->y0, l->x1, l->y1, c->color, k);
            continue;
        }
        int top0 = l->y0 > split, top1 = l->y1 > split;
        if (top0 == top1) {
            stroke(&p, c, l->x0, l->y0, l->x1, l->y1, top0 ? c->color_top : c->color_bottom, k);
        } else {
            float t = (split - l->y0) / (l->y1 - l->y0);
            float mx = l->x0 + (l->x1 - l->x0) * t;
            stroke(&p, c, l->x0, l->y0, mx, split, top0 ? c->color_top : c->color_bottom, k);
            stroke(&p, c, mx, split, l->x1, l->y1, top1 ? c->color_top : c->color_bottom, k);
        }
    }
}
