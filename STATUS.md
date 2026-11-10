# Status

State: 2026-11-10, version 0.2.0 (VectorPie SDK 0.9; the sound through
the SDK's stream, so the cabinet menu's volume applies; all documentation
in English).

## Goal

Battlezone (Atari, 1980) complete and unchanged in C, checked against
MAME. Then: a very smooth picture, better sound, color vectors.

All of it is there: the whole game program is in C and agrees with MAME
in every capture. A smooth picture, color, solid objects, a ground grid,
kept high scores, three ways to drive at once with free bindings, the test
menu and a new sound pack have been added; the owner plays it on the
cabinet and has tuned picture and sound step by step (see the history
below).

## What runs

The game as a C program (`src/game`, 5,000 lines), routine by routine
after the source code of 1980/81:

* main loop, window, mountains, moon and the volcano with its rocks
* choosing the objects in view, projection through the math box, drawing
  every object (tank, super tank, missile, saucer, obstacles, shells,
  wreck, explosions, logo)
* player: both levers, fire, running into obstacles, bouncing back
* enemies: tanks with goals, evading, attacking, backing off after a
  collision; super tank; the missile's weaving approach; the saucer
* radar, sights, messages in four languages, score, bonus tank, the super
  bonus at 100,000
* the cracked screen, game over, the high score page, entering initials
* interrupt: coins with every coin mode and bonus coins, credits, start,
  the sound sequence (POKEY tunes, sound latch), changing the display
  buffers
* the power-on state (PWRON, as far as it leads into the game)

In normal use no 6502 runs.

Rebuilt as parts: the math box, the vector generator with the Battlezone
board's window circuit, the POKEY, the discrete sounds.

`battlezone --original` runs the original program on a 6502 core of its
own, to compare.

## What the port adds

The game's mechanics are unchanged. Every comparison against MAME is still
completely the same after these additions.

* **Smooth picture.** Pictures between the game's 15 frames a second,
  computed from the game's state as in the Star Wars port (`smooth = 1`),
  or the lines of a frame moved to the next (`smooth = 2`). 60 pictures a
  second in a window, 50 on the board (`board_hz`).
* **Color.** The flyer's colors (the default): a blue ground and horizon,
  lilac mountains, the four obstacle shapes from cyan to magenta, an
  orange moon, a red super tank, a yellow missile, a magenta saucer,
  sights, score and logo yellow and orange, warnings red, the high score
  table from yellow to red. Also the first palette, the cabinet's overlay
  (ORIGINAL: all green, the top red) and white. The standard tank and the
  tanks left are green in the palettes, as on the cabinet. Pictures in
  `docs/images`.
* **Solid objects.** Back faces and whatever is behind an object are not
  drawn (`occlusion`).
* **Ground grid** in the game's perspective, hidden by objects, to 24000
  units, coming out of the dark without steps (`ground_grid`).
* **Sounds.** Every one of the 15 can be replaced by a WAV file; a pack
  comes with it (a shot, explosions, tank engines, the enemy's engine, the
  obstacle sounds), on by default (`sound_replace`, in the menu SOUNDS
  ENHANCED or ORIGINAL). docs/SOUNDS.md.
* **High scores** in `save/hiscore.bin`, all ten rows, and as
  `save/highscores.txt` for the cabinet's menu and marquee.
* **Controls.** One joystick, two joysticks and a yoke at once; every
  action bound freely, to several controls.
* **The test menu on Tab**: controls, picture, sound with a sound test,
  game options, resetting the high scores, the calibration picture.
* **Enhanced options**, each switchable (table in the README): sparks,
  stars, the far mountain row with parallax, the glowing volcano, the
  muzzle flash and dust, the lever gauges, stereo, quieter far away, the
  enemy's engine, ducking, the engine following the levers, the cabinet's
  pace, the menu through start and fire or a button of its own, the START
  lamp.
* **The credit** "ENHANCED BY ENDPRODUKT" under Atari's copyright line in
  the attract mode.

## Checked

Against MAME 0.289, set `bzonea`, 16 captures (`tools/captures.sh`):
attract mode, games played with hits and deaths, the missile (also from
behind and out of range), super tank, saucer, bonus tank at 15,000 and
50,000, the super bonus, game over with entering initials, German, French,
Spanish, free play, 2 games a coin with bonus coins, 2 coins a game.

| Check | Extent | Result |
|---|---|---|
| C routines, frame by frame (`bzcompare`) | 70,120 frames | all the same |
| the original program on the 6502 core (`bzcompare --emu`) | 70,120 frames | all the same |
| the interrupt routine in C against the original on the core (`bznmi`) | 1,124,000 interrupts, with coins, slam, game starts | all the same |
| the power-on state in C against the original (`bznmi`) | RAM, vector RAM, POKEY registers | the same |
| `test_core` (no ROMs) | 6502 core, math box, vector generator, macros, high score file | passed |
| the picture drawn ahead against the game's next one (`bzshot`) | 448 frames turning and driving | 0.23 units apart per line on average, 0.14 % without a partner |
| evenness while turning on 60 Hz (`bzshot`) | 1,325 pictures | smoothed 1.6 % standing still and 1.2 % jumps; unsmoothed 74.5 % and 25.5 % |
| the analog mode at its ends against the lever switches (`tools/checkanalog.sh`) | all eight motions | the same place and direction |
| occlusion and the ground grid in pictures (`bzshot`, `docs/images`) | pyramids, boxes, tanks near and far, with `--nosolid` to compare | back faces, mountains and grid behind objects are gone, front edges stay |
| sound replacement with test tones (`bzshot --sounds`) | 60 s of play, 8 sounds replaced | radar beep 38 times, shot 5, explosions 3, warning 4, bump 3 |
| the sound model against MAME (`tools/soundcheck.sh`) | 75 s, every sound | within about 1 dB, the shot's volume bit deliberately different (docs/SOUNDS.md) |
| the cabinet's pace (`bzshot --pace`) against MAME | 150 s attract, 150 s play | 14.62 against 14.69, and 14.77 against 14.8 to 15.0 frames a second |
| stereo (`bzshot --wav`) | 60 s of play | two channels, left and right differ in 13 % of the samples |
| stars and moon turning together (`bzshot --every`) | consecutive pictures while turning | both 1.99 units a picture |
| the SDK's stream (`vp_stream_play`, a test tone) | 0.6 s | the tone arrives in the mix at its level (`vp_audio_recent`) |
| window and test menu by key script and screenshot | pages, binding a key, NONE, high scores reset and loaded | works |

After every frame every byte of RAM and vector RAM and every register of
the math box is compared. Every random number and every read of the
switches must also come at the same place.

Coverage of the C routines by the captures (`tools/coverage.sh`):
`play.c` 99.8 %, `view.c` 99.7 %, `mainline.c` 98.3 %, `nmi.c` 100 %,
`sound.c` 100 %, `coin.c` 98.7 %, `vgut.c` 90.5 %. Not run through are only
`bz_main_poll` (the wait loop of live play), the unused drawing command
`D.SLIM` and three routines of VGUT that Battlezone never calls (`VGRTSL`,
`VGJMPL`, `VGSTA1`).

## Not yet seen or heard

* **0.2.0 on the cabinet.** The SDK update and the sound through the
  SDK's stream are checked on a PC only (the program starts, the stream
  plays, nothing runs dry). Whether the menu's MUSIC VOLUME sets the game
  on the cabinet is not heard yet.
* **The controls of 0.1.13 and later on the cabinet** with levers and yoke
  at once, the menu button, start and fire held, the START lamp.
* **The calibration picture**: it shows; changing size, place and
  brightness is not checked.
* **Several rows of 0.1.16 and later in a full fight**: the grid at 24000
  brought the board close to its limit (see "The beam's way" in
  RESUME.md); measured in the attract mode only.

## Known differences from the cabinet

* **Pace.** The cabinet computes 14.7 to 15.1 frames a second and slows
  down in full scenes, because its 6502 needs up to 102,000 cycles for a
  frame (16 interrupts are 98,304). The C routines need no time: they run
  at 15.3 frames a second, 1 to 4 % faster and without wavering, unless
  `enh_tempo` (the cabinet's pace) is on.
* **Revision.** The behavior is always revision 1 (`bzonea`), also with
  the ROM set `bzone`. Revision 2 changes the high score page.
* **Random numbers.** In live play they come from a shift register of
  their own, not from a cycle-exact POKEY. The sequence differs from MAME,
  the behavior is the same.
* **Math box.** Rebuilt at the level of routines as in MAME, not as
  microcode. It is done at once. The microcode source (MBUCOD.V05) and the
  PROMs are at hand; a microcode-level rebuild would be possible.
* **Vector generator.** The time it needs for a picture (the halt flag) is
  only approximated. The game itself does not ask for it.

## Not ported

* The board's self test (BZSTST.MAC from `RAMTST`: RAM, ROM and math box
  tests, switch test, diagnostic pictures) and the diagnostic program
  BZSADG.MAC. They test hardware that is not here. With `--original` they
  run on the 6502 core (test switch: IN0 bit 4, not yet on a key).
* The guards against crashes (stack check and sync check in the
  interrupt, watchdog) have nothing to guard here and are left out.
* The copy protection check (`TRAP1`, `TRAP2`: a checksum over the
  copyright text in the display list) is ported and computes as in the
  original.

## Known limits of the additions

* **Occlusion.** Faces come only from flat rings of up to eight lines. The
  logo has no such faces and hides nothing. When an object reaches out of
  the picture and a point of a face is missing, that face hides nothing.
* **Ground grid.** It does not bob when the tank bumps. It costs the board
  lit length (at 24000: about 85,000 of the cabinet's units lit a
  picture).
* **Sound replacement.** The recordings' level against the board is a
  guess. With `--original` the eight tunes are not replaced. The game's
  sound counts as music for the cabinet menu's volumes.
* **Pictures between frames.** With `smooth = 2` lines are moved straight
  between two pictures, not projected anew; very near objects while
  turning fast could look slightly bent. With `--original` there are none:
  it shows the original's 15 frames, with the overlay instead of color.
* **Analog with `--original`.** The original program knows only
  switches; there the yoke counts as a pushed lever past half way.
* **The cabinet's pace.** A model, not the original running along. It
  meets MAME's frame rate to about 1 percent.
* **START lamp** on the cabinet only, because on a PC it would switch the
  keyboard's NUM lock.
* **Esc ends the program**, in the menu too. Only while laying out a
  control does it not.
* The missile's flame flickers (the game changes its picture every frame)
  and a shell burst grows in whole steps, also between frames.

## Open points

1. 0.2.0 on the cabinet: does the menu's MUSIC VOLUME set the game's
   sound; does the stream play without clicks (the log line counts the
   times it ran dry).
2. Try the controls of 0.1.13 and later on the cabinet with levers and
   yoke.
3. Watch the board's cost in a full fight with the grid at 24000; if
   pictures come late, set `FAR` in `render/ground.c` back to about 20000.
4. Colors and sounds further after the owner's word.
5. Tidy up: `_scratch/shots/a` and `_scratch/shots/p` hold old test
   pictures (deleting was refused by the safety check).

## Next steps

See [RESUME.md](RESUME.md), "Next steps".

## Releases

* GitHub: https://github.com/Endprodukt/BattlezoneEnhanced/releases
  (`v0.1.21`, 2026-10-19; `v0.2.0`). With every new package also a
  release (`gh release create v<version> build/bzenhanced-<version>.vpk`).
* The shared Google Drive folder "games": 0.1.20 and 0.1.21.

## History

Newest first.

### 0.2.0 (2026-11-10): the new SDK, the menu's volume, English

The VectorPie SDK brought from 0.1 to 0.9.0 (41 commits; it builds
without a change). Its new audio options: the cabinet menu's MUSIC VOLUME
and SOUND VOLUME apply to every game that plays through `vp_audio`. The
port had an SDL audio device of its own, which they did not reach. Now
the sound goes out as the SDK's stream of a game's own samples
(`vp_stream_play`, `platform/audio.c`, as Tempest Enhanced does it): once
a picture the new samples, about a tenth of a second ahead. The SDK counts
such a stream as music, so the menu's MUSIC VOLUME sets it, on top of the
game's own `volume`. The log line counts the times the stream ran dry.
Checked on a PC: it builds, starts, the stream plays without running dry,
a test tone arrives in the mix at its level. All documentation in English
(README, BUILD, STATUS, RESUME, docs/SOUNDS.md; the pictures in
`docs/images` with English names).

### 0.1.21 (2026-10-19): binding as in every vector game

* Menu (`src/platform/menu.c`): START/Enter on a row, then a control; the
  first since the cursor came to the row replaces the binding, every
  further one adds (" or "). While waiting: Esc/Enter (for game functions
  also START) back without a change, START 2 NONE (game functions only),
  Delete (also the keypad's) AUTO. Left/right AUTO/NONE (the menu's row
  never NONE). Shown with real names ("LEFT CTRL + JOY1 BUTTON 1"); one key
  may have several actions.
* The menu goes only by fixed controls (`bz_nav` through
  `src/platform/vpbind.[ch]`, a copy from the Tempest 2000 port): arrows,
  every stick, hat and yoke, START, Enter, keypad Enter. Bindings and the
  ways to drive do not act in the menu.
* AUTO read directly (`AUTO_SPEC` in controls.c: arrows or the first
  stick, E/D/I/K for the levers, left Ctrl + stick button 1 + the left
  mouse button for fire, 1, 5); the yoke's AUTO: the device with "yoke" in
  its name, else joystick 1, directly with the dead zone. New: mouse
  buttons as a binding. Never laid out: Tab, F2, Esc, Enter, keypad Enter,
  Delete; for game functions also START and COIN.
* Checked on a PC (replace, add, NONE); installed on the cabinet (the old
  file `battlezone.before-vpbind`), not tried there.
* GitHub release `v0.1.21` with the same package as in the Drive folder.

### Package 0.1.20 in the Drive (2026-10-17)

`tools/make_vpk.sh` (after Red Baron Enhanced's) makes
`build/bzenhanced-0.1.20.vpk` (4.5 MB: the Pi program, game.ini,
battlezone.ini, marquee, nine sounds, `sounds/CREDITS.txt`; no ROMs; the
program and the folders 755, the rest 644). Uploaded to the shared Google
Drive folder "games" (next to rbenhanced, swenhanced). As the owner
wished, with Battle Zone II's shot, though its license is not known and
the folder is public (noted in CREDITS.txt).

### 0.1.20 (2026-10-17): colors only under PICTURE, the credit

COLORS is no longer also on GAME OPTIONS, only under PICTURE (SOUNDS stays
on both). Under the attract mode's "(C)(P) ATARI 1980" it says "ENHANCED
BY ENDPRODUKT", in the same color, a little smaller (`main.c`, after
`bz_draw_lines`). Picture: `docs/images/credits-endprodukt.png`.

### 0.1.19 (2026-10-16): no haze, a grid without steps, stars with the moon

The distance haze (`enh_haze`) is gone altogether, as the owner wished;
the setting is read over.

The grid "lives in front of my nose": it was dimmed in eight fixed bands
whose borders lie at fixed distances from the tank. A line across the
view, coming nearer while driving, got a step brighter at every border.
Now every stretch has the brightness of its own middle (`lit_at` in
`render/ground.c`), without steps. (Checked and ruled out: the cut at 700
units lies below the picture's bottom edge.)

Stars: they turned with a true view (0.049 units an angle step), the
game's moon and mountains with 0.031; the stars passed the moon 57 %
faster. Everything infinitely far must turn together: the stars now turn
with the moon (`MOUNT_RATE`), measured 1.99 units a picture for both. 100
stars instead of 150, as many to see. The far mountain row stays half as
fast on purpose (parallax, 0.1.16).

### 0.1.18 (2026-10-16): a binding may be NONE

Left/right on a binding's row: bindings of your own back to AUTO, AUTO to
NONE, NONE to AUTO again. A new key replaces NONE. Checked on a PC.

### 0.1.17 (2026-10-16): up to eight bindings

Reported: "left Alt is not taken as a key." Fire had four bindings on the
cabinet already (auto, left Shift, Space, left Ctrl), the limit was four,
and a new one replaced the last. The cabinet's advmame.rc puts Battlezone's
fire on four keys (Shift, Space, Ctrl, Alt). Now up to eight
(`BZ_BIND_ALTS`, `BZ_BIND_SIZE` 320).

### 0.1.16 (2026-10-16): the grid further, the far row with parallax

The ground grid to 24000 instead of 15000 units (`FAR` in
`render/ground.c`); 30000 cost half again the lit length and looked hardly
different. On the cabinet (attract mode): about 85,000 lit units a picture
instead of about 70,000, with the dark way up to about 169,000 at the
peak, 50.0 pictures a second, no picture stood still. The old limit: it
stuttered around 170,000; watch it in a full fight, else `FAR` back to
about 20000.

Parallax: the far row moved 0.029 screen units an angle step, all but as
fast as the game's own row (0.031: 0.996 every 32 steps), so no parallax
was seen. Now half as fast as the first row (`PARALLAX` in
`render/enhance.c`), with fewer corners (52 instead of 96) so the skyline
stays as wide.

### A new marquee (2026-10-16)

From the owner: the "Battlezone Enhanced" marquee (the flyer's picture
with ENHANCED), 2000 x 491, as `package/marquee.png` (before 1920 x 360).
The SDK fits it (`vector_aux_marquee_display_mode` in advmame.rc).

### 0.1.15 (2026-10-16): a new sound against obstacles

Running into obstacles still sounded original: two POKEY tunes, BOING at
the bump and BLKSND while the tank is stuck (the interrupt starts it again
when the board's tune ends after 34 interrupts, so the replacement runs at
the board's pace, and BOING is cut after 0.47 s while the player pushes
on). Now BOING is a heavy metal crash and BLKSND 0.138 s of grinding with
a small clank that, repeated, rattles like slipping tracks
(`tools/mkfreesound.py`, `obstacles`).

### 0.1.14 (2026-10-16): high scores in the menu and on the marquee

The marquee showed high scores for Vectortron, not for this game: `hiscores
= no` in `game.ini`, and the table was only in `save/hiscore.bin`. The
cabinet's menu reads `save/highscores.txt` (`NAME POINTS`, up to ten rows,
vp_hiscore.h). Now the game writes it when it starts, at every new entry
and when the table is reset (`bz_hiscore_save_text`), and `game.ini` says
`hiscores = yes`. Checked on a PC (the power-on table EDR, MPH, JED ...
5000 each).

### 0.1.13 (2026-10-16): every way to drive at once, several bindings

The owner wanted no choice between one joystick, two joysticks and a yoke,
but all at once, to plug in whatever is at hand, and fire on several
buttons as in MAME. The controls page shows every binding; in every
picture the levers of the two joysticks come first, then the yoke out of
its dead zone, then the one joystick (`platform/controls.h`). The setting
`controls` is read over. The arrow keys are no longer the yoke's ends.
Checked on a PC; the MAME comparison and the analog check the same. Mind a
very small dead zone (the cabinet has `deadzone = 0.02`): a yoke trembling
at rest past it counts as used while no lever is held.

### 0.1.12 (2026-10-16): louder beeps, the original selectable

The original beeps were lost: with ENHANCED the POKEY's tunes are now
11 dB louder. The owner wanted the original sound and the original color
(all green, the top red) in the options: both were there
(`sound_replace`, the color OVERLAY), now named plainly and also on GAME
OPTIONS: SOUNDS ENHANCED/ORIGINAL, COLORS FLYER/CLASSIC/ORIGINAL/WHITE.
ORIGINAL for the sound also means no ducking and no louder tunes.

### 0.1.11 (2026-10-16): quieter engines, ducking

"The sound is good, but could be a bit quieter. The radar sounds are lost
now." The engine loops are 3 dB quieter, and `enh_ducking` (on) is new:
the engines 5 dB softer while an effect is heard.

### 0.1.10 (2026-10-16): a tank engine with tracks, the enemies' engines

The engine (from Battle Zone II) did not sound like a tank; a real tank
with tracks was wanted, and the enemy tanks should make noise too. Now
three loops from Freesound recordings of real tanks (CC0): standing,
driving with tracks, the enemy. New `enh_enemy_engine` (on): the enemy
tank's engine from his side, clearly quieter far away, revved while he
drives.

### 0.1.9 (2026-10-16): more color, new sounds

Less blue, more color in the objects, in the flyer's scheme (blue,
purple, futuristic), the ground stays blue: the four obstacle shapes each
a shade of their own (pyramid 0x00 cyan, 0x0c blue, cube 0x0f violet,
0x01 magenta), both mountain rows lilac instead of white-blue, the saucer
a stronger magenta, the player's shot cyan-white. Picture:
`docs/images/color-more-purple.png`.

New sounds: the enemy's explosion and the player's death from Freesound
recordings (CC0), layered. The shot and (until 0.1.10) the engine from
Battle Zone II as it is on the cabinet, as the owner wished; the enemy's
shot is the same, duller and softer. `sound_replace` on by default. The
old computed pack (engine and missile too) is no longer part of it.

The hit enemy's explosion is quieter far away, but less than his other
sounds: 1.1 - distance / 60000, at least 0.55 (`enemy_boom` in
`sound.c`; before the same curve as his shot, down to 0.3). Only with
`enh_distance`.

### 0.1.8: texts and gauges first

Reported with 0.1.7: the top right (score, tanks left) and GAME OVER still
wobble. In the game's order these texts come after the world; the beam
reached them from the end of the last moving line, from somewhere else
every picture, and on the tube they landed slightly differently. Now
texts, score, tanks left, the radar's ring and the sights are drawn first,
then the radar's sweep and blip, then the world, each part in the game's
order (`bz_order_steady`, `sm.order = 2`).

### 0.1.7: a fixed, shorter beam way for the additions

The game's lines keep their order (the sorting stays off, see 0.1.6).
Only the additions are made so the beam has short ways and the order is
the same every picture: grid lines from near to far and every other one
back, stars from left to right and last, the grid before the other
additions. In `bzshot` the dark way fell from 10,800 to 6,300 screen units
a picture. The frame rates of 27 to 37 measured meanwhile did not come
from the port: `deploy.sh` had started it while another game held the
board, and the owner had changed the board's slew and jump rates.
`deploy.sh` no longer starts the game when another program has the board;
it only installs. Started cleanly on the cabinet: 50 pictures a second,
0.8 to 1.0 ms of board time a picture, a dark way of 54,000 to 62,000
units (0.1.4: 89,000 to 100,000), no picture waits. The log counts the
sound's trouble and the loudest value.

### 0.1.6: sorting off again

With 0.1.5 the picture wobbled on the cabinet; before, the vectors were
steady. Sorting by the nearest stroke (`bz_order_lines`) changes the order
from picture to picture as soon as something moves, and the beam comes to
a line differently every time. `sm.order` was set back to 0; the function
stays in the code.

### 0.1.5: a shorter beam way

The cabinet's log showed pictures for which the board needed up to 24 ms,
more than the 20 ms of a picture at 50 Hz: such pictures come late, a real
stutter. The cause was the beam's dark way between lines (90,000 to
120,000 units, more than the lit lines). `bz_order_lines`
(render/smooth.c) orders the lines so the beam jumps to the nearest
stroke, strokes backwards too, stars last. Measured on the cabinet: a dark
way of 26,000 to 36,000 units, at most 1.3 ms a picture.

### 0.1.4: the sound matched to MAME

The sound model compared with MAME: the same writes to the sound hardware
in both, sound by sound (`tools/soundcheck.sh`, results in
docs/SOUNDS.md). Engine, explosions, POKEY tunes and the missile's buzz
are within about 1 dB of MAME, the spectrum too. Fixed: the POKEY's output
was computed linearly instead of through the chip's resistor network; the
tunes sounded dull and the missile's buzz was 7 dB too quiet. Deliberately
not as MAME: the shot's volume by bit D3 (the original program's
`LOUDSH`). The whole model is 6 dB louder than before.

### 0.1.3: pictures between frames as in Star Wars

As the owner wished: instead of moving a frame's finished lines to the
next, the game's state between two frames is set (player, enemy, shells,
saucer, wreck, volcano rocks, radar sweep, logo), and the game's drawing
routines draw that picture on a copy (`compute` in `render/smooth.c`).
Needed because the game turns the view with a 9-bit angle and the objects
with 8: on the copy ROTATE and PNTPUT take sine and cosine from all 16
bits (`bz_machine.fine`), the mountains get the rest of their way
(`mount_shift`), and the 8-bit angles of antenna, saucer and wreck carry
their fraction.

Measured (`bzshot --hz 50`, "single lines": how much a line's step
changes from picture to picture, in percent of a step):

| | driving and turning | play with hits |
|---|---|---|
| computed (`smooth = 1`) | 5.2 | 17.0 |
| lines moved (`smooth = 2`) | 3.3 | 15.1 |
| no pictures between | 198.7 | |

Both ways are even; the computed one has the right perspective and lets
objects come into the picture where they are. The first versions were
not: the fine angles landed in memory instead of the math box, and wreck,
antenna, the missile's height and shell bursts jumped once a frame. Only
measuring per kind of object showed it.

### 0.1.2: the cabinet's second feedback

* **Stepped motion.** The board draws its last picture again and again at
  its own rate (the cabinet's menu: DVG REFRESH, 30 to 60 Hz, default 40).
  The port sent 45 pictures a second; when that does not match the board's
  rate, pictures are shown twice and skipped in turn, which looks like
  stuttering. Star Wars sends 50 and runs smoothly. Now `board_hz`, default
  50; it must equal DVG REFRESH.
* **The grid built in front of the nose.** Up to 5000 units ahead it is
  fully bright without steps; only beyond that it comes out of the dark,
  up at the horizon (see 0.1.19 for the steps).
* **Removed with their options:** the finer grid, track marks, radar dots,
  the arrow. Old keys in `battlezone.ini` are read over without a word.
* **Stars** higher, behind the moon and the skyline, twinkling.
* **A second mountain row** with parallax, only over the first row's
  skyline. One volcano, in front.
* **Explosion:** the wreck in the color of the destroyed vehicle, with
  sparks in the same color (`enh_particles`); the saucer sparks violet.
* **Entering initials with the yoke** works (the lever switches come
  along past half way in the analog mode). The instructions' line is
  replaced per way of driving (English, color modes).

### 0.1.1: reported on the cabinet and fixed

* **The game picture narrower than the calibration picture.** The part
  shown was MAME's (580 by 400 units, with a black border around the
  game). Now it is the game's own window, 512 by 384, exactly 4:3.
* **Jerky motion, objects stopping briefly and moving on.** The cause was
  not the schedule but the game: it rounds every object point's picture
  position to a grid of half a unit, so a far object stands still for
  several frames and then jumps. Now the points are taken unrounded, from
  what the math box divides (`bz_avg_exact`), and every picture starts
  exactly a period after the one before.
* **The ground builds up in front of you.** The grid had five brightness
  steps, and the finer lines appeared suddenly half way. Now all lines
  come out of the dark and get brighter towards the player.
