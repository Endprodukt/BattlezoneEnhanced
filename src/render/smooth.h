/* smooth.h - the game's picture at any time.
 *
 * The game draws a picture a frame, 15 a second; the monitor wants one
 * whenever it is done with the last. A frame's picture shows the world as
 * it stands when the frame begins; the frame then moves the world one
 * step. So the moment a frame has run, the next frame's picture is known
 * as well: its drawing takes nothing in but that state. This module draws
 * it ahead, on a copy of the machine with the game's own routines
 * (bz_picture), and between two frames shows every line of the world on
 * its way from this picture to the next one's: the mountains, the
 * objects, the shells, the radar's sweep. Nothing waits for a later frame,
 * so the picture is never behind the game; and the game itself runs
 * exactly as before, it knows nothing of this.
 *
 * Which line of one picture is which of the other is known from the tags
 * the drawing routines note (core/tags.h): the same thing, the same
 * vector of it. Lines of things that are in one picture only come and go
 * halfway. Texts, the score, the sights are not moved: they are shown as
 * this frame has them.
 *
 * The game's frames do not come evenly (the interrupt lets one go every
 * 16th time, but the display buffers change hands every 6th: 18, 18, 12
 * interrupts apart, and further when a frame has much to draw). Shown
 * that way the motion would stumble, so a frame's picture is begun one
 * period after the one before it began, the period being what the last
 * six frames took on average, or when the frame has run if that is later:
 * up to 16 ms late once it has settled, and even.
 *
 * The game's screen positions are whole numbers of a coarse grid: a thing
 * far away stands still for a few frames and then jumps half a unit. Moved
 * between such positions it would still stop and go. So the objects'
 * points are taken unrounded, from what the math box divided (`exact`,
 * avg/avg.h: bz_avg_exact).
 */
#ifndef BZ_SMOOTH_H
#define BZ_SMOOTH_H

#include "avg/avg.h"
#include "core/machine.h"
#include "render/enhance.h"
#include "render/ground.h"
#include "render/solid.h"

typedef struct bz_smooth_pic {
    bz_avg_list cur;        /* the frame's own picture, lines whole */
    bz_avg_list next;       /* the next frame's, drawn ahead */
    double      start;      /* when it begins to show (seconds) */
    double      dur;        /* how long it takes to become the next */
    bz_pose     pose0, pose1;   /* where the player is in the one and in the other */
    bz_machine  a, b;       /* the game as the frame found it and as it left it (for `computed`) */
    int         have_ab;
    int         land;       /* the picture shows the land (not the high scores) */
    float       win[4];     /* the window the land is cut to */
} bz_smooth_pic;

typedef struct bz_smooth {
    bz_machine    scratch;
    bz_smooth_pic shown, pending;
    int           have_shown, have_pending;
    bz_avg_list   out;
    bz_avg_list   raw;      /* the picture's lines whole, before anything is cut */
    bz_solid     *solid;    /* not NULL: what is behind something is not drawn (render/solid.h) */
    int           grid;     /* 1: a grid on the ground (render/ground.h) */
    int           exact;    /* 1: the objects' points unrounded (avg/avg.h: bz_avg_exact) */
    /* 1: the picture of a moment between two frames is computed (as the
     * Star Wars port does it): everything that moves is put where it is
     * on its way from the frame's state to the next one's, and the game's
     * own drawing routines draw that, on a copy, with all 16 bits of the
     * angles (bz_machine.fine). Right in perspective, and what comes into
     * view comes in where it is. 0: the lines of the frame's picture are
     * moved to where the next one's are. */
    int           computed;
    int           order;    /* 1: the lines put in an order that keeps the beam's dark jumps short (bz_order_lines);
                             * 2: the texts and gauges first, still ones before moving ones, then the world,
                             * each in the game's own order (bz_order_steady) */
    bz_machine    last;     /* the game when the last frame had run */
    int           have_last;
    bz_enhance   *enh;      /* not NULL: its additions to the picture (render/enhance.h) */
    bz_pose       at;       /* where the player is in the picture being made, and when that is */
    double        at_time;
    bz_pose       pose;     /* where the player was when the last frame had run */
    int           have_pose;
    void         *index;    /* work space */
    int           index_cap;
    double        period;   /* how long a picture takes to become the next: the frames' own pace */
    double        arrived;  /* when the last frame came */
    double        took[6];  /* what the last frames took */
    int           ntook;
    uint32_t      frames;   /* pictures taken in */
    int           moved, popped;    /* of the last bz_smooth_lines: lines moved; lines of one picture only */
    uint32_t      shown_count, held_count;  /* pictures made in all; those that had to stand still, their frame's
                                             * way to the next being over and the next not there yet */
    float         last_f;           /* how far along the last one was */
} bz_smooth;

void bz_smooth_init(bz_smooth *s);
void bz_smooth_free(bz_smooth *s);

/* Call when the machine has just run a game frame (bz_machine.frames has
 * changed); now is the game's clock in seconds. */
void bz_smooth_frame(bz_smooth *s, const bz_machine *m, double now);

/* The picture at the time now, cut to the window. smooth 0: every frame's
 * picture as it is. Returns the number of lines; *lines stays valid until
 * the next call. Before the first frame: 0. */
int bz_smooth_lines(bz_smooth *s, double now, int smooth, const bz_line **lines);

/* Put lines in an order in which the beam's dark jumps from one to the
 * next are short: strokes (lines that join end to start) are kept as they
 * are and taken, either way round, nearest first; the stars (dots
 * anywhere) last, as the Star Wars port does, so that no line is drawn
 * right after a long jump to one. The board draws the jumps too: a picture
 * with half its length in jumps takes it twice as long.
 *
 * Only the world's lines are put in another order. The texts, the score,
 * the tanks left, the radar and the sights come first, in the game's own
 * order and direction, the same in every picture: a line on a vector
 * tube lands a little differently with every way the beam comes to it,
 * and a score drawn in another order every picture shimmers. */
void bz_order_lines(bz_line *lines, int n);

/* The texts, the score, the tanks left, the radar's ring and the sights
 * first, then the radar's sweep and blip, then the world: each part in
 * the game's own order and direction. A line on a vector tube lands a
 * little differently with every way the beam comes to it; drawn after the
 * world, the score is reached from wherever the last moving line ended,
 * and it shimmers. Drawn first, it is reached the same way every picture. */
void bz_order_steady(bz_line *lines, int n);

#endif
