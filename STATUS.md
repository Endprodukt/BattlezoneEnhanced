# Status

Stand: 2026-10-17, Version 0.1.20 (Credit ENHANCED BY ENDPRODUKT, Farben nur unter PICTURE)

## Ziel

Battlezone (Atari, 1980) vollständig und unverändert in C, geprüft gegen
MAME. Danach: sehr flüssige Darstellung, besserer Ton, Farbvektoren.

Der erste Teil ist erreicht: Das ganze Spielprogramm liegt in C vor und
stimmt in allen Aufnahmen mit MAME überein. Flüssige Darstellung, Farbe,
gespeicherte Highscores, drei Eingabearten mit freier Belegung und das
Testmenü sind dazugekommen. Der Ton steht noch aus.

## Was läuft

Das Spiel als C-Programm (`src/game`, 5.000 Zeilen), Routine für Routine
nach dem Quelltext von 1980/81:

* Hauptschleife, Fensteraufbau, Berge, Mond und Vulkan mit Steinen
* Auswahl der sichtbaren Objekte, Projektion über die Math Box, Zeichnen
  aller Objekte (Panzer, Super-Panzer, Rakete, Untertasse, Hindernisse,
  Schüsse, Trümmer, Explosionen, Logo)
* Spieler: beide Hebel, Feuer, Anstoßen an Hindernisse, Zurückprallen
* Gegner: Panzer mit Zielen, Ausweichen, Angriff, Rückzug nach Kollision;
  Super-Panzer; Rakete mit Anflug in Schlangenlinie; Untertasse
* Radar, Fadenkreuz, Meldungen in vier Sprachen, Punkte, Bonus-Panzer,
  Super-Bonus bei 100.000
* gesprungene Scheibe, Spielende, Highscore-Seite, Eingabe der Initialen
* Interrupt: Münzen mit allen Münzmodi und Bonus-Münzen, Credits, Start,
  Sound-Ablauf (POKEY-Melodien, Sound-Latch), Wechsel der Displaypuffer
* Einschaltzustand (PWRON, soweit er ins Spiel führt)

Im normalen Betrieb läuft kein 6502.

Als Bausteine nachgebildet: Math Box, Vektorgenerator mit dem
Fenster-Schaltkreis der Battlezone-Platine, POKEY, die diskreten Sounds.

`battlezone --original` lässt zum Vergleich das Originalprogramm auf einem
eigenen 6502-Kern laufen.

## Was der Port hinzufügt

Die Spielmechanik ist unverändert. Alle Vergleiche gegen MAME sind nach
diesen Ergänzungen weiter vollständig gleich.

* **Flüssiges Bild.** Nach jedem Frame wird das Bild des nächsten
  vorausgezeichnet, dazwischen wandern die Linien der Welt. Am Fenster
  60 Bilder pro Sekunde, abschaltbar (`smooth`).
* **Farbe.** Zwei Paletten. Vorgabe ist die nach Ataris Flyer: Berge
  eisweiß über blauem Horizont und blauem Gitter, Mond orange,
  Hindernisse blau und violett, Super-Panzer rot, Rakete gelb, Visier,
  Punkte und Logo gelb und orange, Warnungen rot, Tabelle als Verlauf von
  gelb nach rot. Die erste Palette (Land grün) bleibt wählbar, ebenso
  Farbfolie und Weiß. Standard-Panzer und Lebensanzeige sind in beiden
  Paletten grün wie am Automaten. Bilder in `docs/bilder`.
* **Massive Objekte.** Rückseiten und alles hinter einem Objekt werden
  nicht gezeichnet. Abschaltbar (`occlusion`).
* **Bodengitter** in der Perspektive des Spiels, von Objekten verdeckt.
  Abschaltbar (`ground_grid`).
* **Sound-Ersatz.** 15 Sounds, jeder einzeln durch eine WAV-Datei
  ersetzbar, abschaltbar (`sound_replace`). Liste in `docs/SOUNDS.md`.
  Aufnahmen liegen noch keine bei.
* **Highscores** in `save/hiscore.bin`, alle zehn Zeilen.
* **Eingabe.** Ein Joystick, zwei Joysticks, analoges Yoke mit stufenloser
  Geschwindigkeit. Jede Aktion frei auf Taste, Knopf, Achse oder Hut.
* **Testmenü auf Tab**: Steuerung, Bild, Ton mit Sound-Test,
  Spieloptionen, Highscores zurücksetzen, Kalibrierbild.
* **Enhanced-Optionen**, jede einzeln schaltbar (Tabelle im README):
  Explosionsglühen, Entfernungsdunst, Sterne, Vulkan-Glühen,
  Mündungsblitz und Staub, Kettenspuren, feineres Gitter, erweitertes
  Radar, Pfeil zum Gegner, Hebelanzeige, Stereo nach Richtung, leiser mit
  Entfernung, Motor nach Hebelweg, Tempo des Automaten, Menü über Start
  und Feuer oder eigenen Knopf, START-Lampe.
* **Sound-Paket** für Schüsse, Explosionen, Motor und Rakete, errechnet,
  in der Vorgabe aus.

## Geprüft

Gegen MAME 0.289, Satz `bzonea`, 16 Aufnahmen (`tools/captures.sh`):
Attract-Modus, gespielte Partien mit Abschüssen und Toden, Rakete (auch von
hinten und außer Reichweite), Super-Panzer, Untertasse, Bonus-Panzer bei
15.000 und 50.000, Super-Bonus, Spielende mit Highscore-Eingabe, Deutsch,
Französisch, Spanisch, Freispiel, 2 Spiele je Münze mit Bonus-Münzen,
2 Münzen je Spiel.

| Prüfung | Umfang | Ergebnis |
|---|---|---|
| C-Routinen, Frame für Frame (`bzcompare`) | 70.120 Frames | alle gleich |
| Originalprogramm auf dem 6502-Kern (`bzcompare --emu`) | 70.120 Frames | alle gleich |
| Interrupt-Routine in C gegen das Original auf dem Kern (`bznmi`) | 1.124.000 Interrupts, darunter Münzen, Slam, Spielstarts | alle gleich |
| Einschaltzustand in C gegen das Original (`bznmi`) | RAM, Vektor-RAM, POKEY-Register | gleich |
| `test_core` (ohne ROMs) | 6502-Kern, Math Box, Vektorgenerator, Makros, Highscore-Datei | bestanden |
| Vorausgezeichnetes Bild gegen das nächste Bild des Spiels (`bzshot`) | 448 Frames mit Drehen und Fahren | 0,23 Einheiten Abstand je Linie im Mittel, 0,14 % ohne Partner |
| Gleichmäßigkeit beim Drehen auf 60 Hz (`bzshot`) | 1.325 Bilder | geglättet 1,6 % Stillstand und 1,2 % Sprünge, ungeglättet 74,5 % und 25,5 % |
| Analoger Modus an den Anschlägen gegen die Hebelschalter (`tools/checkanalog.sh`) | alle acht Bewegungen | gleiche Position und Richtung |
| Verdeckung und Bodengitter am Bild (`bzshot`, `docs/bilder`) | Pyramiden, Kisten, Panzer nah und fern, mit `--nosolid` zum Vergleich | Rückseiten, Berge und Gitter hinter Objekten fehlen, Vorderkanten bleiben |
| Sound-Ersatz mit Testtönen (`bzshot --sounds`) | 60 s Spiel, 8 ersetzte Sounds | Radar-Piep 38-mal, Schuss 5-mal, Explosionen 3-mal, Warnton 4-mal, Anstoßen 3-mal gestartet |
| Enhanced-Optionen am Bild (`bzshot`, `docs/bilder`) | Sterne, Radarpunkte, Pfeil, Mündungsblitz, Spuren, Hebelanzeige, Vulkan | im Bild zu sehen und an der richtigen Stelle |
| Tempo des Automaten (`bzshot --pace`) gegen MAME | 150 s Attract, 150 s Spiel | 14,62 gegen 14,69 und 14,77 gegen 14,8 bis 15,0 Frames pro Sekunde |
| Stereo (`bzshot --wav`) | 60 s Spiel | zwei Kanäle, links und rechts unterscheiden sich in 13 % der Samples |
| Fenster und Testmenü über Tastenskript und Bildschirmfoto | Menüseiten, Wechsel der Eingabeart, Taste belegen, Highscores zurücksetzen und beim Start laden | funktioniert |

Verglichen wird nach jedem Frame jedes Byte von RAM und Vektor-RAM und
alle Register der Math Box. Auch jede Zufallszahl und jeder Lesezugriff
auf die Schalter muss an derselben Stelle kommen.

Abdeckung der C-Routinen durch die Aufnahmen (`tools/coverage.sh`):
`play.c` 99,8 %, `view.c` 99,7 %, `mainline.c` 98,3 %, `nmi.c` 100 %,
`sound.c` 100 %, `coin.c` 98,7 %, `vgut.c` 90,5 %. Nicht durchlaufen werden
nur `bz_main_poll` (die Warteschleife des Live-Betriebs), der unbenutzte
Zeichenbefehl `D.SLIM` und drei Routinen aus VGUT, die Battlezone nie
aufruft (`VGRTSL`, `VGJMPL`, `VGSTA1`).

Am Bild geprüft: `bzshot` schreibt die Displayliste als SVG. Attract-Bild
und Spielbild sehen aus wie MAMEs Bildschirmfoto (Ausschnitt, Horizont,
Radar, Fadenkreuz, Texte).

## Noch von niemandem gesehen oder gehört

* **Das Spielfenster in Bewegung.** Standbilder des Fensters und des Menüs
  sind geprüft, die Gleichmäßigkeit ist gemessen. Gespielt hat es noch
  niemand.
* **Joysticks und Yoke.** Belegen und Lesen von Knöpfen, Achsen und Hüten
  ist geschrieben, aber nur mit der Tastatur ausprobiert. Der analoge
  Modus ist nur über Skript gefahren.
* **Eintrag in die Highscore-Tabelle im Fenster.** Laden, Speichern und
  Zurücksetzen sind geprüft. Dass nach der Eingabe von Initialen
  gespeichert wird, ist nur aus dem Code geschlossen.
* **Explosionsglühen, Staub und Dunst.** Im Code und im Farbweg
  eingebaut, aber in keinem der geprüften Bilder gezeigt, weil das
  Testskript keinen Abschuss schafft.
* **Stereo, Entfernung, Motor nach Hebelweg, Sound-Paket.** Nur gemessen,
  nie gehört. Ob links und rechts richtig herum sind, ist aus der
  Projektion abgeleitet.
* **Kalibrierbild.** Es wird gezeigt. Das Verstellen von Größe, Lage und
  Helligkeit ist ungeprüft.
* **Menü über Start und Feuer, eigener Menüknopf, START-Lampe.** Ungeprüft.
* **Der Sound-Ersatz.** Die Aufnahmen starten nachweislich an den
  richtigen Stellen. Gehört hat das Ergebnis niemand, auch nicht die
  Motor-Schleife mit ihrer Tonhöhe und den Sound-Test im Menü.
* **Verdeckung und Gitter in Bewegung.** An Standbildern geprüft. Ob
  Kanten beim Drehen flackern, ist ungesehen.
* **Der Ton.** Gegen MAME abgeglichen (docs/SOUNDS.md), aber von
  niemandem am Automaten gehört.
* **Das Spiel am Automaten.** Es ist dort installiert und läuft: ROMs
  gefunden, USB-DVG, 45 Bilder pro Sekunde, rund 400 Linien, kein Fehler
  im Protokoll. Gesehen hat das Bild auf der Röhre noch niemand. Ob es mit
  Gitter, Sternen und Spuren ruhig steht, ob Lage und Größe passen und ob
  die Steuerung des Automaten wie erwartet belegt ist, ist offen.

## Bekannte Abweichungen vom Automaten

* **Tempo.** Der Automat rechnet 14,7 bis 15,1 Frames pro Sekunde und
  wird in vollen Szenen langsamer, weil sein 6502 für einen Frame bis zu
  102.000 Zyklen braucht (16 Interrupts sind 98.304). Die C-Routinen
  brauchen keine Zeit: Sie laufen immer mit 15,3 Frames pro Sekunde, also
  1 bis 4 % schneller und ohne Schwanken. Wer das Tempo des Automaten
  exakt will, braucht ein Zeitmodell wie `pace.c` im Star-Wars-Port.
  Offen ist, was gewünscht ist.
* **Revision.** Das Verhalten ist immer Revision 1 (`bzonea`), auch mit
  dem ROM-Satz `bzone`. Revision 2 ändert die Highscore-Seite.
* **Zufallszahlen.** Im Live-Betrieb kommen sie aus einem eigenen
  Schieberegister, nicht aus einem zyklusgenauen POKEY. Die Folge ist eine
  andere als in MAME, das Verhalten dasselbe.
* **Math Box.** Nachgebildet auf Routinen-Ebene wie in MAME, nicht als
  Mikrocode. Sie ist sofort fertig. Der Mikrocode-Quelltext (MBUCOD.V05)
  und die PROMs liegen vor, eine Nachbildung auf Mikrocode-Ebene wäre
  möglich.
* **Vektorgenerator.** Die Zeit, die er für ein Bild braucht (Halt-Flag),
  ist nur angenähert. Das Spiel selbst fragt sie nicht ab.

## Nicht portiert

* Der Selbsttest der Platine (BZSTST.MAC ab `RAMTST`: RAM-, ROM- und
  Math-Box-Test, Schaltertest, Diagnosebilder) und das Diagnoseprogramm
  BZSADG.MAC. Sie prüfen Hardware, die es hier nicht gibt. Mit
  `--original` laufen sie auf dem 6502-Kern (Testschalter: IN0 Bit 4, im
  Programm noch nicht auf eine Taste gelegt).
* Die Schutzmechanismen gegen Abstürze (Stack-Prüfung und Sync-Prüfung im
  Interrupt, Watchdog) haben hier nichts zu schützen und fehlen.
* Die Kopierschutz-Prüfung (`TRAP1`, `TRAP2`: Prüfsumme über den
  Copyright-Text in der Displayliste) ist portiert und rechnet wie im
  Original.

## Am Automaten gemeldet und behoben (0.1.1)

* **Spielbild schmaler als das Justagebild.** Der gezeigte Ausschnitt war
  der von MAME (580 mal 400 Einheiten, mit schwarzem Rand um das Spiel).
  Jetzt ist es das Fenster des Spiels selbst, 512 mal 384, genau 4:3.
  Justagebild und Spielbild sind damit gleich breit.
* **Bewegung hakelig, Objekte halten kurz an und fahren weiter.** Ursache
  war nicht der Zeitplan, sondern das Spiel: Es rundet die Bildposition
  jedes Objektpunkts auf ein Raster von einer halben Einheit ab. Ein fernes
  Objekt steht deshalb mehrere Frames still und springt dann. Die
  Zwischenbilder haben dieses Anhalten nur weich nachgezeichnet. Jetzt
  werden die Punkte ungerundet genommen, aus dem, was die Math Box
  dividiert (`bz_avg_exact`). In der Simulation bewegt sich ein Objekt
  seither gleichmäßig. Zusätzlich beginnt jedes Zwischenbild exakt eine
  Periode nach dem vorigen.
* **Der Boden baut sich vor einem auf.** Das Gitter hatte fünf
  Helligkeitsstufen, und die feineren Linien erschienen auf halber
  Strecke schlagartig. Jetzt kommen alle Linien stufenlos aus dem Dunkel
  und werden zum Spieler hin heller.

Am Automaten gemessen nach dem Update (`run.log`, Zeile "battlezone
pace"): Bilder 21,8 bis 23,1 ms auseinander, 15 bis 16 Frames des Spiels
pro Sekunde, kein Bild musste auf seinen Frame warten. Gesehen hat das
Ergebnis noch niemand.

## Bekannte Grenzen der Ergänzungen

* **Verdeckung.** Flächen entstehen nur aus Ringen von höchstens acht
  Linien, die in einer Ebene liegen. Das Logo hat keine solchen Flächen
  und verdeckt nichts. Ragt ein Objekt aus dem Bild und fehlt ein Punkt
  einer Fläche, verdeckt diese Fläche nichts.
* **Bodengitter.** Es wippt beim Anstoßen nicht mit. Es verdoppelt etwa
  die gezeichnete Linienlänge, was auf der Vektorplatine zählt.
* **Sound-Ersatz.** Lautstärke der Aufnahmen gegenüber der Platine
  geschätzt. Mit `--original` werden die acht Melodien nicht ersetzt.

* **Zwischenbilder.** Linien werden zwischen zwei Bildern geradeaus
  verschoben, nicht neu projiziert. Gemessen ist der Fehler klein. Sehr
  nahe Objekte beim schnellen Drehen könnten leicht verzerrt wirken.
* **Zwischenbilder nur mit den C-Routinen.** `--original` zeigt die 15
  Frames des Originals, in Farbfolie statt Farbe.
* **Analog mit `--original`.** Das Originalprogramm kennt nur Schalter.
  Das Yoke gilt dort ab halbem Weg als gedrückter Hebel.
* **Tempo des Automaten.** Ein Modell, kein Mitlauf des Originals. Es
  trifft MAMEs Bildrate auf etwa 1 Prozent. Die Wartezeit eines Frames ist
  die Dauer des vorigen, der Fehler wird im nächsten ausgeglichen.
* **Kettenspuren und Sterne** kosten auf der Vektorplatine Linien. Dort
  prüfen, ob das Bild ruhig bleibt.
* **START-Lampe** nur am Automaten, weil sie am PC die NUM-Taste der
  Tastatur schalten würde.
* **Esc beendet das Programm**, auch im Menü. Nur beim Belegen bricht Esc
  die Belegung ab.

## Offene Punkte

1. Spielen, Ton anhören, Joysticks und Yoke ausprobieren (siehe oben).
2. Farben weiter abstimmen.
3. Neue Sounds (0.1.9, 0.1.10: Schuss, Explosionen, Motoren) am Automaten anhören und nachbessern.
4. Ton verbessern.
5. Menüknopf, Start-und-Feuer-Halten und START-Lampe am Automaten
   ausprobieren.
7. Namen der Hebel-Aktionen (`p1_doubleleft_up` usw.) gegen eine echte
   `advmame.rc` prüfen.
8. Am Automaten: Bild ansehen, kalibrieren, Steuerung prüfen. Paket:
   `tools/make_vpk.sh`; 0.1.20 liegt im Drive-Ordner "games".
9. Kein Git-Repository angelegt, nichts committet.
10. Aufräumen: `_scratch/shots/a` und `_scratch/shots/p` enthalten alte
    Testbilder. Das Löschen wurde von der Sicherheitsprüfung abgelehnt.

## Nächste Schritte

Siehe [RESUME.md](RESUME.md), Abschnitt "Nächste Schritte".

## Paket 0.1.20 im Drive (2026-10-17)

`tools/make_vpk.sh` (nach dem von Red Baron Enhanced) macht
`build/bzenhanced-0.1.20.vpk` (4,5 MB: Pi-Programm, game.ini,
battlezone.ini, Marquee, neun Sounds, `sounds/CREDITS.txt`; keine ROMs;
Programm und Ordner 755, sonst 644). Hochgeladen in den geteilten
Google-Drive-Ordner "games" (neben rbenhanced, swenhanced). Auf Wunsch
des Besitzers mit dem Schuss aus Battle Zone II, obwohl dessen Lizenz
unbekannt und der Ordner öffentlich ist (in CREDITS.txt vermerkt). Mit
INSTALL GAMES installiert hat es noch niemand.

## Farben nur unter PICTURE, Credit (0.1.20, installiert 2026-10-17)

COLORS steht nicht mehr auch auf GAME OPTIONS, nur noch unter PICTURE
(SOUNDS bleibt auf beiden). Unter dem "(C)(P) ATARI 1980" des
Attract-Modus steht "ENHANCED BY ENDPRODUKT", in derselben Farbe, etwas
kleiner (`main.c`, nach `bz_draw_lines`). Bild:
`docs/bilder/credits-endprodukt.png`.

## Kein Dunst, Gitter ohne Stufen, Sterne mit dem Mond (0.1.19, installiert 2026-10-16)

Dunst in der Ferne (`enh_haze`) ganz entfernt, auf Wunsch ("das ist
scheiße"); die Einstellung wird überlesen.

Gitter "lebt vor der Nase": Es wurde in acht festen Helligkeitsstufen
abgeblendet, deren Grenzen in fester Entfernung zum Panzer liegen. Eine
Querlinie, die beim Fahren näher kommt, wurde an jeder Grenze eine Stufe
heller. Jetzt hat jedes Stück die Helligkeit seiner eigenen Mitte
(`lit_at` in `render/ground.c`), stufenlos. (Geprüft und ausgeschlossen:
das Abschneiden bei 700 Einheiten liegt unter dem unteren Bildrand.)

Sterne: Sie liefen mit echter Perspektive (0,049 Einheiten je
Winkelschritt), Mond und Berge des Spiels aber mit 0,031; die Sterne
zogen 57 % schneller am Mond vorbei. Logisch drehen alle unendlich fernen
Dinge gleich schnell: Die Sterne laufen jetzt mit dem Mond (`MOUNT_RATE`),
gemessen 1,99 Einheiten je Bild bei beiden. 100 statt 150 Sterne, gleich
viele zu sehen. Die hintere Bergreihe bleibt bewusst halb so schnell
(Parallaxe, 0.1.16).

## Belegung auf NONE (0.1.18, deployt 2026-10-16)

Gewünscht: eine Eingabe auch auf NONE setzen können. Links/rechts auf
einer Belegungszeile: eigene Belegungen zurück auf AUTO, AUTO auf NONE,
NONE wieder auf AUTO. Eine neue Taste auf NONE ersetzt es. Am PC geprüft
(bind_up = none; FIRE none, dann X: "key X").

## Bis zu acht Belegungen (0.1.17, deployt 2026-10-16)

Gemeldet: "Alt left wird als Taste nicht erkannt." Ursache: Feuer hatte
auf dem Automaten schon vier Belegungen (auto, Left Shift, Space, Left
Ctrl), die Grenze war vier, eine neue ersetzte dann die letzte. Die
advmame.rc des Automaten legt Battlezone-Feuer auf vier Tasten (Shift,
Space, Ctrl, Alt). Jetzt bis zu acht (`BZ_BIND_ALTS`, `BZ_BIND_SIZE` 320).
Am PC geprüft: Left Alt lässt sich belegen und wird gespeichert.

## Gitter weiter, hintere Bergreihe mit Parallaxe (0.1.16, deployt 2026-10-16)

Gewünscht: Das Bodengitter weiter zeichnen (es flackert nicht), und die
zweite Bergreihe soll sich parallax zur vorderen bewegen. Gitter: bis
24000 statt 15000 Einheiten (`FAR` in `render/ground.c`); 30000 kostete
die Hälfte mehr gezeichnete Länge und war im Bild kaum anders. Am
Automaten (Attract-Modus): gezeichnet ~85000 statt ~70000 Einheiten je
Bild, mit Dunkelweg bis ~169000 in der Spitze, 50,0 Bilder/s, kein Bild
stehen geblieben. Grenze von früher: um 170000 stockte es; im vollen
Gefecht beobachten, sonst `FAR` zurück auf etwa 20000.

Parallaxe: Die hintere Reihe lief mit 0,029 Schirmeinheiten je
Winkelschritt fast so schnell wie die vordere des Spiels (0,031: 0,996
alle 32 Schritte), also ohne sichtbare Parallaxe. Jetzt halb so schnell
wie die vordere (`PARALLAX` in `render/enhance.c`), die Ecken weniger
(52 statt 96), damit die Silhouette gleich breit bleibt.

## Neues Marquee (2026-10-16)

Vom Besitzer: "Battlezone Enhanced"-Marquee (Flyer-Motiv mit Schriftzug
ENHANCED), 2000 × 491, als `package/marquee.png` übernommen (vorher 1920 ×
360, Kopie in `_scratch/marquee-alt/`). Das SDK passt es an
(`vector_aux_marquee_display_mode` in advmame.rc). Auf dem Automaten.

## Neuer Sound gegen Hindernisse (0.1.15, deployt 2026-10-16, noch nicht gehört)

Gemeldet: Gegen Hindernisse fahren klang noch original. Das sind zwei
Melodien des POKEY: BOING beim Aufprall, BLKSND solange der Panzer
festhängt (das Interrupt startet es neu, sobald der Original-Ton nach 34
Interrupts endet; der Ersatz läuft also im Takt des Originals, und BOING
wird beim Weiterdrücken nach 0,47 s abgeschnitten). Jetzt: BOING ein
schwerer Metall-Crash, BLKSND 0,138 s Knirschen mit kleinem Klack, das
wiederholt nach rutschenden Ketten rattert (`tools/mkfreesound.py`,
`obstacles`; Aufnahmen in `docs/SOUNDS.md`). Hörprobe:
`_scratch/hoerprobe3/gegen-hindernis.wav`.

## Highscores im Menü und auf dem Marquee (0.1.14, installiert 2026-10-16)

Gemeldet: Das Marquee zeigt bei Vectortron Highscores, bei uns nicht.
Grund: `hiscores = no` in `game.ini`, und unsere Tabelle lag nur als
`save/hiscore.bin` vor. Das Menü des Automaten liest `save/highscores.txt`
(`NAME PUNKTE`, bis zehn Zeilen, vp_hiscore.h). Jetzt schreibt das Spiel
diese Datei beim Start, bei jedem neuen Eintrag und beim Zurücksetzen
(`bz_hiscore_save_text`; Initialen aus dem Alphabet des Vektor-ROMs:
16..48 A..Z, 02..14 Ziffern, 00 und 4A Leerzeichen; Punkte BCD in
Tausendern), und `game.ini` sagt `hiscores = yes`. Am PC geprüft
(Einschalttabelle EDR, MPH, JED ... je 5000). Auf dem Automaten lief Time
Pilot 3D: installiert, nicht gestartet; die Datei einmal aus der dortigen
`hiscore.bin` erzeugt (AAA 20000 an der Spitze). Ob das Marquee sie
zeigt, muss der Besitzer sehen.

Auch `hiscores = no`: Star Wars Enhanced, Red Baron Enhanced, Tempest
Enhanced (andere Projekte).

## Alle Steuerungen zugleich, Mehrfachbelegung (0.1.13, deployt 2026-10-16, am Automaten nicht ausprobiert)

Wunsch: keine Wahl mehr zwischen einem Joystick, zwei Joysticks und Yoke,
sondern alle zugleich, damit man umstecken kann; und Feuer (wie in MAME)
auf mehrere Knöpfe. Jetzt: Die Controls-Seite zeigt alle Belegungen
(JOYSTICK UP ..., LEFT/RIGHT JOYSTICK UP/DOWN, YOKE FORWARD/RIGHT). Vorrang
in jedem Bild: Hebel der zwei Joysticks, dann Yoke außerhalb der
Totzone, dann der eine Joystick (`platform/controls.h`). Jede Aktion hat
bis zu acht Belegungen (seit 0.1.17; vorher vier), verbunden mit " or "; im Menü kommt eine neue
dazu, links/rechts setzt auf AUTO zurück (`bz_config_bind_add`). Die
Einstellung `controls` wird überlesen. Die Pfeiltasten sind nicht mehr
die Enden des Yokes (sie gehören dem einen Joystick, am Automaten dem
linken Hebel). Am PC geprüft: Seite passt auf den Schirm, Feuer auf
"auto or key X or key 5" gelegt und gespeichert; MAME-Vergleich und
Analog-Prüfung gleich.

Offen: am Automaten mit Hebeln und Yoke ausprobieren. Vorsicht bei sehr
kleiner Totzone (dort `deadzone = 0.02`): zittert der Yoke in Ruhe über
die Totzone, gilt er als benutzt, solange kein Hebel gedrückt ist
(harmlos, solange der Ausschlag klein bleibt).

## Pieptöne lauter, Original wählbar (0.1.12, installiert 2026-10-16, noch nicht gehört)

Rückmeldung zu 0.1.11: Die Original-Pieptöne gehen unter. Mit ENHANCED
sind die Melodien des POKEY jetzt 11 dB lauter (`docs/SOUNDS.md`). Dazu
wollte der Besitzer Originalton und Originalfarbe (alles grün, oben rot)
in den Optionen: Beides gab es (`sound_replace`, Farbe OVERLAY), jetzt
verständlich benannt und auch auf GAME OPTIONS: SOUNDS ENHANCED/ORIGINAL,
COLORS FLYER/CLASSIC/ORIGINAL/WHITE. ORIGINAL beim Ton heißt auch: kein
Ducking, keine lauteren Melodien. Bild: `docs/bilder/farbe-original-und-flyer.png`.
Installiert, nicht gestartet (auf dem Automaten lief Visual Pinball).

## Motoren leiser, Ducking (0.1.11, deployt 2026-10-16, noch nicht gehört)

Rückmeldung zu 0.1.10: "Sound ist gut, könnte aber etwas leiser. Die
Radar-Sounds gehen jetzt unter." Die Motorschleifen sind 3 dB leiser, und
neu ist `enh_ducking` (an): Motoren 5 dB leiser, solange ein Effekt zu
hören ist (`docs/SOUNDS.md`, Abschnitt Ducking). Am PC: Grundpegel 3,7 dB
tiefer, Effekte gleich laut, nichts übersteuert.

## Panzermotor mit Ketten, Motoren der Gegner (0.1.10, deployt 2026-10-16, noch nicht gehört)

Rückmeldung zu 0.1.9: Der Motor (aus Battle Zone II) "klingt immer noch
kacke", gewünscht ein echter Panzer mit Ketten; die Gegnerpanzer sollen
auch Geräusche machen. Jetzt: drei Schleifen aus Freesound-Aufnahmen
echter Panzer (CC0): Stand, Fahrt mit Ketten, Gegner. Neu
`enh_enemy_engine` (an): der Motor des Gegnerpanzers von seiner Seite,
mit der Entfernung deutlich leiser, hochgedreht solange er fährt
(`docs/SOUNDS.md`, Abschnitt Motor der Gegner). Am Automaten: 50
Bilder/s, sieben Sounds geladen. Ob Lautstärke und Klang passen, muss der
Besitzer hören.

## Mehr Farbe, neue Sounds (0.1.9, deployt 2026-10-16, noch nicht gesehen und gehört)

Wunsch: weniger blaulastig, mehr Farbe in den Objekten, im Schema des
Flyers (blau, lila, futuristisch), der Boden bleibt blau. Palette "flyer":
die vier Hindernisformen je ein eigener Ton (Pyramide 0x00 cyan, 0x0c
blau, Würfel 0x0f violett, 0x01 magenta), beide Bergreihen lila statt
weißblau, Untertasse kräftiger magenta, eigener Schuss cyanweiß. Bild:
`docs/bilder/farbe-mehr-lila.png`.

Neue Sounds: Explosion des Gegners und eigener Tod aus
Freesound-Aufnahmen (CC0), geschichtet. Schuss und Motor auf Wunsch des
Besitzers aus Battle Zone II, wie es auf dem Automaten liegt ("der ist
geil"); der Schuss des Gegners ist derselbe, dumpfer und leiser. Der
Motor blendet zwischen den beiden Schleifen von Battle Zone II nach dem
Hochdrehen über. Lizenz der Battle-Zone-II-Dateien ungeklärt: nur für den
eigenen Automaten. `tools/mkfreesound.py assets/freesound --bzoneii
assets/bzoneii`, Liste in `docs/SOUNDS.md`. `sound_replace` ist jetzt in der Vorgabe an; in der
gespeicherten Einstellung des Automaten stand 0, dort auf 1 gesetzt. Das
alte errechnete Paket (auch Motor und Rakete) gehört nicht mehr dazu.

Die Explosion des getroffenen Gegners wird mit der Entfernung leiser,
aber weniger als seine anderen Geräusche: 1,1 − Abstand/60000, mindestens
0,55 (`enemy_boom` in `sound.c`; vorher dieselbe Kurve wie sein Schuss,
bis 0,3). Nur mit `enh_distance`.

Am Automaten: läuft mit 50 Bildern/s, lädt die sechs Sounds. Ob Schuss und
Explosion so gefallen, und ob Texte und GAME OVER (0.1.8) jetzt ruhig
stehen, muss der Besitzer sehen und hören.

## Texte und Anzeigen zuerst (0.1.8, am Automaten noch nicht gesehen)

Gemeldet mit 0.1.7: Der Bereich oben rechts (Punktestand, Leben) und GAME
OVER wackeln noch. Vermutete Ursache: In der Reihenfolge des Spiels kommen
diese Texte nach der Welt; der Strahl erreicht sie vom Ende der letzten,
sich bewegenden Linie, jedes Bild von woanders, und auf der Röhre landen
sie dadurch leicht verschieden. Jetzt werden Texte, Punktestand, Leben,
Radarring und Visier zuerst gezeichnet, dann Radarstrahl und Blip, dann
die Welt, jeder Teil in der Reihenfolge des Spiels (`bz_order_steady`,
`sm.order = 2`). So wird der Text in jedem Bild auf demselben Weg
erreicht. Gebaut, nicht deployt: der Automat war aus. Nächster Schritt:
`sh tools/deploy.sh`, dann am Automaten prüfen. Falls es weiter wackelt:
zwischen Texten und Welt einen festen Halt einfügen (CNTR, also den
Strahl in die Mitte), oder die Slew- und Jump-Rate der Platine prüfen.

## Fester, kürzerer Strahlweg für die Zusätze (0.1.7)

Die Linien des Spiels bleiben in ihrer Reihenfolge (die Sortierung bleibt
aus, siehe 0.1.6). Nur die Zusätze werden so erzeugt, dass der Strahl
kurze Wege hat und die Reihenfolge in jedem Bild dieselbe ist: Gitterlinien
von nah nach fern und jede zweite zurück, Sterne von links nach rechts und
zuletzt, das Gitter vor den übrigen Zusätzen. In `bzshot` fällt der dunkle
Weg damit von 10.800 auf 6.300 Einheiten je Bild (Bildschirmeinheiten).
Sollte die Sortierung je wieder eingeschaltet werden, behalten Texte,
Punktestand, Leben, Radar und Visier ihre Reihenfolge und Richtung
(`movable` in `bz_order_lines`): Das war das Flackern rechts oben.

Die zwischenzeitlich gemessenen 27 bis 37 Bilder pro Sekunde kamen nicht
vom Port: `deploy.sh` hatte ihn gestartet, während ein anderes Spiel die
Platine hielt, und der Besitzer hatte Slew- und Jump-Rate der Platine
verstellt. `deploy.sh` startet jetzt nicht mehr, wenn ein anderes Programm
die Platine hat, und installiert nur. Sauber gestartet am Automaten: 50
Bilder pro Sekunde, die Platine braucht 0,8 bis 1,0 ms je Bild, dunkler Weg
54.000 bis 62.000 Einheiten (0.1.4: 89.000 bis 100.000), kein Bild wartet. Ton: das Protokoll zählt jetzt fehlende
und verworfene Samples und den lautesten Wert ("sound: ..." in jeder
pace-Zeile).

## Sortierung wieder aus (0.1.6)

Am Automaten gemeldet: mit 0.1.5 wobbelte das Bild, vorher waren die
Vektoren ruhig. Die Sortierung nach dem nächsten Strich (`bz_order_lines`)
ändert die Reihenfolge von Bild zu Bild, sobald sich etwas bewegt; der
Strahl kommt dann jedes Mal anders an eine Linie heran. `sm.order` steht in
`src/main.c` wieder auf 0, die Funktion bleibt im Code. Der dunkle Weg ist
damit wieder so lang wie vor 0.1.5.

## Kürzerer Strahlweg (0.1.5)

Im Protokoll des Automaten fielen Bilder auf, für die die Platine bis zu
24 ms brauchte, mehr als die 20 ms eines Bildes bei 50 Hz: Solche Bilder
kommen zu spät, das ist echtes Stocken. Ursache war der dunkle Weg des
Strahls zwischen den Linien (90.000 bis 120.000 Einheiten, mehr als die
gezeichneten Linien). `bz_order_lines` (render/smooth.c) ordnet die
Linien jetzt so, dass der Strahl von einem Strich zum nächstgelegenen
springt, Striche auch rückwärts, Sterne zuletzt. Am Automaten gemessen:
dunkler Weg 26.000 bis 36.000 Einheiten, die Platine braucht höchstens
1,3 ms je Bild. Die Bewegung misst sich unverändert.

## Ton gegen MAME abgeglichen (0.1.4)

Das Soundmodell ist jetzt mit MAME verglichen: dieselben Schreibzugriffe
auf die Soundhardware in beiden, Sound für Sound ausgewertet
(`tools/soundcheck.sh`, Ergebnis in docs/SOUNDS.md). Motor, Explosionen,
POKEY-Melodien und Raketensummen liegen innerhalb von gut 1 dB an MAME,
auch im Spektrum. Behoben: Der POKEY-Ausgang war linear statt über das
Widerstandsnetz des Chips gerechnet, die Melodien klangen dadurch dumpf
und das Raketensummen war 7 dB zu leise. Bewusst anders als MAME: die
Lautstärke des Schusses nach Bit D3 (`LOUDSH` des Originalprogramms).
Das ganze Modell ist 6 dB lauter als vorher. Am Automaten noch ungehört.

## Zwischenbilder wie in Star Wars (0.1.3)

Auf Wunsch des Besitzers umgestellt: Statt die fertigen Linien eines
Frames zum nächsten zu verschieben, wird der Spielzustand zwischen zwei
Frames eingesetzt (Spieler, Gegner, Schüsse, Untertasse, Trümmer,
Vulkansteine, Radarstrahl, Logo), und die Zeichenroutinen des Spiels
zeichnen dieses Bild auf einer Kopie neu (`compute` in `render/smooth.c`).

Dafür nötig, weil das Spiel die Sicht nur mit 9 Bit Winkel dreht und die
Objekte mit 8: Auf der Kopie nehmen ROTATE und PNTPUT Sinus und Kosinus
aus allen 16 Bit (`bz_machine.fine`), die Berge bekommen den Rest ihres
Wegs nachgereicht (`mount_shift`), und die 8-Bit-Winkel von Antenne,
Untertasse und Trümmern tragen ihre Nachkommastelle mit.

Gemessen (`bzshot --hz 50`, Zeile "single lines": wie stark sich der
Schritt einer Linie von Bild zu Bild ändert, in Prozent eines Schritts):

| | Fahren und Drehen | Spiel mit Abschüssen |
|---|---|---|
| berechnet (neu, `smooth = 1`) | 5,2 | 17,0 |
| Linien verschoben (`smooth = 2`) | 3,3 | 15,1 |
| ohne Zwischenbilder | 198,7 | |

Beide Verfahren sind also gleichmäßig; das berechnete hat die richtige
Perspektive und lässt Objekte dort ins Bild kommen, wo sie sind. Die
ersten Fassungen waren es nicht: Die feinen Winkel landeten im Speicher
statt in der Math Box, und Trümmer, Antenne, Raketenhöhe und Einschläge
sprangen einmal pro Frame. Erst die Messung je Objektart hat das gezeigt.

Offen: Die Flamme der Rakete flackert (das Spiel wechselt ihr Bild jeden
Frame), ein Einschlag wächst in ganzen Stufen. Am Automaten ungesehen.

## Zweite Rückmeldung vom Automaten (0.1.2)

Umgesetzt:

* **Bewegung stufenhaft.** Die Platine zeichnet ihr letztes Bild mit
  ihrer eigenen Bildrate immer wieder (Menü des Automaten: DVG REFRESH,
  30 bis 60 Hz, Vorgabe 40). Der Port sendete 45 Bilder pro Sekunde:
  Passt das nicht zur Rate der Platine, werden Bilder abwechselnd doppelt
  gezeigt und übersprungen, das sieht aus wie Stocken. Star Wars sendet
  50 und läuft glatt. Jetzt `board_hz`, Vorgabe 50; muss gleich DVG
  REFRESH sein. Am Automaten noch nicht bestätigt (er war aus).
* **Gitter baut sich vor der Nase auf.** Bis 5000 Einheiten voraus ist
  es ohne Stufen voll hell, erst dahinter kommt es in acht Stufen aus dem
  Dunkel, oben am Horizont, wo eine Stufe kaum Bildfläche hat.
* **Entfernt samt Option:** feineres Gitter, Kettenspuren, Radarpunkte,
  Pfeil. Alte Schlüssel in `battlezone.ini` werden stillschweigend
  überlesen.
* **Sterne** höher, hinter Mond und Bergsilhouette, funkelnd.
* **Zweite Bergreihe** mit Parallaxe (0,6-fache Drehrate), nur über der
  Silhouette der ersten Reihe. Ein Vulkan, vorn.
* **Explosion:** Trümmer in der Farbe des zerstörten Fahrzeugs, dazu
  Funken in derselben Farbe (`enh_particles`); die Untertasse funkt
  violett.
* **Highscore-Eingabe mit Yoke** geht (die Hebelschalter werden im
  Analogmodus ab halbem Weg mitgeliefert). Die Anleitungszeile wird je
  Eingabeart ersetzt (Englisch, Farbmodi). Die Ersetzung ist nur im Code
  geprüft, nicht im Bild: Das Testskript kommt nicht bis zur Eingabe.

Geprüft: `bzshot` mit der neuen Zielautomatik (`aim`) erreicht 26.000
Punkte, die Funken und Trümmer sind grün (Panzer). Vergleich gegen MAME
weiter gleich.

## Belegung wie bei allen Vektorspielen (2026-10-19)

- Menü (`src/platform/menu.c`): START/Enter auf einer Zeile, dann ein Knopf; der erste seit der Cursor auf der Zeile steht ersetzt die Belegung, jeder weitere START fügt hinzu (" or "). Während des Wartens: Esc/Enter (bei Spielfunktionen auch START) = zurück ohne Änderung, START 2 = NONE (nur Spielfunktionen), Entf (auch Keypad) = AUTO. Links/Rechts = AUTO/NONE (Menü-Zeile nie NONE). Anzeige mit echten Namen ("LEFT CTRL + JOY1 BUTTON 1"), eine Taste darf mehrere Aktionen haben.
- Das Menü geht nur über feste Eingaben (`bz_nav` über `src/platform/vpbind.[ch]`, Kopie aus dem Tempest-2000-Port): Pfeile, jeder Stick/Hat/Steuerhorn, START/Enter/Keypad-Enter. Belegungen und die Fahrweisen wirken im Menü nicht mehr.
- AUTO direkt gelesen (`AUTO_SPEC` in controls.c: Pfeile/erster Stick, E/D/I/K für die Hebel, Left Ctrl + Stick-Knopf 1 + linke Maustaste fürs Feuer, 1, 5); Steuerhorn-AUTO: das Gerät mit "yoke" im Namen, sonst Joystick 1, direkt mit Totzone. Neu: Maustasten als Belegung. Beim Belegen nie: Tab, F2, Esc, Enter, Keypad-Enter, Entf; bei Spielfunktionen auch START/COIN.
- Am PC geprüft (ersetzen, hinzufügen, NONE), Pi-Build am Automaten installiert (alte Datei `battlezone.before-vpbind`), dort nicht ausprobiert.

GitHub-Release (2026-10-19): `v0.1.21` mit demselben Paket wie im Google-Drive-Ordner `games`. Bei jedem neuen Paket im Drive auch ein Release anlegen (`gh release create v<version> <paket>.vpk`).
