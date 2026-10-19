/* solid.h - things you cannot see through (the setting `occlusion`).
 *
 * The game draws wire frames: every line of every thing shows, the far
 * side of a pyramid and the mountains behind it as well. Here the lines of
 * a picture are cut where something nearer covers them. Nothing of the
 * game changes; it is done to the picture on its way to the tube. (After
 * the Star Wars port's render/solid.)
 *
 * What covers: the faces of the objects (tanks, the missile, the saucer,
 * pyramids and boxes, the pieces of a wreck). The ROM has only their
 * points and lines: the faces are found once, as the rings of lines that
 * lie in a plane (bz_solid_init).
 *
 * How far away a line is the drawing routines note while they draw
 * (bz_line.w0, w1: 1 / the distance ahead, which is straight across a flat
 * face on the screen). The land (horizon, mountains, moon, the volcano's
 * rocks) is behind everything. The score, the texts, the radar, the
 * sights and the crack in the windshield are never cut.
 */
#ifndef BZ_SOLID_H
#define BZ_SOLID_H

#include "avg/avg.h"

#define BZ_SOLID_MODELS   64        /* the objects of the ROM (OBJTBL) */
#define BZ_SOLID_FACE_MAX 8         /* points of a face at most */

typedef struct bz_solid_face {
    uint8_t n;
    uint8_t pt[BZ_SOLID_FACE_MAX];
} bz_solid_face;

typedef struct bz_solid_poly {
    int      n;                     /* its corners */
    float    x[BZ_SOLID_FACE_MAX], y[BZ_SOLID_FACE_MAX];
    float    a, b, c;               /* 1 / distance across it: a x + b y + c */
    float    wmax;
    float    x0, y0, x1, y1;        /* the box around it */
    uint32_t inst;                  /* the object drawn it is a face of */
    uint64_t points;                /* its points, as bits */
} bz_solid_poly;

typedef struct bz_solid {
    struct {
        bz_solid_face *faces;
        int            n;
    } model[BZ_SOLID_MODELS];
    /* the picture being cut */
    bz_solid_poly *polys;
    int            n_polys, cap_polys;
    bz_line       *out;
    int            n_out, cap_out;
    int            cut;             /* of the last run: lines cut or taken away */
} bz_solid;

/* Zero the struct first. Finds the faces of the objects in the ROM (mem:
 * the machine's memory image). */
void bz_solid_init(bz_solid *s, const uint8_t *mem);
void bz_solid_free(bz_solid *s);

/* The whole lines of a picture (bz_avg_run_raw with the notes), cut.
 * Returns how many there are now; *out stays valid until the next call. */
int bz_solid_run(bz_solid *s, const bz_line *in, int n, const bz_line **out);

#endif
