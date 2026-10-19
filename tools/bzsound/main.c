/* bzsound - the sounds of the board, each as a WAV file.
 *
 *   bzsound [--roms DIR] [--out DIR] [NAME ...]
 *
 * Plays every sound the game makes on the port's model of the board (the
 * POKEY and the discrete circuits, sound/sound.h) the way the game asks
 * for it, and records it: a tune of the POKEY from SNDON until MODSND has
 * run it out, a shot and an explosion for as long as the game holds its
 * switch closed and then until they have died away, the missile's buzz
 * and the engine for two seconds each (they are loops in the game). The
 * files are named as the sound replacement wants them (docs/SOUNDS.md):
 * DIR/NAME.wav, 48 kHz, 16 bit, mono. A file put in the program's
 * save/sounds folder under the same name plays in place of the board's
 * sound. Without names: every sound. DIR: sounds.
 *
 * Each sound is brought up to 90 % of full scale: the board's tunes are
 * quiet beside its noises (and their loudness to each other in the model
 * is only an estimate), and at their own level they would keep only a few
 * bits. The sound is made a second time at that volume, not scaled after,
 * so nothing is lost. --raw keeps the board's own levels.
 *
 *   bzsound --replay WRITES.txt --wav OUT.wav
 *
 * Plays a list of writes to the sound hardware (tools/mame/bzsound.lua
 * records MAME's: "SECONDS ADDRESS VALUE" a line) on the model at the
 * same times, and writes what comes out at the board's level: the same
 * game as MAME's WAV of that run, to compare (tools/soundcheck.py).
 */
#include "core/bz_symbols.h"
#include "core/emu.h"
#include "core/machine.h"
#include "game/game.h"
#include "rom/romset.h"
#include "sound/samples.h"
#include "sound/sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
static void make_dir(const char *p) { _mkdir(p); }
#else
#include <sys/stat.h>
static void make_dir(const char *p) { mkdir(p, 0755); }
#endif

static bz_romset roms;
static bz_machine m;
static bz_sound snd;

static const char *arg(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], name)) return argv[i + 1];
    return NULL;
}

/* --- recording ------------------------------------------------------------------- */

static int16_t *rec;
static uint32_t rec_n, rec_cap;

/* One interrupt's time of sound into the recording (the left channel: in
 * the middle both are the same). */
static void run_nmi(void)
{
    bz_sound_run(&snd, BZ_CPU_CYCLES_PER_NMI);
    int16_t buf[2048];
    int n;
    while ((n = bz_sound_read(&snd, buf, 1024)) > 0) {
        if (rec_n + (uint32_t)n > rec_cap) {
            rec_cap = (rec_n + (uint32_t)n) * 2 + 4096;
            rec = (int16_t *)realloc(rec, rec_cap * sizeof *rec);
            if (!rec) exit(1);
        }
        for (int i = 0; i < n; i++) rec[rec_n++] = buf[2 * i];
    }
}

static void run_seconds(double s)
{
    for (long i = (long)(s * BZ_NMI_HZ); i > 0; i--) run_nmi();
}

static int write_wav(const char *path, const int16_t *pcm, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t bytes = n * 2, rate = BZ_SOUND_RATE, v;
    uint16_t w;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    w = 1; fwrite(&w, 2, 1, f); w = 1; fwrite(&w, 2, 1, f);
    fwrite(&rate, 4, 1, f); v = rate * 2; fwrite(&v, 4, 1, f);
    w = 2; fwrite(&w, 2, 1, f); w = 16; fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    fwrite(pcm, 2, n, f);
    fclose(f);
    return 0;
}

/* The board as the game leaves it at power-on, its sound enabled, the
 * recording empty. */
static float g_volume = 1.0f;

static void fresh(void)
{
    bz_machine_init(&m);
    bz_machine_set_rom(&m, roms.rom);
    bz_pwron(&m);
    bz_sound_init(&snd);
    snd.volume = g_volume;
    m.sound_write = bz_sound_write;
    m.sound_user = &snd;
    bz_sound_write(&snd, BZ_SOUND, BZ_SND_ENABLE);
    run_seconds(0.3);                   /* (the output's capacitor settles) */
    rec_n = 0;
}

static void latch(uint8_t v) { bz_sound_write(&snd, BZ_SOUND, (uint8_t)(BZ_SND_ENABLE | v)); }

/* --- the sounds -------------------------------------------------------------------- */

/* A tune of the POKEY: SNDON with its bit, then MODSND once an interrupt
 * (as the interrupt does) until its sequences have all ended. */
static void tune(int bit)
{
    m.r.a = (uint8_t)(1 << bit);
    bz_sndon(&m);
    for (int n = 0; n < 20000; n++) {
        bz_modsnd(&m);
        run_nmi();
        if (!(m.mem[BZ_POINT] | m.mem[BZ_POINT + 1] | m.mem[BZ_POINT + 2] | m.mem[BZ_POINT + 3])) break;
    }
    run_seconds(0.15);
}

/* A burst: the switch closed for so many interrupts (as the interrupt
 * counts SXPCNT and EXPCNT down), then the sound dying away. */
static void burst(uint8_t on, int nmis, double tail)
{
    latch(on);
    for (int i = 0; i < nmis; i++) run_nmi();
    latch(0);
    run_seconds(tail);
}

/* The missile's buzz: channels 3 and 4 a step apart (CHAN3F FF, CHAN4F FE,
 * as R2D3CK sets them), pure tone at the loudness of a missile close by
 * (DRADAR: AF). */
static void missile(void)
{
    bz_sound_write(&snd, BZ_POKEY + 8, m.mem[BZ_AUDCV]);
    bz_sound_write(&snd, BZ_CHAN3F, 0xff);
    bz_sound_write(&snd, BZ_CHAN4F, 0xfe);
    bz_sound_write(&snd, BZ_CHAN3V, 0xaf);
    bz_sound_write(&snd, BZ_CHAN4V, 0xaf);
    run_seconds(2.0);
}

/* The engine, idling or revved: settled first, then two seconds. */
static void engine(int rev)
{
    latch((uint8_t)(BZ_SND_MOTOR | (rev ? BZ_SND_ENGINE_REV : 0)));
    run_seconds(1.5);
    rec_n = 0;
    run_seconds(2.0);
}

static void make(int id)
{
    fresh();
    switch (id) {
    case BZ_SMP_MISSILE:     missile(); break;
    case BZ_SMP_SHELL_LOUD:  burst(BZ_SND_SHELL | BZ_SND_SHELL_LOUD, 4, 0.6); break;    /* SXPCNT 5: 4 interrupts closed */
    case BZ_SMP_SHELL_SOFT:  burst(BZ_SND_SHELL, 4, 0.6); break;
    case BZ_SMP_EXPLO_LOUD:  burst(BZ_SND_EXPLO | BZ_SND_EXPLO_LOUD, 254, 0.8); break;  /* EXPCNT FF */
    case BZ_SMP_EXPLO_SOFT:  burst(BZ_SND_EXPLO, 111, 0.6); break;                     /* EXPCNT 70 */
    case BZ_SMP_ENGINE_IDLE: engine(0); break;
    case BZ_SMP_ENGINE_REV:  engine(1); break;
    default:                 tune(id); break;                                          /* the eight tunes */
    }
}

/* The writes of a recording, played on the model. */
static int replay(const char *path, const char *wav)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "bzsound: cannot read %s\n", path); return 1; }
    bz_sound_init(&snd);
    snd.volume = 1.0f;
    rec_n = 0;
    double done = 0.0;          /* seconds of sound made */
    char line[128];
    while (fgets(line, sizeof line, f)) {
        for (char *c = line; *c; c++) if (*c == ',') *c = '.';      /* (a decimal comma) */
        double t;
        unsigned addr, value;
        if (sscanf(line, "%lf %x %x", &t, &addr, &value) != 3) continue;
        int cycles = (int)((t - done) * BZ_CPU_HZ);
        if (cycles > 0) {
            bz_sound_run(&snd, cycles);
            done += (double)cycles / BZ_CPU_HZ;
            int16_t buf[2048];
            int n;
            while ((n = bz_sound_read(&snd, buf, 1024)) > 0) {
                if (rec_n + (uint32_t)n > rec_cap) {
                    rec_cap = (rec_n + (uint32_t)n) * 2 + 4096;
                    rec = (int16_t *)realloc(rec, rec_cap * sizeof *rec);
                    if (!rec) return 1;
                }
                for (int i = 0; i < n; i++) rec[rec_n++] = buf[2 * i];
            }
        }
        bz_sound_write(&snd, (uint16_t)addr, (uint8_t)value);
    }
    fclose(f);
    if (write_wav(wav, rec, rec_n) != 0) { fprintf(stderr, "bzsound: cannot write %s\n", wav); return 1; }
    printf("bzsound: %.1f s of the model written to %s\n", (double)rec_n / BZ_SOUND_RATE, wav);
    return 0;
}

int main(int argc, char **argv)
{
    if (arg(argc, argv, "--replay"))
        return replay(arg(argc, argv, "--replay"), arg(argc, argv, "--wav") ? arg(argc, argv, "--wav") : "model.wav");
    const char *paths[32];
    int np = 0;
    if (arg(argc, argv, "--roms")) np = bz_romset_add_dir(paths, np, 32, arg(argc, argv, "--roms"));
    np += bz_romset_default_paths(paths + np, 32 - np);
    if (bz_romset_load(&roms, paths, np) != 0) {
        char missing[512];
        fprintf(stderr, "bzsound: ROMs missing: %s\n", bz_romset_missing(&roms, missing, sizeof missing));
        return 1;
    }
    const char *out = arg(argc, argv, "--out") ? arg(argc, argv, "--out") : "sounds";
    make_dir(out);

    int written = 0;
    for (int id = 0; id < BZ_SMP_COUNT; id++) {
        /* only those named, if any are */
        int wanted = 1, named = 0;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--roms") || !strcmp(argv[i], "--out")) { i++; continue; }
            if (!strcmp(argv[i], "--raw")) continue;
            named = 1;
            if (!strcmp(argv[i], bz_sample_name(id))) wanted = 2;
        }
        if (named && wanted != 2) continue;
        int raw = 0;
        for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--raw")) raw = 1;
        g_volume = 1.0f;
        make(id);
        if (!raw) {
            int top = 1;
            for (uint32_t i = 0; i < rec_n; i++) if (abs(rec[i]) > top) top = abs(rec[i]);
            g_volume = 0.9f * 32767.0f / (float)top;
            make(id);
        }
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", out, bz_sample_name(id));
        if (write_wav(path, rec, rec_n) != 0) {
            fprintf(stderr, "bzsound: cannot write %s\n", path);
            return 1;
        }
        int peak = 0;
        for (uint32_t i = 0; i < rec_n; i++) if (abs(rec[i]) > peak) peak = abs(rec[i]);
        printf("%-12s %5.2f s, peak %5.1f %%  %s\n", bz_sample_name(id), (double)rec_n / BZ_SOUND_RATE,
               100.0 * peak / 32767.0, path);
        written++;
    }
    printf("bzsound: %d sound(s) written to %s\n", written, out);
    return 0;
}
