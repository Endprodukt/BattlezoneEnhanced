#include "rom/romset.h"

#include "miniz.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum dest { D_ROM, D_AVG, D_MBMAP, D_MBPROM };

typedef struct rom_entry {
    const char *name;     /* MAME's name, for messages only */
    uint32_t    crc;
    uint32_t    size;
    enum dest   dest;
    uint32_t    offset;   /* D_ROM: the address; D_MBPROM: which */
    int         optional; /* not needed by the game */
    int         revision; /* of the program, for the ROM that differs; else 0 */
} rom_entry;

/* MAME "bzonea" and "bzone" (src/mame/atari/bzone.cpp). */
static const rom_entry ROMS[] = {
    { "036414-01.e1",  0xefbc3fa0, 0x0800, D_ROM, 0x5000, 0, 1 },
    { "036414-02.e1",  0x13de36d5, 0x0800, D_ROM, 0x5000, 0, 2 },
    { "036413-01.h1",  0x5d9d9111, 0x0800, D_ROM, 0x5800, 0, 0 },
    { "036412-01.j1",  0xab55cbd2, 0x0800, D_ROM, 0x6000, 0, 0 },
    { "036411-01.k1",  0xad281297, 0x0800, D_ROM, 0x6800, 0, 0 },
    { "036410-01.lm1", 0x0b7bfaa4, 0x0800, D_ROM, 0x7000, 0, 0 },
    { "036409-01.n1",  0x1e14e919, 0x0800, D_ROM, 0x7800, 0, 0 },
    { "036422-01.bc3", 0x7414177b, 0x0800, D_ROM, 0x3000, 0, 0 },
    { "036421-01.a3",  0x8ea8f939, 0x0800, D_ROM, 0x3800, 0, 0 },
    { "036408-01.k7",  0x5903af03, 0x0100, D_AVG, 0, 1, 0 },
    { "036174-01.b1",  0x8b04f921, 0x0020, D_MBMAP, 0, 1, 0 },
    { "036175-01.m1",  0x2af82e87, 0x0100, D_MBPROM, 0, 1, 0 },
    { "036176-01.l1",  0xb31f6e24, 0x0100, D_MBPROM, 1, 1, 0 },
    { "036177-01.k1",  0x8119b847, 0x0100, D_MBPROM, 2, 1, 0 },
    { "036178-01.j1",  0x09f5a4d5, 0x0100, D_MBPROM, 3, 1, 0 },
    { "036179-01.h1",  0x823b61ae, 0x0100, D_MBPROM, 4, 1, 0 },
    { "036180-01.f1",  0x276eadd5, 0x0100, D_MBPROM, 5, 1, 0 },
};
#define NROMS ((int)(sizeof ROMS / sizeof ROMS[0]))
#define ALL_FOUND ((1u << NROMS) - 1u)

/* Every ROM the game needs is there (one of the two revisions will do). */
static int complete(const bz_romset *rs)
{
    for (int i = 0; i < NROMS; i++) {
        if (ROMS[i].optional || ROMS[i].revision) continue;
        if (!(rs->found & (1u << i))) return 0;
    }
    return rs->revision != 0;
}

static void place(bz_romset *rs, int i, const uint8_t *data)
{
    const rom_entry *e = &ROMS[i];
    switch (e->dest) {
    case D_ROM:
        /* Revision 1 is what the source describes: it wins over 2. */
        if (e->revision && rs->revision && rs->revision < e->revision) break;
        memcpy(rs->rom + (e->offset - 0x3000), data, e->size);
        if (e->revision) rs->revision = e->revision;
        break;
    case D_AVG:    memcpy(rs->avg_prom, data, e->size); break;
    case D_MBMAP:  memcpy(rs->mb_map, data, e->size); break;
    case D_MBPROM: memcpy(rs->mb_prom[e->offset], data, e->size); break;
    }
    rs->found |= 1u << i;
}

/* Offer one file's contents: placed if it is a wanted ROM not yet found. */
static void offer(bz_romset *rs, const uint8_t *data, size_t size)
{
    uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, data, size);
    for (int i = 0; i < NROMS; i++) {
        if (ROMS[i].crc == crc && ROMS[i].size == size && !(rs->found & (1u << i)))
            place(rs, i, data);
    }
}

static int wanted_size(size_t size)
{
    for (int i = 0; i < NROMS; i++)
        if (ROMS[i].size == size) return 1;
    return 0;
}

static void scan_zip(bz_romset *rs, const char *path)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, path, 0)) return;
    mz_uint n = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < n && rs->found != ALL_FOUND; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory) continue;
        if (!wanted_size((size_t)st.m_uncomp_size)) continue;
        /* The zip's own CRC tells whether the file is worth extracting. */
        int match = 0;
        for (int k = 0; k < NROMS; k++)
            if (ROMS[k].crc == st.m_crc32 && !(rs->found & (1u << k))) match = 1;
        if (!match) continue;
        size_t size = 0;
        void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (data) {
            offer(rs, (const uint8_t *)data, size);
            mz_free(data);
        }
    }
    mz_zip_reader_end(&zip);
}

static void scan_file(bz_romset *rs, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint8_t buf[0x0800 + 1];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n < sizeof buf && wanted_size(n)) offer(rs, buf, n);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int ends_with_zip(const char *path)
{
    size_t n = strlen(path);
    return n > 4 && (strcmp(path + n - 4, ".zip") == 0 || strcmp(path + n - 4, ".ZIP") == 0);
}

static void scan_dir(bz_romset *rs, const char *path, int depth)
{
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    char full[2048];
    while ((de = readdir(d)) != NULL && rs->found != ALL_FOUND) {
        if (de->d_name[0] == '.') continue;
        snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        if (is_dir(full)) {
            /* an unpacked bzone.zip keeps revision 1 in the folder bzonea */
            if (depth == 0 && strcmp(de->d_name, "bzonea") == 0) scan_dir(rs, full, 1);
        } else if (!ends_with_zip(full)) {
            scan_file(rs, full);
        }
    }
    closedir(d);
}

int bz_romset_load(bz_romset *rs, const char *const *paths, int npaths)
{
    memset(rs, 0, sizeof *rs);
    for (int i = 0; i < npaths && rs->found != ALL_FOUND; i++) {
        if (!paths[i] || !*paths[i]) continue;
        if (is_dir(paths[i])) scan_dir(rs, paths[i], 0);
        else if (ends_with_zip(paths[i])) scan_zip(rs, paths[i]);
        else scan_file(rs, paths[i]);
    }
    return complete(rs) ? 0 : -1;
}

const char *bz_romset_missing(const bz_romset *rs, char *buf, int size)
{
    int len = 0;
    buf[0] = '\0';
    for (int i = 0; i < NROMS; i++) {
        if ((rs->found & (1u << i)) || ROMS[i].optional) continue;
        if (ROMS[i].revision && rs->revision) continue;
        if (ROMS[i].revision == 2) continue;        /* named once, as revision 1 */
        len += snprintf(buf + len, len < size ? (size_t)(size - len) : 0, "%s%s",
                        len ? ", " : "", ROMS[i].name);
        if (len >= size) break;
    }
    return buf;
}

#define MAX_STORE 48
static char g_store[MAX_STORE][1024];
static int  g_nstore;

int bz_romset_add_dir(const char **out, int n, int max, const char *dir)
{
    static const char *const forms[] = { "%s/bzone.zip", "%s/bzonea.zip", "%s" };
    for (int f = 0; f < 3 && n < max && g_nstore < MAX_STORE; f++) {
        snprintf(g_store[g_nstore], sizeof g_store[g_nstore], forms[f], dir);
        out[n++] = g_store[g_nstore++];
    }
    return n;
}

int bz_romset_default_paths(const char **out, int max)
{
    int n = 0;
    static char envcopy[2048];

    const char *env = getenv("BZ_ROMS");
    if (env && *env) {
        snprintf(envcopy, sizeof envcopy, "%s", env);
        /* ';' separates on Windows, where a path holds a ':' */
        const char *sep = strchr(envcopy, ';') || (envcopy[1] == ':') ? ";" : ":";
        for (char *p = strtok(envcopy, sep); p && n < max; p = strtok(NULL, sep))
            n = bz_romset_add_dir(out, n, max, p);
    }

    n = bz_romset_add_dir(out, n, max, "roms");
    const char *home = getenv("HOME");
    static char h1[1024], h2[1024];
    if (home) {
        snprintf(h1, sizeof h1, "%s/.advance/rom", home);
        snprintf(h2, sizeof h2, "%s/mame/roms", home);
        n = bz_romset_add_dir(out, n, max, h1);
        n = bz_romset_add_dir(out, n, max, h2);
    }
    n = bz_romset_add_dir(out, n, max, "/home/pi/.advance/rom");
    return n;
}
