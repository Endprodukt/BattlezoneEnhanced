/* hiscore.h - the high score table kept across power-offs.
 *
 * The cabinet has no memory for it: every power-on begins with ten times
 * 5000 points and the initials of the people who made the game. Here the
 * table (ten scores and their initials, HSCTBL and INITLS) is written to a
 * file whenever it has changed, and put back after power-on. The game's
 * own attract mode shows it, all ten rows.
 */
#ifndef BZ_HISCORE_H
#define BZ_HISCORE_H

#include "core/machine.h"

/* Put the saved table into the machine (after bz_pwron). Returns 0, or -1
 * if there is none (the machine keeps the power-on table). */
int bz_hiscore_load(bz_machine *m, const char *path);

/* Write the machine's table. Returns 0 or -1. */
int bz_hiscore_save(const bz_machine *m, const char *path);

/* The table once more, as the VectorPie menu reads it for every game with
 * "hiscores = yes" in its game.ini and shows on the marquee: up to ten
 * lines, best first, "NAME POINTS" (vp_hiscore.h; save/highscores.txt).
 * Written to a new file and renamed over the old. Returns 0 or -1. */
int bz_hiscore_save_text(const bz_machine *m, const char *path);

/* A number that changes when the table does; 0 while initials are being
 * entered (the table is not final then). */
uint32_t bz_hiscore_stamp(const bz_machine *m);

/* The power-on table again (ten times 5000). */
void bz_hiscore_reset(bz_machine *m);

#endif
