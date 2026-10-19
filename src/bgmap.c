/*
 * bgmap.c - tile-map background layers
 *
 * The board's two far background planes are tile maps (from the stage's STG<stage>.J). For each
 * plane this keeps the visible part as a grid of tiles, refills the rows and columns that scroll
 * into view (the map wraps around), and builds one sprite or textured quad per tile into the
 * plane's own ordering table every frame.
 */
#include "common.h"
#include "topshop.h"

/* Add a tile-map layer to ot: its ordering table, framed by draw-offset packets that apply the
 * sub-tile scroll (0x80021E08) */
/* 0x80021E08 */
void BgMap_Draw(BgMap *map, GsOT *ot, short pri)
{
    GsOT *mapOt;
    u_short originX;
    short originY;
    u_short ofs[2];

    mapOt = map->ot[map->buf & 1];
    originX = g_bufferOrigin[g_otIndex * 2];
    originY = g_bufferOrigin[g_otIndex * 2 + 1];
    /* the layer is built at whole tiles: shift it by the part of a tile scrolled, then back
     * (the packet added last is drawn first) */
    ofs[0] = originX - ((map->scrollX >> 12) % map->tileW);
    ofs[1] = originY - ((map->scrollY >> 12) % map->tileH);
    SetDrawOffset(g_primPtr,ofs);
    AddPrim(mapOt->org + 1,g_primPtr);
    g_primPtr = (DR_TWIN *)g_primPtr + 1;
    ofs[0] = originX;
    ofs[1] = originY;
    SetDrawOffset(g_primPtr,ofs);
    AddPrim(mapOt->org,g_primPtr);
    mapOt->point = pri;
    g_primPtr = (DR_TWIN *)g_primPtr + 1;
    GsSortOt(mapOt,ot);
}

/* Rebuild the layer's ordering table for the current buffer: one SPRT or POLY_FT4 per visible tile,
 * with draw-mode packets when the texture page changes (0x80021FD4) */
/* 0x80021FD4 */
void BgMap_BuildPrims(BgMap *map)
{
    BgCell *cell;
    GsOT_TAG *ot;
    POLY_FT4 *poly;
    SPRT *sprt;
    void *prim;
    u32 tpage;
    short x;
    short y;

    /* the texture page the sprites added since the last draw-mode packet use; 0x8000 = none. An
     * ordering table slot draws what was added last first, so a draw-mode packet added after a
     * run of sprites sets their page. */
    tpage = 0x8000;
    GsClearOt(0,0,map->ot[map->buf & 1]);
    g_bgPrimCols = map->tileW + 1;
    cell = map->visible;
    g_bgPrimRows = map->tileH + 1;
    g_bgSpanY = map->tileH + 240;
    g_bgSpanX = map->tileW + 320;
    ot = map->ot[map->buf & 1]->org + 1;
    prim = g_primPtr;
    for (y = 0; y < g_bgSpanY; y += map->tileH)
    {
        for (x = 0; x < g_bgSpanX; x += map->tileW)
        {
            if (((tpage & 0x8000) == 0) && (tpage != cell->tpage) && (cell->type != BGTILE_EMPTY))
            {
                SetDrawMode(prim,0,0,tpage,NULL);
                AddPrim(ot,prim);
                prim = (DR_MODE *)prim + 1;
            }
            switch (cell->type)
            {
            case BGTILE_SPRITE:
                sprt = prim;
                SetSprt(sprt);
                tpage = cell->tpage;
                sprt->r0 = g_tintR;
                sprt->g0 = g_tintG;
                sprt->b0 = g_tintB;
                sprt->x0 = x;
                sprt->y0 = y;
                sprt->clut = cell->clut;
                sprt->u0 = cell->u[0];
                sprt->v0 = cell->v[0];
                sprt->w = map->tileW;
                sprt->h = map->tileH;
                AddPrim(ot,sprt);
                prim = sprt + 1;
                break;
            case BGTILE_FLIPV:
            case BGTILE_FLIPH:
            case BGTILE_FLIPHV:
                poly = prim;
                SetPolyFT4(poly);
                poly->r0 = g_tintR;
                poly->g0 = g_tintG;
                poly->b0 = g_tintB;
                poly->clut = cell->clut;
                poly->tpage = cell->tpage;
                if (cell->type == BGTILE_FLIPV)
                {
                    poly->x0 = x;
                    poly->y0 = y + map->tileH;
                    poly->x1 = x + map->tileW;
                    poly->y1 = y + map->tileH;
                    poly->x2 = x;
                    poly->y2 = y;
                    poly->x3 = x + map->tileW;
                    poly->y3 = y;
                }
                else if (cell->type == BGTILE_FLIPH)
                {
                    poly->x0 = x + map->tileW;
                    poly->y0 = y;
                    poly->x1 = x;
                    poly->y1 = y;
                    poly->x2 = x + map->tileW;
                    poly->y2 = y + map->tileH;
                    poly->x3 = x;
                    poly->y3 = y + map->tileH;
                }
                else
                {
                    poly->x0 = x + map->tileW;
                    poly->y0 = y + map->tileH;
                    poly->x1 = x;
                    poly->y1 = y + map->tileH;
                    poly->x2 = x + map->tileW;
                    poly->y2 = y;
                    poly->x3 = x;
                    poly->y3 = y;
                }
                poly->u0 = cell->u[0];
                poly->v0 = cell->v[0];
                poly->u1 = cell->u[1];
                poly->v1 = cell->v[1];
                poly->u2 = cell->u[2];
                poly->v2 = cell->v[2];
                poly->u3 = cell->u[3];
                poly->v3 = cell->v[3];
                tpage = 0x8000;
                AddPrim(ot,poly);
                prim = poly + 1;
                break;
            }
            cell++;
        }
    }
    if ((tpage & 0x8000) == 0)
    {
        SetDrawMode(prim,0,0,tpage,NULL);
        AddPrim(ot,prim);
        prim = (DR_MODE *)prim + 1;
    }
    g_primPtr = prim;
}

/* One visible tile from map tile index tile (0xFFFF = empty): CLUT, texture page and the texture
 * corners by its mapping (BGTILE_*). The flipped ones start one texel early and may cross into
 * the neighboring 128-texel page. g_bgSpanY / g_bgSpanX hold the tile size - 1. (Inline in
 * BgMap_FillRow and BgMap_FillColumn in the original.) */
static void BgMap_SetCell(BgMap *map, BgCell *c, u16 tile)
{
    BgTile *t;
    u8 u;
    u8 v;

    if (tile == 0xffff)
    {
        c->type = BGTILE_EMPTY;
        return;
    }
    t = &map->tiles[tile];
    c->type = t->type;
    c->clut = t->clut;
    c->tpage = map->tpageBase | (t->tpage & 0x1f);
    u = t->u;
    v = t->v;
    switch (c->type)
    {
    case BGTILE_SPRITE:
        c->u[0] = u;
        c->v[0] = v;
        break;
    case BGTILE_FLIPV:
        c->u[0] = u;
        c->v[0] = v - 1;
        c->u[1] = u + map->tileW;
        c->v[1] = v - 1;
        c->u[2] = u;
        c->v[2] = v + g_bgSpanX;
        c->u[3] = u + map->tileW;
        c->v[3] = v + g_bgSpanX;
        if (0xff < t->u + map->tileW)
        {
            c->tpage++;
            c->u[0] -= 0x80;
            c->u[1] -= 0x80;
            c->u[2] -= 0x80;
            c->u[3] -= 0x80;
        }
        break;
    case BGTILE_FLIPH:
    case BGTILE_FLIPHV:
        c->u[0] = u - 1;
        c->u[1] = u + g_bgSpanY;
        c->u[2] = u - 1;
        c->u[3] = u + g_bgSpanY;
        if (c->type == BGTILE_FLIPH)
        {
            c->v[0] = v;
            c->v[1] = v;
            c->v[2] = v + map->tileH;
            c->v[3] = v + map->tileH;
        }
        else
        {
            c->v[0] = v - 1;
            c->v[1] = v - 1;
            c->v[2] = v + g_bgSpanX;
            c->v[3] = v + g_bgSpanX;
        }
        if (t->u - 1 < 1)
        {
            c->tpage--;
            c->u[0] -= 0x80;
            c->u[1] -= 0x80;
            c->u[2] -= 0x80;
            c->u[3] -= 0x80;
        }
        break;
    }
}

/* Fill a row of visible tiles from map position col, row (wrapping around the map) (0x800224C0) */
/* 0x800224C0 */
void BgMap_FillRow(BgMap *map, BgCell *dst, u32 col, u32 row)
{
    u16 *cell;
    int i;

    g_bgSpanY = map->tileW - 1;
    g_bgSpanX = map->tileH - 1;
    cell = map->cells + (row & 0xff) * map->mapW + (col & 0xff);
    for (i = 0; i < map->cols; i++)
    {
        BgMap_SetCell(map,&dst[(short)i],*cell);
        col++;
        cell++;
        if ((col & 0xff) == map->mapW)
        {
            col = 0;
            cell -= map->mapW;
        }
    }
}

/* Fill a column of visible tiles from map position col, row (wrapping around the map) (0x8002285C) */
/* 0x8002285C */
void BgMap_FillColumn(BgMap *map, BgCell *dst, u32 col, u8 row)
{
    u16 *cell;
    u16 n;
    int i;

    g_bgSpanY = map->tileW - 1;
    g_bgSpanX = map->tileH - 1;
    n = map->rows * map->cols;
    cell = map->cells + row * map->mapW + (col & 0xff);
    for (i = 0; (short)i < n; i += map->cols)
    {
        BgMap_SetCell(map,&dst[(short)i],*cell);
        row++;
        cell += map->mapW;
        if (row == map->mapH)
        {
            row = 0;
            cell = map->cells + (col & 0xff);
        }
    }
}

/* Move the visible tiles of a layer by whole cells: cell [r][c] takes the one at [r + dr][c + dc];
 * the cells with nothing to take are left for BgMap_FillRow / BgMap_FillColumn. (Inline, once
 * per direction, in BgMap_Scroll in the original.) */
static void BgMap_ShiftCells(BgMap *map, int dr, int dc)
{
    BgCell *dst;
    BgCell *src;
    int n;
    int k;
    int i;
    int r;
    int c;

    n = map->rows * map->cols;
    for (k = 0; k < n; k++)
    {
        /* in the direction that reads every cell before it is overwritten */
        i = 0 < dr * map->cols + dc ? k : n - 1 - k;
        r = i / map->cols;
        c = i % map->cols;
        if ((r + dr < 0) || (map->rows <= r + dr) || (c + dc < 0) || (map->cols <= c + dc))
        {
            continue;
        }
        dst = &map->visible[i];
        src = &map->visible[(r + dr) * map->cols + c + dc];
        dst->type = src->type;
        if ((src->type < 0) || (BGTILE_FLIPHV < src->type))
        {
            continue;                       /* empty: only the type */
        }
        if (src->type != BGTILE_SPRITE)
        {
            dst->v[1] = src->v[1];
            dst->u[1] = src->u[1];
            dst->v[2] = src->v[2];
            dst->u[2] = src->u[2];
            dst->v[3] = src->v[3];
            dst->u[3] = src->u[3];
        }
        dst->clut = src->clut;
        dst->tpage = src->tpage;
        dst->v[0] = src->v[0];
        dst->u[0] = src->u[0];
    }
}

/* BgMap_Scroll's `moved`: the cells the map scrolled by, horizontally (bits 0-1) and
 * vertically (bits 4-5) */
#define SCROLL_LEFT   0x01
#define SCROLL_RIGHT  0x03
#define SCROLL_UP     0x10
#define SCROLL_DOWN   0x30

/* Scroll a tile-map layer by dx, dy (20.12): refill the rows/columns of visible tiles that changed
 * (all of them after a jump of a tile or more), then rebuild its ordering table (0x80022C18) */
/* 0x80022C18 */
void BgMap_Scroll(BgMap *map, int dx, int dy)
{
    int width;
    int height;
    int col;
    int row;
    int moved;
    int i;

    /* the new position, wrapped around the map */
    width = map->mapW * map->tileW * 0x1000;
    map->scrollX += dx;
    if (width <= map->scrollX)
    {
        map->scrollX -= width;
    }
    else if (map->scrollX < 0)
    {
        map->scrollX += width;
    }
    height = map->mapH * map->tileH * 0x1000;
    map->scrollY += dy;
    if (height <= map->scrollY)
    {
        map->scrollY -= height;
    }
    else if (map->scrollY < 0)
    {
        map->scrollY += height;
    }

    if ((map->tileW <= (dx < 0 ? -dx : dx) >> 12) || (map->tileH <= (dy < 0 ? -dy : dy) >> 12))
    {
        /* a jump of a tile or more: every column again */
        col = (map->scrollX >> 12) / map->tileW;
        row = (map->scrollY >> 12) / map->tileH;
        map->col = col;
        map->row = row;
        for (i = 0; i < map->cols; i++)
        {
            BgMap_FillColumn(map,&map->visible[i],col & 0xff,row);
            col++;
            if (col == map->mapW)
            {
                col = 0;
            }
        }
    }
    else
    {
        /* at most one tile: move the cells over and fill the new edge row / column */
        moved = 0;
        col = (map->scrollX >> 12) / map->tileW % map->mapW;
        if (map->col != col)
        {
            map->col = col;
            moved = dx < 0 ? SCROLL_LEFT : SCROLL_RIGHT;
        }
        row = (map->scrollY >> 12) / map->tileH % map->mapH;
        if (map->row != row)
        {
            map->row = row;
            moved += dy < 0 ? SCROLL_UP : SCROLL_DOWN;
        }
        switch (moved)
        {
        case SCROLL_LEFT:
            BgMap_ShiftCells(map,0,-1);
            BgMap_FillColumn(map,map->visible,(u8)map->col,map->row);
            break;
        case SCROLL_RIGHT:
            BgMap_ShiftCells(map,0,1);
            BgMap_FillColumn(map,&map->visible[map->cols - 1],(map->col + map->cols - 1) % map->mapW & 0xff,
                             map->row);
            break;
        case SCROLL_UP:
            BgMap_ShiftCells(map,-1,0);
            BgMap_FillRow(map,map->visible,(u8)map->col,(u8)map->row);
            break;
        case SCROLL_UP | SCROLL_LEFT:
            BgMap_ShiftCells(map,-1,-1);
            BgMap_FillColumn(map,map->visible,(u8)map->col,map->row);
            BgMap_FillRow(map,map->visible,(u8)map->col,(u8)map->row);
            break;
        case SCROLL_UP | SCROLL_RIGHT:
            BgMap_ShiftCells(map,-1,1);
            BgMap_FillRow(map,map->visible,(u8)map->col,(u8)map->row);
            BgMap_FillColumn(map,&map->visible[map->cols - 1],(map->col + map->cols - 1) % map->mapW & 0xff,
                             map->row);
            break;
        case SCROLL_DOWN:
            BgMap_ShiftCells(map,1,0);
            BgMap_FillRow(map,&map->visible[(map->rows - 1) * map->cols],(u8)map->col,
                          (map->row + map->rows - 1) % map->mapH & 0xff);
            break;
        case SCROLL_DOWN | SCROLL_LEFT:
            BgMap_ShiftCells(map,1,-1);
            BgMap_FillColumn(map,map->visible,(u8)map->col,map->row);
            BgMap_FillRow(map,&map->visible[(map->rows - 1) * map->cols],(u8)map->col,
                          (map->row + map->rows - 1) % map->mapH & 0xff);
            break;
        case SCROLL_DOWN | SCROLL_RIGHT:
            BgMap_ShiftCells(map,1,1);
            BgMap_FillRow(map,&map->visible[(map->rows - 1) * map->cols],(u8)map->col,
                          (map->row + map->rows - 1) % map->mapH & 0xff);
            BgMap_FillColumn(map,&map->visible[map->cols - 1],(map->col + map->cols - 1) % map->mapW & 0xff,
                             map->row);
            break;
        }
    }
    map->buf ^= 1;                      /* build the other ordering table */
    BgMap_BuildPrims(map);
}

/* Fill all visible tiles from the scroll position and build both ordering tables (0x80023E10) */
/* 0x80023E10 */
void BgMap_Build(BgMap *map)
{
    int col;
    int row;
    int c;
    int i;

    col = (map->scrollX >> 12) / map->tileW;
    row = (map->scrollY >> 12) / map->tileH;
    for (i = 0; i < map->cols; i++)
    {
        c = col & 0xff;
        BgMap_FillColumn(map,&map->visible[i],c,row);
        col++;
        if (c + 1 == map->mapW)
        {
            col = 0;
        }
    }
    map->buf &= ~1;
    BgMap_BuildPrims(map);
    map->buf |= 1;
    BgMap_BuildPrims(map);
}
