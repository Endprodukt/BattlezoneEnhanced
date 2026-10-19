#!/bin/sh
# captures.sh - record the standard set of MAME runs the port is checked
# against (tools/checkall.sh), into a folder (default _scratch/cap).
#
#   sh tools/captures.sh [FOLDER]
#
# Each run is MAME's bzonea played by a script (tools/mame/bzcapture.lua):
# coins, start, then a player that turns to the enemy and fires, or one
# that drives about at random. What play does not reach by itself is
# poked into the machine: a score near the missile level, the bonus tank
# and 100000, the super tank, lives.
#   HITS (the score in thousands, BCD) is 00B8/00B9, LIVES 00CC,
#   NOR2D3 (missiles so far; 5 and up: super tanks) 02EC.
cd "$(dirname "$0")/.." || exit 1
dir=${1:-_scratch/cap}
mkdir -p "$dir"
COIN="60 coin 2;70 coin 2;80 coin 2;90 coin 2"
GAME="$COIN;130 start 2"
LIVES="400 poke CC 05;900 poke CC 05;1500 poke CC 05;2200 poke CC 05;3000 poke CC 05;4000 poke CC 05;5000 poke CC 05"

cap() { echo "== $1"; sh tools/capture.sh "$dir/$1.bzcap" "$2" "$3"; }

# (the best score poked to 1,500,000: its row of the table gets ten tank symbols)
cap attract   150 "600 poke 0301 15"
cap play1     240 "$GAME;140 auto 6"
cap kills     400 "$GAME;140 aim 3"
# the missile: from 10000 points on
cap missile   450 "$GAME;140 aim 3;150 poke B8 09;$LIVES"
# the missile again, with a player who drives about at random: it comes
# from behind and gets out of range
cap missile2  450 "$GAME;140 auto 4;150 poke B8 12;$LIVES"
# and with the player turned about and the enemy moved by pokes
P=""; f=300; i=0
while [ $f -lt 6500 ]; do
    case $((i % 4)) in
        0) P="$P;$f poke 2A 00" ;;
        1) P="$P;$f poke 30 $(printf %02X $(( (i*53) % 256 )));$((f+1)) poke 34 $(printf %02X $(( (i*97+64) % 256 )))" ;;
        2) P="$P;$f poke 2A 80" ;;
        3) P="$P;$f poke CC 05;$((f+1)) poke 2A 40" ;;
    esac
    f=$((f+37)); i=$((i+1))
done
cap missile3  450 "$GAME;140 aim 5;150 poke B8 12$P"
# the bonus tank at 15000, the super bonus at 100000
cap bonus     450 "$GAME;140 aim 3;150 poke B8 14;$LIVES;1200 poke B8 98;2500 poke B8 99"
# the high score page with tank symbols (a score of 100000 and more), the
# bonus tank at 50000, the missile from 30000 on
cap bonus50   300 "5 dip DSW0 3D;$GAME;140 aim 3;150 poke B8 49;$LIVES;700 poke B8 99;1500 poke CC 01;1510 auto 5"
# super tanks, saucers, a long game
cap supertank 500 "$GAME;140 aim 3;150 poke B8 30;160 poke 02EC 05;$LIVES"
# game over with a high score: initials by the random player
cap hiscore   300 "$GAME;140 aim 3;200 poke B8 45;600 poke CC 01;610 auto 5"
# the other languages (DSW0 D7,D6), and the other option settings
cap german    200 "5 dip DSW0 55;$GAME;140 aim 3;700 auto 6"
cap french    200 "5 dip DSW0 96;$GAME;140 aim 3;700 auto 6"
cap spanish   200 "5 dip DSW0 EB;$GAME;140 aim 3;700 auto 6"
# free play; 2 plays a coin with the bonus adder; 2 coins a play
cap freeplay  150 "5 dip DSW1 00;130 start 2;140 aim 4"
# (two coins in quick succession: the second one's credit comes early)
cap twoplays  150 "5 dip DSW1 21;60 coin 2;64 coin 2;68 coin 2;130 start 2;140 auto 6"
cap twocoins  150 "5 dip DSW1 5F;$COIN;100 coin 2;110 coin 2;130 start 2;140 auto 6"
