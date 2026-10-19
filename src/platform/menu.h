/* menu.h - Battlezone Enhanced's own test menu.
 *
 * The Tab key opens it; the game stands still meanwhile.
 * It holds what the port adds to the cabinet and the cabinet's own option
 * switches:
 *
 *   CONTROLS       how the tank is driven (one joystick, two joysticks, an
 *                  analog yoke), and what every action is bound to: fire
 *                  on a line, then press the key or button or push the
 *                  stick that is to do it
 *   PICTURE        smooth motion, solid objects, the ground's grid, the
 *                  colors, the brightness; ENHANCED PICTURE with an on/off
 *                  for each of render/enhance.h's additions; the screen's
 *                  calibration
 *   GAME OPTIONS   tanks, missile, bonus tank, language, coins
 *   SOUND          the volume, recordings in place of the board's sounds
 *                  on or off, and a test that plays each recording
 *   the high scores back to the factory's
 *
 *   up / down       choose a line
 *   left / right    change its value (a binding: back to the cabinet's own)
 *   fire            change it, open a page, or do what the line says
 *   Tab             save the settings and go back to the game
 *
 * Whatever drives the tank moves through the menu, and the arrow keys,
 * Enter and the space bar always do.
 */
#ifndef BZ_MENU_H
#define BZ_MENU_H

#include "platform/controls.h"
#include "render/config.h"
#include "sound/samples.h"

#include <vectorpie/vp.h>

enum {
    BZ_MENU_NOTHING = 0,
    BZ_MENU_CHANGED,        /* a setting changed: take it over */
    BZ_MENU_RESET_SCORES,   /* the high scores back to the factory's */
    BZ_MENU_SOUND,          /* play the recording bz_menu.sound */
    BZ_MENU_CALIBRATE,      /* show the calibration screen */
    BZ_MENU_CLOSE,          /* leave */
};

typedef struct bz_menu {
    int    open;
    int    page;
    int    item[6];         /* the line chosen on each page */
    int    sound;           /* the sound test's choice (BZ_SMP_xxx) */
    int    capture;         /* the action a binding is being taken for, or -1 */
    int    adding;          /* the line (+1) that had something bound since the cursor came: the next is added */
    int    confirm;         /* fire once more resets the high scores */
    int    dir_x, dir_y;    /* where the controls pointed the last time */
    double repeat_at;       /* when a held direction counts again (seconds) */
} bz_menu;

void bz_menu_open(bz_menu *u);

/* Once a picture while the menu is open, after bz_controls_read, at the
 * time now (seconds). Returns what is to be done (BZ_MENU_xxx). */
int  bz_menu_update(bz_menu *u, bz_config *cfg, bz_controls *ctl, double now);
/* smp: the recordings, for the sound test's line. */
void bz_menu_draw(const bz_menu *u, const bz_config *cfg, const bz_samples *smp, vp_dvg *d);

#endif
