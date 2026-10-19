/* avg.h - Battlezone's analog vector generator, as a display-list
 * interpreter.
 *
 * Instruction set (VGMC.MAC, the program's own VG macros; 16-bit words, low
 * byte first as the 6502 writes them; the list starts at 2000):
 *
 *   000 VCTR  two words: DY (13-bit signed), then Z<<13 | DX (13-bit signed)
 *   001 HALT
 *   010 SVEC  one word: DY/2 in bits 12-8, Z in 7-5, DX/2 in 4-0 (5-bit signed)
 *   011 STAT  bit 12 clear: intensity in bits 7-4; if bit 10 is clear too, the
 *             beam's place becomes a corner of the window outside which
 *             nothing is drawn: bit 9 clear the lower left one, set the
 *             upper right one
 *       SCAL  bit 12 set: binary scale in bits 10-8 (n = 1/2^n), linear
 *             scale in 7-0 (0 full size, FF = 1/256)
 *   100 CNTR  beam to the center
 *   101 JSRL  call word address (bits 12-0, from 2000), 4-deep stack
 *   110 RTSL
 *   111 JMPL
 *
 * Z (0..7) is the vector's brightness: 0 draws nothing (a move), 1 means
 * "as STAT set it" (0..15), anything else is twice itself.
 *
 * The arithmetic is that of MAME's avg_bzone_device (src/devices/video/
 * avgdvg.cpp, BSD-3-Clause, Mathis Rosenhauer), so that both draw to the
 * same places: the deltas are normalized as the board does it (which
 * drops their lowest bits) and the beam's position is kept as 16.16 fixed
 * point. Positions come out in screen units with the center at 0,0, x to
 * the right, y up; MAME shows -290..290 by -200..200 of them. The program
 * draws at the binary scale 1, where its coordinates (-512..511) are
 * halved.
 */
#ifndef BZ_AVG_H
#define BZ_AVG_H

#include <stdint.h>

typedef struct bz_line {
    float    x0, y0, x1, y1;
    uint8_t  intensity;     /* 1..15 */
    uint8_t  z;             /* the vector's own 1..7 */
    uint16_t src;           /* address of the instruction of the list itself (in vector RAM) that drew it */
    uint16_t sub;           /* the vector ROM routine that instruction called, or 0 */
    uint16_t n;             /* the vector's number within that instruction (moves counted too) */
    uint16_t tag;           /* what it belongs to (core/tags.h), if the list's tags were given */
    uint16_t ord;           /* the vector's number among those the list itself draws under this tag (sub == 0) */
    float    win[4];        /* the window it is to be cut to: left, bottom, right, top */
    /* From the list's notes (bz_machine.vgnote), for render/solid.h: */
    float    w0, w1;        /* 1 / the distance ahead at each end; 0: none known (as far as can be) */
    uint8_t  p0, p1;        /* the object's point at each end, plus one; 0: none */
    uint8_t  model;         /* the object's number (OBJTBL) */
    float    gain;          /* the renderer's own lines: their brightness as a part of `intensity`'s; 0: all of it */
} bz_line;

enum bz_avg_status {
    BZ_AVG_OK = 0,          /* reached HALT */
    BZ_AVG_LIMIT,           /* instruction limit: a runaway list */
};

typedef struct bz_avg_list {
    bz_line *lines;
    int      count;
    int      cap;
    int      instructions;  /* executed by the last run */
    int      status;
    int      clipped;       /* lines the window cut or removed */
} bz_avg_list;

void bz_avg_list_free(bz_avg_list *l);

/* Run the display list at `start` (normally 2000) of the machine's memory
 * image (0000-7FFF) until HALT. Lines are appended to out (cleared first),
 * already cut to the window the list sets. max_instructions <= 0 uses a
 * default. Returns out->status. */
int bz_avg_run(const uint8_t *mem, uint16_t start, bz_avg_list *out, int max_instructions);

/* The same with the lines whole, not cut: each carries the window it is
 * to be cut to (bz_line_clip), and, if tags is not NULL (one per word of
 * vector RAM, 0x800 of them: bz_machine.vgtag), what it belongs to. For
 * moving the picture between two frames (render/smooth.h). */
int bz_avg_run_raw(const uint8_t *mem, uint16_t start, const uint16_t *tags, const uint32_t *notes,
                   bz_avg_list *out, int max_instructions);

/* The unrounded places of the objects' points (bz_machine.vgqx, vgqy: side
 * and height over distance) for the next bz_avg_run_raw calls: a vector
 * with a note then ends where its point is, not at the whole number the
 * game drew it to. NULL, NULL: as the game drew it. */
void bz_avg_exact(const float *qx, const float *qy);

/* Cut a line to its window. Returns 0 if nothing of it is left. */
int bz_line_clip(bz_line *l);

/* Room for at least n lines. Returns 0, or -1 out of memory. */
int bz_avg_list_reserve(bz_avg_list *l, int n);

/* How long the board takes to draw the list, in cycles of the 12.096 MHz
 * master clock, until HALT (bit 6 of IN0 tells the program). */
uint32_t bz_avg_cycles(const uint8_t *mem, uint16_t start);

#endif
