/* draw.h - the vector generator's lines onto the VectorPie device layer. */
#ifndef BZ_DRAW_H
#define BZ_DRAW_H

#include "avg/avg.h"
#include "render/config.h"
#include "render/enhance.h"

#include <vectorpie/vp_dvg.h>

/* Screen units -> board units (0..4095, may fall outside: the SDK clips). */
void bz_map_point(const bz_config *c, float x, float y, int *bx, int *by);

/* Draw lines in list order, chaining strokes where one line ends where the
 * next begins and setting the color only when it changes. A line across
 * the overlay's border is drawn in two parts. mem: the machine's memory
 * image (for BZ_COLOR_COLOR, render/color.h). */
void bz_draw_lines(vp_dvg *d, const bz_config *c, const uint8_t *mem, const bz_line *lines, int count);

/* A text of the game (a BZ_TAG_TEXT tag) that bz_draw_lines is to leave
 * out, so that another can be drawn in its place: box gets where it was
 * (left, bottom, right, top in screen units; left > right if it was not
 * in the picture). tag 0: none. */
void bz_draw_leave_out(uint16_t tag, float box[4]);

/* What bz_draw_lines is to use for the colors of the enhancements
 * (render/enhance.h) and the game's clock they are colored by; NULL: none. */
void bz_draw_enhance(const bz_enhance *e, double now);

#endif
