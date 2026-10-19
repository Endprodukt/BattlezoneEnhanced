#include "platform/controls.h"
#include "platform/vpbind.h"

#include <SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COIN_PULSE_NMIS 25     /* about 100 ms closed: well inside 16..800 ms */

enum { B_AUTO = 0, B_NONE, B_KEY, B_BUTTON, B_AXIS, B_HAT, B_MOUSE_BUTTON };

static const char *const HAT_NAME[4] = { "up", "right", "down", "left" };
static const char *const MOUSE_BUTTON[3] = { "left", "middle", "right" };
static const uint8_t HAT_BIT[4] = { SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT };

/* --- bindings as text ------------------------------------------------------------ */

static void parse_bind(const char *spec, bz_bind *b)
{
    int joy, k;
    char word[16];
    memset(b, 0, sizeof *b);
    b->type = B_AUTO;
    if (!strcmp(spec, "none")) {
        b->type = B_NONE;
    } else if (!strncmp(spec, "mouse button ", 13)) {
        for (int i = 0; i < 3; i++)
            if (!strcmp(spec + 13, MOUSE_BUTTON[i])) {
                b->type = B_MOUSE_BUTTON;
                b->index = (int16_t)(i + 1);        /* SDL_BUTTON_LEFT ... */
            }
    } else if (!strncmp(spec, "key ", 4)) {
        SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
        if (spec[4] == '#') sc = (SDL_Scancode)atoi(spec + 5);
        else sc = SDL_GetScancodeFromName(spec + 4);
        if (sc > SDL_SCANCODE_UNKNOWN && sc < SDL_NUM_SCANCODES) {
            b->type = B_KEY;
            b->index = (int16_t)sc;
        }
    } else if (sscanf(spec, "joy%d button %d", &joy, &k) == 2 && joy >= 1 && joy <= BZ_MAX_JOYSTICKS) {
        b->type = B_BUTTON;
        b->joy = (uint8_t)(joy - 1);
        b->index = (int16_t)k;
    } else if (sscanf(spec, "joy%d axis %d %1s", &joy, &k, word) == 3 && joy >= 1 && joy <= BZ_MAX_JOYSTICKS) {
        b->type = B_AXIS;
        b->joy = (uint8_t)(joy - 1);
        b->index = (int16_t)k;
        b->dir = word[0] == '-' ? -1 : 1;
    } else if (sscanf(spec, "joy%d hat %d %15s", &joy, &k, word) == 3 && joy >= 1 && joy <= BZ_MAX_JOYSTICKS) {
        for (int i = 0; i < 4; i++)
            if (!strcmp(word, HAT_NAME[i])) {
                b->type = B_HAT;
                b->joy = (uint8_t)(joy - 1);
                b->index = (int16_t)k;
                b->dir = (int8_t)HAT_BIT[i];
            }
    }
}

/* --- joysticks ------------------------------------------------------------------- */

static void open_joysticks(bz_controls *c)
{
    /* (SDL counts its opens: these are the SDK's joysticks once more) */
    for (int i = 0; i < c->joys; i++)
        if (c->joy[i]) SDL_JoystickClose((SDL_Joystick *)c->joy[i]);
    c->joys = SDL_NumJoysticks();
    if (c->joys > BZ_MAX_JOYSTICKS) c->joys = BZ_MAX_JOYSTICKS;
    if (c->joys < 0) c->joys = 0;
    for (int i = 0; i < c->joys; i++) c->joy[i] = SDL_JoystickOpen(i);
}

static float axis_of(const bz_controls *c, int joy, int axis)
{
    if (joy >= c->joys || !c->joy[joy]) return 0.0f;
    return (float)SDL_JoystickGetAxis((SDL_Joystick *)c->joy[joy], axis) / 32767.0f;
}

/* --- reading --------------------------------------------------------------------- */

/* A binding of the program's own: 0..1 (an axis: -1..1, the way that
 * counts positive). */
static float bound(const bz_controls *c, const bz_bind *b, const Uint8 *keys)
{
    SDL_Joystick *j = b->joy < c->joys ? (SDL_Joystick *)c->joy[b->joy] : NULL;
    switch (b->type) {
    case B_KEY:    return keys[b->index] ? 1.0f : 0.0f;
    case B_BUTTON: return j && SDL_JoystickGetButton(j, b->index) ? 1.0f : 0.0f;
    case B_HAT:    return j && (SDL_JoystickGetHat(j, b->index) & (uint8_t)b->dir) ? 1.0f : 0.0f;
    case B_AXIS:   return axis_of(c, b->joy, b->index) * (float)b->dir;
    case B_MOUSE_BUTTON: return SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(b->index) ? 1.0f : 0.0f;
    default:       return 0.0f;
    }
}

/* What "auto" is for a switch, read directly as every other binding (not
 * by advmame.rc's names): the keys, the first stick and the mouse's buttons
 * AdvanceMAME gives these by default, and the levers' keys as before. */
static const char *const AUTO_SPEC[BZ_ACT_COUNT] = {
    "key Up or joy1 axis 1 -", "key Down or joy1 axis 1 +", "key Left or joy1 axis 0 -", "key Right or joy1 axis 0 +",
    "key E", "key D", "key I", "key K",
    "", "",                                 /* (the yoke's axes: yoke() below) */
    "key Left Ctrl or joy1 button 0 or mouse button left",
    "key 1", "key 5",
    "",                                     /* (the Tab key, always: below) */
};
static bz_bind g_auto[BZ_ACT_COUNT][BZ_BIND_ALTS];
static int g_nauto[BZ_ACT_COUNT];

/* A switch: any of its bindings ("auto" the ones above). */
static int held(const bz_controls *c, vp_input *in, const Uint8 *keys, int act)
{
    (void)in;
    for (int i = 0; i < c->binds[act]; i++) {
        const bz_bind *b = &c->bind[act][i];
        if (b->type == B_AUTO) {
            for (int a = 0; a < g_nauto[act]; a++)
                if (bound(c, &g_auto[act][a], keys) > 0.5f) return 1;
        } else if (bound(c, b, keys) > 0.5f) {
            return 1;
        }
    }
    return 0;
}

static float dead(float v, float zone)
{
    float a = fabsf(v);
    if (a <= zone) return 0.0f;
    a = (a - zone) / (1.0f - zone);
    if (a > 1.0f) a = 1.0f;
    return v < 0.0f ? -a : a;
}

/* An axis of the yoke, -1..1: forward and right positive; of several
 * bindings the one pushed furthest. (The arrow keys are no longer its
 * ends: they are the one joystick's, or on the cabinet the left lever.) */
static float yoke(const bz_controls *c, vp_input *in, const Uint8 *keys, int act)
{
    float best = 0.0f;
    for (int i = 0; i < c->binds[act]; i++) {
        const bz_bind *b = &c->bind[act][i];
        float v = 0.0f;
        if (b->type == B_AXIS) v = dead(bound(c, b, keys), c->deadzone);
        else if (b->type == B_AUTO) {   /* the yoke (by its name), else the first stick: read directly */
            int j = vpb_yoke() >= 0 ? vpb_yoke() : 0;
            (void)in;
            v = dead(act == BZ_ACT_DRIVE ? -axis_of(c, j, 1) : axis_of(c, j, 0), c->deadzone);   /* (up is the low end) */
        }
        if (fabsf(v) > fabsf(best)) best = v;
    }
    return best > 1.0f ? 1.0f : best < -1.0f ? -1.0f : best;
}

/* Bindings as written ("auto or joy2 button 3"): each of the " or " parts. */
static int parse_binds(const char *spec, bz_bind *out)
{
    int n = 0;
    for (const char *p = spec; n < BZ_BIND_ALTS;) {
        const char *q = strstr(p, " or ");
        char one[BZ_BIND_SIZE];
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len >= sizeof one) len = sizeof one - 1;
        memcpy(one, p, len);
        one[len] = '\0';
        parse_bind(one, &out[n]);
        if (out[n].type != B_NONE || n == 0) n++;   /* ("none" only alone) */
        if (!q) break;
        p = q + 4;
    }
    if (n > 1 && out[0].type == B_NONE) {           /* (a "none" first and something after it) */
        memmove(out, out + 1, (size_t)(n - 1) * sizeof *out);
        n--;
    }
    return n;
}

static int16_t lever(float v)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (int16_t)lroundf(v * 256.0f);
}

void bz_controls_init(bz_controls *c, const bz_config *cfg)
{
    memset(c, 0, sizeof *c);
    c->cap_action = -1;
    c->mode = BZ_INPUT_SINGLE;
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    open_joysticks(c);
    bz_controls_configure(c, cfg, NULL);
}

void bz_controls_close(bz_controls *c)
{
    for (int i = 0; i < c->joys; i++)
        if (c->joy[i]) SDL_JoystickClose((SDL_Joystick *)c->joy[i]);
    c->joys = 0;
}

void bz_controls_configure(bz_controls *c, const bz_config *cfg, vp_input *in)
{
    c->deadzone = cfg->deadzone < 0.0f ? 0.0f : cfg->deadzone > 0.5f ? 0.5f : cfg->deadzone;
    c->menu_hold = cfg->menu_hold;
    for (int i = 0; i < BZ_ACT_COUNT; i++) {
        c->binds[i] = parse_binds(cfg->bind[i], c->bind[i]);
        g_nauto[i] = AUTO_SPEC[i][0] ? parse_binds(AUTO_SPEC[i], g_auto[i]) : 0;
    }
    if (in) vp_input_set_deadzone(in, c->deadzone);
}

void bz_controls_read(bz_controls *c, vp_input *in)
{
    if (SDL_NumJoysticks() != c->joys && SDL_NumJoysticks() <= BZ_MAX_JOYSTICKS) open_joysticks(c);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    int up = held(c, in, keys, BZ_ACT_UP), down = held(c, in, keys, BZ_ACT_DOWN);
    int left = held(c, in, keys, BZ_ACT_LEFT), right = held(c, in, keys, BZ_ACT_RIGHT);
    int lf = held(c, in, keys, BZ_ACT_LEFT_FWD), lb = held(c, in, keys, BZ_ACT_LEFT_BACK);
    int rf = held(c, in, keys, BZ_ACT_RIGHT_FWD), rb = held(c, in, keys, BZ_ACT_RIGHT_BACK);
    float drive = yoke(c, in, keys, BZ_ACT_DRIVE), turn = yoke(c, in, keys, BZ_ACT_TURN);
    int fire = held(c, in, keys, BZ_ACT_FIRE), start = held(c, in, keys, BZ_ACT_START);
    if (up && down) up = down = 0;
    if (left && right) left = right = 0;
    if (lf && lb) lf = lb = 0;              /* a lever is forward or back, never both */
    if (rf && rb) rf = rb = 0;

    /* For a menu: its own controls only, whatever is bound (vpbind.h) - the
     * arrow keys, any stick, hat or the yoke; START, Enter and the keypad's
     * Enter to do. No binding acts in it, none can take it away. */
    bz_nav nav = { 0, 0, 0 };
    c->in = in;
    vpb_update();
    nav.y = vpb_menu_way_y();
    nav.x = vpb_menu_way_x();
    int fire_now = vpb_menu_confirm_held(in);
    nav.fire = fire_now && !c->fire_was;
    c->fire_was = fire_now;
    c->nav = nav;

    int menu_now = keys[SDL_SCANCODE_TAB] || held(c, in, keys, BZ_ACT_MENU);
    /* A cabinet without a key for it: start and fire held together. */
    if (c->menu_hold && fire && start) {
        if (!c->hold_since) c->hold_since = SDL_GetTicks() | 1u;
        else if (SDL_GetTicks() - c->hold_since >= 2000u) menu_now = 1;
    } else {
        c->hold_since = 0;
    }
    c->menu = menu_now && !c->menu_was;
    c->menu_was = menu_now;

    /* Which way drives now: the two joysticks if a lever is held, else the
     * yoke if it is out of its dead zone, else the one joystick; nothing
     * touched: the way used last (controls.h). */
    if (lf || lb || rf || rb) c->mode = BZ_INPUT_DUAL;
    else if (fabsf(drive) > 0.02f || fabsf(turn) > 0.02f) c->mode = BZ_INPUT_ANALOG;
    else if (up || down || left || right) c->mode = BZ_INPUT_SINGLE;

    /* The levers. */
    int a = 0, b = 0, d = 0, e = 0;         /* left forward, left back, right forward, right back */
    c->left = c->right = 0;
    switch (c->mode) {
    case BZ_INPUT_DUAL:
        a = lf; b = lb; d = rf; e = rb;
        break;
    case BZ_INPUT_ANALOG:
        /* The yoke as the two levers: forward pushes both, to the right
         * the left one more and the right one less. */
        c->left = lever(drive + turn);
        c->right = lever(drive - turn);
        break;
    default:
        if (up) {
            if (!right) d = 1;              /* (up and left: the right lever alone, a left turn forward) */
            if (!left) a = 1;
        } else if (down) {
            if (!right) b = 1;              /* (down and left: the left lever back alone) */
            if (!left) e = 1;
        } else if (left) {
            b = 1; d = 1;                   /* pivot left */
        } else if (right) {
            a = 1; e = 1;                   /* pivot right */
        }
        break;
    }
    uint8_t v = 0;
    if (a) v |= BZ_IN3_LEFT_FWD;
    if (b) v |= BZ_IN3_LEFT_BACK;
    if (d) v |= BZ_IN3_RIGHT_FWD;
    if (e) v |= BZ_IN3_RIGHT_BACK;
    if (fire) v |= BZ_IN3_FIRE;
    if (start) v |= BZ_IN3_START;
    c->in3 = v;

    int coin = held(c, in, keys, BZ_ACT_COIN);
    if (coin && !c->coin_was && c->coin_pulse[0] == 0) c->coin_pulse[0] = COIN_PULSE_NMIS;
    c->coin_was = coin;
    if (vp_input_pressed(in, "coin2") && c->coin_pulse[1] == 0) c->coin_pulse[1] = COIN_PULSE_NMIS;
}

void bz_controls_tick(bz_controls *c, bz_machine *m, int native)
{
    uint8_t in0 = 0x3f;                     /* nothing closed (active low) */
    if (c->coin_pulse[0] > 0) { in0 &= (uint8_t)~BZ_IN0_COIN_R; c->coin_pulse[0]--; }
    if (c->coin_pulse[1] > 0) { in0 &= (uint8_t)~BZ_IN0_COIN_M; c->coin_pulse[1]--; }
    m->in.in0 = in0;

    uint8_t in3 = c->in3;
    int analog = c->mode == BZ_INPUT_ANALOG;
    /* The yoke as the lever switches too, past half way: the C routines
     * drive by the analog levers and ignore these while driving, but the
     * entry of initials reads the right lever's switches (LETCHK), and
     * the original program knows nothing else. */
    if (analog) {
        if (c->left > 128) in3 |= BZ_IN3_LEFT_FWD;
        if (c->left < -128) in3 |= BZ_IN3_LEFT_BACK;
        if (c->right > 128) in3 |= BZ_IN3_RIGHT_FWD;
        if (c->right < -128) in3 |= BZ_IN3_RIGHT_BACK;
    }
    m->in.in3 = in3;
    m->analog.on = (uint8_t)(analog && native);
    m->analog.left = c->left;
    m->analog.right = c->right;
}

/* --- a new binding --------------------------------------------------------------- */

/* Never bound: the menu's keys (Tab, F2, Esc, Enter, the keypad's Enter,
 * Delete); for what is not itself the start, the coin or the menu, also
 * the cabinet's start and coin. */
static int kept_key(int action, int sc)
{
    if (sc == SDL_SCANCODE_TAB || sc == SDL_SCANCODE_F2 || sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_RETURN
        || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_KP_PERIOD) return 1;
    if (action >= BZ_ACT_START) return 0;
    for (int k = BZ_ACT_START; k <= BZ_ACT_COIN; k++)
        for (int i = 0; i < g_nauto[k]; i++)
            if (g_auto[k][i].type == B_KEY && g_auto[k][i].index == sc) return 1;
    return 0;
}

void bz_controls_capture_begin(bz_controls *c, int action)
{
    c->cap_action = action;
    c->cap_armed = 0;
    c->cap_still = 0;
    for (int j = 0; j < BZ_MAX_JOYSTICKS; j++)
        for (int a = 0; a < BZ_MAX_AXES; a++)
            c->cap_rest[j][a] = j < c->joys && c->joy[j] ? SDL_JoystickGetAxis((SDL_Joystick *)c->joy[j], a) : 0;
}

int bz_controls_capture_poll(bz_controls *c, char *spec, size_t size)
{
    if (c->cap_action < 0) return -1;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int analog = c->cap_action == BZ_ACT_DRIVE || c->cap_action == BZ_ACT_TURN;
    int found = 0, moving = 0;
    spec[0] = '\0';

    /* Leaving it as it was: Esc, Enter (START too, where it is not START's
     * own line); START 2 makes it none, Delete auto. */
    if (c->cap_armed) {
        int cabinet_line = c->cap_action >= BZ_ACT_START;
        if (keys[SDL_SCANCODE_ESCAPE] || vpb_key_pressed(SDL_SCANCODE_RETURN) || vpb_key_pressed(SDL_SCANCODE_KP_ENTER)
            || (!cabinet_line && c->in && vp_input_pressed(c->in, "start1"))) {
            c->cap_action = -1;
            return -1;
        }
        if (!cabinet_line && c->in && vp_input_pressed(c->in, "start2")) { c->cap_action = -1; snprintf(spec, size, "none"); return 2; }
        if (vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD)) {
            c->cap_action = -1;
            snprintf(spec, size, "auto");
            return 2;
        }
        if (!cabinet_line)              /* the start or the coin is the cabinet's */
            for (int k = BZ_ACT_START; k <= BZ_ACT_COIN; k++)
                for (int i = 0; i < g_nauto[k]; i++)
                    if (g_auto[k][i].type == B_KEY && keys[g_auto[k][i].index]) return 0;
    }
    for (int sc = 1; sc < SDL_NUM_SCANCODES && !found; sc++) {
        if (!keys[sc] || kept_key(c->cap_action, sc)) continue;
        found = 1;
        if (analog) continue;               /* (a key is no axis: only keeps the wait from arming) */
        const char *name = SDL_GetScancodeName((SDL_Scancode)sc);
        if (name && *name) snprintf(spec, size, "key %s", name);
        else snprintf(spec, size, "key #%d", sc);
    }
    for (int j = 0; j < c->joys && !found; j++) {
        SDL_Joystick *joy = (SDL_Joystick *)c->joy[j];
        if (!joy) continue;
        int axes = SDL_JoystickNumAxes(joy);
        if (axes > BZ_MAX_AXES) axes = BZ_MAX_AXES;
        for (int a = 0; a < axes && !found; a++) {
            int v = SDL_JoystickGetAxis(joy, a) - c->cap_rest[j][a];
            if (v > 3000 || v < -3000) moving = 1;
            if (v > 19000 || v < -19000) {
                found = 1;
                /* The yoke's own two: pushed forward or to the right now.
                 * A switch: the way it was pushed. */
                snprintf(spec, size, "joy%d axis %d %c", j + 1, a, v > 0 ? '+' : '-');
            }
        }
        if (analog) continue;
        for (int b = 0; b < SDL_JoystickNumButtons(joy) && !found; b++)
            if (SDL_JoystickGetButton(joy, b)) {
                found = 1;
                snprintf(spec, size, "joy%d button %d", j + 1, b);
            }
        for (int h = 0; h < SDL_JoystickNumHats(joy) && !found; h++) {
            uint8_t v = SDL_JoystickGetHat(joy, h);
            for (int i = 0; i < 4 && !found; i++)
                if (v & HAT_BIT[i]) {
                    found = 1;
                    snprintf(spec, size, "joy%d hat %d %s", j + 1, h, HAT_NAME[i]);
                }
        }
    }
    if (!analog && !found) {            /* the mouse's buttons */
        Uint32 m = SDL_GetMouseState(NULL, NULL);
        for (int i = 0; i < 3 && !found; i++)
            if (m & SDL_BUTTON(i + 1)) { found = 1; snprintf(spec, size, "mouse button %s", MOUSE_BUTTON[i]); }
    }
    if (!c->cap_armed) {
        /* What opened the line (fire, a stick still held over) has to be
         * let go first. */
        if (found || moving) {
            /* (where the sticks rest is taken once they lie still) */
            int action = c->cap_action;
            bz_controls_capture_begin(c, action);
        } else if (++c->cap_still >= 12) {
            c->cap_armed = 1;
        }
        return 0;
    }
    if (!found || !spec[0]) return 0;
    c->cap_action = -1;
    return 1;
}
