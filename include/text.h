/*
 * text.h - texts
 *
 * Building a text from a string with its control codes, drawing it; number and string formatting;
 * Shift-JIS voiced marks.
 * The declarations of src/text.c: its types and constants, its globals and its functions.
 * See docs/text.md.
 */

#ifndef TEXT_H
#define TEXT_H

#include "common.h"
#include "psyq.h"

/* A text string is a chain of GlyphNode in g_glyphNodes, built by Text_Build from a
 * Shift-JIS string with %-codes. The first node (the head, whose index is the text
 * id) describes the string; the following nodes are its glyphs. `next` of the last
 * node points to itself; -1 marks a free node. */
typedef struct GlyphNode
{
    short next;      /* 0x00 next node; self = last; -1 = free */
    short u;         /* 0x02 glyph: VRAM x of its cell (16-bit units); head: width in 8-pixel columns */
    short v;         /* 0x04 glyph: VRAM y of its cell; head: alignment (TEXT_ALIGN_*) */
    short cell;      /* 0x06 glyph: cache cell; head: screen x */
    short col;       /* 0x08 glyph: column of the glyph in the string; head: screen y */
    short pal;       /* 0x0A glyph: palette (CLUT x / 16); head: width of the field to align in */
    short width;     /* 0x0C glyph: width in pixels (8 half-width, 16 full-width) */
} GlyphNode;         /* size 0x0E */

#define GLYPH_NODES     512
#define TEXT_ALIGN_LEFT   0
#define TEXT_ALIGN_RIGHT  1
#define TEXT_ALIGN_CENTER 2

/* A Shift-JIS character and its form with a (semi-)voiced mark (k_sjisRemapA / B). */
typedef struct SjisPair
{
    char from[2];
    char to[2];
} SjisPair;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u16 g_wideHexDigits[16];      /* 0x800A8828 */
extern char g_hexDigits[16];         /* 0x800A8848 */
extern SjisPair k_sjisRemapA[52];    /* 0x800A8EB0 dakuten: 51 pairs, then 0 */
extern SjisPair k_sjisRemapB[21];    /* 0x800A8F80 handakuten: 20 pairs, then 0 */
extern short g_glyphNodeNext;        /* 0x800B2486 */
extern char *g_textContinue;         /* 0x800B2498 */
extern char g_textPalette;           /* 0x800B24A4 */
extern char g_textAlign;             /* 0x800B24BA */
extern GlyphNode g_glyphNodes[512];  /* 0x800CC238 */

/* ---- Functions ---- */

/* src/text.c */
void Str_AppendAt(char *buf, char *src, u16 *pos);        /* 0x8001ABF8 */
short Text_Build(char *text);                             /* 0x8001DFFC */
void Text_Free(short id);                                 /* 0x8001E33C */
void Str_FormatIntWide(int value, int width, u8 *out);    /* 0x8001E468 */
void Text_Render(short id, GsOT *ot, u16 pri, short clutX, u16 clutY); /* 0x8001E974 */
void Text_DrawDefault(short id, GsOT *ot, short pri);     /* 0x8001EF1C */
void Str_FormatInt(int value, int width, char *out);      /* 0x8001EFFC */
short Text_Create(char *text);                            /* 0x8001F168 */
void Text_Draw(short id, short x, short y, GsOT *ot, short pri); /* 0x8001F1B4 */
int Text_GetWidth(short id);                              /* 0x8001F21C */
void Text_SetPalette(int id, u16 pal);                    /* 0x8001F28C */
void Text_DrawClut(short id, short x, short y, GsOT *ot, short pri, short clutX, short clutY); /* 0x8001F310 */
void Str_FillWideSpaces(int n, u16 *out);                 /* 0x8001F388 */
void Str_FillSpaces(int width, char *out);                /* 0x8001F3BC */
u32 Sjis_RemapA(char *ch);                                /* 0x80042BB8 */
u32 Sjis_RemapB(char *ch);                                /* 0x80042C2C */

#endif /* TEXT_H */
