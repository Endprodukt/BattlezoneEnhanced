#include "render/color.h"

#include "core/bz_symbols.h"
#include "core/tags.h"

#define RGB(r, g, b) do { rgb[0] = (r); rgb[1] = (g); rgb[2] = (b); return; } while (0)

/* The vector ROM's routines for the horizon and the first section of the
 * mountains, which has the moon after its 16th vector. */
#define MTN0 0x3016

/* The flyer's colors, where they differ from the classic ones. Returns 0
 * for what it leaves to those. */
static int flyer(const uint8_t *mem, const bz_line *l, uint8_t rgb[3])
{
    unsigned key = BZ_TAG_KEY(l->tag);
#define SET(r, g, b) do { rgb[0] = (r); rgb[1] = (g); rgb[2] = (b); return 1; } while (0)
    switch (BZ_TAG_KIND(l->tag)) {
    case BZ_TAG_MOUNT:
        if (l->sub == BZ_HORIZN) SET(70, 130, 255);             /* the horizon: the blue of the ground */
        if (l->sub == MTN0 && l->n >= 16) SET(255, 150, 40);    /* the moon */
        SET(205, 175, 255);                                     /* mountains of ice, lilac */
    case BZ_TAG_VOLCANO:
        SET(255, 90, 30);
    case BZ_TAG_OBJECT:
        if (key >= BZ_ID_LOGO) {
            if (key - BZ_ID_LOGO >= 2) SET(255, 130, 20);       /* ZONE */
            SET(255, 220, 40);                                  /* BATTLE */
        }
        if (key >= BZ_ID_FIELD) {
            unsigned obj = mem[(BZ_PTBLO1 + 2 * (key - BZ_ID_FIELD)) & 0x7fff];
            /* each of the four shapes its own shade, from cyan to magenta */
            if (obj == 0x00) SET(40, 210, 255);                 /* pyramids */
            if (obj == 0x0c) SET(100, 130, 255);
            if (obj == 0x0f) SET(165, 95, 255);                 /* cubes */
            SET(225, 85, 255);
        }
        if (key >= BZ_ID_PIECE) SET(255, 170, 40);
        switch (key) {
        case BZ_ID_TANK:       SET(60, 255, 80);                /* (green, as under the cabinet's overlay) */
        case BZ_ID_SUPERTANK:  SET(255, 50, 40);
        case BZ_ID_MISSILE:    SET(255, 200, 40);
        case BZ_ID_TREAD:      SET(130, 255, 140);
        case BZ_ID_ANTENNA:    SET(210, 255, 120);
        case BZ_ID_EXHAUST:    SET(255, 255, 220);
        case BZ_ID_SHELL:      SET(190, 255, 255);
        case BZ_ID_ESHELL:     SET(255, 120, 40);
        case BZ_ID_SHELL_HIT:
        case BZ_ID_ESHELL_HIT: SET(255, 240, 150);
        case BZ_ID_SAUCER:     SET(255, 80, 230);
        default:               return 0;
        }
    case BZ_TAG_GROUND: SET(50, 100, 255);
    case BZ_TAG_RADAR:  SET(255, 60, 40);
    case BZ_TAG_SWEEP:  SET(255, 220, 40);
    case BZ_TAG_BLIP:   SET(255, 255, 255);
    case BZ_TAG_CROSS:
        if (key) SET(255, 70, 40);                              /* the enemy in the sights */
        SET(255, 220, 40);
    case BZ_TAG_LIVES:  SET(60, 255, 80);
    case BZ_TAG_SCORE:
        if (key) SET(255, 140, 30);                             /* the high score */
        SET(255, 220, 40);
    case BZ_TAG_HISCORE: {
        /* The table: the logo's yellow at the top down to the sky's red. */
        static const uint8_t rows[10][3] = {
            { 255, 235, 60 }, { 255, 215, 50 }, { 255, 195, 45 }, { 255, 172, 40 }, { 255, 150, 38 },
            { 255, 128, 36 }, { 255, 106, 36 }, { 255, 86, 40 }, { 255, 66, 50 }, { 250, 50, 70 },
        };
        const uint8_t *c = rows[key < 10 ? key : 9];
        SET(c[0], c[1], c[2]);
    }
    case BZ_TAG_TEXT:
        switch (key) {
        case 0x18: SET(255, 220, 40);                           /* SCORE */
        case 0x0e: SET(255, 140, 30);                           /* HIGH SCORE */
        case 0x1a: case 0x1e: SET(255, 220, 40);                /* HIGH SCORES, GREAT SCORE */
        case 0x08: case 0x0a: case 0x0c: SET(200, 230, 255);    /* how to enter initials */
        case 0x20: case 0x22: case 0x24: case 0x26: SET(200, 230, 255);    /* coins and plays */
        case 0x2a: case 0x2c: SET(255, 140, 30);                /* BONUS TANK AT ... */
        case 0x2e: SET(90, 140, 255);                           /* (C)(P) ATARI 1980 */
        default:   return 0;
        }
    default:
        return 0;
    }
#undef SET
}

const uint8_t *bz_color_horizon(int palette)
{
    static const uint8_t HORIZON[2][3] = { { 30, 190, 70 }, { 70, 130, 255 } };
    return HORIZON[palette == BZ_PALETTE_FLYER];
}

void bz_color_of(const uint8_t *mem, const bz_line *l, int palette, uint8_t rgb[3])
{
    unsigned key = BZ_TAG_KEY(l->tag);
    if (palette == BZ_PALETTE_FLYER && flyer(mem, l, rgb)) return;

    switch (BZ_TAG_KIND(l->tag)) {
    case BZ_TAG_MOUNT:
        if (l->sub == BZ_HORIZN) RGB(30, 190, 70);              /* the horizon */
        if (l->sub == MTN0 && l->n >= 16) RGB(255, 236, 160);   /* the moon */
        RGB(40, 220, 90);                                       /* mountains */
    case BZ_TAG_VOLCANO:
        RGB(255, 120, 30);
    case BZ_TAG_OBJECT:
        if (key >= BZ_ID_LOGO) {
            if (key - BZ_ID_LOGO >= 2) RGB(255, 225, 40);       /* ZONE */
            RGB(255, 120, 20);                                  /* BATTLE */
        }
        if (key >= BZ_ID_FIELD) {
            /* (PTBLO1: the object's number, then its angle) */
            unsigned obj = mem[(BZ_PTBLO1 + 2 * (key - BZ_ID_FIELD)) & 0x7fff];
            if (obj == 0x00 || obj == 0x0c) RGB(70, 175, 255);  /* pyramids */
            RGB(120, 140, 255);                                 /* cubes */
        }
        if (key >= BZ_ID_PIECE) RGB(255, 140, 30);              /* pieces of the wreck */
        switch (key) {
        case BZ_ID_TANK:       RGB(120, 255, 60);               /* (green, as under the cabinet's overlay) */
        case BZ_ID_SUPERTANK:  RGB(255, 60, 210);
        case BZ_ID_MISSILE:    RGB(255, 165, 0);
        case BZ_ID_TREAD:      RGB(170, 255, 120);
        case BZ_ID_ANTENNA:    RGB(255, 220, 60);
        case BZ_ID_EXHAUST:    RGB(255, 240, 120);
        case BZ_ID_SHELL:      RGB(255, 255, 200);
        case BZ_ID_ESHELL:     RGB(255, 90, 40);
        case BZ_ID_SHELL_HIT:
        case BZ_ID_ESHELL_HIT: RGB(255, 235, 60);
        case BZ_ID_SAUCER:     RGB(190, 110, 255);
        default:               RGB(255, 255, 255);
        }
    case BZ_TAG_GROUND: RGB(30, 150, 70);
    case BZ_TAG_RADAR:  RGB(255, 50, 50);
    case BZ_TAG_SWEEP:  RGB(60, 255, 90);
    case BZ_TAG_BLIP:   RGB(255, 240, 80);
    case BZ_TAG_CROSS:
        if (key) RGB(255, 235, 60);                             /* the enemy in the sights */
        RGB(60, 255, 90);
    case BZ_TAG_CRACK:  RGB(235, 245, 255);
    case BZ_TAG_LIVES:  RGB(60, 255, 90);
    case BZ_TAG_SCORE:
        if (key) RGB(80, 220, 255);                             /* the high score */
        RGB(255, 225, 40);
    case BZ_TAG_INITIALS: RGB(255, 255, 255);
    case BZ_TAG_HISCORE: {
        /* The table: from yellow at the top down to blue. */
        static const uint8_t rows[10][3] = {
            { 255, 240, 60 }, { 255, 205, 50 }, { 255, 165, 45 }, { 255, 120, 60 }, { 250, 85, 100 },
            { 225, 80, 160 }, { 185, 90, 215 }, { 140, 105, 250 }, { 100, 135, 255 }, { 70, 170, 255 },
        };
        const uint8_t *c = rows[key < 10 ? key : 9];
        RGB(c[0], c[1], c[2]);
    }
    case BZ_TAG_TEXT:
        switch (key) {
        case 0x00: case 0x02: case 0x04: case 0x06:             /* ENEMY TO LEFT / RIGHT / REAR */
        case 0x10:                                              /* ENEMY IN RANGE */
            RGB(255, 50, 50);
        case 0x12: RGB(255, 150, 30);                           /* MOTION BLOCKED BY OBJECT */
        case 0x14: RGB(255, 50, 50);                            /* GAME OVER */
        case 0x16: case 0x28: RGB(255, 255, 255);               /* PRESS START, INSERT COIN */
        case 0x20: case 0x22: case 0x24: case 0x26: RGB(60, 255, 90);  /* coins and plays */
        case 0x18: RGB(255, 225, 40);                           /* SCORE */
        case 0x0e: RGB(80, 220, 255);                           /* HIGH SCORE */
        case 0x1a: RGB(255, 225, 40);                           /* HIGH SCORES */
        case 0x1e: RGB(255, 225, 40);                           /* GREAT SCORE */
        case 0x08: case 0x0a: case 0x0c: RGB(80, 220, 255);     /* how to enter initials */
        case 0x2a: case 0x2c: RGB(60, 255, 90);                 /* BONUS TANK AT ... */
        case 0x2e: RGB(110, 140, 255);                          /* (C)(P) ATARI 1980 */
        default:   RGB(255, 255, 255);
        }
    default:
        RGB(255, 255, 255);
    }
}
