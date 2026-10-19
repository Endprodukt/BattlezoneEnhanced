#!/bin/sh
# make_vpk.sh - the game as a VectorPie package: build/bzenhanced-<version>.vpk
# (external/VectorPie/sdk/docs/07-packages.md; after Red Baron Enhanced's).
#
# Run from the project folder, in Git Bash, after the Pi build (BUILD.md).
# The package holds the Pi program, game.ini, the default settings
# (battlezone.ini), the marquee and the sounds that replace the board's
# (package/sounds, made by tools/mkfreesound.py; their sources in
# sounds/CREDITS.txt). The ROMs are not in it (the cabinet's MAME set,
# bzone). The cabinet's menu installs it from a USB drive's games/ folder
# or from /persistent/games/packages/ (Settings -> GAMES -> INSTALL GAMES).
# (run.sh is not in it: it is only how tools/deploy.sh starts the game.)
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
stage="$root/build/pkg/bzenhanced"
[ -f "$root/build/pi/battlezone" ] || { echo "make_vpk: no Pi build in build/pi" >&2; exit 1; }
rm -rf "$root/build/pkg"
mkdir -p "$stage/sounds"
cp "$root/build/pi/battlezone" "$root/package/marquee.png" "$stage/"
# (the text files with Unix line ends: a Windows checkout may give them CR LF)
for f in game.ini battlezone.ini; do tr -d '\r' < "$root/package/$f" > "$stage/$f"; done
cp "$root"/package/sounds/*.wav "$stage/sounds/"
tr -d '\r' < "$root/package/sounds/CREDITS.txt" > "$stage/sounds/CREDITS.txt"
tr -d '\r' < "$root/external/VectorPie/sdk/tools/vp-pack/vp-pack.sh" > "$root/build/pkg/vp-pack.sh"
sh "$root/build/pkg/vp-pack.sh" "$stage" "$root/build"

# Built on Windows the files in the archive carry no Unix modes worth having
# (the file system has none for vp-pack's chmod to set): the program 755,
# folders 755, the rest 644.
version=$(sed -n 's/^version[[:space:]]*=[[:space:]]*//p' "$root/package/game.ini" | tr -d '\r' | sed 's/[[:space:]].*//')
vpk="$root/build/bzenhanced-$version.vpk"
py -3.10 -I - "$vpk" <<'PY'
import io, sys, tarfile
path = sys.argv[1]
out = io.BytesIO()
with tarfile.open(path, "r:gz") as old, tarfile.open(fileobj=out, mode="w:gz") as new:
    for member in old.getmembers():
        if member.isdir() or member.name == "bzenhanced/battlezone":
            member.mode = 0o755
        else:
            member.mode = 0o644
        member.uid = member.gid = 1000
        member.uname = member.gname = "pi"
        new.addfile(member, old.extractfile(member) if member.isfile() else None)
open(path, "wb").write(out.getvalue())
PY
ls -l "$vpk"
