/* samples.c - see samples.h. */
#include "sound/samples.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const NAMES[BZ_SMP_COUNT] = {
    "RBEEP", "BOING", "BLKSND", "BONER", "WARNG", "DISINT", "SAUSND", "SUPBON",
    "MISSILE",
    "SHELL_LOUD", "SHELL_SOFT", "EXPLO_LOUD", "EXPLO_SOFT",
    "ENGINE_IDLE", "ENGINE_REV",
    "ENEMY_ENGINE",
};

const char *bz_sample_name(int id)
{
    return id >= 0 && id < BZ_SMP_COUNT ? NAMES[id] : "";
}

/* --- WAV files ---------------------------------------------------------------------- */

static uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static unsigned le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

/* The file as mono samples at `rate`. Returns 0, or -1 if it is not a PCM
 * WAV file of 8 or 16 bit. */
static int load_wav(const char *path, int rate, int16_t **pcm, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = size > 44 && size < (64L << 20) ? (uint8_t *)malloc((size_t)size) : NULL;
    if (!d || fread(d, 1, (size_t)size, f) != (size_t)size) { free(d); fclose(f); return -1; }
    fclose(f);

    int ok = -1;
    unsigned channels = 0, bits = 0;
    uint32_t hz = 0;
    const uint8_t *data = NULL;
    uint32_t bytes = 0;
    if (memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WAVE", 4) == 0) {
        for (long at = 12; at + 8 <= size; ) {
            uint32_t len = le32(d + at + 4);
            if ((long)len > size - at - 8) len = (uint32_t)(size - at - 8);
            if (memcmp(d + at, "fmt ", 4) == 0 && len >= 16) {
                unsigned format = le16(d + at + 8);
                channels = le16(d + at + 10);
                hz = le32(d + at + 12);
                bits = le16(d + at + 22);
                if (format != 1 && format != 0xfffe) channels = 0;     /* PCM only */
            } else if (memcmp(d + at, "data", 4) == 0) {
                data = d + at + 8;
                bytes = len;
            }
            at += 8 + (long)len + (len & 1);
        }
    }
    if (data && (channels == 1 || channels == 2) && (bits == 8 || bits == 16) && hz >= 4000 && hz <= 192000) {
        unsigned step = channels * bits / 8;
        uint32_t frames = bytes / step;
        uint32_t out = (uint32_t)((uint64_t)frames * (unsigned)rate / hz);
        int16_t *p = out ? (int16_t *)malloc(out * sizeof *p) : NULL;
        if (p) {
            for (uint32_t i = 0; i < out; i++) {
                /* between the two samples it falls between */
                double at = (double)i * hz / rate;
                uint32_t i0 = (uint32_t)at, i1 = i0 + 1 < frames ? i0 + 1 : i0;
                double k = at - (double)i0, v[2];
                uint32_t idx[2] = { i0, i1 };
                for (int q = 0; q < 2; q++) {
                    const uint8_t *s = data + (size_t)idx[q] * step;
                    double sum = 0;
                    for (unsigned c = 0; c < channels; c++)
                        sum += bits == 8 ? ((int)s[c] - 128) * 256 : (int16_t)le16(s + 2 * c);
                    v[q] = sum / channels;
                }
                p[i] = (int16_t)(v[0] + (v[1] - v[0]) * k);
            }
            *pcm = p;
            *n = out;
            ok = 0;
        }
    }
    free(d);
    return ok;
}

void bz_samples_init(bz_samples *s)
{
    memset(s, 0, sizeof *s);
    s->tune.id = s->shell.id = s->explo.id = s->test.id = -1;
}

int bz_samples_load(bz_samples *s, const char *dir, int rate)
{
    int found = 0;
    for (int i = 0; i < BZ_SMP_COUNT; i++) {
        char path[1024];
        if (s->sound[i].pcm) continue;                  /* (already there, from a folder asked before) */
        snprintf(path, sizeof path, "%s/%s.wav", dir, NAMES[i]);
        if (load_wav(path, rate, &s->sound[i].pcm, &s->sound[i].n) == 0) found++;
    }
    return found;
}

void bz_samples_free(bz_samples *s)
{
    for (int i = 0; i < BZ_SMP_COUNT; i++) free(s->sound[i].pcm);
    bz_samples_init(s);
}

int bz_samples_has(const bz_samples *s, int id)
{
    return s->enabled && id >= 0 && id < BZ_SMP_COUNT && s->sound[id].pcm != NULL;
}

/* --- playing --------------------------------------------------------------------------- */

void bz_samples_start(bz_samples *s, int voice, int id)
{
    if (id >= BZ_SMP_COUNT || (id >= 0 && !s->sound[id].pcm)) id = -1;
    switch (voice) {
    case 0:  s->tune.id = id;  s->tune.at = 0;  break;
    case 1:  s->shell.id = id; s->shell.at = 0; break;
    case 2:  s->explo.id = id; s->explo.at = 0; break;
    default: s->test.id = id;  s->test.at = 0;  break;
    }
    if (id >= 0) s->sound[id].played++;
}

int bz_samples_voice(bz_samples *s, int voice)
{
#define VOICE(v) \
    if (s->v.id >= 0) { \
        int out = s->sound[s->v.id].pcm[s->v.at]; \
        if (++s->v.at >= s->sound[s->v.id].n) s->v.id = -1; \
        return out; \
    } \
    return 0;
    switch (voice) {
    case 0:  VOICE(tune)
    case 1:  VOICE(shell)
    case 2:  VOICE(explo)
    default: VOICE(test)
    }
#undef VOICE
}

double bz_samples_loop(const bz_samples *s, int id, double *at, double rate)
{
    const int16_t *pcm = s->sound[id].pcm;
    uint32_t n = s->sound[id].n;
    if (!pcm || !n) return 0.0;
    if (*at >= (double)n || *at < 0.0) *at = 0.0;
    uint32_t i0 = (uint32_t)*at, i1 = i0 + 1 < n ? i0 + 1 : 0;
    double k = *at - (double)i0;
    double v = pcm[i0] + (pcm[i1] - pcm[i0]) * k;
    *at += rate;
    while (*at >= (double)n) *at -= (double)n;
    return v;
}
