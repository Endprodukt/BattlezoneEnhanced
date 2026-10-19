#include "render/hiscore.h"

#include <stdio.h>
#include <string.h>

#define TABLE_BYTES (3 * BZ_HSCNUM)
static const char MAGIC[8] = "BZHISC1";

int bz_hiscore_load(bz_machine *m, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t d[8 + 2 * TABLE_BYTES];
    size_t n = fread(d, 1, sizeof d, f);
    fclose(f);
    if (n != sizeof d || memcmp(d, MAGIC, 8) != 0) return -1;
    /* Scores are BCD, initials places in the vector ROM's alphabet (even,
     * up to 4A): anything else is not a table. */
    for (int i = 0; i < TABLE_BYTES; i++) {
        uint8_t s = d[8 + i], c = d[8 + TABLE_BYTES + i];
        if (i % 3 != 2 && ((s & 0x0f) > 9 || (s >> 4) > 9)) return -1;
        if ((c & 1) || c > 0x4a) return -1;
    }
    memcpy(m->mem + BZ_HSCTBL, d + 8, TABLE_BYTES);
    memcpy(m->mem + BZ_INITLS, d + 8 + TABLE_BYTES, TABLE_BYTES);
    return 0;
}

int bz_hiscore_save(const bz_machine *m, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = fwrite(MAGIC, 1, 8, f) == 8 &&
             fwrite(m->mem + BZ_HSCTBL, 1, TABLE_BYTES, f) == TABLE_BYTES &&
             fwrite(m->mem + BZ_INITLS, 1, TABLE_BYTES, f) == TABLE_BYTES;
    fclose(f);
    return ok ? 0 : -1;
}

/* An initial's place in the vector ROM's alphabet as a character: 00 a
 * space, 02..14 the digits, 16..48 A..Z, 4A a space again (what LETCHK
 * steps through: 16 to 4A). */
static char letter(uint8_t c)
{
    if (c >= 0x16 && c <= 0x48) return (char)('A' + (c - 0x16) / 2);
    if (c >= 0x02 && c <= 0x14) return (char)('0' + (c - 0x02) / 2);
    return ' ';
}

static int bcd(uint8_t b) { return (b >> 4) * 10 + (b & 0x0f); }

int bz_hiscore_save_text(const bz_machine *m, const char *path)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.new", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    for (int i = 0; i < BZ_HSCNUM; i++) {
        /* (a score: BCD thousands, low byte first; the game writes "000" after it) */
        const uint8_t *s = m->mem + BZ_HSCTBL + 3 * i, *in = m->mem + BZ_INITLS + 3 * i;
        long points = (long)(bcd(s[1]) * 100 + bcd(s[0])) * 1000L;
        if (points <= 0) continue;
        char name[4];
        int n = 0;
        for (int k = 0; k < 3; k++) {
            char ch = letter(in[k]);
            if (ch != ' ') name[n++] = ch;          /* (no spaces in the menu's format) */
        }
        name[n] = '\0';
        fprintf(f, "%s %ld\n", n ? name : "---", points);
    }
    int ok = fclose(f) == 0;
    if (ok) {
        remove(path);                               /* (Windows renames over nothing) */
        ok = rename(tmp, path) == 0;
    }
    return ok ? 0 : -1;
}

uint32_t bz_hiscore_stamp(const bz_machine *m)
{
    if (m->mem[BZ_GOVER] & 0x80) return 0;      /* ACCEPTING INITIALS */
    uint32_t h = 2166136261u;
    for (int i = 0; i < TABLE_BYTES; i++) h = (h ^ m->mem[BZ_HSCTBL + i]) * 16777619u;
    for (int i = 0; i < TABLE_BYTES; i++) h = (h ^ m->mem[BZ_INITLS + i]) * 16777619u;
    return h ? h : 1;
}

void bz_hiscore_reset(bz_machine *m)
{
    /* As PWRON: every score 5000, the initials from the ROM. */
    for (int i = 0; i < BZ_HSCNUM; i++) {
        m->mem[BZ_HSCTBL + 3 * i] = 0x05;
        m->mem[BZ_HSCTBL + 3 * i + 1] = 0x00;
        m->mem[BZ_HSCTBL + 3 * i + 2] = 0x00;
    }
    memcpy(m->mem + BZ_INITLS, m->mem + BZ_FDGTBL, TABLE_BYTES);
}
