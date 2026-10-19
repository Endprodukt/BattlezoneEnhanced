#!/bin/sh
# checkanalog.sh - the analog mode at the ends of its levers must do what
# the cabinet's lever switches do: the same script driven both ways has to
# leave the tank at the same place with the same heading.
#
#   sh tools/checkanalog.sh [ROMS]
ROMS=${1:-D:/Mame/roms}
BZSHOT=build/pc/bzshot.exe
START="3 coin;5 start"
DIGITAL="6 drive f f;10 drive f -;13 drive b f;15 drive b b;18 drive - b;20 drive f b;22 drive - f;24 drive b -;26 drive f f;27 fire"
ANALOG="6 yoke 256 256;10 yoke 256 0;13 yoke -256 256;15 yoke -256 -256;18 yoke 0 -256;20 yoke 256 -256;22 yoke 0 256;24 yoke -256 0;26 yoke 256 256;27 fire"

a=$("$BZSHOT" --roms "$ROMS" --seconds 40 --play "$START;$DIGITAL" | grep 'the tank at')
b=$("$BZSHOT" --roms "$ROMS" --seconds 40 --play "$START;$ANALOG" | grep 'the tank at')
echo "switches:$a"
echo "analog:  $b"
if [ -n "$a" ] && [ "$a" = "$b" ]; then
    echo "THE SAME"
else
    echo "DIFFERENT"
    exit 1
fi
