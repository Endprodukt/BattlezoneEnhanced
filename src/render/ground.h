/* ground.h - a grid on the ground (the setting `ground_grid`).
 *
 * The cabinet's ground is empty: only the things standing on it tell how
 * the tank moves. Atari's flyer of 1980 paints a grid on it, and so does
 * this: lines every 2048 units of the playfield both ways, as the game's
 * own projection would draw them (the math box's: distance ahead and to
 * the side from the player's place and heading, the screen's X and Y as
 * 509.5 times side or height over the distance, the eye 320 above the
 * ground). They come out of the dark far away and grow brighter towards
 * the player, without a step: nothing of the ground appears at once.
 *
 * Nothing of the game: the lines are made for the picture alone, from
 * where the player is. They carry their distance, so objects cover them
 * (render/solid.h).
 */
#ifndef BZ_GROUND_H
#define BZ_GROUND_H
/* (the ground: lit in full near the player, out of the dark far away) */

#include "avg/avg.h"

/* Where the player is: the playfield's 16-bit X and Y, the heading as 16
 * bits a turn (TANGLE, LANGLE). */
typedef struct bz_pose {
    float x, y, angle;
    float bounce;           /* how far the view is thrown up by a collision (BOUNCE) */
} bz_pose;

/* The pose f of the way from a to b (the playfield and the turn wrap
 * around). A jump (the player put somewhere else) is not flown through. */
bz_pose bz_pose_between(bz_pose a, bz_pose b, float f);

/* Add the grid's lines to out, whole, to be cut to the window win (left,
 * bottom, right, top). */
void bz_ground_lines(bz_pose p, const float win[4], bz_avg_list *out);

/* A point of the playfield (z: its height above the ground) as the player
 * sees it. Returns 0 if it is not ahead; else the screen's x and y and
 * *ahead, *side: how far ahead and to the side (left positive) it is. */
int bz_ground_point(bz_pose p, float wx, float wy, float z, float *sx, float *sy, float *ahead, float *side);

#endif
