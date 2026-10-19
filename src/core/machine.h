/* machine.h - the Battlezone board, as the program sees it.
 *
 * The port keeps the original memory layout: RAM at 0000-03FF, vector RAM
 * at 2000-2FFF, the ROMs at 3000-7FFF, every variable at the address the
 * source gives it (core/bz_symbols.h). The game's state can then be
 * compared byte for byte with a MAME recording, and the 1980 source reads
 * one to one against the C.
 *
 *   0800       IN0: coins, slam, self test, diagnostic step, VG halted, 3 kHz
 *   0A00 0C00  option switches
 *   1000       coin counters      1200 VG go    1400 watchdog   1600 VG reset
 *   1800       math box status    1810 1818 its result, low and high
 *   1820-182F  POKEY: the tunes; reg 8 read is the controls, reg A random
 *   1840       the latch of the discrete sounds (engine, shell, explosion)
 *   1860-187F  math box
 */
#ifndef BZ_MACHINE_H
#define BZ_MACHINE_H

#include <stdint.h>

#include "core/bz_symbols.h"
#include "core/tags.h"
#include "mbox/mathbox.h"

/* IN0 (0800), as the switches present it: coins, self test and diagnostic
 * step are active LOW. */
#define BZ_IN0_COIN_R  0x01    /* right mech (MAME: Coin 1) */
#define BZ_IN0_COIN_M  0x02    /* middle mech (MAME: Coin 2) */
#define BZ_IN0_COIN_L  0x04    /* left mech */
#define BZ_IN0_SLAM    0x08
#define BZ_IN0_TEST    0x10    /* self test switch */
#define BZ_IN0_DIAG    0x20    /* diagnostic step */
#define BZ_IN0_HALT    0x40    /* the vector generator has halted (active high) */
#define BZ_IN0_3KHZ    0x80

/* The controls (POKEY ALLPOT, 1828), active HIGH. */
#define BZ_IN3_RIGHT_BACK 0x01
#define BZ_IN3_RIGHT_FWD  0x02
#define BZ_IN3_LEFT_BACK  0x04
#define BZ_IN3_LEFT_FWD   0x08
#define BZ_IN3_FIRE       0x10
#define BZ_IN3_START      0x20
#define BZ_IN3_START2     0x40

/* The sound latch (1840). */
#define BZ_SND_EXPLO      0x01  /* explosion enable */
#define BZ_SND_EXPLO_LOUD 0x02
#define BZ_SND_SHELL      0x04  /* shell enable */
#define BZ_SND_SHELL_LOUD 0x08
#define BZ_SND_ENGINE_REV 0x10
#define BZ_SND_ENABLE     0x20  /* all sound, the POKEY included */
#define BZ_SND_START_LED  0x40
#define BZ_SND_MOTOR      0x80  /* engine sound enable */

typedef struct bz_inputs {
    uint8_t in0;     /* D0-D5 of 0800 */
    uint8_t dsw0;    /* 0A00 (OPTION): lives, missile level, bonus tank, language */
    uint8_t dsw1;    /* 0C00 (OPTON2): the coin mode */
    uint8_t in3;     /* the controls */
} bz_inputs;

/* The 6502's registers and flags, for the C routines (game/m6502.h): they
 * pass values in A, X, Y and the carry just as the original does. */
typedef struct bz_regs {
    uint8_t a, x, y;
    uint8_t c, z, n, v;    /* each 0 or 1 */
    uint8_t d;             /* decimal mode */
} bz_regs;

typedef struct bz_machine bz_machine;

typedef uint8_t (*bz_prng_fn)(void *user);
/* A read of a watched address: gets the value the machine would give and
 * returns the value to use (checking against a recording). */
typedef uint8_t (*bz_read_fn)(void *user, uint16_t addr, uint8_t value);
/* A write to the POKEY (1820-182F) or the sound latch (1840). */
typedef void (*bz_sound_fn)(void *user, uint16_t addr, uint8_t value);

struct bz_machine {
    uint8_t    mem[0x8000];     /* RAM 0000-03FF, vector RAM 2000-2FFF, ROM 3000-7FFF */
    int        have_rom;
    bz_inputs  in;
    bz_mathbox mb;

    uint8_t    pokey[16];       /* the POKEY's registers as last written */
    uint8_t    sound_latch;     /* 1840 */
    uint8_t    coin_counter;    /* 1000 */
    uint32_t   vg_starts;       /* writes to GOADD: a new picture for the renderer */
    uint32_t   vg_left;         /* master clock cycles until the vector generator halts; 0: halted */
    uint64_t   cpu_cycles;      /* for the 3 kHz bit of IN0 (the 6502 core counts them) */
    uint32_t   watchdog;        /* writes to WTCHDG */

    bz_prng_fn prng;            /* NULL: the machine's own LFSR */
    void      *prng_user;
    uint32_t   lfsr;

    bz_read_fn read_hook;       /* NULL: none */
    void      *read_user;
    uint8_t    watch[0x2000];   /* 1: reads of this address pass through read_hook */

    bz_sound_fn sound_write;    /* NULL: nobody listens */
    /* SNDON starts one of the POKEY's tunes (a: its bit); for sound/samples.h. */
    void     (*sound_request)(void *user, uint8_t a);
    void      *sound_user;

    /* --- for the renderer only: notes the C routines make next to what
     * they write (core/tags.h). They are no part of the original's memory
     * and change nothing the game computes. --- */
    uint16_t   vgtag[0x800];    /* what each word of vector RAM belongs to */
    uint16_t   tag;             /* the tag being written now */
    /* For render/solid.h: which point of which object a vector goes to and
     * how far away that point is: object number << 24, the point's number
     * plus one << 16, its distance ahead (the math box's X'). 0: nothing. */
    uint32_t   vgnote[0x800];
    uint32_t   note;            /* the note being written now */
    int16_t    pt_depth[64];    /* PNTPUT: how far ahead each point of the object in PNTTBL is */
    /* The same points unrounded: what the math box divided, side over
     * distance and height over distance, before the quotient lost its
     * fraction. The game's screen positions are whole numbers, so a thing
     * far away stands still for some frames and then jumps a step; from
     * these the renderer draws it where it is (avg/avg.h). */
    float      pt_qx[64], pt_qy[64];
    float      vgqx[0x800], vgqy[0x800];    /* of the point each vector goes to */
    float      note_qx, note_qy;
    uint8_t    vis_id[32];      /* which thing each entry of PTBLO2 is (BZ_ID_xxx) */
    uint8_t    cur_id;          /* the thing ROTPNT is looking at */
    uint8_t    enemy_id;        /* what the enemy is (BZ_ID_TANK, SUPERTANK, MISSILE), as ROTATE last saw him */
    uint8_t    draw_only;       /* 1: a frame's picture only, without its step of the game (render/smooth.h) */
    /* 1: the picture is of a moment between two frames (render/smooth.h).
     * The game turns the view by an angle of 9 bits and the objects by one
     * of 8: fine for 15 pictures a second, steps for 50. With this set the
     * drawing routines take sine and cosine of all 16 bits of the angles
     * (TANGLE, LANGLE) instead of the tables' entries. Never set on the
     * machine the game runs on. */
    uint8_t    fine;
    uint8_t    vis_lo[32];      /* the low byte of the angle of each entry of PTBLO2 (the enemy's LANGLE; else 0) */
    /* the same of the angles the game keeps in 8 bits, set for such a picture by who makes it: */
    uint8_t    antenna_lo, saucer_lo, piece_lo[6];
    float      mount_shift;     /* MOUNTS: what the mountains' place lost by being a whole number (screen units) */

    /* The analog control mode (an addition: platform/controls.h). on: the
     * lever switches are not used; left and right say how far each lever
     * is pushed, -256 (full back) .. 256 (full forward). The tank does
     * what the cabinet's does with its levers at the ends, and everything
     * in between. rem: the parts of a step not yet driven. */
    struct { uint8_t on; int16_t left, right; int16_t rem_x, rem_y; } analog;

    /* --- the C routines (src/game) --- */
    bz_regs    r;
    uint8_t    in_nmi;          /* inside bz_nmi */
    uint8_t    main_state;      /* BZ_MAIN_xxx (game/mainline.c) */
    uint32_t   nmis;            /* interrupts run */
    uint32_t   frames;          /* game frames run */
    uint32_t   n_rnd;           /* random numbers read */
    uint32_t   n_mbox;          /* math box routines started */
    uint32_t   n_rd, n_wr;      /* bytes read and written through bz_rd8 / bz_wr8 (game/pace.h counts by them) */
};

void bz_machine_init(bz_machine *m);
/* The program and vector ROMs, as one image of 3000-7FFF (0x5000 bytes). */
void bz_machine_set_rom(bz_machine *m, const uint8_t *rom3000);

uint8_t bz_rd8(bz_machine *m, uint16_t addr);
void    bz_wr8(bz_machine *m, uint16_t addr, uint8_t v);

/* Little-endian words, as the 6502 keeps them. */
static inline uint16_t bz_peek16(const bz_machine *m, uint16_t addr)
{
    return (uint16_t)(m->mem[addr & 0x7fff] | (m->mem[(addr + 1) & 0x7fff] << 8));
}

#endif
