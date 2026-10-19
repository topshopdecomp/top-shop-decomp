/*
 * fade.h - screen fades, tints and wipes
 *
 * Screen fades, tints and the shape wipes between turns.
 * The declarations of src/fade.c: the work areas of its tasks, its globals and its functions.
 * See docs/graphics.md.
 */

#ifndef FADE_H
#define FADE_H

#include "common.h"
#include "psyq.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Wipe_OpenTask */
typedef struct WipeOpenWork
{
    int size;   /* 0x00 */
    unsigned char pad04[96];
} WipeOpenWork;

/* work area of Wipe_CloseTask */
typedef struct WipeCloseWork
{
    int size;   /* 0x00 */
    unsigned char pad04[96];
} WipeCloseWork;

/* work area of Fade_AnimTask */
typedef struct FadeAnimWork
{
    unsigned char pad00[4];
    int frames;   /* 0x04 */
    int fadeR;   /* 0x08 */
    int fadeG;   /* 0x0C */
    int fadeB;   /* 0x10 */
    int stepR;   /* 0x14 */
    int stepG;   /* 0x18 */
    int stepB;   /* 0x1C */
    unsigned char pad20[68];
} FadeAnimWork;

/* work area of Tint_AnimTask */
typedef struct TintAnimWork
{
    int slot;   /* 0x00 */
    int frames;   /* 0x04 */
    int targetR;   /* 0x08 */
    int targetG;   /* 0x0C */
    int targetB;   /* 0x10 */
    int stepR;   /* 0x14 */
    int stepG;   /* 0x18 */
    int stepB;   /* 0x1C */
    unsigned char pad20[68];
} TintAnimWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short *g_charWipeShapes[16];  /* 0x800AA760 */
extern short g_tintSlotB[2];         /* 0x800B23C0 */
extern short g_tintSlotG[2];         /* 0x800B23C4 */
extern short g_tintSlotR[2];         /* 0x800B23C8 */
extern u8 g_skipFrameClear;          /* 0x800B23FD 0: each frame starts with a clear to black (Sys_EndFrame); 1 during / after a fade in */
extern char g_fadeActive;            /* 0x800B25B8 */
extern short g_fadeTaskId;           /* 0x800B25BA */
extern int *g_fadeWork;              /* 0x800B25BC */
extern short g_tintTaskId;           /* 0x800B25C0 */
extern int *g_tintWork;              /* 0x800B25C4 */
extern int g_fadeR;                  /* 0x80111EF0 */
extern int g_fadeG;                  /* 0x80111EF4 */
extern int g_fadeB;                  /* 0x80111EF8 */
extern int g_tintAnimR;              /* 0x80111F00 */
extern int g_tintAnimG;              /* 0x80111F04 */
extern int g_tintAnimB;              /* 0x80111F08 */

/* ---- Functions ---- */

/* src/fade.c */
u8 Wipe_Color(short tint);                                /* C-only helper, no original address */
void Wipe_DrawShape(short *shape, int scale, GsOT *ot, u16 pri); /* 0x8002C414 */
int Wipe_Scale(short *shape, short size);                 /* 0x8002FDF0 */
int Wipe_OpenTask(short *step, WipeOpenWork *work);       /* 0x80049CB4 */
int Wipe_CloseTask(short *step, WipeCloseWork *work);     /* 0x80049DD8 */
int Fade_AnimTask(short *step, FadeAnimWork *work);       /* 0x80055A2C */
u32 Tint_AnimTask(short *step, TintAnimWork *work);       /* 0x80055CB0 */
void Fade_DrawOverlay(void);                              /* 0x80056030 */
/* fade (0x80056124..) */
void Fade_Init(void);                                     /* 0x80056124 */
u32 Fade_GetLevel(u16 *r, u16 *g, u16 *b);                /* 0x8005615C */
u32 Fade_SetLevel(int r, int g, int b);                   /* 0x800561BC */
int  Fade_SetClear(void);                                 /* 0x80056210 */
int  Fade_SetBlack(void);                                 /* 0x8005624C */
u32 Fade_SetWhite(void);                                  /* 0x80056290 */
int  Fade_In(int frames);                                 /* 0x800562D4 */
int  Fade_Out(int frames);                                /* 0x8005631C */
u32 Fade_FromWhite(u32 frames);                           /* 0x8005636C */
u32 Fade_ToWhite(u32 frames);                             /* 0x800563B4 */
u32 Fade_To(int r, int g, int b, u32 frames);             /* 0x80056404 */
u32 Tint_To(int r, int g, int b, u32 frames, short slot); /* 0x80056464 */
int  Fade_IsDone(void);                                   /* 0x800564C4 */
void Tint_Poll(void);                                     /* 0x800564F8 */
int Fade_ApplyTask(void);                                 /* 0x8005651C */

#endif /* FADE_H */
