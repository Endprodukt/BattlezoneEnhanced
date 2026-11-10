# Sounds

All 15 sounds the game makes. Each can be replaced on its own by a
recording: a WAV file named as in the tables, in `sounds/` next to the
program or in `save/sounds/`. Sounds without a file sound as they always
did.

* Format: WAV, PCM, 8 or 16 bit, mono or stereo, any rate.
* On and off: `sound_replace` in `battlezone.ini`, or in the test menu
  (Tab) under SOUND (or GAME OPTIONS): SOUNDS ENHANCED or ORIGINAL. SOUND
  TEST there plays every recording on its own. ORIGINAL is the board's
  sound with nothing added: no recordings, no ducking, no louder tunes
  (stereo and distance have switches of their own).
* The volume: the game's `volume`, on top of the cabinet menu's MUSIC
  VOLUME. The sound goes out as the SDK's stream of a game's own samples
  (`vp_stream_play`, SDK 0.9; `src/platform/audio.c`), which the SDK
  counts as music.
* Code: `src/sound/samples.[ch]`, hooked into `src/sound/sound.c`.

## The POKEY's tunes and signals (8)

The game starts them through `SNDON`. Only one plays at a time: a new one
ends the one playing. The length is the original's; a recording may be
longer or shorter.

| File | Name in the source | What | When | Length on the board |
|---|---|---|---|---|
| `RBEEP.wav` | RBEEP | radar beep | each time the radar's sweep passes the enemy | 0.07 s |
| `BOING.wav` | BOING | bump | the player's tank runs into an obstacle | 0.47 s |
| `BLKSND.wav` | BLKSND | "motion blocked" | while the tank is stuck at an obstacle, started again and again | 0.13 s |
| `BONER.wav` | BONER | bell | a bonus tank | 0.91 s |
| `WARNG.wav` | WARNG | warning | "enemy in range": an enemy appears | 0.59 s |
| `DISINT.wav` | DISINT | disintegration | the saucer is hit, started again and again while it comes apart; also the slam alarm | 0.10 s |
| `SAUSND.wav` | SAUSND | saucer | while the saucer is in the picture, started again and again | 0.26 s |
| `SUPBON.wav` | SUPBON | fanfare (the 1812 Overture) | a new high score, and at 100,000 points | 2.54 s |

The game starts the three sounds that come "again and again" from the
beginning each time the previous one has run out, by the board's length,
not the recording's (the interrupt starts the next when the board's own
tune ends). A recording for them should be about as long as the original
or repeat without a seam. The same holds for BOING: when the player keeps
pushing, BLKSND cuts it after 0.47 s.

## Missile (1)

| File | What | When |
|---|---|---|
| `MISSILE.wav` | the missile's buzz (POKEY channels 3 and 4) | while the missile comes in. The file loops, as loud as the original is at the moment |

## Shells and explosions (4)

On the board, noise from discrete circuits. The recording starts the
moment the board switches the sound on.

| File | What | When |
|---|---|---|
| `SHELL_LOUD.wav` | shot, loud | the player fires |
| `SHELL_SOFT.wav` | shot, soft | the enemy fires |
| `EXPLO_LOUD.wav` | explosion, loud | the player's tank is hit (the screen cracks), the explosion with the fanfare |
| `EXPLO_SOFT.wav` | explosion, soft | the enemy is hit, a shell hits an obstacle |

## Engine (2)

| File | What | When |
|---|---|---|
| `ENGINE_IDLE.wav` | the engine idling | loops while the game runs and the tank stands |
| `ENGINE_REV.wav` | the engine under load | loops while the tank drives or turns |

With only `ENGINE_IDLE.wav`, it plays faster while driving (up to 1.6
times). With both, they are faded into each other. The change follows
the original circuit: slowly up, quickly down again. Without
`ENGINE_IDLE.wav` the original engine stays.

## The enemy tank's engine (an addition)

| File | What | When |
|---|---|---|
| `ENEMY_ENGINE.wav` | the enemy tank's engine (not a sound of the board) | loops while an enemy tank lives |

`enh_enemy_engine` (test menu SOUND, ENEMY ENGINES; with the enhanced
sounds only): while an enemy tank lives (`COLFLG+2` is 0; not the missile,
bit 7 of `R2D3FL`), `ENEMY_ENGINE.wav` loops from his side. It is clearly
quieter far away, more so than his shot: 1.15 - distance / 22000, at least
6 percent. When his place changes he is driving: the engine revs up in
half a second (played faster); when he stands, it goes down again. Fading
in and out takes a quarter of a second. Without the file the game takes
the player's engine loops, lower. Code: `bz_sound_place` and the mixer in
`sound/sound.c`.

## The pack that comes with it

`package/sounds/` (`sounds/` next to the program in a build) holds nine
sounds: the shots from Battle Zone II (as it is on the owner's cabinet,
`/persistent/games/bzoneii/sounds`), and explosions, tank engines and the
obstacle sounds from Freesound recordings, CC0 (free, no attribution
needed).

| File | What | Length | Made from |
|---|---|---|---|
| `SHELL_LOUD.wav` | the player's shot | 2.6 s | Battle Zone II `player_fire.wav` |
| `SHELL_SOFT.wav` | the enemy's shot: the same, duller and softer | 2.6 s | as above |
| `EXPLO_SOFT.wav` | the enemy destroyed | 2.6 s | 394128, 490266, 413224, 814052 |
| `EXPLO_LOUD.wav` | the player's tank destroyed | 3.6 s | 873747, 394128, 490266, 814052 |
| `ENGINE_IDLE.wav` | the engine, standing (loop) | 8 s | 182793, 186934 |
| `ENGINE_REV.wav` | engine and tracks, driving (loop) | 8 s | 329800, 186934, 849062, 121531 |
| `ENEMY_ENGINE.wav` | the enemy tanks' engine (loop) | 8 s | 572294, 187676, 121531 |
| `BOING.wav` | the bump into an obstacle | 0.8 s | 151624, 67617, 837389 |
| `BLKSND.wav` | stuck at an obstacle: grinding, started by the game every 0.138 s | 0.14 s | 336001, 697758, 67617 |

* **Shots.** Battle Zone II's `player_fire.wav`, cut to 2.6 s and lightly
  saturated (it peaks high but is soft on average); the enemy's is the
  same, duller and softer. Its license is not known. The copies are in
  `assets/bzoneii/`.
* **Explosions.** Each layered from several recordings: a bright attack
  that cuts through on the cabinet's speaker, body and bass under it. The
  lowest bass (under about 35 to 70 Hz) is filtered away, a soft
  saturation makes them loud.
* **Engines.** Loops of 8 s from recordings of real tanks: `ENGINE_IDLE`
  the tank standing with the engine running, `ENGINE_REV` driving with the
  engine up and the tracks rattling; the game fades between them as the
  engine revs. `ENEMY_ENGINE` is another engine, so that it does not sound
  like the player's. Each layer is cut to the same length, its join faded
  with equal power, and the level evened out over the loop. They are
  3 dB under the first version (0.1.10), which was too loud.
* **Obstacles.** BOING is a heavy metal crash with what counts in its
  first half second (it is cut after 0.47 s while the player pushes on).
  BLKSND is 0.138 s of grinding with a small clank at its start: started
  over and over at the board's pace, a rattle of tracks slipping.

The recordings (Freesound number, file, author):

* 394128 Space Explosion.wav (morganpurkis)
* 490266 Sci-Fi_Explosion_2.wav (Anomaex)
* 413224 Energy impact 4 (ihitokage)
* 814052 Retro Bit Explosion (qubodup)
* 873747 Low_Mid_End_Void_Energy_Blast_Punch_01 (The_Elementus)
* 182793 Military Vehicle Humming (qubodup)
* 186934 engine4_loop.wav (ReadeOnly)
* 329800 t26_moving_01.wav (greatmganga), a T-26 driving
* 849062 tracks moving (christerljung)
* 121531 tracked vehicle.wav (cognito perceptu)
* 572294 tank_engine.wav (monosfera)
* 187676 Driving Tank Engine (qubodup)
* 151624 Clank Car Crash Collision (qubodup)
* 67617 Metal Crash Collision (qubodup)
* 837389 Heavy Robot Footsteps (Artem_Zaitsev), one step as the weight
* 336001 Metal Grinding Very Slow (Ev-Dawg)
* 697758 Metal Resonant Scrape Impact (SoundDesignForYou)
* for the first shot (0.1.9), replaced by Battle Zone II's (without
  `--bzoneii` the script still builds it): 702000 Heavy blaster shot 05
  (xkeril), 728519 Sci fi cannon shot (doudar41), 567308 PlasmaCannon.wav
  (BigDino1995)
* fetched, not used: 569903 ScifiCannon.wav (BigDino1995), 867558 sci-fi
  weapon shot (dimapain), both nearly all deep bass; 332058 Collision
  (qubodup), 512475 Metallic clunk (michael_grinnell), 569555
  Foley_Impact_Metal_Long_Mono (Nox_Sound)

The originals are in `assets/freesound/`. To make the pack again:

```bash
python -I tools/mkfreesound.py assets/freesound --bzoneii assets/bzoneii
```

(needs numpy and soundfile). The missile and the eight tunes stay the
board's. Your own files in `save/sounds/` come first. `tools/deploy.sh`
empties `sounds/` on the cabinet before copying.

`sound_replace` is on by default. The computed sounds of
`tools/mksounds.py` (engine and missile too) are no longer part of the
pack; the script writes to `_scratch/synth`.

The levels in the game (60 s on a PC, `bzshot`): the engine about 1.5 to
2 dB over the board's before the 3 dB cut, the shot a little softer than
the explosions, nothing clips.

## Louder tunes

With ENHANCED the POKEY's tunes (radar beep, warning, bell, fanfare,
saucer) are 11 dB louder (`TUNE_BOOST` in `sound/sound.c`); the missile's
buzz is not. On the board they are 14 dB under the engine and still
heard, because its engine is a low hum; the recorded engines and tracks
fill the middle where the tunes are, and they were lost. Measured on a PC
(60 s of play): the tunes are now 4 dB over everything else at the same
moments, before 6.5 dB under.

## Ducking

`enh_ducking` (test menu SOUND): while a POKEY tune plays (radar beep,
warning, bell ...), a shot or an explosion is heard, the engines, the
player's and the enemy's, go down to 55 percent (5 dB): down in 15 ms, up
again in 0.3 s. The reason: with the tank engines the radar sounds were
lost. With ORIGINAL there is no ducking.

## Stereo and distance

With `enh_stereo` the enemy's shot and explosion, the missile's buzz and
the saucer's two sounds come from the side the source is on. With
`enh_distance` they are quieter far away, down to 30 percent; the enemy's
explosion less, down to 55 percent (1.1 - distance / 60000; it should
still crash far away). The player's shot, the player being hit, the
engine, the radar and the other tunes stay in the middle. This holds for
the board's sounds and for recordings alike.

## Checked against MAME

The port's sound is a model: the POKEY and the analog circuits (noise,
shot, explosion, engine) computed in C, after MAME's description of the
board (`src/sound/sound.c`, `src/sound/pokey.c`). It is checked against
MAME with the same writes:

```bash
sh tools/soundcheck.sh
```

`tools/mame/bzsound.lua` plays 75 seconds of the game in MAME (hits,
being hit, missiles, all eight tunes, the missile's buzz) and records
every write to the POKEY and the sound latch with its time; MAME itself
records the sound (`-wavwrite`). `bzsound --replay` plays the same writes
at the same times on the model. `tools/soundcheck.py` cuts from both the
places where a sound is heard alone (with the engine) and compares the
level (against the engine idling), the strongest frequency, the
brightness and, for the POKEY, the spectrum in bands.

After the match (model minus MAME, dB):

| Sound | Level | Pitch |
|---|---|---|
| engine idling | reference | 53 Hz, the same |
| engine under load | -1.2 | 59 Hz, the same |
| explosion loud / soft | 0.0 / -0.5 | the same / 71 instead of 58 Hz |
| POKEY tunes | -0.7, every band up to 12 kHz within 1.2 | 209 Hz, the same |
| missile buzz | -1.1, every band within 1.2 | 105 Hz, the same |
| shot loud / soft | +4.2 / -2.2, see below | |

Before the match the POKEY's tunes lacked 10 to 15 dB in the highs and
the missile's buzz nearly 7 dB: the model had added the channels linearly
and put them behind a fixed 1 kHz filter. The real POKEY pulls its output
down through a network of resistors, one per volume bit; not linearly,
and the filter behind it opens with the volume. It is now computed tick
by tick as in MAME (`pokey_run_rc`). The whole model is also 6 dB louder
than before, close to MAME (which on purpose drives it into clipping).

**The shot, a deliberate difference.** In MAME's circuit the explosion's
bit (D1) sets the shot's volume, not the shot's own (D3). The original
program calls D3 `LOUDSH`, sets it when the player fires and clears it
for the enemy's shot; Al Kossow's list of the bits in MAME's source says
the same. The port follows the program: the player's shot is loud, the
enemy's soft. With MAME's wiring (tried) the model is within 0.4 dB for
the shot too, so the circuit itself is right.

## The board's sounds as files

`sounds/` in the project folder holds all 15 sounds as the port's sound
model makes them, under the names of the tables above (48 kHz, 16 bit,
mono). Made with:

```bash
build/pc/bzsound.exe --roms D:/Mame/roms
```

`bzsound` plays each sound as the game asks for it: the tunes through
SNDON and MODSND until they end; shot and explosion switched on as long as
in the game (4 and 254 or 111 interrupts) and then dying away; the
missile's buzz and the engine two seconds each. Every file is brought to
90 % of full scale (the sound is made a second time, louder, not
amplified afterwards). `--raw` keeps the board's levels to each other;
then the tunes are very quiet (0.1 to 1.7 % of full scale), the noises
louder (up to 31 %).

The files are a starting point for editing: a changed file of the same
name in `save/sounds/` replaces the sound in the game.

Limits: it is the port's model, not a recording of a cabinet, matched
with MAME (above).

## Open points

* Loud and soft for the explosions are taken from the source code, not
  heard on a cabinet.
* Recordings are mono; stereo files are mixed down when loaded.
* The level of the recordings against the board's sounds is a guess (half
  of full scale). In the game on a PC the shot and the explosion are about
  5 dB over the loudest sound before, without clipping.
* The pack is mixed by measurements; on the cabinet the owner has heard
  and judged it step by step (shot, engines, ducking, tunes).
* With `--original` only shots, explosions, engine and missile are
  replaced, not the eight tunes.
* The game's sound counts as music for the cabinet menu's volumes (the
  SDK's rule for a game's own stream): MUSIC VOLUME sets it, SOUND VOLUME
  does not.
