/*
 * text.c - texts
 *
 * Building a text from a string with its control codes (%C, %K, %E, ...) into glyphs of the
 * cache, drawing it with a palette and measuring it; formatting numbers and padding strings,
 * half- and full-width; remapping Shift-JIS voiced marks.
 */
#include "common.h"
#include "topshop.h"

/* Append src to buf at *pos, advance *pos past it and terminate buf (the
 * terminator is not counted). (0x8001ABF8) */
void Str_AppendAt(char *buf, char *src, u16 *pos)
{
    while (*src != 0)
    {
        buf[*pos] = *src;
        src++;
        *pos = *pos + 1;
    }
    buf[*pos] = 0;
}

/* Build a text string: a head GlyphNode, then one node per character with its cached glyph. Codes:
 * %C/%R/%L center/right/left align, %Pnn palette, %Snn add nn columns, %K stop here (g_textContinue
 * = the rest), %E end; %% only skips its first %, so the second starts a code (it prints no
 * percent sign; no text uses it). Returns the text id (head node) (0x8001DFFC) */
/* 0x8001DFFC */
short Text_Build(char *text)
{
    GlyphNode *head;
    short headId;
    short node;
    short glyph;
    short prev;
    u8 *s;

    /* the head node: width so far (u) and alignment (v) */
    do
    {
        node = g_glyphNodeNext + 1;
        if (g_glyphNodeNext == GLYPH_NODES - 1)
        {
            node = 0;
        }
        g_glyphNodeNext = node;
    } while (g_glyphNodes[node].next != -1);
    headId = node;
    head = &g_glyphNodes[headId];
    head->u = 0;
    head->v = g_textAlign;
    for (;;)
    {
        s = (u8 *)text;
        if (*s == 0)
        {
            break;
        }
        if (*s == '%')
        {
            /* control codes */
            text = (char *)s + 1;
            switch (s[1])
            {
            case '%':
                continue;
            case 'C':
                head->v = TEXT_ALIGN_CENTER;
                g_textAlign = TEXT_ALIGN_CENTER;
                text = (char *)s + 2;
                continue;
            case 'R':
                head->v = TEXT_ALIGN_RIGHT;
                g_textAlign = TEXT_ALIGN_RIGHT;
                text = (char *)s + 2;
                continue;
            case 'L':
                head->v = TEXT_ALIGN_LEFT;
                g_textAlign = TEXT_ALIGN_LEFT;
                text = (char *)s + 2;
                continue;
            case 'E':
                g_textContinue = NULL;
                g_glyphNodes[g_glyphNodeNext].next = g_glyphNodeNext;
                return headId;
            case 'K':
                /* a new line: the rest goes to the next text */
                g_textContinue = (char *)s + 2;
                g_glyphNodes[g_glyphNodeNext].next = g_glyphNodeNext;
                return headId;
            case 'P':
                /* palette, two digits */
                g_textPalette = (s[2] - '0') * 10 + (s[3] - '0');
                text = (char *)s + 4;
                continue;
            case 'S':
                /* skip columns, two digits */
                head->u += (s[2] - '0') * 10 + (s[3] - '0');
                text = (char *)s + 4;
                continue;
            default:
                text = (char *)s + 2;
                continue;
            }
        }
        /* a character: a node after the last one, its glyph cached */
        prev = g_glyphNodeNext;
        do
        {
            node = prev + 1;
            if (prev == GLYPH_NODES - 1)
            {
                node = 0;
            }
            prev = node;
        } while (g_glyphNodes[node].next != -1);
        g_glyphNodes[g_glyphNodeNext].next = node;
        glyph = Font_CacheGlyph(s);
        g_glyphNodes[node].cell = glyph;
        /* the cell in VRAM: 16 per row of 64 units from x 0x340, 16 rows from y 0x100 */
        g_glyphNodes[node].u = glyph % 16 * 4 + glyph / 256 * 64 + 0x340;
        g_glyphNodes[node].v = glyph % 256 / 16 * 16 + 0x100;
        g_glyphNodes[node].pal = g_textPalette;
        g_glyphNodes[node].col = head->u;
        g_glyphNodeNext = node;
        head->u += g_glyphLastWidth / 8;
        g_glyphNodes[node].width = g_glyphLastWidth;
        text = (char *)s + g_glyphLastWidth / 8;
    }
    g_textContinue = NULL;
    g_glyphNodes[g_glyphNodeNext].next = g_glyphNodeNext;
    return headId;
}

/* Free a text string's nodes and the glyph cells it used (except reserved cells) (0x8001E33C) */
/* 0x8001E33C */
void Text_Free(short id)
{
    short node;
    short next;
    int cell;

    if (id < 0)
    {
        return;
    }
    next = g_glyphNodes[id].next;
    g_glyphNodes[id].next = -1;
    node = next;
    while (g_glyphNodes[node].next != -1)
    {
        cell = g_glyphNodes[node].cell;
        if (g_glyphCacheReserved < cell)
        {
            g_glyphCacheUsed[cell / 16] &= ~g_bitMask32[cell % 16];
        }
        next = g_glyphNodes[node].next;
        g_glyphNodes[node].next = -1;
        node = next;
    }
}

/* Str_FormatInt with Shift-JIS full-width digits (g_wideHexDigits); width 0 = no padding, else
 * right-aligned in width characters with full-width spaces (0x8001E468) */
/* 0x8001E468 */
void Str_FormatIntWide(int value, int width, u8 *out)
{
    u32 bcd;
    u16 ch;
    int positive;
    int started;
    int signDone;
    int i;

    if (8 < width)
    {
        width = 8;
    }
    positive = 0 <= value;
    if (!positive)
    {
        value = -value;
    }
    bcd = Util_ToBcd(value);
    if (width == 0)
    {
        /* as many digits as needed */
        if (!positive)
        {
            out[0] = 0x81;                  /* full-width minus */
            out[1] = 0x7c;
            out += 2;
        }
        started = false;
        for (i = 0; i < 8; i++)
        {
            ch = bcd >> ((7 - i) * 4) & 0xf;
            if (ch != 0)
            {
                started = true;
            }
            if (started || (i == 7))
            {
                ch = g_wideHexDigits[ch];
                out[0] = ch;
                out[1] = ch >> 8;
                out += 2;
            }
        }
        *out = 0;
        return;
    }
    /* right-aligned, from the last digit back */
    out[width * 2] = 0;
    signDone = false;
    for (i = (width - 1) * 2; i >= 0; i -= 2)
    {
        if (bcd != 0)
        {
            ch = g_wideHexDigits[bcd & 0xf];
        }
        else if (i == (width - 1) * 2)
        {
            ch = g_wideHexDigits[0];
        }
        else if (positive || signDone)
        {
            ch = 0x4081;                    /* full-width space */
        }
        else
        {
            signDone = true;
            ch = 0x7c81;                    /* full-width minus */
        }
        bcd >>= 4;
        out[i] = ch;
        out[i + 1] = ch >> 8;
    }
}

/* Draw a text string at its head's position, aligned in its field width, one sprite per glyph
 * (0x8001E974) */
/* 0x8001E974 */
void Text_Render(short id, GsOT *ot, u16 pri, short clutX, u16 clutY)
{
    GlyphNode *head;
    GlyphNode *g;
    short x;
    short y;

    if ((id == -1) || (g_glyphNodes[id].next == id))
    {
        return;
    }
    head = &g_glyphNodes[id];
    y = head->col;
    /* (another alignment leaves x unset, as in the original) */
    switch (head->v)
    {
    case TEXT_ALIGN_LEFT:
        x = head->cell;
        break;
    case TEXT_ALIGN_RIGHT:
        x = head->cell + (head->pal - head->u) * 7;
        break;
    case TEXT_ALIGN_CENTER:
        x = head->cell + ((head->pal - head->u) * 7 / 2);
        break;
    }
    do
    {
        id = g_glyphNodes[id].next;
        g = &g_glyphNodes[id];
        g_sprite.attribute = 0x8000000;
        g_sprite.x = x + g->col * 7;
        g_sprite.w = g->width;
        g_sprite.h = 16;
        g_sprite.tpage = g->u / 64 + (g->v / 256) * 16;
        g_sprite.u = (g->u & 0x3f) * 4;
        g_sprite.v = g->v;
        g_sprite.cy = clutY;
        g_sprite.cx = clutX + g->pal * 16;
        g_sprite.r = g_tintR;
        g_sprite.g = g_tintG;
        g_sprite.b = g_tintB;
        g_sprite.y = y;
        GsSortFastSprite(&g_sprite,ot,pri);
    } while (id != g_glyphNodes[id].next);
}

/* Text_Render with the font CLUT (0, 0x1FF) (0x8001EF1C) */
/* 0x8001EF1C */
void Text_DrawDefault(short id, GsOT *ot, short pri)
{
    Text_Render(id,ot,pri,0,0x1ff);
}

/* Decimal text of value via BCD (Util_ToBcd). width 0 = no padding, else right-aligned in width
 * chars (max 8). (0x8001EFFC) */
/* 0x8001EFFC */
void Str_FormatInt(int value, int width, char *out)
{
    u32 bcd;
    u32 digit;
    int positive;
    int started;
    int signDone;
    int i;

    if (8 < width)
    {
        width = 8;
    }
    positive = 0 <= value;
    if (!positive)
    {
        value = -value;
    }
    bcd = Util_ToBcd(value);
    if (width == 0)
    {
        /* as many digits as needed */
        if (!positive)
        {
            *out++ = '-';
        }
        started = false;
        for (i = 0; i < 8; i++)
        {
            digit = bcd >> ((7 - i) * 4) & 0xf;
            if (digit != 0)
            {
                started = true;
            }
            if (started || (i == 7))
            {
                *out++ = g_hexDigits[digit];
            }
        }
        *out = 0;
        return;
    }
    /* right-aligned, from the last digit back */
    signDone = false;
    out[width] = 0;
    for (i = width - 1; i >= 0; i--)
    {
        if (bcd != 0)
        {
            out[i] = g_hexDigits[bcd & 0xf];
        }
        else if (i == width - 1)
        {
            out[i] = g_hexDigits[0];
        }
        else if (positive || signDone)
        {
            out[i] = ' ';
        }
        else
        {
            signDone = true;
            out[i] = '-';
        }
        bcd >>= 4;
    }
}

/* Text_Build with palette 0, left-aligned (0x8001F168) */
/* 0x8001F168 */
short Text_Create(char *text)
{
    short id;

    g_textPalette = 0;
    id = Text_Build(text);
    if (0 <= id)
    {
        g_glyphNodes[id].v = TEXT_ALIGN_LEFT;
    }
    return id;
}

/* Draw a text string at x, y with the font CLUT (0x8001F1B4) */
/* 0x8001F1B4 */
void Text_Draw(short id, short x, short y, GsOT *ot, short pri)
{
    short u;

    g_glyphNodes[id].cell = x;
    u = g_glyphNodes[id].u;
    g_glyphNodes[id].col = y;
    g_glyphNodes[id].pal = u;
    Text_Render(id,ot,pri,0,0x1ff);
}

/* Width of a text string in 8-pixel columns (0x8001F21C) */
/* 0x8001F21C */
int Text_GetWidth(short id)
{
    return g_glyphNodes[id].u;
}

/* Set the palette of every glyph of a text string (0x8001F28C) */
/* 0x8001F28C */
void Text_SetPalette(int id, u16 pal)
{
    short next;
    int node;

    node = (short)id;
    if ((g_glyphNodes[node].next != node) && (node != g_glyphNodes[node].next))
    {
        do
        {
            id = g_glyphNodes[(short)id].next;
            next = g_glyphNodes[id].next;
            g_glyphNodes[id].pal = pal;
        } while (id != next);
    }
}

/* Draw a text string at x, y with the given CLUT (0x8001F310) */
/* 0x8001F310 */
void Text_DrawClut(short id, short x, short y, GsOT *ot, short pri, short clutX, short clutY)
{
    short u;

    u = g_glyphNodes[id].u;
    g_glyphNodes[id].cell = x;
    g_glyphNodes[id].col = y;
    g_glyphNodes[id].pal = u;
    Text_Render(id,ot,pri,clutX,clutY);
}

/* n Shift-JIS full-width spaces, NUL-terminated (0x8001F388) */
/* 0x8001F388 */
void Str_FillWideSpaces(int n, u16 *out)
{
    int i;

    for (i = 0; i < n; i++)
    {
        out[i] = 0x4081;                  /* bytes 0x81 0x40 */
    }
    *(u8 *)&out[i] = 0;
}

/* Blank text width half-width columns wide: full-width spaces, with half-width spaces for an odd
 * start address or an odd width (0x8001F3BC) */
/* 0x8001F3BC */
void Str_FillSpaces(int width, char *out)
{
    int i;

    if (((u32)out & 1) == 1)
    {
        *out++ = ' ';
        width--;
    }
    for (i = 0; i < width / 2 * 2; i += 2)
    {
        out[i] = 0x81;              /* full-width space */
        out[i + 1] = 0x40;
    }
    out[i] = 0;
    if (width / 2 * 2 == width - 1)
    {
        while (*out != 0)
        {
            out++;
        }
        out[0] = ' ';
        out[1] = 0;
    }
}

/* Add the voiced mark (dakuten) to the 2-byte Shift-JIS character at ch: か -> が, カ -> ガ ...
 * (k_sjisRemapA); 1 if it has one (0x80042BB8) */
/* 0x80042BB8 */
u32 Sjis_RemapA(char *ch)
{
    SjisPair *pair;

    for (pair = k_sjisRemapA; pair->from[0] != 0; pair++)
    {
        if (pair->from[0] == ch[0] && pair->from[1] == ch[1])
        {
            ch[0] = pair->to[0];
            ch[1] = pair->to[1];
            return 1;
        }
    }
    return 0;
}

/* Add the semi-voiced mark (handakuten) to the 2-byte Shift-JIS character at ch: は -> ぱ ...
 * (k_sjisRemapB); 1 if it has one (0x80042C2C) */
/* 0x80042C2C */
u32 Sjis_RemapB(char *ch)
{
    SjisPair *pair;

    for (pair = k_sjisRemapB; pair->from[0] != 0; pair++)
    {
        if (pair->from[0] == ch[0] && pair->from[1] == ch[1])
        {
            ch[0] = pair->to[0];
            ch[1] = pair->to[1];
            return 1;
        }
    }
    return 0;
}
