# Building and checking

## Fetch once

Two things are not part of the project and are fetched next to it (both
are in `.gitignore`):

```bash
git clone --depth 1 https://github.com/historicalsource/battlezone.git upstream
```

```bash
git clone --depth 1 https://github.com/Goldfinch2/VectorPie.git external/VectorPie
```

`upstream` is the original source code (read only). `external/VectorPie`
is the SDK for the output (0.9.0 or later: the sound goes out through its
`vp_stream_play`). Without the SDK the tools still build
(`-DBZ_BUILD_GAME=OFF`). To bring the SDK up to date:

```bash
git -C external/VectorPie pull --ff-only
```

## PC (Windows)

Needs MSYS2 with MinGW-w64 GCC and SDL2 in `C:\msys64\mingw64`, plus
CMake and Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
```

```bash
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
```

```bash
cmake --build build/pc
```

Run it (the ROMs: `--roms`, or `rom_path` in `build/pc/save/battlezone.ini`):

```bash
build/pc/battlezone.exe --windowed --roms D:/Mame/roms
```

## Tests

```bash
build/pc/test_core.exe
```

Needs no ROMs: the 6502 core, the math box, the vector generator, and the
arithmetic macros of the C routines against the core.

## Checking against MAME

MAME 0.289 and its ROM folder: `$BZ_MAME` (default `D:/Mame/mame.exe`) and
`$BZ_MAME_ROMS` (default `D:/Mame/roms`). MAME runs without picture and
sound, with a configuration folder of its own under `_scratch/mame` that is
emptied before every capture. Your own MAME settings are not touched.

Make all the standard captures again (about 10 minutes, 220 MB in
`_scratch/cap`):

```bash
sh tools/captures.sh
```

Compare everything:

```bash
sh tools/checkall.sh
```

```bash
sh tools/checkall.sh --emu
```

```bash
sh tools/checkall.sh --nmi
```

Without an option this checks the C routines frame by frame; with `--emu`
the original program on the 6502 core; with `--nmi` the interrupt routine
and the power-on state in C against the original on the core. The last
line is `ALL THE SAME` or `DIFFERENCES`.

A single capture (the script: see `tools/mame/bzcapture.lua`):

```bash
sh tools/capture.sh _scratch/cap/test.bzcap 120 "60 coin 2;70 coin 2;130 start 2;140 aim 3"
```

```bash
build/pc/bzcompare.exe _scratch/cap/test.bzcap --show 20
```

`bzcompare` knows `--emu`, `--from F`, `--to F`, `--max N` (frames
reported), `--show N` (bytes reported per frame), `--trace F` (every switch
read of frame F).

### How the comparison works

For every game frame the capture holds the original's memory at the start
of the frame (RAM, vector RAM, math box), plus the random numbers and the
reads of the switches and of the interrupt's memory that the main loop
made in that frame. `bzcompare` loads a frame's memory, runs one frame and
compares with the memory of the next. Every frame is checked on its own;
one difference does not hide the next.

What belongs to the interrupt (coins, credits, sync, tunes) is not
compared, because in MAME it came at other moments. `bznmi` checks that.

## Coverage

Which lines of the C routines the captures run through:

```bash
cmake -S . -B build/cov -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBZ_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=--coverage -O0"
```

```bash
cmake --build build/cov
```

```bash
sh tools/coverage.sh
```

## Picture and sound without a screen

```bash
build/pc/bzshot.exe --roms D:/Mame/roms --seconds 40 --play "3 coin;5 start;6 drive f f;12 fire" --svg _scratch/shots/play.svg --at 20 --wav _scratch/shots/play.wav
```

`magick -density 60 picture.svg picture.png` makes a PNG of it. With
`--original` the original program runs on the core. `--color white`,
`--color overlay` and `--nosmooth` switch color and the pictures between
frames off, `--color classic` chooses the first palette, `--nosolid` and
`--nogrid` switch occlusion and the ground grid off. `--sounds FOLDER`
loads recordings and reports at the end how often each was started.
`--soundlist` prints the lengths of the POKEY's eight tunes. `--every S`
writes a numbered picture every S seconds (0.01667: every picture of a
60 Hz screen).

On every run `bzshot` also measures how smooth the picture is:

* how well the picture drawn ahead meets the game's next one (the line
  "pictures drawn ahead")
* how evenly the mountains move across a 60 Hz screen while turning (the
  line "the land on a 60 Hz screen"), with `--nosmooth` to compare
* "single lines": how much the step of single lines changes from picture
  to picture (a few percent is even, 200 is stop-and-go)
* "the beam's way": the lit and the dark length per picture, the board's
  cost

`--hz N` measures the evenness at N pictures a second (the cabinet: 50),
`--rounded` takes the game's rounded positions, `--lines` moves lines
instead of computing the pictures between frames. `--plain` switches every
enhanced option off, `--pace` the cabinet's pace. In the script, `aim`
makes the tank turn to the enemy and fire by itself, so hits get into the
picture. The sound is stereo.

Fitting the pace model (`src/game/pace.c`) again:

```bash
: > _scratch/pace.csv; for f in _scratch/cap/*.bzcap; do build/pc/bzcompare.exe "$f" --quiet --csv _scratch/pace.csv; done
python -I _scratch/pacefit.py
```

`bznmi` prints how many cycles an interrupt takes. `sh tools/soundcheck.sh`
checks the sound against MAME (needs a Python with numpy, `BZ_PYTHON`). The
board's own sounds are written to `sounds/` by
`build/pc/bzsound.exe --roms D:/Mame/roms` (docs/SOUNDS.md).

The sound pack in `package/sounds` is mixed from recordings by
`python -I tools/mkfreesound.py assets/freesound --bzoneii assets/bzoneii`
(needs numpy and soundfile). `tools/mksounds.py` makes the older computed
sounds (into `_scratch/synth`, not used).

The analog mode can be driven in the script with `yoke L R` (both levers,
-256 to 256). `sh tools/checkanalog.sh` checks that at the ends it does
the same as the lever switches.

## Making the symbols again

`src/core/bz_symbols.h` is generated, not edited by hand:

```bash
python -I tools/mksyms.py > src/core/bz_symbols.h
```

Looking up addresses (the ROMs unpacked in a folder, with
`bzonea/036414-01.e1`):

```bash
python -I tools/dis6502.py _scratch/bzroms 6810 68D5
```

## The cabinet (Raspberry Pi, VectorPie)

From Windows, without WSL: clang and lld on the PC against a copy of the
cabinet's libraries (by default the Tempest 2000 port's). In Git Bash:

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DBZ_PI_SYSROOT=F:/Tempest 2000 Port/build/pi-sysroot"
cmake --build build/pi --target battlezone
sh tools/deploy.sh
```

`tools/deploy.sh` copies the program, `game.ini`, `battlezone.ini`,
`marquee.png`, `run.sh` and the sound pack to `/persistent/games/bzenhanced`
on the cabinet (`pi@192.168.0.122`), ends a game that is running and starts
the new one through `run.sh`, which holds the cabinet's menu and brings it
back when the game ends. The output with the picture costs is in `run.log`
there. If another game has the board, the new build is only installed.

The folder is also the package the menu knows
(`external/VectorPie/sdk/docs/07-packages.md`): title, manufacturer and
marquee come from `package/game.ini`. The menu reads the manifests when it
starts. Count the version in `package/game.ini` up before a new release.

The package file for other cabinets: `sh tools/make_vpk.sh` (after the Pi
build) makes `build/bzenhanced-<version>.vpk` with the program, `game.ini`,
`battlezone.ini`, the marquee and the sounds (with `sounds/CREDITS.txt`),
without ROMs. It is installed from the cabinet's menu (Settings -> GAMES
-> INSTALL GAMES), from `/persistent/games/packages/` or a USB drive's
`games/` folder. Releases: a GitHub release with the package
(`gh release create v<version> build/bzenhanced-<version>.vpk`), and the
shared Google Drive folder "games"
(https://drive.google.com/drive/folders/1N5vPb5K46OlYJ-xtTOY6IWD4E5GqMYiS),
uploaded through the owner's Chrome, "New" -> "File upload".

The program finds the ROMs through `dir_rom` in the cabinet's `advmame.rc`
(`/usr/local/share/advance/rom/bzone.zip`). Settings and high scores are
kept there under `save/`.

With `VP_STATS=1` (as `run.sh` starts it) the program writes a
"battlezone pace" line every second: the time between pictures, the game's
frames, how many pictures had to stand still, and the sound (times the
stream ran dry, samples dropped, the loudest sample).

Without a keyboard, the test menu opens when start and fire are held
together for two seconds.
