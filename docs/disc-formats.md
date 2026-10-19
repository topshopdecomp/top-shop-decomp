# Disc file formats

## What is on the disc

The disc's root holds `SYSTEM.CNF`, `SLUS_013.33` (the executable), `license_data.dat`, `ZZZ.ZZZ`
(33.8 MB of zero bytes: padding) and the directory `A/` with 947 files:

| Extension | Count | What they are |
|---|---|---|
| `.TIM` | 580 | standard PSX TIM images |
| `.J` | 219 | archives (format below) |
| `.SPC` | 130 | one LZSS block each, unpacking to an image stream or a sprite set ([graphics.md](graphics.md)) |
| `.XA` | 15 | XA streamed audio (the voices; the `GM` files are stored as plain data, [fileio.md](fileio.md)) |
| `.FNT` | 2 | the fonts: `FONT0.FNT` (128 KB), `FONT0H.FNT` (4 KB) |
| `.BIN` | 1 | `EVENT.BIN` (272 bytes, file index 0); it contains names like "CHR2P". No code loads it |

The game loads its files by index, through a table of positions and sizes in the executable, not
by name ([fileio.md](fileio.md)). [filetable.txt](filetable.txt) lists index → file for all 930
table entries.

## `.J` archives

All 219 files validate against this layout:

```
u32 count
u32 offset[count + 1]     // offset[0] == 4 + 4 * (count + 1), the header's size;
                          // offset[count] == the file's size; ascending
u8  data[]                // entry i = data[offset[i] .. offset[i + 1])
```

For example `CAL_01.J` has count 3 and the offsets `0x14, 0x524, 0x800, 0x3AA8` (15016 bytes).
The entries are padded to 4 bytes.

The **last entry is often LZSS-compressed** (a `u32` size, then an LZSS stream; see
[fileio.md](fileio.md)) and decompresses to an image stream (TIM `0x10` and palette `0x11` records).
This was confirmed for `CAL_01`, `CNT00`, `EVE_000` and `KAO0P`.

### The families

| Files | What they hold |
|---|---|
| `CAL_01`–`CAL_13` | the calendar pages: entry 0 the page's script, entry 1 the scene images, entry 2 an LZSS-packed image stream ([script.md](script.md)) |
| `CNT00`–`CNT19` | the story mode's scenes, laid out the same way; the script contains its own name ("CNT00") |
| `EVE_000`–`EVE_099` | the events' scenes, the same way; the script contains its own name ("EVE_000") |
| `TALK` | 32 LZSS-packed character talk scripts (entries 0–15 at the start of a turn, 16–31 at its end) |
| `CHR0P0`–`CHRFP0` | each character's board piece: a palette for each player slot, then the sprite set of its walking animations (`Board_LoadData`, sprite group 3); the ending's results screen uses them too |
| `KAO0P`–`KAOFP` | the faces (顔 "face"): the portraits (`Portrait_LoadFace`) |
| `STG00`–`STG09` | each stage's background map: entry 0 the map (at +4 its width and height in tiles and the tile width and height, from +8 the cells), entry 1 the tile table (from +8). `Board_LoadData` sets both background planes up on it (`Board_InitPlane`); plane 1 then takes the season's map |
| `STG00_00`–`STG09_03` | each stage's map for plane 1 in each of the four seasons, laid out the same way (`Board_LoadSeasonGraphics`) |
| `OPEN5` | a picture file laid out like `CHR?P0.J` (a palette, then a sprite set): the title screen loads its palette and its sprites (sprite group 3) |
| `SEQ` | the music and sound-effect sequences: 59 entries (songs 0–22 LZSS-packed, the sound effects not) |
| `VAB` | the sound bank (about 370 KB): entry 0 the samples (VB), entry 1 the header (VH) (`Snd_LoadVab`) |

`STG01.J`'s last entry does not decode as an image stream: it is the stage's tile table, as above.

## The pictures (`.TIM`)

| Files | What they are |
|---|---|
| `SH<type>L<n>_<k>.TIM` (440) | the stores' pictures: for each of the 44 store types, one for each length n of a chain (1–4) and place k in it (1–n), 10 a type, in that order. `ShopOpen_Task` reads `FILE_SH00L1_1_TIM` + 10 × the type + 0, 1, 3 or 6 for a chain of 1–4 + the place (`Store_ImageFile`, with `Util_DecimalTier`, `0x8007B988`) |
| `SHO4L4_1`–`SHO4L4_4.TIM` | four more spelled with a letter O, in the file table after `SH43L4_4.TIM`; the formula never reaches them |
| `C_000_`–`C_099_.TIM` (100) | the events' pictures, turned over before an event's script (`EventCard_LoadTask`, [cpu.md](cpu.md)) |
| the others (36) | single pictures: the logos (`A1.TIM`, `E_KID.TIM`, [graphics.md](graphics.md)), backgrounds (`HYO_BG.TIM`, `END_BG.TIM`), `SUCCESS.TIM`, `CORNER.TIM`, `R_01`–`R_20.TIM`, `0`–`3.TIM` and more; `TIT_A.TIM` and `WAIWAI.TIM` are not in the file table, so the game can't load them |
