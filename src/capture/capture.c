#include "capture/capture.h"

#include "core/bz_symbols.h"

#include <string.h>

/* The same list as WATCH in tools/mame/bzcapture.lua. */
static const uint16_t WATCH[][2] = {
    { BZ_CRDT, BZ_CRDT },           /* credits */
    { BZ_BUFRDY, BZ_BUFRDY },       /* the buffer the interrupt hands out */
    { BZ_SYNC, BZ_INTCT },          /* frame sync (not MAIN's own polls of it), interrupt count */
    { BZ_CMODE, BZ_CMODE },         /* coin mode */
    { BZ_POINT, BZ_POINT + 3 },     /* the tunes' pointers */
    { BZ_HALT, BZ_HALT }, { BZ_OPTION, BZ_OPTION }, { BZ_OPTON2, BZ_OPTON2 }, { BZ_ALLPOT, BZ_ALLPOT },
};
#define NWATCH ((int)(sizeof WATCH / sizeof WATCH[0]))

int bzcap_watched(uint16_t addr)
{
    for (int i = 0; i < NWATCH; i++)
        if (addr >= WATCH[i][0] && addr <= WATCH[i][1]) return 1;
    return 0;
}

void bzcap_watch_set(uint8_t *watch)
{
    memset(watch, 0, 0x2000);
    for (int i = 0; i < NWATCH; i++)
        for (unsigned a = WATCH[i][0]; a <= WATCH[i][1]; a++) watch[a] = 1;
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int bzcap_open(bzcap_reader *r, const char *path)
{
    memset(r, 0, sizeof *r);
    r->f = fopen(path, "rb");
    if (!r->f) return -1;
    char magic[8];
    if (fread(magic, 1, 8, r->f) != 8 || memcmp(magic, "BZCAP1\0\0", 8) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    return 0;
}

void bzcap_close(bzcap_reader *r)
{
    if (r->f) fclose(r->f);
    r->f = NULL;
}

int bzcap_next_chunk(bzcap_reader *r, char tag[4], uint8_t *buf, uint32_t cap, uint32_t *len)
{
    uint8_t head[8];
    size_t n = fread(head, 1, 8, r->f);
    if (n == 0) return 0;
    if (n != 8) return -1;
    memcpy(tag, head, 4);
    *len = u32(head + 4);
    if (memcmp(tag, "ROMS", 4) == 0 && *len == sizeof r->rom) {
        if (fread(r->rom, 1, sizeof r->rom, r->f) != sizeof r->rom) return -1;
        r->have_rom = 1;
        return 1;
    }
    uint32_t take = *len < cap ? *len : cap;
    if (take && fread(buf, 1, take, r->f) != take) return -1;
    if (*len > take && fseek(r->f, (long)(*len - take), SEEK_CUR) != 0) return -1;
    return 1;
}

void bzcap_parse_frame(const uint8_t *p, bzcap_frame *t)
{
    t->frame = u32(p);
    t->nmis = u32(p + 4);
    p += 8;
    memcpy(t->ram, p, BZCAP_RAM_SIZE);
    p += BZCAP_RAM_SIZE;
    memcpy(t->vgram, p, BZCAP_VGRAM_SIZE);
    p += BZCAP_VGRAM_SIZE;
    for (int i = 0; i < 16; i++) t->mb_reg[i] = (int16_t)(uint16_t)(p[2 * i] | (p[2 * i + 1] << 8));
    t->mb_result = (int16_t)(uint16_t)(p[32] | (p[33] << 8));
}
