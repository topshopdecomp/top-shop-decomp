/*
 * title.h - game mode 2: the title screen
 *
 * Its animations, the intro and the attract demo.
 * The declarations of src/title.c: the work areas of its tasks, its globals and its functions.
 * See docs/modes.md.
 */

#ifndef TITLE_H
#define TITLE_H

#include "common.h"
#include "gfx.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Title_SplashTask */
typedef struct TitleSplashWork
{
    signed char started;   /* 0x00 */
    unsigned char pad01[3];
    int timer;   /* 0x04 */
    unsigned char pad08[92];
} TitleSplashWork;

/* work area of TitleMenu_Task */
typedef struct TitleMenuWork
{
    unsigned char active;   /* 0x00 input accepted */
    unsigned char pad01[3];
    int speed;   /* 0x04 */
    int speedY;   /* 0x08 */
    unsigned char wheelShown;   /* 0x0C */
    unsigned char pad0D[1];
    unsigned short wheelPalette;   /* 0x0E 8.8: the wheel's palette (0-13) */
    unsigned short spin;   /* 0x10 */
    unsigned char pad12[2];
    int angle;   /* 0x14 the wheel's */
    unsigned char logoPhase;   /* 0x18 */
    unsigned char logoShown;   /* 0x19 */
    unsigned short unused1A;   /* 0x1A */
    int logoX;   /* 0x1C */
    int logoY;   /* 0x20 */
    int titleY;   /* 0x24 */
    int titleX;   /* 0x28 */
    unsigned char signShown;   /* 0x2C */
    unsigned char pad2D[3];
    int signX;   /* 0x30 */
    int signY;   /* 0x34 */
    int signW;   /* 0x38 */
    int signH;   /* 0x3C */
    unsigned char effectState;   /* 0x40 0 none, 1 waiting effectDelay frames, 2 an effect task running */
    unsigned char pad41[1];
    short effectDelay;   /* 0x42 */
    int idleTimer;   /* 0x44 */
    unsigned char pad48[28];
} TitleMenuWork;

/* work area of TitleMenu_EffectATask */
typedef struct TitleMenuEffectAWork
{
    signed char started;   /* 0x00 */
    signed char kind;   /* 0x01 where the character comes out (0-5) */
    unsigned short reveal;   /* 0x02 8.8: 0-3, how far it is out (the sprite) */
    int timer;   /* 0x04 */
    unsigned char pad08[92];
} TitleMenuEffectAWork;

/* work area of TitleMenu_EffectBTask */
typedef struct TitleMenuEffectBWork
{
    signed char started;   /* 0x00 */
    signed char leftward;   /* 0x01 walking right to left (drawn mirrored) */
    unsigned short walk;   /* 0x02 8.8: walking animation frame 0-5 */
    int x;   /* 0x04 20.12 */
    int y;   /* 0x08 20.12 */
    unsigned char pad0C[88];
} TitleMenuEffectBWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern SpriteList g_titleSpriteList;       /* 0x800FEDF8 */
extern SpritePart g_titleSpriteParts[17];  /* 0x800FEE28 */

/* ---- Functions ---- */

/* src/title.c */
void Title_LoadGraphics(void);                            /* 0x80040AB8 */
int Title_Task(short *step, void *work);                  /* 0x80040EB0 */
u32 Title_SplashTask(u16 *step, TitleSplashWork *work);   /* 0x8004126C */
int TitleMenu_Task(short *step, TitleMenuWork *work);     /* 0x8004191C */
u32 TitleMenu_EffectATask(u16 *step, TitleMenuEffectAWork *work); /* 0x800422E0 */
u32 TitleMenu_EffectBTask(u16 *step, TitleMenuEffectBWork *work); /* 0x80042680 */

#endif /* TITLE_H */
