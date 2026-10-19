#include "sound/sound.h"

#include "core/bz_symbols.h"
#include "core/machine.h"

#include <math.h>
#include <string.h>

#define CPU_HZ   1512000.0
#define DT       (1.0 / BZ_SOUND_RATE)

/* Parts (bzone_a.cpp) */
#define R_ON     270.0          /* a closed CD4066 switch */
#define R5       1e3
#define R6       4.7e3
#define R7       1e3
#define R8       100e3
#define R9       22e3
#define R10      100e3
#define R11      49e3           /* the engine frequency pot at MAME's default (40 % from 75k to 10k) */
#define R12      33e3
#define R13      10e3
#define R14      22e3
#define R15      1e3
#define R32      330e3
#define C9       4.7e-6
#define C11      0.015e-6
#define C13      10e-6
#define C14      10e-6
#define C21      0.0047e-6
#define C29      0.47e-6
#define VP       20.5           /* the op-amps' rail less what they cannot reach */

static double par(double a, double b) { return a * b / (a + b); }
static double rc_exp(double rc) { return 1.0 - exp(-DT / rc); }

void bz_sound_init(bz_sound *s)
{
    memset(s, 0, sizeof *s);
    pokey_reset(&s->pk);
    s->lfsr = 0;
    s->volume = 1.0f;
    bz_samples_init(&s->smp);
    s->enemy_gain = s->saucer_gain = s->enemy_boom = 1.0f;
    s->duck = 1.0;
    s->tune = -1;
    s->throttle = -1.0f;
    s->cv = 5.0 * R6 / (R7 + R6);
}

void bz_sound_write(void *user, uint16_t addr, uint8_t value)
{
    bz_sound *s = (bz_sound *)user;
    if (addr == BZ_SOUND) {
        /* A burst begins as its switch closes: the recording for it, if
         * there is one, instead of the circuit's noise. */
        uint8_t on = (uint8_t)(value & ~s->latch);
        if (on & BZ_SND_SHELL) {
            int id = (value & BZ_SND_SHELL_LOUD) ? BZ_SMP_SHELL_LOUD : BZ_SMP_SHELL_SOFT;
            s->shell_replaced = (uint8_t)bz_samples_has(&s->smp, id);
            if (s->shell_replaced) bz_samples_start(&s->smp, 1, id);
        }
        if (on & BZ_SND_EXPLO) {
            int id = (value & BZ_SND_EXPLO_LOUD) ? BZ_SMP_EXPLO_LOUD : BZ_SMP_EXPLO_SOFT;
            s->explo_replaced = (uint8_t)bz_samples_has(&s->smp, id);
            if (s->explo_replaced) bz_samples_start(&s->smp, 2, id);
        }
        s->latch = value;
    }
    else if (addr >= BZ_POKEY && addr < BZ_POKEY + 0x10) pokey_write(&s->pk, addr - BZ_POKEY, value);
}

void bz_sound_request(void *user, uint8_t a)
{
    bz_sound *s = (bz_sound *)user;
    int id = -1;
    for (int i = 0; i < 8; i++)
        if (a >> i & 1) id = i;                 /* (SNDON takes the highest bit) */
    /* A new tune ends the one playing, replaced or not. */
    s->tune = id;
    s->tune_replaced = (uint8_t)(id >= 0 && bz_samples_has(&s->smp, id));
    bz_samples_start(&s->smp, 0, s->tune_replaced ? id : -1);
}

void bz_sound_tune(bz_sound *s, int tune_running)
{
    if (!tune_running) {
        s->tune_replaced = 0;
        s->tune = -1;
    }
}

void bz_sound_throttle(bz_sound *s, float amount)
{
    s->throttle = amount > 1.0f ? 1.0f : amount;
}

static double wrap16(double v)
{
    v = fmod(v, 65536.0);
    if (v >= 32768.0) v -= 65536.0;
    if (v < -32768.0) v += 65536.0;
    return v;
}

#define DUCK_TO 0.55        /* the engines under an effect: 5 dB softer */
/* With the recordings, the POKEY's tunes (the radar's beep, the warning,
 * the bell, the fanfare) 11 dB up: on the board they are 14 dB under its
 * engine and heard over its low hum, but the recorded engines and tracks
 * fill the middle where they are. (Not the missile's buzz, channels 3 and 4.) */
#define TUNE_BOOST 3.5

/* Where a place of the playfield is heard from the player's. */
static void hear(const uint8_t *mem, unsigned x_at, unsigned y_at, int stereo, int distance, float *pan, float *gain,
                 float *boom, float *hum)
{
    double a = (mem[BZ_TANGLE] << 8 | mem[BZ_LANGLE]) / 65536.0 * 6.283185307;
    double dx = wrap16((double)(mem[x_at] | mem[x_at + 1] << 8) - (double)(mem[BZ_TPOSX] | mem[BZ_TPOSX + 1] << 8));
    double dy = wrap16((double)(mem[y_at] | mem[y_at + 1] << 8) - (double)(mem[BZ_TPOSY] | mem[BZ_TPOSY + 1] << 8));
    double side = -dx * sin(a) + dy * cos(a), far = hypot(dx, dy);     /* (side: to the left) */
    *pan = stereo && far > 1.0 ? (float)(-side / far) : 0.0f;
    double g = 1.15 - far / 40000.0;
    *gain = distance ? (float)(g > 1.0 ? 1.0 : g < 0.3 ? 0.3 : g) : 1.0f;
    double b = 1.1 - far / 60000.0;                 /* (at the radar's edge still more than half) */
    if (boom) *boom = distance ? (float)(b > 1.0 ? 1.0 : b < 0.55 ? 0.55 : b) : 1.0f;
    double h = 1.15 - far / 22000.0;                /* (an engine at the radar's edge is all but gone) */
    if (hum) *hum = distance ? (float)(h > 1.0 ? 1.0 : h < 0.06 ? 0.06 : h) : 0.7f;
}

void bz_sound_place(bz_sound *s, const uint8_t *mem, int stereo, int distance)
{
    hear(mem, BZ_TPOSX + 2, BZ_TPOSY + 2, stereo, distance, &s->enemy_pan, &s->enemy_gain, &s->enemy_boom, &s->enemy_hum);
    hear(mem, BZ_SAPOSX, BZ_SAPOSX + 2, stereo, distance, &s->saucer_pan, &s->saucer_gain, NULL, NULL);

    /* The enemy's engine: a tank (not the missile, it has its buzz) that
     * is not blown up (COLFLG+2 is 0 from TANKCK until he is hit). He
     * drives when his place changes; the game moves him once a frame,
     * every fourth or fifth interrupt, so still means still for a while. */
    uint16_t x = (uint16_t)(mem[BZ_TPOSX + 2] | mem[BZ_TPOSX + 3] << 8);
    uint16_t y = (uint16_t)(mem[BZ_TPOSY + 2] | mem[BZ_TPOSY + 3] << 8);
    if (x != s->enemy_last[0] || y != s->enemy_last[1]) s->enemy_still = 0;
    else if (s->enemy_still < 1000) s->enemy_still++;
    s->enemy_last[0] = x;
    s->enemy_last[1] = y;
    int alive = s->enemy_engine && mem[BZ_COLFLG + 2] == 0 && !(mem[BZ_R2D3FL] & 0x80);
    float driving = s->enemy_still < 12 ? 1.0f : 0.0f;
    s->enemy_level += ((alive ? 1.0f : 0.0f) - s->enemy_level) * 0.03f;     /* (a quarter of a second) */
    s->enemy_rev += (driving - s->enemy_rev) * 0.015f;                      /* (half a second to rev up or down) */
}

/* The noise generator: 16 bits, the inverted XOR of bits 3 and 14 shifted
 * in at bit 0, clocked at 6 kHz. The shell's noise is bit 15 halved; the
 * explosion's is the NAND of bits 11-14, halved. */
static void noise_clock(bz_sound *s)
{
    unsigned fb = (((s->lfsr >> 3) ^ (s->lfsr >> 14)) & 1) ^ 1;
    s->lfsr = ((s->lfsr << 1) | fb) & 0xffff;
    uint8_t out = (uint8_t)((s->lfsr >> 15) & 1);
    if (out && !s->noise_out) s->div_shell ^= 1;
    s->noise_out = out;
    uint8_t nand = (uint8_t)(((s->lfsr >> 11) & 0x0f) != 0x0f);
    if (nand && !s->nand_out) s->div_explo ^= 1;
    s->nand_out = nand;
}

/* A capacitor charged through a closed switch while the sound is enabled
 * and discharged through r by the noise when it is not; what comes out is
 * its voltage whenever the noise is high. */
static double burst(double *cap, int enable, int noise, double r, double c)
{
    if (enable) {
        *cap += (5.0 - *cap) * rc_exp(par(R_ON, r) * c);
        return noise ? *cap : 0.0;
    }
    if (!noise) return 0.0;
    *cap -= *cap * rc_exp(r * c);
    return *cap;
}

/* The amplifier after it: more gain with the "loud" switch closed, and a
 * low-pass. */
static double amp(double *out, double in, int loud)
{
    double gain = R32 / ((loud ? par(R_ON, R12) : R12) + R13) + 1.0;
    double v = in * (R15 / (R14 + R15)) * gain;
    if (v > VP) v = VP;
    if (v < 0.0) v = 0.0;
    *out += (v - *out) * rc_exp(R32 * C21);
    return *out;
}

static double engine(bz_sound *s)
{
    /* The control voltage: 4.1 V idling, 2.5 V with "engine rev"; it goes
     * up through R8 and down, faster, through R8 and R9 side by side. */
    double idle = 5.0 * R6 / (R7 + R6), rev = 5.0 * par(R_ON + R5, R6) / (R7 + par(R_ON + R5, R6));
    double target = (s->latch & BZ_SND_ENGINE_REV) ? rev : idle;
    if (s->throttle >= 0.0f) target = idle + (rev - idle) * s->throttle;     /* (an addition: anywhere between) */
    if (target > s->cv) s->cv += (target - s->cv) * rc_exp(R8 * C13);
    else s->cv += (target - s->cv) * rc_exp(par(R8, R9) * C13);

    /* The 555: its capacitor swings between half the control voltage and
     * the control voltage. */
    double cv = s->cv < 0.5 ? 0.5 : (s->cv > 4.9 ? 4.9 : s->cv);
    double period = (R10 + R11) * C11 * log((5.0 - cv / 2.0) / (5.0 - cv)) + R11 * C11 * log(2.0);
    if (!(s->latch & BZ_SND_MOTOR)) {
        s->cnt12 = 4;           /* the counters are held at their start */
        s->cnt10 = 6;
    } else {
        s->vco_t += DT;
        while (s->vco_t >= period) {
            s->vco_t -= period;
            s->cnt12 = (uint8_t)(s->cnt12 >= 15 ? 4 : s->cnt12 + 1);
            s->cnt10 = (uint8_t)(s->cnt10 >= 15 ? 6 : s->cnt10 + 1);
        }
    }
    /* QD and the ripple carry of each counter, through four equal
     * resistors onto C29. */
    double v = 3.4 * ((s->cnt12 > 7) + (s->cnt12 == 15) + (s->cnt10 > 7) + (s->cnt10 == 15)) / 4.0;
    s->eng_lp += (v - s->eng_lp) * rc_exp((33e3 / 4.0) * C29);
    return s->eng_lp;
}

/* A sound that is heard at `pan` (-1 left .. 1 right) with this gain, added
 * to the two channels. In the middle both have all of it. */
static void place(double v, float pan, float gain, double *l, double *r)
{
    v *= gain;
    *l += pan > 0.0f ? v * (1.0 - 0.85 * pan) : v;
    *r += pan < 0.0f ? v * (1.0 + 0.85 * pan) : v;
}

void bz_sound_run(bz_sound *s, int cpu_cycles)
{
    s->frac += (double)cpu_cycles * BZ_SOUND_RATE / CPU_HZ;
    int n = (int)s->frac;
    s->frac -= n;
    if (n <= 0) return;
    /* The POKEY's output has an RC of 10k and 0.015 uF behind it. */

    for (int i = 0; i < n; i++) {
        s->noise_t += DT;
        while (s->noise_t >= 1.0 / 6000.0) {
            s->noise_t -= 1.0 / 6000.0;
            noise_clock(s);
        }
        double shell = amp(&s->flt_shell, burst(&s->cap_shell, s->latch & BZ_SND_SHELL, s->div_shell, R14 + R15, C9),
                           s->latch & BZ_SND_SHELL_LOUD);
        double explo = amp(&s->flt_explo, burst(&s->cap_explo, s->latch & BZ_SND_EXPLO, s->div_explo, R14 + R15, C14),
                           s->latch & BZ_SND_EXPLO_LOUD);
        double eng = engine(s);
        if (s->shell_replaced) shell = 0.0;
        if (s->explo_replaced) explo = 0.0;

        /* Whose sound is whose: the soft shot is the enemy's and the soft
         * explosion is at his place; the saucer has two of the tunes. */
        int enemy_shell = !(s->latch & BZ_SND_SHELL_LOUD), enemy_explo = !(s->latch & BZ_SND_EXPLO_LOUD);
        int saucer_tune = s->tune == BZ_SMP_SAUSND || s->tune == BZ_SMP_DISINT;

        /* Ducking: down in 15 ms while an effect sounds, up again in 0.3 s. */
        int effect = s->tune >= 0 || s->smp.shell.id >= 0 || s->smp.explo.id >= 0 ||
                     (!s->shell_replaced && (s->latch & BZ_SND_SHELL)) || (!s->explo_replaced && (s->latch & BZ_SND_EXPLO));
        double duck_to = s->ducking && s->smp.enabled && effect ? DUCK_TO : 1.0;     /* (the original sounds: never) */
        s->duck += (duck_to - s->duck) * (duck_to < s->duck ? 1.0 / (0.015 * BZ_SOUND_RATE) : 1.0 / (0.3 * BZ_SOUND_RATE));
        eng *= s->duck;

        /* The recordings, -1..1, by where they are heard. */
        double rec_l = 0.0, rec_r = 0.0;
        if (bz_samples_has(&s->smp, BZ_SMP_ENGINE_IDLE)) {
            /* how far the engine has revved up: the 555's control voltage
             * between its two ends (4.1 V idling, 2.5 V) */
            double k = (4.12 - s->cv) / (4.12 - 2.5), v = 0.0;
            k = k < 0.0 ? 0.0 : k > 1.0 ? 1.0 : k;
            eng = 0.0;
            if (s->latch & BZ_SND_MOTOR) {
                if (bz_samples_has(&s->smp, BZ_SMP_ENGINE_REV))
                    v = (bz_samples_loop(&s->smp, BZ_SMP_ENGINE_IDLE, &s->smp.engine_at[0], 1.0) * (1.0 - k) +
                         bz_samples_loop(&s->smp, BZ_SMP_ENGINE_REV, &s->smp.engine_at[1], 1.0) * k) / 32768.0;
                else
                    v = bz_samples_loop(&s->smp, BZ_SMP_ENGINE_IDLE, &s->smp.engine_at[0], 1.0 + 0.6 * k) / 32768.0;
            }
            place(v * s->duck, 0.0f, 1.0f, &rec_l, &rec_r);
        }
        /* The enemy tank's engine, at his place. */
        if (s->enemy_level > 0.001f) {
            double k = s->enemy_rev, v = 0.0;
            if (bz_samples_has(&s->smp, BZ_SMP_ENEMY_ENGINE))
                v = bz_samples_loop(&s->smp, BZ_SMP_ENEMY_ENGINE, &s->smp.enemy_at[0], 0.9 + 0.25 * k);
            else if (bz_samples_has(&s->smp, BZ_SMP_ENGINE_REV))      /* (the player's engine, lower) */
                v = bz_samples_loop(&s->smp, BZ_SMP_ENGINE_IDLE, &s->smp.enemy_at[0], 0.82) * (1.0 - k) +
                    bz_samples_loop(&s->smp, BZ_SMP_ENGINE_REV, &s->smp.enemy_at[1], 0.82) * k;
            else if (bz_samples_has(&s->smp, BZ_SMP_ENGINE_IDLE))
                v = bz_samples_loop(&s->smp, BZ_SMP_ENGINE_IDLE, &s->smp.enemy_at[0], 0.8 + 0.4 * k);
            place(v / 32768.0 * s->enemy_level * s->duck, s->enemy_pan, s->enemy_hum, &rec_l, &rec_r);
        }
        /* The missile's buzz: the POKEY's channels 3 and 4. */
        s->pk.mute = s->tune_replaced ? 0x03 : 0x00;
        if (bz_samples_has(&s->smp, BZ_SMP_MISSILE)) {
            unsigned vol = s->pk.audc[2] & 0x0f, vol4 = s->pk.audc[3] & 0x0f;
            if (vol4 > vol) vol = vol4;
            s->pk.mute |= 0x0c;
            if (vol)
                place(bz_samples_loop(&s->smp, BZ_SMP_MISSILE, &s->smp.missile_at, 1.0) / 32768.0 * (vol > 8 ? 1.0 : vol / 8.0),
                      s->enemy_pan, s->enemy_gain, &rec_l, &rec_r);
            else
                s->smp.missile_at = 0.0;
        }
        {
            int tune_saucer = s->smp.tune.id == BZ_SMP_SAUSND || s->smp.tune.id == BZ_SMP_DISINT;
            int shell_enemy = s->smp.shell.id == BZ_SMP_SHELL_SOFT, explo_enemy = s->smp.explo.id == BZ_SMP_EXPLO_SOFT;
            place(bz_samples_voice(&s->smp, 0) / 32768.0, tune_saucer ? s->saucer_pan : 0.0f,
                  tune_saucer ? s->saucer_gain : 1.0f, &rec_l, &rec_r);
            place(bz_samples_voice(&s->smp, 1) / 32768.0, shell_enemy ? s->enemy_pan : 0.0f,
                  shell_enemy ? s->enemy_gain : 1.0f, &rec_l, &rec_r);
            place(bz_samples_voice(&s->smp, 2) / 32768.0, explo_enemy ? s->enemy_pan : 0.0f,
                  explo_enemy ? s->enemy_boom : 1.0f, &rec_l, &rec_r);
        }
        if (s->test) rec_l = rec_r = 0.0;
        place(bz_samples_voice(&s->smp, 3) / 32768.0, 0.0f, 1.0f, &rec_l, &rec_r);

        s->tick_frac += CPU_HZ / BZ_SOUND_RATE;         /* 31.5 of the chip's clock ticks a sample */
        unsigned ticks = (unsigned)s->tick_frac;
        s->tick_frac -= ticks;
        /* channels 1 and 2 the tunes (the saucer's two by the saucer), 3 and 4 the missile */
        const uint8_t group[4] = { (uint8_t)(saucer_tune ? 2 : 0), (uint8_t)(saucer_tune ? 2 : 0), 1, 1 };
        pokey_run_rc(&s->pk, ticks, group, s->pokey_lp);

        /* The final mixer: each through its resistor (volts, about 0..7),
         * once for each ear. */
        double l = 0.0, r = 0.0;
        place(shell / 100e3 * 22e3, enemy_shell ? s->enemy_pan : 0.0f, enemy_shell ? s->enemy_gain : 1.0f, &l, &r);
        place(explo / 100e3 * 22e3, enemy_explo ? s->enemy_pan : 0.0f, enemy_explo ? s->enemy_boom : 1.0f, &l, &r);
        double tunes = s->smp.enabled ? TUNE_BOOST : 1.0;
        place((eng / 41.25e3 + s->pokey_lp[0] / 330e3 * tunes) * 22e3, 0.0f, 1.0f, &l, &r);
        place(s->pokey_lp[1] / 330e3 * 22e3, s->enemy_pan, s->enemy_gain, &l, &r);
        place(s->pokey_lp[2] / 330e3 * tunes * 22e3, s->saucer_pan, s->saucer_gain, &l, &r);
        s->dc += (l - s->dc) * 0.0005;                  /* the output capacitor */
        s->dc_r += (r - s->dc_r) * 0.0005;
        /* (half a volt of the mixer a tenth of full scale: as loud as MAME without its clipping) */
        double v[2] = { (l - s->dc) / 2.0 + rec_l * 0.5, (r - s->dc_r) / 2.0 + rec_r * 0.5 };
        if (!(s->latch & BZ_SND_ENABLE)) v[0] = v[1] = 0.0;
        if (s->test) { v[0] = rec_l * 0.5; v[1] = rec_r * 0.5; }

        uint32_t w = s->ring_w;
        if (w - s->ring_r < BZ_SOUND_RING) {
            for (int k = 0; k < 2; k++) {
                double x = v[k] * s->volume;
                if (fabs(x) > s->peak) s->peak = (float)(fabs(x) > 1.0 ? 1.0 : fabs(x));
                if (x > 1.0) x = 1.0;
                if (x < -1.0) x = -1.0;
                s->ring[(w % BZ_SOUND_RING) * 2 + (unsigned)k] = (int16_t)(x * 30000.0);
            }
            s->ring_w = w + 1;
        }
    }
}

int bz_sound_read(bz_sound *s, int16_t *out, int n)
{
    uint32_t r = s->ring_r, have = s->ring_w - r;
    if ((uint32_t)n > have) n = (int)have;
    for (int i = 0; i < n; i++) {
        out[2 * i] = s->ring[((r + (uint32_t)i) % BZ_SOUND_RING) * 2];
        out[2 * i + 1] = s->ring[((r + (uint32_t)i) % BZ_SOUND_RING) * 2 + 1];
    }
    s->ring_r = r + (uint32_t)n;
    return n;
}
