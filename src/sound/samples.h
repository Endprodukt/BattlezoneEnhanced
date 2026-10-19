/* samples.h - sounds of the game replaced by recordings (the setting
 * `sound_replace`).
 *
 * Every sound the game makes (docs/SOUNDS.md: 15 of them) can be replaced
 * by a WAV file named after it: sounds/SHELL_LOUD.wav is played whenever
 * the player fires, and the board's own sound for that is then not made.
 * Sounds without a file sound as they always did, so they can be replaced
 * one at a time. (After the Star Wars port's sound/samples.)
 *
 * The files: WAV, PCM, 8 or 16 bit, mono or stereo, any rate (they are
 * brought to the output's rate and to mono when loaded).
 *
 * How they play, after what the board does with its own:
 *   tunes      (the POKEY's eight, started by SNDON) one at a time: a new
 *              one, replaced or not, ends the one playing. The game starts
 *              some again and again while their reason lasts (the saucer,
 *              "motion blocked"): the recording starts again each time.
 *   shell, explosion   each starts when the board's switch for it closes,
 *              the loud or the soft one by the board's "loud" switch
 *   missile    a loop, for as long as the POKEY's channels 3 and 4 sound,
 *              as loud as they are
 *   engine     a loop while the motor is on. ENGINE_IDLE alone is played
 *              faster as the engine revs up; with ENGINE_REV as well the
 *              two are faded into each other.
 *
 * One more that the board does not have (the option enh_enemy_engine):
 *   enemy engine   ENEMY_ENGINE, a loop while an enemy tank lives, at his
 *              place, faster while he drives. Without the file the
 *              player's engine loops are taken, lower.
 */
#ifndef BZ_SAMPLES_H
#define BZ_SAMPLES_H

#include <stdint.h>

enum {
    /* the tunes, in the order of SNDON's bits */
    BZ_SMP_RBEEP = 0, BZ_SMP_BOING, BZ_SMP_BLKSND, BZ_SMP_BONER,
    BZ_SMP_WARNG, BZ_SMP_DISINT, BZ_SMP_SAUSND, BZ_SMP_SUPBON,
    BZ_SMP_MISSILE,
    BZ_SMP_SHELL_LOUD, BZ_SMP_SHELL_SOFT, BZ_SMP_EXPLO_LOUD, BZ_SMP_EXPLO_SOFT,
    BZ_SMP_ENGINE_IDLE, BZ_SMP_ENGINE_REV,
    BZ_SMP_ENEMY_ENGINE,        /* (not the board's: an addition) */
    BZ_SMP_COUNT
};

typedef struct bz_samples {
    int      enabled;
    struct {
        int16_t *pcm;               /* mono, at the output's rate; NULL: not replaced */
        uint32_t n;
        uint32_t played;            /* how often it was started */
    } sound[BZ_SMP_COUNT];
    struct {
        int      id;                /* the sound playing, -1: none */
        uint32_t at;
    } tune, shell, explo, test;
    double   missile_at, engine_at[2], enemy_at[2];
} bz_samples;

/* The sound's name, as its file is called ("SHELL_LOUD"). */
const char *bz_sample_name(int id);

/* Zero the struct first (bz_samples_init). Loads DIR/NAME.wav for every
 * sound that has one and is not loaded yet, at `rate` samples a second.
 * Returns how many were found. */
void bz_samples_init(bz_samples *s);
int  bz_samples_load(bz_samples *s, const char *dir, int rate);
void bz_samples_free(bz_samples *s);

/* Is there a recording for the sound, and are recordings on? */
int  bz_samples_has(const bz_samples *s, int id);

/* Start one of the one-shot sounds. voice: 0 tune, 1 shell, 2 explosion,
 * 3 the test menu's. id -1 stops the voice. */
void bz_samples_start(bz_samples *s, int voice, int id);

/* The next sample of a one-shot voice (0 tune, 1 shell, 2 explosion, 3
 * test), 16 bit range; what the voice is playing is in its id until it ends. */
int  bz_samples_voice(bz_samples *s, int voice);

/* The next sample of a loop at `rate` times its own speed; *at: where in
 * the loop it is. */
double bz_samples_loop(const bz_samples *s, int id, double *at, double rate);

#endif
