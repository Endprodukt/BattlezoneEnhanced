# Battlezone Enhanced

Battlezone (Atari, 1980) als C-Programm. Grundlage ist der originale
Quelltext ([historicalsource/battlezone](https://github.com/historicalsource/battlezone)),
Routine für Routine nach C übertragen und Frame für Frame gegen MAME geprüft.

Ziel: erst das unveränderte Spiel vollständig in C, dann eine flüssige
Darstellung, besserer Ton und Farbvektoren. Das Spiel, die flüssige
Darstellung und die Farbe sind da, der Ton steht noch aus. Aktueller
Stand, offene Punkte und nächste Schritte: [STATUS.md](STATUS.md). Zum Weiterarbeiten:
[RESUME.md](RESUME.md). Bauen und Prüfen: [BUILD.md](BUILD.md).

## Was es ist

* **Das Spiel ist das Original, in C** (`src/game`): Hauptschleife,
  Interrupt, Münzen, Ansicht, Gegner, Rakete, Untertasse, Highscores, alle
  vier Sprachen. Die Routinen heißen wie im Quelltext und tragen dessen
  Kommentare. Es läuft kein 6502 mit.
* **Geprüft gegen MAME** (Satz `bzonea`): RAM, Vektor-RAM und Math Box
  stimmen nach jedem Frame Byte für Byte überein. Zahlen in STATUS.md.
* **Referenz im selben Programm**: `--original` lässt das Originalprogramm
  aus den ROMs auf einem eigenen 6502-Kern laufen.
* **Nachgebildete Hardware**: Math Box, Vektorgenerator (mit dem Fenster
  der Battlezone-Platine), POKEY, die diskreten Sounds (Motor, Schuss,
  Explosion).
* **Ausgabe über das VectorPie-SDK**: am PC ein Fenster, am Automaten die
  Vektorplatine (USB-DVG). Auf dem Automaten installiert als "Battlezone
  Enhanced" mit eigenem Marquee (`package/`, `tools/deploy.sh`).

Was der Port dem Automaten hinzufügt. Die Spielmechanik bleibt dabei
unverändert, das Spiel selbst merkt von alledem nichts:

* **Flüssiges Bild.** Das Spiel rechnet 15 Frames pro Sekunde. Für jedes
  Bild dazwischen setzt der Port alles, was sich bewegt, an seinen Platz
  zwischen zwei Frames und lässt die Zeichenroutinen des Spiels dieses
  Bild auf einer Kopie neu zeichnen, mit feinerer Winkelauflösung als das
  Original (`render/smooth.h`; dasselbe Verfahren wie im Star-Wars-Port).
* **Farbe.** Jede Linie bekommt eine Farbe nach dem, was sie ist
  (`render/color.h`). Zwei Paletten: nach Ataris Flyer von 1980 (Vorgabe)
  und die erste, grüne. Bilder: [docs/bilder](docs/bilder).
* **Massive Objekte.** Was hinter einem Objekt liegt, wird nicht
  gezeichnet: Rückseiten, Berge, Gitter (`render/solid.h`).
* **Bodengitter** in der Perspektive des Spiels (`render/ground.h`).
* **Sound-Ersatz.** Jeder der 15 Sounds lässt sich durch eine WAV-Datei
  ersetzen. Ein errechnetes Paket für Schüsse, Explosionen, Motor und
  Rakete liegt bei. Liste: [docs/SOUNDS.md](docs/SOUNDS.md).
* **Enhanced-Optionen**: Glühen, Dunst, Sterne, Spuren, Mündungsblitz,
  erweitertes Radar, Stereo-Ton, Tempo des Automaten und mehr. Siehe
  Abschnitt "Enhanced-Optionen".
* **Highscores bleiben erhalten** (`save/hiscore.bin`; dieselbe Tabelle als
  `save/highscores.txt` für das Menü des Automaten und das Marquee,
  `hiscores = yes` in `game.ini`), alle zehn Zeilen
  der Tabelle, die der Attract-Modus zeigt.
* **Drei Arten zu fahren, alle zugleich**: ein Joystick, zwei Joysticks,
  analoges Yoke mit stufenloser Geschwindigkeit. Jede Aktion frei und
  mehrfach belegbar (bis zu acht, wie in MAME).
* **Testmenü auf Tab** für alles das und für die Schalter des Automaten.

## ROMs

Die Daten des Spiels (Vektor-ROM, Objekte, Spielfeld, Texte, Melodien)
werden beim Start aus dem eigenen MAME-ROM-Satz gelesen. Im Projekt liegt
nichts davon. Gebraucht wird `bzone.zip`. Gesucht wird in:

1. `--roms PFAD`
2. `rom_path` in `battlezone.ini`
3. `dir_rom` aus der `advmame.rc` des Automaten
4. `$BZ_ROMS`, `./roms`, `~/.advance/rom`, `~/mame/roms`

Die Dateien werden per CRC erkannt, Namen und Ordner spielen keine Rolle.

Der Quelltext beschreibt **Revision 1** des Programms. Das ist MAMEs Satz
`bzonea`; dessen `036414-01` liegt in `bzone.zip` im Ordner `bzonea` und ist
Byte für Byte die ROM-Datei aus dem Quelltext-Repository. MAMEs `bzone` ist
Revision 2 und unterscheidet sich in 149 Bytes des ersten Programm-ROMs
(Highscore-Seite). Die C-Routinen lesen nur Tabellen aus ROMs, die beide
Revisionen gemeinsam haben, und laufen deshalb mit beiden Sätzen. Sie
verhalten sich immer wie Revision 1.

## Programme

| Programm | Zweck |
|---|---|
| `battlezone` | das Spiel in C |
| `battlezone --original` | das Originalprogramm auf dem 6502-Kern, zum Vergleich |
| `bzcompare AUFNAHME.bzcap` | prüft die C-Routinen Frame für Frame gegen eine MAME-Aufnahme (`--emu`: den 6502-Kern) |
| `bznmi AUFNAHME.bzcap` | prüft Interrupt-Routine und Einschaltzustand in C gegen das Original auf dem Kern |
| `bzshot` | lässt das Spiel ohne Bildschirm laufen und schreibt Bild (SVG) und Ton (WAV) |
| `bzsound` | schreibt jeden der 15 Sounds der Platine als WAV nach `sounds/` (docs/SOUNDS.md) |
| `test_core` | Tests ohne ROMs: 6502-Kern, Math Box, Vektorgenerator, Rechenmakros |

Optionen von `battlezone`: `--roms PFAD`, `--config INI`, `--original`,
`--nosound`, `--hz N`, `--dvg [PORT]`, `--window`, `--windowed`,
`--vertical`. `VP_STATS=1` gibt die Kosten pro Bild aus.

## Steuerung

| Automat | Aktion (advmame.rc) | Taste am PC |
|---|---|---|
| linker Hebel vor / zurück | `p1_doubleleft_up` / `p1_doubleleft_down` | E / D |
| rechter Hebel vor / zurück | `p1_doubleright_up` / `p1_doubleright_down` | I / K |
| Feuer | `p1_button1` | Strg links |
| Start | `start1` | 1 |
| Münze | `coin1`, `coin2` | 5, 6 |
| Ende | `ui_cancel` | Esc |

Die Tabelle zeigt die Vorgabe `auto`: die Steuerung des Automaten nach
den Namen in `advmame.rc`, am PC die genannten Tasten. Jede Aktion lässt
sich im Testmenü frei auf eine Taste, einen Knopf, eine Achse oder einen
Hut eines beliebigen Joysticks legen, auch auf mehrere zugleich (bis zu
vier, wie in MAME mit "or": `bind_fire = auto or joy2 button 3` feuert mit
dem Knopf des Automaten und mit diesem). Im Menü: Feuer auf der Zeile,
dann das Neue drücken, es kommt dazu; links oder rechts setzt die Zeile
zurück auf AUTO, auf AUTO noch einmal auf NONE (nichts löst die Aktion
aus), dann wieder AUTO.

Drei Arten zu fahren, alle zugleich (seit 0.1.13; vorher war eine zu
wählen). Was angeschlossen ist, geht:

| Art | Bedeutung |
|---|---|
| ein Joystick | digital oder die Pfeiltasten (`p1_up`, `p1_down`, `p1_left`, `p1_right`): hoch ist beide Hebel vor, runter beide zurück, links und rechts drehen auf der Stelle, die Diagonalen sind die vier Kurven des Automaten |
| zwei Joysticks | linker und rechter Joystick hoch und runter, die beiden Hebel des Automaten |
| Yoke | oder Analogstick (`p1_sticky`, `p1_stickx`): vor und zurück bestimmt die Geschwindigkeit stufenlos, links und rechts die Drehung |

Weil eine Bedienung zu mehreren gehören kann (am Automaten ist der linke
Hebel die Pfeiltasten, also auch der eine Joystick; die Achse eines
Sticks ist Yoke-Achse und, über die Hälfte, eine Richtung), gilt in jedem
Bild: Ist ein Hebel der zwei Joysticks gedrückt, fahren die Hebel. Sonst,
wenn der Yoke außerhalb seiner Totzone steht, der Yoke. Sonst der eine
Joystick. Ein digitaler Stick, der sich als Achsen meldet, fährt damit
über den Yoke-Weg, an den Anschlägen gleich.

Mit dem Yoke gelten die Hebel als stufenlos. An den Anschlägen fährt
der Panzer exakt wie am Automaten, schneller wird er nie.

## Testmenü

Bei der Highscore-Eingabe ersetzt der Port die Zeile "CHANGE LETTER WITH
RIGHT HAND CONTROLLER" durch eine, die zur zuletzt benutzten Steuerung
passt (Yoke: vor und zurück, ein Joystick: hoch und runter). Nur auf Englisch
und nur in den Farbmodi, in Farbfolie und Weiß bleibt der Originaltext.

Tab öffnet es, das Spiel steht solange. Tab speichert und kehrt zurück.
Ohne Tastatur: Start und Feuer zwei Sekunden halten, oder einen Knopf
dafür belegen (`bind_menu`).
Hoch und runter wählt eine Zeile, links und rechts ändert den Wert, Feuer
(oder Enter) öffnet eine Seite oder führt die Zeile aus.

| Seite | Inhalt |
|---|---|
| CONTROLS | Art zu fahren, Belegung jeder Aktion, Totzone des Yokes. Feuer auf einer Zeile, dann die gewünschte Taste drücken oder den Stick bewegen. Links oder rechts setzt die Zeile auf `auto` zurück, Esc bricht ab |
| PICTURE | flüssige Bewegung, massive Objekte, Bodengitter (jeweils an oder aus), Palette (Flyer, klassisch, Farbfolie, Weiß), Helligkeit, die Seite ENHANCED PICTURE, das Kalibrierbild |
| SOUND | Lautstärke, Stereo, leiser mit Entfernung, Sound-Ersatz an oder aus, Sound-Test für jede Aufnahme |
| GAME OPTIONS | Panzer, Rakete, Bonus-Panzer, Sprache, Münzen |
| Hauptseite | Highscores zurücksetzen (zweimal Feuer) |

## Enhanced-Optionen

Jede ist einzeln schaltbar, in `battlezone.ini` und im Testmenü. Keine
ändert, was das Spiel rechnet, mit einer Ausnahme: `enh_tempo` ändert,
wann es rechnet.

| Schlüssel | Vorgabe | Was | Menü |
|---|---|---|---|
| `enh_particles` | an | Funken, wenn Gegner oder Untertasse getroffen werden, in der Farbe des Getroffenen (wie in Tempest 2000). Die Trümmer haben immer diese Farbe | PICTURE, ENHANCED PICTURE |
| `enh_stars` | an | funkelnde Sterne über den Bergen, hinter Mond und Bergsilhouette; sie drehen mit dem Mond | dort |
| `enh_far` | an | zweite Bergreihe im Hintergrund, die sich beim Drehen halb so schnell bewegt wie die vordere (Parallaxe) und nur über der ersten Reihe zu sehen ist | dort |
| `enh_volcano` | an | Vulkansteine glühen im Aufstieg und kühlen im Fall ab | dort |
| `enh_flash` | an | Mündungsblitz beim eigenen Schuss, Staub wo ein Schuss einschlägt | dort |
| `enh_levers` | an | im Analogmodus: Anzeige für beide Hebel in den unteren Ecken | dort |
| `enh_stereo` | an | Schuss und Explosion des Gegners, Rakete und Untertasse kommen von ihrer Seite | SOUND |
| `enh_distance` | an | dieselben Sounds werden mit der Entfernung leiser | SOUND |
| `enh_enemy_engine` | an | Motor der Gegnerpanzer, von ihrer Seite, nah lauter, fahrend hochgedreht (nur mit `sound_replace`; `ENEMY_ENGINE.wav`, sonst der eigene Motor tiefer) | SOUND, ENEMY ENGINES |
| `enh_ducking` | an | Motoren (eigener, Gegner) werden 5 dB leiser, solange Radar-Piep, Warnton und die anderen Melodien, ein Schuss oder eine Explosion zu hören sind | SOUND |
| `enh_engine` | an | im Analogmodus: Motor dreht stufenlos mit dem Hebelweg hoch | CONTROLS |
| `enh_tempo` | aus | Tempo des Automaten: ein Frame dauert so lang, wie sein 6502 brauchte | GAME OPTIONS, CABINET PACE |
| `menu_hold` | an | Start und Feuer zwei Sekunden gehalten öffnen das Testmenü | CONTROLS |
| `bind_menu` | `auto` | eigene Taste oder Knopf für das Testmenü, zusätzlich zu Tab | CONTROLS, TEST MENU |
| `start_lamp` | an | am Automaten: START-Lampe, wie das Spiel sie schaltet | nur in der Datei |

## Einstellungen

`battlezone.ini` neben dem Programm (Vorgaben), eigene Werte in
`save/battlezone.ini`.

| Schlüssel | Bedeutung |
|---|---|
| `rom_path` | Ordner oder zip mit den ROMs |
| `lives` | Panzer pro Spiel, 2 bis 5 |
| `missile_at` | Rakete ab 5000, 10000, 20000 oder 30000 Punkten |
| `bonus_at` | Bonus-Panzer bei 15000, 25000 oder 50000 (und 100000), 0: keiner |
| `language` | `english`, `german`, `french`, `spanish` |
| `coin_mode` | `free`, `2plays`, `1play`, `2coins` |
| `controls` | entfallen (0.1.13): alle Steuerungen gehen zugleich; wird überlesen |
| `deadzone` | Totzone des Yokes, Anteil des Wegs |
| `bind_up` ... `bind_coin` | Belegung je Aktion: `auto`, `none`, `key NAME`, `joyN button K`, `joyN axis K +`/`-`, `joyN hat K up`; bis zu acht, verbunden mit ` or ` |
| `smooth` | 1: Bilder zwischen den Frames, aus dem Spielzustand neu berechnet. 2: die Linien eines Frames zum nächsten verschoben. 0: wie der Automat |
| `board_hz` | Bilder pro Sekunde auf der Vektorplatine. Muss der Bildrate der Platine entsprechen (Menü des Automaten, DVG REFRESH), sonst werden Bilder abwechselnd doppelt gezeigt und übersprungen. Vorgabe 50 |
| `occlusion` | 1: Objekte sind massiv. 0: durchsichtige Drahtmodelle wie am Automaten |
| `ground_grid` | 1: Gitter auf dem Boden. 0: leerer Boden wie am Automaten |
| `sound_replace` | im Menü SOUNDS ENHANCED (1, Vorgabe) oder ORIGINAL (0, der Ton der Platine pur). 1: WAV-Dateien in `sounds/` oder `save/sounds/` ersetzen Sounds; mitgeliefert: Schuss aus Battle Zone II, futuristische Explosionen, Panzermotor mit Ketten (docs/SOUNDS.md) |
| `volume` | 0 bis 1 |
| `color_mode` | `flyer`: jede Linie nach dem, was sie ist, in den Farben des Flyers. `color`: dasselbe mit der ersten Palette. `overlay` (im Menü ORIGINAL): wie die Farbfolie des Automaten, oberes Fünftel rot, Rest grün. `white`: eine Farbe |
| `color`, `color_top`, `color_bottom`, `overlay_split` | die Farben dazu (R G B) und die Grenze |
| `brightness`, `gamma`, `floor` | Helligkeit der Linien |
| `view_*`, `margin_*`, `flip_*` | Ausschnitt und Lage des Bildes |

## Rechtliches

Battlezone stammt von Atari. Programm- und Vektor-ROMs sind nicht
enthalten, sie kommen aus dem eigenen ROM-Satz. Das Projekt ist für den
privaten Einsatz gedacht.

Übernommen mit Nachweis im Quelltext: die Rechnung der Math Box aus MAMEs
`mathbox.cpp` (Eric Smith), die Rechnung des Vektorgenerators aus MAMEs
`avgdvg.cpp` (Mathis Rosenhauer), die Bauteilwerte der Sound-Schaltungen
aus MAMEs `bzone_a.cpp` (Couriersud), alle BSD-3-Clause. `third_party/miniz`
(MIT) liest die zip-Datei. Das VectorPie-SDK (MIT) wird unverändert
eingebunden.
