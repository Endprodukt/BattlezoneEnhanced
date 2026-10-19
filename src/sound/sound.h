/* sound.h - Battlezone's sound: the POKEY and the discrete circuits.
 *
 * The program writes the POKEY's registers (1820-182F: the tunes on
 * channels 1 and 2, the missile's buzz on 3 and 4) and one latch (1840):
 *
 *   D7 motor enable         the engine sound
 *   D6 start LED
 *   D5 sound enable         ALL sound outputs, the POKEY's included
 *   D4 engine rev enable    the engine's speed
 *   D3 shell loud, soft/    D2 shell enable
 *   D1 explosion loud       D0 explosion enable
 *
 * The discrete sounds are modelled after the board's circuits as MAME's
 * netlist of them describes them (src/mame/atari/bzone_a.cpp, BSD-3-Clause,
 * Couriersud; values of the parts from there):
 *   noise      a 16-bit shift register clocked at 6 kHz
 *   shell      the noise halved, gated onto a capacitor's decaying charge
 *              (0.1 s), amplified more (loud) or less, low-passed
 *   explosion  the same from a slower noise, decaying over 0.23 s
 *   engine     a 555 whose pitch follows a control voltage that rises
 *              slowly and falls fast with "engine rev"; two counters
 *              divide it by 12 and by 10, their outputs mixed
 *
 * The machine's sound_write hook delivers the writes; bz_sound_run makes
 * the samples of the time that has passed, into a ring the sound device
 * reads from another thread.
 *
 * The cabinet has one speaker. Here the output is stereo, and three
 * options (bz_sound_place, bz_sound_throttle) add what one speaker cannot
 * do: the enemy's shot, his explosion and the missile's buzz come from the
 * side he is on, the saucer's sounds from its side; both are quieter the
 * farther away they are; and with analog controls the engine revs with how
 * far the levers are pushed, not just up or down. With all three off both
 * channels carry the cabinet's one.
 */
#ifndef BZ_SOUND_H
#define BZ_SOUND_H

#include <stdint.h>

#include "sound/pokey.h"
#include "sound/samples.h"

#define BZ_SOUND_RATE 48000
#define BZ_SOUND_RING 16384     /* a third of a second, in pairs of left and right */

typedef struct bz_sound {
    pokey    pk;
    uint8_t  latch;

    /* noise */
    uint32_t lfsr;
    double   noise_t;           /* time into the 6 kHz clock's period */
    uint8_t  noise_out, div_shell, nand_out, div_explo;
    /* shell and explosion: the capacitor, and the filter's output */
    double   cap_shell, cap_explo, flt_shell, flt_explo;
    /* engine */
    double   cv;                /* the 555's control voltage */
    double   vco_t;             /* time into the 555's period */
    uint8_t  cnt12, cnt10;
    double   eng_lp;
    /* the output's coupling capacitor (takes the DC away) */
    double   dc, dc_r;          /* (left and right) */

    double   pokey_lp[3];       /* the POKEY's output behind its RC: what is in the middle, the enemy's, the saucer's */
    double   tick_frac;         /* POKEY clock ticks owed */
    double   frac;              /* samples owed */
    float    volume;            /* 0..1 */

    /* Recordings in place of the board's own sounds (sound/samples.h). */
    bz_samples smp;
    uint8_t  tune_replaced;     /* a recording plays for the tune the POKEY is at: its channels 1 and 2 are unheard */
    uint8_t  shell_replaced, explo_replaced;    /* the same for the burst going on */
    uint8_t  test;              /* 1: the test menu is open, only what it asks for is heard */
    float    peak;              /* the loudest sample made (0..1, 1: clipped), for the log */

    /* Where things are heard (bz_sound_place): -1 left .. 1 right, and how loud (0..1). */
    float    enemy_pan, enemy_gain, saucer_pan, saucer_gain;
    float    enemy_boom;        /* the enemy's explosion: softer far away too, but less (it still has to crash) */
    int      tune;              /* the tune the POKEY is at (BZ_SMP_xxx), -1: none */
    float    throttle;          /* < 0: the engine revs by the board's switch; else 0..1, how far */

    /* The enemy tank's engine (an addition, recordings only): */
    uint8_t  enemy_engine;      /* 1: heard (the option enh_enemy_engine) */
    float    enemy_hum;         /* how loud by his distance: falls off sooner than his shot */
    float    enemy_level;       /* 0..1: faded in while he lives, out when he is gone */
    float    enemy_rev;         /* 0..1: idling .. driving */
    uint16_t enemy_last[2];     /* where he was, to see him move */
    int      enemy_still;       /* interrupts since he last moved */

    /* Ducking (an addition, the option enh_ducking): the engines, the
     * player's and the enemy's, a little softer while a tune (the radar's
     * beep, the warning ...), a shot or an explosion is heard. */
    uint8_t  ducking;           /* 1: on */
    double   duck;              /* the engines' gain now: 1, or down to DUCK_TO */

    int16_t  ring[BZ_SOUND_RING * 2];
    volatile uint32_t ring_w, ring_r;
} bz_sound;

void bz_sound_init(bz_sound *s);

/* A write to 1820-182F or 1840 (bz_machine.sound_write, user = the bz_sound). */
void bz_sound_write(void *user, uint16_t addr, uint8_t value);

/* SNDON has started a tune (bz_machine.sound_request, user = the bz_sound;
 * a: the tune's bit). */
void bz_sound_request(void *user, uint8_t a);

/* The game's tune has run out (POINT is 0 in all four places): the POKEY
 * is heard again. tune_running: 0 if so. Call once an interrupt. */
void bz_sound_tune(bz_sound *s, int tune_running);

/* Where the enemy and the saucer are from the player, out of the game's
 * memory (bz_machine.mem): for the stereo picture and for how loud they
 * are. stereo, distance: 0 switches each off (everything in the middle,
 * everything at its full level). Call once a frame or an interrupt. */
void bz_sound_place(bz_sound *s, const uint8_t *mem, int stereo, int distance);

/* The engine by how far the levers are pushed (0..1), or by the board's
 * switch alone (a negative value). */
void bz_sound_throttle(bz_sound *s, float amount);

/* Advance by cpu_cycles of the 1.512 MHz clock: the samples go into the ring. */
void bz_sound_run(bz_sound *s, int cpu_cycles);

/* Take up to n pairs of samples (left, right) out of the ring (another
 * thread may do this): out holds 2 n. Returns how many pairs there were. */
int bz_sound_read(bz_sound *s, int16_t *out, int n);

#endif
