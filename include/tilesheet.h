/*
 * tilesheet.h - tile sheets
 *
 * Pictures cut into 8x8 / 16x16 cells kept in VRAM and drawn cell by cell (portraits, scene
 * images).
 * The declarations of src/tilesheet.c: its types and its functions.
 * See docs/graphics.md.
 */

#ifndef TILESHEET_H
#define TILESHEET_H

#include "common.h"
#include "psyq.h"

/* A frame of a tile sheet: an image cut into 16x16 cells stored one after the other in a
 * VRAM area (15 cells per 64-unit row, 225 per column of rows). */
typedef struct TileFrame
{
    short cell;                  /* 0x00 first cell */
    short mode;                  /* 0x02 0 = 4bpp, 1 = 8bpp (TIM pmode & 3) */
    short w, h;                  /* 0x04 size in cells */
    short clutX, clutY;          /* 0x08 */
    short small;                 /* 0x0C nonzero: 8x8 cells instead of 16x16 (TileSheet_DrawAligned) */
    short unused0E;              /* 0x0E 0 (a VramImage has its height here, see Script_OpGetImageSize) */
} TileFrame;                     /* size 0x10 */

/* A tile sheet (g_sceneSheet, g_portraitSheet): frames loaded from TIMs by
 * TileSheet_LoadTims / TileSheet_AddImage, staged in RAM and uploaded by TileSheet_Flush. */
typedef struct TileSheet
{
    TileFrame *frames;           /* 0x00 */
    unsigned char *stage;        /* 0x04 staging buffer (one 60x240 VRAM column) */
    unsigned char *pixels;       /* 0x08 image being cut */
    short vramX, vramY;          /* 0x0C cell area */
    short nextCell;              /* 0x10 */
    short copyCol;               /* 0x12 cells of the current row copied so far */
    short copyRow;               /* 0x14 */
    unsigned char full;          /* 0x16 staging buffer full: flush and call again */
    signed char count;           /* 0x17 frames */
} TileSheet;                     /* size 0x18 */

/* ---- Functions ---- */

/* src/tilesheet.c */
u32 TileSheet_AddImage(TileSheet *sheet, int pixels, u16 w, u16 h); /* 0x8002FF18 */
void TileSheet_CopyCells(TileSheet *sheet, u8 *pixels, int n); /* 0x80030164 */
u32 * TileSheet_LoadTims(TileSheet *sheet, u32 *data, char upload, short *offset); /* 0x800302C8 */
void TileSheet_DrawAligned(TileSheet *set, u32 idx, u32 attr, short *pos, u32 align, GsOT *ot, int pri); /* 0x800305D4 */
void TileSheet_Draw4_16(TileSheet *sheet, u32 idx, u32 attr, short *pos, GsOT *ot, int pri); /* 0x800307F8 */
void TileSheet_Draw8_16(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri); /* 0x80030B98 */
void TileSheet_Draw4_8(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri); /* 0x80030F04 */
void TileSheet_Draw8_8(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri); /* 0x80031284 */
void TileSheet_DrawRotated(TileSheet *set, u16 idx, u32 attr, short *pos, u32 align, u32 angle, GsOT *ot, int pri); /* 0x800315C8 */
void TileSheet_DrawSquashed(TileSheet *sheet, u16 idx, u32 attr, short *pos, u32 align, short height, GsOT *ot, int pri); /* 0x800321A4 */
void TileSheet_Flush(TileSheet *sheet);                   /* 0x80032850 */

#endif /* TILESHEET_H */
