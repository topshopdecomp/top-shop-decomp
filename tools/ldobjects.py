"""Put the game's C objects into splat's linker script in the order of config/c_files.txt.

splat writes one line per C file of config/topshop.yaml (the original's split) in each section;
the C sources are organised differently now, so every run of those lines (one per section:
.rodata, .text, .data, .bss) is replaced by the files listed in config/c_files.txt. src/data.c
and src/bss.c keep their own lines and places.

    python3 -I tools/ldobjects.py build/topshop.ld config/c_files.txt > OUT.ld   (run by make)
"""
import glob
import os
import re
import sys

FIXED = ("data", "bss")


def main():
    ld, listing = sys.argv[1], sys.argv[2]
    files = [l.strip() for l in open(listing) if l.strip() and not l.startswith("#")]
    have = sorted(os.path.basename(p)[:-2] for p in glob.glob("src/*.c"))
    want = sorted(files + list(FIXED))
    if have != want:
        sys.exit(f"{listing} and src/*.c differ: only in src: {sorted(set(have) - set(want))}, "
                 f"only in the list: {sorted(set(want) - set(have))}")
    out, run = [], None
    for line in open(ld).read().split("\n"):
        m = re.match(r"^(\s*)build/src/(\w+)\.o\(\.(rodata|text|data|bss)\);$", line)
        if m and m.group(2) not in FIXED:
            if run != m.group(3):
                run = m.group(3)
                out += [f"{m.group(1)}build/src/{f}.o(.{run});" for f in files]
            continue
        run = None
        out.append(line)
    print("\n".join(out))


if __name__ == "__main__":
    main()
