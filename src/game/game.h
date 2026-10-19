/* game.h - the Battlezone program in C, routine by routine. Each function
 * names the original routine it follows (BZONE.MAC unless said otherwise)
 * and takes and leaves A, X, Y and the carry as the original does
 * (game/m6502.h). */
#ifndef BZ_GAME_H
#define BZ_GAME_H

#include "core/machine.h"

/* --- running the game ------------------------------------------------------ */

/* PWRON (BZSTST.MAC): the state the game starts from. The ROMs must be in
 * the machine. */
void bz_pwron(bz_machine *m);

/* The interrupt, 246.09 times a second (game/nmi.c). */
void bz_nmi(bz_machine *m);

/* MAIN: call after every interrupt. Runs a game frame whenever one is due
 * (the interrupt's sync every 16th time, and a display buffer free). */
void bz_main_poll(bz_machine *m);

/* One whole game frame at once, from MAIN's sync to its JMP MAIN: what
 * tools/bzcompare holds against a recording. */
void bz_frame(bz_machine *m);

/* A frame's picture only, for the renderer (render/smooth.h): VOLCNO and
 * MAIN's drawing, into the display buffer at `buffer` (20 or 28), without
 * the step of the game that follows it. On a copy of the machine: what
 * the drawing itself changes (the pieces of an explosion, the radar's
 * sweep) is changed. */
void bz_picture(bz_machine *m, uint8_t buffer);

/* As bz_main_poll up to where the frame's work begins: 1 if it would begin
 * now (SYNC has come and a display buffer is free), without doing it. */
int bz_main_ready(bz_machine *m);

/* bz_machine.main_state */
enum { BZ_MAIN_SYNC = 0, BZ_MAIN_BUFFER };

/* --- sounds (SNDON's A) ------------------------------------------------------- */

enum {
    BZ_S_RBEEP  = 0x01,     /* radar beep */
    BZ_S_BOING  = 0x02,     /* bumped into an object */
    BZ_S_BLKSND = 0x04,     /* motion blocked */
    BZ_S_BONER  = 0x08,     /* bonus tank */
    BZ_S_WARNG  = 0x10,     /* enemy in range */
    BZ_S_DISINT = 0x20,     /* saucer hit; the slam alarm */
    BZ_S_SAUSND = 0x40,     /* saucer in view */
    BZ_S_SUPBON = 0x80      /* the 1812 overture: a high score, or 100000 */
};

/* --- routines ------------------------------------------------------------------ */

/* VGUT.MAC (game/vgut.c): instructions into the display list at VGLIST */
void bz_vgrtsl(bz_machine *m);
void bz_vghalt(bz_machine *m);
void bz_vghex(bz_machine *m);
void bz_vghexz(bz_machine *m);
void bz_vgjmpl(bz_machine *m);
void bz_vgjsrl(bz_machine *m);
void bz_vgstat(bz_machine *m);
void bz_vgsta1(bz_machine *m);
void bz_vgcntr(bz_machine *m);
void bz_vgadd2(bz_machine *m);
void bz_vgadd(bz_machine *m);
void bz_vgscal(bz_machine *m);
void bz_vgsca1(bz_machine *m);
void bz_vgdot(bz_machine *m);
void bz_vgvtr(bz_machine *m);
void bz_vgvtr1(bz_machine *m);
void bz_vgvtr2(bz_machine *m);
void bz_vgvctr(bz_machine *m);
void bz_digits(bz_machine *m);      /* BZSTST.MAC */
void bz_digtys(bz_machine *m);
void bz_digt2s(bz_machine *m);

/* BZSOUN.MAC (game/sound.c), COIN65.MAC (game/coin.c) */
void bz_sndon(bz_machine *m);
void bz_modsnd(bz_machine *m);
void bz_moolah(bz_machine *m);

/* game/mainline.c */
void bz_battle(bz_machine *m);
void bz_batint(bz_machine *m);
void bz_msgs(bz_machine *m);

/* game/view.c */
void bz_wait(bz_machine *m);
void bz_sin(bz_machine *m);
void bz_cos(bz_machine *m);
void bz_sinxtr(bz_machine *m);
void bz_cosxtr(bz_machine *m);
void bz_mounts(bz_machine *m);
void bz_volcno(bz_machine *m);
void bz_rotate(bz_machine *m);
void bz_draw(bz_machine *m);
void bz_pntput(bz_machine *m);
void bz_bound(bz_machine *m);

/* game/play.c */
void bz_qwikck(bz_machine *m);
void bz_colchk(bz_machine *m);
void bz_shupdt(bz_machine *m);
void bz_motion(bz_machine *m);
void bz_robot(bz_machine *m);
void bz_saucmv(bz_machine *m);
void bz_track(bz_machine *m);
void bz_objobj(bz_machine *m);
void bz_tr7chk(bz_machine *m);
void bz_robchk(bz_machine *m);
void bz_tankck(bz_machine *m);
void bz_r2d3ck(bz_machine *m);
void bz_dradar(bz_machine *m);

#endif
