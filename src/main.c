/* battlezone - Battlezone (Atari, 1980), its program in C.
 *
 * Finds and checks the ROM set (the pictures, the playfield and the tunes
 * are read from it), then runs the game: the interrupt 246 times a second
 * and the main line after it, as the C routines of src/game, and shows
 * what they write into the display list.
 *
 *   battlezone [--roms PATH] [--config INI] [--original] [--nosound] [--hz N]
 *              [--dvg [PORT] | --window | --windowed]
 *
 *   --original  the original program on the 6502 core, to compare with
 *
 * What it adds to the cabinet: pictures between the game's frames
 * (render/smooth.h), a color for every line (render/color.h), the high
 * scores kept in save/hiscore.bin (render/hiscore.h), three ways to drive
 * with every control freely bound (platform/controls.h), and the test menu
 * on the Tab key where all this is chosen (platform/menu.h).
 */
#include "avg/avg.h"
#include "core/emu.h"
#include "core/machine.h"
#include "game/game.h"
#include "game/pace.h"
#include "platform/app.h"
#include "platform/audio.h"
#include "platform/controls.h"
#include "platform/menu.h"
#include "render/color.h"
#include "render/config.h"
#include "render/draw.h"
#include "render/hiscore.h"
#include "render/smooth.h"
#include "rom/romset.h"
#include "sound/sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATHS 48

/* More than this much real time behind (a stall, a debugger) is dropped
 * rather than caught up with. */
#define MAX_CATCHUP_S 0.25

static int rom_paths(int argc, char **argv, const bz_config *cfg, const char **paths)
{
    int n = 0;
    const char *arg = bz_arg(argc, argv, "--roms");
    if (arg) n = bz_romset_add_dir(paths, n, MAX_PATHS, arg);
    if (cfg->rom_path[0]) n = bz_romset_add_dir(paths, n, MAX_PATHS, cfg->rom_path);

    /* The cabinet's own ROM folders: AdvanceMAME's dir_rom. */
    char rcpath[1024];
    vp_rc *rc = NULL;
    if (vp_rc_default_path(rcpath, sizeof rcpath) == VP_OK && vp_rc_load(rcpath, &rc) == VP_OK) {
        const char *dirs = vp_rc_get(rc, "dir_rom");
        if (dirs) {
            static char copy[2048];
            snprintf(copy, sizeof copy, "%s", dirs);
            for (char *p = strtok(copy, ":;"); p && n < MAX_PATHS; p = strtok(NULL, ":;"))
                n = bz_romset_add_dir(paths, n, MAX_PATHS, p);
        }
        vp_rc_free(rc);
    }
    n += bz_romset_default_paths(paths + n, MAX_PATHS - n);
    return n;
}

/* --- the calibration screen ---------------------------------------------- */

/* A picture to set the screen by: the edge of what is shown, the middle,
 * a circle that is round when the proportions are right, and every
 * brightness the game uses. Through the same mapping as the game. */
static int calibration_lines(const bz_config *c, bz_line *out, int max)
{
    int n = 0;
    float l = c->view_left, r = c->view_right, b = c->view_bottom, t = c->view_top;
#define ADD(X0, Y0, X1, Y1, I) do { if (n < max) { bz_line *q = &out[n++]; memset(q, 0, sizeof *q); \
        q->x0 = (X0); q->y0 = (Y0); q->x1 = (X1); q->y1 = (Y1); q->intensity = (uint8_t)(I); } } while (0)
    ADD(l, b, r, b, 15); ADD(r, b, r, t, 15); ADD(r, t, l, t, 15); ADD(l, t, l, b, 15);
    ADD(-30.0f, 0.0f, 30.0f, 0.0f, 15); ADD(0.0f, -30.0f, 0.0f, 30.0f, 15);
    float sx = (r - l) / 4.0f, sy = (t - b) / 3.0f;     /* (the tube is 4 by 3) */
    for (int i = 0; i < 48; i++) {
        float a0 = (float)i / 48.0f * 6.2831853f, a1 = (float)(i + 1) / 48.0f * 6.2831853f;
        ADD(cosf(a0) * sx, sinf(a0) * sy, cosf(a1) * sx, sinf(a1) * sy, 8);
    }
    for (int i = 1; i <= 15; i++) {
        float x = l + 40.0f + (float)(i - 1) * (r - l - 80.0f) / 15.0f;
        ADD(x, t - 30.0f, x + 22.0f, t - 30.0f, i);
        ADD(x, t - 38.0f, x + 22.0f, t - 38.0f, i);
    }
#undef ADD
    return n;
}

static void calibration_adjust(bz_config *c, int mode, int dx, int dy)
{
    switch (mode) {
    case 0:     /* size: the margins shrink or grow the picture */
        c->margin_left -= dx * 16; c->margin_right -= dx * 16;
        c->margin_bottom -= dy * 16; c->margin_top -= dy * 16;
        break;
    case 1:     /* position */
        c->margin_left += dx * 16; c->margin_right -= dx * 16;
        c->margin_bottom += dy * 16; c->margin_top -= dy * 16;
        break;
    default:    /* brightness, and the floor under the dimmest lines */
        c->brightness += (float)dy * 0.05f;
        c->floor += (float)dx * 0.01f;
        if (c->brightness < 0.3f) c->brightness = 0.3f;
        if (c->brightness > 3.0f) c->brightness = 3.0f;
        if (c->floor < 0.0f) c->floor = 0.0f;
        if (c->floor > 0.5f) c->floor = 0.5f;
        break;
    }
}

static void draw_text_lines(vp_dvg *d, const char *const *text, int n, int y)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    vp_color(d, 160, 160, 160);
    for (int i = 0; i < n; i++, y -= 160)
        vp_text(d, text[i], 2048, (float)y, 100, VP_TEXT_CENTER);
}

int main(int argc, char **argv)
{
    static bz_config cfg;
    bz_config_defaults(&cfg);
    char cfg_path[1024], buf[1024];
    const char *arg = bz_arg(argc, argv, "--config");
    if (arg) {
        snprintf(cfg_path, sizeof cfg_path, "%s", arg);
    } else {
        /* Package defaults, then the owner's own in save/. */
        bz_config_load(&cfg, vp_asset_path("battlezone.ini", buf, sizeof buf));
        snprintf(cfg_path, sizeof cfg_path, "%s/battlezone.ini", vp_save_dir());
    }
    bz_config_load(&cfg, cfg_path);

    static bz_romset roms;
    const char *paths[MAX_PATHS];
    int npaths = rom_paths(argc, argv, &cfg, paths);
    int roms_ok = bz_romset_load(&roms, paths, npaths) == 0;
    char missing[512];
    if (roms_ok) printf("battlezone: ROM set complete, program revision %d\n", roms.revision);
    else fprintf(stderr, "battlezone: ROMs missing: %s\n", bz_romset_missing(&roms, missing, sizeof missing));

    bz_app app;
    if (bz_app_open(&app, "Battlezone Enhanced", argc, argv, cfg.board_hz) != 0) return 1;

    static bz_machine m;
    static bz_emu emu;
    static bz_sound snd;
    static bz_controls ctl;
    static bz_avg_list list;
    static bz_smooth sm;
    static bz_solid solid;
    static bz_enhance enh;
    static bz_menu menu;
    bz_pace pace = { 0, 0, 0, 0 };          /* the 6502's time for a frame (game/pace.h) */
    int calibrating = 0, cal_mode = 0, cal_x = 0, cal_y = 0;
    int original = bz_flag(argc, argv, "--original");
    int have_sound = 0;
    char hs_path[1024];
    uint32_t hs_stamp = 0;
    snprintf(hs_path, sizeof hs_path, "%s/hiscore.bin", vp_save_dir());
    char hs_text[1024];                 /* the same table for the VectorPie menu (game.ini: hiscores = yes) */
    snprintf(hs_text, sizeof hs_text, "%s/highscores.txt", vp_save_dir());
    bz_smooth_init(&sm);
    menu.capture = -1;

    bz_machine_init(&m);
    bz_enhance_init(&enh, &m);
    m.in.dsw0 = bz_config_dsw0(&cfg);
    m.in.dsw1 = bz_config_dsw1(&cfg);
    bz_controls_init(&ctl, &cfg);
    bz_controls_configure(&ctl, &cfg, app.in);
    if (roms_ok) {
        bz_machine_set_rom(&m, roms.rom);
        bz_solid_init(&solid, m.mem);
        if (!bz_flag(argc, argv, "--nosound")) {
            bz_sound_init(&snd);
            snd.volume = cfg.volume;
            m.sound_write = bz_sound_write;
            m.sound_request = bz_sound_request;
            m.sound_user = &snd;
            /* Recordings in place of the board's sounds: the owner's, then
             * those that come with the program. */
            char dir[1024];
            snprintf(dir, sizeof dir, "%s/sounds", vp_save_dir());
            int found = bz_samples_load(&snd.smp, dir, BZ_SOUND_RATE);
            found += bz_samples_load(&snd.smp, vp_asset_path("sounds", dir, sizeof dir), BZ_SOUND_RATE);
            snd.smp.enabled = cfg.sound_replace;
            if (found) printf("battlezone: %d sound(s) replaced by recordings%s\n", found,
                              cfg.sound_replace ? "" : " (switched off)");
            have_sound = bz_audio_open(&snd) == 0;
        }
        if (original) {
            bz_emu_init(&emu, &m);
        } else {
            bz_pwron(&m);
            /* The table of the last time instead of the factory's. (Not
             * for the original program: its power-on comes later and
             * writes its own.) */
            if (bz_hiscore_load(&m, hs_path) == 0) printf("battlezone: high scores from %s\n", hs_path);
            hs_stamp = bz_hiscore_stamp(&m);
            bz_hiscore_save_text(&m, hs_text);      /* (for the cabinet's menu and marquee) */
        }
    }

    const double nmi_period = 1.0 / BZ_NMI_HZ;
    double acc = 0.0, dt;
    double game_t = 0.0;                    /* the game's clock: its interrupts so far */
    int status = 0;
    while (bz_app_poll(&app, &dt)) {
        bz_controls_read(&ctl, app.in);

        /* The test menu: Tab opens it, and saves and leaves it. */
        int leave = 0;
        if (ctl.menu && roms_ok && !(menu.open && menu.capture >= 0)) {
            if (calibrating) calibrating = 0;       /* (back to the menu) */
            else if (menu.open) leave = 1;
            else bz_menu_open(&menu);
        } else if (calibrating) {
            /* The calibration screen: a push changes what its mode names. */
            int dx = ctl.nav.x != cal_x ? ctl.nav.x : 0, dy = ctl.nav.y != cal_y ? -ctl.nav.y : 0;
            cal_x = ctl.nav.x;
            cal_y = ctl.nav.y;
            if (ctl.nav.fire) cal_mode = (cal_mode + 1) % 3;
            if (dx || dy) calibration_adjust(&cfg, cal_mode, dx, dy);
        }
        if (menu.open && !leave && !calibrating) {
            switch (bz_menu_update(&menu, &cfg, &ctl, app.clock)) {
            case BZ_MENU_CHANGED:
                bz_controls_configure(&ctl, &cfg, app.in);
                m.in.dsw0 = bz_config_dsw0(&cfg);
                m.in.dsw1 = bz_config_dsw1(&cfg);
                snd.volume = cfg.volume;
                snd.smp.enabled = cfg.sound_replace;
                break;
            case BZ_MENU_SOUND:
                bz_samples_start(&snd.smp, 3, menu.sound);
                break;
            case BZ_MENU_CALIBRATE:
                calibrating = 1;
                cal_x = ctl.nav.x;
                cal_y = ctl.nav.y;
                break;
            case BZ_MENU_RESET_SCORES:
                if (!original) {
                    bz_hiscore_reset(&m);
                    bz_hiscore_save(&m, hs_path);
                    bz_hiscore_save_text(&m, hs_text);
                    hs_stamp = bz_hiscore_stamp(&m);
                }
                break;
            case BZ_MENU_CLOSE:
                leave = 1;
                break;
            }
        }
        if (leave) {
            menu.open = 0;
            menu.capture = -1;
            if (bz_config_save(&cfg, cfg_path) != 0) fprintf(stderr, "battlezone: cannot write %s\n", cfg_path);
        }
        app.hold_cancel = menu.open && menu.capture >= 0;      /* (Esc gives a binding up) */
        /* In the menu the game stands still; its sound test is heard. */
        snd.test = (uint8_t)menu.open;
        if (menu.open && have_sound) {
            bz_sound_run(&snd, (int)((dt > 0.1 ? 0.1 : dt) * 1512000.0));
            bz_audio_balance(&snd);
        }

        if (roms_ok && !menu.open) {
            acc += dt;
            if (acc > MAX_CATCHUP_S) acc = nmi_period;
            while (acc >= nmi_period) {
                acc -= nmi_period;
                bz_controls_tick(&ctl, &m, !original);
                game_t += nmi_period;
                if (have_sound) {
                    /* Where the enemy is heard, and the engine by the levers. */
                    int levers = abs(ctl.left) > abs(ctl.right) ? abs(ctl.left) : abs(ctl.right);
                    snd.enemy_engine = (uint8_t)cfg.enh_enemy_engine;
                    snd.ducking = (uint8_t)cfg.enh_ducking;
                    bz_sound_place(&snd, m.mem, cfg.enh_stereo, cfg.enh_distance);
                    bz_sound_throttle(&snd, cfg.enh_engine && m.analog.on ? (float)levers / 256.0f : -1.0f);
                }
                if (original) {
                    bz_emu_nmi(&emu);
                } else {
                    uint32_t frames = m.frames;
                    bz_nmi(&m);
                    if (cfg.enh_tempo) bz_pace_poll(&m, &pace);
                    else bz_main_poll(&m);
                    if (m.frames != frames) {
                        bz_smooth_frame(&sm, &m, game_t);
                        bz_enhance_frame(&enh, game_t);
                    }
                    bz_sound_tune(&snd, m.mem[BZ_POINT] | m.mem[BZ_POINT + 1] | m.mem[BZ_POINT + 2] | m.mem[BZ_POINT + 3]);
                }
                if (have_sound) bz_sound_run(&snd, BZ_CPU_CYCLES_PER_NMI);
            }
            if (have_sound) bz_audio_balance(&snd);

            /* A new entry in the table (once its initials are in): keep it. */
            if (!original) {
                uint32_t stamp = bz_hiscore_stamp(&m);
                if (stamp && stamp != hs_stamp) {
                    hs_stamp = stamp;
                    if (bz_hiscore_save(&m, hs_path) != 0) fprintf(stderr, "battlezone: cannot write %s\n", hs_path);
                    if (bz_hiscore_save_text(&m, hs_text) != 0) fprintf(stderr, "battlezone: cannot write %s\n", hs_text);
                }
            }
        }

        /* The START lamp, on the cabinet (on a PC it would be the keyboard's NUM LOCK). */
        if (roms_ok && cfg.start_lamp && app.on_board)
            vp_lamps_set(!menu.open && (m.sound_latch & BZ_SND_START_LED) ? VP_LAMP_START1 : 0);

        /* With VP_STATS: how even the program's own pictures come, once a
         * second (the longest and the shortest time between two, the
         * game's frames, and how many pictures stood still). */
        if (app.stats) {
            static double since, lo = 1.0, hi;
            static uint32_t frames0, held0, shown0;
            if (dt < lo) lo = dt;
            if (dt > hi) hi = dt;
            if (app.clock - since >= 1.0) {
                uint32_t dry = 0, dropped = 0;
                if (have_sound) bz_audio_trouble(&dry, &dropped);
                printf("battlezone pace: pictures %.1f to %.1f ms apart, %u game frames, %u of %u pictures stood still, "
                       "period %.1f ms; sound: %u samples short, %u dropped, peak %.0f %%\n", lo * 1000.0, hi * 1000.0,
                       (unsigned)(m.frames - frames0), (unsigned)(sm.held_count - held0),
                       (unsigned)(sm.shown_count - shown0), sm.period * 1000.0, (unsigned)dry, (unsigned)dropped,
                       100.0 * snd.peak);
                snd.peak = 0.0f;
                since = app.clock; lo = 1.0; hi = 0.0;
                frames0 = m.frames; held0 = sm.held_count; shown0 = sm.shown_count;
            }
        }

        bz_app_begin_frame(&app);
        if (!roms_ok) {
            const char *text[] = { "BATTLEZONE ENHANCED", "ROMS NOT FOUND", "NEEDED: MAME SET BZONE", "BZONE.ZIP",
                                   "IN THE ROM FOLDER OR --ROMS" };
            draw_text_lines(app.dvg, text, 5, 2600);
        } else if (calibrating) {
            static bz_line cal[128];
            static const char *const modes[] = { "SIZE", "POSITION", "BRIGHTNESS UP DOWN  FLOOR LEFT RIGHT" };
            char line[96];
            bz_draw_enhance(NULL, 0.0);
            bz_draw_lines(app.dvg, &cfg, m.mem, cal, calibration_lines(&cfg, cal, 128));
            snprintf(line, sizeof line, "%s   FIRE NEXT   TAB BACK", modes[cal_mode]);
            const char *text[] = { line };
            draw_text_lines(app.dvg, text, 1, 700);
        } else if (menu.open) {
            bz_menu_draw(&menu, &cfg, &snd.smp, app.dvg);
        } else if (original) {
            /* (the original's lines say nothing of what they are: no colors of their own) */
            bz_config shown = cfg;
            if (shown.color_mode >= BZ_COLOR_COLOR) shown.color_mode = BZ_COLOR_OVERLAY;
            bz_avg_run(m.mem, BZ_VECRAM, &list, 0);
            bz_draw_enhance(NULL, 0.0);
            bz_draw_lines(app.dvg, &shown, m.mem, list.lines, list.count);
        } else {
            const bz_line *lines;
            sm.solid = cfg.occlusion ? &solid : NULL;
            sm.grid = cfg.ground_grid;
            sm.enh = &enh;
            sm.exact = cfg.smooth;      /* (the objects' points unrounded: without it they move in steps) */
            sm.computed = cfg.smooth == 1;  /* (2: the frame's lines moved instead) */
            /* The lines in a steady order: sorted anew each frame (0.1.5) the beam came at
             * them from other sides and the picture wobbled on the tube (0.1.6); in the
             * game's order the score still did, reached from wherever the world's last
             * moving line ended (0.1.7). The texts and gauges first (0.1.8). */
            sm.order = 2;
            enh.on.particles = cfg.enh_particles; enh.on.stars = cfg.enh_stars;
            enh.on.far = cfg.enh_far; enh.on.volcano = cfg.enh_volcano; enh.on.flash = cfg.enh_flash;
            enh.on.levers = cfg.enh_levers;
            enh.analog = m.analog.on;
            enh.lever_l = ctl.left;
            enh.lever_r = ctl.right;
            bz_draw_enhance(&enh, game_t + acc);
            int n = bz_smooth_lines(&sm, game_t + acc, cfg.smooth, &lines);
            /* The initials' instructions name the cabinet's right hand
             * controller: with other controls, the line is replaced by one
             * that fits them (the English one; the other languages keep
             * their own). */
            float box[4];
            const char *how = NULL;
            if (!strcmp(cfg.language, "english") && cfg.color_mode >= BZ_COLOR_COLOR) {
                /* (by the way of driving used last: they all work at once) */
                if (ctl.mode == BZ_INPUT_ANALOG) how = "CHANGE LETTER WITH YOKE FORWARD OR BACK";
                else if (ctl.mode == BZ_INPUT_SINGLE) how = "CHANGE LETTER WITH JOYSTICK UP OR DOWN";
            }
            bz_draw_leave_out(how ? BZ_TAG(BZ_TAG_TEXT, 0x0a) : 0, box);
            bz_draw_lines(app.dvg, &cfg, m.mem, lines, n);
            bz_draw_leave_out(0, NULL);
            {
                /* Under the game's (C)(P) ATARI 1980, in its color, a little
                 * smaller: who made this one. */
                float cl = 1e9f, cb = 1e9f, cr = -1e9f, ct = -1e9f;
                for (int i = 0; i < n; i++)
                    if (lines[i].tag == BZ_TAG(BZ_TAG_TEXT, 0x2e)) {
                        const bz_line *l = &lines[i];
                        if (l->x0 < cl) cl = l->x0;
                        if (l->x1 < cl) cl = l->x1;
                        if (l->x0 > cr) cr = l->x0;
                        if (l->x1 > cr) cr = l->x1;
                        if (l->y0 < cb) cb = l->y0;
                        if (l->y1 < cb) cb = l->y1;
                        if (l->y0 > ct) ct = l->y0;
                        if (l->y1 > ct) ct = l->y1;
                    }
                if (cl < cr) {
                    float h = (ct - cb) * 0.8f, mid = 0.5f * (cl + cr), base = cb - (ct - cb) * 1.9f;
                    int bx, by, bx2, by2;
                    bz_map_point(&cfg, mid, base, &bx, &by);
                    bz_map_point(&cfg, mid, base + h, &bx2, &by2);
                    uint8_t rgb[3];
                    if (cfg.color_mode >= BZ_COLOR_COLOR) {
                        bz_line fake;
                        memset(&fake, 0, sizeof fake);
                        fake.tag = BZ_TAG(BZ_TAG_TEXT, 0x2e);
                        bz_color_of(m.mem, &fake, cfg.color_mode == BZ_COLOR_FLYER ? BZ_PALETTE_FLYER : BZ_PALETTE_CLASSIC, rgb);
                    } else {
                        memcpy(rgb, cfg.color_mode == BZ_COLOR_OVERLAY ? cfg.color_bottom : cfg.color, 3);
                    }
                    vp_text_font(VP_FONT_ASTEROIDS);
                    vp_color(app.dvg, rgb[0], rgb[1], rgb[2]);
                    vp_text(app.dvg, "ENHANCED BY ENDPRODUKT", (float)bx, (float)by, (float)(by2 - by), VP_TEXT_CENTER);
                }
            }
            if (how && box[0] < box[2]) {
                /* where the game's line was: its middle, its baseline, its height */
                int bx, by, bx2, by2;
                bz_map_point(&cfg, 0.5f * (box[0] + box[2]), box[1], &bx, &by);
                bz_map_point(&cfg, 0.5f * (box[0] + box[2]), box[3], &bx2, &by2);
                uint8_t rgb[3];
                bz_line fake;
                memset(&fake, 0, sizeof fake);
                fake.tag = BZ_TAG(BZ_TAG_TEXT, 0x0a);
                bz_color_of(m.mem, &fake, cfg.color_mode == BZ_COLOR_FLYER ? BZ_PALETTE_FLYER : BZ_PALETTE_CLASSIC, rgb);
                vp_text_font(VP_FONT_ASTEROIDS);
                vp_color(app.dvg, rgb[0], rgb[1], rgb[2]);
                vp_text(app.dvg, how, (float)bx, (float)by, (float)(by2 - by), VP_TEXT_CENTER);
            }
        }
        if (bz_app_end_frame(&app) != 0) {
            status = 1;
            break;
        }
    }
    bz_audio_close();
    bz_controls_close(&ctl);
    bz_app_close(&app);
    bz_avg_list_free(&list);
    bz_smooth_free(&sm);
    bz_solid_free(&solid);
    return status;
}
