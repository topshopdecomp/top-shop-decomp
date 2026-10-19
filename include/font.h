/*
 * font.h - the fonts and the glyph cache
 *
 * The fonts: expanding glyphs from the font files, the glyph cache in VRAM.
 * The declarations of src/font.c: its constants, its globals and its functions.
 * See docs/text.md.
 */

#ifndef FONT_H
#define FONT_H

#include "common.h"
#include "psyq.h"

/* Glyphs are drawn from 16x16 cells of a 4bpp VRAM area: cell n is at
 * VRAM x = 0x340 + (n / 256) * 64 + (n % 16) * 4 (16-bit units), y = 0x100 + (n % 256) / 16 * 16.
 * g_glyphCacheUsed has one bit per cell; the first g_glyphCacheReserved cells hold
 * glyphs that are never freed (digits and a few symbols, see Font_CacheDigits). */
#define GLYPH_CELLS     512
#define GLYPH_VRAM_X    0x340
#define GLYPH_VRAM_Y    0x100
/* Give a glyph cache cell back (the first g_glyphCacheReserved cells are never given back) */
#define GLYPH_CELL_FREE(cell) \
    do { if (g_glyphCacheReserved < (cell)) g_glyphCacheUsed[(cell) / 16] &= ~(u16)g_bitMask32[(cell) % 16]; } while (0)

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u16 g_fontClut[256];         /* 0x800A8858 */
extern u8 g_fontDigitsSjis[22];     /* 0x800A8A58 */
extern u8 g_fontKanaSjis[10];       /* 0x800A8A70 */
extern char g_fontKanaCached;       /* 0x800B2480 */
extern short g_glyphCacheNext;      /* 0x800B2482 */
extern short g_glyphLastWidth;      /* 0x800B2484 */
extern u8 *g_fontAscii;             /* 0x800B2488 */
extern u8 g_fontDepth;              /* 0x800B248C */
extern u8 *g_fontKanji;             /* 0x800B2490 */
extern char g_fontDigitsCached;     /* 0x800B2494 */
extern u16 g_fontFullJis;           /* 0x800B24A0 */
extern short g_glyphCacheReserved;  /* 0x800B24A2 */
extern short g_fontGlyphBytes;      /* 0x800B24A6 */
extern short g_fontDigitsBase;      /* 0x800B24A8 */
extern u8 *g_glyphUploadBuf;        /* 0x800B24AC */
extern u8 *g_glyphUploadPtr;        /* 0x800B24B0 */
extern u8 g_glyphUploadCount;       /* 0x800B24B4 */
extern u8 g_fontGlyphW;             /* 0x800B24B5 */
extern u8 g_fontGlyphH;             /* 0x800B24B6 */
extern short g_fontKanaBase;        /* 0x800B24B8 */
extern u8 g_glyphUploadRun;         /* 0x800B24BB */
extern short g_fontWorkBlock;       /* 0x800B253C */
extern u16 g_glyphCacheUsed[32];    /* 0x800CC1F8 */
extern u8 g_fontHalf;               /* 0x8012F000 */
extern u8 g_fontFull;               /* 0x80130000 */

/* ---- Functions ---- */

/* src/font.c */
void Font_ExpandKanji(u8 *dst, int rowCells, u8 *sjis);   /* 0x8001D924 */
int Font_AllocRun(void);                                  /* 0x8001DA50 */
short Font_CacheGlyph(u8 *sjis);                          /* 0x8001DBB4 */
void Font_FlushUploads(void);                             /* 0x8001DE80 */
void Font_CacheDigits(void);                              /* 0x8001E5FC */
void Font_CacheKana(void);                                /* 0x8001E71C */
void Font_DrawCell(short cell, short x, short y, int pal, GsOT *ot, u16 pri); /* 0x8001E83C */
void Font_SetSource(u8 *kanji, u8 *ascii, int w, int h, int depth, u16 fullJis); /* 0x8001EBFC */
void Font_ExpandAscii(u8 *dst, int rowCells, u8 *ch);     /* 0x8001EC44 */
int Font_JisToIndex(u32 jis);                             /* 0x8001ED2C */
short Font_SjisToJis(u8 *sjis);                           /* 0x8001ED80 */
void Font_Init(u8 *kanji, u8 *ascii, u8 *uploadBuf);      /* 0x8001EE24 */
void Font_ReserveCells(short n);                          /* 0x8001EF54 */
void Font_DebugPrintCacheWord(short id);                  /* 0x8001F248 */

#endif /* FONT_H */
