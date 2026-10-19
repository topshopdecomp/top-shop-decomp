# Board Game - Top Shop — decompilation

A decompilation of **Board Game - Top Shop** for the Sony PlayStation (USA release,
SLUS-01333) into C.

- All 657 functions of the game's own code are readable C (`src/`, 51 files organized by
  subsystem), with named functions, variables, structures and constants, and comments.
- The game's data tables are C as well (`src/data.c`, `src/bss.c`).
- Only Sony's PsyQ library (and the executable's header) stays assembly. It is not part of this
  repository: the build disassembles it from your copy of the game's executable (`make split`,
  with splat).
- The 165 event, story, calendar and character-talk scripts the game runs are decompiled into
  a readable, commented script language (`scripts/`), with a tool that disassembles them and
  assembles them back to the exact bytes on the disc (`tools/tss.py`).
- `docs/` documents the game: its systems, file formats, the script interpreter, and the
  findings along the way.

The rebuilt game is not yet byte matching, but it is behavior-equivalent: the code is compiled
with the PsyQ-era GCC 2.8.1 and linked into a new executable, which was compared with the
original in an emulator over 25 scenarios (menus, story mode, free play, events, saving and
loading) with identical game state.

**No game assets are included.** You must provide your own legally obtained disc image of the game:
the build extracts the assets from it.

## Requirements

- Linux on x86-64 (`make setup` downloads Linux builds of the PsyQ GCC and of
  [mkpsxiso](https://github.com/Lameguy64/mkpsxiso))
- `make`, a C compiler, `curl`, `gpgv`, `git`, `tar` with xz, Python 3.9 or newer
  (`uv` is used if it is installed, else `python3 -m venv` and `pip`)
- a disc image of **Board Game - Top Shop (USA)** (SLUS-01333) as `.bin` + `.cue`; the `.bin`
  must have the MD5 `e886c77d61880e3ff655af46ac82c323`

## Building

```sh
make setup          # once: installs the toolchain into tools/ (binutils is built from
                    # source, which takes a few minutes. GCC 2.8.1, maspsx, splat, mkpsxiso
                    # and dumpsxiso are downloaded. Nothing is installed system-wide)
cp game.bin game.cue rom/   # your image's .bin and .cue; keep their names (the .cue names its .bin)
make                # the game as released
make CPU_TALK=1     # or: re-enables the CPU players talking before and after each turn, which
                    # the English release switched off. Still untranslated here.
```

`make` runs the whole build:

1. **Extract** the game's files from your disc image into `disc/` and its layout into
   `disc.xml` (`tools/extract_disc.sh`, which runs `dumpsxiso -x disc -s disc.xml <cue>`). First
   the `.bin` is checked: its MD5 must be `e886c77d61880e3ff655af46ac82c323` (`ROM_MD5` in the
   Makefile), else the build stops with an error. Then every extracted file is checked against
   `tools/disc_files.sha1`. Last, the characters' voices are written into `voices/`
   (`tools/extract_voices.py`), as XA files for a `CPU_TALK` build and as WAV files to listen to
   (`docs/fileio.md`). Instead of copying the image into `rom/` you can
   name it: `make extract CUE="path/to/Board Game - Top Shop (USA).cue"`.
2. **Split** the original executable `disc/SLUS_013.33` with splat (`config/topshop.yaml`):
   the PsyQ library and the executable's header become `asm/`.
3. **Compile and link** `src/*.c` and `asm/` into `build/SLUS_013.33`
   (GCC 2.8.1 → maspsx → GNU as / ld; link order `config/c_files.txt`).
4. **Make a disc image**: the original disc's files with the new executable (and, with
   `CPU_TALK=1`, the voices as XA), `build/topshop.bin` / `build/topshop.cue`, which boots in a
   PlayStation emulator.

Steps 1 and 2 run only when `disc/` or `asm/` is missing (or incomplete). Each step can be run on
its own: `make extract`, `make split`, `make exe`, `make image`; `make voices` writes `voices/`
again and `make scripts` assembles the scripts (below). `make clean` removes `build/`;
`make distclean` also removes everything extracted from or generated out of your disc image.

`make CPU_TALK=1` builds a game that plays the CPU players' talk at the start and the end of their
turns, which the retail game never shows, with the characters' voices (`docs/build.md`,
`docs/script.md`). It goes with any target (`make CPU_TALK=1 image`); changing it compiles the C
again. Without it, the build is the retail game.

`make split` prints splat's suggestions for splitting the read-only data into files and about
`sdk_rodata`: they are expected, and don't apply to this build (`docs/build.md`).

## Layout

| Path | What |
|---|---|
| `src/` | the game's code and data in C |
| `include/` | one header per C file (`include/<name>.h` for `src/<name>.c`: its types, constants, globals and prototypes); `topshop.h` includes them all |
| `config/` | splat's configuration and symbol lists, the link order (`c_files.txt`) |
| `scripts/` | the game's scripts as text, see `scripts/README.md` |
| `tools/` | the build tools, the script tools, `setup.sh` (which installs the toolchain here) |
| `docs/` | documentation of the game; start at `docs/index.md` |
| `rom/` | where your disc image goes (not distributed) |
| `disc/`, `disc.xml`, `voices/`, `asm/`, `build/` | made by the build from your disc image (not distributed) |

## The scripts

The scenes of the game (the events, the story mode's scenes, the animated calendar pages and
the characters' comments during a turn) are bytecode run by an interpreter in `src/script.c`.
`scripts/` holds all of them as text with labels, named variables and constants, the routines
they share named, comments, and an English rendering of every text (some were left in
Japanese in this release). `scripts/README.md` describes the language, and how to edit the
scripts and build them back into the files that hold them on the disc.

```sh
make scripts                                   # assemble scripts/ into the disc files that
                                               # hold them, in scripts/build/ (needs disc/)
python3 -I tools/tss.py build DIR -o OUT       # the same for the .tss files in DIR, into OUT
python3 -I tools/tss.py asm scripts/events/eve_049.tss out.bin   # one script to bytecode
python3 -I tools/tss.py dis disc/A/EVE_049.J   # one script file to text
python3 -I tools/tss.py extract                # write scripts/ again from the disc
```

## Documentation

`docs/` records how the game works, as found during the decompilation: the boot and main loop,
the task system, file I/O and the CD queue, graphics, sound, the board game's rules, the CPU
players, the event cards, saving, the script interpreter, and the 46 events the game never plays.
Addresses are those of the
original executable. The comparisons with the original game were made with an emulator test
harness that is not part of this distribution.

## Legal

This repository contains no assets from the game. Use it with a copy you have legally obtained.
The tools `make setup` downloads come under their own licenses (mkpsxiso's is in
`tools/mkpsxiso/share/doc/mkpsxiso/` once it is installed).
