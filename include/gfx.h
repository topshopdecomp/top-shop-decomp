/*
 * gfx.h - drawing images and sprites
 *
 * Drawing images and sprites (plain, aligned, scaled, rotated, tiled, numbers), loading sprite sets
 * and TIM streams, the ordering-table helpers.
 * The declarations of src/gfx.c: its types and constants, its globals and its functions.
 * See docs/graphics.md.
 */

#ifndef GFX_H
#define GFX_H

#include "common.h"
#include "psyq.h"

/* An image in VRAM. Also the 16-byte entry format of .SPC sprite sets
 * (there w is in VRAM 16-bit units; Sprite_LoadSet converts it to pixels). */
typedef struct VramImage
{
    short mode;                  /* 0x00 IMG_MODE_*: 4bpp, 8bpp, 16bpp */
    short clutX;                 /* 0x02 CLUT position in VRAM */
    short clutY;                 /* 0x04 */
    short palette;               /* 0x06 default palette offset (see Gfx_DrawImage) */
    short x;                     /* 0x08 VRAM x (16-bit units) */
    short y;                     /* 0x0A VRAM y */
    short w;                     /* 0x0C width in pixels */
    short h;                     /* 0x0E height */
} VramImage;                     /* size 0x10 */

/* VramImage.mode: the color depth (texture page mode; also the VRAM viewer's depth) */
#define IMG_MODE_4BIT   0        /* 4 bits a pixel, 16-color CLUT: 4 pixels a VRAM unit */
#define IMG_MODE_8BIT   1        /* 8 bits, 256-color CLUT: 2 pixels a VRAM unit */
#define IMG_MODE_16BIT  2        /* 16-bit direct color, no CLUT */

/* Range of g_sprites owned by one sprite group. */
typedef struct SpriteGroup
{
    short size;                  /* 0x00 capacity */
    short base;                  /* 0x02 first sprite id */
    short next;                  /* 0x04 next free id (Sprite_LoadSet appends here) */
} SpriteGroup;

/* A screen position */
typedef struct ScreenPos
{
    short x, y;
} ScreenPos;

/* One frame of a character animation: on the setup screens (k_charAnims*) and the board pieces
 * (k_pieceAnims). */
typedef struct CharAnimFrame
{
    short sprite;                /* 0x00 */
    unsigned char flip;          /* 0x02 xor'ed into the sprite attributes (board pieces; the setup screens ignore it) */
    unsigned char frames;        /* 0x03 frames the picture stays */
    signed char next;            /* 0x04 frame to go to afterwards; -2 = the following one */
    unsigned char unused05;      /* 0x05 not read */
} CharAnimFrame;                 /* size 6 */

/* A character picture file (the faces KAO?P.J in g_faceData, the results pictures CHR?P0.J):
 * after a 0x14-byte header, at the offset in word 1, one 16-color CLUT per player color; word
 * 2 + n: the offset of the n-th packed image (an LZSS TIM, or a sprite set) */
#define CHARFILE_CLUT(data, player)  ((unsigned long *)((unsigned char *)(data) + 0x14 + ((int *)(data))[1] + (player) * 0x20))
#define CHARFILE_IMAGE(data, n)      ((int *)((unsigned char *)(data) + ((int *)(data))[2 + (n)]))

/* Gfx_DrawImage attr bits */
#define GFX_SEMITRANS  0x40000000   /* semi-transparency on */
#define GFX_ABR_SHIFT  28           /* bits 28-29: blend mode (1 = additive) */
#define GFX_FLIP_X     0x00400000
#define GFX_FLIP_Y     0x00800000
#define GFX_FLIP_NONE  0

/* Gfx_DrawImage palette argument */
#define PAL_DEFAULT    (-100)       /* use img->palette */
#define PAL_BLANK      (-101)       /* CLUT at (0, 0x1FE) */

/* Sprite_DrawAligned align bits */
#define ALIGN_HCENTER  0x00
#define ALIGN_RIGHT    0x01
#define ALIGN_LEFT     0x02
#define ALIGN_VCENTER  0x00
#define ALIGN_TOP      0x10
#define ALIGN_BOTTOM   0x20

/* The two fields as the drawing routines switch on them: align & 3, (align >> 4) & 3 */
#define ALIGN_H_CENTER  ALIGN_HCENTER          /* centered on the position */
#define ALIGN_H_RIGHT   ALIGN_RIGHT            /* the right edge at the position */
#define ALIGN_H_LEFT    ALIGN_LEFT             /* the left edge at the position */
#define ALIGN_V_CENTER  (ALIGN_VCENTER >> 4)   /* centered on the position */
#define ALIGN_V_TOP     (ALIGN_TOP >> 4)       /* the top edge at the position */
#define ALIGN_V_BOTTOM  (ALIGN_BOTTOM >> 4)    /* the bottom edge at the position */

#define NUM_SPRITES    1005

/* A color component (0-255) computed from a tint value (g_tintR..B, 0x80 = none) */
#define TINT_CLAMP(v)  ((v) < 0 ? 0 : (v) > 0xff ? 0xff : (v))

/* Sprite n of sprite group g (g_spriteGroups[g]; group 0 is the common set) */
#define GROUP_SPRITE(g, n)  (g_spriteGroups[g].base + (n))
/* The menus' pointing hand (sprite group 0), its four frames */
#define MENU_HAND(frameVsync)  (GROUP_SPRITE(0,0xb1) + (frameVsync) / 8 % 4)

/* A picture made of several sprites (SpriteList_*): the parts, ended by sprite -1, and
 * the size SpriteList_GetBounds computes from them. */
typedef struct SpritePart
{
    short sprite;                  /* 0x00 g_sprites index; -1 ends the list */
    short x, y;                    /* 0x02 offset in the picture */
    short flip;                    /* 0x06 PART_* (SpriteList_Draw) */
} SpritePart;                      /* size 8 */

/* SpritePart.flip */
#define PART_NO_FLIP   0
#define PART_FLIP_Y    1           /* flipped vertically */
#define PART_FLIP_X    2           /* flipped horizontally */
#define PART_FLIP_XY   3           /* both */

typedef struct SpriteList
{
    short w, h;                    /* 0x00 bounding box (SpriteList_GetBounds) */
    SpritePart *parts;             /* 0x04 */
} SpriteList;                      /* size 8 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_clearQueued;               /* 0x800B23CC */
extern short g_rect[4];                   /* 0x800B23E0 */
extern short g_tintR, g_tintG, g_tintB;   /* 0x800B23F4 / 0x800B23F2 / 0x800B23F0 */
extern SpriteGroup *g_spriteGroups;       /* 0x800B24D8 */
extern GsSPRITE g_sprite;                 /* 0x800B38A8 */
extern GsIMAGE g_timInfo;                 /* 0x800B3950 */
extern GsLINE g_line;                     /* 0x800B3CD0 */
extern GsBOXF g_boxFill;                  /* 0x800B3CE0 */
extern int g_bufferOrigin[4];             /* 0x800CBD18 */
extern short g_drawPos[4];                /* 0x800D09F8 */
extern VramImage g_sprites[NUM_SPRITES];  /* 0x800D0A38 */
extern DRAWENV g_drawEnvBase;             /* 0x801192E0 libgs GsDRAWENV */
extern void *g_primPtr;                   /* 0x80119428 */

/* ---- Functions ---- */

/* src/gfx.c */
void Gfx_QueueClear(unsigned char r, unsigned char g, unsigned char b); /* 0x8001A2FC */
void Gfx_SetDrawArea(void *area, GsOT *ot, int pri);      /* area: RECT-shaped x, y, w, h */     /* 0x8001A40C */
int  Sprite_LoadSet(int *spc, short group, short *vramOffsets); /* 0x80024DE0 */
void Tim_UploadStream(unsigned long *data, short *vramOffsets); /* 0x80024FA4 */
void Gfx_DrawImage(VramImage *img, unsigned long attr, short palette, short *pos,
                   GsOT *ot, int pri);                                                       /* 0x800251B0 */
void Sprite_DrawRotated(u32 id, u32 attr, short palette, short *pos, u32 align, u32 angle, GsOT *ot, int pri); /* 0x80025B9C */
void Sprite_DrawStretchedRotated(u32 id, u32 attr, short palette, short *rect, u16 align, int angle, GsOT *ot, int pri); /* 0x80026704 */
void Gfx_DrawVramImage(VramImage *img, unsigned long attr, short *pos, GsOT *ot,
                       unsigned short pri);                                                  /* 0x80026F48 */
void Gfx_DrawNumber(int value, int x, short y, short digits, short spacing, short digit0, char palette, GsOT *ot, short pri); /* 0x800272E8 */
void Sprite_DrawZoomed(u32 id, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri); /* 0x80027490 */
void Gfx_DrawImageStretched(VramImage *img, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri); /* 0x800276F8 */
void Sprite_DrawTiled(int id, u32 attr, short palette, short *rect, u32 align, GsOT *ot, int pri); /* 0x80027F48 */
void SpriteList_Draw(SpriteList *list, u32 attr, short palette, short *pos, GsOT *ot, int pri); /* 0x8002824C */
void SpriteList_DrawScaled(SpriteList *list, u32 attr, short palette, short *rect, u32 angle, GsOT *ot, int pri); /* 0x800287E8 */
void SpriteList_GetBounds(SpriteList *out, SpritePart *parts); /* 0x800293DC */
void Gfx_DrawImageRotScaled(VramImage *img, u32 attr, short palette, short *pos, u32 align, int angle, int scale, GsOT *ot, int pri); /* 0x800294E8 */
void Gfx_DrawImageEx(VramImage *img, u32 attr, short palette, short *pos, u32 align, int angle, int scale, GsOT *ot, int pri); /* 0x8002A160 */
void Sprite_InitGroups(SpriteGroup *groups, short count); /* 0x8002ADE4 */
void Sprite_Draw(unsigned short id, unsigned long attr, short palette, short *pos,
                 GsOT *ot, int pri);                                                         /* 0x8002AE48 */
void Sprite_DrawAligned(unsigned short id, unsigned long attr, short palette, short *pos,
                        unsigned int align, GsOT *ot, int pri);                              /* 0x8002AE90 */
void Gfx_DrawImageAligned(VramImage *img, u32 attr, short palette, short *pos, u32 align, GsOT *ot, int pri); /* 0x8002B02C */
void Sprite_DrawScaled(u32 id, u32 attr, short palette, short *rect, short align, u32 angle, GsOT *ot, int pri); /* 0x8002B174 */
void Sprite_DrawStretched(u32 id, u32 attr, short palette, short *rect, short align, GsOT *ot, int pri); /* 0x8002B238 */
void SpriteList_DrawStretched(SpriteList *list, u32 attr, short palette, short *rect, u32 angle, GsOT *ot, int pri); /* 0x8002B288 */
void Sprite_DrawRotScaled(u32 id, u32 attr, short palette, short *pos, short align, int angle, int scale, GsOT *ot, int pri); /* 0x8002B358 */
void Sprite_DrawEx(u16 id, u32 attr, short palette, short *pos, short align, int angle, int scale, GsOT *ot, int pri); /* 0x8002B3B8 */
void Gfx_EnableRender(void);                              /* 0x80036A70 */
void Gfx_DrawBackground(void);                            /* 0x80042CA0 */

#endif /* GFX_H */
