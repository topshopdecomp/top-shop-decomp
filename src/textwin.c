/*
 * textwin.c - text windows
 *
 * Windows of text lines in a frame: creating them, adding text, opening and closing them with an
 * animation, menus with a cursor and scroll arrows; and drawing the frames (the window, panel
 * and talk panel styles).
 */
#include "common.h"
#include "topshop.h"

/* Free all text windows and lines (0x8001F470) */
/* 0x8001F470 */
void TextWin_InitAll(void)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        g_textWinUsed[i / 8] &= ~g_bitMask32[i % 8];
        g_textWins[i].firstLine = -1;
    }
    for (i = 0; i < 0x140; i++)
    {
        g_textLines[i].next = -1;
    }
    g_textLineNext = 0;
    g_textWinCount = 0;
    g_textFocusOn = 0;
}

/* Allocate a text window (TEXTWIN_AUTO / -1 = center / fit the text); it is drawn on top. Returns
 * its id, -1 if all 16 are used (0x8001F558) */
/* 0x8001F558 */
short TextWin_Create(short x,short y,short cols,short rows)
{
    TextWin *w;
    short id;

    for (id = 0; id < TEXT_WINS; id++)
    {
        if ((g_textWinUsed[id / 8] & g_bitMask32[id % 8]) == 0)
        {
            break;
        }
    }
    if (id == TEXT_WINS)
    {
        return -1;
    }
    g_textWinUsed[id / 8] |= g_bitMask32[id % 8];
    w = &g_textWins[id];
    w->laidOut = 0;
    w->closing = 0;
    w->menuOn = 0;
    w->x = x;
    w->y = y;
    w->cols = cols;
    w->rows = rows;
    w->align = 0;
    w->pal = 0;
    w->numLines = 0;
    w->rowsPlaced = 0;
    w->arrowUp = 0;
    w->arrowDown = 0;
    w->state = TEXTWIN_IDLE;
    w->anim = 0;
    w->scroll = 0;
    w->depth = g_textWinCount;
    w->firstLine = -1;
    g_textWinCount++;
    return id;
}

/* Draw a text window: scroll arrows, the focus frame, its lines (clipped to the window), the menu
 * cursor bar and the frame (0x8001F6A0) */
/* 0x8001F6A0 */
void TextWin_Draw(short id, GsOT *ot, u16 pri)
{
    TextWin *w;
    short line;
    short prev;
    short top;
    int r;
    int g;
    int b;
    short pos[4];

    w = &g_textWins[id];
    w->align = 0;
    w->pal = 0;
    TextWin_Layout(id);
    TextWin_PlaceRows(id);
    /* the scroll arrows, animated */
    if (w->arrowUp == 1)
    {
        pos[0] = w->x + 16;
        pos[1] = w->y - 7;
        Sprite_DrawAligned(g_spriteGroups[0].base + g_frameVsync % 32 / 4 + 0xa2,0,PAL_DEFAULT,pos,
                           ALIGN_LEFT | ALIGN_TOP,ot,(short)pri);
    }
    if (w->arrowDown == 1)
    {
        pos[0] = w->x + 16;
        pos[1] = w->y + w->rows * 16 - 2;
        Sprite_DrawAligned(g_spriteGroups[0].base + g_frameVsync % 32 / 4 + 0xa2,0x800000,PAL_DEFAULT,pos,
                           ALIGN_LEFT | ALIGN_TOP,ot,(short)pri);
    }
    /* the focus frame: corner marks */
    if ((g_textFocusWin == id) && (g_textFocusOn == 1))
    {
        pos[0] = w->x;
        pos[1] = w->y;
        Sprite_Draw(g_spriteGroups[0].base + 0x8d,0,PAL_DEFAULT,pos,ot,(short)pri);
        pos[0] = pos[0] + 14 + w->cols * 7;
        Sprite_DrawAligned(g_spriteGroups[0].base + 0x8d,0x400000,PAL_DEFAULT,pos,ALIGN_RIGHT | ALIGN_TOP,ot,
                           (short)pri);
        pos[1] = pos[1] + 8 + w->rows * 16;
        Sprite_DrawAligned(g_spriteGroups[0].base + 0x8d,0xc00000,PAL_DEFAULT,pos,ALIGN_RIGHT | ALIGN_BOTTOM,ot,
                           (short)pri);
        pos[0] = w->x;
        Sprite_DrawAligned(g_spriteGroups[0].base + 0x8d,0x800000,PAL_DEFAULT,pos,ALIGN_LEFT | ALIGN_BOTTOM,ot,
                           (short)pri);
    }
    /* the lines in view (scroll counts 256 per row), clipped to the window */
    Gfx_SetDrawArea(0,ot,(short)pri);
    if (w->firstLine != -1)
    {
        line = w->firstLine;
        do
        {
            top = w->scroll / 256;
            if ((top <= g_textLines[line].row) && (g_textLines[line].row <= top + w->rows))
            {
                Text_DrawDefault(g_textLines[line].text,ot,pri);
            }
            prev = line;
            line = g_textLines[prev].next;
        } while (prev != g_textLines[prev].next);
        FntPrint("\n");
    }
    pos[0] = w->x;
    pos[1] = w->y;
    pos[2] = w->cols * 7 + 14;
    pos[3] = w->rows * 16 + 8;
    Gfx_SetDrawArea(pos,ot,(short)pri);
    /* the menu cursor bar, darker as the screen is tinted down */
    if (w->menuOn != 0)
    {
        g_boxFill.attribute = 0x40000000;
        g_boxFill.x = w->x + 4;
        g_boxFill.y = w->y + (w->menuRow + w->menuCursor) * 16 + 5;
        g_boxFill.h = 14;
        g_boxFill.w = w->cols * 7 + 6;
        r = (g_tintR - 0x80) * 2;
        g = (g_tintG - 0x80) * 2;
        b = (g_tintB - 0x80) * 2;
        g_boxFill.r = r < 0 ? 0 : r;
        g_boxFill.g = g < 0 ? 0 : g;
        g_boxFill.b = b < 0 ? 0 : b;
        GsSortBoxFill(&g_boxFill,ot,pri);
    }
    pos[0] = w->x;
    pos[1] = w->y;
    pos[2] = w->cols * 7 + 14;
    pos[3] = w->rows * 16 + 8;
    TextWin_DrawFrame(pos,ot,pri);
}

/* Draw a window frame {x, y, w, h}: corner sprites, stretched edges, tiled fill (0x8001FC80) */
/* 0x8001FC80 */
void TextWin_DrawFrame(short *rect, GsOT *ot, short pri)
{
    VramImage *corner;
    VramImage *edgeH;
    VramImage *edgeV;
    POLY_FT4 *poly;
    GsOT_TAG *tag;
    u16 clut;
    u16 tpage;
    short pos[4];
    short w;
    short h;
    u8 u;
    u8 v;

    /* sprites 0x92 corner, 0x93 top/bottom edge, 0x94 side edge, 0x95 fill of the common sprites,
     * all in the corner's texture page and palette */
    corner = &g_sprites[g_spriteGroups[0].base + 0x92];
    edgeH = &g_sprites[g_spriteGroups[0].base + 0x93];
    edgeV = &g_sprites[g_spriteGroups[0].base + 0x94];
    tag = ot->org + pri;
    clut = GetClut(corner->clutX,corner->clutY);
    tpage = corner->x / 64 + corner->y / 256 * 16;

    /* the corners: the same sprite flipped */
    pos[0] = rect[0];
    pos[1] = rect[1];
    Sprite_Draw(g_spriteGroups[0].base + 0x92,0,PAL_DEFAULT,pos,ot,pri);
    pos[0] += rect[2] - corner->w;
    Sprite_Draw(g_spriteGroups[0].base + 0x92,0x400000,PAL_DEFAULT,pos,ot,pri);
    pos[1] += rect[3] - corner->h;
    Sprite_Draw(g_spriteGroups[0].base + 0x92,0xc00000,PAL_DEFAULT,pos,ot,pri);
    pos[0] = rect[0];
    Sprite_Draw(g_spriteGroups[0].base + 0x92,0x800000,PAL_DEFAULT,pos,ot,pri);

    /* the top edge stretched between the corners, the bottom one flipped */
    u = edgeH->x * 4;
    v = edgeH->y;
    pos[0] += corner->w;
    pos[1] = rect[1];
    w = rect[2] - corner->w * 2;
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->clut = clut;
    poly->tpage = tpage;
    poly->x0 = pos[0];
    poly->y0 = pos[1];
    poly->x1 = pos[0] + w;
    poly->y1 = pos[1];
    poly->x2 = pos[0];
    poly->y2 = pos[1] + edgeH->h;
    poly->x3 = pos[0] + w;
    poly->y3 = pos[1] + edgeH->h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = edgeH->w + u - 1;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = edgeH->h + v;
    poly->u3 = edgeH->w + u - 1;
    poly->v3 = edgeH->h + v;
    AddPrim(tag,poly);
    g_primPtr = poly + 1;
    pos[1] += rect[3] - corner->h;
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->clut = clut;
    poly->tpage = tpage;
    poly->x0 = pos[0];
    poly->y0 = pos[1] + edgeH->h;
    poly->x1 = pos[0] + w;
    poly->y1 = pos[1] + edgeH->h;
    poly->x2 = pos[0];
    poly->y2 = pos[1];
    poly->x3 = pos[0] + w;
    poly->y3 = pos[1];
    poly->u0 = u;
    poly->v0 = v - 1;
    poly->u1 = edgeH->w + u - 1;
    poly->v1 = v - 1;
    poly->u2 = u;
    poly->v2 = edgeH->h + v - 1;
    poly->u3 = edgeH->w + u - 1;
    poly->v3 = edgeH->h + v - 1;
    AddPrim(tag,poly);
    g_primPtr = poly + 1;

    /* the left edge stretched between the corners, the right one flipped */
    u = edgeV->x * 4;
    v = edgeV->y;
    pos[0] -= corner->w;
    h = rect[3] - corner->h * 2;
    pos[1] = rect[1] + corner->h;
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->clut = clut;
    poly->tpage = tpage;
    poly->x0 = pos[0];
    poly->y0 = pos[1];
    poly->x1 = pos[0] + edgeV->w;
    poly->y1 = pos[1];
    poly->x2 = pos[0];
    poly->y2 = pos[1] + h;
    poly->x3 = pos[0] + edgeV->w;
    poly->y3 = pos[1] + h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = edgeV->w + u;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = edgeV->h + v - 1;
    poly->u3 = edgeV->w + u;
    poly->v3 = edgeV->h + v - 1;
    AddPrim(tag,poly);
    g_primPtr = poly + 1;
    pos[0] += rect[2] - corner->w;
    poly = (POLY_FT4 *)g_primPtr;
    SetPolyFT4(poly);
    poly->r0 = g_tintR;
    poly->g0 = g_tintG;
    poly->b0 = g_tintB;
    poly->clut = clut;
    poly->tpage = tpage;
    poly->x0 = pos[0] + edgeV->w;
    poly->y0 = pos[1];
    poly->x1 = pos[0];
    poly->y1 = pos[1];
    poly->x2 = pos[0] + edgeV->w;
    poly->y2 = pos[1] + h;
    poly->x3 = pos[0];
    poly->y3 = pos[1] + h;
    poly->u0 = u - 1;
    poly->v0 = v;
    poly->u1 = edgeV->w + u - 1;
    poly->v1 = v;
    poly->u2 = u - 1;
    poly->v2 = edgeV->h + v - 1;
    poly->u3 = edgeV->w + u - 1;
    poly->v3 = edgeV->h + v - 1;
    AddPrim(tag,poly);
    g_primPtr = poly + 1;

    /* the inside */
    pos[0] = rect[0] + 2;
    pos[1] = rect[1] + 2;
    pos[2] = rect[2] - 4;
    pos[3] = rect[3] - 4;
    Sprite_DrawTiled((short)(g_spriteGroups[0].base + 0x95),0,PAL_DEFAULT,pos,ALIGN_LEFT | ALIGN_TOP,ot,pri);
}

/* Once per window: size it to its text if cols/rows are -1, center it if x/y are TEXTWIN_AUTO, and
 * set every line's x and field width (0x800209A0) */
/* 0x800209A0 */
void TextWin_Layout(short id)
{
    TextWin *w;
    TextLine *line;
    short maxWidth;
    short maxRow;
    short i;

    w = &g_textWins[id];
    if (w->laidOut == 1)
    {
        return;
    }
    w->laidOut = 1;
    if ((w->x == TEXTWIN_AUTO) || (w->y == TEXTWIN_AUTO) || (w->cols == -1) || (w->rows == -1))
    {
        /* the widest line and the last row */
        maxWidth = 0;
        maxRow = 0;
        if (w->firstLine != -1)
        {
            i = w->firstLine;
            do
            {
                line = &g_textLines[i];
                if (maxRow < line->row)
                {
                    maxRow = line->row;
                }
                if (maxWidth < g_glyphNodes[line->text].u)
                {
                    maxWidth = g_glyphNodes[line->text].u;
                }
                i = line->next;
            } while (line->next != line - g_textLines);
            maxWidth++;
            maxRow++;
        }
        if (w->cols == -1)
        {
            w->cols = maxWidth;
        }
        if (w->rows == -1)
        {
            w->rows = maxRow;
        }
        if (w->x == TEXTWIN_AUTO)
        {
            w->x = 153 - w->cols / 2 * 7;
        }
        if (w->y == TEXTWIN_AUTO)
        {
            w->y = 116 - maxRow * 8;
        }
    }
    /* each line: its field is the window's width, 7 pixels in */
    if (w->firstLine < 0)
    {
        return;
    }
    i = w->firstLine;
    do
    {
        line = &g_textLines[i];
        if (line->next < 0)
        {
            return;
        }
        if (0 <= line->text)
        {
            g_glyphNodes[line->text].pal = w->cols;
            g_glyphNodes[line->text].cell = w->x + 7;
        }
        i = line->next;
    } while (line->next != line - g_textLines);
}

/* Set the screen y of every line from its row and the window's scroll position (0x80020C5C) */
/* 0x80020C5C */
void TextWin_PlaceRows(short id)
{
    TextWin *w;
    TextLine *line;
    short i;

    w = &g_textWins[id];
    if (w->rowsPlaced == 1)
    {
        return;
    }
    w->rowsPlaced = 1;
    if (w->firstLine == -1)
    {
        return;
    }
    i = w->firstLine;
    do
    {
        line = &g_textLines[i];
        if (line->next < 0)
        {
            return;
        }
        if (0 <= line->text)
        {
            /* 16 pixels per row; the scroll position counts 256 per row */
            g_glyphNodes[line->text].col = w->y + 5 + (line->row - (w->scroll / 256)) * 16 -
                                           (w->scroll % 256) / 16;
        }
        i = line->next;
    } while (line->next != line - g_textLines);
}

/* Append text to a window, one line per %K-separated part. Returns 0, -1 if out of lines
 * (0x80020D7C) */
/* 0x80020D7C */
int TextWin_AddText(short id, char *text)
{
    TextWin *w;
    short textId;
    short line;
    short failNext;
    short i;

    failNext = g_textLineNext;
    if ((g_textWinUsed[id / 8] & g_bitMask32[id % 8]) != 0)
    {
        w = &g_textWins[id];
        g_textAlign = w->align;
        g_textPalette = w->pal;
        /* a free line from g_textLineNext on */
        for (i = 0; i < TEXT_LINES; i++)
        {
            line = g_textLineNext;
            if (g_textLines[line].next == -1)
            {
                break;
            }
            if (g_textLineNext++ == TEXT_LINES - 1)
            {
                g_textLineNext = 0;
            }
        }
        failNext = g_textLineNext;
        if (i != TEXT_LINES)
        {
            if (w->firstLine == -1)
            {
                w->firstLine = g_textLineNext;
                w->lastLine = g_textLineNext;
            }
            else
            {
                /* (lastLine stays the old one, as in the original) */
                g_textLines[w->lastLine].next = g_textLineNext;
            }
            g_textLines[g_textLineNext].next = g_textLineNext;
            for (;;)
            {
                textId = Text_Build(text);
                line = g_textLineNext;
                g_textLines[line].text = textId;
                failNext = line + 1;
                if (textId == -1)
                {
                    break;
                }
                g_textLines[line].row = w->numLines;
                text = g_textContinue;
                w->numLines++;
                if (g_textContinue == NULL)
                {
                    w->laidOut = 0;
                    w->rowsPlaced = 0;
                    w->align = g_textAlign;
                    w->pal = g_textPalette;
                    return 0;
                }
                /* after %K the rest goes on a new line */
                g_textLineNext = line == TEXT_LINES - 1 ? 0 : line + 1;
                for (i = 0; i < TEXT_LINES; i++)
                {
                    line = g_textLineNext;
                    if (g_textLines[line].next == -1)
                    {
                        break;
                    }
                    if (g_textLineNext++ == TEXT_LINES - 1)
                    {
                        g_textLineNext = 0;
                    }
                }
                if (i == TEXT_LINES)
                {
                    return -1;
                }
                g_textLines[w->lastLine].next = g_textLineNext;
                g_textLines[g_textLineNext].next = g_textLineNext;
                w->lastLine = g_textLineNext;
            }
        }
    }
    g_textLineNext = failNext;
    return -1;
}

/* Free all lines (and their text) of a window (0x800210E8) */
/* 0x800210E8 */
void TextWin_ClearText(short id)
{
    TextLine *line;
    short i;
    short next;

    if (g_textWins[id].firstLine == -1)
    {
        return;
    }
    i = g_textWins[id].firstLine;
    do
    {
        line = &g_textLines[i];
        if (line->next == -1)
        {
            break;
        }
        Text_Free(line->text);
        next = line->next;
        line->next = -1;
        line->text = -1;
        if (i == next)
        {
            break;
        }
        i = next;
    } while (1);
    g_textWins[id].firstLine = -1;
    g_textWins[id].numLines = 0;
}

/* One frame of the opening animation: the frame grows from 16x16 to full size (0x800211E4) */
/* 0x800211E4 */
void TextWin_AnimOpen(short id)
{
    TextWin *w;
    short fullW;
    short fullH;
    short rect[4];

    w = &g_textWins[id];
    fullW = w->cols * 7 + 14;
    fullH = w->rows * 16 | 8;
    /* anim / 8 of the full size, at least 16 */
    rect[2] = fullW * (u16)w->anim / 8;
    if (rect[2] < 16)
    {
        rect[2] = 16;
    }
    else if (fullW < rect[2])
    {
        rect[2] = fullW;
    }
    rect[3] = fullH * (u16)w->anim / 8;
    if (rect[3] < 16)
    {
        rect[3] = 16;
    }
    else if (fullH < rect[3])
    {
        rect[3] = fullH;
    }
    rect[0] = w->x + (fullW - rect[2]) / 2;
    rect[1] = w->y + (fullH - rect[3]) / 2;
    TextWin_DrawFrame(rect,g_ot + g_otIndex,2);
    w->anim += 2;
    if ((rect[2] == fullW) && (rect[3] == fullH))
    {
        w->anim = 0;
        w->state = TEXTWIN_SHOWN1;
    }
}

/* One frame of the closing animation: the frame shrinks to 16x16 (0x8002140C) */
/* 0x8002140C */
void TextWin_AnimClose(short id)
{
    TextWin *w;
    short fullW;
    short fullH;
    short rect[4];

    w = &g_textWins[id];
    fullW = w->cols * 7 + 14;
    fullH = w->rows * 16 | 8;
    /* (8 - anim) / 8 of the full size, at least 16 */
    rect[2] = fullW * (8 - (u16)w->anim) / 8;
    if (rect[2] < 16)
    {
        rect[2] = 16;
    }
    else if (fullW < rect[2])
    {
        rect[2] = fullW;
    }
    rect[3] = fullH * (8 - (u16)w->anim) / 8;
    if (rect[3] < 16)
    {
        rect[3] = 16;
    }
    else if (fullH < rect[3])
    {
        rect[3] = fullH;
    }
    rect[0] = w->x + (fullW - rect[2]) / 2;
    rect[1] = w->y + (fullH - rect[3]) / 2;
    TextWin_DrawFrame(rect,g_ot + g_otIndex,2);
    w->anim += 2;
    if ((rect[2] == 16) && (rect[3] == 16))
    {
        w->state = TEXTWIN_CLOSED;
        w->anim = 0;
    }
}

/* Run every window's state (open, shown, close, free) and draw it, from depth g_textWinCount - 1
 * down to 0 (0x80021634) */
/* 0x80021634 */
void TextWin_DrawAll(void)
{
    TextWin *w;
    short depth;
    short id;

    for (depth = g_textWinCount - 1; depth >= 0; depth--)
    {
        for (id = 0; id < 16; id++)
        {
            if (((g_textWinUsed[id / 8] & g_bitMask32[id % 8]) != 0) && (g_textWins[id].depth == depth))
            {
                break;
            }
        }
        if (id == 16)
        {
            continue;
        }
        w = &g_textWins[id];
        switch (w->state)
        {
        case TEXTWIN_OPEN:
            TextWin_Layout(id);
            TextWin_PlaceRows(id);
            w->state = TEXTWIN_GROWING;
            /* fall through */
        case TEXTWIN_GROWING:
            TextWin_AnimOpen(id);
            break;
        case TEXTWIN_SHOWN1:
            w->state = TEXTWIN_SHOWN2;
            TextWin_Draw(id,&g_ot[g_otIndex],2);
            break;
        case TEXTWIN_SHOWN2:
            w->state = TEXTWIN_SHOWN;
            TextWin_Draw(id,&g_ot[g_otIndex],2);
            break;
        case TEXTWIN_SHOWN:
            if (w->closing != 0)
            {
                w->anim = 0;
                w->state = TEXTWIN_SHRINKING;
                TextWin_AnimClose(id);
            }
            else
            {
                TextWin_Draw(id,&g_ot[g_otIndex],2);
            }
            break;
        case TEXTWIN_SHRINKING:
            TextWin_AnimClose(id);
            break;
        case TEXTWIN_CLOSED:
            w->state = TEXTWIN_FREEING;
            /* fall through */
        case TEXTWIN_FREEING:
            w->anim++;
            if (14 < (u16)w->anim)
            {
                if (w->firstLine != -1)
                {
                    TextWin_ClearText(id);
                }
                TextWin_RemoveDepth(w->depth);
                g_textWinUsed[id / 8] &= ~g_bitMask32[id % 8];
                w->state = TEXTWIN_IDLE;
            }
            break;
        }
    }
}

/* Free every window in use (0x80021998) */
/* 0x80021998 */
void TextWin_FreeAll(void)
{
    short id;

    for (id = 0; id < TEXT_WINS; id++)
    {
        if ((g_textWinUsed[id / 8] & g_bitMask32[id % 8]) != 0)
        {
            if (g_textWins[id].firstLine != -1)
            {
                TextWin_ClearText(id);
            }
            TextWin_RemoveDepth(g_textWins[id].depth);
            g_textWinUsed[id / 8] &= ~g_bitMask32[id % 8];
            g_textWins[id].state = TEXTWIN_IDLE;
        }
    }
}

/* Free a window now (no closing animation) (0x80021AAC) */
/* 0x80021AAC */
void TextWin_Free(short id)
{
    if (g_textWins[id].firstLine != -1)
    {
        TextWin_ClearText(id);
    }
    TextWin_RemoveDepth(g_textWins[id].depth);
    g_textWinUsed[id / 8] = g_textWinUsed[id / 8] & ~g_bitMask32[id % 8];   /* the window's slot is free */
}

/* Start showing a window (opening animation) (0x80021B5C) */
/* 0x80021B5C */
void TextWin_Open(short id)
{
    if (g_textWins[id].state == TEXTWIN_IDLE)
    {
        g_textWins[id].anim = 0;
        g_textWins[id].state = TEXTWIN_OPEN;
    }
}

/* Start closing a shown window; it is freed after the closing animation (0x80021B9C) */
/* 0x80021B9C */
void TextWin_Close(short id)
{
    g_textWins[id].closing = 1;
}

/* Close the gap a freed window leaves in the drawing order (0x80021BC8) */
/* 0x80021BC8 */
void TextWin_RemoveDepth(char depth)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (depth < g_textWins[i].depth)
        {
            g_textWins[i].depth--;
        }
    }
    g_textWinCount--;
}

/* Show a menu cursor bar over rows row..row+count-1 (-1 keeps a value; count < 1 hides it)
 * (0x80021C44) */
/* 0x80021C44 */
void TextWin_SetMenu(short id, char row, char count, char cursor)
{
    if (count < 1)
    {
        g_textWins[id].menuOn = 0;
        return;
    }
    g_textWins[id].menuOn = 1;
    if (row != -1)
    {
        g_textWins[id].menuRow = row;
    }
    if (count != -1)
    {
        g_textWins[id].menuCount = count;
    }
    if (cursor != -1)
    {
        g_textWins[id].menuCursor = cursor;
    }
}

/* Selected menu item, 0 if the window has no menu (0x80021CE0) */
/* 0x80021CE0 */
int TextWin_GetCursor(short id)
{
    int next;

    next = 0;
    if (g_textWins[id].menuOn == 1)
    {
        next = g_textWins[id].menuCursor;
    }
    return next;
}

/* Move the menu cursor up, wrapping to the last item (0x80021D1C) */
/* 0x80021D1C */
void TextWin_CursorUp(short id)
{
    TextWin *win = &g_textWins[id];

    win->menuCursor = ((win->menuCursor == 0) ? win->menuCount : win->menuCursor) - 1;
}

/* Move the menu cursor down, wrapping to the first item (0x80021D64) */
/* 0x80021D64 */
void TextWin_CursorDown(short id)
{
    char menuCursor;

    menuCursor = g_textWins[id].menuCursor + 1;
    if (g_textWins[id].menuCursor + 1 == g_textWins[id].menuCount)
    {
        menuCursor = 0;
    }
    g_textWins[id].menuCursor = menuCursor;
}

/* Draw the blinking focus frame around window id (on = 1) or not (0x80021DA8) */
/* 0x80021DA8 */
void TextWin_SetFocus(short id, u16 on)
{
    g_textFocusWin = id;
    g_textFocusOn = on;
}

/* Show the 'more above' arrow (0x80021DB8) */
/* 0x80021DB8 */
void TextWin_SetArrowUp(short id, u8 on)
{
    g_textWins[id].arrowUp = on;
}

/* Show the 'more below' arrow (0x80021DE0) */
/* 0x80021DE0 */
void TextWin_SetArrowDown(short id, u8 on)
{
    g_textWins[id].arrowDown = on;
}

/* Sprites of the frames, in sprite group 0 */
#define FRAME_PANEL_FILL    0x8E
#define FRAME_PANEL_HEDGE   0x8F   /* horizontal edge */
#define FRAME_PANEL_CORNER  0x90
#define FRAME_PANEL_VEDGE   0x91   /* vertical edge */
#define FRAME_PLAIN_CORNER  0xF3
#define FRAME_PLAIN_HEDGE   0xF4
#define FRAME_PLAIN_VEDGE   0xF5
#define FRAME_TALK_CORNER   0xF6
#define FRAME_TALK_HEDGE    0xF7
#define FRAME_TALK_VEDGE    0xF8
#define FRAME_TALK_FILL     0xF9
#define FRAME_SPRITE(n)     GROUP_SPRITE(0,n)

/* The texture page of a sprite's image */
#define SPRITE_TPAGE(s)     ((s)->x / 64 + (s)->y / 256 * 16)

/* The four corners of a frame: sprite `corner` at each, mirrored to fit; pos is left at the
 * bottom left one */
static void Frame_DrawCorners(short *rect, int corner, u32 attr, short palette, short *pos, GsOT *ot, int pri)
{
    pos[0] = rect[0];
    pos[1] = rect[1];
    Sprite_Draw(corner,attr,palette,pos,ot,pri);
    pos[0] += rect[2] - g_sprites[corner].w;
    Sprite_Draw(corner,attr | GFX_FLIP_X,palette,pos,ot,pri);
    pos[1] += rect[3] - g_sprites[corner].h;
    Sprite_Draw(corner,attr | GFX_FLIP_X | GFX_FLIP_Y,palette,pos,ot,pri);
    pos[0] = rect[0];
    Sprite_Draw(corner,attr | GFX_FLIP_Y,palette,pos,ot,pri);
}

/* A flat-textured quad in ordering table entry tag: corners (left, top), (right, top), (left,
 * bottom), (right, bottom) showing the texture's (uLeft, vTop) .. (uRight, vBottom) (swapped
 * sides mirror it), in the tint color */
static void Frame_AddQuad(GsOT_TAG *tag, u16 clut, u16 tpage, short left, short top, short right, short bottom, u8 uLeft, u8 vTop, u8 uRight, u8 vBottom, int semiTrans)
{
    POLY_FT4 *quad = g_primPtr;

    SetPolyFT4(quad);
    quad->r0 = g_tintR;
    quad->g0 = g_tintG;
    quad->b0 = g_tintB;
    quad->clut = clut;
    quad->tpage = tpage;
    quad->x0 = left;
    quad->y0 = top;
    quad->x1 = right;
    quad->y1 = top;
    quad->x2 = left;
    quad->y2 = bottom;
    quad->x3 = right;
    quad->y3 = bottom;
    quad->u0 = uLeft;
    quad->v0 = vTop;
    quad->u1 = uRight;
    quad->v1 = vTop;
    quad->u2 = uLeft;
    quad->v2 = vBottom;
    quad->u3 = uRight;
    quad->v3 = vBottom;
    if (semiTrans)
    {
        SetSemiTrans(quad,1);
    }
    AddPrim(tag,quad);
    g_primPtr = quad + 1;
}

/* The top edge of a frame from x, y, w wide (sprite edge stretched), then the bottom one upside
 * down; y is left at the bottom edge */
static void Frame_AddHEdges(GsOT_TAG *tag, short *rect, VramImage *edge, u16 clut, u16 tpage, short x, short *y, short w, int semiTrans)
{
    u8 u = edge->x * 4;
    u8 v = edge->y;

    Frame_AddQuad(tag,clut,tpage,x,*y,x + w,*y + edge->h,u,v,u + edge->w - 1,v + edge->h,semiTrans);
    *y += rect[3] - edge->h;
    Frame_AddQuad(tag,clut,tpage,x,*y + edge->h,x + w,*y,u,v - 1,u + edge->w - 1,v + edge->h - 1,
                  semiTrans);
}

/* The left edge of a frame at x, y, h high, then the right one mirrored at x + right */
static void Frame_AddVEdges(GsOT_TAG *tag, VramImage *edge, u16 clut, u16 tpage, short x, short y, short h, short right, int semiTrans)
{
    u8 u = edge->x * 4;
    u8 v = edge->y;

    Frame_AddQuad(tag,clut,tpage,x,y,x + edge->w,y + h,u,v,u + edge->w,v + edge->h - 1,semiTrans);
    x += right;
    Frame_AddQuad(tag,clut,tpage,x + edge->w,y,x,y + h,u - 1,v,u + edge->w - 1,v + edge->h - 1,
                  semiTrans);
}

/* Draw a panel frame {x, y, w, h}: corner sprites, stretched edges and a tiled fill (sprites
 * FRAME_PANEL_*), and a shadow 4 pixels down and right (the right and bottom side: corners and
 * edges in the blank palette, semi-transparent, and a box) in the next ordering table entry;
 * palette PAL_BLANK = the frame semi-transparent (0x8002CD60) */
/* 0x8002CD60 */
void Frame_DrawPanel(short *rect, short palette, GsOT *ot, short pri)
{
    int corner;
    VramImage *cornerImg;
    VramImage *hEdge;
    VramImage *vEdge;
    GsOT_TAG *tag;
    u32 attr;
    u16 clut;
    u16 tpage;
    short pos[4];
    short y;

    corner = FRAME_SPRITE(FRAME_PANEL_CORNER);
    cornerImg = &g_sprites[corner];
    hEdge = &g_sprites[FRAME_SPRITE(FRAME_PANEL_HEDGE)];
    vEdge = &g_sprites[FRAME_SPRITE(FRAME_PANEL_VEDGE)];
    if (palette == PAL_BLANK)
    {
        attr = GFX_SEMITRANS;
        clut = GetClut(0,0x1fe);
    }
    else
    {
        attr = 0;
        clut = GetClut(cornerImg->clutX + palette * 16,cornerImg->clutY);
    }
    tag = ot->org + pri;
    tpage = SPRITE_TPAGE(cornerImg);           /* (the corner's page for the edges too) */
    Frame_DrawCorners(rect,corner,attr,palette,pos,ot,pri);
    y = rect[1];
    Frame_AddHEdges(tag,rect,hEdge,clut,tpage,rect[0] + cornerImg->w,&y,rect[2] - cornerImg->w * 2,
                    attr != 0);
    Frame_AddVEdges(tag,vEdge,clut,tpage,rect[0],rect[1] + cornerImg->h,rect[3] - cornerImg->h * 2,
                    rect[2] - cornerImg->w,attr != 0);
    pos[0] = rect[0] + 2;
    pos[1] = rect[1] + 2;
    pos[2] = rect[2] - 4;
    pos[3] = rect[3] - 4;
    Sprite_DrawTiled((short)FRAME_SPRITE(FRAME_PANEL_FILL),attr,palette,pos,0x12,ot,pri);

    /* the shadow */
    tag = ot->org + pri + 1;
    clut = GetClut(0,0x1fe);
    pos[0] = rect[0] + 4 + (rect[2] - cornerImg->w);
    pos[1] = rect[1] + 4;
    Sprite_Draw(corner,GFX_SEMITRANS | GFX_FLIP_X,PAL_BLANK,pos,ot,pri + 1);
    pos[1] += rect[3] - cornerImg->h;
    Sprite_Draw(corner,GFX_SEMITRANS | GFX_FLIP_X | GFX_FLIP_Y,PAL_BLANK,pos,ot,pri + 1);
    pos[0] = rect[0] + 4;
    Sprite_Draw(corner,GFX_SEMITRANS | GFX_FLIP_Y,PAL_BLANK,pos,ot,pri + 1);
    {
        short x = pos[0] + cornerImg->w;
        short w = rect[2] - cornerImg->w * 2;
        u8 u = hEdge->x * 4;
        u8 v = hEdge->y;

        y = rect[1] + 4 + (rect[3] - hEdge->h);
        Frame_AddQuad(tag,clut,tpage,x,y + hEdge->h,x + w,y,u,v - 1,u + hEdge->w - 1,v + hEdge->h - 1,1);
    }
    {
        short x = rect[0] + 4 + (rect[2] - cornerImg->w);
        short h = rect[3] - cornerImg->h * 2;
        u8 u = vEdge->x * 4;
        u8 v = vEdge->y;

        y = rect[1] + cornerImg->h + 4;
        Frame_AddQuad(tag,clut,tpage,x + vEdge->w,y,x,y + h,u - 1,v,u + vEdge->w - 1,v + vEdge->h - 1,1);
    }
    g_boxFill.attribute = GFX_SEMITRANS;
    g_boxFill.x = rect[0] + 6;
    g_boxFill.y = rect[1] + 6;
    g_boxFill.w = rect[2] - 4;
    g_boxFill.h = rect[3] - 4;
    g_boxFill.r = Wipe_Color(g_tintR);
    g_boxFill.g = Wipe_Color(g_tintG);
    g_boxFill.b = Wipe_Color(g_tintB);
    GsSortBoxFill(&g_boxFill,ot,pri + 1);
}

/* Frame_DrawPanel with the dialog frame sprites (FRAME_TALK_*), without the shadow; the right
 * edge is placed by the edge's width (0x8002E3EC) */
/* 0x8002E3EC */
void Frame_DrawTalk(short *rect, short palette, GsOT *ot, short pri)
{
    int corner;
    VramImage *cornerImg;
    VramImage *hEdge;
    VramImage *vEdge;
    GsOT_TAG *tag;
    u32 attr;
    u16 clut;
    short pos[4];
    short y;

    corner = FRAME_SPRITE(FRAME_TALK_CORNER);
    cornerImg = &g_sprites[corner];
    hEdge = &g_sprites[FRAME_SPRITE(FRAME_TALK_HEDGE)];
    vEdge = &g_sprites[FRAME_SPRITE(FRAME_TALK_VEDGE)];
    if (palette == PAL_BLANK)
    {
        attr = GFX_SEMITRANS;
        clut = GetClut(0,0x1fe);
    }
    else
    {
        attr = 0;
        clut = GetClut(cornerImg->clutX + palette * 16,cornerImg->clutY);
    }
    tag = ot->org + pri;
    Frame_DrawCorners(rect,corner,attr,palette,pos,ot,pri);
    y = rect[1];
    Frame_AddHEdges(tag,rect,hEdge,clut,SPRITE_TPAGE(hEdge),rect[0] + cornerImg->w,&y,
                    rect[2] - cornerImg->w * 2,attr != 0);
    Frame_AddVEdges(tag,vEdge,clut,SPRITE_TPAGE(vEdge),rect[0],rect[1] + cornerImg->h,
                    rect[3] - cornerImg->h * 2,rect[2] - vEdge->w,attr != 0);
    pos[0] = rect[0] + 3;
    pos[1] = rect[1] + 3;
    pos[2] = rect[2] - 6;
    pos[3] = rect[3] - 6;
    Sprite_DrawTiled((short)FRAME_SPRITE(FRAME_TALK_FILL),attr,palette,pos,0x12,ot,pri);
}

/* The plain frame: corners and edges (FRAME_PLAIN_*) in their own palette, no fill (0x8002F19C) */
/* 0x8002F19C */
void Frame_DrawPlain(short *rect, GsOT *ot, short pri)
{
    int corner;
    VramImage *cornerImg;
    VramImage *hEdge;
    VramImage *vEdge;
    GsOT_TAG *tag;
    u16 clut;
    short pos[2];
    short y;

    corner = FRAME_SPRITE(FRAME_PLAIN_CORNER);
    cornerImg = &g_sprites[corner];
    hEdge = &g_sprites[FRAME_SPRITE(FRAME_PLAIN_HEDGE)];
    vEdge = &g_sprites[FRAME_SPRITE(FRAME_PLAIN_VEDGE)];
    tag = ot->org + pri;
    clut = GetClut(cornerImg->clutX,cornerImg->clutY);
    Frame_DrawCorners(rect,corner,0,PAL_DEFAULT,pos,ot,pri);
    y = rect[1];
    Frame_AddHEdges(tag,rect,hEdge,clut,SPRITE_TPAGE(hEdge),rect[0] + cornerImg->w,&y,
                    rect[2] - cornerImg->w * 2,0);
    Frame_AddVEdges(tag,vEdge,clut,SPRITE_TPAGE(vEdge),rect[0],rect[1] + cornerImg->h,
                    rect[3] - cornerImg->h * 2,rect[2] - cornerImg->w,0);
}
