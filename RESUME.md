# Weiterarbeiten

Stand und offene Punkte stehen in [STATUS.md](STATUS.md), Bauen und Prüfen in
[BUILD.md](BUILD.md). Hier steht, was man wissen muss, um im Code
weiterzumachen.

## Aufbau

```
upstream/                Originalquelltext (historicalsource/battlezone), nur lesen
external/VectorPie/      das SDK, unverändert
src/core/machine.[ch]    die Platine: Speicher 0000-7FFF, Schalter, POKEY-Register, Sound-Latch
src/core/bz_symbols.h    alle Adressen unter den Originalnamen (erzeugt: tools/mksyms.py)
src/core/emu.[ch]        das Originalprogramm auf dem 6502-Kern (Referenz)
src/cpu/m6502.[ch]       der 6502-Kern
src/mbox/                Math Box
src/avg/                 Vektorgenerator: Displayliste -> Linien, mit dem Fenster
src/game/m6502.h         die Makros, mit denen die C-Routinen geschrieben sind
src/game/mainline.c      MAIN, WNSHLD, BATTLE, Highscore-Seiten, MSGS, INFO
src/game/view.c          MOUNTS, VOLCNO, ROTATE, ROTPNT, EXPLDE, DRAW, PNTPUT, Sinus
src/game/play.c          Schüsse, Kollisionen, MOTION, ROBOT, BUZBOM, SAUCMV, TRACK, DRADAR
src/game/nmi.c           der Interrupt und PWRON
src/game/vgut.c          VGUT (Displayliste schreiben) und DIGITS
src/game/sound.c         BZSOUN (SNDON, MODSND)
src/game/coin.c          MOOLAH
src/sound/               POKEY und die diskreten Sounds
src/core/tags.h          was eine Linie ist: die Notizen der Zeichenroutinen (vgtag)
src/render/smooth.[ch]   Bilder zwischen den Frames
src/render/color.[ch]    die Farbe jeder Linie
src/render/solid.[ch]    Verdeckung: Flächen der Objekte, Linien dahinter abschneiden
src/render/ground.[ch]   Bodengitter
src/render/hiscore.[ch]  Highscore-Tabelle laden und speichern
src/sound/samples.[ch]   Sound-Ersatz durch WAV-Dateien
assets/freesound/        die Freesound-Aufnahmen (CC0), aus denen package/sounds gemischt ist
assets/bzoneii/          Sounds von Battle Zone II vom Automaten (der Schuss), nur für den eigenen
src/render/enhance.[ch]  Enhanced-Optionen im Bild: Sterne, Spuren, Staub, Blitz, Glühen, Dunst, Radar, Pfeil, Hebel
src/game/pace.[ch]       Tempo des Automaten (enh_tempo)
src/render/config.[ch], draw.[ch]   Einstellungen, Zeichnen über das SDK
src/platform/controls.[ch]   Belegungen, die drei Arten zu fahren
src/platform/menu.[ch]   das Testmenü (Tab)
src/platform/app, audio, src/main.c   Fenster, Ton, Programmschleife
src/capture/             das Format der MAME-Aufnahmen
tools/                   bzcompare, bznmi, bzshot, Aufnahme- und Prüfskripte, Disassembler
```

## Wie die C-Routinen geschrieben sind

Jede Routine des Originals ist eine C-Funktion mit dem Originalnamen
(`bz_rotate` ist `ROTATE`). Die Register des 6502 stehen in der Maschine
(`m->r`), jeder Befehl ist ein Makro aus `src/game/m6502.h`:

```c
LDA(RD(BZ_TANGLE));  SEC();  SBC(RD(BZ_RGOAL));  if (BPL) goto l3;
```

Das ist Absicht und kein Zwischenstand, den man glätten sollte. Das
Original reicht Werte in A, X, Y und dem Carry zwischen Routinen weiter,
prüft Flags, die eine Routine viel früher gesetzt hat, addiert ohne `CLC`
mit dem Carry von irgendwo, und hinterlässt Reste in Hilfszellen, die ein
späterer Frame liest. Nur so stimmt jedes Byte mit MAME überein.

Regeln:

* Sprungmarken heißen wie die lokalen Marken des Originals (`10$` wird
  `l10`). Kommentare in Großbuchstaben sind die des Originals.
* Jeder Speicherzugriff geht über `RD`/`WR`, auch auf Page 0. Der
  Vergleich hängt sich dort ein.
* `PHA`/`PLA` sind eine lokale Variable, `PHP`/`PLP` sind `FLAGS`/`RESTORE`.
* Berechnete Sprünge (`PHA PHA RTS` über eine Tabelle) sind ein `switch`.
* Zahlen im Quelltext sind hexadezimal (`.RADIX 16`), dezimale haben einen
  Punkt (`22.`). Im Zweifel im Disassembly nachsehen (`tools/dis6502.py`).
  `16*2` in WNSHLD ist 2C.
* Index 0 ist der Spieler, Index 2 der Gegner: `TPOSX+2`, `FIRECT+2`,
  `COLFLG+2`, `HITS+2`. Die Zellen dazwischen gehören anderen Variablen
  (`COLFLG+2` ist `SAVE1+1`).
* `ATRACT` ist FF, während gespielt wird, und 0 im Attract-Modus.

Wer eine Routine verständlicher schreiben will: erst wenn die flüssige
Darstellung es braucht, und dann als zweite Funktion daneben, die den
Zustand liest. Die geprüfte Routine bleibt.

## Zeitablauf

* Interrupt (NMI): 246,09 pro Sekunde (12,096 MHz / 8 / 6144).
* Spielframe: wenn der Interrupt `SYNC` gesetzt hat (jeder 16.) und ein
  Displaypuffer frei ist. Der Puffer wird bei jedem 6. Interrupt
  gewechselt. Daraus ergeben sich rund 15 Frames pro Sekunde.
* `bz_main_poll` nach jedem Interrupt: rechnet einen Frame in einem Zug,
  sobald beides erfüllt ist. Der 6502 des Automaten braucht dafür 50.000
  bis 100.000 Zyklen und wird in vollen Szenen langsamer als 15 pro
  Sekunde. Die C-Routinen brauchen keine Zeit (siehe STATUS.md, Tempo).
* Zwei Displaypuffer (2002 und 2802). `VECRAM` (2000) ist ein Sprung auf
  den gezeigten. Der Vektorgenerator fängt immer bei 2000 an.

## Prüfen nach jeder Änderung an `src/game`

```bash
sh tools/checkall.sh && sh tools/checkall.sh --nmi
```

Beides muss `ALL THE SAME` melden. Wenn nicht: `bzcompare DATEI --from F
--to F --show 40` zeigt die abweichenden Bytes mit Namen. Was der Frame
gelesen hat: `--trace F`.

Für neue Spielsituationen eine Aufnahme mit Skript machen
(`tools/mame/bzcapture.lua`): `coin`, `start`, `fire`, `drive`, `aim` (dreht
zum Gegner und schießt), `auto` (fährt zufällig), `poke ADRESSE WERT`, `dip`.
Nützliche Adressen: Punkte `B8`/`B9` (BCD, in Tausend), Panzer `CC`,
Raketen bisher `02EC` (ab 5 kommen Super-Panzer).

## Fallen

* **Quelltext nie über Shell-Heredocs ändern, wenn Backslashes oder
  Apostrophe vorkommen.** Aus `\n` wird dort ein echter Zeilenumbruch. Das
  hat hier mehrmals C-Dateien zerstört. Dateien mit dem Edit- oder
  Write-Werkzeug ändern.
* Python unter Windows kennt keine Pfade wie `/f/...`. Relativ zum
  Projektordner arbeiten oder `F:/...` schreiben.
* MAME merkt sich Optionsschalter, die ein Skript gesetzt hat.
  `tools/capture.sh` leert deshalb vor jeder Aufnahme seinen
  Konfigurationsordner.
* MAME-Lua: `machine:exit()` aus einem Tap beendet MAME nicht. Die Länge
  einer Aufnahme bestimmt `-seconds_to_run`.
* Das Spiel liest mit `CMP (0,X)` "um Zeit zu verbrauchen" beliebige
  Adressen, je nachdem, was in Page 0 steht: auch Schalter, die
  Zufallszahl oder die Adresse des NMI-Vektors. Die C-Routinen machen
  diese Zugriffe mit (`IX(0)`), sonst verschiebt sich die Folge der
  Zufallszahlen. Die Aufnahme erkennt den Interrupt deshalb nicht am
  Lesen des Vektors allein.
* Der erste Interrupt nach dem Einschalten darf erst nach einer Periode
  kommen. Vorher steht der Stackpointer noch nicht, und die
  Interrupt-Routine hält dann an (Stack-Prüfung `TSX, BMI`).
* `ROBCHK` wird beim Spielstart aus dem Interrupt aufgerufen und benutzt
  Math Box und Hilfszellen der Hauptschleife. Das ist im Original so.

## Was der Port hinzufügt und wie

Alles hier läuft neben dem Spiel. Die C-Routinen rechnen dasselbe wie
vorher; `checkall.sh` muss nach jeder Änderung weiter "ALL THE SAME" sagen.

**Notizen der Zeichenroutinen.** Jede Routine, die in die Displayliste
schreibt, setzt vorher `m->tag` (`core/tags.h`: Art und Schlüssel, etwa
Objekt + Panzer). `bz_wr8` legt das Tag zu jedem Wort des Vektor-RAM in
`m->vgtag`. `bz_avg_run_raw` gibt jede Linie ungeschnitten mit Tag, ROM-
Unterprogramm (`sub`), Platz darin (`n`) und laufender Nummer (`ord`) aus.

**Flüssiges Bild, berechnet** (`render/smooth.c`, `compute`; Vorgabe
seit 0.1.3). Für jeden Frame liegen die Maschine davor (`a`) und danach
(`b`) vor. Für ein Bild zur Zeit t wird `a` kopiert, alles Bewegte auf den
Weg nach `b` gesetzt (`between16`, `between_hl`, `between_angle`; Sprünge
werden nicht durchflogen) und `bz_picture` zeichnet. Auf der Kopie ist
`fine` gesetzt: `fine_view` und `fine_object` in `game/view.c` schreiben
Sinus und Kosinus aus 16 Bit Winkel in die Math Box (BSIN und ACOS sind
deren Eingänge, kein Speicher). Was neu interpoliert werden muss, wenn
etwas springt: Die Zeile "single lines" von `bzshot` je Objektart ansehen
(der Filter dafür war vorübergehend eingebaut, siehe Verlauf in STATUS.md).
EXPLDE und die Einschläge bewegen sich im Zeichenweg selbst einen Schritt
weiter; das gilt für jedes Zwischenbild gleich und stört nicht.

**Flüssiges Bild, Linien verschoben** (`smooth = 2`, das erste Verfahren). Ein Frame zeichnet erst und bewegt
dann die Welt. Nach dem Frame steht also fest, wie das nächste Bild
aussieht. `bz_picture` zeichnet es auf einer Kopie der Maschine mit den
Routinen des Spiels (`draw_only`: `main_body` kehrt nach dem Zeichnen um).
Zwischen zwei Frames wandert jede Linie der Welt vom Bild zum
vorausgezeichneten; zugeordnet über Tag, `sub` und `n` oder `ord`. Texte,
Punkte und Fadenkreuz wandern nicht. Linien, die es nur in einem der
Bilder gibt, wechseln bei der Hälfte. Das Bild ist dadurch nie hinter dem
Spiel zurück.

Die Bildpositionen des Spiels sind auf eine halbe Einheit abgerundet.
Ferne Objekte stehen deshalb mehrere Frames still und springen dann; am
Automaten sah das hakelig aus. `pntput` notiert deshalb zu jedem Punkt die
ungerundeten Quotienten der Math Box (Register 8 und 9 durch 7, Register B
durch 7), `bz_avg_run_raw` setzt die Linienenden darauf (`bz_avg_exact`,
Faktor 510). Wer an der Glättung arbeitet, prüft Objekte, nicht nur die
Berge: Die Berge waren immer gleichmäßig.

Die Frames kommen ungleichmäßig (18, 18, 12 Interrupts). Der Zeitplan
beginnt ein Bild eine Periode nach dem vorigen; die Periode ist der
Mittelwert der letzten sechs Abstände. Hier lag ein Fehler, den erst die
Messung zeigte: ein fester Plan lief dem Spiel davon, und es wurde gar
nicht geglättet. Deshalb nach Änderungen an `smooth.c` immer die Zeile
"the land on a 60 Hz screen" von `bzshot` ansehen (BUILD.md).

**Farbe** (`render/color.c`): eine Tabelle von Tag nach Farbe. Die Art
eines Hindernisses (Pyramide, Würfel) steht in PTBLO1 im ROM.

Zwei Paletten: `BZ_PALETTE_FLYER` nach Ataris Flyer (Eisberge, blauer
Boden, oranger Mond, Gelb und Orange für Logo, Visier und Punkte) und
`BZ_PALETTE_CLASSIC`. Der Standard-Panzer und die Lebensanzeige sind auf
Wunsch des Besitzers in beiden grün, wie unter der Farbfolie.

**Verdeckung** (`render/solid.c`, nach `solid.c` des Star-Wars-Ports).
`dovect` notiert zu jedem Vektor Objektnummer, Punktnummer und Entfernung
des Punkts (`m->vgnote`; die Entfernung ist das, wodurch die Math Box
teilt, Register 7). `bz_solid_init` liest Punkte und Zeichenlisten aller
Objekte aus dem ROM und findet die Flächen als ebene Ringe von Linien.
Pro Bild entsteht aus jeder Fläche ein Polygon mit der Ebene von
1/Entfernung; Linien der Welt werden abgeschnitten, wo sie dahinter
liegen. Kanten der Fläche selbst bleiben. Linien, die an einer Ecke der
Fläche beginnen, liegen als Ganzes davor oder dahinter. Berge, Mond und
Vulkan gelten als unendlich weit. Texte, Radar, Visier und der Sprung in
der Scheibe werden nie geschnitten. Reihenfolge in `smooth.c`: erst
Zwischenbild aus ganzen Linien, dann Verdeckung, dann Fenster.

**Bodengitter** (`render/ground.c`). Die Projektion des Spiels,
ausgemessen mit `_scratch/probe.c`: voraus = dx cos a + dy sin a, seitlich
= -dx sin a + dy cos a (a: TANGLE, LANGLE als 16 Bit pro Umdrehung),
Schirm X = -509,5 * seitlich / voraus, Schirm Y = 509,5 * Höhe / voraus,
der Boden liegt 320 unter dem Auge. Das Gitter wird für jedes gezeigte
Bild aus der Lage des Spielers neu erzeugt (zwischen zwei Frames
eingesetzt), nicht verschoben. Das Wippen beim Anstoßen (BOUNCE) macht es
nicht mit.

**Sound-Ersatz** (`sound/samples.c`, nach `samples.c` des Star-Wars-Ports,
Liste in docs/SOUNDS.md). `bz_sndon` meldet jede gestartete Melodie über
`m->sound_request`. Schuss und Explosion starten an der steigenden Flanke
des Schalters im Sound-Latch. Rakete und Motor sind Schleifen. Was ersetzt
ist, wird im Modell stumm geschaltet (`pokey.mute`, `shell_replaced`
usw.). Im Testmenü steht das Spiel; `snd.test` lässt dann nur die Stimme
des Sound-Tests durch.

**Enhanced-Optionen im Bild** (`render/enhance.c`). `bz_enhance_frame`
merkt sich nach jedem Frame, was geschah (Gegner zerfällt: Typ aus
`m->enemy_id`; Untertasse getroffen; Schuss gefeuert oder eingeschlagen)
und erzeugt Funken und Staub. `bz_enhance_scene` nimmt aus den Linien des
Bildes die Silhouette der Berge (je Spalte) und den Mond.
`bz_enhance_world` fügt zweite Bergreihe (nur über der Silhouette),
Sterne (hinter Silhouette und Mond, funkelnd), Funken und Staub als ganze
Linien mit Entfernung ein, nach den Linien des Spiels und vor der
Verdeckung. `bz_enhance_overlay` zeichnet Blitz und Hebelanzeige nach dem
Fensterschnitt. `bz_enhance_color` färbt Trümmer und Funken in der Farbe
des Getroffenen (`l->model` trägt die ID), Dunst und Vulkan. Eigene Linien
tragen `BZ_TAG_FX`. Die zweite Bergreihe ist periodisch über 0,6 × 65536,
damit der Sprung des Winkels bei einer vollen Drehung nahtlos bleibt.

**Strahlweg.** Die Platine braucht für den dunklen Weg zwischen Linien
so viel Zeit wie für gezeichnete Linien. Über etwa 170.000 Einheiten
(lit plus blank im Protokoll) schafft sie 50 Bilder pro Sekunde nicht
mehr, Bilder kommen zu spät. `bz_order_lines` hält den dunklen Weg kurz;
`bzshot` gibt ihn aus ("the beam's way"). Neue Effekte immer daran messen.

**Bildrate der Platine.** `board_hz` (Vorgabe 50) muss der Rate der
Platine entsprechen (DVG REFRESH im Menü des Automaten), sonst stockt die
Bewegung, weil die Platine ihr letztes Bild wiederholt. Bei Rückmeldung
"stufenhaft" zuerst das prüfen.

**Stereo** (`sound/sound.c`). Der Ring hält Paare. `bz_sound_place` liest
Lage von Gegner und Untertasse aus dem Speicher. Leiser Schuss und leise
Explosion gelten als die des Gegners, POKEY-Kanäle 3 und 4 als Rakete,
die Melodien SAUSND und DISINT als Untertasse. `pokey.level[]` liefert
dafür die Kanäle einzeln.

**Tempo des Automaten** (`game/pace.c`). Die Zyklen eines Frames werden
aus gezählten Lese- und Schreibzugriffen geschätzt (angepasst an das
Original auf dem 6502-Kern, 790 Zyklen Abweichung bei 56.000 im Mittel).
Der Interrupt nimmt dem Hauptprogramm 690 von 6144 Zyklen. Entscheidend
ist nicht, dass Frames länger als 16 Interrupts dauern (unter 1 Prozent),
sondern wann der Displaypuffer fertig gemeldet wird: Der Interrupt
übergibt ihn nur an jedem sechsten. Deshalb wird die Arbeit eines Frames
erst ausgeführt, wenn der 6502 damit fertig wäre (`bz_main_ready`, dann
warten). Ergebnis: 14,62 Frames pro Sekunde im Attract-Modus gegen 14,69
in MAME, 14,77 im Spiel gegen 14,8 bis 15,0.

**Highscores** (`render/hiscore.c`): HSCTBL und INITLS, 60 Bytes, nach
`bz_pwron` geladen, gespeichert sobald sich die Tabelle ändert und keine
Initialen mehr eingegeben werden.

**Analoges Fahren** (`analog_motion` in `game/play.c`, die einzige
Ergänzung im Spiel selbst, nur aktiv mit `m->analog.on`). Die acht
Bewegungen des Automaten sind die Ecken von: Schritte = l + r, Drehung =
(r - l) / 2, mit l und r von -1 bis 1. Dazwischen wird stufenlos
gerechnet, Kollision und Motorton laufen über denselben Code wie vorher.

**Steuerung** (`platform/controls.c`): eigene Belegungen über SDL neben
`vp_input`, je Aktion bis zu acht (" or ", `parse_binds`, im Menü
`bz_config_bind_add`). Seit 0.1.13 kein Modus mehr: alle drei Arten zu
fahren gelten zugleich, Vorrang Hebel, dann Yoke, dann ein Joystick;
`bz_controls.mode` ist die zuletzt benutzte (für den Initialen-Text). `auto` heißt: die Aktion des SDK. Das Menü öffnet nur Tab.
`p1_button6` wäre naheliegend, ist am PC aber die Taste X und kam damit
einer freien Belegung in die Quere.

## Nächste Schritte

1. **Am Bildschirm spielen und anhören.** Fenster und Menü sind bisher
   nur über Bildschirmfotos und Tastenskripte geprüft
   (`_scratch/winshot.ps1`), Joysticks und Yoke gar nicht.
2. **Ton.** Pegel und Klang gegen eine MAME-Aufnahme (`-wavwrite`) und
   gegen Aufnahmen eines echten Automaten abgleichen, dann verbessern
   (bessere Filter, Stereo, eigene Samples als Option).
3. **Sound-Paket anhören und nachbessern.** Seit 0.1.9 aus
   Freesound-Aufnahmen und Battle Zone II (Schuss; `tools/mkfreesound.py
   assets/freesound --bzoneii assets/bzoneii`, Schichten und Pegel im
   Skript), `sound_replace` in der Vorgabe an. Battle-Zone-II-Dateien nicht
   weitergeben (Lizenz ungeklärt). Weitere
   Aufnahmen: nur CC0, über den Chrome des Besitzers, je Download fragen;
   ein Download nach dem anderen (mehrere in einem Batch lehnt die
   Prüfung ab).
4. **Farben weiter abstimmen** nach Rückmeldung des Besitzers.
   Alles steht in `render/color.c`.
5. **Glättung verfeinern**, falls nötig: Beim Drehen werden die Endpunkte
   der Linien geradeaus verschoben, nicht neu projiziert. Bei 15 Frames
   pro Sekunde ist der Fehler klein. Sichtbar werden könnte er bei sehr
   nahen Objekten.
6. **Automat.** Das Spiel ist dort installiert (BUILD.md, Abschnitt
   Automat). Offen: Bild auf der Röhre ansehen und kalibrieren, prüfen ob
   es mit Gitter, Sternen und Spuren ruhig steht (rund 65.000 Einheiten
   gezeichnete Länge pro Bild bei 45 Hz), Steuerung prüfen. Paket:
   `tools/make_vpk.sh`, Hochladen in den Drive-Ordner "games" (BUILD.md).
