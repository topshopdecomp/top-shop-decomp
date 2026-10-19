/*
 * debug.c - debug tools left in the game
 *
 * Code the developers used: outlined boxes (the minimap draws its frame with them), a VRAM
 * viewer opened by holding circle + R1 + R2, and dumping a VRAM rectangle to a file on the
 * development PC. Nothing in the game calls the viewer or the dump any more.
 */
#include "common.h"
#include "topshop.h"

/* Debug: print box (x, y, w, h) and draw it as four lines rotated by angle (4096 =
 * 360 degrees) around (x, y); the line color drifts with the screen tint. (0x8001A588) */
void Debug_DrawBox(short *box, int angle, GsOT *ot, u16 pri)
{
    GsLINE *line = (GsLINE *)&g_line;
    int s, c, i, x, y, v;
    int px[4], py[4];

    FntPrint("Box : %d, %d, %d, %d\n", box[0], box[1], box[2], box[3]);

    v = line->r + (g_tintR - 0x80) * 2;
    line->r = ((short)v < 0) ? 0 : ((short)v < 0x100) ? v : 0xff;
    v = line->g + (g_tintG - 0x80) * 2;
    line->g = ((short)v < 0) ? 0 : ((short)v < 0x100) ? v : 0xff;
    v = line->b + (g_tintB - 0x80) * 2;
    line->b = ((short)v < 0) ? 0 : ((short)v < 0x100) ? v : 0xff;

    s = rsin(angle);
    c = rcos(angle);
    /* corners: (-w, -h) (w-1, -h) (-w, h-1) (w-1, h-1), rotated, around (x, y) */
    px[0] = -box[2];
    px[2] = -box[2];
    px[1] = box[2] - 1;
    px[3] = box[2] - 1;
    py[0] = -box[3];
    py[1] = -box[3];
    py[2] = box[3] - 1;
    py[3] = box[3] - 1;
    for (i = 0; i < 4; i++)
    {
        x = c * px[i] - s * py[i];
        y = s * px[i] + c * py[i];
        px[i] = (x / 4096) + box[0];   /* (x + 0xfff) >> 12 for negatives: truncation */
        py[i] = (y / 4096) + box[1];
    }
    line->x0 = px[0]; line->y0 = py[0]; line->x1 = px[1]; line->y1 = py[1];
    GsSortLine(line, ot, pri);
    line->x0 = px[0]; line->y0 = py[0]; line->x1 = px[2]; line->y1 = py[2];
    GsSortLine(line, ot, pri);
    line->x0 = px[2]; line->y0 = py[2]; line->x1 = px[3]; line->y1 = py[3];
    GsSortLine(line, ot, pri);
    line->x0 = px[1]; line->y0 = py[1]; line->x1 = px[3]; line->y1 = py[3];
    GsSortLine(line, ot, pri);
}

/* Debug VRAM viewer controls: d-pad scrolls (with circle: moves the CLUT), R1/R2 change the bit
 * depth, cross closes (0x8002BED8) */
/* 0x8002BED8 */
void VramViewer_Input(void)
{
    if ((g_pads[0].pressed & PADRdown) != 0)
    {
        g_vramViewerState = 0;
    }
    /* the depth (0 4-bit, 1 8-bit, 2 16-bit) down / up, wrapping */
    if (g_pads[0].pressed & PADR1)
    {
        if (g_vramViewerDepth-- == IMG_MODE_4BIT)
        {
            g_vramViewerDepth = IMG_MODE_16BIT;
        }
    }
    if (g_pads[0].pressed & PADR2)
    {
        if (g_vramViewerDepth++ == IMG_MODE_16BIT)
        {
            g_vramViewerDepth = IMG_MODE_4BIT;
        }
    }
    if ((g_pads[0].held & PADRright) == 0)
    {
        if ((g_pads[0].held & PADLright) == 0)
        {
            if ((g_pads[0].held & PADLleft) != 0)
            {
                g_vramViewerX -= 4;
            }
        }
        else
        {
            g_vramViewerX += 4;
        }
        if ((g_pads[0].held & PADLup) != 0)
        {
            g_vramViewerY -= 4;
            return;
        }
        if ((g_pads[0].held & PADLdown) != 0)
        {
            g_vramViewerY += 4;
        }
    }
    else
    {
        if ((g_pads[0].repeat & PADLright) == 0)
        {
            if ((g_pads[0].repeat & PADLleft) != 0)
            {
                g_vramViewerClutX -= 0x10;
            }
        }
        else
        {
            g_vramViewerClutX += 0x10;
        }
        if ((g_pads[0].repeat & PADLup) != 0)
        {
            g_vramViewerClutY--;
            return;
        }
        if ((g_pads[0].repeat & PADLdown) != 0)
        {
            g_vramViewerClutY++;
            return;
        }
    }
}

/* Debug VRAM viewer: draw all 32 texture pages (2 rows of 16) at the scroll position in the
 * chosen depth, and print the settings (0x8002C070) */
/* 0x8002C070 */
void VramViewer_Draw(void)
{
    short x;
    short y;
    short row;
    short page;
    u16 tpage;

    g_sprite.u = 0;
    g_sprite.v = 0;
    g_sprite.r = 0x80;
    g_sprite.g = 0x80;
    g_sprite.b = 0x80;
    g_sprite.cx = g_vramViewerClutX;
    g_sprite.h = 0x100;
    g_sprite.cy = g_vramViewerClutY;
    switch (g_vramViewerDepth)
    {
    case IMG_MODE_4BIT:
        g_sprite.attribute = 0x8000000;
        g_sprite.w = 0x100;
        break;
    case IMG_MODE_8BIT:
        g_sprite.attribute = 0x9000000;
        g_sprite.w = 0x80;
        break;
    case IMG_MODE_16BIT:
        g_sprite.attribute = 0xa000000;
        g_sprite.w = 0x40;
        break;
    }
    tpage = 0;
    for (row = 0; row < 2; row++)
    {
        for (page = 0; page < 16; page++, tpage++)
        {
            /* a page is 64 halfwords: 256 pixels at 4 bits, 128 at 8 */
            x = page * 64 - g_vramViewerX;
            if (g_vramViewerDepth == IMG_MODE_4BIT)
            {
                x *= 4;
            }
            else if (g_vramViewerDepth == IMG_MODE_8BIT)
            {
                x *= 2;
            }
            y = row * 0x100 - g_vramViewerY;
            g_sprite.x = x;
            g_sprite.y = y;
            g_sprite.tpage = tpage;
            if (-g_sprite.w <= g_sprite.x && g_sprite.x < 320 && -g_sprite.h <= y && y < 240)
            {
                GsSortFastSprite(&g_sprite,g_ot + g_otIndex,0);
            }
        }
    }
    g_boxFill.attribute = 0;
    g_boxFill.w = 0x140;
    g_boxFill.x = 0;
    g_boxFill.y = 0;
    g_boxFill.h = 0xf0;
    g_boxFill.r = 0;
    g_boxFill.g = 0;
    g_boxFill.b = 0;
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,0);
    Gfx_QueueClear(0,0,0);
    FntPrint("~c0F0TextuerPage Mie-ru\n");
    switch (g_vramViewerDepth)
    {
    case IMG_MODE_4BIT:  FntPrint("Bit Depth : 4\n"); break;
    case IMG_MODE_8BIT:  FntPrint("Bit Depth : 8\n"); break;
    case IMG_MODE_16BIT: FntPrint("Bit Depth : 16\n"); break;
    }
    FntPrint("( %d, %d ) : ( %d, %d )\n", g_vramViewerX, g_vramViewerY, g_vramViewerClutX,
             g_vramViewerClutY);
    g_debugPrint = 1;
}

/* Debug VRAM viewer, run every frame: opens when circle + R1 + R2 are held (0x8002C368) */
/* 0x8002C368 */
void VramViewer_Update(void)
{
    if (g_vramViewerState == 1)
    {
        g_vramViewerX = 0x140;
        g_vramViewerClutY = 0x1e0;
        g_vramViewerY = 0;
        g_vramViewerClutX = 0;
        g_vramViewerDepth = g_vramViewerState;
        g_vramViewerState = 2;
    }
    else
    {
        if (g_vramViewerState < 2)
        {
            if (g_vramViewerState != 0)
            {
                return;
            }
            if ((g_pads[0].held & (PADR2 | PADR1 | PADRright)) != 0x2a)
            {
                return;
            }
            g_vramViewerState = 1;
            return;
        }
        if (g_vramViewerState != 2)
        {
            return;
        }
    }
    VramViewer_Draw();
    VramViewer_Input();
}

/* Debug: write a VRAM rectangle (x, y, w, h) to a PC file: "DC", the width and height, then the
 * pixels a line at a time, all 16-bit big-endian; -1 if the file can't be made or written
 * (0x80042D28) */
/* 0x80042D28 */
u32 Debug_DumpVram(short *rect, char *path)
{
    long fd;
    RECT line;
    int x;
    int y;

    fd = open(path,0x202);          /* O_CREAT | O_WRONLY */
    if (fd == -1)
    {
        return -1;
    }
    g_vramDumpHeader[0] = 0x4344;   /* "DC" */
    g_vramDumpHeader[1] = rect[2] << 8 | (u16)rect[2] >> 8;
    g_vramDumpHeader[2] = rect[3] << 8 | (u16)rect[3] >> 8;
    write(fd,g_vramDumpHeader,6);
    line.x = rect[0];
    line.y = rect[1];
    line.w = rect[2];
    line.h = 1;
    for (y = 0; y < rect[3]; y++)
    {
        StoreImage(&line,(u_long *)g_vramDumpBuf);
        for (x = 0; x < rect[2]; x++)
        {
            g_vramDumpBuf[x] = g_vramDumpBuf[x] << 8 | g_vramDumpBuf[x] >> 8;
        }
        if (write(fd,g_vramDumpBuf,rect[2] * 2) == -1)
        {
            close(fd);
            return -1;
        }
        line.y++;
    }
    close(fd);
    return 0;
}
