/* menu.c - see menu.h. */
#include "platform/menu.h"
#include "platform/vpbind.h"

#include <SDL.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Every line the menu can show, on whichever page. */
enum {
    L_GAP = 0,
    L_PAGE_CONTROLS, L_PAGE_PICTURE, L_PAGE_SOUND, L_PAGE_OPTIONS, L_VOLUME, L_REPLACE, L_SOUND_TEST,
    L_RESET_SCORES, L_EXIT, L_BACK,
    L_DEADZONE, L_DEFAULTS,
    L_SMOOTH, L_OCCLUSION, L_GRID, L_COLORS, L_BRIGHTNESS,
    L_LIVES, L_MISSILE, L_BONUS, L_LANGUAGE, L_COINS,
    L_PAGE_ENHANCED, L_CALIBRATE,
    L_TOG0,                 /* + T_xxx: an option that is on or off */
    L_BIND0 = L_TOG0 + 24,  /* + BZ_ACT_xxx */
    L_END = 0xff
};

enum { P_MAIN, P_CONTROLS, P_PICTURE, P_OPTIONS, P_SOUND, P_ENHANCED, N_PAGES };

/* The options that are on or off. */
enum {
    T_PARTICLES, T_STARS, T_FAR, T_VOLCANO, T_FLASH, T_LEVERS,
    T_STEREO, T_DISTANCE, T_ENGINE, T_TEMPO, T_MENU_HOLD, T_ENEMY_ENGINE, T_DUCKING, T_COUNT
};
static const struct { const char *label; size_t at; } TOGGLE[T_COUNT] = {
    { "EXPLOSION SPARKS", offsetof(bz_config, enh_particles) },
    { "STARS", offsetof(bz_config, enh_stars) },
    { "FAR MOUNTAINS", offsetof(bz_config, enh_far) },
    { "VOLCANO GLOW", offsetof(bz_config, enh_volcano) },
    { "MUZZLE FLASH AND DUST", offsetof(bz_config, enh_flash) },
    { "LEVER GAUGES", offsetof(bz_config, enh_levers) },
    { "STEREO BY DIRECTION", offsetof(bz_config, enh_stereo) },
    { "QUIETER FAR AWAY", offsetof(bz_config, enh_distance) },
    { "ENGINE FOLLOWS SPEED", offsetof(bz_config, enh_engine) },
    { "CABINET PACE", offsetof(bz_config, enh_tempo) },
    { "START AND FIRE HELD", offsetof(bz_config, menu_hold) },
    { "ENEMY ENGINES", offsetof(bz_config, enh_enemy_engine) },
    { "ENGINES DUCK UNDER EFFECTS", offsetof(bz_config, enh_ducking) },
};
static int *toggle_of(const bz_config *cfg, int t) { return (int *)((char *)cfg + TOGGLE[t].at); }

#define PAGE_MAX 28

static const char *const TITLE[N_PAGES] = { "BATTLEZONE ENHANCED  TEST MENU", "CONTROLS", "PICTURE", "GAME OPTIONS", "SOUND", "ENHANCED PICTURE" };

static const char *const BIND_LABEL[BZ_ACT_COUNT] = {
    "JOYSTICK UP", "JOYSTICK DOWN", "JOYSTICK LEFT", "JOYSTICK RIGHT",
    "LEFT JOYSTICK UP", "LEFT JOYSTICK DOWN", "RIGHT JOYSTICK UP", "RIGHT JOYSTICK DOWN",
    "YOKE FORWARD", "YOKE RIGHT",
    "FIRE", "START", "COIN", "TEST MENU",
};

static const char *const LANGUAGES[4] = { "english", "german", "french", "spanish" };
static const char *const COINS[4] = { "free", "2plays", "1play", "2coins" };
static const char *const COINS_TEXT[4] = { "FREE PLAY", "1 COIN 2 PLAYS", "1 COIN 1 PLAY", "2 COINS 1 PLAY" };
static const int MISSILE_AT[4] = { 5000, 10000, 20000, 30000 };
static const int BONUS_AT[4] = { 0, 15000, 25000, 50000 };

/* The lines of a page, gaps included. The controls' page shows the
 * bindings of the way of driving that is chosen. */
static int page_lines(int page, const bz_config *cfg, uint8_t *out)
{
    int n = 0;
#define ADD(l) out[n++] = (uint8_t)(l)
    switch (page) {
    case P_CONTROLS:
        /* All three ways of driving at once (controls.h): every binding. */
        (void)cfg;
        for (int a = BZ_ACT_UP; a <= BZ_ACT_RIGHT; a++) ADD(L_BIND0 + a);
        ADD(L_GAP);
        for (int a = BZ_ACT_LEFT_FWD; a <= BZ_ACT_RIGHT_BACK; a++) ADD(L_BIND0 + a);
        ADD(L_GAP);
        ADD(L_BIND0 + BZ_ACT_DRIVE); ADD(L_BIND0 + BZ_ACT_TURN); ADD(L_DEADZONE); ADD(L_TOG0 + T_ENGINE);
        ADD(L_GAP);
        ADD(L_BIND0 + BZ_ACT_FIRE); ADD(L_BIND0 + BZ_ACT_START); ADD(L_BIND0 + BZ_ACT_COIN);
        ADD(L_BIND0 + BZ_ACT_MENU); ADD(L_TOG0 + T_MENU_HOLD);
        ADD(L_GAP); ADD(L_DEFAULTS); ADD(L_BACK);
        break;
    case P_PICTURE:
        ADD(L_SMOOTH); ADD(L_OCCLUSION); ADD(L_GRID); ADD(L_COLORS); ADD(L_BRIGHTNESS); ADD(L_GAP);
        ADD(L_PAGE_ENHANCED); ADD(L_CALIBRATE); ADD(L_GAP); ADD(L_BACK);
        break;
    case P_ENHANCED:
        for (int t = T_PARTICLES; t <= T_FLASH; t++) ADD(L_TOG0 + t);
        ADD(L_GAP); ADD(L_TOG0 + T_LEVERS);
        ADD(L_GAP); ADD(L_BACK);
        break;
    case P_SOUND:
        ADD(L_VOLUME); ADD(L_TOG0 + T_STEREO); ADD(L_TOG0 + T_DISTANCE); ADD(L_TOG0 + T_ENEMY_ENGINE);
        ADD(L_TOG0 + T_DUCKING); ADD(L_GAP);
        ADD(L_REPLACE); ADD(L_SOUND_TEST); ADD(L_GAP); ADD(L_BACK);
        break;
    case P_OPTIONS:
        ADD(L_REPLACE); ADD(L_GAP);          /* (also on SOUND: original or enhanced at a glance; the colors only on PICTURE) */
        ADD(L_LIVES); ADD(L_MISSILE); ADD(L_BONUS); ADD(L_LANGUAGE); ADD(L_COINS); ADD(L_GAP);
        ADD(L_TOG0 + T_TEMPO); ADD(L_GAP); ADD(L_BACK);
        break;
    default:
        ADD(L_PAGE_CONTROLS); ADD(L_PAGE_PICTURE); ADD(L_PAGE_SOUND); ADD(L_PAGE_OPTIONS); ADD(L_GAP);
        ADD(L_RESET_SCORES); ADD(L_GAP); ADD(L_EXIT);
        break;
    }
#undef ADD
    out[n] = L_END;
    return n;
}

/* The nth line that is no gap, and how many there are. */
static int chosen_line(const uint8_t *lines, int item, int *count)
{
    int k = 0, line = L_GAP;
    for (int i = 0; lines[i] != L_END; i++) {
        if (lines[i] == L_GAP) continue;
        if (k == item) line = lines[i];
        k++;
    }
    *count = k;
    return line;
}

static int step(int v, int d, int lo, int hi)
{
    v += d;
    return v > hi ? lo : v < lo ? hi : v;
}

static int index_of_int(const int *list, int n, int v)
{
    int best = 0;
    for (int i = 0; i < n; i++)
        if (list[i] <= v) best = i;
    return best;
}

static int index_of_str(const char *const *list, int n, const char *v, int fallback)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(list[i], v)) return i;
    return fallback;
}

static float stepf(float v, int d, float by, float lo, float hi)
{
    v += (float)d * by;
    return v < lo ? lo : v > hi ? hi : v;
}

void bz_menu_open(bz_menu *u)
{
    u->open = 1;
    u->page = P_MAIN;
    u->capture = -1;
    u->adding = 0;
    u->confirm = 0;
    u->dir_x = u->dir_y = 0;
}

int bz_menu_update(bz_menu *u, bz_config *cfg, bz_controls *ctl, double now)
{
    if (u->page < 0 || u->page >= N_PAGES) u->page = P_MAIN;

    if (u->capture >= 0) {
        char spec[BZ_BIND_SIZE];
        int r = bz_controls_capture_poll(ctl, spec, sizeof spec);
        if (r == 0) return BZ_MENU_NOTHING;
        int action = u->capture;
        u->capture = -1;
        /* (what was pressed is still held: it is not to count as a push) */
        u->dir_x = ctl->nav.x;
        u->dir_y = ctl->nav.y;
        u->repeat_at = now + 0.6;
        ctl->fire_was = 1;
        if (r < 0) return BZ_MENU_NOTHING;
        if (r == 2) {                   /* none or auto */
            u->adding = 0;
            if (action == BZ_ACT_MENU && !strcmp(spec, "none")) return BZ_MENU_NOTHING;   /* (never none: Tab must stay) */
            snprintf(cfg->bind[action], BZ_BIND_SIZE, "%s", spec);
            return BZ_MENU_CHANGED;
        }
        /* The first since the cursor came to the line replaces what was
         * bound; every further one is added (what is there still counts). */
        if (u->adding != action + 1 || !strcmp(cfg->bind[action], "auto") || !strcmp(cfg->bind[action], "none")) {
            snprintf(cfg->bind[action], BZ_BIND_SIZE, "%s", spec);
            u->adding = action + 1;
            return BZ_MENU_CHANGED;
        }
        bz_config_bind_add(cfg, action, spec);
        return BZ_MENU_CHANGED;
    }

    uint8_t lines[PAGE_MAX + 1];
    int n;
    page_lines(u->page, cfg, lines);
    int line = chosen_line(lines, u->item[u->page], &n);
    if (u->item[u->page] >= n) {
        u->item[u->page] = 0;
        line = chosen_line(lines, 0, &n);
    }

    /* Delete on a binding (also the keypad's): back to auto */
    if ((vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD)) && line >= L_BIND0) {
        u->adding = 0;
        if (!strcmp(cfg->bind[line - L_BIND0], "auto")) return BZ_MENU_NOTHING;
        snprintf(cfg->bind[line - L_BIND0], BZ_BIND_SIZE, "auto");
        return BZ_MENU_CHANGED;
    }

    /* a push counts once, and again and again while it is held */
    int x = ctl->nav.x, y = ctl->nav.y, dx = 0, dy = 0;
    if (x != u->dir_x || y != u->dir_y) {
        dx = x != u->dir_x ? x : 0;
        dy = y != u->dir_y ? y : 0;
        u->repeat_at = now + 0.5;
    } else if ((x || y) && now >= u->repeat_at) {
        dx = x;
        dy = y;
        u->repeat_at = now + 0.15;
    }
    u->dir_x = x;
    u->dir_y = y;
    int fire = ctl->nav.fire;

    if (dy) {
        u->item[u->page] = step(u->item[u->page], dy, 0, n - 1);
        u->confirm = 0;
        u->adding = 0;                  /* the next binding replaces again */
        return BZ_MENU_NOTHING;
    }
    int d = dx ? dx : (fire ? 1 : 0);
    if (!d) return BZ_MENU_NOTHING;

    if (line >= L_BIND0) {
        int action = line - L_BIND0;
        if (fire) {
            u->capture = action;
            bz_controls_capture_begin(ctl, action);
            return BZ_MENU_NOTHING;
        }
        /* left or right: NONE, or AUTO again (the menu's line never none) */
        const char *to = strcmp(cfg->bind[action], "none") && action != BZ_ACT_MENU ? "none" : "auto";
        u->adding = 0;
        if (!strcmp(cfg->bind[action], to)) return BZ_MENU_NOTHING;
        snprintf(cfg->bind[action], BZ_BIND_SIZE, "%s", to);
        return BZ_MENU_CHANGED;
    }
    if (line >= L_TOG0 && line < L_TOG0 + T_COUNT) {
        int *v = toggle_of(cfg, line - L_TOG0);
        *v = !*v;
        return BZ_MENU_CHANGED;
    }
    switch (line) {
    case L_PAGE_ENHANCED: if (fire) u->page = P_ENHANCED; break;
    case L_CALIBRATE:     if (fire) return BZ_MENU_CALIBRATE; break;
    case L_PAGE_CONTROLS: if (fire) { u->page = P_CONTROLS; u->adding = 0; } break;
    case L_PAGE_PICTURE:  if (fire) u->page = P_PICTURE; break;
    case L_PAGE_OPTIONS:  if (fire) u->page = P_OPTIONS; break;
    case L_PAGE_SOUND:    if (fire) u->page = P_SOUND; break;
    case L_REPLACE:
        cfg->sound_replace = !cfg->sound_replace;
        return BZ_MENU_CHANGED;
    case L_SOUND_TEST:
        if (dx) u->sound = step(u->sound, dx, 0, BZ_SMP_COUNT - 1);
        if (fire) return BZ_MENU_SOUND;
        break;
    case L_BACK:          if (fire) u->page = u->page == P_ENHANCED ? P_PICTURE : P_MAIN; break;
    case L_EXIT:          if (fire) return BZ_MENU_CLOSE; break;
    case L_VOLUME:
        cfg->volume = stepf(cfg->volume, d, 0.1f, 0.0f, 1.0f);
        return BZ_MENU_CHANGED;
    case L_RESET_SCORES:
        if (!fire) break;
        if (!u->confirm) { u->confirm = 1; break; }
        u->confirm = 0;
        return BZ_MENU_RESET_SCORES;
    case L_DEADZONE:
        cfg->deadzone = stepf(cfg->deadzone, d, 0.02f, 0.0f, 0.4f);
        return BZ_MENU_CHANGED;
    case L_DEFAULTS:
        if (!fire) break;
        for (int i = 0; i < BZ_ACT_COUNT; i++) snprintf(cfg->bind[i], BZ_BIND_SIZE, "auto");
        return BZ_MENU_CHANGED;
    case L_SMOOTH:
        cfg->smooth = !cfg->smooth;
        return BZ_MENU_CHANGED;
    case L_OCCLUSION:
        cfg->occlusion = !cfg->occlusion;
        return BZ_MENU_CHANGED;
    case L_GRID:
        cfg->ground_grid = !cfg->ground_grid;
        return BZ_MENU_CHANGED;
    case L_COLORS:
        cfg->color_mode = step(cfg->color_mode, -d, 0, 3);      /* (flyer, classic, overlay, white) */
        return BZ_MENU_CHANGED;
    case L_BRIGHTNESS:
        cfg->brightness = stepf(cfg->brightness, d, 0.1f, 0.3f, 3.0f);
        return BZ_MENU_CHANGED;
    case L_LIVES:
        cfg->lives = step(cfg->lives < 2 ? 2 : cfg->lives > 5 ? 5 : cfg->lives, d, 2, 5);
        return BZ_MENU_CHANGED;
    case L_MISSILE:
        cfg->missile_at = MISSILE_AT[step(index_of_int(MISSILE_AT, 4, cfg->missile_at), d, 0, 3)];
        return BZ_MENU_CHANGED;
    case L_BONUS:
        cfg->bonus_at = BONUS_AT[step(index_of_int(BONUS_AT, 4, cfg->bonus_at), d, 0, 3)];
        return BZ_MENU_CHANGED;
    case L_LANGUAGE:
        snprintf(cfg->language, sizeof cfg->language, "%s",
                 LANGUAGES[step(index_of_str(LANGUAGES, 4, cfg->language, 0), d, 0, 3)]);
        return BZ_MENU_CHANGED;
    case L_COINS:
        snprintf(cfg->coin_mode, sizeof cfg->coin_mode, "%s",
                 COINS[step(index_of_str(COINS, 4, cfg->coin_mode, 2), d, 0, 3)]);
        return BZ_MENU_CHANGED;
    }
    return BZ_MENU_NOTHING;
}

static void upper(char *s)
{
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* A line's label and its value ("" for none). */
static void describe(int line, const bz_menu *u, const bz_config *cfg, const bz_samples *smp,
                     const char **label, char *value, size_t size)
{
    static const char *const COLORS[4] = { "WHITE", "ORIGINAL", "CLASSIC", "FLYER" };    /* (ORIGINAL: the cabinet's overlay, green, the top red) */
    value[0] = '\0';
    if (line >= L_BIND0) {
        int action = line - L_BIND0;
        *label = BIND_LABEL[action];
        if (u->capture == action) {
            snprintf(value, size, "%s", action == BZ_ACT_DRIVE ? "PUSH IT FORWARD" :
                                        action == BZ_ACT_TURN ? "PUSH IT TO THE RIGHT" : "PRESS THE BUTTON");
        } else {
            /* all of them by what they are ("LEFT CTRL + JOY1 BUTTON 1"),
             * " + " between them */
            size_t k = 0;
            value[0] = '\0';
            for (const char *p = cfg->bind[action]; *p;) {
                const char *q = strstr(p, " or ");
                size_t len = q ? (size_t)(q - p) : strlen(p);
                char one[64], word[64], rest[16];
                int j, n;
                snprintf(one, sizeof one, "%.*s", (int)(len < 63 ? len : 63), p);
                if (!strncmp(one, "key ", 4)) snprintf(word, sizeof word, "%s", one + 4);
                else if (sscanf(one, "joy%d button %d", &j, &n) == 2) snprintf(word, sizeof word, "joy%d button %d", j, n + 1);
                else if (sscanf(one, "joy%d axis %d %15s", &j, &n, rest) == 3) snprintf(word, sizeof word, "joy%d axis %d %s", j, n + 1, rest);
                else if (sscanf(one, "joy%d hat %d %15s", &j, &n, rest) == 3) snprintf(word, sizeof word, "joy%d hat %s", j, rest);
                else if (!strncmp(one, "mouse button ", 13)) snprintf(word, sizeof word, "mouse %s", one + 13);
                else snprintf(word, sizeof word, "%s", one);
                if (k && k + 3 < size) { memcpy(value + k, " + ", 3); k += 3; }
                for (const char *w = word; *w && k + 1 < size; w++) value[k++] = *w;
                value[k] = '\0';
                if (!q) break;
                p = q + 4;
            }
            upper(value);
        }
        return;
    }
    if (line >= L_TOG0 && line < L_TOG0 + T_COUNT) {
        *label = TOGGLE[line - L_TOG0].label;
        snprintf(value, size, "%s", *toggle_of(cfg, line - L_TOG0) ? "ON" : "OFF");
        return;
    }
    switch (line) {
    case L_PAGE_ENHANCED: *label = "ENHANCED PICTURE"; break;
    case L_CALIBRATE:     *label = "SCREEN CALIBRATION"; break;
    case L_PAGE_CONTROLS: *label = "CONTROLS"; break;
    case L_PAGE_PICTURE:  *label = "PICTURE"; break;
    case L_PAGE_OPTIONS:  *label = "GAME OPTIONS"; break;
    case L_PAGE_SOUND:    *label = "SOUND"; break;
    case L_REPLACE:       *label = "SOUNDS"; snprintf(value, size, "%s", cfg->sound_replace ? "ENHANCED" : "ORIGINAL"); break;
    case L_SOUND_TEST: {
        int id = u->sound >= 0 && u->sound < BZ_SMP_COUNT ? u->sound : 0;
        *label = "SOUND TEST";
        snprintf(value, size, "%s%s", bz_sample_name(id), smp && smp->sound[id].pcm ? "" : "  NO FILE");
        break;
    }
    case L_VOLUME:        *label = "VOLUME"; snprintf(value, size, "%d", (int)(cfg->volume * 10.0f + 0.5f)); break;
    case L_RESET_SCORES:  *label = "RESET HIGH SCORES"; snprintf(value, size, "%s", u->confirm ? "START AGAIN" : ""); break;
    case L_EXIT:          *label = "SAVE AND EXIT"; break;
    case L_BACK:          *label = "BACK"; break;
    case L_DEADZONE:      *label = "YOKE DEAD ZONE"; snprintf(value, size, "%d", (int)(cfg->deadzone * 100.0f + 0.5f)); break;
    case L_DEFAULTS:      *label = "ALL BACK TO AUTO"; break;
    case L_SMOOTH:        *label = "SMOOTH MOTION"; snprintf(value, size, "%s", cfg->smooth ? "ON" : "OFF"); break;
    case L_OCCLUSION:     *label = "SOLID OBJECTS"; snprintf(value, size, "%s", cfg->occlusion ? "ON" : "OFF"); break;
    case L_GRID:          *label = "GROUND GRID"; snprintf(value, size, "%s", cfg->ground_grid ? "ON" : "OFF"); break;
    case L_COLORS:        *label = "COLORS"; snprintf(value, size, "%s", COLORS[cfg->color_mode & 3]); break;
    case L_BRIGHTNESS:    *label = "BRIGHTNESS"; snprintf(value, size, "%d", (int)(cfg->brightness * 10.0f + 0.5f)); break;
    case L_LIVES:         *label = "TANKS"; snprintf(value, size, "%d", cfg->lives); break;
    case L_MISSILE:       *label = "MISSILE AT"; snprintf(value, size, "%d", MISSILE_AT[index_of_int(MISSILE_AT, 4, cfg->missile_at)]); break;
    case L_BONUS: {
        int b = BONUS_AT[index_of_int(BONUS_AT, 4, cfg->bonus_at)];
        *label = "BONUS TANK AT";
        if (b) snprintf(value, size, "%d AND 100000", b);
        else snprintf(value, size, "NONE");
        break;
    }
    case L_LANGUAGE:      *label = "LANGUAGE"; snprintf(value, size, "%s", cfg->language); upper(value); break;
    case L_COINS:         *label = "COINS"; snprintf(value, size, "%s", COINS_TEXT[index_of_str(COINS, 4, cfg->coin_mode, 2)]); break;
    default:              *label = "?"; break;
    }
}

void bz_menu_draw(const bz_menu *u, const bz_config *cfg, const bz_samples *smp, vp_dvg *d)
{
    int page = u->page >= 0 && u->page < N_PAGES ? u->page : P_MAIN;
    float STEP = 190.0f, GAP = 110.0f, H = 85.0f;
    const float LEFT = 620.0f, RIGHT = 3560.0f;
    uint8_t lines[PAGE_MAX + 1];
    page_lines(page, cfg, lines);
    {
        /* a long page (the controls: every binding) is written smaller to fit */
        float tall = 0.0f;
        for (int i = 0; lines[i] != L_END; i++) tall += lines[i] ? STEP : GAP;
        if (tall > 2800.0f) {
            float k = 2800.0f / tall;
            STEP *= k; GAP *= k; H *= k;
        }
    }

    vp_text_font(VP_FONT_ASTEROIDS);
    vp_color(d, 255, 200, 40);
    vp_text(d, TITLE[page], 2048, 3680, 95, VP_TEXT_CENTER);

    /* the lines, from the top, the gaps where the page has them; the
     * block centred on the screen */
    float height = 0.0f;
    for (int i = 0; lines[i] != L_END; i++) height += lines[i] ? STEP : GAP;
    float y = 2048.0f + height * 0.5f - STEP * 0.5f + 100.0f;
    int k = 0, chosen_line_id = L_GAP;
    for (int i = 0; lines[i] != L_END; i++) {
        int line = lines[i];
        if (!line) { y -= GAP; continue; }
        const char *label;
        char value[BZ_BIND_SIZE + 8];
        describe(line, u, cfg, smp, &label, value, sizeof value);
        int chosen = k == u->item[page];
        if (chosen) chosen_line_id = line;
        if (chosen) {                   /* an arrow before the line chosen */
            vp_color(d, 255, 255, 255);
            vp_moveto(d, (int)LEFT - 160, (int)(y + H * 0.88f));
            vp_lineto(d, (int)LEFT - 70, (int)(y + H * 0.5f));
            vp_lineto(d, (int)LEFT - 160, (int)(y + H * 0.12f));
        } else {
            vp_color(d, 60, 200, 90);
        }
        vp_text(d, label, LEFT, y, H, VP_TEXT_LEFT);
        if (value[0]) {
            /* (a long binding is written smaller rather than into its label) */
            float room = RIGHT - LEFT - vp_text_width(label, H) - 150.0f, w = vp_text_width(value, H), h = H;
            if (w > room && w > 0.0f) h = H * room / w;
            if (chosen) vp_color(d, 255, 255, 255);
            else vp_color(d, 80, 220, 255);
            if (u->capture >= 0 && chosen) vp_color(d, 255, 60, 50);
            vp_text(d, value, RIGHT, y, h, VP_TEXT_RIGHT);
        }
        y -= STEP;
        k++;
    }
    vp_color(d, 110, 130, 255);
    const char *one = "UP DOWN CHOOSE   LEFT RIGHT CHANGE", *two = "START OR ENTER DO   TAB SAVE AND EXIT";
    if (u->capture >= 0) {
        int cabinet_line = u->capture >= BZ_ACT_START;
        one = u->adding == u->capture + 1 ? "PRESS THE BUTTON TO ADD" : "PRESS THE BUTTON";
        two = cabinet_line ? "ENTER - BACK   DEL - AUTO" : "START - BACK  DEL - AUTO  START 2 - NONE";
    } else if (chosen_line_id >= L_BIND0) {
        one = "LEFT RIGHT - AUTO OR NONE";
        two = "START, ENTER - ADD BUTTON   DEL - AUTO";
    }
    vp_text(d, one, 2048, 380, 55, VP_TEXT_CENTER);
    vp_text(d, two, 2048, 270, 55, VP_TEXT_CENTER);
}
