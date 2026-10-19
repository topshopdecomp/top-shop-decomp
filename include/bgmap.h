/*
 * bgmap.h - tile-map background layers
 *
 * Tile-map background layers (the board's two far planes).
 * The declarations of src/bgmap.c: its types and constants, its globals and its functions.
 * See docs/graphics.md.
 */

#ifndef BGMAP_H
#define BGMAP_H

#include "common.h"
#include "psyq.h"

/* A tile of a tile map (BgMap.tiles), 8 bytes. */
typedef struct BgTile
{
    unsigned char u, v;          /* 0x00 texture position */
    unsigned short clut;         /* 0x02 */
    unsigned short type;         /* 0x04 BGTILE_*: how the texture is mapped */
    unsigned short tpage;        /* 0x06 low 5 bits: texture page (or'ed with BgMap.tpageBase) */
} BgTile;

#define BGTILE_SPRITE 0          /* drawn as a SPRT */
#define BGTILE_FLIPV  1          /* POLY_FT4, texture flipped vertically */
#define BGTILE_FLIPH  2          /* POLY_FT4, flipped horizontally */
#define BGTILE_FLIPHV 3          /* POLY_FT4, flipped both ways */
#define BGTILE_EMPTY  0xFFFF

/* A visible tile, ready to draw (BgMap.visible[row * cols + col]), 16 bytes. */
typedef struct BgCell
{
    unsigned short clut;         /* 0x00 */
    unsigned short tpage;        /* 0x02 */
    unsigned char v[4];          /* 0x04 texture v of the four corners */
    unsigned char u[4];          /* 0x08 texture u of the four corners */
    int type;                    /* 0x0C BgTile.type, BGTILE_EMPTY = nothing */
} BgCell;

/* A scrolling tile-map layer (board backgrounds). Filled from a map entry of a
 * STG*.J archive, then prepared by BgMap_Build (0x80023E10). */
typedef struct BgMap
{
    unsigned char mapW, mapH;    /* 0x00 map size in tiles */
    unsigned char tileW, tileH;  /* 0x02 tile size in pixels */
    unsigned short *cells;       /* 0x04 [y * mapW + x] -> index into tiles, 0xFFFF = empty */
    BgTile *tiles;               /* 0x08 */
    BgCell *visible;             /* 0x0C cols * rows visible tiles */
    void *prims[2];              /* 0x10 primitive buffers set by Board_LoadData, not used by the tile-map code */
    GsOT *ot[2];                 /* 0x18 one per frame buffer */
    int scrollX, scrollY;        /* 0x20 20.12 fixed point, wraps at the map size */
    unsigned short tpageBase;    /* 0x28 or'ed into BgTile.tpage (0x80) */
    short cols, rows;            /* 0x2A tiles needed to cover 320x240 (+1 partial, +1 scroll) */
    short col, row;              /* 0x2E map position of the top-left visible tile */
    unsigned short buf;          /* 0x32 bit 0: which ordering table is built next */
} BgMap;                         /* size 0x34 */

/* A tile-map layer with its two ordering tables (0x80111D60, 0x80111DC0). */
typedef struct BgPlane
{
    GsOT ot[2];                  /* 0x00 */
    BgMap map;                   /* 0x28 */
    unsigned char pad5C[4];
} BgPlane;                       /* size 0x60 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 g_bgPrimCols;  /* 0x800B2398 */
extern u8 g_bgPrimRows;  /* 0x800B2399 */
extern u16 g_bgSpanY;    /* 0x800B23A0 */
extern u16 g_bgSpanX;    /* 0x800B23A2 */

/* ---- Functions ---- */

/* src/bgmap.c */
void BgMap_Draw(BgMap *map, GsOT *ot, short pri);         /* 0x80021E08 */
void BgMap_BuildPrims(BgMap *map);                        /* 0x80021FD4 */
void BgMap_FillRow(BgMap *map, BgCell *dst, u32 col, u32 row); /* 0x800224C0 */
void BgMap_FillColumn(BgMap *map, BgCell *dst, u32 col, u8 row); /* 0x8002285C */
void BgMap_Scroll(BgMap *map, int dx, int dy);            /* 0x80022C18 */
void BgMap_Build(BgMap *map);                             /* 0x80023E10 */

#endif /* BGMAP_H */
