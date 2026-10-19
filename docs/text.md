# Text: fonts, the glyph cache, texts, text windows, message windows

The code is in `src/font.c`, `src/text.c`, `src/textwin.c` and `src/msg.c`, declared in
`include/font.h`, `text.h`, `textwin.h` and `msg.h`. Most of it follows the glyph cache
(`Font_CacheGlyph`, `0x8001DBB4`) in the original.

## Strings

Strings are Shift-JIS, with %-codes that `Text_Build` reads:

| Code | Meaning |
|---|---|
| `%C`, `%L`, `%R` | center, left- or right-align the text |
| `%K` | a new line: the text stops here, and the rest (`g_textContinue`) becomes the next text |
| `%E` | the end of the text |
| `%Pnn` | palette nn (two digits) |
| `%Snn` | skip nn columns (two digits) |
| `%%` | only skips its first `%`, so the second one starts a code: it prints no percent sign (no text the game builds uses it) |

The scripts' talk panel has its own text path with its own codes (`%N..` for player names;
`Talk_PutChar`, see [script.md](script.md)). Many constant strings are short `.sdata` literals
(`"%K"`, `"%E"`).

## Fonts

The game's Shift-JIS font (`FONT0.FNT`: 16 × 16, one bit per pixel, indexed by JIS code) and its
half-width font (`FONT0H.FNT`, 4096 bytes: 256 glyphs of 8 × 16, one bit per pixel, one for every
byte value; ASCII and the half-width katakana). Glyphs are expanded from them into 16 × 16 cells of
4 bits per pixel, a half-width glyph filling 8 of the 16 columns (`Font_ExpandKanji`,
`Font_ExpandAscii`, `Font_SjisToJis`, `Font_JisToIndex`).

## The glyph cache

- 512 cells in VRAM from x 0x340, y 0x100 (`GLYPH_CELLS`, `GLYPH_VRAM_X`, `GLYPH_VRAM_Y`), with one
  bit per cell in `g_glyphCacheUsed`.
- The first `g_glyphCacheReserved` cells (the digits, a few kana and symbols) are never freed.
- `Font_CacheGlyph` returns a glyph's cell; the uploads are batched in `g_glyphUploadBuf` and sent by
  `Font_FlushUploads` once per frame.

## Texts

`Text_Create` / `Text_Build` turn a string into a chain of `GlyphNode`s: a head node, whose index is
the text id, then one node per glyph. `Text_Draw`, `Text_DrawClut`, `Text_GetWidth` and `Text_Free`
work on a text by its id.

Number formatting: `Str_FormatInt` (ASCII), `Str_FormatIntWide` (full width), `Str_FillSpaces`.

## Text windows

16 `TextWin`s, each holding `TextLine` chains (one text per line). The functions:

- creating and filling: `TextWin_Create`, `TextWin_AddText`, `TextWin_ClearText`;
- opening and closing, with the grow and shrink animations: `TextWin_Open`, `TextWin_Close`;
- menus with a cursor bar: `TextWin_SetMenu`, `TextWin_CursorUp`, `TextWin_CursorDown`,
  `TextWin_GetCursor`, `TextWin_SetFocus`, and the scroll arrows `TextWin_SetArrowUp` /
  `TextWin_SetArrowDown`;
- drawing, every frame: `TextWin_DrawAll` (laying a window out the first time, `TextWin_Layout`).

The states are `TEXTWIN_*` in `include/textwin.h`.

## Message windows

`src/msg.c` shows a message in a text window and waits for a button, or asks yes or no:
`Msg_Show`, `Msg_AskYesNo` (with `Msg_AskYesNoCursor` / `Msg_AskYesNoAt`), `Msg_Close`,
`Msg_IsDone`, `Msg_IsAnswered`. They fill the work area of `g_uiTaskIds[0]` (`MsgWork`) and start
`Msg_Task` or `MsgChoice_Task` on it. For a CPU player the game answers by itself.

## See also

- The tile-map background layers (`BgMap_*`, `src/bgmap.c`): [graphics.md](graphics.md).
- The scripts' talk panel (`Talk_PutChar`): [script.md](script.md).
