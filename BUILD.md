# Bauen und Prüfen

## Einmalig holen

Zwei Dinge gehören nicht zum Projekt und werden daneben geholt (beide
stehen in `.gitignore`):

```bash
git clone --depth 1 https://github.com/historicalsource/battlezone.git upstream
```

```bash
git clone --depth 1 https://github.com/Goldfinch2/VectorPie.git external/VectorPie
```

`upstream` ist der Originalquelltext (nur lesen). `external/VectorPie` ist
das SDK für die Ausgabe. Ohne SDK lassen sich die Werkzeuge trotzdem bauen
(`-DBZ_BUILD_GAME=OFF`).

## PC (Windows)

Voraussetzung: MSYS2 mit MinGW-w64-GCC und SDL2 unter `C:\msys64\mingw64`,
dazu CMake und Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
```

```bash
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
```

```bash
cmake --build build/pc
```

Starten (die ROMs: `--roms`, oder `rom_path` in `build/pc/save/battlezone.ini`):

```bash
build/pc/battlezone.exe --windowed --roms D:/Mame/roms
```

## Tests

```bash
build/pc/test_core.exe
```

Braucht keine ROMs: 6502-Kern, Math Box, Vektorgenerator, die Rechenmakros
der C-Routinen gegen den Kern.

## Prüfen gegen MAME

MAME 0.289 und sein ROM-Ordner: `$BZ_MAME` (Vorgabe `D:/Mame/mame.exe`) und
`$BZ_MAME_ROMS` (Vorgabe `D:/Mame/roms`). MAME läuft ohne Bild und Ton mit
einem eigenen Konfigurationsordner unter `_scratch/mame`, der vor jeder
Aufnahme geleert wird. Die eigenen MAME-Einstellungen bleiben unberührt.

Alle Standardaufnahmen neu machen (rund 10 Minuten, 220 MB in `_scratch/cap`):

```bash
sh tools/captures.sh
```

Alles vergleichen:

```bash
sh tools/checkall.sh
```

```bash
sh tools/checkall.sh --emu
```

```bash
sh tools/checkall.sh --nmi
```

Ohne Option prüft das die C-Routinen Frame für Frame, mit `--emu` das
Originalprogramm auf dem 6502-Kern, mit `--nmi` Interrupt-Routine und
Einschaltzustand in C gegen das Original auf dem Kern. Am Ende steht
`ALL THE SAME` oder `DIFFERENCES`.

Eine einzelne Aufnahme (Skript: siehe `tools/mame/bzcapture.lua`):

```bash
sh tools/capture.sh _scratch/cap/test.bzcap 120 "60 coin 2;70 coin 2;130 start 2;140 aim 3"
```

```bash
build/pc/bzcompare.exe _scratch/cap/test.bzcap --show 20
```

`bzcompare` kennt `--emu`, `--from F`, `--to F`, `--max N` (gemeldete
Frames), `--show N` (gemeldete Bytes je Frame), `--trace F` (alle
Schalter-Lesezugriffe des Frames F).

### Wie der Vergleich arbeitet

Die Aufnahme enthält zu jedem Spielframe den Speicher des Originals beim
Start des Frames (RAM, Vektor-RAM, Math Box), dazu die Zufallszahlen und
die Lesezugriffe auf Schalter und auf den Speicher des Interrupts, die
die Hauptschleife in diesem Frame gemacht hat. `bzcompare` lädt den
Speicher eines Frames, lässt einen Frame laufen und vergleicht mit dem
Speicher des nächsten. Jeder Frame wird für sich geprüft, ein Fehler
verdeckt den nächsten nicht.

Nicht verglichen wird, was dem Interrupt gehört (Münzen, Credits,
Sync, Melodien), weil er in MAME zu anderen Zeitpunkten kam. Dafür gibt es
`bznmi`.

## Abdeckung

Welche Zeilen der C-Routinen die Aufnahmen durchlaufen:

```bash
cmake -S . -B build/cov -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBZ_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=--coverage -O0"
```

```bash
cmake --build build/cov
```

```bash
sh tools/coverage.sh
```

## Bild und Ton ohne Bildschirm

```bash
build/pc/bzshot.exe --roms D:/Mame/roms --seconds 40 --play "3 coin;5 start;6 drive f f;12 fire" --svg _scratch/shots/play.svg --at 20 --wav _scratch/shots/play.wav
```

`magick -density 60 bild.svg bild.png` macht daraus ein PNG. Mit
`--original` läuft das Originalprogramm auf dem Kern. `--color white`,
`--color overlay` und `--nosmooth` schalten Farbe und Zwischenbilder ab,
`--color classic` wählt die erste Palette, `--nosolid` und `--nogrid`
schalten Verdeckung und Bodengitter ab. `--sounds ORDNER` lädt Aufnahmen
und meldet am Ende, wie oft jede gestartet wurde. `--soundlist` gibt die
Längen der acht POKEY-Melodien aus.

`bzshot` misst bei jedem Lauf auch die flüssige Darstellung:

* wie gut das vorausgezeichnete Bild das nächste Bild des Spiels trifft
  (Zeile "pictures drawn ahead")
* wie gleichmäßig die Berge auf einem 60-Hz-Schirm beim Drehen wandern
  (Zeile "the land on a 60 Hz screen"), mit `--nosmooth` zum Vergleich

`--hz N` misst die Gleichmäßigkeit bei N Bildern pro Sekunde (Automat:
50), `--rounded` nimmt die gerundeten Positionen des Spiels, `--lines`
verschiebt Linien, statt die Zwischenbilder zu berechnen. Die Zeile
"single lines" sagt, wie stark sich der Schritt einzelner Linien von Bild
zu Bild ändert: wenige Prozent sind gleichmäßig, 200 sind Stop-and-go.
`--plain` schaltet alle Enhanced-Optionen ab, `--pace` das Tempo des
Automaten. Im Skript lässt `aim` den Panzer selbst auf den Gegner
drehen und schießen, so kommen Abschüsse ins Bild. Der Ton ist stereo.

Das Tempo-Modell (`src/game/pace.c`) neu anpassen:

```bash
: > _scratch/pace.csv; for f in _scratch/cap/*.bzcap; do build/pc/bzcompare.exe "$f" --quiet --csv _scratch/pace.csv; done
python -I _scratch/pacefit.py
```

`bznmi` gibt aus, wie viele Zyklen ein Interrupt braucht. Das Sound-Paket
erzeugt `python -I tools/mksounds.py`. Den Ton gegen MAME prüft
`sh tools/soundcheck.sh` (braucht ein Python mit numpy, `BZ_PYTHON`). Die Sounds der Platine selbst
schreibt `build/pc/bzsound.exe --roms D:/Mame/roms` nach `sounds/`
(docs/SOUNDS.md).

Der analoge Modus lässt sich im Skript mit `yoke L R` fahren (beide Hebel,
-256 bis 256). `sh tools/checkanalog.sh` prüft, dass er an den Anschlägen
dasselbe tut wie die Hebelschalter.

## Symbole neu erzeugen

`src/core/bz_symbols.h` wird erzeugt und nicht von Hand geändert:

```bash
python -I tools/mksyms.py > src/core/bz_symbols.h
```

Adressen nachsehen (ROMs entpackt in einem Ordner, mit `bzonea/036414-01.e1`):

```bash
python -I tools/dis6502.py _scratch/bzroms 6810 68D5
```

## Automat (Raspberry Pi, VectorPie)

Von Windows aus, ohne WSL: clang und lld vom PC gegen eine Kopie der
Bibliotheken des Automaten (voreingestellt die des Tempest-2000-Ports).
In Git Bash:

```bash
export PATH="/c/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin:$PATH"
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DBZ_PI_SYSROOT=F:/Tempest 2000 Port/build/pi-sysroot"
cmake --build build/pi --target battlezone
sh tools/deploy.sh
```

`tools/deploy.sh` kopiert Programm, `game.ini`, `battlezone.ini`,
`marquee.png`, `run.sh` und das Sound-Paket nach
`/persistent/games/bzenhanced` auf dem Automaten (`pi@192.168.0.122`),
beendet ein laufendes Spiel und startet das neue über `run.sh`. Das hält
das Menü des Automaten an und holt es zurück, wenn das Spiel endet. Die
Ausgabe mit den Bildkosten steht dort in `run.log`.

Der Ordner ist zugleich das Paket, das das Menü kennt
(`external/VectorPie/sdk/docs/07-packages.md`): Titel, Hersteller und
Marquee kommen aus `package/game.ini`. Das Menü liest die Manifeste beim
Start. Vor einem neuen Stand die Version in `package/game.ini` hochzählen.

Paketdatei für andere Automaten: `sh tools/make_vpk.sh` (nach dem Pi-Build)
macht `build/bzenhanced-<Version>.vpk` mit Programm, `game.ini`,
`battlezone.ini`, Marquee und Sounds (samt `sounds/CREDITS.txt`), ohne
ROMs. Installieren über das Menü des Automaten (Settings -> GAMES ->
INSTALL GAMES) aus `/persistent/games/packages/` oder dem `games/`-Ordner
eines USB-Sticks. Hochladen in den geteilten Google-Drive-Ordner "games"
(https://drive.google.com/drive/folders/1N5vPb5K46OlYJ-xtTOY6IWD4E5GqMYiS):
über den Chrome des Besitzers, "Neu" -> "Datei hochladen".

Die ROMs findet das Programm über `dir_rom` der `advmame.rc` des
Automaten (`/usr/local/share/advance/rom/bzone.zip`). Eigene Einstellungen
und Highscores liegen dort unter `save/`.

Mit `VP_STATS=1` (so startet `run.sh`) schreibt das Programm jede Sekunde
eine Zeile "battlezone pace": Abstand der Bilder, Frames des Spiels, und
wie viele Bilder stillstehen mussten.

Ohne Tastatur öffnet sich das Testmenü, wenn Start und Feuer zwei
Sekunden zusammen gehalten werden.
