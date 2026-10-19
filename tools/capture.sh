#!/bin/sh
# capture.sh - record a run of Battlezone (MAME set bzonea) for bzcompare.
#
#   sh tools/capture.sh OUT.bzcap SECONDS ["PLAY SCRIPT"]
#
# MAME and its ROM folder: $BZ_MAME (default D:/Mame/mame.exe) and
# $BZ_MAME_ROMS (default D:/Mame/roms). MAME runs without picture and
# sound, as fast as it can, with a configuration folder of its own
# (_scratch/mame), so that the owner's MAME settings stay untouched.
# The play script: see tools/mame/bzcapture.lua.
here=$(cd "$(dirname "$0")/.." && { pwd -W 2>/dev/null || pwd; })
out=$1
secs=${2:-150}
play=$3
case "$out" in
    /*|?:*) ;;
    *) out="$({ pwd -W 2>/dev/null || pwd; })/$out" ;;
esac
# (a fresh configuration every time: MAME keeps option switches a script
# has set, and the next recording would start with them)
rm -rf "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
mkdir -p "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
cd "$here/_scratch/mame" || exit 1
BZCAP_OUT="$out" BZCAP_PLAY="$play" "${BZ_MAME:-D:/Mame/mame.exe}" bzonea \
    -rompath "${BZ_MAME_ROMS:-D:/Mame/roms}" -cfg_directory cfg -nvram_directory nvram \
    -autoboot_script "$here/tools/mame/bzcapture.lua" \
    -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run "$secs" 2>&1 | grep -i "bzcapture\|error"
