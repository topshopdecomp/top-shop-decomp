/*
 * menu.h - pieces shared by the setup menus
 *
 * The sliding entries, the bands, the hand cursor, the characters' animations.
 * The declarations of src/menu.c: its types and constants, its globals and its functions.
 * See docs/modes.md.
 */

#ifndef MENU_H
#define MENU_H

#include "common.h"
#include "gfx.h"

/* A character animated on a setup screen (player menu, name entry). */
typedef struct MenuChar
{
    signed char shown;           /* 0x00 */
    unsigned char unused01;      /* 0x01 not used */
    unsigned short anim;         /* 0x02 index into k_charAnims* */
    unsigned short lastAnim;     /* 0x04 anim the frame counter belongs to */
    unsigned short frame;        /* 0x06 CharAnimFrame index */
    unsigned short timer;        /* 0x08 frames the current picture has been shown */
    unsigned short unused0A;     /* 0x0A set to 0, not read */
    short x, y;                  /* 0x0C */
} MenuChar;                      /* size 0x10 */

/* An animated entry of the setup and stage clear menus (g_menuItems[16], 0x80117D10),
 * drawn by MenuItems_Draw. */
/* MenuItem.state (MenuItems_Draw) */
#define MENUITEM_STILL      0
#define MENUITEM_SLIDE_IN   1   /* from the right, then unfolds */
#define MENUITEM_UNFOLD     2
#define MENUITEM_FOLD       3   /* then slides out to the left */
#define MENUITEM_SLIDE_OUT  4

typedef struct MenuItem
{
    unsigned char state;         /* 0x00 MENUITEM_* */
    unsigned char pad01;
    short sprite;                /* 0x02 sprite of group 5 */
    short offsetX;               /* 0x04 slides from 0x140 to 0 when it comes in */
    short offsetY;               /* 0x06 */
    short x, y;                  /* 0x08 */
    int scale;                   /* 0x0C horizontal, 0x1000 = 1 */
    int scaleY;                  /* 0x10 */
} MenuItem;                      /* size 0x14 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u16 g_frameCursorPal;             /* 0x800B25C8 */
extern short g_menuRowItems[6];          /* 0x800B27F0 */
extern SpriteList g_menuSpriteList;      /* 0x800FEDF0 */
extern SpritePart g_menuSpriteParts[5];  /* 0x800FEE00 */
extern MenuItem g_menuItems[16];         /* 0x80117D10 */

/* ---- Functions ---- */

/* src/menu.c */
u16 MenuChar_Animate(MenuChar *ch, int charId);  /* C-only helper, no original address */
void Menu_DrawBand(short y, int h, int fade);    /* 0x80037ECC */
void MenuItems_Draw(u16 count);                  /* 0x80038054 */
void Menu_DrawLinesUnused(void);                 /* 0x80038338 */
void Menu_DrawHand(short y);                     /* C-only helper, no original address */
void Menu_DrawFrameCursor(short x, short y);     /* 0x80062174 */

#endif /* MENU_H */
