"""Patch the PS-X EXE header of a built image to match its contents.

The header comes from the original (asm/header.s); after code changes the
entry point (pc0 = `start`) and text size (file size - 0x800) must follow the
build. Everything else (load address, stack, region string) stays original.

Usage: python3 -I tools/fixexe.py NM ELF EXE
"""
import os
import struct
import subprocess
import sys

nm, elf, exe = sys.argv[1:4]
syms = {}
for line in subprocess.run([nm, elf], capture_output=True, text=True, check=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3:
        syms[p[2]] = int(p[0], 16)

size = os.path.getsize(exe)
assert size % 0x800 == 0, "pad the image to 2 KB first"
with open(exe, "r+b") as f:
    head = bytearray(f.read(0x800))
    assert head[:8] == b"PS-X EXE"
    struct.pack_into("<I", head, 0x10, syms["start"])      # pc0
    struct.pack_into("<I", head, 0x1C, size - 0x800)        # t_size
    f.seek(0)
    f.write(head)
