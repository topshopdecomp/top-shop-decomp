/*
 * gfx.c - drawing images and sprites
 *
 * The game's drawing layer over the GPU: images in VRAM (VramImage) and the sprite table
 * (g_sprites, in groups) drawn plain, aligned, stretched, scaled, rotated, tiled or as numbers
 * into an ordering table; loading sprite sets (.SPC) and TIM streams into VRAM; the draw-area
 * and clear packets.
 */
#include "common.h"
#include "topshop.h"

/* Once per frame (guard g_clearQueued, reset by Sys_EndFrame): copy the base DRAWENV
 * g_drawEnvBase to g_drawEnv, set isbg = 1 with color (r, g, b) and add the DR_ENV packet
 * at the back of the current OT (entry 31), so the frame starts with a clear. (0x8001A2FC) */
void Gfx_QueueClear(u8 r, u8 g, u8 b)
{
    DRAWENV *env = &g_drawEnv;
    DR_ENV *pkt;

    if (g_clearQueued == 0)
    {
        *env = g_drawEnvBase;
        /* clip and offset y follow the frame buffer being drawn (low half of the int) */
        env->clip.y = g_bufferOrigin[g_otIndex * 2 + 1];
        env->ofs[1] = g_bufferOrigin[g_otIndex * 2 + 1];
        env->isbg = 1;
        env->r0 = r;
        env->g0 = g;
        env->b0 = b;
        pkt = &g_drEnvPackets[g_otIndex];
        SetDrawEnv(pkt, env);
        AddPrim(g_ot[g_otIndex].org + 0x1f, pkt);
        g_clearQueued = 1;
    }
}

/* Add a DR_AREA primitive limiting drawing to rect (screen coordinates, clipped to
 * 320x240) at priority pri of ot. rect == NULL means the whole screen; the original
 * then writes the full-screen rectangle through the NULL pointer itself (to address 0,
 * low RAM) and hands that to SetDrawArea, which reads it back; kept as is. (0x8001A40C) */
void Gfx_SetDrawArea(void *area, GsOT *ot, int pri)
{
    RECT *rect = area;   /* callers pass x, y, w, h shorts (often g_drawPos); clipped in place */

    if (rect == NULL)
    {
        rect->x = g_bufferOrigin[g_otIndex * 2];
        rect->y = g_bufferOrigin[g_otIndex * 2 + 1];
        rect->w = 320;
        rect->h = 240;
    }
    else
    {
        /* to corners, clip, back to size, then to frame buffer coordinates */
        rect->w = rect->w + rect->x;
        rect->h = rect->h + rect->y;
        if (rect->x < 0)
            rect->x = 0;
        if (rect->y < 0)
            rect->y = 0;
        if (320 < rect->w)
            rect->w = 320;
        if (240 < rect->h)
            rect->h = 240;
        rect->w = rect->w - rect->x;
        rect->h = rect->h - rect->y;
        rect->x = rect->x + g_bufferOrigin[g_otIndex * 2];
        rect->y = rect->y + g_bufferOrigin[g_otIndex * 2 + 1];
    }
    SetDrawArea((DR_AREA *)g_primPtr, rect);
    AddPrim((u8 *)ot->org + ((short)pri << 2), g_primPtr);
    g_primPtr = (u8 *)g_primPtr + sizeof(DR_AREA);
}

/* 0x80024DE0
 * spc (already LZSS-decompressed): u32 count, count x 16-byte VramImage
 * entries (w in VRAM 16-bit units), then an image stream for Tim_UploadStream.
 * vramOffsets (may be NULL): [0],[1] added to x/y, [2],[3] added to clutX/clutY. */
int Sprite_LoadSet(int *spc, short group, short *vramOffsets)
{
    short ox = 0, oy = 0, cx = 0, cy = 0;
    unsigned long *data = (unsigned long *)(spc + 1);
    VramImage *dst;
    short *e;
    int id, end;

    if (vramOffsets != NULL)
    {
        ox = vramOffsets[0];
        oy = vramOffsets[1];
        cx = vramOffsets[2];
        cy = vramOffsets[3];
    }

    id = g_spriteGroups[group].next;
    end = spc[0] + id;
    for (dst = &g_sprites[id]; id < end; id++, dst++, data += 4)
    {
        e = (short *)data;
        dst->mode = e[0];
        dst->clutX = e[1] + cx;
        dst->clutY = e[2] + cy;
        dst->palette = e[3];
        dst->x = e[4] + ox;
        dst->y = e[5] + oy;
        switch (e[0])                   /* VRAM units -> pixels */
        {
        case IMG_MODE_4BIT:  dst->w = e[6] << 2; break;
        case IMG_MODE_8BIT:  dst->w = e[6] << 1; break;
        case IMG_MODE_16BIT: dst->w = e[6];      break;
        }
        dst->h = e[7];
    }
    g_spriteGroups[group].next = end;

    Tim_UploadStream(data, vramOffsets);
    return 0;
}

/* 0x80024FA4
 * Upload a stream of image records until a word that is neither 0x10 nor 0x11:
 *   0x10  standard TIM: pixels offset by vramOffsets[0..1], CLUT by [2..3]
 *   0x11  one block only (palette rows), offset by vramOffsets[2..3] */
void Tim_UploadStream(unsigned long *data, short *vramOffsets)
{
    short px = 0, py = 0, cx = 0, cy = 0;

    if (vramOffsets != NULL)
    {
        px = vramOffsets[0];
        py = vramOffsets[1];
        cx = vramOffsets[2];
        cy = vramOffsets[3];
    }
    for (;;)
    {
        if (*data == 0x10)
        {
            GsGetTimInfo(data + 1, &g_timInfo);
            data += 1 + (g_timInfo.pw * g_timInfo.ph) / 2 + 4;
            if (g_timInfo.pw != 0)
            {
                g_timInfo.px += px;
                g_timInfo.py += py;
                LoadImage((RECT *)&g_timInfo.px, g_timInfo.pixel);
            }
            if ((g_timInfo.pmode >> 3) & 1)            /* has a CLUT */
            {
                g_timInfo.cx += cx;
                g_timInfo.cy += cy;
                LoadImage((RECT *)&g_timInfo.cx, g_timInfo.clut);
                data += (g_timInfo.cw * g_timInfo.ch) / 2 + 3;
            }
        }
        else if (*data == 0x11)
        {
            GsGetTimInfo(data + 1, &g_timInfo);
            data += 1 + (g_timInfo.pw * g_timInfo.ph) / 2 + 4;
            if (g_timInfo.pw != 0)
            {
                g_timInfo.px += cx;
                g_timInfo.py += cy;
                LoadImage((RECT *)&g_timInfo.px, g_timInfo.pixel);
            }
        }
        else
        {
            break;
        }
    }
}

/* Draw one VRAM image at pos. palette: -100 = img->palette, -101 = blank CLUT at (0,0x1FE), else
 * img->palette + palette (4bpp: +16*p in X, 8bpp: +p rows). attr: 0x400000 flip X, 0x800000 flip Y
 * (POLY_FT4 from the packet buffer g_primPtr), else GsSortFastSprite; 0x40000000 semi-transparent,
 * bits 28-29 blend mode. Tinted by g_tintR/G/B. (0x800251B0) */
/* 0x800251B0 */
void Gfx_DrawImage(VramImage *img, unsigned long attr, short palette, short *pos, GsOT *ot, int pri)
{
    POLY_FT4 *poly;
    u16 pal;
    short cx;
    short cy;
    short x;
    short y;
    short left;
    short right;
    short top;
    short bottom;
    int column;
    int pageX;
    u8 u;
    u8 v;

    /* the CLUT (16-bit images have none: the original passes what its registers hold) */
    cx = 0;
    cy = 0;
    if (palette == -101)
    {
        cx = 0;
        cy = 0x1fe;
    }
    else
    {
        pal = palette == PAL_DEFAULT ? img->palette : img->palette + palette;
        if (img->mode == IMG_MODE_4BIT)
        {
            cx = img->clutX + (short)pal * 16;
            cy = img->clutY;
        }
        else if (img->mode == IMG_MODE_8BIT)
        {
            cx = img->clutX;
            cy = img->clutY + pal;
        }
    }
    x = pos[0];
    y = pos[1];
    column = img->x % 64;           /* position in the texture page, VRAM units */

    if ((attr & 0xc00000) == 0)
    {
        g_sprite.attribute = attr + img->mode * 0x1000000;
        g_sprite.w = img->w;
        g_sprite.h = img->h;
        g_sprite.tpage = img->x / 64 + (img->y / 256) * 16;
        switch (img->mode)
        {
        case IMG_MODE_4BIT:
            g_sprite.u = column * 4;
            break;
        case IMG_MODE_8BIT:
            g_sprite.u = column * 2;
            break;
        case IMG_MODE_16BIT:
            g_sprite.u = column;
            break;
        }
        g_sprite.v = img->y;
        g_sprite.r = g_tintR;
        g_sprite.g = g_tintG;
        g_sprite.b = g_tintB;
        g_sprite.x = x;
        g_sprite.y = y;
        g_sprite.cx = cx;
        g_sprite.cy = cy;
        GsSortFastSprite(&g_sprite,ot,pri);
        return;
    }

    /* flipped: a POLY_FT4 with its corners swapped */
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    left = x;
    right = x + img->w;
    top = y;
    bottom = y + img->h;
    if ((attr & 0x400000) != 0)
    {
        left = x + img->w;
        right = x;
    }
    if ((attr & 0x800000) != 0)
    {
        top = y + img->h;
        bottom = y;
    }
    poly->x0 = left;
    poly->y0 = top;
    poly->x1 = right;
    poly->y1 = top;
    poly->x2 = left;
    poly->y2 = bottom;
    poly->x3 = right;
    poly->y3 = bottom;
    if ((attr & 0x400000) != 0)
    {
        /* mirrored: u starts one texel to the left; at a page boundary that is the end of the
         * previous page (except for 4bpp, which wraps to 0xFF) */
        if (column == 0)
        {
            u = img->mode == IMG_MODE_8BIT ? 0x7f : (img->mode == IMG_MODE_16BIT ? 0x3f : 0xff);
        }
        else
        {
            u = img->mode == IMG_MODE_4BIT ? column * 4 - 1 : (img->mode == IMG_MODE_8BIT ? column * 2 - 1 : column - 1);
        }
        pageX = img->x / 64 - ((column == 0 && img->mode != IMG_MODE_4BIT) ? 1 : 0);
    }
    else
    {
        switch (img->mode)
        {
        case IMG_MODE_4BIT:
            u = img->x * 4;
            break;
        case IMG_MODE_8BIT:
            u = column * 2;
            break;
        case IMG_MODE_16BIT:
            u = column;
            break;
        default:
            u = 0;
            break;
        }
        pageX = img->x / 64;
    }
    v = (attr & 0x800000) != 0 ? img->y - 1 : img->y;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + img->w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + img->h;
    poly->u3 = u + img->w;
    poly->v3 = v + img->h;
    poly->tpage = GetTPage(img->mode,attr >> 0x1c & 3,pageX << 6,(img->y / 256) << 8);
    poly->clut = GetClut(cx,cy);
    if ((attr & 0x40000000) != 0)
    {
        SetSemiTrans(poly,1);
    }
    AddPrim(ot->org + pri,poly);
    g_primPtr = poly + 1;
}

/* Draw sprite id aligned at pos and rotated by angle (4096 = a full turn) around its center.
 * Angles 0 and 0x800 draw the plain image (flipped both ways for 0x800) through Gfx_DrawImage;
 * otherwise a POLY_FT4 with the sprite's own CLUT (palette is not used) (0x80025B9C) */
/* 0x80025B9C */
void Sprite_DrawRotated(u32 id, u32 attr, short palette, short *pos, u32 align, u32 angle, GsOT *ot, int pri)
{
    VramImage *img;
    POLY_FT4 *poly;
    short topLeft[2];
    short cornerX[4];
    short cornerY[4];
    int halfW;
    int halfH;
    int dx;
    int dy;
    int cs;
    int sn;
    int flip;
    int i;
    u8 u;
    u8 v;

    img = &g_sprites[id & 0xffff];
    /* the top left corner from the anchor (see ALIGN_*); 3 leaves it unset, as in the original */
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        topLeft[0] = pos[0] - (img->w - 1) / 2;
        break;
    case ALIGN_H_RIGHT:
        topLeft[0] = pos[0] + 1 - img->w;
        break;
    case ALIGN_H_LEFT:
        topLeft[0] = pos[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        topLeft[1] = pos[1] - (img->h - 1) / 2;
        break;
    case ALIGN_V_TOP:
        topLeft[1] = pos[1];
        break;
    case ALIGN_V_BOTTOM:
        topLeft[1] = pos[1] + 1 - img->h;
        break;
    }
    if ((angle & 0x7ff) == 0)
    {
        if ((angle & 0xfff) != 0)
        {
            attr ^= 0xc00000;
        }
        Gfx_DrawImage(img,attr,palette,topLeft,ot,pri);
        return;
    }

    if ((attr & 0xc00000) == 0xc00000)
    {
        /* flipped both ways = half a turn more */
        angle += 0x800;
        attr &= ~0xc00000;
    }
    cs = rcos(-angle);
    sn = rsin(-angle);
    halfW = img->w * 0x800;                 /* half the size, 20.12 fixed point */
    halfH = img->h * 0x800;
    /* the corners (top left, top right, bottom left, bottom right) turned around the center,
     * rounded */
    for (i = 0; i < 4; i++)
    {
        dx = (i & 1) != 0 ? halfW : -halfW;
        dy = (i & 2) != 0 ? halfH : -halfH;
        cornerX[i] = topLeft[0] + ((Math_MulFixed12(dx,cs) + Math_MulFixed12(dy,-sn) + halfW + 0x800) / 4096);
        cornerY[i] = topLeft[1] + ((Math_MulFixed12(dx,sn) + Math_MulFixed12(dy,cs) + halfH + 0x800) / 4096);
    }
    /* a flip swaps the corners: X left with right, Y top with bottom */
    flip = (attr & 0xc00000) == 0x400000 ? 1 : ((attr & 0xc00000) == 0x800000 ? 2 : 0);

    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->x0 = cornerX[0 ^ flip];
    poly->y0 = cornerY[0 ^ flip];
    poly->x1 = cornerX[1 ^ flip];
    poly->y1 = cornerY[1 ^ flip];
    poly->x2 = cornerX[2 ^ flip];
    poly->y2 = cornerY[2 ^ flip];
    poly->x3 = cornerX[3 ^ flip];
    poly->y3 = cornerY[3 ^ flip];
    u = 0;
    switch (img->mode)
    {
    case IMG_MODE_4BIT:
        u = img->x % 64 * 4;
        break;
    case IMG_MODE_8BIT:
        u = img->x % 64 * 2;
        break;
    case IMG_MODE_16BIT:
        u = img->x % 64;
        break;
    }
    v = img->y;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + img->w - 1;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + img->h - 1;
    poly->u3 = u + img->w - 1;
    poly->v3 = v + img->h - 1;
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x,img->y);
    poly->clut = GetClut(img->clutX,img->clutY);
    AddPrim(ot->org + pri,poly);
    g_primPtr = poly + 1;
}

/* Draw sprite id stretched to rect {x, y, w, h} and turned by angle (4096 = a full turn) around
 * the anchor point (x, y) (see ALIGN_*); palette as in Gfx_DrawImage (0x80026704) */
/* 0x80026704 */
void Sprite_DrawStretchedRotated(u32 id, u32 attr, short palette, short *rect, u16 align, int angle, GsOT *ot, int pri)
{
    VramImage *img;
    POLY_FT4 *poly;
    u16 pal;
    short cx;
    short cy;
    short w;
    short h;
    short cornerX[4];
    short cornerY[4];
    int ox;
    int oy;
    int dx;
    int dy;
    int cs;
    int sn;
    int flip;
    int i;
    u8 u;
    u8 v;

    img = &g_sprites[id & 0xffff];
    w = rect[2];
    h = rect[3];
    cx = 0;
    cy = 0;
    if (palette == -101)
    {
        cx = 0;
        cy = 0x1fe;
    }
    else
    {
        pal = palette == PAL_DEFAULT ? img->palette : img->palette + palette;
        if (img->mode == IMG_MODE_4BIT)
        {
            cx = img->clutX + (short)pal * 16;
            cy = img->clutY;
        }
        else if (img->mode == IMG_MODE_8BIT)
        {
            cx = img->clutX;
            cy = img->clutY + pal;
        }
    }
    /* the top left corner relative to the anchor; 3 leaves it unset, as in the original */
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        ox = -((w - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        ox = -w;
        break;
    case ALIGN_H_LEFT:
        ox = 0;
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        oy = -((h - 1) / 2);
        break;
    case ALIGN_V_TOP:
        oy = 0;
        break;
    case ALIGN_V_BOTTOM:
        oy = -h;
        break;
    }
    /* the corners (top left, top right, bottom left, bottom right) turned around the anchor,
     * rounded */
    cs = rcos(-angle);
    sn = rsin(-angle);
    for (i = 0; i < 4; i++)
    {
        dx = (i & 1) != 0 ? ox + w : ox;
        dy = (i & 2) != 0 ? oy + h : oy;
        cornerX[i] = rect[0] + ((cs * dx - sn * dy + 0x800) / 4096);
        cornerY[i] = rect[1] + ((sn * dx + cs * dy + 0x800) / 4096);
    }
    /* a flip swaps the corners: X left with right, Y top with bottom */
    flip = ((attr & 0x400000) != 0 ? 1 : 0) | ((attr & 0x800000) != 0 ? 2 : 0);

    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->x0 = cornerX[0 ^ flip];
    poly->y0 = cornerY[0 ^ flip];
    poly->x1 = cornerX[1 ^ flip];
    poly->y1 = cornerY[1 ^ flip];
    poly->x2 = cornerX[2 ^ flip];
    poly->y2 = cornerY[2 ^ flip];
    poly->x3 = cornerX[3 ^ flip];
    poly->y3 = cornerY[3 ^ flip];
    u = 0;
    switch (img->mode)
    {
    case IMG_MODE_4BIT:
        u = img->x % 64 * 4;
        break;
    case IMG_MODE_8BIT:
        u = img->x % 64 * 2;
        break;
    case IMG_MODE_16BIT:
        u = img->x % 64;
        break;
    }
    v = img->y;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + img->w - 1;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + img->h - 1;
    poly->u3 = u + img->w - 1;
    poly->v3 = v + img->h - 1;
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x & 0xffc0,img->y & 0xff00);
    poly->clut = GetClut(cx,cy);
    if ((attr & 0x40000000) != 0)
    {
        SetSemiTrans(poly,1);
    }
    AddPrim(ot->org + pri,poly);
    g_primPtr = poly + 1;
}

/* Draw an image already in VRAM as GsSortFastSprite calls: clipped to the 320x240 screen and
 * split where it crosses 256-texel texture-page boundaries (0x80026F48) */
/* 0x80026F48 */
void Gfx_DrawVramImage(VramImage *img, unsigned long attr, short *pos, GsOT *ot, unsigned short pri)
{
    short drawX;
    short drawY;
    short x;
    short skipY;
    u16 skipX;
    u16 srcX;
    u16 srcV;
    u16 width;
    u16 height;
    u16 left;
    u16 pageX;
    u16 u;
    u16 v;
    int partW;
    int partH;

    if ((pos[0] + img->w <= 0) || (0x140 <= pos[0]))
    {
        return;
    }
    drawX = pos[0];
    skipX = 0;
    if (pos[0] < 0)
    {
        drawX = 0;
        skipX = -pos[0];
    }
    if ((pos[1] + img->h <= 0) || (0xf0 <= pos[1]))
    {
        return;
    }
    drawY = pos[1];
    skipY = 0;
    if (pos[1] < 0)
    {
        drawY = 0;
        skipY = -pos[1];
    }
    g_sprite.attribute = attr | img->mode << 0x18;
    g_sprite.cx = img->clutX;
    g_sprite.cy = img->clutY;
    width = img->w - skipX;
    if (0x140 - drawX < (short)width)
    {
        width = 0x140 - drawX;
    }
    height = img->h - skipY;
    if (0xf0 - drawY < (short)height)
    {
        height = 0xf0 - drawY;
    }
    srcV = img->y + skipY;
    g_sprite.r = g_tintR;
    g_sprite.b = g_tintB;
    g_sprite.g = g_tintG;
    pageX = 0;
    u = 0;
    /* bands of rows up to the next 256-line boundary, columns up to the next page */
    for (; height != 0; height -= partH)
    {
        v = srcV & 0xff;
        partH = (short)height;
        if (0x100 - (short)v < partH)
        {
            partH = 0x100 - (short)v;
        }
        srcX = skipX;
        x = drawX;
        for (left = width; left != 0; left -= partW)
        {
            switch (img->mode)
            {
            case IMG_MODE_4BIT:
                pageX = img->x + ((short)srcX >> 2) & 0xffc0;
                u = img->x * 4 + srcX & 0xff;
                break;
            case IMG_MODE_8BIT:
                pageX = img->x + ((short)srcX >> 1) & 0xffc0;
                u = img->x * 2 + srcX - pageX * 2 & 0xff;
                break;
            case IMG_MODE_16BIT:
                pageX = img->x + srcX & 0xffc0;
                u = img->x + srcX - pageX & 0xff;
                break;
            }
            partW = (short)left;
            if (0x100 - (short)u < partW)
            {
                partW = 0x100 - (short)u;
            }
            srcX += partW;
            /* (the texture page row is the first band's for all of them, as in the original) */
            g_sprite.tpage = GetTPage(img->mode,0,(short)pageX,(short)(img->y + skipY & 0xff00));
            g_sprite.x = x;
            x += partW;
            g_sprite.w = partW;
            g_sprite.u = u;
            g_sprite.v = v;
            g_sprite.y = drawY;
            g_sprite.h = partH;
            GsSortFastSprite(&g_sprite,ot,pri);
        }
        drawY += partH;
        srcV = v + partH;
    }
}

/* Draw value with one digit sprite per digit (sprite digit0 + d, the minus sign digit0 + 10), right
 * to left from the last of `digits` positions spacing pixels apart (0x800272E8) */
/* 0x800272E8 */
void Gfx_DrawNumber(int value, int x, short y, short digits, short spacing, short digit0, char palette, GsOT *ot, short pri)
{
    short at[16][2];
    int sprite[16];
    short pos[2];
    u32 bcd;
    int px;
    int n;
    int i;

    bcd = Util_ToBcd(value < 0 ? -value : value);
    n = 0;
    px = (digits - 1) * spacing + x;
    for (i = digits - 1; i >= 0; i--)
    {
        if ((bcd == 0) && (i != digits - 1))
        {
            /* no digits left: the minus sign */
            if (value < 0)
            {
                at[n][0] = px;
                at[n][1] = y;
                sprite[n] = digit0 + 10;
                n++;
            }
            break;
        }
        sprite[n] = (bcd & 0xf) + digit0;
        bcd >>= 4;
        at[n][0] = px;
        at[n][1] = y;
        px -= spacing;
        n++;
    }
    /* drawn from the left */
    while (--n >= 0)
    {
        pos[0] = at[n][0];
        pos[1] = at[n][1];
        Gfx_DrawImage(&g_sprites[(u16)sprite[n]],0,palette,pos,ot,pri);
    }
}

/* Draw sprite id at rect x, y (see ALIGN_*) scaled by rect[2], rect[3] (4.12, 0x1000 = 1:
 * Gfx_DrawImageStretched with rect changed to the scaled size) (0x80027490) */
/* 0x80027490 */
void Sprite_DrawZoomed(u32 id, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri)
{
    VramImage *img;
    short pos[2];

    img = &g_sprites[id & 0xffff];
    if ((rect[2] != 0x1000) || (rect[3] != 0x1000))
    {
        rect[2] = Math_MulFixed12(img->w,rect[2]);
        rect[3] = Math_MulFixed12(img->h,rect[3]);
        Gfx_DrawImageStretched(img,attr,palette,rect,(short)align,ot,pri);
        return;
    }
    /* 3 leaves the coordinate unset, as in the original */
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        pos[0] = rect[0] - (img->w - 1) / 2;
        break;
    case ALIGN_H_RIGHT:
        pos[0] = rect[0] + 1 - img->w;
        break;
    case ALIGN_H_LEFT:
        pos[0] = rect[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        pos[1] = rect[1] - (img->h - 1) / 2;
        break;
    case ALIGN_V_TOP:
        pos[1] = rect[1];
        break;
    case ALIGN_V_BOTTOM:
        pos[1] = rect[1] + 1 - img->h;
        break;
    }
    Gfx_DrawImage(img,attr,palette,pos,ot,pri);
}

/* Draw an image stretched to rect {x, y, w, h} (see ALIGN_*): a POLY_FT4, or Gfx_DrawImage when
 * the size is the image's. Changes rect: a size not larger than the image's grows by one, as the
 * polygon leaves out its right and bottom edges (0x800276F8) */
/* 0x800276F8 */
void Gfx_DrawImageStretched(VramImage *img, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri)
{
    POLY_FT4 *poly;
    u16 pal;
    short cx;
    short cy;
    short x;
    short y;
    short left;
    short right;
    short top;
    short bottom;
    short pos[2];
    u8 u;
    u8 v;

    if ((img->w == rect[2]) && (img->h == rect[3]))
    {
        /* 3 leaves the coordinate unset, as in the original */
        switch (align & 3)
        {
        case ALIGN_H_CENTER:
            pos[0] = rect[0] - ((img->w - 1) / 2);
            break;
        case ALIGN_H_RIGHT:
            pos[0] = rect[0] + 1 - img->w;
            break;
        case ALIGN_H_LEFT:
            pos[0] = rect[0];
            break;
        }
        switch (align >> 4 & 3)
        {
        case ALIGN_V_CENTER:
            pos[1] = rect[1] - ((img->h - 1) / 2);
            break;
        case ALIGN_V_TOP:
            pos[1] = rect[1];
            break;
        case ALIGN_V_BOTTOM:
            pos[1] = rect[1] + 1 - img->h;
            break;
        }
        Gfx_DrawImage(img,attr,palette,pos,ot,pri);
        return;
    }
    cx = 0;
    cy = 0;
    if (palette == -101)
    {
        cx = 0;
        cy = 0x1fe;
    }
    else
    {
        pal = palette == PAL_DEFAULT ? img->palette : palette + img->palette;
        if (img->mode == IMG_MODE_4BIT)
        {
            cx = img->clutX + (short)pal * 16;
            cy = img->clutY;
        }
        else if (img->mode == IMG_MODE_8BIT)
        {
            cx = img->clutX;
            cy = img->clutY + pal;
        }
    }
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        x = rect[0] - ((rect[2] - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        x = rect[0] + 1 - rect[2];
        break;
    case ALIGN_H_LEFT:
        x = rect[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        y = rect[1] - ((rect[3] - 1) / 2);
        break;
    case ALIGN_V_TOP:
        y = rect[1];
        break;
    case ALIGN_V_BOTTOM:
        y = rect[1] + 1 - rect[3];
        break;
    }
    if (rect[2] <= img->w)
    {
        rect[2]++;
    }
    if (rect[3] <= img->h)
    {
        rect[3]++;
    }
    left = x;
    right = x + rect[2];
    top = y;
    bottom = y + rect[3];
    if ((attr & 0x400000) != 0)
    {
        left = x + rect[2];
        right = x;
    }
    if ((attr & 0x800000) != 0)
    {
        top = y + rect[3];
        bottom = y;
    }
    poly->x0 = left;
    poly->y0 = top;
    poly->x1 = right;
    poly->y1 = top;
    poly->x2 = left;
    poly->y2 = bottom;
    poly->x3 = right;
    poly->y3 = bottom;
    switch (img->mode)
    {
    case IMG_MODE_4BIT:
        u = img->x % 64 * 4;
        poly->clut = GetClut(cx,cy);
        break;
    case IMG_MODE_8BIT:
        u = img->x % 64 * 2;
        poly->clut = GetClut(cx,cy);
        break;
    case IMG_MODE_16BIT:
        u = img->x % 64;                  /* 16-bit: no CLUT */
        break;
    default:
        u = x;                            /* (what the register holds in the original) */
        break;
    }
    v = img->y;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + img->w - 1;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + img->h - 1;
    poly->u3 = u + img->w - 1;
    poly->v3 = v + img->h - 1;
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x,img->y);
    if ((attr & 0x40000000) != 0)
    {
        SetSemiTrans(poly,1);
    }
    AddPrim(ot->org + pri,poly);
    g_primPtr = poly + 1;
}

/* Fill rect {x, y, w, h} (see ALIGN_*) with sprite id repeated: the image drawn at the rect's size
 * inside a texture window of the sprite. (An ordering table slot draws what was added last
 * first: window set, image, window reset.) (0x80027F48) */
/* 0x80027F48 */
void Sprite_DrawTiled(int id, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri)
{
    VramImage img;
    short pos[2];
    RECT win;

    win.x = 0;
    win.y = 0;
    win.w = 0;
    win.h = 0;
    SetTexWindow(g_primPtr,&win);
    AddPrim(ot->org + pri,g_primPtr);
    g_primPtr = (DR_TWIN *)g_primPtr + 1;
    img = g_sprites[(short)id];
    img.w = rect[2];
    img.h = rect[3];
    /* 3 leaves the coordinate unset, as in the original */
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        pos[0] = rect[0] - ((rect[2] - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        pos[0] = rect[0] + 1 - rect[2];
        break;
    case ALIGN_H_LEFT:
        pos[0] = rect[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        pos[1] = rect[1] - (((u16)img.h - 1) / 2);
        break;
    case ALIGN_V_TOP:
        pos[1] = rect[1];
        break;
    case ALIGN_V_BOTTOM:
        pos[1] = rect[1] + 1 - img.h;
        break;
    }
    Gfx_DrawImage(&img,attr,palette,pos,ot,pri);
    win.x = g_sprites[(short)id].x % 64 * 4;
    win.y = g_sprites[(short)id].y % 256;
    win.w = g_sprites[(short)id].w;
    win.h = g_sprites[(short)id].h;
    SetTexWindow(g_primPtr,&win);
    AddPrim(ot->org + pri,g_primPtr);
    g_primPtr = (DR_TWIN *)g_primPtr + 1;
}

/* Draw a multi-part sprite: each part at pos plus its offset, flipped as its flip says (0 none,
 * 1 Y, 2 X, 3 both; other values keep the previous part's attributes). attr's own flips only
 * move the parts: flipped in X they are placed mirrored; flipped in Y (a bug of the original)
 * every part's x is set to the mirrored y and its y left unset; flipped both ways they are placed
 * as if unflipped (0x8002824C) */
/* 0x8002824C */
void SpriteList_Draw(SpriteList *list, u32 attr, short palette, short *pos, GsOT *ot, int pri)
{
    SpritePart *part;
    short at[2];
    u32 flip;
    u32 partAttr;
    int i;

    flip = attr & 0xc00000;
    partAttr = 0;                       /* (a register left from the caller in the original) */
    for (i = 0; list->parts[(short)i].sprite != -1; i++)
    {
        part = &list->parts[(short)i];
        switch (flip)
        {
        case GFX_FLIP_X:
            at[0] = pos[0] - (part->x + g_sprites[part->sprite].w);
            at[1] = part->y + pos[1];
            break;
        case GFX_FLIP_Y:
            at[0] = pos[1] - (part->y + g_sprites[part->sprite].h);
            break;
        default:
            at[0] = part->x + pos[0];
            at[1] = part->y + pos[1];
            break;
        }
        switch (part->flip)
        {
        case PART_NO_FLIP:
            partAttr = attr ^ flip;
            break;
        case PART_FLIP_Y:
            partAttr = attr ^ flip ^ GFX_FLIP_Y;
            break;
        case PART_FLIP_X:
            partAttr = attr ^ flip ^ GFX_FLIP_X;
            break;
        case PART_FLIP_XY:
            partAttr = attr ^ flip ^ (GFX_FLIP_X | GFX_FLIP_Y);
            break;
        }
        Gfx_DrawImage(&g_sprites[(u16)part->sprite],partAttr,palette,at,ot,pri);
    }
}

/* SpriteList_Draw scaled by rect[2], rect[3] (4.12) and turned by angle (4096 = a full turn)
 * around rect x, y: one POLY_FT4 per part, flipped as the part's flip says (0x800287E8) */
/* 0x800287E8 */
void SpriteList_DrawScaled(SpriteList *list, u32 attr, short palette, short *rect, u32 angle, GsOT *ot, int pri)
{
    SpritePart *part;
    VramImage *img;
    POLY_FT4 *poly;
    short cornerX[4];
    short cornerY[4];
    u16 pal;
    short cx;
    short cy;
    u32 partAttr;
    int a;
    int b;
    int c;
    int d;
    int cs;
    int sn;
    int x0;
    int x1;
    int y0;
    int y1;
    int px;
    int py;
    int flip;
    int i;
    int k;
    u8 u;
    u8 v;

    if ((rect[2] == 0x1000) && (rect[3] == 0x1000) && ((angle & 0x7ff) == 0))
    {
        if ((angle & 0xfff) != 0)
        {
            attr ^= 0xc00000;
        }
        SpriteList_Draw(list,attr,palette,rect,ot,pri);
        return;
    }
    if ((attr & 0xc00000) == 0xc00000)
    {
        /* flipped both ways = half a turn more */
        angle += 0x800;
        attr &= ~0xc00000;
    }
    cs = rcos(-angle);
    sn = rsin(-angle);
    a = Math_MulFixed12(cs,rect[2]);    /* the turn and scale, 4.12 */
    b = Math_MulFixed12(-sn,rect[3]);
    c = Math_MulFixed12(sn,rect[2]);
    d = Math_MulFixed12(cs,rect[3]);
    /* (in the original these carry over from the previous part, or the caller's registers) */
    partAttr = 0;
    cx = 0;
    cy = 0;
    for (i = 0; list->parts[(short)i].sprite != -1; i++)
    {
        part = &list->parts[(short)i];
        img = &g_sprites[part->sprite];
        /* the part's rectangle in the picture, sides swapped as attr flips it; its corners (top
         * left, top right, bottom left, bottom right) transformed */
        x0 = part->x;
        x1 = part->x + img->w;
        y0 = part->y;
        y1 = part->y + img->h;
        if ((attr & 0x400000) != 0)
        {
            x0 = part->x + img->w;
            x1 = part->x;
        }
        if ((attr & 0x800000) != 0)
        {
            y0 = part->y + img->h;
            y1 = part->y;
        }
        for (k = 0; k < 4; k++)
        {
            px = (k & 1) != 0 ? x1 : x0;
            py = (k & 2) != 0 ? y1 : y0;
            cornerX[k] = ((a * px + b * py) / 4096) + rect[0];
            cornerY[k] = ((c * px + d * py) / 4096) + rect[1];
        }
        switch (part->flip)
        {
        case PART_NO_FLIP:
            partAttr = attr;
            break;
        case PART_FLIP_Y:
            partAttr = attr ^ GFX_FLIP_Y;
            break;
        case PART_FLIP_X:
            partAttr = attr ^ GFX_FLIP_X;
            break;
        case PART_FLIP_XY:
            partAttr = attr ^ (GFX_FLIP_X | GFX_FLIP_Y);
            break;
        }
        if (palette == -101)
        {
            cx = 0;
            cy = 0x1fe;
        }
        else
        {
            pal = palette == PAL_DEFAULT ? img->palette : palette + img->palette;
            if (img->mode == IMG_MODE_4BIT)
            {
                cx = img->clutX + (short)pal * 16;
                cy = img->clutY;
            }
            else if (img->mode == IMG_MODE_8BIT)
            {
                cx = img->clutX;
                cy = img->clutY + pal;
            }
        }
        /* the part's flip swaps the corners: X left with right, Y top with bottom */
        flip = ((partAttr & 0x400000) != 0 ? 1 : 0) | ((partAttr & 0x800000) != 0 ? 2 : 0);
        poly = (POLY_FT4 *)g_primPtr;
        SetPolyFT4(poly);
        poly->x0 = cornerX[0 ^ flip];
        poly->y0 = cornerY[0 ^ flip];
        poly->x1 = cornerX[1 ^ flip];
        poly->y1 = cornerY[1 ^ flip];
        poly->x2 = cornerX[2 ^ flip];
        poly->y2 = cornerY[2 ^ flip];
        poly->x3 = cornerX[3 ^ flip];
        poly->y3 = cornerY[3 ^ flip];
        switch (img->mode)
        {
        case IMG_MODE_4BIT:
            u = img->x % 64 * 4;
            break;
        case IMG_MODE_8BIT:
            u = img->x % 64 * 2;
            break;
        case IMG_MODE_16BIT:
            u = img->x % 64;
            break;
        default:
            u = cornerX[0];                 /* (what the register holds in the original) */
            break;
        }
        v = img->y;
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + img->w - 1;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + img->h - 1;
        poly->u3 = u + img->w - 1;
        poly->v3 = v + img->h - 1;
        poly->r0 = g_tintR;
        poly->g0 = g_tintG;
        poly->b0 = g_tintB;
        poly->tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x & 0xffc0,img->y & 0xff00);
        poly->clut = GetClut(cx,cy);
        if ((attr & 0x40000000) != 0)
        {
            SetSemiTrans(poly,1);
        }
        AddPrim(ot->org + pri,poly);
        g_primPtr = poly + 1;
    }
}

/* The size of a multi-part sprite: its parts' bounding box, which includes the origin (0x800293DC) */
/* 0x800293DC */
void SpriteList_GetBounds(SpriteList *out, SpritePart *parts)
{
    SpritePart *part;
    short minX;
    short minY;
    short maxX;
    short maxY;
    int i;

    minX = 0;
    minY = 0;
    maxX = 0;
    maxY = 0;
    out->parts = parts;
    for (i = 0; parts[(short)i].sprite != -1; i++)
    {
        part = &parts[(short)i];
        if (part->x < minX)
        {
            minX = part->x;
        }
        if (part->y < minY)
        {
            minY = part->y;
        }
        if (maxX < part->x + g_sprites[part->sprite].w)
        {
            maxX = part->x + g_sprites[part->sprite].w;
        }
        if (maxY < part->y + g_sprites[part->sprite].h)
        {
            maxY = part->y + g_sprites[part->sprite].h;
        }
    }
    out->w = maxX - minX;
    out->h = maxY - minY;
}

/* Draw an image aligned at pos as if turned by angle (4096 = a full turn) around its horizontal
 * axis: the height shrinks with the cosine, the top edge widens and the bottom edge narrows by
 * a skew from the sine and scale (4.12), as four triangles around the center. Angle 0 draws the
 * plain image (0x800294E8) */
/* 0x800294E8 */
void Gfx_DrawImageRotScaled(VramImage *img, u32 attr, short palette, short *pos, u32 align, int angle, int scale, GsOT *ot, int pri)
{
    static const char tri[4][3] = {{0, 1, 4}, {0, 4, 2}, {4, 1, 3}, {2, 4, 3}};
    POLY_FT3 *poly;
    short px[5];
    short py[5];
    u8 pu[5];
    u8 pv[5];
    u16 pal;
    short cx;
    short cy;
    short x;
    short y;
    short h;
    short skew;
    short adj;
    short left;
    short right;
    short top;
    short bottom;
    short topLeft[2];
    u16 clut;
    u16 tpage;
    u8 u;
    int t;
    int k;

    if (angle == 0)
    {
        /* 3 leaves the coordinate unset, as in the original */
        switch (align & 3)
        {
        case ALIGN_H_CENTER:
            topLeft[0] = pos[0] - ((img->w - 1) / 2);
            break;
        case ALIGN_H_RIGHT:
            topLeft[0] = pos[0] + 1 - img->w;
            break;
        case ALIGN_H_LEFT:
            topLeft[0] = pos[0];
            break;
        }
        switch (align >> 4 & 3)
        {
        case ALIGN_V_CENTER:
            topLeft[1] = pos[1] - ((img->h - 1) / 2);
            break;
        case ALIGN_V_TOP:
            topLeft[1] = pos[1];
            break;
        case ALIGN_V_BOTTOM:
            topLeft[1] = pos[1] + 1 - img->h;
            break;
        }
        Gfx_DrawImage(img,attr,palette,topLeft,ot,pri);
        return;
    }
    cx = 0;
    cy = 0;
    if (palette == -101)
    {
        cx = 0;
        cy = 0x1fe;
    }
    else
    {
        pal = palette == PAL_DEFAULT ? img->palette : img->palette + palette;
        if (img->mode == IMG_MODE_4BIT)
        {
            cx = img->clutX + (short)pal * 16;
            cy = img->clutY;
        }
        else if (img->mode == IMG_MODE_8BIT)
        {
            cx = img->clutX;
            cy = img->clutY + pal;
        }
    }
    skew = (Math_MulFixed12(rsin(angle),img->w * scale) / 8192);
    h = (rcos(angle) * img->h / 4096);
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        x = pos[0] - ((img->w - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        x = pos[0] + 1 - img->w;
        break;
    case ALIGN_H_LEFT:
        x = pos[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        y = pos[1] - ((h - 1) / 2);
        break;
    case ALIGN_V_TOP:
        y = pos[1];
        break;
    case ALIGN_V_BOTTOM:
        y = pos[1] + 1 - h;
        break;
    }
    /* corners: 0 top left, 1 top right, 2 bottom left, 3 bottom right; 4 the center */
    px[4] = x + img->w / 2;
    py[4] = y + h / 2;
    if (h <= img->h)
    {
        h++;
    }
    adj = (attr & 0x800000) != 0 ? -1 : 1;
    left = x;
    right = x + img->w + adj;
    top = y;
    bottom = y + h;
    if ((attr & 0x800000) != 0)
    {
        top = y + h;
        bottom = y;
    }
    if ((attr & 0x400000) != 0)
    {
        px[0] = right + skew;
        px[1] = left - skew;
        px[2] = right - skew;
        px[3] = left + skew;
    }
    else
    {
        px[0] = left - skew;
        px[1] = right + skew;
        px[2] = left + skew;
        px[3] = right - skew;
    }
    py[0] = top;
    py[1] = top;
    py[2] = bottom;
    py[3] = bottom;

    clut = 0;
    switch (img->mode)
    {
    case IMG_MODE_4BIT:
        u = img->x % 64 * 4;
        clut = GetClut(cx,cy);
        break;
    case IMG_MODE_8BIT:
        u = img->x % 64 * 2;
        clut = GetClut(cx,cy);
        break;
    case IMG_MODE_16BIT:
        u = img->x % 64;                  /* 16-bit: no CLUT */
        break;
    default:
        u = x;                            /* (what the register holds in the original) */
        break;
    }
    pu[0] = u;
    pv[0] = img->y;
    pu[1] = u + img->w - 1;
    pv[1] = img->y;
    pu[2] = u;
    pv[2] = img->y + img->h - 1;
    pu[3] = u + img->w - 1;
    pv[3] = img->y + img->h - 1;
    pu[4] = u + img->w / 2;
    pv[4] = img->y + img->h / 2;
    tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x,img->y);
    for (t = 0; t < 4; t++)
    {
        poly = (POLY_FT3 *)g_primPtr;
        SetPolyFT3(poly);
        k = tri[t][0];
        poly->x0 = px[k];
        poly->y0 = py[k];
        poly->u0 = pu[k];
        poly->v0 = pv[k];
        k = tri[t][1];
        poly->x1 = px[k];
        poly->y1 = py[k];
        poly->u1 = pu[k];
        poly->v1 = pv[k];
        k = tri[t][2];
        poly->x2 = px[k];
        poly->y2 = py[k];
        poly->u2 = pu[k];
        poly->v2 = pv[k];
        poly->r0 = g_tintR;
        poly->g0 = g_tintG;
        poly->b0 = g_tintB;
        poly->tpage = tpage;
        poly->clut = clut;
        if ((attr & 0x40000000) != 0)
        {
            SetSemiTrans(poly,1);
        }
        AddPrim(ot->org + pri,poly);
        g_primPtr = poly + 1;
    }
}

/* Draw an image aligned at pos as if turned by angle (4096 = a full turn) around its vertical
 * axis: the width shrinks with the cosine, the left edge grows and the right edge shrinks by a
 * skew from the sine and scale (4.12), as four triangles around the center. Angle 0 draws the
 * plain image (0x8002A160) */
/* 0x8002A160 */
void Gfx_DrawImageEx(VramImage *img, u32 attr, short palette, short *pos, u32 align, int angle, int scale, GsOT *ot, int pri)
{
    static const char tri[4][3] = {{0, 1, 4}, {0, 4, 2}, {4, 1, 3}, {2, 4, 3}};
    POLY_FT3 *poly;
    short gx[4];
    short gy[4];
    short px[5];
    short py[5];
    u8 pu[5];
    u8 pv[5];
    u16 pal;
    short cx;
    short cy;
    short x;
    short y;
    short w;
    short skew;
    short topLeft[2];
    u16 clut;
    u16 tpage;
    u8 u;
    int flip;
    int t;
    int k;

    if (angle == 0)
    {
        /* 3 leaves the coordinate unset, as in the original */
        switch (align & 3)
        {
        case ALIGN_H_CENTER:
            topLeft[0] = pos[0] - ((img->w - 1) / 2);
            break;
        case ALIGN_H_RIGHT:
            topLeft[0] = pos[0] + 1 - img->w;
            break;
        case ALIGN_H_LEFT:
            topLeft[0] = pos[0];
            break;
        }
        switch (align >> 4 & 3)
        {
        case ALIGN_V_CENTER:
            topLeft[1] = pos[1] - ((img->h - 1) / 2);
            break;
        case ALIGN_V_TOP:
            topLeft[1] = pos[1];
            break;
        case ALIGN_V_BOTTOM:
            topLeft[1] = pos[1] + 1 - img->h;
            break;
        }
        Gfx_DrawImage(img,attr,palette,topLeft,ot,pri);
        return;
    }
    cx = 0;
    cy = 0;
    if (palette == -101)
    {
        cx = 0;
        cy = 0x1fe;
    }
    else
    {
        pal = palette == PAL_DEFAULT ? img->palette : img->palette + palette;
        if (img->mode == IMG_MODE_4BIT)
        {
            cx = img->clutX + (short)pal * 16;
            cy = img->clutY;
        }
        else if (img->mode == IMG_MODE_8BIT)
        {
            cx = img->clutX;
            cy = img->clutY + pal;
        }
    }
    skew = (Math_MulFixed12(rsin(angle),img->h * scale) / 8192);
    w = (rcos(angle) * img->w / 4096);
    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        x = pos[0] - ((w - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        x = pos[0] + 1 - w;
        break;
    case ALIGN_H_LEFT:
        x = pos[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        y = pos[1] - ((img->h - 1) / 2);
        break;
    case ALIGN_V_TOP:
        y = pos[1];
        break;
    case ALIGN_V_BOTTOM:
        y = pos[1] + 1 - img->h;
        break;
    }
    px[4] = x + w / 2;
    py[4] = y + img->h / 2;
    if (w <= img->w)
    {
        w++;
    }
    /* the shape: top and bottom of the long left edge and the short right one */
    gx[0] = x;
    gy[0] = y - skew;
    gx[1] = x + w;
    gy[1] = y + skew + 1;
    gx[2] = x;
    gy[2] = y + img->h + skew;
    gx[3] = x + w;
    gy[3] = y + img->h - skew + 1;
    /* the image's corners (0 top left, 1 top right, 2 bottom left, 3 bottom right) on it: a flip
     * swaps X left with right, Y top with bottom; 4 is the center */
    flip = ((attr & 0x400000) != 0 ? 1 : 0) | ((attr & 0x800000) != 0 ? 2 : 0);
    for (k = 0; k < 4; k++)
    {
        px[k] = gx[k ^ flip];
        py[k] = gy[k ^ flip];
    }

    clut = 0;
    switch (img->mode)
    {
    case IMG_MODE_4BIT:
        u = img->x % 64 * 4;
        clut = GetClut(cx,cy);
        break;
    case IMG_MODE_8BIT:
        u = img->x % 64 * 2;
        clut = GetClut(cx,cy);
        break;
    case IMG_MODE_16BIT:
        u = img->x % 64;                  /* 16-bit: no CLUT */
        break;
    default:
        u = x;                            /* (what the register holds in the original) */
        break;
    }
    pu[0] = u;
    pv[0] = img->y;
    pu[1] = u + img->w - 1;
    pv[1] = img->y;
    pu[2] = u;
    pv[2] = img->y + img->h - 1;
    pu[3] = u + img->w - 1;
    pv[3] = img->y + img->h - 1;
    pu[4] = u + img->w / 2;
    pv[4] = img->y + img->h / 2;
    tpage = GetTPage(img->mode,attr >> 0x1c & 3,img->x,img->y);
    for (t = 0; t < 4; t++)
    {
        poly = (POLY_FT3 *)g_primPtr;
        SetPolyFT3(poly);
        k = tri[t][0];
        poly->x0 = px[k];
        poly->y0 = py[k];
        poly->u0 = pu[k];
        poly->v0 = pv[k];
        k = tri[t][1];
        poly->x1 = px[k];
        poly->y1 = py[k];
        poly->u1 = pu[k];
        poly->v1 = pv[k];
        k = tri[t][2];
        poly->x2 = px[k];
        poly->y2 = py[k];
        poly->u2 = pu[k];
        poly->v2 = pv[k];
        poly->r0 = g_tintR;
        poly->g0 = g_tintG;
        poly->b0 = g_tintB;
        poly->tpage = tpage;
        poly->clut = clut;
        if ((attr & 0x40000000) != 0)
        {
            SetSemiTrans(poly,1);
        }
        AddPrim(ot->org + pri,poly);
        g_primPtr = poly + 1;
    }
}

/* 0x8002ADE4 */
void Sprite_InitGroups(SpriteGroup *groups, short count)
{
    short i, sum = 0;

    g_spriteGroups = groups;
    for (i = 0; i < count; i++)
    {
        groups[i].base = sum;
        groups[i].next = sum;
        sum += groups[i].size;
    }
}

/* 0x8002AE48 */
void Sprite_Draw(unsigned short id, unsigned long attr, short palette, short *pos,
                 GsOT *ot, int pri)
{
    Gfx_DrawImage(&g_sprites[id], attr, palette, pos, ot, pri);
}

/* 0x8002AE90
 * Like Sprite_Draw, but pos is an anchor point (see ALIGN_* in gfx.h).
 * Alignment value 3 leaves that coordinate uninitialized, as in the original. */
void Sprite_DrawAligned(unsigned short id, unsigned long attr, short palette, short *pos,
                        unsigned int align, GsOT *ot, int pri)
{
    VramImage *img = &g_sprites[id];
    short p[2];

    switch (align & 3)
    {
    case ALIGN_H_CENTER: p[0] = pos[0] - (img->w - 1) / 2; break;
    case ALIGN_H_RIGHT:  p[0] = pos[0] + 1 - img->w;       break;
    case ALIGN_H_LEFT:   p[0] = pos[0];                    break;
    }
    switch ((align >> 4) & 3)
    {
    case ALIGN_V_CENTER: p[1] = pos[1] - (img->h - 1) / 2; break;
    case ALIGN_V_TOP:    p[1] = pos[1];                    break;
    case ALIGN_V_BOTTOM: p[1] = pos[1] + 1 - img->h;       break;
    }
    Gfx_DrawImage(img, attr, palette, p, ot, pri);
}

/* Gfx_DrawImage with the image aligned at pos (see ALIGN_*; 3 leaves the coordinate unset, as in
 * the original) (0x8002B02C) */
/* 0x8002B02C */
void Gfx_DrawImageAligned(VramImage *img, u32 attr, short palette, short *pos, u32 align, GsOT *ot, int pri)
{
    short at[2];

    switch (align & 3)
    {
    case ALIGN_H_CENTER:
        at[0] = pos[0] - ((img->w - 1) / 2);
        break;
    case ALIGN_H_RIGHT:
        at[0] = pos[0] + 1 - img->w;
        break;
    case ALIGN_H_LEFT:
        at[0] = pos[0];
        break;
    }
    switch (align >> 4 & 3)
    {
    case ALIGN_V_CENTER:
        at[1] = pos[1] - ((img->h - 1) / 2);
        break;
    case ALIGN_V_TOP:
        at[1] = pos[1];
        break;
    case ALIGN_V_BOTTOM:
        at[1] = pos[1] + 1 - img->h;
        break;
    }
    Gfx_DrawImage(img,attr,palette,at,ot,pri);
}

/* Sprite_DrawStretchedRotated with the size given as scales rect[2], rect[3] (4.12) of the sprite's
 * (0x8002B174) */
/* 0x8002B174 */
void Sprite_DrawScaled(u32 id, u32 attr, short palette, short *rect, short align, u32 angle, GsOT *ot, int pri)
{
    short size[4];

    size[0] = rect[0];
    size[1] = rect[1];
    size[2] = rect[2] * g_sprites[id & 0xffff].w / 4096;
    size[3] = rect[3] * g_sprites[id & 0xffff].h / 4096;
    Sprite_DrawStretchedRotated(id & 0xffff,attr,palette,size,align,angle,ot,pri);
}

/* Gfx_DrawImageStretched for sprite id (0x8002B238) */
/* 0x8002B238 */
void Sprite_DrawStretched(u32 id, u32 attr, short palette, short *rect, short align, GsOT *ot, int pri)
{
    Gfx_DrawImageStretched(&g_sprites[id & 0xffff],attr,palette,rect,align,ot,pri);
}

/* SpriteList_DrawScaled with the size given in pixels (the list's own size from
 * SpriteList_GetBounds) (0x8002B288) */
/* 0x8002B288 */
void SpriteList_DrawStretched(SpriteList *list, u32 attr, short palette, short *rect, u32 angle, GsOT *ot, int pri)
{
    short scale[4];

    scale[0] = rect[0];
    scale[1] = rect[1];
    scale[2] = (rect[2] << 12) / list->w;
    scale[3] = (rect[3] << 12) / list->h;
    SpriteList_DrawScaled(list,attr,palette,scale,angle,ot,pri);
}

/* Gfx_DrawImageRotScaled for sprite id (0x8002B358) */
/* 0x8002B358 */
void Sprite_DrawRotScaled(u32 id, u32 attr, short palette, short *pos, short align, int angle, int scale, GsOT *ot, int pri)
{
    Gfx_DrawImageRotScaled(&g_sprites[id & 0xffff],attr,palette,pos,align,angle,scale,ot,pri);
}

/* g_sprites[id] through Gfx_DrawImageEx (rotated and scaled) (0x8002B3B8) */
/* 0x8002B3B8 */
void Sprite_DrawEx(u16 id, u32 attr, short palette, short *pos, short align, int angle, int scale, GsOT *ot, int pri)
{
    Gfx_DrawImageEx(g_sprites + id,attr,palette,pos,align,angle,scale,ot,pri);
}

/* Fade_SetBlack, g_renderEnabled = 1. (0x80036A70) */
/* 0x80036A70 */
void Gfx_EnableRender(void)
{
    Fade_SetBlack();
    g_renderEnabled = 1;
}

/* Draw the 320x240 8bpp background picture kept in VRAM at (0x2C0, 0), CLUT row 0x1E9 (0x80042CA0) */
/* 0x80042CA0 */
void Gfx_DrawBackground(void)
{
    VramImage img;
    short pos [4];

    img.mode = 1;
    img.x = 0x2c0;
    img.w = 0x140;
    img.h = 0xf0;
    img.clutY = 0x1e9;
    img.y = 0;
    img.clutX = 0;
    pos[0] = 0;
    pos[1] = 0;
    Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,0x1e);
}
