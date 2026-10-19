/* solid.c - see solid.h. */
#include "render/solid.h"

#include "core/bz_symbols.h"
#include "core/tags.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PTS      64
#define FLATNESS     0.12f      /* how far out of its plane a ring's points may be, of its size */
#define MIN_AREA     1.0f       /* a face seen edge-on covers nothing (screen units squared) */
#define W_REL        0.004f     /* a line is behind a face if it is this much farther away ... */
#define XY_SLACK     0.75f      /* ... and more than this much of a screen unit's worth of the face's slope */
#define MIN_PIECE    0.2f       /* what is left of a line and shorter is dropped */
#define MAX_CUTS     64
#define MAX_INST     48

/* --- the faces of an object ------------------------------------------------------- */

typedef struct finder {
    int16_t  pts[MAX_PTS][3];
    uint8_t  adj[MAX_PTS][MAX_PTS];
    int      n_pts;
    int      path[BZ_SOLID_FACE_MAX];
    bz_solid_face *faces;
    int      n, cap;
} finder;

/* A ring of lines is a face if its points lie in a plane, near enough. */
static int flat(const finder *f, const int *ring, int n)
{
    double nx = 0, ny = 0, nz = 0, cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < n; i++) {
        const int16_t *p = f->pts[ring[i]], *q = f->pts[ring[(i + 1) % n]];
        nx += (double)(p[1] - q[1]) * (p[2] + q[2]);        /* Newell: the area as a vector */
        ny += (double)(p[2] - q[2]) * (p[0] + q[0]);
        nz += (double)(p[0] - q[0]) * (p[1] + q[1]);
        cx += p[0]; cy += p[1]; cz += p[2];
    }
    double len = sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1.0) return 0;
    nx /= len; ny /= len; nz /= len;
    cx /= n; cy /= n; cz /= n;
    double off = 0;
    for (int i = 0; i < n; i++) {
        const int16_t *p = f->pts[ring[i]];
        double d = fabs((p[0] - cx) * nx + (p[1] - cy) * ny + (p[2] - cz) * nz);
        if (d > off) off = d;
    }
    return off <= FLATNESS * sqrt(len * 0.5);
}

static void add_face(finder *f, const int *ring, int n)
{
    if (!flat(f, ring, n)) return;
    if (f->n == f->cap) {
        int cap = f->cap ? f->cap * 2 : 32;
        bz_solid_face *p = (bz_solid_face *)realloc(f->faces, (size_t)cap * sizeof *p);
        if (!p) return;
        f->faces = p;
        f->cap = cap;
    }
    bz_solid_face *face = &f->faces[f->n++];
    face->n = (uint8_t)n;
    for (int i = 0; i < n; i++) face->pt[i] = (uint8_t)ring[i];
}

/* Every ring of lines without a line across it that starts at path[0],
 * its lowest point, each found once. */
static void rings(finder *f, int len)
{
    int s = f->path[0], v = f->path[len - 1];
    for (int u = s + 1; u < f->n_pts; u++) {
        if (!f->adj[v][u]) continue;
        int ok = 1;
        for (int i = 0; i < len && ok; i++)
            if (f->path[i] == u || (i >= 1 && i < len - 1 && f->adj[u][f->path[i]])) ok = 0;
        if (!ok) continue;
        f->path[len] = u;
        if (len >= 2 && f->adj[u][s]) {
            if (f->path[1] < u) add_face(f, f->path, len + 1);
        } else if (len + 1 < BZ_SOLID_FACE_MAX) {
            rings(f, len + 1);
        }
    }
}

static unsigned word(const uint8_t *mem, unsigned a)
{
    return mem[a & 0x7fff] | (unsigned)mem[(a + 1) & 0x7fff] << 8;
}

static int in_rom(unsigned a) { return a >= 0x3000 && a < 0x8000; }

void bz_solid_init(bz_solid *s, const uint8_t *mem)
{
    static finder f;
    for (unsigned id = 0; id < BZ_SOLID_MODELS; id++) {
        free(s->model[id].faces);
        s->model[id].faces = NULL;
        s->model[id].n = 0;
        /* The object's points (as PNTPUT reads them: a length, then X, Y,
         * Z of each) and its drawing list (DRAW: a command in the low
         * three bits, the point's number above; FF ends it). */
        unsigned pts = word(mem, BZ_OBJPNT + 2 * id), list = word(mem, BZ_OBJTBL + 2 * id);
        if (!in_rom(pts) || !in_rom(list)) continue;
        memset(&f, 0, sizeof f);
        unsigned len = mem[pts];
        for (unsigned y = 1; y < len && f.n_pts < MAX_PTS; y += 6, f.n_pts++)
            for (int k = 0; k < 3; k++) f.pts[f.n_pts][k] = (int16_t)word(mem, pts + y + 2 * (unsigned)k);
        int at = -1;
        for (unsigned i = 0; i < 400 && in_rom(list + i); i++) {
            unsigned b = mem[list + i];
            if (b == 0xff) break;
            int cmd = b & 7, p = (int)(b >> 3);
            if (cmd == 1 || cmd == 5 || cmd > 5) continue;      /* brightness, the explosion's picture */
            if (p >= f.n_pts) { at = -1; continue; }
            if (cmd == 4 && at >= 0 && at != p) f.adj[at][p] = f.adj[p][at] = 1;
            at = p;
        }
        for (int a = 0; a < f.n_pts; a++) {
            f.path[0] = a;
            rings(&f, 1);
        }
        s->model[id].faces = f.faces;
        s->model[id].n = f.n;
    }
}

void bz_solid_free(bz_solid *s)
{
    for (int i = 0; i < BZ_SOLID_MODELS; i++) free(s->model[i].faces);
    free(s->polys);
    free(s->out);
    memset(s, 0, sizeof *s);
}

/* --- a picture -------------------------------------------------------------------- */

typedef struct inst {
    uint32_t key;               /* the tag and the object's number */
    uint64_t have;
    float    x[MAX_PTS], y[MAX_PTS], w[MAX_PTS];
} inst;

static int world(const bz_line *l)
{
    int kind = BZ_TAG_KIND(l->tag);
    return kind == BZ_TAG_MOUNT || kind == BZ_TAG_VOLCANO || kind == BZ_TAG_OBJECT || kind == BZ_TAG_GROUND ||
           kind == BZ_TAG_FX;
}

static uint32_t inst_key(const bz_line *l)
{
    return (uint32_t)l->tag << 8 | l->model;
}

static void note_point(inst *in, unsigned p, float x, float y, float w)
{
    if (!p || p > MAX_PTS || w <= 0.0f) return;
    p--;
    in->have |= 1ull << p;
    in->x[p] = x; in->y[p] = y; in->w[p] = w;
}

static void add_poly(bz_solid *s, const inst *in, const bz_solid_face *face)
{
    bz_solid_poly p;
    memset(&p, 0, sizeof p);
    p.n = face->n;
    p.inst = in->key;
    p.x0 = p.y0 = 1e30f;
    p.x1 = p.y1 = -1e30f;
    double mx = 0, my = 0, mw = 0;
    float w[BZ_SOLID_FACE_MAX];
    for (int i = 0; i < p.n; i++) {
        unsigned k = face->pt[i];
        if (!(in->have >> k & 1)) return;       /* (a point not drawn to: nothing known of the face) */
        p.points |= 1ull << k;
        p.x[i] = in->x[k]; p.y[i] = in->y[k]; w[i] = in->w[k];
        if (p.x[i] < p.x0) p.x0 = p.x[i];
        if (p.x[i] > p.x1) p.x1 = p.x[i];
        if (p.y[i] < p.y0) p.y0 = p.y[i];
        if (p.y[i] > p.y1) p.y1 = p.y[i];
        if (w[i] > p.wmax) p.wmax = w[i];
        mx += p.x[i]; my += p.y[i]; mw += w[i];
    }
    double area = 0;
    for (int i = 0; i < p.n; i++) {
        int j = (i + 1) % p.n;
        area += (double)p.x[i] * p.y[j] - (double)p.x[j] * p.y[i];
    }
    if (fabs(area) * 0.5 < MIN_AREA) return;
    /* 1 / distance is straight across the face: the plane through its
     * corners, by least squares. */
    mx /= p.n; my /= p.n; mw /= p.n;
    double sxx = 0, sxy = 0, syy = 0, sxw = 0, syw = 0;
    for (int i = 0; i < p.n; i++) {
        double dx = p.x[i] - mx, dy = p.y[i] - my, dw = w[i] - mw;
        sxx += dx * dx; sxy += dx * dy; syy += dy * dy; sxw += dx * dw; syw += dy * dw;
    }
    double det = sxx * syy - sxy * sxy;
    if (fabs(det) < 1e-6) return;
    p.a = (float)((sxw * syy - syw * sxy) / det);
    p.b = (float)((syw * sxx - sxw * sxy) / det);
    p.c = (float)(mw - p.a * mx - p.b * my);

    if (s->n_polys == s->cap_polys) {
        int cap = s->cap_polys ? s->cap_polys * 2 : 128;
        bz_solid_poly *q = (bz_solid_poly *)realloc(s->polys, (size_t)cap * sizeof *q);
        if (!q) return;
        s->polys = q;
        s->cap_polys = cap;
    }
    s->polys[s->n_polys++] = p;
}

static int inside(const bz_solid_poly *p, float x, float y)
{
    int in = 0;
    for (int i = 0, j = p->n - 1; i < p->n; j = i++)
        if ((p->y[i] > y) != (p->y[j] > y) &&
            x < (p->x[j] - p->x[i]) * (y - p->y[i]) / (p->y[j] - p->y[i]) + p->x[i])
            in = !in;
    return in;
}

static int float_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static void emit(bz_solid *s, const bz_line *l)
{
    if (s->n_out == s->cap_out) {
        int cap = s->cap_out ? s->cap_out * 2 : 512;
        bz_line *q = (bz_line *)realloc(s->out, (size_t)cap * sizeof *q);
        if (!q) return;
        s->out = q;
        s->cap_out = cap;
    }
    s->out[s->n_out++] = *l;
}

/* The parts of a line that a face covers, as stretches of 0..1 along it,
 * added to cut[]. */
static int cover(const bz_solid_poly *p, const bz_line *l, float (*cut)[2], int n)
{
    float lx0 = l->x0 < l->x1 ? l->x0 : l->x1, lx1 = l->x0 < l->x1 ? l->x1 : l->x0;
    float ly0 = l->y0 < l->y1 ? l->y0 : l->y1, ly1 = l->y0 < l->y1 ? l->y1 : l->y0;
    if (lx1 < p->x0 || lx0 > p->x1 || ly1 < p->y0 || ly0 > p->y1) return n;
    float wfar = l->w0 > l->w1 ? l->w0 : l->w1;
    if (wfar > 0.0f && l->w0 > p->wmax * 1.02f && l->w1 > p->wmax * 1.02f) return n;   /* all of it nearer */

    /* A line of the same object: the face's own edges show; one that
     * leaves a corner of the face is before it or behind it as a whole. */
    int own = inst_key(l) == p->inst && BZ_TAG_KIND(l->tag) == BZ_TAG_OBJECT;
    int on0 = own && l->p0 && (p->points >> (l->p0 - 1) & 1);
    int on1 = own && l->p1 && (p->points >> (l->p1 - 1) & 1);
    if (on0 && on1) return n;

    float dx = l->x1 - l->x0, dy = l->y1 - l->y0;
    float ts[BZ_SOLID_FACE_MAX + 2];
    int nt = 0;
    ts[nt++] = 0.0f;
    for (int i = 0, j = p->n - 1; i < p->n; j = i++) {
        float ex = p->x[i] - p->x[j], ey = p->y[i] - p->y[j];
        float den = dx * ey - dy * ex;
        if (fabsf(den) < 1e-9f) continue;
        float t = ((p->x[j] - l->x0) * ey - (p->y[j] - l->y0) * ex) / den;
        float u = ((p->x[j] - l->x0) * dy - (p->y[j] - l->y0) * dx) / den;
        if (t > 0.0f && t < 1.0f && u >= 0.0f && u <= 1.0f) ts[nt++] = t;
    }
    ts[nt++] = 1.0f;
    qsort(ts, (size_t)nt, sizeof ts[0], float_cmp);

    float slack = W_REL * p->wmax + XY_SLACK * (fabsf(p->a) + fabsf(p->b));
    for (int i = 0; i + 1 < nt && n < MAX_CUTS; i++) {
        float ta = ts[i], tb = ts[i + 1];
        if (tb - ta < 1e-5f) continue;
        float tm = 0.5f * (ta + tb);
        if (!inside(p, l->x0 + dx * tm, l->y0 + dy * tm)) continue;
        /* how much farther than the face the line is at each end of the stretch (above 0: behind) */
        float da = p->a * (l->x0 + dx * ta) + p->b * (l->y0 + dy * ta) + p->c - (l->w0 + (l->w1 - l->w0) * ta);
        float db = p->a * (l->x0 + dx * tb) + p->b * (l->y0 + dy * tb) + p->c - (l->w0 + (l->w1 - l->w0) * tb);
        if (on0 || on1) {
            if (da <= slack && db <= slack) continue;
        } else if (da <= slack && db <= slack) {
            continue;
        } else if (da <= slack || db <= slack) {
            /* goes through the face's plane: behind it from there on */
            float tc = ta + (tb - ta) * (slack - da) / (db - da);
            if (da <= slack) ta = tc;
            else tb = tc;
        }
        cut[n][0] = ta;
        cut[n][1] = tb;
        n++;
    }
    return n;
}

static int cut_cmp(const void *a, const void *b)
{
    float x = ((const float *)a)[0], y = ((const float *)b)[0];
    return x < y ? -1 : x > y;
}

int bz_solid_run(bz_solid *s, const bz_line *in, int n, const bz_line **out)
{
    static inst insts[MAX_INST];
    int n_inst = 0;
    s->n_polys = 0;
    s->n_out = 0;
    s->cut = 0;

    /* The objects of the picture: where each of their points is. */
    for (int i = 0; i < n; i++) {
        const bz_line *l = &in[i];
        if (BZ_TAG_KIND(l->tag) != BZ_TAG_OBJECT || (!l->p0 && !l->p1) || l->sub) continue;
        if (l->model >= BZ_SOLID_MODELS || !s->model[l->model].n) continue;
        uint32_t key = inst_key(l);
        int k = 0;
        while (k < n_inst && insts[k].key != key) k++;
        if (k == n_inst) {
            if (n_inst == MAX_INST) continue;
            insts[k].key = key;
            insts[k].have = 0;
            n_inst++;
        }
        note_point(&insts[k], l->p0, l->x0, l->y0, l->w0);
        note_point(&insts[k], l->p1, l->x1, l->y1, l->w1);
    }
    for (int k = 0; k < n_inst; k++) {
        unsigned model = insts[k].key & 0xff;
        for (int f = 0; f < s->model[model].n; f++) add_poly(s, &insts[k], &s->model[model].faces[f]);
    }

    for (int i = 0; i < n; i++) {
        const bz_line *l = &in[i];
        if (!s->n_polys || !world(l)) { emit(s, l); continue; }
        float cut[MAX_CUTS][2];
        int nc = 0;
        for (int k = 0; k < s->n_polys && nc < MAX_CUTS; k++) nc = cover(&s->polys[k], l, cut, nc);
        if (!nc) { emit(s, l); continue; }
        s->cut++;
        qsort(cut, (size_t)nc, sizeof cut[0], cut_cmp);
        float len = hypotf(l->x1 - l->x0, l->y1 - l->y0);
        if (len <= 0.0f) continue;              /* a dot under a face */
        float at = 0.0f;
        for (int k = 0; k <= nc; k++) {
            float to = k < nc ? cut[k][0] : 1.0f;
            if (to - at > 0.0f && (to - at) * len >= MIN_PIECE) {
                bz_line piece = *l;
                piece.x0 = l->x0 + (l->x1 - l->x0) * at; piece.y0 = l->y0 + (l->y1 - l->y0) * at;
                piece.x1 = l->x0 + (l->x1 - l->x0) * to; piece.y1 = l->y0 + (l->y1 - l->y0) * to;
                piece.w0 = l->w0 + (l->w1 - l->w0) * at;
                piece.w1 = l->w0 + (l->w1 - l->w0) * to;
                emit(s, &piece);
            }
            if (k < nc && cut[k][1] > at) at = cut[k][1];
        }
    }
    *out = s->out;
    return s->n_out;
}
