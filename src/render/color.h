/* color.h - a color for every line, by what it is.
 *
 * The cabinet's tube is white, under an overlay that makes the top fifth
 * red and the rest green. With the tags the drawing routines note
 * (core/tags.h) every line can have a color of its own instead:
 *
 *   the land      horizon and mountains green, the moon pale yellow, the
 *                 volcano's rocks glowing orange
 *   obstacles     blue: they are in the way, not after you
 *   enemies       the tank green (as the cabinet's overlay shows it), the super tank magenta, the missile
 *                 orange, the saucer violet; their shells red
 *   yours         the shell and the sights; the sights turn yellow with
 *                 the enemy in them
 *   hits          yellow flashes, the wreck's pieces orange
 *   the radar     ring red, sweep green, the blip yellow
 *   texts         warnings red, the score yellow, the high score cyan,
 *                 the table of high scores from yellow down to blue
 *   the logo      BATTLE orange, ZONE yellow
 *
 * That is the palette BZ_PALETTE_CLASSIC. BZ_PALETTE_FLYER takes its colors
 * from Atari's sales flyer of 1980: mountains of ice, white and pale blue,
 * over a blue horizon and a blue grid; an orange moon; the tank green as
 * on the cabinet, the super tank in the red of the flyer's sky;
 * yellow and orange for the logo, the sights and the score.
 *
 * Only the color: how bright a line is stays the game's.
 */
#ifndef BZ_COLOR_H
#define BZ_COLOR_H

#include "avg/avg.h"

/* The color of a line (full brightness, 0..255 each). mem: the machine's
 * memory image, for the kinds of the playfield's objects. */
enum { BZ_PALETTE_CLASSIC = 0, BZ_PALETTE_FLYER };

void bz_color_of(const uint8_t *mem, const bz_line *l, int palette, uint8_t rgb[3]);

/* The palette's color of the horizon (what far things fade into). */
const uint8_t *bz_color_horizon(int palette);

#endif
