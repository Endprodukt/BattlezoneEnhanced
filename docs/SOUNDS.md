# Sounds

Alle 15 Sounds, die das Spiel macht. Jeder lässt sich einzeln durch eine
Aufnahme ersetzen: eine WAV-Datei mit dem Namen aus der Tabelle, abgelegt
in `sounds/` neben dem Programm oder in `save/sounds/`. Sounds ohne Datei
klingen wie bisher.

* Format: WAV, PCM, 8 oder 16 Bit, Mono oder Stereo, beliebige Rate.
* An und aus: `sound_replace` in `battlezone.ini` oder im Testmenü (Tab)
  unter SOUND (oder GAME OPTIONS), SOUNDS: ENHANCED oder ORIGINAL. Dort
  spielt SOUND TEST jede Aufnahme einzeln ab. ORIGINAL ist der Ton der
  Platine ohne Zutat: keine Aufnahmen, kein Ducking, keine lauteren
  Melodien (Stereo und Entfernung haben eigene Schalter).
* Technik: `src/sound/samples.[ch]`, eingehängt in `src/sound/sound.c`.

## Melodien und Signale des POKEY (8)

Das Spiel startet sie über `SNDON`. Es läuft immer nur eine: Eine neue
beendet die laufende. Die Länge ist die des Originals; eine Aufnahme darf
länger oder kürzer sein.

| Datei | Name im Quelltext | Was | Wann | Länge im Original |
|---|---|---|---|---|
| `RBEEP.wav` | RBEEP | Radar-Piep | jedes Mal, wenn der Radarstrahl den Gegner überstreicht | 0,07 s |
| `BOING.wav` | BOING | Anstoßen | der eigene Panzer fährt gegen ein Hindernis | 0,47 s |
| `BLKSND.wav` | BLKSND | "Motion blocked" | solange der Panzer am Hindernis hängt, immer wieder neu gestartet | 0,13 s |
| `BONER.wav` | BONER | Glocke | Bonus-Panzer erreicht | 0,91 s |
| `WARNG.wav` | WARNG | Warnton | "Enemy in range": ein Gegner taucht auf | 0,59 s |
| `DISINT.wav` | DISINT | Auflösen | Untertasse getroffen, solange sie zerfällt immer wieder neu gestartet; auch der Slam-Alarm | 0,10 s |
| `SAUSND.wav` | SAUSND | Untertasse | solange die Untertasse im Bild ist, immer wieder neu gestartet | 0,26 s |
| `SUPBON.wav` | SUPBON | Fanfare (Ouvertüre 1812) | neuer Highscore, und bei 100.000 Punkten | 2,54 s |

Die drei Sounds, die das Spiel "immer wieder neu startet", beginnt es
jedes Mal von vorn, sobald der vorige Durchlauf zu Ende ist. Eine Aufnahme
dafür sollte deshalb etwa so lang sein wie das Original oder sich nahtlos
wiederholen lassen.

## Rakete (1)

| Datei | Was | Wann |
|---|---|---|
| `MISSILE.wav` | das Summen der Rakete (POKEY-Kanäle 3 und 4) | solange die Rakete anfliegt. Die Datei läuft als Schleife, so laut wie das Original gerade ist |

## Schüsse und Explosionen (4)

Im Original Rauschen aus diskreten Schaltungen. Die Aufnahme startet in
dem Moment, in dem die Platine den Sound einschaltet.

| Datei | Was | Wann |
|---|---|---|
| `SHELL_LOUD.wav` | Schuss, laut | der Spieler feuert |
| `SHELL_SOFT.wav` | Schuss, leise | der Gegner feuert |
| `EXPLO_LOUD.wav` | Explosion, laut | der eigene Panzer wird getroffen (die Scheibe springt), Explosion bei der Fanfare |
| `EXPLO_SOFT.wav` | Explosion, leise | der Gegner wird getroffen, ein Schuss trifft ein Hindernis |

## Motor (2)

| Datei | Was | Wann |
|---|---|---|
| `ENGINE_IDLE.wav` | Motor im Leerlauf | Schleife, solange das Spiel läuft und der Panzer steht |
| `ENGINE_REV.wav` | Motor unter Last | Schleife, solange der Panzer fährt oder dreht |

Gibt es nur `ENGINE_IDLE.wav`, wird sie beim Fahren schneller abgespielt
(bis zum 1,6-Fachen). Gibt es beide, werden sie ineinander übergeblendet.
Der Übergang folgt der Schaltung des Originals: langsam hoch, schnell
wieder runter. Ohne `ENGINE_IDLE.wav` bleibt der Motor des Originals.

## Abgleich mit MAME

Der Ton des Ports ist ein Modell: POKEY und die Analogschaltungen
(Rauschen, Schuss, Explosion, Motor) in C nachgerechnet, nach MAMEs
Beschreibung der Platine (`src/sound/sound.c`, `src/sound/pokey.c`).
Geprüft wird es gegen MAME mit denselben Schreibzugriffen:

```bash
sh tools/soundcheck.sh
```

`tools/mame/bzsound.lua` spielt in MAME 75 Sekunden Spiel (Abschüsse,
Treffer, Raketen, alle acht Melodien, das Raketensummen) und schreibt jeden
Zugriff auf POKEY und Sound-Latch mit Zeit mit, MAME selbst den Ton
(`-wavwrite`). `bzsound --replay` spielt dieselben Zugriffe zur selben
Zeit auf dem Modell. `tools/soundcheck.py` schneidet aus beiden Aufnahmen
die Stellen, an denen ein Sound allein (mit dem Motor) klingt, und
vergleicht Pegel (relativ zum Motor im Leerlauf), stärkste Frequenz,
Helligkeit und für den POKEY das Spektrum in Bändern.

Stand nach dem Abgleich (Modell minus MAME, dB):

| Sound | Pegel | Tonhöhe |
|---|---|---|
| Motor Leerlauf | Bezug | 53 Hz gleich |
| Motor unter Last | -1,2 | 59 Hz gleich |
| Explosion laut / leise | 0,0 / -0,5 | gleich / 71 statt 58 Hz |
| POKEY-Melodien | -0,7, alle Bänder bis 12 kHz innerhalb 1,2 | 209 Hz gleich |
| Raketensummen | -1,1, alle Bänder innerhalb 1,2 | 105 Hz gleich |
| Schuss laut / leise | +4,2 / -2,2, siehe unten | |

Vor dem Abgleich fehlten den POKEY-Melodien 10 bis 15 dB in den Höhen
und dem Raketensummen fast 7 dB: Das Modell hatte die Kanäle linear
addiert und hinter einen festen Filter von 1 kHz gelegt. Der echte POKEY
zieht seinen Ausgang über ein Widerstandsnetz je Lautstärke-Bit herunter;
nichtlinear, und der Filter dahinter wird mit der Lautstärke offener.
Jetzt wird das Takt für Takt so gerechnet wie in MAME (`pokey_run_rc`).
Das ganze Modell ist außerdem 6 dB lauter als vorher, nahe an MAME (das
absichtlich bis in die Begrenzung übersteuert).

**Schuss, eine bewusste Abweichung.** In MAMEs Schaltung stellt das Bit
der Explosion (D1) die Lautstärke des Schusses ein, nicht das des Schusses
(D3). Das Originalprogramm nennt D3 aber `LOUDSH` und setzt es, wenn der
Spieler schießt, und löscht es beim Schuss des Gegners; die Liste der
Bits von Al Kossow in MAMEs Quelle sagt dasselbe. Der Port folgt dem
Programm: Der eigene Schuss ist laut, der des Gegners leise. Mit MAMEs
Verdrahtung (versuchsweise) liegt das Modell auch beim Schuss innerhalb
0,4 dB, die Schaltung selbst stimmt also.

## Die Sounds der Platine als Dateien

`sounds/` im Projektordner enthält alle 15 Sounds, wie das Soundmodell
des Ports sie erzeugt, unter den Namen aus den Tabellen oben (48 kHz,
16 Bit, mono). Erzeugt mit:

```bash
build/pc/bzsound.exe --roms D:/Mame/roms
```

`bzsound` spielt jeden Sound so ab, wie das Spiel ihn anfordert: die
Melodien über SNDON und MODSND, bis sie zu Ende sind; Schuss und
Explosion so lange geschaltet wie im Spiel (4 und 254 bzw. 111
Interrupts) und dann ausklingend; Raketensummen und Motor je zwei
Sekunden. Jede Datei ist auf 90 % Vollaussteuerung gebracht (der Sound
wird dafür ein zweites Mal lauter erzeugt, nicht nachträglich verstärkt).
`--raw` behält die Pegel der Platine zueinander; dann sind die Melodien
sehr leise (0,1 bis 1,7 % Aussteuerung), die Geräusche lauter (bis 31 %).

Die Dateien sind ein Ausgangspunkt zum Bearbeiten: Eine geänderte Datei
unter demselben Namen in `save/sounds/` ersetzt den Sound im Spiel.

Grenzen: Es ist das Modell des Ports, keine Aufnahme eines Automaten,
abgeglichen mit MAME (oben). Gehört hat sie noch niemand.

## Das mitgelieferte Paket (0.1.9)

`package/sounds/` (im Build `sounds/` neben dem Programm) enthält sieben
Sounds: die Schüsse aus Battle Zone II (wie es auf dem Automaten liegt,
`/persistent/games/bzoneii/sounds`), Explosionen und Panzermotoren aus
Aufnahmen von Freesound, CC0 (frei, ohne Namensnennung).

Battle Zone II: `player_fire.wav` ist der eigene Schuss (auf 2,6 s
gekürzt, leicht gesättigt, weil er im Mittel leise ist), der Schuss des
Gegners derselbe, dumpfer und leiser. Die Lizenz ist nicht geklärt: nur
für den eigenen Automaten, nicht weitergeben. Die Kopien liegen in
`assets/bzoneii/`. (0.1.9 nahm auch seinen Motor; dem Besitzer klang er
nicht nach Panzer.)

Die Motoren (0.1.10) sind Schleifen von 8 s aus Aufnahmen echter Panzer:
`ENGINE_IDLE` der stehende Panzer mit laufendem Motor, `ENGINE_REV` die
Fahrt mit hochgedrehtem Motor und rasselnden Ketten; das Spiel blendet
zwischen beiden nach dem Hochdrehen über. `ENEMY_ENGINE` ist der Motor der
Gegnerpanzer, ein anderer Motor, damit er nicht wie der eigene klingt.
Jede Schicht ist auf dieselbe Länge geschnitten, ihre Naht mit gleicher
Leistung überblendet, die Lautstärke über die Schleife ausgeglichen.

Die Explosionen sind aus mehreren Aufnahmen geschichtet: ein heller

Die Explosionen sind aus mehreren Aufnahmen geschichtet: ein heller
Anschlag, der auf dem Lautsprecher des Automaten durchkommt, darunter
Körper und Bass. Der tiefste Bass (unter etwa 35 bis 70 Hz) ist
weggefiltert, eine weiche Sättigung macht sie laut.

| Datei | Was | Länge | aus |
|---|---|---|---|
| `SHELL_LOUD.wav` | eigener Schuss | 2,6 s | Battle Zone II `player_fire.wav` |
| `SHELL_SOFT.wav` | Schuss des Gegners: derselbe, dumpfer und leiser | 2,6 s | wie oben |
| `ENGINE_IDLE.wav` | Motor, Stand (Schleife) | 8 s | 182793, 186934 |
| `ENGINE_REV.wav` | Motor und Ketten, Fahrt (Schleife) | 8 s | 329800, 186934, 849062, 121531 |
| `BOING.wav` | Aufprall auf ein Hindernis (0.1.15) | 0,8 s | 151624, 67617, 837389 |
| `BLKSND.wav` | am Hindernis festhängen: Knirschen, vom Spiel alle 0,138 s neu gestartet | 0,14 s | 336001, 697758, 67617 |
| `ENEMY_ENGINE.wav` | Motor der Gegnerpanzer (Schleife, kein Sound der Platine) | 8 s | 572294, 187676, 121531 |
| `EXPLO_SOFT.wav` | Gegner zerstört | 2,6 s | 394128, 490266, 413224, 814052 |
| `EXPLO_LOUD.wav` | eigener Panzer zerstört | 3,6 s | 873747, 394128, 490266, 814052 |

Die Aufnahmen (Freesound-Nummer, Datei, Urheber):

* 182793 Military Vehicle Humming (qubodup)
* 186934 engine4_loop.wav (ReadeOnly)
* 329800 t26_moving_01.wav (greatmganga), ein fahrender T-26
* 849062 tracks moving (christerljung)
* 121531 tracked vehicle.wav (cognito perceptu)
* 572294 tank_engine.wav (monosfera)
* 187676 Driving Tank Engine (qubodup)
* 151624 Clank Car Crash Collision (qubodup)
* 67617 Metal Crash Collision (qubodup)
* 837389 Heavy Robot Footsteps (Artem_Zaitsev), ein Schritt als Gewicht
* 336001 Metal Grinding Very Slow (Ev-Dawg)
* 697758 Metal Resonant Scrape Impact (SoundDesignForYou)
* geladen, nicht verwendet: 332058 Collision (qubodup), 512475 Metallic
  clunk (michael_grinnell), 569555 Foley_Impact_Metal_Long_Mono (Nox_Sound)
* 394128 Space Explosion.wav (morganpurkis)
* 490266 Sci-Fi_Explosion_2.wav (Anomaex)
* 413224 Energy impact 4 (ihitokage)
* 814052 Retro Bit Explosion (qubodup)
* 873747 Low_Mid_End_Void_Energy_Blast_Punch_01 (The_Elementus)
* für den ersten Schuss von 0.1.9, ersetzt durch den aus Battle Zone II
  (ohne `--bzoneii` baut das Skript ihn weiter): 702000 Heavy blaster shot
  05 (xkeril), 728519 Sci fi cannon shot (doudar41), 567308
  PlasmaCannon.wav (BigDino1995)
* geladen, nicht verwendet: 569903 ScifiCannon.wav (BigDino1995), 867558
  sci-fi weapon shot (dimapain); beide fast nur Tiefbass

Die Originale liegen in `assets/freesound/`. Neu bauen:
`python tools/mkfreesound.py assets/freesound --bzoneii assets/bzoneii`
(braucht numpy und soundfile). Rakete und die acht Melodien bleiben die
der Platine.
Eigene Dateien in `save/sounds/` haben Vorrang. `tools/deploy.sh` leert
`sounds/` auf dem Automaten vor dem Kopieren.

`sound_replace` ist in der Vorgabe an. Abschalten im Testmenü unter SOUND,
REPLACED SOUNDS. Die errechneten Sounds von `tools/mksounds.py` (auch Motor
und Rakete) gehören nicht mehr zum Paket; das Skript schreibt nach
`_scratch/synth`.

Pegel im Spiel (60 s am PC, `bzshot`): Motor etwa 1,5 bis 2 dB über dem
der Platine, der Schuss etwas leiser als die Explosionen, nichts
übersteuert.

## Motor der Gegner (0.1.10)

`enh_enemy_engine` (Testmenü SOUND, ENEMY ENGINES; nur mit
`sound_replace`): Solange ein Gegnerpanzer lebt (`COLFLG+2` ist 0, nicht
die Rakete: `R2D3FL` Bit 7), läuft `ENEMY_ENGINE.wav` als Schleife von
seiner Seite. Mit der Entfernung wird er deutlich leiser, stärker als
sein Schuss: 1,15 − Abstand/22000, mindestens 6 Prozent. Ändert sich
seine Position, fährt er: Der Motor dreht in einer halben Sekunde hoch
(schneller abgespielt), steht er, wieder herunter. Ein- und Ausblenden
dauern eine Viertelsekunde. Fehlt die Datei, nimmt das Spiel die eigenen
Motorschleifen, tiefer. Code: `bz_sound_place` und der Mischer in
`sound/sound.c`.

## Lautere Melodien (0.1.12)

Mit ENHANCED sind die Melodien des POKEY (Radar-Piep, Warnton, Glocke,
Fanfare, Untertasse) 11 dB lauter (`TUNE_BOOST` in `sound/sound.c`), das
Summen der Rakete nicht. Auf der Platine liegen sie 14 dB unter dem Motor
und sind trotzdem zu hören, weil der Motor tief brummt; Motor und Ketten
der Aufnahmen füllen die Mitten, in denen sie liegen, und sie gingen unter.
Gemessen am PC (60 s Spiel): die Melodien jetzt 4 dB über allem anderen
in denselben Momenten, vorher 6,5 dB darunter.

## Ducking (0.1.11)

`enh_ducking` (Testmenü SOUND): Solange eine Melodie des POKEY läuft
(Radar-Piep, Warnton, Glocke ...), ein Schuss oder eine Explosion zu
hören ist, werden die Motoren, der eigene und der des Gegners, auf 55
Prozent abgesenkt (5 dB): in 15 ms hinunter, in 0,3 s wieder hinauf. Grund:
Mit den Panzermotoren gingen die Radar-Sounds unter. Dazu sind die
Motorschleifen seit 0.1.11 3 dB leiser (`tools/mkfreesound.py`).

## Stereo

Mit `enh_stereo` kommen Schuss und Explosion des Gegners, das Summen der
Rakete und die beiden Sounds der Untertasse von der Seite, auf der die
Quelle steht. Mit `enh_distance` werden sie mit der Entfernung leiser, bis
auf 30 Prozent; die Explosion des Gegners weniger, bis auf 55 Prozent
(sie soll auch weit weg noch krachen). Der eigene Schuss, der eigene Treffer, Motor, Radar und
die übrigen Melodien bleiben in der Mitte. Das gilt für die Sounds der
Platine und für Aufnahmen gleich.

## Offene Punkte

* Laut und leise bei den Explosionen sind aus dem Quelltext abgeleitet,
  nicht am Automaten gehört.
* Aufnahmen sind mono. Stereo wird beim Laden zusammengemischt.
* Die Lautstärke der Aufnahmen gegenüber den Sounds der Platine ist
  geschätzt (halbe Vollaussteuerung). Im Spiel am PC liegen Schuss und
  Explosion etwa 5 dB über dem bisher lautesten Geräusch, ohne Übersteuern.
* Das Paket von 0.1.9 ist nach Messwerten gemischt, nicht angehört.
* Mit `--original` werden nur Schüsse, Explosionen, Motor und Rakete
  ersetzt, die acht Melodien nicht.
