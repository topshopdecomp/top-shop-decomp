/*
 * font.c - the fonts and the glyph cache
 *
 * The game's Shift-JIS font (FONT0.FNT: 16 x 16, one bit per pixel, indexed by JIS code) and its
 * half-width font (FONT0H.FNT). Glyphs are expanded to 4-bit cells and kept in a cache of 512
 * cells in VRAM, uploaded once per frame; the digits and a few kana stay cached for good.
 */
#include "common.h"
#include "topshop.h"

/* Expand a 16x16 1bpp glyph of the kanji font into 4bpp pixels for the glyph upload buffer
 * (rowCells = cells per buffer row) (0x8001D924) */
/* 0x8001D924 */
void Font_ExpandKanji(u8 *dst, int rowCells, u8 *sjis)
{
    u8 *src;
    int row;
    int col;

    src = g_fontKanji + Font_JisToIndex((u16)Font_SjisToJis(sjis)) * (u16)g_fontGlyphBytes;
    for (row = 0; row < 16; row++)
    {
        /* 2 bytes of 8 pixels; each output byte holds 2 pixels, color 1 where the bit is set */
        for (col = 0; col < 2; col++)
        {
            dst[0] = (*src >> 7) | (*src >> 2 & 0x10);
            dst[1] = (*src >> 5 & 1) | (*src & 0x10);
            dst[2] = (*src >> 3 & 1) | (*src & 4) << 2;
            dst[3] = (*src >> 1 & 1) | (*src & 1) << 4;
            dst += 4;
            src++;
        }
        dst += rowCells * 8 - 8;
    }
}

/* Find the next free glyph-cache cell from g_glyphCacheNext and count the free cells after it in
 * the same 16-cell row; clear that much of the upload buffer. Returns the run length, -1 if the
 * cache is full (0x8001DA50) */
/* 0x8001DA50 */
int Font_AllocRun(void)
{
    short tries;
    short c;
    int run;

    g_glyphUploadCount = 0;
    for (tries = 0; tries < 0x200; tries++)
    {
        if ((g_glyphCacheUsed[g_glyphCacheNext / 16] & g_bitMask32[g_glyphCacheNext % 16]) == 0)
        {
            break;
        }
        if (g_glyphCacheNext++ == 0x1ff)
        {
            g_glyphCacheNext = 0;
        }
    }
    if (tries == 0x200)
    {
        return -1;
    }
    run = 0;
    for (c = g_glyphCacheNext % 16; c < 16; c++)
    {
        if ((g_glyphCacheUsed[g_glyphCacheNext / 16] & g_bitMask32[c]) != 0)
        {
            break;
        }
        run++;
    }
    memset(g_glyphUploadPtr,0,run << 7);
    return run;
}

/* Cell holding the glyph of the character at sjis: the permanent digit/kana cells, else a new cell
 * (expanded into the upload buffer, uploaded by Font_FlushUploads when the run is full). Sets
 * g_glyphLastWidth to 16 (full-width) or 8 (half-width) (0x8001DBB4) */
/* 0x8001DBB4 */
short Font_CacheGlyph(u8 *sjis)
{
    u8 *dst;
    short cell;
    short i;

    if (g_fontDigitsCached != 0)
    {
        for (i = 0; i < 22; i += 2)
        {
            if ((sjis[0] == g_fontDigitsSjis[i]) && (sjis[1] == g_fontDigitsSjis[i + 1]))
            {
                g_glyphLastWidth = 16;
                return g_fontDigitsBase + i / 2;
            }
        }
    }
    if (g_fontKanaCached != 0)
    {
        for (i = 0; i < 10; i += 2)
        {
            if ((sjis[0] == g_fontKanaSjis[i]) && (sjis[1] == g_fontKanaSjis[i + 1]))
            {
                g_glyphLastWidth = 16;
                return g_fontKanaBase + i / 2;
            }
        }
    }
    if (g_glyphUploadRun == 0)
    {
        g_glyphUploadRun = Font_AllocRun();
    }
    dst = g_glyphUploadPtr + g_glyphUploadCount * 8;
    switch (*sjis & 0xf0)
    {
    case 0x80:
    case 0x90:
    case 0xe0:
    case 0xf0:
        /* the first byte of a 2-byte character */
        Font_ExpandKanji(dst,g_glyphUploadRun,sjis);
        g_glyphLastWidth = 16;
        break;
    default:
        Font_ExpandAscii(dst,g_glyphUploadRun,sjis);
        g_glyphLastWidth = 8;
        break;
    }
    cell = g_glyphCacheNext + g_glyphUploadCount;
    g_glyphUploadCount++;
    if (g_glyphUploadCount == g_glyphUploadRun)
    {
        Font_FlushUploads();
        g_glyphUploadCount = 0;
        g_glyphUploadRun = 0;
    }
    return cell;
}

/* Upload the pending glyph cells to VRAM and mark them used (0x8001DE80) */
/* 0x8001DE80 */
void Font_FlushUploads(void)
{
    short i;
    RECT rect;

    if ((g_glyphUploadCount == 0) || (g_glyphUploadRun == 0))
    {
        return;
    }
    /* the cells sit 16 per row of 64 VRAM units from x 0x340, 16 rows from y 0x100 */
    rect.x = g_glyphCacheNext % 16 * 4 + g_glyphCacheNext / 256 * 64 + 0x340;
    rect.y = g_glyphCacheNext % 256 / 16 * 16 + 0x100;
    rect.w = g_glyphUploadRun * 4;
    rect.h = 16;
    LoadImage(&rect,(u_long *)g_glyphUploadPtr);
    g_glyphUploadPtr += g_glyphUploadRun * 0x100;
    for (i = 0; i < g_glyphUploadCount; i++)
    {
        g_glyphCacheUsed[g_glyphCacheNext / 16] |= g_bitMask32[g_glyphCacheNext % 16];
        if (g_glyphCacheNext == 0x1ff)
        {
            g_glyphCacheNext = 0;
        }
        else
        {
            g_glyphCacheNext++;
        }
    }
    g_glyphUploadRun = 0;
    g_glyphUploadCount = 0;
}

/* Mark the n cells after the reserved ones used (the original marks reserved + 1 to reserved + n;
 * inline in Font_CacheDigits and Font_CacheKana) */
static void Font_MarkReserved(int n)
{
    int r;
    short c;

    for (r = g_glyphCacheReserved; r < g_glyphCacheReserved + n; r++)
    {
        c = r + 1;
        g_glyphCacheUsed[c / 16] |= g_bitMask32[c % 16];
    }
}

/* Cache the 11 glyphs of g_fontDigitsSjis (full-width 0-9 and minus) in reserved cells (0x8001E5FC) */
/* 0x8001E5FC */
void Font_CacheDigits(void)
{
    short i;

    g_fontDigitsCached = 0;
    g_fontDigitsBase = Font_CacheGlyph(g_fontDigitsSjis);
    for (i = 2; i < 22; i += 2)
    {
        Font_CacheGlyph(g_fontDigitsSjis + i);
    }
    Font_FlushUploads();
    DrawSync(0);
    g_glyphUploadPtr = g_glyphUploadBuf;
    Font_MarkReserved(11);
    g_fontDigitsCached = 1;
    g_glyphCacheReserved += 11;
}

/* Cache the 5 glyphs of g_fontKanaSjis in reserved cells (a Japanese 'steps left' message; kept
 * from the Japanese version) (0x8001E71C) */
/* 0x8001E71C */
void Font_CacheKana(void)
{
    short i;

    g_fontKanaCached = 0;
    g_fontKanaBase = Font_CacheGlyph(g_fontKanaSjis);
    for (i = 2; i < 10; i += 2)
    {
        Font_CacheGlyph(g_fontKanaSjis + i);
    }
    Font_FlushUploads();
    DrawSync(0);
    g_glyphUploadPtr = g_glyphUploadBuf;
    Font_MarkReserved(5);
    g_fontKanaCached = 1;
    g_glyphCacheReserved += 5;
}

/* Draw glyph-cache cell `cell` as a 16x16 sprite at x, y with palette pal (0x8001E83C) */
/* 0x8001E83C */
void Font_DrawCell(short cell, short x, short y, int pal, GsOT *ot, u16 pri)
{
    short u;
    short v;

    u = cell % 16 * 4 + cell / 256 * 64 + 0x340;
    v = cell % 256 / 16 * 16 + 0x100;
    g_sprite.attribute = 0x8000000;
    g_sprite.w = 16;
    g_sprite.h = 16;
    g_sprite.tpage = u / 64 + v / 256 * 16;
    g_sprite.u = (u & 0x3c) * 4;
    g_sprite.cx = (short)pal * 16;
    g_sprite.v = v;
    g_sprite.cy = 0x1ff;
    g_sprite.r = g_tintR;
    g_sprite.g = g_tintG;
    g_sprite.b = g_tintB;
    g_sprite.x = x;
    g_sprite.y = y;
    GsSortFastSprite(&g_sprite,ot,pri);
}

/* Select the font data: kanji and half-width glyph bitmaps, glyph size and bit depth (0x8001EBFC) */
/* 0x8001EBFC */
void Font_SetSource(u8 *kanji, u8 *ascii, int w, int h, int depth, u16 fullJis)
{
    g_fontKanji = kanji;
    g_fontAscii = ascii;
    g_fontGlyphW = w;
    g_fontGlyphH = h;
    g_fontDepth = depth;
    g_fontFullJis = fullJis;
    g_fontGlyphBytes = (w * h * depth + 7U >> 3);
}

/* Expand a 8x16 1bpp half-width glyph into 4bpp pixels for the glyph upload buffer (0x8001EC44) */
/* 0x8001EC44 */
void Font_ExpandAscii(u8 *dst, int rowCells, u8 *ch)
{
    u8 *src;
    int row;

    src = g_fontAscii + (*ch * (u16)g_fontGlyphBytes) / 2;
    for (row = 0; row < 16; row++)
    {
        dst[0] = (*src >> 7) | (*src >> 2 & 0x10);
        dst[1] = (*src >> 5 & 1) | (*src & 0x10);
        dst[2] = (*src >> 3 & 1) | (*src & 4) << 2;
        dst[3] = (*src >> 1 & 1) | (*src & 1) << 4;
        dst += rowCells * 8;
        src++;
    }
}

/* Glyph index in the kanji font of a JIS X 0208 code (94 per row from 0x2121; the rows of the
 * gap before the level 1 kanji are not in the font) (0x8001ED2C) */
/* 0x8001ED2C */
int Font_JisToIndex(u32 jis)
{
    short index;

    index = (((jis & 0xffff) >> 8) - 0x21) * 94 + (jis & 0xff) - 0x21;
    if (0x1000 < index)
    {
        index -= 0xd40;
    }
    return index;
}

/* Shift-JIS to JIS X 0208; codes past the font (level 2 kanji) give 0x217A unless g_fontFullJis
 * (0x8001ED80) */
/* 0x8001ED80 */
short Font_SjisToJis(u8 *sjis)
{
    u16 first;
    u16 second;
    u16 hi;
    short row;

    if ((g_fontFullJis == 0) && (0x988f < (sjis[0] << 8 | sjis[1])))
    {
        return 0x217a;
    }
    first = sjis[0];
    second = sjis[1];
    row = first < 0xa0 ? first - 0x71 : first - 0xb1;
    hi = row * 2 | 1;
    if (0x7f < second)
    {
        second--;
    }
    if (second < 0x9e)
    {
        return (short)(second - 0x1f) + hi * 0x100;
    }
    hi = row * 2 + 2;
    return (short)(second - 0x7d) + hi * 0x100;
}

/* Reset the glyph cache and text nodes, load the font CLUT, select the default 16x16 1bpp font
 * (0x8001EE24) */
/* 0x8001EE24 */
void Font_Init(u8 *kanji, u8 *ascii, u8 *uploadBuf)
{
    int i;
    RECT rect;

    rect.y = 0x1ff;
    rect.w = 0x100;
    rect.x = 0;
    rect.h = 1;
    LoadImage(&rect,(u_long *)g_fontClut);
    for (i = 0; i < 32; i++)
    {
        g_glyphCacheUsed[i] = 0;
    }
    g_glyphCacheNext = 0;
    g_glyphUploadRun = 0;
    g_glyphCacheReserved = -1;
    for (i = 0; i < 512; i++)
    {
        g_glyphNodes[i].next = -1;
    }
    g_fontAscii = ascii;
    g_fontKanji = kanji;
    g_glyphUploadBuf = uploadBuf;
    g_glyphUploadPtr = uploadBuf;
    g_fontGlyphW = 16;
    g_fontGlyphH = 16;
    g_fontDepth = 1;
    g_glyphNodeNext = 0;
    g_textAlign = 0;
    g_textPalette = 0;
    g_fontFullJis = 0;
    g_fontGlyphBytes = 0x20;
    g_fontDigitsCached = 0;
}

/* Mark the next n cells after the reserved ones as used, and reserve them (0x8001EF54) */
/* 0x8001EF54 */
void Font_ReserveCells(short n)
{
    Font_MarkReserved(n);
    g_glyphCacheReserved += n;
}

/* Debug: print one word of the glyph-cache bitmap (0x8001F248) */
/* 0x8001F248 */
void Font_DebugPrintCacheWord(short id)
{
    FntPrint("FontFl[%d] : %04X\n", id, g_glyphCacheUsed[id]);
}
