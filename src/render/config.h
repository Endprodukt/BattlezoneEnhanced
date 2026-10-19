/* config.h - the settings: where the picture lands on the tube, how bright
 * and in which colors, the cabinet's option switches, the controls.
 *
 * Read from battlezone.ini (key = value, ';' or '#' comments): the
 * package's next to the program, then the owner's in save/.
 */
#ifndef BZ_CONFIG_H
#define BZ_CONFIG_H

#include <stdint.h>

enum {
    BZ_COLOR_WHITE = 0,     /* one color: as the tube itself */
    BZ_COLOR_OVERLAY,       /* as the cabinet's overlay: the top fifth red, the rest green */
    BZ_COLOR_COLOR,         /* every line by what it is (render/color.h), the classic palette */
    BZ_COLOR_FLYER          /* the same in the colors of Atari's flyer */
};

/* What can be bound to a key, a button, a stick (platform/controls.h). */
enum {
    BZ_ACT_UP = 0, BZ_ACT_DOWN, BZ_ACT_LEFT, BZ_ACT_RIGHT,                  /* one joystick */
    BZ_ACT_LEFT_FWD, BZ_ACT_LEFT_BACK, BZ_ACT_RIGHT_FWD, BZ_ACT_RIGHT_BACK, /* two joysticks: the levers */
    BZ_ACT_DRIVE, BZ_ACT_TURN,                                              /* the yoke's two axes */
    BZ_ACT_FIRE, BZ_ACT_START, BZ_ACT_COIN, BZ_ACT_MENU,
    BZ_ACT_COUNT
};
#define BZ_BIND_SIZE 320       /* (up to BZ_BIND_ALTS bindings, joined by " or ") */
#define BZ_BIND_ALTS 8          /* (4 until 0.1.17: the cabinet fires with four keys and auto) */

enum { BZ_INPUT_SINGLE = 0, BZ_INPUT_DUAL, BZ_INPUT_ANALOG };

typedef struct bz_config {
    /* The part of the vector generator's field that is shown, in its
     * screen units (avg/avg.h): what the game draws in, -256..256 by
     * -192..192, stretched to the board's 0..4095. */
    float view_left, view_right, view_bottom, view_top;
    /* Then a margin per side in board units (overscan / tube edge). */
    int   margin_left, margin_right, margin_bottom, margin_top;
    int   flip_x, flip_y;

    /* Intensity 1..15 -> beam: out = floor + (1 - floor) * (i/15)^gamma,
     * times brightness. */
    float gamma, floor, brightness;

    int     color_mode;         /* BZ_COLOR_xxx */
    uint8_t color[3];           /* BZ_COLOR_WHITE: the one color */
    uint8_t color_top[3];       /* BZ_COLOR_OVERLAY: above the line, */
    uint8_t color_bottom[3];    /* and below it */
    float   overlay_split;      /* where the line is, from the top: 0.2 */

    /* The option switches, as what they mean. */
    int  lives;                 /* 2..5 */
    int  missile_at;            /* 5000, 10000, 20000 or 30000 */
    int  bonus_at;              /* 0 (none), 15000, 25000 or 50000 (and 100000) */
    char language[16];          /* english, german, french, spanish */
    char coin_mode[16];         /* free, 2plays (a coin), 1play, 2coins (a play) */

    /* How the tank is driven: no choice to make. One digital joystick, two
     * (the cabinet's levers) and a yoke all work at once, whichever is
     * plugged in (platform/controls.h). */
    float deadzone;             /* of the yoke, as a fraction of its travel */
    /* What each action is bound to: "auto" (the cabinet's own, from
     * advmame.rc, or the PC defaults), "none", "key NAME", "joyN button K",
     * "joyN axis K +" / "-", "joyN hat K up" / "down" / "left" / "right";
     * up to BZ_BIND_ALTS of them joined by " or ", as in MAME
     * ("auto or joy2 button 3": either does it). */
    char  bind[BZ_ACT_COUNT][BZ_BIND_SIZE];

    int   smooth;               /* pictures between the game's frames (render/smooth.h). 1: computed from the
                                 * game's state. 2: the frame's lines moved. 0: none */
    float board_hz;             /* pictures a second on the vector board: the board's own refresh rate (DVG REFRESH) */
    int   occlusion;            /* 1: what is behind something is not drawn (render/solid.h) */
    int   ground_grid;          /* 1: a grid on the ground (render/ground.h) */

    /* The enhanced options, each 1 or 0 (render/enhance.h, sound/sound.h,
     * game/pace.h, platform/controls.h). */
    int   enh_particles, enh_stars, enh_far, enh_volcano, enh_flash, enh_levers;
    int   enh_stereo, enh_distance, enh_engine;
    int   enh_enemy_engine;     /* the enemy tank's engine is heard, at his place (recordings only) */
    int   enh_ducking;          /* the engines a little softer while an effect sounds */
    int   enh_tempo;            /* the cabinet's pace: frames take as long as its 6502 took */
    int   menu_hold;            /* start and fire held together for two seconds open the test menu */
    int   start_lamp;           /* on the cabinet: the START lamp as the game switches it */
    float volume;               /* 0..1 */
    int   sound_replace;        /* 1: recordings in sounds/ play in place of the board's sounds (sound/samples.h) */
    char rom_path[512];         /* a folder or zip with the ROMs, or empty */
} bz_config;

void bz_config_defaults(bz_config *c);

/* Overlay the settings found in path. Returns 0, or -1 if the file cannot
 * be read (the settings are then unchanged). Unknown keys are reported on
 * stderr and ignored. */
int bz_config_load(bz_config *c, const char *path);

/* Write every setting. */
int bz_config_save(const bz_config *c, const char *path);

/* The option switch banks the settings mean (0A00 and 0C00). */
/* The name of an action in the file ("up", "left_fwd", ...). */
const char *bz_config_action_name(int action);

/* One more binding for an action ("joy2 button 3"), after the ones it
 * has (MAME's "or"). Already there: nothing changes. "none": replaced.
 * BZ_BIND_ALTS of them already: the last one is replaced. Returns 1 if the
 * binding changed. */
int bz_config_bind_add(bz_config *c, int action, const char *spec);

uint8_t bz_config_dsw0(const bz_config *c);
uint8_t bz_config_dsw1(const bz_config *c);

#endif
