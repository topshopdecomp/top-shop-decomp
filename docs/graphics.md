# Graphics: frames, images, sprites, fades, tile maps, tile sheets

The code is in `src/gfx.c` (images and sprites), `src/fade.c` (fades, tints, wipes), `src/bgmap.c`
(tile-map layers) and `src/tilesheet.c` (tile sheets), declared in `include/gfx.h` (with
`VramImage` and `SpriteGroup`), `include/fade.h`, `include/bgmap.h` and `include/tilesheet.h`. The
frame and ordering-table flow is in [system.md](system.md).

## The frame

- A double-buffered ordering table, `g_ot[2]` (`0x800CBCF0`, 32 tags each); `g_otIndex` is the
  current one.
- `Gfx_QueueClear(r, g, b)` (`0x8001A2FC`), once per frame, queues a DR_ENV that clears the screen
  to (r, g, b). The mode tasks call it first each frame.
- The global tint `g_tintR` / `G` / `B` (`0x800B23F4` / `F2` / `F0`; 0x80 = none) applies to all
  image drawing.
- `g_primPtr` (`0x80119428`): the frame's packet buffer for the primitives that are not
  `GsSPRITE`s.

## Images

`VramImage` = {`mode` (`IMG_MODE_*`: 0, 1, 2 = 4, 8, 16 bits per pixel), `clutX`, `clutY`,
`palette`, `x`, `y` (in VRAM, in 16-bit units), `w` (in pixels), `h`}.

- `Gfx_DrawVramImage` (`0x80026F48`): draws an image of any size (e.g. a full 320×240 screen) as
  several `GsSortFastSprite`s, split at the 256-texel page boundaries and clipped to the screen.
- `Gfx_DrawImage(img, attr, palette, pos, ot, pri)` (`0x800251B0`): one sprite.
  - `palette`: −100 (`PAL_DEFAULT`) is `img->palette`; −101 (`PAL_BLANK`) the blank CLUT at
    (0, 0x1FE); anything else `img->palette + palette` (at 4 bits per pixel +16 per step in X, at 8
    bits +1 row).
  - `attr`: `0x400000` flips X and `0x800000` flips Y (`GFX_FLIP_*`; a flip draws a `POLY_FT4`),
    `0x40000000` is semi-transparency (`GFX_SEMITRANS`), bits 28–29 the blend mode.
- `Gfx_DrawImageEx(img, attr, palette, pos, align, angle, scale, ot, pri)` (`0x8002A160`): the same,
  aligned, rotated and scaled.

### Alignment codes

Every drawing routine reads `align` the same way:

| Bits | Value | Meaning |
|---|---|---|
| 0–1 (horizontal) | 0 | centered on the position |
| | 1 (`ALIGN_RIGHT`) | ending at the position |
| | 2 (`ALIGN_LEFT`) | starting there |
| | 3 | the coordinate is left unset |
| 4–5 (vertical) | 0 | centered |
| | 1 (`ALIGN_TOP`) | starting at the position |
| | 2 (`ALIGN_BOTTOM`) | ending there |
| | 3 | left unset |

## Sprites

- `g_sprites` (`0x800D0A38`): `VramImage[1005]`, split into 7 groups by `Sprite_InitGroups`
  (`0x8002ADE4`) from `g_spriteGroupTable` (`0x800FE140`), of 275, 70, 10, 100, 240, 190 and 120
  sprites. A sprite's id is `g_spriteGroups[g].base + n` (`GROUP_SPRITE(g, n)`).
- Group 0 holds the common sprites from `ETC2.SPC` (270 sprites in a group of 275), loaded by
  `Sys_LoadCommonSprites` (`0x80036CB8`) after the logos.
- `Sprite_LoadSet(spc, group, vramOffsets)` (`0x80024DE0`) appends a sprite set to a group.
- `Sprite_Draw` (`0x8002AE48`), `Sprite_DrawAligned` (`0x8002AE90`, with the alignment codes above),
  `Sprite_DrawEx` (`0x8002B3B8`, rotated and scaled through `Gfx_DrawImageEx`).
- A picture made of several sprites is a `SpriteList`: a list of `SpritePart`s (`{sprite, x, y,
  flip}`, ended by sprite −1) and its bounding box, which `SpriteList_GetBounds` (`0x800293DC`)
  computes.

## Formats

- **`.SPC`**: the whole file is one LZSS block (its first word the file's size; see
  [fileio.md](fileio.md)). Unpacked, it is either an image stream (TIM `0x10` and palette `0x11`
  records) or a sprite set: a `u32` count, that many 16-byte `VramImage`s (with `w` in VRAM
  units), then an image stream. Of the 130 files:
  - 84 are image streams: `BGCM01`–`BGCM13`, `BG_M01`, `C_BG00`–`C_BG19`, `KAO0P0`–`KAO0P4`,
    `LOAD_256`, `M256_BG`, `SB00_00`–`SB09_03`, `STAFF1`–`STAFF3`;
  - 46 are sprite sets: `CHR0P_1`–`CHRFP_1`, `ENDING`, `ETC2`, `E_CHR0`, `E_CHR1`, `OPEN2`–`OPEN5`,
    `S00_EXC1`–`S09_EXC1`, `SEQ`, `SET0CHR0`–`SET0CHR2`, `SET1`–`SET3`, `SF00_00`, `SF01_00`,
    `SF02_00`, `SF05_00`, `STAFF0`.
- **The logos**: `A1.TIM` goes to VRAM (448, 256) with its CLUT on row 493, `E_KID.TIM` to (608, 256)
  with row 494; both are 320×240 at 8 bits per pixel.

## Fades, tints and wipes (`src/fade.c`)

- **The fade level** (`g_fadeR` / `G` / `B`, −255 to 255 per channel) is drawn over the frame as a
  full-screen box by `Fade_DrawOverlay`: subtractive when darkening, additive when brightening.
  `Fade_SetLevel` and `Fade_GetLevel` set and read it; the presets are `Fade_SetClear` (0),
  `Fade_SetBlack` and `Fade_SetWhite`. `Fade_In(frames)` and `Fade_Out(frames)` fade from and to
  black, `Fade_FromWhite` and `Fade_ToWhite` from and to white, `Fade_To(r, g, b, frames)` to any
  level; `Fade_AnimTask` (`0x80055A2C`) moves the level, then hands over to `Fade_ApplyTask`, and
  `Fade_IsDone` polls.
- **Tints**: `Tint_To(r, g, b, frames, slot)` moves the color multiplier of one of the two tint
  slots (`g_tintSlotR` / `G` / `B`, 0x80 = none) over a number of frames (`Tint_AnimTask`).
- **Wipes**: the shapes that close and open the screen between turns (`Wipe_CloseTask`,
  `Wipe_OpenTask`; the shapes in `g_charWipeShapes`, drawn by `Wipe_DrawShape`).

## Tile-map layers (`src/bgmap.c`)

The board's two far background planes are tile maps, from the stage's `STG<stage>.J`
([disc-formats.md](disc-formats.md)). A `BgPlane` holds a `BgMap` with its two ordering tables
(`g_bgPlanes`, `0x80111D60` and `0x80111DC0`). For each plane, `BgMap_Build` (`0x80023E10`) prepares
the visible part as a grid of tiles (`BgCell`s, enough to cover 320×240 plus a partial and a
scrolling tile); `BgMap_Scroll` moves it (20.12 fixed point, the map wraps around), refilling the
rows and columns that come into view (`BgMap_FillRow`, `BgMap_FillColumn`); `BgMap_Draw` builds one
sprite or textured quad per tile (`BGTILE_*`: as a sprite, or flipped) into the plane's own
ordering table every frame.

## Tile sheets (`src/tilesheet.c`)

A tile sheet (`TileSheet`: the scene's images `g_sceneSheet`, the portraits `g_portraitSheet`) keeps
pictures cut into 8×8 or 16×16 cells in VRAM, in columns of cells (15 cells per 64-unit row, 225 per
column of rows). `TileSheet_LoadTims` and `TileSheet_AddImage` cut TIMs into frames (`TileFrame`),
staged in RAM and uploaded by `TileSheet_Flush`. A frame is drawn strip by strip of cells: aligned
(`TileSheet_DrawAligned`), rotated and scaled (`TileSheet_DrawRotated`), or squashed
(`TileSheet_DrawSquashed`, the portrait folding).

## A quirk of the original

`Sys_LoadCommonSprites` sizes its decompression buffer from the *compressed* size: `ETC2.SPC` is
27,124 bytes compressed (14 sectors allocated) but decompresses to 83,472 bytes (270 sprites and 23
image records), so it writes about 54 KB past its block. That is harmless in practice because the
heap is otherwise empty at that point. It is a bug of the original, kept as it is (confirmed with
`src/lzss.c` on the file).

## Answered questions

- `Gfx_DrawImageEx`'s arguments: `(img, attr, palette, pos, align, angle, scale, ot, pri)`.
- Which `.SPC` files are sprite sets and which plain image streams: listed under [Formats](#formats)
  (from the first word of each unpacked file: a count, or `0x10` / `0x11`).
