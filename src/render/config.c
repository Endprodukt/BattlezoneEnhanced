#include "render/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bz_config_defaults(bz_config *c)
{
    memset(c, 0, sizeof *c);
    /* What the game draws in: its window is 506 units wide and its top
     * line (the tanks left) 191 up, a picture of 4 by 3. (MAME shows more,
     * -290..290 by -200..200, with a black edge around the game.) */
    c->view_left = -256.0f;
    c->view_right = 256.0f;
    c->view_bottom = -192.0f;
    c->view_top = 192.0f;
    c->margin_left = c->margin_right = c->margin_bottom = c->margin_top = 64;
    c->gamma = 1.0f;
    c->floor = 0.15f;
    c->brightness = 1.3f;
    c->color_mode = BZ_COLOR_FLYER;
    c->smooth = 1;
    c->occlusion = 1;
    c->sound_replace = 1;       /* (the futuristic shots and explosions that come with the program, 0.1.9) */
    c->ground_grid = 1;
    c->enh_particles = c->enh_stars = c->enh_far = c->enh_volcano = c->enh_flash = 1;
    c->enh_levers = 1;
    c->board_hz = 50.0f;
    c->enh_stereo = c->enh_distance = c->enh_engine = c->enh_enemy_engine = c->enh_ducking = 1;
    c->start_lamp = 1;
    c->menu_hold = 1;           /* (radar, arrow and the cabinet's pace change how the game plays: off) */
    c->deadzone = 0.08f;
    for (int i = 0; i < BZ_ACT_COUNT; i++) snprintf(c->bind[i], BZ_BIND_SIZE, "auto");
    c->color[0] = 255; c->color[1] = 255; c->color[2] = 255;
    c->color_top[0] = 255; c->color_top[1] = 32; c->color_top[2] = 32;
    c->color_bottom[0] = 32; c->color_bottom[1] = 255; c->color_bottom[2] = 32;
    c->overlay_split = 0.2f;
    c->lives = 3;
    c->missile_at = 10000;
    c->bonus_at = 15000;
    snprintf(c->language, sizeof c->language, "english");
    snprintf(c->coin_mode, sizeof c->coin_mode, "1play");
    c->volume = 0.8f;
}

static const char *const ACTION_NAME[BZ_ACT_COUNT] = {
    "up", "down", "left", "right", "left_fwd", "left_back", "right_fwd", "right_back",
    "drive", "turn", "fire", "start", "coin", "menu",
};
static const char *const COLOR_NAME[] = { "white", "overlay", "color", "flyer" };

const char *bz_config_action_name(int action)
{
    return action >= 0 && action < BZ_ACT_COUNT ? ACTION_NAME[action] : "";
}

int bz_config_bind_add(bz_config *c, int action, const char *spec)
{
    if (action < 0 || action >= BZ_ACT_COUNT || !spec[0]) return 0;
    char *list = c->bind[action];
    if (!strcmp(list, "none") || !list[0]) {
        snprintf(list, BZ_BIND_SIZE, "%s", spec);
        return 1;
    }
    /* the ones there, and where the last begins */
    int n = 1;
    const char *last = list;
    for (const char *p = list;; p += 4) {
        const char *q = strstr(p, " or ");
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == strlen(spec) && !strncmp(p, spec, len)) return 0;
        if (!q) break;
        p = q;
        last = q + 4;
        n++;
    }
    char out[2 * BZ_BIND_SIZE];         /* (room to see that it is too long) */
    if (n >= BZ_BIND_ALTS) snprintf(out, sizeof out, "%.*s%s", (int)(last - list), list, spec);
    else snprintf(out, sizeof out, "%s or %s", list, spec);
    if (strlen(out) >= BZ_BIND_SIZE - 1) return 0;
    memcpy(list, out, strlen(out) + 1);      /* (it fits: checked above) */
    return 1;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static int parse_rgb(const char *val, uint8_t rgb[3])
{
    int r, g, b;
    if (sscanf(val, "%d %d %d", &r, &g, &b) != 3) return -1;
    rgb[0] = (uint8_t)r; rgb[1] = (uint8_t)g; rgb[2] = (uint8_t)b;
    return 0;
}

static int set(bz_config *c, const char *key, const char *val)
{
    struct { const char *k; float *f; } floats[] = {
        { "view_left", &c->view_left }, { "view_right", &c->view_right },
        { "view_bottom", &c->view_bottom }, { "view_top", &c->view_top },
        { "gamma", &c->gamma }, { "floor", &c->floor }, { "brightness", &c->brightness },
        { "overlay_split", &c->overlay_split }, { "volume", &c->volume }, { "deadzone", &c->deadzone },
        { "board_hz", &c->board_hz },
    };
    struct { const char *k; int *i; } ints[] = {
        { "margin_left", &c->margin_left }, { "margin_right", &c->margin_right },
        { "margin_bottom", &c->margin_bottom }, { "margin_top", &c->margin_top },
        { "flip_x", &c->flip_x }, { "flip_y", &c->flip_y },
        { "lives", &c->lives }, { "missile_at", &c->missile_at }, { "bonus_at", &c->bonus_at },
        { "smooth", &c->smooth }, { "occlusion", &c->occlusion },
        { "sound_replace", &c->sound_replace }, { "ground_grid", &c->ground_grid },
        { "enh_particles", &c->enh_particles }, { "enh_stars", &c->enh_stars },
        { "enh_far", &c->enh_far }, { "enh_volcano", &c->enh_volcano }, { "enh_flash", &c->enh_flash },
        { "enh_levers", &c->enh_levers }, { "enh_stereo", &c->enh_stereo }, { "enh_distance", &c->enh_distance },
        { "enh_enemy_engine", &c->enh_enemy_engine }, { "enh_ducking", &c->enh_ducking },
        { "enh_engine", &c->enh_engine }, { "enh_tempo", &c->enh_tempo }, { "menu_hold", &c->menu_hold },
        { "start_lamp", &c->start_lamp },
    };
    struct { const char *k; char *s; size_t n; } strs[] = {
        { "language", c->language, sizeof c->language }, { "coin_mode", c->coin_mode, sizeof c->coin_mode },
        { "rom_path", c->rom_path, sizeof c->rom_path },
    };
    struct { const char *k; uint8_t *rgb; } colors[] = {
        { "color", c->color }, { "color_top", c->color_top }, { "color_bottom", c->color_bottom },
    };
    for (unsigned i = 0; i < sizeof floats / sizeof floats[0]; i++)
        if (!strcmp(key, floats[i].k)) { *floats[i].f = (float)atof(val); return 0; }
    for (unsigned i = 0; i < sizeof ints / sizeof ints[0]; i++)
        if (!strcmp(key, ints[i].k)) { *ints[i].i = atoi(val); return 0; }
    for (unsigned i = 0; i < sizeof strs / sizeof strs[0]; i++)
        if (!strcmp(key, strs[i].k)) { snprintf(strs[i].s, strs[i].n, "%s", val); return 0; }
    for (unsigned i = 0; i < sizeof colors / sizeof colors[0]; i++)
        if (!strcmp(key, colors[i].k)) return parse_rgb(val, colors[i].rgb);
    if (!strcmp(key, "color_mode")) {
        for (int i = 0; i < 4; i++)
            if (!strcmp(val, COLOR_NAME[i])) { c->color_mode = i; return 0; }
        return -1;
    }
    /* settings of earlier versions, no longer there: taken without a word
     * (controls: single, dual or analog; since 0.1.13 all three at once) */
    static const char *const GONE[] = { "enh_glow", "enh_tracks", "enh_fine_grid", "enh_radar", "enh_arrow", "controls",
                                        "enh_haze" };
    for (unsigned i = 0; i < sizeof GONE / sizeof GONE[0]; i++)
        if (!strcmp(key, GONE[i])) return 0;
    if (!strncmp(key, "bind_", 5)) {
        for (int i = 0; i < BZ_ACT_COUNT; i++)
            if (!strcmp(key + 5, ACTION_NAME[i])) { snprintf(c->bind[i], BZ_BIND_SIZE, "%s", *val ? val : "auto"); return 0; }
        return -1;
    }
    return -1;
}

int bz_config_load(bz_config *c, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        char *p = trim(line);
        if (!*p || *p == ';' || *p == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p), *val = trim(eq + 1);
        if (set(c, key, val) != 0) fprintf(stderr, "%s: setting not understood: %s = %s\n", path, key, val);
    }
    fclose(f);
    return 0;
}

int bz_config_save(const bz_config *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "; Battlezone Enhanced: the owner's settings (see the README)\n");
    fprintf(f, "rom_path = %s\n", c->rom_path);
    fprintf(f, "lives = %d\nmissile_at = %d\nbonus_at = %d\nlanguage = %s\ncoin_mode = %s\n",
            c->lives, c->missile_at, c->bonus_at, c->language, c->coin_mode);
    fprintf(f, "deadzone = %.2f\n", c->deadzone);
    for (int i = 0; i < BZ_ACT_COUNT; i++) fprintf(f, "bind_%s = %s\n", ACTION_NAME[i], c->bind[i]);
    fprintf(f, "volume = %.2f\nsound_replace = %d\nsmooth = %d\nocclusion = %d\n", c->volume, c->sound_replace,
            c->smooth, c->occlusion);
    fprintf(f, "ground_grid = %d\n", c->ground_grid);
    fprintf(f, "enh_particles = %d\nenh_stars = %d\nenh_far = %d\nenh_volcano = %d\nenh_flash = %d\n"
               "enh_levers = %d\n",
            c->enh_particles, c->enh_stars, c->enh_far, c->enh_volcano, c->enh_flash, c->enh_levers);
    fprintf(f, "enh_stereo = %d\nenh_distance = %d\nenh_engine = %d\nenh_tempo = %d\nmenu_hold = %d\nstart_lamp = %d\n",
            c->enh_stereo, c->enh_distance, c->enh_engine, c->enh_tempo, c->menu_hold, c->start_lamp);
    fprintf(f, "enh_enemy_engine = %d\nenh_ducking = %d\n", c->enh_enemy_engine, c->enh_ducking);
    fprintf(f, "color_mode = %s\n", COLOR_NAME[c->color_mode & 3]);
    fprintf(f, "color = %d %d %d\ncolor_top = %d %d %d\ncolor_bottom = %d %d %d\noverlay_split = %.3f\n",
            c->color[0], c->color[1], c->color[2], c->color_top[0], c->color_top[1], c->color_top[2],
            c->color_bottom[0], c->color_bottom[1], c->color_bottom[2], c->overlay_split);
    fprintf(f, "brightness = %.2f\ngamma = %.2f\nfloor = %.2f\nboard_hz = %.0f\n", c->brightness, c->gamma, c->floor,
            c->board_hz);
    fprintf(f, "view_left = %.1f\nview_right = %.1f\nview_bottom = %.1f\nview_top = %.1f\n",
            c->view_left, c->view_right, c->view_bottom, c->view_top);
    fprintf(f, "margin_left = %d\nmargin_right = %d\nmargin_bottom = %d\nmargin_top = %d\nflip_x = %d\nflip_y = %d\n",
            c->margin_left, c->margin_right, c->margin_bottom, c->margin_top, c->flip_x, c->flip_y);
    fclose(f);
    return 0;
}

uint8_t bz_config_dsw0(const bz_config *c)
{
    unsigned v = 0;
    int lives = c->lives < 2 ? 2 : (c->lives > 5 ? 5 : c->lives);
    v |= (unsigned)(lives - 2);
    v |= (c->missile_at >= 30000 ? 3u : c->missile_at >= 20000 ? 2u : c->missile_at >= 10000 ? 1u : 0u) << 2;
    v |= (c->bonus_at >= 50000 ? 3u : c->bonus_at >= 25000 ? 2u : c->bonus_at >= 15000 ? 1u : 0u) << 4;
    if (!strcmp(c->language, "german")) v |= 0x40;
    else if (!strcmp(c->language, "french")) v |= 0x80;
    else if (!strcmp(c->language, "spanish")) v |= 0xc0;
    return (uint8_t)v;
}

uint8_t bz_config_dsw1(const bz_config *c)
{
    if (!strcmp(c->coin_mode, "free")) return 0x00;
    if (!strcmp(c->coin_mode, "2plays")) return 0x01;
    if (!strcmp(c->coin_mode, "2coins")) return 0x03;
    return 0x02;
}
