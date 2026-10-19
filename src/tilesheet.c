/*
 * tilesheet.c - tile sheets
 *
 * Pictures cut into 8x8 or 16x16 cells and kept in VRAM in columns of cells: loading TIMs into a
 * sheet, and drawing a picture strip by strip of cells, aligned, rotated and scaled, or squashed
 * (the portrait folding). The scripts' scene images and the portraits are tile sheets.
 */
#include "common.h"
#include "topshop.h"

/* Copy an image (w x h cells) into the sheet's staging buffer, each row's cells in runs that fit
 * the buffer's rows of 15; when the buffer is full (225 cells), upload it (TileSheet_Flush) and
 * return 1 to be called again (with sheet->full set) for the rest (0x8002FF18) */
/* 0x8002FF18 */
u32 TileSheet_AddImage(TileSheet *sheet, int pixels, u16 w, u16 h)
{
    TileFrame *frame;
    short run;

    if (sheet->full == 0)
    {
        sheet->pixels = (u8 *)pixels;
        sheet->copyCol = 0;
        sheet->copyRow = 0;
        frame = &sheet->frames[sheet->count];
        frame->small = 0;
        frame->w = w;
        frame->h = h;
        frame->cell = sheet->nextCell;
    }
    frame = &sheet->frames[sheet->count];
    for (; sheet->copyRow < frame->h; sheet->copyRow++)
    {
        while (sheet->copyCol < frame->w)
        {
            run = frame->w - sheet->copyCol;
            if (15 - sheet->nextCell % 15 < run)
            {
                run = 15 - sheet->nextCell % 15;
            }
            TileSheet_CopyCells(sheet,sheet->pixels,run);
            sheet->copyCol += run;
            if (sheet->nextCell % 225 == 0)
            {
                sheet->full = 1;
                TileSheet_Flush(sheet);
                return 1;
            }
        }
        sheet->copyCol = 0;
    }
    sheet->full = 0;
    sheet->count++;
    return 0;
}

/* Copy n 16x16 cells of the current image row into the staging buffer (0x80030164) */
/* 0x80030164 */
void TileSheet_CopyCells(TileSheet *sheet, u8 *pixels, int n)
{
    short cell;
    int w;
    u8 *dst;
    u8 *src;
    int row;

    /* the staging buffer holds 15 x 15 cells of 8 bytes x 16 rows; image rows are w * 8 bytes */
    cell = sheet->nextCell;
    w = sheet->frames[sheet->count].w;
    sheet->nextCell = cell + n;
    src = pixels + sheet->copyRow * w * 0x80 + sheet->copyCol * 8;
    dst = sheet->stage + (cell % 15) * 8 + ((cell % 225) / 15) * 0x780;
    for (row = 0; row < 16; row++)
    {
        Mem_Copy(dst,src,(short)(n * 8));
        src += w * 8;
        dst += 0x78;
    }
}

/* Tags of the images in a tile sheet stream */
#define TIM_CELLS  0x10   /* a TIM cut into the sheet's cells */
#define TIM_WHOLE  0x11   /* a TIM uploaded as it is */

/* Load a stream of tagged TIMs into a tile sheet (TIM_CELLS, TIM_WHOLE; anything else ends it).
 * offset (or none): x, y added to the TIMs' VRAM positions, then the CLUT given to images without
 * one. With upload 0 nothing is uploaded and the CLUT stays. Returns where to continue when the
 * staging buffer filled up (call again), NULL at the end (0x800302C8) */
/* 0x800302C8 */
u32 * TileSheet_LoadTims(TileSheet *sheet, u32 *data, char upload, short *offset)
{
    short dx;
    short dy;
    short clutX;
    short clutY;
    u32 *tim;

    dx = (offset != NULL) ? offset[0] : 0;
    dy = (offset != NULL) ? offset[1] : 0;
    if (sheet->full == 0)
    {
        clutX = (offset != NULL) ? offset[2] : 0;
        clutY = (offset != NULL) ? offset[3] : 0;
    }
    else
    {
        /* the rest of the image the staging buffer stopped */
        clutX = sheet->frames[sheet->count].clutX;
        clutY = sheet->frames[sheet->count].clutY;
        TileSheet_AddImage(sheet,0,0,0);
    }
    for (;;)
    {
        if (*data == TIM_CELLS)
        {
            tim = data + 1;
            GsGetTimInfo((u_long *)tim,&g_timInfo);
            sheet->frames[sheet->count].mode = g_timInfo.pmode & 3;
            if (g_timInfo.pmode & 8)
            {
                /* its CLUT */
                if (upload != 0)
                {
                    clutX = g_timInfo.cx + dx;
                    clutY = g_timInfo.cy + dy;
                    g_timInfo.cx = clutX;
                    g_timInfo.cy = clutY;
                    LoadImage((RECT *)&g_timInfo.cx,g_timInfo.clut);
                }
                tim += g_timInfo.cw * g_timInfo.ch / 2 + 3;       /* 3 words of header, then the entries */
            }
            if (upload != 0)
            {
                sheet->frames[sheet->count].clutX = clutX;
                sheet->frames[sheet->count].clutY = clutY;
            }
            data = tim + g_timInfo.pw * g_timInfo.ph / 2 + 4;  /* the pixel block: 4 words of header */
            if (TileSheet_AddImage(sheet,(int)g_timInfo.pixel,g_timInfo.pw >> 2,g_timInfo.ph >> 4) != 0)
            {
                return data;
            }
        }
        else if (*data == TIM_WHOLE)
        {
            data++;
            GsGetTimInfo((u_long *)data,&g_timInfo);
            if (g_timInfo.pw != 0)
            {
                if (upload != 0)
                {
                    /* (the position goes through clutX, clutY: an image without a CLUT after it gets it) */
                    clutX = g_timInfo.px + dx;
                    clutY = g_timInfo.py + dy;
                    g_timInfo.px = clutX;
                    g_timInfo.py = clutY;
                    LoadImage((RECT *)&g_timInfo.px,g_timInfo.pixel);
                }
                data += g_timInfo.pw * g_timInfo.ph / 2 + 4;
            }
        }
        else
        {
            return NULL;
        }
    }
}

/* Screen position of a strip of n cells from column col of row `row` of a w x h cell frame drawn at
 * origin, cells of `size` pixels, mirrored for GFX_FLIP_X / GFX_FLIP_Y */
static void TileSheet_StripPos(short *out, short *origin, u32 attr, int col, int n, int row, short w, short h, int size)
{
    out[0] = origin[0] + ((attr & GFX_FLIP_X) ? w - col - n : col) * size;
    out[1] = origin[1] + ((attr & GFX_FLIP_Y) ? h - row - 1 : row) * size;
}

/* 0x800305D4 */
/* Draw element idx of a frame set at pos, aligned by align (ALIGN_*: bits 0-1 horizontal,
 * 0 centered, 1 ending at pos (ALIGN_RIGHT), 2 starting there (ALIGN_LEFT); bits 4-5
 * vertical, 0 centered, 1 starting at pos (ALIGN_TOP), 2 ending there (ALIGN_BOTTOM); 3
 * leaves that coordinate unset, as in the original), with one of four routines picked by
 * the frame's depth and cell size. */
void TileSheet_DrawAligned(TileSheet *set, u32 idx, u32 attr, short *pos, u32 align, GsOT *ot, int pri)
{
    TileFrame *frame = &set->frames[idx & 0xffff];
    int unit = (frame->small != 0) ? 8 : 16;
    short xy[2];

    switch (align & 3)
    {
    case ALIGN_H_CENTER: xy[0] = pos[0] - (unit * frame->w - 1) / 2; break;
    case ALIGN_H_RIGHT:  xy[0] = pos[0] + 1 - (u16)frame->w * unit; break;
    case ALIGN_H_LEFT:   xy[0] = pos[0]; break;
    }
    switch ((align >> 4) & 3)
    {
    case ALIGN_V_CENTER: xy[1] = pos[1] - (unit * frame->h - 1) / 2; break;
    case ALIGN_V_TOP:    xy[1] = pos[1]; break;
    case ALIGN_V_BOTTOM: xy[1] = pos[1] + 1 - (u16)frame->h * unit; break;
    }
    if (frame->mode == 0)
    {
        if (frame->small == 0)
            TileSheet_Draw4_16(set, idx & 0xffff, attr, xy, ot, pri);
        else
            TileSheet_Draw4_8(set, idx & 0xffff, attr, xy, ot, pri);
    }
    else if (frame->small == 0)
        TileSheet_Draw8_16(set, idx & 0xffff, attr, xy, ot, pri);
    else
        TileSheet_Draw8_8(set, idx & 0xffff, attr, xy, ot, pri);
}

/* Draw a 4bpp tile-sheet element made of 16x16 cells. In VRAM the cells are in columns 64 units
 * wide: 15 rows of 15 cells (4 units each, from 2 units and 8 lines in); a row's cells that are
 * next to each other in VRAM are drawn as one strip (0x800307F8) */
/* 0x800307F8 */
void TileSheet_Draw4_16(TileSheet *sheet, u32 idx, u32 attr, short *pos, GsOT *ot, int pri)
{
    TileFrame *frame = &sheet->frames[idx & 0xffff];
    VramImage img;
    short at[2];
    short cell;
    short row;
    short col;
    short run;

    img.mode = frame->mode;
    img.palette = 0;
    img.h = 16;
    img.clutX = frame->clutX;
    img.clutY = frame->clutY;
    cell = frame->cell;
    for (row = 0; row < frame->h; row++)
    {
        for (col = 0; col < frame->w; col += run)
        {
            run = frame->w - col;
            if (15 - cell % 15 < run)
            {
                run = 15 - cell % 15;
            }
            img.x = sheet->vramX + cell / 225 * 64 + cell % 15 * 4 + 2;
            img.w = run * 16;
            img.y = sheet->vramY + cell % 225 / 15 * 16 + 8;
            TileSheet_StripPos(at,pos,attr,col,run,row,frame->w,frame->h,16);
            Gfx_DrawImage(&img,attr,0,at,ot,pri);
            cell += run;
        }
    }
}

/* Draw an 8bpp tile-sheet element made of 16x16 cells: 15 rows of 8 cells (8 units each, from 8
 * lines in) per VRAM column (0x80030B98) */
/* 0x80030B98 */
void TileSheet_Draw8_16(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri)
{
    TileFrame *frame = &sheet->frames[idx];
    VramImage img;
    short at[2];
    short cell;
    short row;
    short col;
    short run;

    img.mode = frame->mode;
    img.palette = 0;
    img.h = 16;
    img.clutX = frame->clutX;
    img.clutY = frame->clutY;
    cell = frame->cell;
    for (row = 0; row < frame->h; row++)
    {
        for (col = 0; col < frame->w; col += run)
        {
            run = frame->w - col;
            if (8 - cell % 8 < run)
            {
                run = 8 - cell % 8;
            }
            img.x = sheet->vramX + cell / 120 * 64 + cell % 8 * 8;
            img.w = run * 16;
            img.y = sheet->vramY + cell % 120 / 8 * 16 + 8;
            TileSheet_StripPos(at,pos,attr,col,run,row,frame->w,frame->h,16);
            Gfx_DrawImage(&img,attr,0,at,ot,pri);
            cell += run;
        }
    }
}

/* Draw a 4bpp tile-sheet element made of 8x8 cells, one at a time. In VRAM the cells go in pairs
 * one above the other, 30 pairs (2 units wide) a row from 2 units and 8 lines in, 15 rows per
 * 64-unit column. Mirrored, the cells come one cell further right (as in the original)
 * (0x80030F04) */
/* 0x80030F04 */
void TileSheet_Draw4_8(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri)
{
    TileFrame *frame = &sheet->frames[idx];
    VramImage img;
    short at[2];
    short cell;
    short row;
    short col;

    img.mode = frame->mode;
    img.palette = 0;
    img.w = 8;
    img.h = 8;
    img.clutX = frame->clutX;
    img.clutY = frame->clutY;
    cell = frame->cell;
    for (row = 0; row < frame->h; row++)
    {
        for (col = 0; col < frame->w; col++)
        {
            img.x = sheet->vramX + cell / 900 * 64 + cell % 60 / 2 * 2 + 2;
            img.y = sheet->vramY + cell % 900 / 60 * 16 + 8 + cell % 2 * 8;
            TileSheet_StripPos(at,pos,attr,col,0,row,frame->w,frame->h,8);
            Gfx_DrawImage(&img,attr,0,at,ot,pri);
            cell++;
        }
    }
}

/* Draw an 8bpp tile-sheet element made of 8x8 cells: as TileSheet_Draw4_8 with 16 pairs (4 units
 * wide) a row, from 8 lines in (0x80031284) */
/* 0x80031284 */
void TileSheet_Draw8_8(TileSheet *sheet, u16 idx, u32 attr, short *pos, GsOT *ot, int pri)
{
    TileFrame *frame = &sheet->frames[idx];
    VramImage img;
    short at[2];
    short cell;
    short row;
    short col;

    img.mode = frame->mode;
    img.palette = 0;
    img.w = 8;
    img.h = 8;
    img.clutX = frame->clutX;
    img.clutY = frame->clutY;
    cell = frame->cell;
    for (row = 0; row < frame->h; row++)
    {
        for (col = 0; col < frame->w; col++)
        {
            img.x = sheet->vramX + cell / 480 * 64 + cell % 32 / 2 * 4;
            img.y = sheet->vramY + cell % 480 / 32 * 16 + 8 + cell % 2 * 8;
            TileSheet_StripPos(at,pos,attr,col,0,row,frame->w,frame->h,8);
            Gfx_DrawImage(&img,attr,0,at,ot,pri);
            cell++;
        }
    }
}

/* 0x800315C8 */
/* Draw element idx of a frame set rotated by angle (4096 = 360 degrees) and scaled by
 * pos[2] (x) and pos[3] (y) (4096 = 1.0) around pos[0], pos[1]. Unscaled and unrotated
 * (or turned by exactly 180 degrees) it is handed to TileSheet_DrawAligned. Otherwise the
 * element is cut into 16x16 tiles: the tile corners are transformed once into a vertex
 * grid, then every tile is drawn as a POLY_FT4 from the packet buffer.
 * attr: 0x400000 / 0x800000 mirror horizontally / vertically (both = 180 degrees),
 * bits 28-29 semi-transparency rate, 0x40000000 semi-transparent.
 * align as for TileSheet_DrawAligned. */
void TileSheet_DrawRotated(TileSheet *set, u16 idx, u32 attr, short *pos, u32 align, u32 angle, GsOT *ot, int pri)
{
    int vtx[259][2];        /* screen x, y of the tile corners, row by row (0x800315C8 sp+0x20) */
    TileFrame *frame;
    int m00, m01, m10, m11; /* rotation * scale, 20.12 fixed point */
    int dx, dy;
    short left = 0, top = 0;   /* the original leaves them unset for align code 3 */
    short i, j, k, x, y;
    short firstCol = 0, endCol = 0, dcol = 0, firstRow = 0, endRow = 0, drow = 0;
    short row, col, t, tpx, u, v;
    u16 tile, w;
    u32 flip;
    POLY_FT4 *quad;
    int *a, *b, *c, *d;

    if (pos[2] == 0x1000 && pos[3] == 0x1000 && (angle & 0x7ff) == 0)
    {
        if ((angle & 0xfff) != 0)
            attr ^= (GFX_FLIP_X | GFX_FLIP_Y);
        TileSheet_DrawAligned(set, idx, attr, pos, (short)align, ot, pri);
        return;
    }
    if ((attr & (GFX_FLIP_X | GFX_FLIP_Y)) == (GFX_FLIP_X | GFX_FLIP_Y))
    {
        angle += 0x800;
        attr &= ~(GFX_FLIP_X | GFX_FLIP_Y);
    }
    m00 = m11 = rcos(-angle);
    m10 = rsin(-angle);
    m01 = -m10;
    m00 = Math_MulFixed12(m00, pos[2]);
    m01 = Math_MulFixed12(m01, pos[3]);
    m10 = Math_MulFixed12(m10, pos[2]);
    m11 = Math_MulFixed12(m11, pos[3]);

    frame = &set->frames[idx];
    switch (align & 3)
    {
    case ALIGN_H_CENTER: left = -((frame->w * 16 - 1) / 2); break;
    case ALIGN_H_RIGHT:  left = 1 - (u16)frame->w * 16; break;
    case ALIGN_H_LEFT:   left = 0; break;
    }
    switch ((align >> 4) & 3)
    {
    case ALIGN_V_CENTER: top = -((frame->h * 16 - 1) / 2); break;
    case ALIGN_V_TOP:    top = 0; break;
    case ALIGN_V_BOTTOM: top = 1 - (u16)frame->h * 16; break;
    }

    /* corner (i, j) = pos + M * (left + 16 i, top + 16 j): first the row terms ... */
    k = 0;
    for (j = 0, y = top; j <= frame->h; j++, y += 16)
    {
        dx = Math_MulFixed12(m01, y);
        dy = Math_MulFixed12(m11, y);
        for (i = 0; i <= frame->w; i++, k++)
        {
            vtx[k][0] = dx + pos[0];
            vtx[k][1] = dy + pos[1];
        }
    }
    /* ... then the column terms */
    for (i = 0, x = left; i <= frame->w; i++, x += 16)
    {
        dx = Math_MulFixed12(m00, x);
        dy = Math_MulFixed12(m10, x);
        for (j = 0, k = i; j <= frame->h; j++, k += (u16)frame->w + 1)
        {
            vtx[k][0] += dx;
            vtx[k][1] += dy;
        }
    }

    flip = attr & (GFX_FLIP_X | GFX_FLIP_Y);
    switch (flip)
    {
    case GFX_FLIP_X:    firstCol = frame->w - 1; endCol = -1;   dcol = -1; firstRow = 0; endRow = frame->h; drow = 1; break;
    case GFX_FLIP_NONE: firstCol = 0;        endCol = frame->w; dcol = 1;  firstRow = 0; endRow = frame->h; drow = 1; break;
    case GFX_FLIP_Y:    firstCol = 0;        endCol = frame->w; dcol = 1;  firstRow = frame->h - 1; endRow = -1; drow = -1; break;
    }
    tile = frame->cell;
    for (row = firstRow; row != endRow; row += drow)
    {
        for (col = firstCol; col != endCol; col += dcol)
        {
            quad = (POLY_FT4 *)g_primPtr;
            SetPolyFT4(quad);
            /* tiles are stored in 64-halfword wide texture pages with an 8-pixel margin */
            t = tile;
            if (frame->mode == 0)          /* 4-bit: 15 x 15 tiles per page */
            {
                tpx = set->vramX + (t / 225) * 64;
                u = (t % 15) * 16 + 8;
                v = ((t % 225) / 15) * 16;
            }
            else                    /* 8-bit: 8 x 15 tiles per page */
            {
                tpx = set->vramX + (t / 120) * 64;
                u = (t % 8) * 16;
                if ((tile & 7) == 0)
                {
                    tpx -= 64;
                    u += 128;
                }
                v = ((t % 120) / 8) * 16;
            }
            quad->r0 = g_tintR;
            quad->g0 = g_tintG;
            quad->b0 = g_tintB;
            quad->tpage = GetTPage(frame->mode, (attr >> 28) & 3, tpx, set->vramY);
            quad->clut = GetClut(frame->clutX, frame->clutY);
            quad->u0 = u;      quad->v0 = v + 8;
            quad->u1 = u + 15; quad->v1 = v + 8;
            quad->u2 = u;      quad->v2 = v + 23;
            quad->u3 = u + 15; quad->v3 = v + 23;
            /* the four corners of the tile; mirroring swaps them */
            w = frame->w;
            k = col + row * (frame->w + 1);
            a = vtx[k];
            b = vtx[(short)(k + 1)];
            c = vtx[(short)(k + 1 + w)];
            d = vtx[(short)(k + 2 + w)];
            switch (flip)
            {
            case GFX_FLIP_X:
                quad->x1 = a[0]; quad->y1 = a[1]; quad->x0 = b[0]; quad->y0 = b[1];
                quad->x3 = c[0]; quad->y3 = c[1]; quad->x2 = d[0]; quad->y2 = d[1];
                break;
            case GFX_FLIP_NONE:
                quad->x0 = a[0]; quad->y0 = a[1]; quad->x1 = b[0]; quad->y1 = b[1];
                quad->x2 = c[0]; quad->y2 = c[1]; quad->x3 = d[0]; quad->y3 = d[1];
                break;
            case GFX_FLIP_Y:
                quad->x2 = a[0]; quad->y2 = a[1]; quad->x3 = b[0]; quad->y3 = b[1];
                quad->x0 = c[0]; quad->y0 = c[1]; quad->x1 = d[0]; quad->y1 = d[1];
                break;
            }
            if (attr & GFX_SEMITRANS)
                SetSemiTrans(quad, 1);
            tile++;
            AddPrim((u32 *)ot->org + pri, quad);
            g_primPtr = quad + 1;
        }
    }
}

/* Draw a 4bpp tile-sheet element (16x16 cells, as TileSheet_Draw4_16) squashed to height lines:
 * of each cell, the first `height` lines of its top half and of its bottom half, 8 lines apart
 * (mirrored vertically, the last ones). align: bits 0-1 horizontal (0 centered, 1 ending at pos, 2
 * starting there), 4-5 vertical (0 centered, 1 starting at pos, 2 ending there); 3 leaves the
 * coordinate unset in the original, 0 here (0x800321A4) */
/* 0x800321A4 */
void TileSheet_DrawSquashed(TileSheet *sheet, u16 idx, u32 attr, short *pos, u32 align, short height, GsOT *ot, int pri)
{
    TileFrame *frame = &sheet->frames[idx];
    VramImage img;
    short origin[2];
    short at[2];
    short cell;
    short row;
    short col;
    short run;

    img.mode = 0;
    img.palette = 0;
    img.h = height;
    origin[0] = 0;
    origin[1] = 0;
    switch (align & 3)
    {
    case ALIGN_H_CENTER: origin[0] = pos[0] - (frame->w * 16 - 1) / 2; break;
    case ALIGN_H_RIGHT:  origin[0] = pos[0] + 1 - (frame->w << 4); break;
    case ALIGN_H_LEFT:   origin[0] = pos[0]; break;
    }
    switch ((align >> 4) & 3)
    {
    case ALIGN_V_CENTER: origin[1] = pos[1] - (frame->h * 16 - 1) / 2; break;
    case ALIGN_V_TOP:    origin[1] = pos[1]; break;
    case ALIGN_V_BOTTOM: origin[1] = pos[1] + 1 - (frame->h << 4); break;
    }
    cell = frame->cell;
    for (row = 0; row < frame->h; row++)
    {
        for (col = 0; col < frame->w; col += run)
        {
            run = frame->w - col;
            if (15 - cell % 15 < run)
            {
                run = 15 - cell % 15;
            }
            img.w = run * 16;
            img.x = sheet->vramX + cell / 225 * 64 + cell % 15 * 4 + 2;
            img.clutX = frame->clutX;
            img.clutY = frame->clutY;
            img.y = sheet->vramY + cell % 225 / 15 * 16 + 8;
            TileSheet_StripPos(at,origin,attr,col,run,row,frame->w,frame->h,16);
            if (attr & GFX_FLIP_Y)
            {
                img.y -= img.h - 7;
                Gfx_DrawImage(&img,attr,0,at,ot,pri);
                at[1] -= 8;
                img.y -= 8;
            }
            else
            {
                Gfx_DrawImage(&img,attr,0,at,ot,pri);
                at[1] += 8;
                img.y += 8;
            }
            Gfx_DrawImage(&img,attr,0,at,ot,pri);
            cell += run;
        }
    }
}

/* Upload the filled part of the staging buffer to the sheet's VRAM area: the rows begun in the
 * current column of 225 cells, or all of them when it is full (0x80032850) */
/* 0x80032850 */
void TileSheet_Flush(TileSheet *sheet)
{
    short cells;
    RECT rect;

    cells = sheet->nextCell % 225;
    if (cells == 0)
    {
        if (sheet->full == 0)
        {
            return;
        }
        cells = 224;
    }
    rect.x = sheet->vramX + (short)((sheet->nextCell - 1) / 225) * 64 + 2;
    rect.w = 60;
    rect.y = sheet->vramY + 8;
    rect.h = cells / 15 * 16 + 16;
    LoadImage(&rect,(u_long *)sheet->stage);
}
