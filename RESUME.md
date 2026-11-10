# Carrying on

The state and what is open are in [STATUS.md](STATUS.md), building and
checking in [BUILD.md](BUILD.md). This is what you need to know to carry on
with the code.

## Layout

```
upstream/                the original source (historicalsource/battlezone), read only
external/VectorPie/      the SDK (0.9.0 or later), unchanged
src/core/machine.[ch]    the board: memory 0000-7FFF, switches, POKEY registers, sound latch
src/core/bz_symbols.h    every address under its original name (generated: tools/mksyms.py)
src/core/emu.[ch]        the original program on the 6502 core (the reference)
src/cpu/m6502.[ch]       the 6502 core
src/mbox/                the math box
src/avg/                 the vector generator: display list -> lines, with the window
src/game/m6502.h         the macros the C routines are written in
src/game/mainline.c      MAIN, WNSHLD, BATTLE, the high score pages, MSGS, INFO
src/game/view.c          MOUNTS, VOLCNO, ROTATE, ROTPNT, EXPLDE, DRAW, PNTPUT, sine
src/game/play.c          shells, collisions, MOTION, ROBOT, BUZBOM, SAUCMV, TRACK, DRADAR
src/game/nmi.c           the interrupt and PWRON
src/game/vgut.c          VGUT (writing the display list) and DIGITS
src/game/sound.c         BZSOUN (SNDON, MODSND)
src/game/coin.c          MOOLAH
src/game/pace.[ch]       the cabinet's pace (enh_tempo)
src/sound/               the POKEY and the discrete sounds
src/sound/samples.[ch]   sounds replaced by WAV files
src/core/tags.h          what a line is: the drawing routines' notes (vgtag)
src/render/smooth.[ch]   pictures between the frames
src/render/color.[ch]    every line's color
src/render/solid.[ch]    occlusion: the objects' faces, lines behind them cut away
src/render/ground.[ch]   the ground grid
src/render/hiscore.[ch]  the high score table, loaded and saved (and as text for the menu)
src/render/enhance.[ch]  the enhanced options in the picture: stars, far mountains, sparks, dust, flash, volcano, levers
src/render/config.[ch], draw.[ch]   settings, drawing through the SDK
src/platform/controls.[ch]   bindings, the three ways to drive
src/platform/menu.[ch]   the test menu (Tab)
src/platform/app, audio, src/main.c   window, sound (the SDK's stream), the program loop
src/capture/             the format of the MAME captures
assets/freesound/        the Freesound recordings (CC0) package/sounds is mixed from
assets/bzoneii/          sounds of Battle Zone II from the cabinet (the shot; license not known)
tools/                   bzcompare, bznmi, bzshot, capture and check scripts, disassembler, mkfreesound.py, make_vpk.sh, deploy.sh
```

## How the C routines are written

Every routine of the original is a C function with the original's name
(`bz_rotate` is `ROTATE`). The 6502's registers are in the machine
(`m->r`); every instruction is a macro from `src/game/m6502.h`:

```c
LDA(RD(BZ_TANGLE));  SEC();  SBC(RD(BZ_RGOAL));  if (BPL) goto l3;
```

This is on purpose, not a stage to be smoothed out. The original passes
values in A, X, Y and the carry between routines, tests flags a routine
set long before, adds without `CLC` with a carry from somewhere, and
leaves remains in scratch cells that a later frame reads. Only so does
every byte agree with MAME.

Rules:

* Labels are named as the original's local labels (`10$` becomes `l10`).
  Comments in capitals are the original's.
* Every memory access goes through `RD`/`WR`, page 0 too. The comparison
  hooks in there.
* `PHA`/`PLA` are a local variable, `PHP`/`PLP` are `FLAGS`/`RESTORE`.
* Computed jumps (`PHA PHA RTS` through a table) are a `switch`.
* Numbers in the source are hexadecimal (`.RADIX 16`); decimal ones have
  a dot (`22.`). When in doubt, look in the disassembly
  (`tools/dis6502.py`). `16*2` in WNSHLD is 2C.
* Index 0 is the player, index 2 the enemy: `TPOSX+2`, `FIRECT+2`,
  `COLFLG+2`, `HITS+2`. The cells in between belong to other variables
  (`COLFLG+2` is `SAVE1+1`).
* `ATRACT` is FF while a game is played and 0 in the attract mode.

To write a routine more readably: only when the smooth picture needs it,
and then as a second function next to it that reads the state. The
checked routine stays.

## Timing

* Interrupt (NMI): 246.09 a second (12.096 MHz / 8 / 6144).
* Game frame: when the interrupt has set `SYNC` (every 16th) and a display
  buffer is free. The buffer is changed at every 6th interrupt. That gives
  about 15 frames a second.
* `bz_main_poll` after every interrupt: computes a frame at once when both
  hold. The cabinet's 6502 needs 50,000 to 100,000 cycles for it and gets
  slower than 15 a second in full scenes. The C routines take no time (see
  STATUS.md, pace).
* Two display buffers (2002 and 2802). `VECRAM` (2000) is a jump to the
  one shown. The vector generator always starts at 2000.

## Checking after every change to `src/game`

```bash
sh tools/checkall.sh && sh tools/checkall.sh --nmi
```

Both must say `ALL THE SAME`. If not: `bzcompare FILE --from F --to F
--show 40` shows the bytes that differ, with names. What the frame read:
`--trace F`.

For new game situations, make a capture with a script
(`tools/mame/bzcapture.lua`): `coin`, `start`, `fire`, `drive`, `aim`
(turns to the enemy and fires), `auto` (drives at random), `poke ADDRESS
VALUE`, `dip`. Useful addresses: score `B8`/`B9` (BCD, in thousands), tanks
`CC`, missiles so far `02EC` (from 5 on, super tanks come).

## Traps

* **Never change source files through shell heredocs when they hold
  backslashes or apostrophes.** `\n` becomes a real line break there. That
  broke C files here more than once. Change files with the Edit or Write
  tool, or a Python script written with Write.
* Python on Windows knows no paths like `/f/...`. Work relative to the
  project folder or write `F:/...`.
* MAME remembers option switches a script has set. `tools/capture.sh`
  therefore empties its configuration folder before every capture.
* MAME Lua: `machine:exit()` from a tap does not end MAME. The length of a
  capture is set by `-seconds_to_run`.
* The game reads arbitrary addresses with `CMP (0,X)` "to use up time",
  whatever page 0 points at: switches too, the random number, or the NMI
  vector's address. The C routines make these reads as well (`IX(0)`), or
  the sequence of random numbers shifts. So the capture does not recognize
  the interrupt by the vector being read alone.
* The first interrupt after power-on must come only after a period. Before
  that the stack pointer is not set, and the interrupt routine stops (the
  stack check `TSX, BMI`).
* `ROBCHK` is called from the interrupt when a game starts and uses the
  math box and the main loop's scratch cells. That is so in the original.

## What the port adds, and how

All of this runs beside the game. The C routines compute the same as
before; `checkall.sh` must still say "ALL THE SAME" after every change.

**The drawing routines' notes.** Every routine that writes to the display
list sets `m->tag` first (`core/tags.h`: kind and key, say object + tank).
`bz_wr8` puts the tag next to every word of vector RAM in `m->vgtag`.
`bz_avg_run_raw` gives every line, uncut, with its tag, ROM subroutine
(`sub`), place in it (`n`) and running number (`ord`).

**Smooth picture, computed** (`render/smooth.c`, `compute`; the default
since 0.1.3). For every frame the machine before (`a`) and after (`b`) is
at hand. For a picture at time t, `a` is copied, everything that moves is
set on its way to `b` (`between16`, `between_hl`, `between_angle`; jumps
are not flown through) and `bz_picture` draws. On the copy `fine` is set:
`fine_view` and `fine_object` in `game/view.c` write sine and cosine of a
16-bit angle into the math box (BSIN and ACOS are its inputs, not memory).
What has to be interpolated anew when something jumps: look at the
"single lines" line of `bzshot` per kind of object. EXPLDE and the shell
bursts move a step on in the drawing path itself; that is the same for
every picture in between and does no harm.

**Smooth picture, lines moved** (`smooth = 2`, the first way). A frame
draws first and then moves the world, so after the frame it is settled
what the next picture looks like. `bz_picture` draws it on a copy of the
machine with the game's routines (`draw_only`: `main_body` returns after
drawing). Between two frames every line of the world moves from the
picture to the one drawn ahead, matched by tag, `sub` and `n` or `ord`.
Texts, score and sights do not move. Lines that are in only one of the
pictures change at the half. The picture is so never behind the game.

The game's picture positions are rounded down to half a unit, so far
objects stood still for several frames and then jumped; on the cabinet
that looked jerky. `pntput` therefore notes for every point the math box's
unrounded quotients (registers 8 and 9 over 7, register B over 7), and
`bz_avg_run_raw` sets the line ends on them (`bz_avg_exact`, factor 510).
Whoever works on the smoothing checks objects, not only the mountains: the
mountains were always even.

The frames come unevenly (18, 18, 12 interrupts). The schedule starts a
picture a period after the one before; the period is the mean of the last
six gaps. There was a fault here that only measuring showed: a fixed
schedule ran away from the game, and nothing was smoothed at all. So after
changes to `smooth.c` always look at the line "the land on a 60 Hz
screen" of `bzshot` (BUILD.md).

**Line order** (`smooth.c`, `bz_order_steady`, `sm.order = 2`). Texts,
score, tanks left, the radar's ring and the sights first, in the game's
own order; then the radar's sweep and blip; then the world. A line on a
vector tube lands a little differently with every way the beam comes to
it; drawn first, the score is reached the same way in every picture and
does not shimmer.

**Color** (`render/color.c`): a table from tag to color. The kind of an
obstacle (pyramid, cube; four shapes, 0x00, 0x0c, 0x01, 0x0f) is in
PTBLO1 in the ROM. Palettes: `BZ_PALETTE_FLYER` after Atari's flyer (blue
ground and horizon, lilac mountains, the obstacles from cyan to magenta,
an orange moon, yellow and orange for logo, sights and score) and
`BZ_PALETTE_CLASSIC`. The standard tank and the tanks-left display are
green in both, as under the overlay, as the owner wished. `color_mode`
also has the overlay (in the menu ORIGINAL) and white.

**Occlusion** (`render/solid.c`, after the Star Wars port's `solid.c`).
`dovect` notes for every vector the object number, point number and the
point's distance (`m->vgnote`; the distance is what the math box divides
by, register 7). `bz_solid_init` reads the points and drawing lists of
every object from the ROM and finds the faces as flat rings of lines. For
every picture, every face becomes a polygon with the plane of
1/distance; lines of the world are cut where they lie behind it. The
face's own edges stay. Lines that start at a corner of the face are in
front or behind as a whole. Mountains, moon and volcano count as
infinitely far. Texts, radar, sights and the crack in the screen are never
cut. The order in `smooth.c`: the picture between frames from whole lines
first, then occlusion, then the window.

**Ground grid** (`render/ground.c`). The game's projection, measured with
`_scratch/probe.c`: ahead = dx cos a + dy sin a, aside = -dx sin a + dy
cos a (a: TANGLE, LANGLE as 16 bits a turn), screen X = -509.5 * aside /
ahead, screen Y = 509.5 * height / ahead, the ground 320 under the eye.
The grid is made anew for every picture shown from the player's place
(between two frames), not moved. It reaches to 24000 (`FAR`); every
stretch is as bright as its own middle is far (`lit_at`: full to 5000,
then down to nothing at `FAR`, without steps). It does not bob with
BOUNCE.

**Sound replacement** (`sound/samples.c`, after the Star Wars port's
`samples.c`; list in docs/SOUNDS.md). `bz_sndon` reports every tune
started through `m->sound_request`. Shell and explosion start on the
rising edge of their switch in the sound latch. Missile, engines and the
enemy's engine are loops. What is replaced is silenced in the model
(`pokey.mute`, `shell_replaced` and so on). In the test menu the game
stands; `snd.test` then lets only the sound test's voice through.

**Sound output** (`platform/audio.c`, since 0.2.0). Once a picture the
new samples of the ring go to the SDK's stream (`vp_stream_play`,
`vp_stream_write`), about a tenth of a second ahead; more than a quarter
of a second waiting is dropped, oldest first. The SDK mixes the stream at
the cabinet menu's MUSIC VOLUME. The log line "battlezone pace" counts the
times the stream ran dry and the samples dropped.

**Stereo, distance, ducking** (`sound/sound.c`). The ring holds pairs.
`bz_sound_place` reads where the enemy and the saucer are from memory and
whether the enemy tank lives and drives (`COLFLG+2`, `R2D3FL`, his place).
The soft shot and the soft explosion count as the enemy's, POKEY channels
3 and 4 as the missile, the tunes SAUSND and DISINT as the saucer.
`enemy_gain` (his shot, down to 0.3), `enemy_boom` (his explosion, down to
0.55), `enemy_hum` (his engine, down to 0.06). Ducking (`DUCK_TO`): the
engines go to 0.55 while a tune, a shot or an explosion sounds. With the
recordings the POKEY's tunes are `TUNE_BOOST` (3.5) louder.

**Enhanced options in the picture** (`render/enhance.c`).
`bz_enhance_frame` remembers after every frame what happened (the enemy
comes apart: its kind from `m->enemy_id`; the saucer is hit; a shell is
fired or comes down) and makes sparks and dust. `bz_enhance_scene` takes
the mountains' skyline (per column) and the moon from the picture's lines.
`bz_enhance_world` adds the second mountain row (only over the skyline),
the stars (behind the skyline and the moon, twinkling), sparks and dust as
whole lines with a distance, after the game's lines and before occlusion.
`bz_enhance_overlay` draws the flash and the lever gauges after the
window cut. `bz_enhance_color` colors the wreck and the sparks in the
color of what was hit (`l->model` holds its id), the far row and the
volcano. Its own lines carry `BZ_TAG_FX`. The game's mountains and moon
move 0.996 screen units every 32 angle steps (`MOUNT_RATE`, 2040 units a
turn); the stars turn with them (they are as far as the moon), the far
row half as fast (`PARALLAX`; periodic over its circle, so a full turn is
seamless).

**The beam's way.** The board needs as much time for the dark way between
lines as for lit lines. Over about 170,000 units (lit plus blank in the
log) it does not make 50 pictures a second, and pictures come late. The
grid at 24000 brought the cabinet to about 85,000 lit and up to 169,000 in
all (in the attract mode). `bzshot` prints the way ("the beam's way").
Measure every new effect with it.

**The board's picture rate.** `board_hz` (default 50) must match the
board's rate (DVG REFRESH in the cabinet's menu), or the motion stutters
because the board shows its last picture again. On a report of "steps",
check that first.

**The cabinet's pace** (`game/pace.c`). A frame's cycles are estimated
from counted reads and writes (fitted to the original on the 6502 core,
790 cycles off at 56,000 on average). The interrupt takes 690 of 6144
cycles from the main program. What matters is not that frames take longer
than 16 interrupts (under 1 percent) but when the display buffer is
reported done: the interrupt hands it over only at every sixth. So a
frame's work is done only when the 6502 would be done with it
(`bz_main_ready`, then wait). The result: 14.62 frames a second in the
attract mode against 14.69 in MAME, 14.77 in a game against 14.8 to 15.0.

**High scores** (`render/hiscore.c`): HSCTBL and INITLS, 60 bytes,
loaded after `bz_pwron`, saved as soon as the table changes and no
initials are being entered. The same table as `save/highscores.txt`
(`bz_hiscore_save_text`: initials from the vector ROM's alphabet, 16..48
A..Z, 02..14 digits, 00 and 4A a space; points BCD in thousands) for the
cabinet's menu and marquee.

**The credit.** Under the attract mode's "(C)(P) ATARI 1980" the port
writes "ENHANCED BY ENDPRODUKT" in the same color, a little smaller
(`main.c`, after `bz_draw_lines`, found by the text's tag 0x2e).

**Analog driving** (`analog_motion` in `game/play.c`, the only addition
in the game itself, active only with `m->analog.on`). The cabinet's eight
motions are the corners of: steps = l + r, turn = (r - l) / 2, with l and
r from -1 to 1. In between it is computed without steps; collision and
engine sound go through the same code as before.

**Controls** (`platform/controls.c`, `platform/vpbind.[ch]`): bindings
read directly through SDL (keyboard, joysticks, mouse buttons), several per
action (" or ", `parse_binds`). AUTO is `AUTO_SPEC` in controls.c (arrow
keys or the first stick, E/D/I/K, left Ctrl + the first joystick button +
the left mouse button, 1, 5); the yoke's AUTO is the joystick with "yoke"
in its name (`vpb_yoke`), else joystick 1. Laying out in the menu is as in
the cabinet's other vector games (vpbind.h; the master copy is the Tempest
2000 port's): START/Enter, then a control; the first replaces, further ones
add; START 2 NONE, Delete AUTO. The menu goes by its own controls only
(`bz_nav` through vpbind). Since 0.1.13 there is no mode: all three ways to
drive count at once, the levers first, then the yoke, then the one
joystick; `bz_controls.mode` is the way used last (for the initials'
line). Only Tab (or a binding, or start and fire held) opens the menu.

## Next steps

1. **Play and listen on the cabinet.** Window and menu were checked only
   by screenshots and key scripts (`_scratch/winshot.ps1`).
2. **The sound pack.** Since 0.1.9 from Freesound recordings and Battle
   Zone II (the shot; `tools/mkfreesound.py assets/freesound --bzoneii
   assets/bzoneii`, layers and levels in the script), on by default.
   Battle Zone II's files: license not known. More recordings: CC0 only,
   through the owner's Chrome, asking for each download; one download at a
   time (several in one batch are refused by the check).
3. **Colors**: everything is in `render/color.c`; tune after the owner's
   word.
4. **Smoothing**, if needed: when turning, the line ends are moved
   straight, not projected anew. At 15 frames a second the error is small;
   it could show with very near objects.
5. **Releases**: `tools/make_vpk.sh`, then a GitHub release and the Drive
   folder "games" (BUILD.md).
