/* romset.h - the original ROMs, read from the owner's MAME ROM set.
 *
 * The game's data (the vector ROM's pictures and letters, the objects, the
 * playfield, the tunes) stays in the ROMs and is read from the cabinet's
 * own set at start-up; none of it is part of this program.
 *
 * The source this port follows (historicalsource/battlezone, linked
 * 1-SEP-81) is revision 1 of the program: MAME's set "bzonea", whose
 * 036414-01 the repository's own ROM file is, byte for byte. MAME keeps it
 * in bzone.zip (as bzonea/036414-01.e1) next to revision 2 (036414-02, the
 * set "bzone"); the other ROMs are the same for both. Revision 2 differs in
 * 149 bytes of the first program ROM (the high score page).
 *
 * The C routines read their tables only from ROMs both revisions share, so
 * they run with either set; the original program on the 6502 core is the
 * revision found (1 if both are there).
 *
 * Files are matched by CRC32, so names and zip layout do not matter.
 */
#ifndef BZ_ROMSET_H
#define BZ_ROMSET_H

#include <stdint.h>

typedef struct bz_romset {
    uint8_t  rom[0x5000];       /* 3000-7FFF: vector ROM, then the program */
    uint8_t  avg_prom[0x100];   /* the vector generator's state PROM (not needed) */
    uint8_t  mb_map[0x20];      /* math box mapping PROM (not needed) */
    uint8_t  mb_prom[6][0x100]; /* math box micro-code, six 256x4 PROMs (not needed) */
    uint32_t found;             /* bit per entry of the ROM table */
    int      revision;          /* of the program: 1 or 2; 0: none found */
} bz_romset;

/* Look in each path (a .zip or a directory) for the set's ROMs, in order,
 * until all are found. Returns 0 when the game can run; otherwise -1 and
 * bz_romset_missing() names what is missing. */
int bz_romset_load(bz_romset *rs, const char *const *paths, int npaths);

/* The ROMs not found, as a comma-separated list (empty when complete). */
const char *bz_romset_missing(const bz_romset *rs, char *buf, int size);

/* The default search list: $BZ_ROMS (a ';'- or ':'-separated list), then
 * ./roms and the usual AdvanceMAME/MAME rom folders, each tried as
 * bzone.zip, bzonea.zip and a directory. Fills up to max entries with
 * pointers into an internal buffer; returns the count. */
int bz_romset_default_paths(const char **out, int max);

/* The same list for one more folder (a command line option, a setting):
 * its forms appended at out[n]; returns the new count. */
int bz_romset_add_dir(const char **out, int n, int max, const char *dir);

#endif
