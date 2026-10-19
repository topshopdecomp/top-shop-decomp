# Overview: the executable's layout and the toolchain

The game is Board Game - Top Shop (USA). Its executable is `disc/SLUS_013.33` (667,648 bytes).

## The PS-X EXE header

Parsed from the file:
- the magic `PS-X EXE`, and the region string "Sony Computer Entertainment Inc. for North America
  area";
- the entry point `pc0 = 0x8008DC04` (`start`); `gp0 = 0` in the header, so `start` sets `$gp` itself;
- the text loads at `0x80010000`, size `0xA2800`, so it ends at `0x800B2800`; there is no BSS in the
  header.

`SYSTEM.CNF` sets `TCB = 4`, `EVENT = 16`, `STACK = 0x801FFF00`.

## Sections inside the loaded image

| Range | Contents | How it is known |
|---|---|---|
| `0x80010000`–`0x80018EF7` | the game's read-only data (`.rodata`): format strings (e.g. "Box : %d, %d, %d, %d\n" at `0x80010000`), string literals and switch jump tables (`src/data.c`) | inspection |
| `0x80018EF8`–`0x800196E7` | the PsyQ libraries' read-only data | `config/topshop.yaml` |
| `0x800196E8`–`0x8008DBFB` | **the game's code** (`.text`). The first function is `Sys_VSyncCallback` at `0x800196E8`, found through its `VSyncCallback` argument | confirmed |
| `0x8008DBFC`–`0x8008DC03` | `__main` (the PsyQ runtime) | the PsyQ libraries' signatures |
| `0x8008DC04`–`0x800A56CF` | the PsyQ runtime and SDK libraries: crt0's `start`, libsnd `Ss*`, libspu `Spu*`, libgpu, libcd, libpad (`PAD_dr` at `0x800A56C0`)… | matched against the PsyQ libraries' signatures |
| `0x800A56D0`–`0x800B2397` | the initialized data: `.data` from `0x800A56D0`, with `g_bitMask32` (`0x800A56D0`), `g_fileLocTable` (`0x800A5750`) and `g_fileSectorTable` (`0x800A65D8`); `.sdata` from `0x800B1E74` | `config/topshop.yaml` |
| `0x800B2398`–`0x8011F3CF` | the BSS, cleared by `start`: the game's variables (`src/bss.c`, to `0x80117EAF`), then the libraries'. Its first 0x468 bytes are the end of the loaded image, all zeros | `config/topshop.yaml` |

`$gp = 0x800B1E74` (confirmed; see [system.md](system.md)).

## The toolchain

The string "Library Programs (c) 1993-1997 Sony Computer Entertainment Inc." dates the PsyQ
libraries to 1997 (a guess from those years: version 4.0 or 4.1). The game was built in November
2000 (the timestamp of `SLUS_013.33`). The exact compiler version was not determined; it would only matter for a matching
decompilation. (The rebuild uses the PsyQ build of GCC 2.8.1; see [build.md](build.md).)

## Overlays

None. All 219 `.J` files on the disc are data archives ([disc-formats.md](disc-formats.md)); a scan
for `jr ra` across all the files that are not media found only 3 stray matches in about 2.8 MB. All the
game's code is in the executable.

## The top-level flow

See [system.md](system.md) (boot, the main loop, the frame, the controllers) and `src/main.c`.
