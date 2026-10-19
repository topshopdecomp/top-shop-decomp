"""Fail if the linker script pins a symbol of the moving part of the image.

build/undefined.ld (tools/filter_undefined.py) defines the symbols no object defines at
their original address. That is right for addresses outside the EXE (RAM below it,
heap buffers above BSS, hardware) and for the read-only data at the start, which keeps
its place; anywhere else the rebuilt layout differs (the C code is not the original
size), so the C would read and write whatever is now at the old address.

A typical cause: two names for one address (D_ADDR and a real name); splat emits a label
for one of them only.
Usage: python3 -I tools/check_layout.py build/undefined.ld   (run by make)
"""
import re
import sys

FIXED_LO, FIXED_HI = 0x80010000, 0x8001FEF8   # .rodata: same address and contents
MOVING_HI = 0x80120000                        # end of the original BSS (heap buffers above)

bad = []
for line in open(sys.argv[1] if len(sys.argv) > 1 else "build/undefined.ld"):
    m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+);", line)
    if m:
        a = int(m.group(2), 16)
        if FIXED_HI <= a < MOVING_HI:
            bad.append((m.group(1), a))
if bad:
    print("check_layout: symbols pinned to their original address in the moving part of the image:")
    for n, a in bad:
        print(f"  {n} = 0x{a:08X}")
    sys.exit(1)
print("check_layout: ok")
