/*
 * menu.c - pieces shared by the setup menus
 *
 * The entries that slide in and out, the bands behind them, the hand cursor and the frame
 * cursor, and the characters' animations on the setup screens.
 */
#include "common.h"
#include "topshop.h"

/* The setup-screen animation of a character (charId) shown as ch: returns the picture to draw
 * this frame and advances ch->frame / ch->timer */
u16 MenuChar_Animate(MenuChar *ch, int charId)
{
    CharAnimFrame *anim;

    if (ch->lastAnim != ch->anim)
    {
        ch->frame = 0;
        ch->timer = 0;
        ch->lastAnim = ch->anim;
    }
    if (charId == 0)
    {
        anim = k_charAnimsChar0[ch->anim];
    }
    else if (charId == 4)
    {
        anim = k_charAnimsChar4[ch->anim];
    }
    else
    {
        anim = k_charAnims[ch->anim];
    }
    anim += ch->frame;
    ch->timer++;
    if (ch->timer == (char)anim->frames)
    {
        if (anim->next == -2)
        {
            ch->frame++;
        }
        else
        {
            ch->frame = anim->next;
        }
        ch->timer = 0;
    }
    return anim->sprite;
}

/* A full-width semi-transparent band behind a menu row, h lines high around y, narrowed to nothing
 * as fade goes from 0 to 0x100; dark blue at the normal tint (0x80037ECC) */
/* 0x80037ECC */
void Menu_DrawBand(short y, int h, int fade)
{
    g_drawPos[0] = 0;
    g_drawPos[1] = y + (h / 2) - 1;
    g_drawPos[2] = 0x140;
    g_drawPos[3] = h * (0x100 - fade) / 0x100;
    if (g_drawPos[3] <= 0)
    {
        return;
    }
    g_boxFill.y = g_drawPos[1] - ((g_drawPos[3] - 1) / 2);
    g_boxFill.attribute = GFX_SEMITRANS;
    g_boxFill.x = 0;
    g_boxFill.w = 0x140;
    g_boxFill.r = TINT_CLAMP((g_tintR - 0x80) * 2);
    g_boxFill.g = TINT_CLAMP((g_tintG - 0x80) * 2);
    g_boxFill.b = TINT_CLAMP((g_tintB - 0x80) * 2 + 0x80);
    g_boxFill.h = g_drawPos[3];
    g_drawPos[1] = g_boxFill.y;
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,0x1d);
}

/* Animate and draw the menu entries g_menuItems[0 .. count - 1]: state 1 slides in from the right
 * (32 pixels a frame), 2 unfolds (scaleY up to 1.0), 3 folds, 4 slides out to the left, 0 still.
 * An entry's sprite of group 5, or (sprite 0 or less) the two-part picture g_menuSpriteList,
 * centered on its line (0x80038054) */
/* 0x80038054 */
void MenuItems_Draw(u16 count)
{
    MenuItem *item;
    int i;

    for (i = 0; i < (short)count; i++)
    {
        item = &g_menuItems[i];
        switch(item->state)
        {
        case MENUITEM_SLIDE_IN:
            item->offsetX -= 0x20;
            if (item->offsetX < 1)
            {
                item->offsetX = 0;
                item->state = MENUITEM_UNFOLD;
            }
            break;
        case MENUITEM_UNFOLD:
            item->scaleY += 0x333;
            if (item->scaleY > 0xfff)
            {
                item->scaleY = 0x1000;
                item->state = MENUITEM_STILL;
            }
            break;
        case MENUITEM_FOLD:
            item->scaleY -= 0x333;
            if (item->scaleY < 0x334)
            {
                item->scaleY = 0x333;
                item->state = MENUITEM_SLIDE_OUT;
            }
            break;
        case MENUITEM_SLIDE_OUT:
            item->offsetX -= 0x20;
            if (item->offsetX < -0x13f)
            {
                item->offsetX = -0x140;
                item->state = MENUITEM_STILL;
            }
            break;
        }
        g_drawPos[0] = item->offsetX + item->x;
        g_drawPos[2] = item->scale;
        g_drawPos[3] = item->scaleY;
        if (item->sprite < 1)
        {
            g_drawPos[1] = item->offsetY + item->y + g_sprites[g_menuSpriteList.parts->sprite].h / 2 - 1;
            SpriteList_DrawScaled(&g_menuSpriteList,0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
        }
        else
        {
            g_drawPos[1] = item->offsetY + item->y + g_sprites[GROUP_SPRITE(5,item->sprite)].h / 2 - 1;
            Sprite_DrawZoomed((u16)GROUP_SPRITE(5,item->sprite),0,PAL_DEFAULT,g_drawPos,2,g_ot + g_otIndex,8);
        }
    }
}

/* Not called: horizontal lines over the screen (0x80038338) */
/* 0x80038338 */
void Menu_DrawLinesUnused(void)
{
    int i;

    for (i = 0; i < 0xf0; i += 0xc)
    {
        g_drawPos[0] = 0;
        g_drawPos[2] = 0x13f;
        g_line.x1 = i;
        g_line.attribute = GFX_SEMITRANS;
        g_line.x0 = 0;
        g_line.y0 = 0x13f;
        g_line.r = TINT_CLAMP((g_tintR - 0x80) * 2 + 0xff);
        g_line.g = TINT_CLAMP((g_tintG - 0x80) * 2 + 0xff);
        g_line.b = TINT_CLAMP((g_tintB - 0x80) * 2 + 0xff);
        g_line.y1 = g_line.x1;
        g_drawPos[1] = g_line.x1;
        g_drawPos[3] = g_line.x1;
        GsSortLine(&g_line,g_ot + g_otIndex,8);
    }
    for (i = 0; i < 0x140; i += 0x10)
    {
        g_line.x0 = i;
        g_drawPos[1] = 0;
        g_drawPos[3] = 0xef;
        g_line.attribute = GFX_SEMITRANS;
        g_line.x1 = 0;
        g_line.y1 = 0xef;
        g_line.r = TINT_CLAMP((g_tintR - 0x80) * 2 + 0xff);
        g_line.g = TINT_CLAMP((g_tintG - 0x80) * 2 + 0xff);
        g_line.b = TINT_CLAMP((g_tintB - 0x80) * 2 + 0xff);
        g_line.y0 = g_line.x0;
        g_drawPos[0] = g_line.x0;
        g_drawPos[2] = g_line.x0;
        GsSortLine(&g_line,g_ot + g_otIndex,8);
    }
}

/* The pointing hand at y (sprite group 5, four frames) */
void Menu_DrawHand(short y)
{
    g_drawPos[0] = 0x18;
    g_drawPos[1] = y;
    Sprite_DrawAligned(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                       g_ot + g_otIndex,8);
}

/* Draw the animated frame cursor ("waku", sprite 0x7B, palette cycling) at x, y (0x80062174) */
/* 0x80062174 */
void Menu_DrawFrameCursor(short x, short y)
{
    short palette;

    /* the 15 palettes in turn, one a frame */
    palette = (short)(g_frameCursorPal + 1) % 15;
    g_frameCursorPal = palette;
    g_drawPos[0] = x;
    g_drawPos[1] = y;
    Sprite_DrawAligned(GROUP_SPRITE(0,0x7b),0,palette,g_drawPos,ALIGN_TOP,g_frontOt + g_otIndex,0xf);
    FntPrint("waku : %d, %d\n", g_sprites[g_spriteGroups->base + 0x7b].clutX,
             g_sprites[g_spriteGroups->base + 0x7b].clutY);
}
