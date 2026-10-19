#!/bin/sh
# soundcheck.sh - the port's sound model against MAME's (docs/SOUNDS.md).
#
#   sh tools/soundcheck.sh [SECONDS]
#
# MAME plays a game with every sound in it and records its sound and every
# write to the sound hardware (tools/mame/bzsound.lua); the same writes are
# played on the model (bzsound --replay); tools/soundcheck.py compares the
# two sound by sound. Needs a Python with numpy ($BZ_PYTHON).
here=$(cd "$(dirname "$0")/.." && { pwd -W 2>/dev/null || pwd; })
secs=${1:-75}
py=${BZ_PYTHON:-/c/Users/ocare/AppData/Local/Programs/Python/Python310/python.exe}
mkdir -p "$here/_scratch/snd" "$here/_scratch/mame"
rm -rf "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
mkdir -p "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
cd "$here/_scratch/mame" || exit 1
BZSND_OUT="$here/_scratch/snd/writes.txt" "${BZ_MAME:-D:/Mame/mame.exe}" bzonea \
    -rompath "${BZ_MAME_ROMS:-D:/Mame/roms}" -cfg_directory cfg -nvram_directory nvram \
    -autoboot_script "$here/tools/mame/bzsound.lua" -video none -sound none -nothrottle \
    -skip_gameinfo -seconds_to_run "$secs" -wavwrite "$here/_scratch/snd/mame.wav" 2>&1 | grep -a "bzsnd"
cd "$here" || exit 1
build/pc/bzsound.exe --replay _scratch/snd/writes.txt --wav _scratch/snd/model.wav || exit 1
"$py" -I tools/soundcheck.py _scratch/snd/writes.txt _scratch/snd/mame.wav _scratch/snd/model.wav
