/*
 * stageclear.h - game mode 8: between story stages
 *
 * The declarations of src/stageclear.c: the work areas of its tasks, its globals and its functions.
 * See docs/modes.md.
 */

#ifndef STAGECLEAR_H
#define STAGECLEAR_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of StageClearMenu_Task */
typedef struct StageClearMenuWork
{
    signed char shown;   /* 0x00 */
    signed char moving;   /* 0x01 */
    signed char item;   /* 0x02 */
    unsigned char pad03[1];
    short fade;   /* 0x04 the band: 0x100 none, 0 all there */
    short animItem;   /* 0x06 the next row to slide in / out */
    short count;   /* 0x08 */
    unsigned char pad0A[2];
    int delay;   /* 0x0C */
    unsigned char pad10[84];
} StageClearMenuWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_clearedStage;  /* 0x800B2650 */

/* ---- Functions ---- */

/* src/stageclear.c */
int StageClear_Task(short *step, void *work);             /* 0x8007496C */
u32 StageClearMenu_Task(short *step, StageClearMenuWork *work); /* 0x80074C7C */

#endif /* STAGECLEAR_H */
