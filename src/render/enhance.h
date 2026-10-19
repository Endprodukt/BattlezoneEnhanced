/* enhance.h - what the picture gets that the cabinet's never had.
 *
 * Every one of these is an option of its own (battlezone.ini, the test
 * menu's ENHANCED page), and none of them is the game's: they are made
 * for the picture from what the game's memory says, and the game runs as
 * it always did.
 *
 *   particles a burst of sparks when the enemy or the saucer is hit, in
 *             the color of what was hit (as Tempest 2000 does it); the
 *             pieces of the wreck are in that color too
 *   stars     stars over the mountains, turning with the view, twinkling;
 *             behind the moon and the mountains
 *   far       a second row of mountains behind the first, farther away:
 *             it turns slower with the view (parallax) and shows only
 *             over the first row's skyline
 *   volcano   its rocks glow as they rise and cool as they fall
 *   flash     a flash at the muzzle when the player fires, and dust where
 *             a shell comes down
 *   levers    with analog controls: how far each lever is pushed
 */
#ifndef BZ_ENHANCE_H
#define BZ_ENHANCE_H

#include "avg/avg.h"
#include "core/machine.h"
#include "render/ground.h"

/* The key of BZ_TAG_FX: what a line of these is. */
enum {
    BZ_FX_STAR = 1, BZ_FX_DUST, BZ_FX_FLASH, BZ_FX_LEVER, BZ_FX_FAR,
    BZ_FX_BOOM              /* a spark; bz_line.model holds what was hit (BZ_ID_xxx) */
};

typedef struct bz_enh_opts {
    int particles, stars, far, volcano, flash, levers;  /* (haze: gone in 0.1.19) */
} bz_enh_opts;

#define BZ_ENH_BITS 224
#define BZ_ENH_SKY  513         /* the skyline, a column for each unit of x from -256 to 256 */

typedef struct bz_enhance {
    bz_enh_opts on;
    const bz_machine *m;        /* the game, to read */
    double   now;               /* the game's clock at the last frame */

    double   explode_at;        /* when the enemy began to fly apart; < 0: he has not */
    uint8_t  boom_id;           /* what it was (BZ_ID_xxx) */
    double   fire_at;           /* when the player fired last; < 0: never */
    uint8_t  was_exploding, was_saucer_hit, was_firect[3];

    /* Sparks and dust: each flies from where it was born and falls. */
    struct { float x, y, z, vx, vy, vz; double born; float life; uint8_t what; } bit[BZ_ENH_BITS];
    int      n_bit;

    /* Of the picture being made (bz_enhance_scene): the first row of
     * mountains as a skyline, and where the moon is. */
    float    sky[BZ_ENH_SKY];
    float    moon_x, moon_y, moon_r;
    int      have_moon;

    int      analog;            /* the analog controls are in use, and where the levers are (-256..256) */
    int      lever_l, lever_r;
} bz_enhance;

void bz_enhance_init(bz_enhance *e, const bz_machine *m);

/* When the machine has just run a game frame; now: the game's clock. */
void bz_enhance_frame(bz_enhance *e, double now);

/* The picture's own lines (the game's, as they are to be shown): the
 * skyline and the moon are taken from them. Before bz_enhance_world. */
void bz_enhance_scene(bz_enhance *e, const bz_line *lines, int n);

/* The things in the world (stars, the far mountains, sparks, dust), whole
 * lines with their distance, to be cut by what covers them and to the
 * window win. */
void bz_enhance_world(const bz_enhance *e, bz_pose pose, double now, const float win[4], bz_avg_list *out);

/* What lies over the picture (the flash, the levers): added to the
 * finished picture `out`. */
void bz_enhance_overlay(const bz_enhance *e, bz_pose pose, double now, const float win[4], bz_avg_list *out);

/* Change a line's color and brightness k (0..1) for the sparks, the
 * wreck and the volcano; palette: BZ_PALETTE_xxx (render/color.h),
 * horizon: its color of the horizon. Sets the color of the lines made
 * here. e may be NULL. */
void bz_enhance_color(const bz_enhance *e, const bz_line *l, double now, int palette, const uint8_t horizon[3],
                      uint8_t rgb[3], float *k);

#endif
