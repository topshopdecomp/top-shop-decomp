#!/bin/sh
# Extract the game's files from your own disc image of Board Game - Top Shop (USA), SLUS-01333,
# into disc/ (the assets the build uses) and write disc.xml (the disc's layout, for mkpsxiso),
# then the characters' voices into voices/ (tools/extract_voices.py).
# The .bin is checked against the MD5 the Makefile gives (ROM_MD5) before anything is
# extracted, and every extracted file against tools/disc_files.sha1 after.
#
#   tools/extract_disc.sh MD5 [path/to/image.cue]      (make extract runs it)
#
# Without a .cue it uses the one .cue file in rom/. The .bin it names must be next to it.
set -eu
cd "$(dirname "$0")/.."

md5=${1:-}
cue=${2:-}
if [ -z "$md5" ]; then
    echo "usage: tools/extract_disc.sh MD5 [image.cue] (make extract gives the MD5)" >&2
    exit 1
fi
if [ -z "$cue" ]; then
    set -- rom/*.cue
    if [ ! -e "$1" ]; then
        echo "no disc image: put the .bin and .cue of Board Game - Top Shop (USA) in rom/," >&2
        echo "or give the .cue: make extract CUE=path/to/image.cue" >&2
        exit 1
    fi
    if [ $# -ne 1 ]; then
        echo "several .cue files in rom/; pick one: make extract CUE=path/to/image.cue" >&2
        exit 1
    fi
    cue=$1
fi
if [ ! -f "$cue" ]; then
    echo "$cue: no such file" >&2
    exit 1
fi

# the .bin the .cue names, next to the .cue
bin=$(sed -n 's/^[[:space:]]*FILE[[:space:]]*"\(.*\)"[[:space:]]*BINARY.*/\1/p' "$cue" | head -n 1)
if [ -z "$bin" ]; then
    echo "$cue: names no .bin (no line FILE \"....bin\" BINARY)" >&2
    exit 1
fi
case $bin in
    /*) ;;
    *) bin=$(dirname "$cue")/$bin ;;
esac
if [ ! -f "$bin" ]; then
    echo "$bin: no such file (the .bin $cue names)" >&2
    exit 1
fi
echo "checking $bin"
sum=$(md5sum "$bin" | cut -d ' ' -f 1)
if [ "$sum" != "$md5" ]; then
    echo "error: $bin is not the expected disc image" >&2
    echo "  its MD5 is $sum, expected $md5:" >&2
    echo "  Board Game - Top Shop (USA), SLUS-01333, a 1:1 rip of the disc as one .bin + .cue" >&2
    exit 1
fi

echo "extracting $cue"
rm -rf disc disc.xml voices
tools/mkpsxiso/bin/dumpsxiso -q -x disc -s disc.xml "$cue"

if [ ! -f disc/SLUS_013.33 ]; then
    echo "no SLUS_013.33 on that disc: it is not Board Game - Top Shop (USA)" >&2
    exit 1
fi
if ! sha1sum -c --quiet tools/disc_files.sha1; then
    echo "the files above differ from the USA release (SLUS-01333); the build needs that version" >&2
    exit 1
fi
echo "disc/: $(find disc -type f | wc -l) files, all as on the original disc"

# the voices, which the disc stores as plain data: as XA files for a CPU_TALK disc image, and
# as WAV files
python3 -I tools/extract_voices.py disc/A -o voices
