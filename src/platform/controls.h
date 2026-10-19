/* controls.h - keys, buttons and sticks as the Battlezone board's switches.
 *
 * Three ways to drive the tank, all at once: whatever is plugged in works,
 * nothing is chosen.
 *
 *   single   one digital joystick: up is both levers forward, down both
 *            back, left and right pivot, the diagonals are the cabinet's
 *            four turns
 *   dual     two digital joysticks, the cabinet's two levers
 *            (ALLPOT D3 / D2 the left one forward / back, D1 / D0 the right)
 *   analog   a yoke or an analog stick: forward and back for the speed,
 *            left and right to turn, everything in between (the game's
 *            addition for it: analog_motion in game/play.c)
 *
 * Several of them may come from one control: on the cabinet the left
 * lever is the arrow keys, which are the single joystick too; a stick's
 * axis is both a yoke axis and, past half way, a digital direction. So,
 * each picture: a lever of the two joysticks held drives as the levers; if
 * none is, the yoke out of its dead zone drives as the yoke (a digital
 * stick that reports axes then drives the same, at its ends); if it is
 * not, the one joystick. With nothing touched the last way stays
 * (bz_controls.mode), for the instructions it gives.
 *
 * and fire (ALLPOT D4), start (D5) and the coin switch (IN0 D0, as a
 * 100 ms pulse: MOOLAH accepts only 16 to 800 ms of closure).
 *
 * Every action can be bound to any key, any button, hat or axis of any
 * joystick, and to several (up to eight, joined by "or" as in MAME: fire on
 * two buttons, say) (bz_config.bind, kept in battlezone.ini). An action left at
 * "auto" is the cabinet's own control, by the names of advmame.rc as
 * MAME's Battlezone uses them (vp_input):
 *
 *   up down left right       p1_up p1_down p1_left p1_right (PC: the arrow keys)
 *   the levers               p1_doubleleft_up / _down, p1_doubleright_up / _down (and the keys E D, I K)
 *   the yoke                 p1_sticky, p1_stickx (and the arrow keys, as its ends)
 *   fire, start, coin        p1_button1, start1, coin1 (coin2: the middle mech)
 *   the test menu            the Tab key, always; a binding adds a second way
 */
#ifndef BZ_CONTROLS_H
#define BZ_CONTROLS_H

#include "core/machine.h"
#include "render/config.h"

#include <stddef.h>
#include <vectorpie/vp_input.h>

#define BZ_MAX_JOYSTICKS 8
#define BZ_MAX_AXES      16

typedef struct bz_bind {
    uint8_t type;           /* what it is bound to (controls.c) */
    uint8_t joy;            /* which joystick, from 0 */
    int16_t index;          /* the scancode, button, axis or hat */
    int8_t  dir;            /* an axis: 1 or -1, the way that counts; a hat: its direction bit */
} bz_bind;

/* Where the controls point for a menu: -1, 0, 1 each way (y: up is -1),
 * and fire as it goes down. Whatever is bound to driving counts, and the
 * arrow keys, Enter and the space bar always. */
typedef struct bz_nav {
    int x, y, fire;
} bz_nav;

typedef struct bz_controls {
    int     mode;                       /* BZ_INPUT_xxx: the way of driving used last */
    float   deadzone;
    vp_input *in;           /* the SDK's input, as the last read had it (the starts) */
    bz_bind bind[BZ_ACT_COUNT][BZ_BIND_ALTS];
    int     binds[BZ_ACT_COUNT];        /* how many of them each action has */

    uint8_t in3;                        /* the levers and buttons from the last read */
    int16_t left, right;                /* analog: the levers, -256..256 */
    int     coin_pulse[2];              /* interrupts left with the coin switch closed */
    bz_nav  nav;
    int     menu;                       /* the test menu's key went down (Tab, or what is bound to it) */
    int     fire_was, coin_was, menu_was;
    int     menu_hold;                  /* start and fire held for two seconds open the menu too */
    uint32_t hold_since;                /* since when they are (SDL's ticks, 0: they are not) */

    void   *joy[BZ_MAX_JOYSTICKS];      /* SDL_Joystick */
    int     joys;

    /* taking a new binding from the next thing pressed */
    int     cap_action;                 /* -1: none */
    int     cap_armed;                  /* everything has been let go */
    int     cap_still;                  /* pictures for which nothing has moved */
    int16_t cap_rest[BZ_MAX_JOYSTICKS][BZ_MAX_AXES];
} bz_controls;

void bz_controls_init(bz_controls *c, const bz_config *cfg);
void bz_controls_close(bz_controls *c);

/* Take the dead zone and bindings from the settings (again after they
 * have changed). */
void bz_controls_configure(bz_controls *c, const bz_config *cfg, vp_input *in);

/* Once per picture: sample everything. */
void bz_controls_read(bz_controls *c, vp_input *in);

/* Once per interrupt, before it: present the switches to the board.
 * native: the C routines run (they know the analog mode); 0: the original
 * program, to which a yoke is two levers at their ends. */
void bz_controls_tick(bz_controls *c, bz_machine *m, int native);

/* A new binding for an action: begin, then once per picture after
 * bz_controls_read. Returns 0 while waiting, 1 with the binding written to
 * spec (bz_config.bind's form), -1 if given up (Esc). For the yoke's two
 * axes only an axis counts, pushed the way that is to mean forward
 * (BZ_ACT_DRIVE) or right (BZ_ACT_TURN). */
void bz_controls_capture_begin(bz_controls *c, int action);
int  bz_controls_capture_poll(bz_controls *c, char *spec, size_t size);

#endif
