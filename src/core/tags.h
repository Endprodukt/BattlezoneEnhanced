/* tags.h - what each word of a display list belongs to.
 *
 * The game writes a new display list every frame. To move the picture
 * between two frames (render/smooth.h) and to color it (render/color.h)
 * the renderer has to know what a line is: the routines that write the
 * list note a tag next to every word, a kind and a key. The tags are no
 * part of the original's memory and change nothing the game computes.
 */
#ifndef BZ_TAGS_H
#define BZ_TAGS_H

enum {
    BZ_TAG_NONE = 0,
    BZ_TAG_WINDOW,      /* setting up the window and the scale */
    BZ_TAG_TEXT,        /* a message; key: its number */
    BZ_TAG_INFO,        /* the way to the score line */
    BZ_TAG_LIVES,       /* the tanks left */
    BZ_TAG_SCORE,       /* digits; key: 0 the score, 1 the high score */
    BZ_TAG_MOUNT,       /* horizon, mountains, moon (told apart by the vector ROM routine) */
    BZ_TAG_VOLCANO,     /* the way to the volcano (key 0), a rock it throws (key 1 + its number) */
    BZ_TAG_OBJECT,      /* a thing in the world; key: BZ_ID_xxx */
    BZ_TAG_RADAR,       /* the radar's ring */
    BZ_TAG_SWEEP,       /* its sweep line */
    BZ_TAG_BLIP,        /* the enemy's blip on it */
    BZ_TAG_CROSS,       /* the sights; key: 1 with the enemy in them */
    BZ_TAG_CRACK,       /* the cracked windshield */
    BZ_TAG_HISCORE,     /* a row of the high score table; key: the row */
    BZ_TAG_INITIALS,    /* the initials being entered */
    BZ_TAG_END,         /* the list's end */
    BZ_TAG_GROUND,      /* the grid on the ground: not the game's, the renderer's own (render/ground.h) */
    BZ_TAG_FX,          /* the renderer's other additions; key: BZ_FX_xxx (render/enhance.h) */
    BZ_TAG_KINDS
};

#define BZ_TAG(kind, key)  ((uint16_t)(((kind) << 8) | ((key) & 0xff)))
#define BZ_TAG_KIND(tag)   ((tag) >> 8)
#define BZ_TAG_KEY(tag)    ((tag) & 0xff)

/* Things in the world (the key of BZ_TAG_OBJECT, bz_machine.vis_id). */
enum {
    BZ_ID_NONE = 0,
    BZ_ID_TANK,             /* the enemy tank */
    BZ_ID_SUPERTANK,
    BZ_ID_MISSILE,
    BZ_ID_TREAD,            /* the tank's moving treads */
    BZ_ID_ANTENNA,          /* its radar */
    BZ_ID_EXHAUST,          /* the missile's */
    BZ_ID_SHELL,            /* the player's shell */
    BZ_ID_ESHELL,           /* the enemy's */
    BZ_ID_SHELL_HIT,        /* the player's shell exploding */
    BZ_ID_ESHELL_HIT,
    BZ_ID_SAUCER,
    BZ_ID_PIECE = 0x10,     /* + n: a piece of the enemy blown up */
    BZ_ID_FIELD = 0x20,     /* + n: the nth object of the playfield */
    BZ_ID_LOGO = 0x60       /* + n: a part of the BATTLE ZONE logo */
};

#endif
