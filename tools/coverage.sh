#!/bin/sh
# coverage.sh - which lines of the C routines (src/game) the recordings
# run through. Needs the coverage build (BUILD.md) in build/cov.
#
#   sh tools/coverage.sh [FOLDER] [--lines]
#
# --lines also lists every line never run.
cd "$(dirname "$0")/.." || exit 1
dir=_scratch/cap
lines=
for a in "$@"; do
    case "$a" in
        --lines) lines=1 ;;
        *) dir=$a ;;
    esac
done
command -v gcov > /dev/null || { echo "coverage.sh: gcov is not in the PATH (BUILD.md)"; exit 1; }
find build/cov -name "*.gcda" -delete
for f in "$dir"/*.bzcap; do
    ./build/cov/bzcompare "$f" --quiet > /dev/null
    ./build/cov/bznmi "$f" > /dev/null
done
mkdir -p build/cov/gc
cd build/cov/gc || exit 1
for f in mainline view play nmi vgut sound coin; do
    gcov ../CMakeFiles/bzcore.dir/src/game/$f.c.gcda 2>/dev/null | grep -A1 "src/game/$f.c'" | grep "Lines" | sed "s/^/$f.c: /"
    if [ -n "$lines" ]; then
        grep "#####" $f.c.gcov | sed 's/^ *#####: *//' | cut -c1-100
    fi
done
