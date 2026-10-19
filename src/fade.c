/*
 * fade.c - screen fades, tints and wipes
 *
 * The fade level and the tint every drawing routine applies, fading in and out from black or
 * white over a number of frames, tint animations, and the shape wipes that close and open the
 * screen between turns.
 */
#include "common.h"
#include "topshop.h"

/* A color component of the wipe: black, moved by the tint (0x80 = none), kept in 0-255 */
u8 Wipe_Color(short tint)
{
    int value = (tint - 0x80) * 2;

    if (value < 0)
    {
        return 0;
    }
    if (value > 0xff)
    {
        return 0xff;
    }
    return value;
}

/* A box of the wipe color */
static void Wipe_SortBox(short x, short y, short w, short h, GsOT *ot, u16 pri)
{
    g_boxFill.attribute = 0;
    g_boxFill.x = x;
    g_boxFill.y = y;
    g_boxFill.w = w;
    g_boxFill.h = h;
    g_boxFill.r = Wipe_Color(g_tintR);
    g_boxFill.g = Wipe_Color(g_tintG);
    g_boxFill.b = Wipe_Color(g_tintB);
    GsSortBoxFill(&g_boxFill,ot,pri);
}

/* A line of the wipe color (a box one line high) */
static void Wipe_SortLine(short left, short top, short right, short bottom, GsOT *ot, u16 pri)
{
    g_line.attribute = 0;
    g_line.x0 = left;
    g_line.y0 = top;
    g_line.x1 = right;
    g_line.y1 = bottom;
    g_line.r = Wipe_Color(g_tintR);
    g_line.g = Wipe_Color(g_tintG);
    g_line.b = Wipe_Color(g_tintB);
    GsSortLine(&g_line,ot,pri);
}

/* Wipe_DrawShape's states in a row */
#define WIPE_OUTSIDE   0   /* covering: the next value is where the shape starts */
#define WIPE_INSIDE    1   /* in the shape: the next value is where it ends */
#define WIPE_SKIP_ROW  2   /* the row starts on the same screen line as the last one: skipped */

/* Shape wipe: cover the screen except a run-length shape (a character silhouette: width, height,
 * two words, the number of values, then for each row the x where the shape starts and ends, any
 * number of times, and -1) scaled by scale (4.12) around the screen center, with boxes and lines
 * (one-line boxes) of the wipe color; it prints its geometry with FntPrint (0x8002C414) */
/* 0x8002C414 */
void Wipe_DrawShape(short *shape, int scale, GsOT *ot, u16 pri)
{
    short count;
    short *value;
    int originX;
    int rowY;
    int state;
    short coverFrom;
    short left;
    short right;
    short top;
    short bottom;
    short i;

    count = shape[4];
    value = shape + 5;
    originX = 160 - shape[0] * scale / 8192;
    rowY = 0x78000 - shape[1] * scale / 2;    /* 20.12 */
    FntPrint("org_x : %x\n", originX);
    FntPrint("org_y : %x\n", rowY);
    FntPrint("sc : %08x\n", scale);
    FntPrint("elmax : %x\n", count);
    if (rowY > 0xfff)
    {
        Wipe_SortBox(0,0,320,rowY >> 12,ot,pri);
    }
    coverFrom = 0;
    state = WIPE_OUTSIDE;
    for (i = 0; i < count; i++, value++)
    {
        left = coverFrom;
        if (*value == -1)
        {
            if (state == WIPE_INSIDE)
            {
                continue;
            }
            if (state == WIPE_OUTSIDE)
            {
                /* the rest of the row */
                top = rowY / 4096;
                bottom = (rowY + scale) / 4096;
                if (left < 320 && top < 240 && bottom > 0)
                {
                    if (left < 0)
                    {
                        left = 0;
                    }
                    if (top < 0)
                    {
                        top = 0;
                    }
                    if (bottom > 239)
                    {
                        bottom = 239;
                    }
                    if (bottom == top)
                    {
                        Wipe_SortLine(left,top,319,bottom,ot,pri);
                    }
                    else
                    {
                        Wipe_SortBox(left,top,320 - left,bottom - top + 1,ot,pri);
                        FntPrint("~c000x,y:%d,%d\n", left, top);
                        FntPrint("~c000w,h:%d,%d\n", (short)g_boxFill.w, (short)g_boxFill.h);
                    }
                }
            }
            /* the next row */
            coverFrom = 0;
            state = (rowY / 4096 == (rowY + scale) / 4096) ? WIPE_SKIP_ROW : WIPE_OUTSIDE;
            rowY += scale;
        }
        else if (state == WIPE_OUTSIDE)
        {
            /* covered up to where the shape starts */
            right = originX + *value * scale / 4096;
            top = rowY / 4096;
            bottom = (rowY + scale) / 4096;
            if (left < 320 && right > 0 && top < 240 && bottom > 0)
            {
                if (left < 0)
                {
                    left = 0;
                }
                if (right > 319)
                {
                    right = 319;
                }
                if (top < 0)
                {
                    top = 0;
                }
                if (bottom > 239)
                {
                    bottom = 239;
                }
                if (bottom == top)
                {
                    Wipe_SortLine(left,top,right,bottom,ot,pri);
                }
                else
                {
                    Wipe_SortBox(left,top,right - left + 1,bottom - top + 1,ot,pri);
                    FntPrint("~cF00x,y:%d,%d\n", left, top);
                    FntPrint("~cF00w,h:%d,%d\n", (short)g_boxFill.w, (short)g_boxFill.h);
                }
            }
            state = WIPE_INSIDE;
        }
        else if (state == WIPE_INSIDE)
        {
            coverFrom = originX + *value * scale / 4096;
            state = WIPE_OUTSIDE;
        }
    }
    if (rowY < 0xf0000)
    {
        top = rowY / 4096;
        Wipe_SortBox(0,top,320,240 - top,ot,pri);
    }
}

/* Scale (4.12) that makes the wipe shape size pixels wide (0x8002FDF0) */
/* 0x8002FDF0 */
int Wipe_Scale(short *shape, short size)
{
    if (size == 0)
    {
        return 0;
    }
    return (size << 12) / shape[0];
}

/* Wipe_OpenTask steps */
enum WipeOpenStep
{
    WIPEOPEN_INIT = 0,
    WIPEOPEN_GROW = 1   /* 40 a frame up to 400 */
};

/* Task: the current player's character-shape wipe opens (size 0 -> 400, 40 per frame) (0x80049CB4) */
/* 0x80049CB4 */
int Wipe_OpenTask(short *step, WipeOpenWork *work)
{
    int scale;
    int size;

    if (*step == WIPEOPEN_INIT)
    {
        work->size = 0;
        Fade_SetClear();
        *step += 1;
    }
    else if (*step != WIPEOPEN_GROW)
    {
        return 0;
    }
    scale = Wipe_Scale(g_charWipeShapes[(char)g_players[g_curPlayer].charId],work->size);
    Wipe_DrawShape(g_charWipeShapes[(char)g_players[g_curPlayer].charId],scale,g_ot + g_otIndex,0);
    size = work->size + 0x28;
    work->size = size;
    return (399 < size);
}

/* Wipe_CloseTask steps */
enum WipeCloseStep
{
    WIPECLOSE_INIT   = 0,
    WIPECLOSE_SHRINK = 1   /* down to 0 */
};

/* Task: the character-shape wipe closes (size 400 -> 0), then the screen is set black (0x80049DD8) */
/* 0x80049DD8 */
int Wipe_CloseTask(short *step, WipeCloseWork *work)
{
    int scale;
    int size;

    if (*step == WIPECLOSE_INIT)
    {
        work->size = 400;
        *step += 1;
    }
    else if (*step != WIPECLOSE_SHRINK)
    {
        return 0;
    }
    scale = Wipe_Scale(g_charWipeShapes[(char)g_players[g_curPlayer].charId],work->size);
    Wipe_DrawShape(g_charWipeShapes[(char)g_players[g_curPlayer].charId],scale,g_ot + g_otIndex,0);
    size = work->size - 0x28;
    work->size = size;
    if (size < 1)
    {
        Fade_SetBlack();
    }
    return (size < 1);
}

/* A fade or tint level (20.12) kept in -0xFF to 0xFF */
static int Fade_ClampLevel(int level)
{
    if (level > 0xff000)
    {
        return 0xff000;
    }
    if (level < -0xff000)
    {
        return -0xff000;
    }
    return level;
}

/* Fade_AnimTask steps */
enum FadeAnimStep
{
    FADEANIM_INIT = 0,  /* the step per frame */
    FADEANIM_RUN  = 1 
};

/* Task func: move the fade level (g_fadeR/G/B, 20.12) to the target over work->frames frames
 * (started by Fade_In / Fade_Out), then hand over to Fade_ApplyTask (0x80055A2C) */
/* 0x80055A2C */
int Fade_AnimTask(short *step, FadeAnimWork *work)
{
    if (*step == FADEANIM_INIT)
    {
        work->stepR = (work->fadeR - g_fadeR) / work->frames;
        work->stepG = (work->fadeG - g_fadeG) / work->frames;
        work->stepB = (work->fadeB - g_fadeB) / work->frames;
        *step = FADEANIM_RUN;
    }
    else if (*step != FADEANIM_RUN)
    {
        return 0;
    }
    if (--work->frames < 1)
    {
        g_fadeR = work->fadeR;
        g_fadeG = work->fadeG;
        g_fadeB = work->fadeB;
        Fade_DrawOverlay();
        g_fadeActive = 0;
        Task_Start(-1,Fade_ApplyTask);
    }
    else
    {
        g_fadeR = Fade_ClampLevel(g_fadeR + work->stepR);
        g_fadeG = Fade_ClampLevel(g_fadeG + work->stepG);
        g_fadeB = Fade_ClampLevel(g_fadeB + work->stepB);
        Fade_DrawOverlay();
        /* (always 1: the levels never get below -0xFF000) */
        g_skipFrameClear = (g_fadeB < -0xfeffff || g_fadeG < -0xfeffff) ? 0 : 1;
    }
    return 0;
}

/* Tint_AnimTask steps */
enum TintAnimStep
{
    TINTANIM_INIT = 0,  /* the step per frame */
    TINTANIM_RUN  = 1 
};

/* Task: move the color multiplier of tint slot work->slot to the target (work->targetR/G/B,
 * 20.12 about 0x80) over work->frames frames (0x80055CB0) */
/* 0x80055CB0 */
u32 Tint_AnimTask(short *step, TintAnimWork *work)
{
    if (*step == TINTANIM_INIT)
    {
        g_tintAnimR = (g_tintSlotR[work->slot] - 0x80) * 0x1000;
        g_tintAnimG = (g_tintSlotG[work->slot] - 0x80) * 0x1000;
        g_tintAnimB = (g_tintSlotB[work->slot] - 0x80) * 0x1000;
        work->stepR = (work->targetR - g_tintAnimR) / work->frames;
        work->stepG = (work->targetG - g_tintAnimG) / work->frames;
        work->stepB = (work->targetB - g_tintAnimB) / work->frames;
        *step = TINTANIM_RUN;
    }
    else if (*step != TINTANIM_RUN)
    {
        return 0;
    }
    if (--work->frames < 1)
    {
        g_tintSlotR[work->slot] = work->targetR / 0x1000 + 0x80;
        g_tintSlotG[work->slot] = work->targetG / 0x1000 + 0x80;
        g_tintSlotB[work->slot] = work->targetB / 0x1000 + 0x80;
        Task_Stop(-1);
        return 1;
    }
    g_tintAnimR = Fade_ClampLevel(g_tintAnimR + work->stepR);
    g_tintAnimG = Fade_ClampLevel(g_tintAnimG + work->stepG);
    g_tintAnimB = Fade_ClampLevel(g_tintAnimB + work->stepB);
    g_tintSlotR[work->slot] = g_tintAnimR / 0x1000 + 0x80;
    g_tintSlotG[work->slot] = g_tintAnimG / 0x1000 + 0x80;
    g_tintSlotB[work->slot] = g_tintAnimB / 0x1000 + 0x80;
    return 0;
}

/* Draw the fade level as a full-screen box: subtractive when darkening, additive when brightening
 * (0x80056030) */
/* 0x80056030 */
void Fade_DrawOverlay(void)
{
    int r;
    int g;
    int b;

    r = (short)(g_fadeR / 4096);
    if (r < 0)
    {
        r = -r;
    }
    g_boxFill.r = r;
    g = (short)(g_fadeG / 4096);
    if (g < 0)
    {
        g = -g;
    }
    g_boxFill.g = g;
    b = (short)(g_fadeB / 4096);
    if (b < 0)
    {
        b = -b;
    }
    g_boxFill.b = b;
    g_boxFill.w = 0x140;
    g_boxFill.x = 0;
    g_boxFill.y = 0;
    g_boxFill.h = 0xf0;
    if (g_fadeB < 0)
    {
        g_boxFill.attribute = 0x60000000;
    }
    else
    {
        g_boxFill.attribute = 0x50000000;
    }
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,0);
}

/* Create the two fade tasks (ids 0x800B25BA / 0x800B25C0, work 0x800B25BC / 0x800B25C4).
 * (0x80056124) */
/* 0x80056124 */
void Fade_Init(void)
{
    int id;

    id = Task_Create(0,&g_fadeWork);
    g_fadeTaskId = id;
    id = Task_Create(0,&g_tintWork);
    g_tintTaskId = id;
    g_fadeActive = 0;
}

/* Current fade level per channel (-255..255) (0x8005615C) */
/* 0x8005615C */
u32 Fade_GetLevel(u16 *r, u16 *g, u16 *b)
{
    *r = g_fadeR / 4096;
    *g = g_fadeG / 4096;
    *b = g_fadeB / 4096;
    return 0;
}

/* Set the fade level (-255..255 per channel) immediately (0x800561BC) */
/* 0x800561BC */
u32 Fade_SetLevel(int r, int g, int b)
{
    g_fadeR = (r << 0x10) >> 4;
    g_fadeG = (g << 0x10) >> 4;
    g_fadeB = (b << 0x10) >> 4;
    Task_Start(g_fadeTaskId,Fade_ApplyTask);
    return 0;
}

/* Set fade level to 0 (no darkening) immediately. (0x80056210) */
/* 0x80056210 */
int Fade_SetClear(void)
{
    g_fadeR = 0;
    g_fadeG = 0;
    g_fadeB = 0;
    Task_Start(g_fadeTaskId,Fade_ApplyTask);
    return 0;
}

/* Set fade level to -255.0 (0xFFF01000, 20.12 fixed) per channel immediately: screen black.
 * (0x8005624C) */
/* 0x8005624C */
int Fade_SetBlack(void)
{
    g_fadeR = -0xff000;
    g_fadeG = -0xff000;
    g_fadeB = -0xff000;
    Task_Start(g_fadeTaskId,Fade_ApplyTask);
    return 0;
}

/* Set the fade level to +255: screen white (0x80056290) */
/* 0x80056290 */
u32 Fade_SetWhite(void)
{
    g_fadeR = 0xff000;
    g_fadeG = 0xff000;
    g_fadeB = 0xff000;
    Task_Start(g_fadeTaskId,Fade_ApplyTask);
    return 0;
}

/* Animate fade level to 0 over frames (fade from black). Fade_IsDone polls. (0x800562D4) */
/* 0x800562D4 */
int Fade_In(int frames)
{
    short id;
    int *fadeWork;

    fadeWork = g_fadeWork;
    g_fadeWork[1] = frames & 0xffff;
    id = g_fadeTaskId;
    fadeWork[2] = 0;
    fadeWork[3] = 0;
    fadeWork[4] = 0;
    Task_Start(id,Fade_AnimTask);
    g_fadeActive = 1;
    return 0;
}

/* Animate fade level to -255.0 over frames (fade to black). (0x8005631C) */
/* 0x8005631C */
int Fade_Out(int frames)
{
    short id;
    int *fadeWork;

    fadeWork = g_fadeWork;
    g_fadeWork[1] = frames & 0xffff;
    id = g_fadeTaskId;
    fadeWork[2] = -0xff000;
    fadeWork[3] = -0xff000;
    fadeWork[4] = -0xff000;
    Task_Start(id,Fade_AnimTask);
    g_fadeActive = 1;
    return 0;
}

/* Animate the fade level to 0 over frames (as Fade_In; used after Fade_SetWhite) (0x8005636C) */
/* 0x8005636C */
u32 Fade_FromWhite(u32 frames)
{
    short id;
    int *fadeWork;

    fadeWork = g_fadeWork;
    g_fadeWork[1] = frames & 0xffff;
    id = g_fadeTaskId;
    fadeWork[2] = 0;
    fadeWork[3] = 0;
    fadeWork[4] = 0;
    Task_Start(id,Fade_AnimTask);
    g_fadeActive = 1;
    return 0;
}

/* Animate the fade level to +255 (white) over frames (0x800563B4) */
/* 0x800563B4 */
u32 Fade_ToWhite(u32 frames)
{
    short id;
    int *fadeWork;

    fadeWork = g_fadeWork;
    g_fadeWork[1] = frames & 0xffff;
    id = g_fadeTaskId;
    fadeWork[2] = 0xff000;
    fadeWork[3] = 0xff000;
    fadeWork[4] = 0xff000;
    Task_Start(id,Fade_AnimTask);
    g_fadeActive = 1;
    return 0;
}

/* Animate the fade level to r, g, b (-255..255) over frames (0x80056404) */
/* 0x80056404 */
u32 Fade_To(int r, int g, int b, u32 frames)
{
    short id;
    int *fadeWork;

    fadeWork = g_fadeWork;
    g_fadeWork[3] = (g << 0x10) >> 4;
    fadeWork[2] = (r << 0x10) >> 4;
    id = g_fadeTaskId;
    fadeWork[1] = frames & 0xffff;
    fadeWork[4] = (b << 0x10) >> 4;
    Task_Start(id,Fade_AnimTask);
    g_fadeActive = 1;
    return 0;
}

/* Animate the color multiplier of tint slot slot to r, g, b over frames (Tint_AnimTask)
 * (0x80056464) */
/* 0x80056464 */
u32 Tint_To(int r, int g, int b, u32 frames, short slot)
{
    short id;
    int *tintWork;

    tintWork = g_tintWork;
    g_tintWork[3] = (g << 0x10) >> 4;
    tintWork[2] = (r << 0x10) >> 4;
    id = g_tintTaskId;
    *tintWork = slot;
    tintWork[1] = frames & 0xffff;
    tintWork[4] = (b << 0x10) >> 4;
    Task_Start(id,Tint_AnimTask);
    return 0;
}

/* 1 when no fade is animating. (0x800564C4) */
/* 0x800564C4 */
int Fade_IsDone(void)
{
    int next;

    if (g_fadeActive == 0)
    {
        next = 1;
    }
    else
    {
        next = Task_IsDone(g_fadeTaskId);
    }
    return next;
}

/* Task_IsDone on the tint task (result unused) (0x800564F8) */
/* 0x800564F8 */
void Tint_Poll(void)
{
    Task_IsDone(g_tintTaskId);
}

/* Task func (started by Fade_SetBlack / Fade_SetClear, and by Fade_AnimTask when it is done):
 * draws the fade overlay every frame while the screen is tinted; ends at full black
 * (g_skipFrameClear = 0) or once the levels are back to 0. The original's (step, work) parameters are
 * not read (0x8005651C) */
/* 0x8005651C */
int Fade_ApplyTask(void)
{
    Fade_DrawOverlay();
    if (g_fadeB < -0xfefff || g_fadeG < -0xfefff)
    {
        g_skipFrameClear = 0;
    }
    else
    {
        g_skipFrameClear = 1;
        if (g_fadeB != 0 || g_fadeG != 0)
        {
            return 0;
        }
    }
    Task_Stop(-1);
    return 1;
}
