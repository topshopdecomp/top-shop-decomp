/*
 * options.h - the turn options and the save prompt
 *
 * The turn options (event scenes, CPU speed, controllers, save) and the stage-mode save prompt.
 * The declarations of src/options.c: the work areas of its tasks and its functions.
 * See docs/board.md.
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of ControllerSelect_Task */
typedef struct ControllerSelectWork
{
    int win;   /* 0x00 */
    unsigned char player;   /* 0x04 */
    unsigned char pad05[95];
} ControllerSelectWork;

/* work area of TurnOptions_Task */
typedef struct TurnOptionsWork
{
    unsigned char pad00[4];
    unsigned char shown;   /* 0x04 */
    unsigned char result;   /* 0x05 */
    unsigned char redraw;   /* 0x06 */
    signed char cursor;   /* 0x07 */
    signed char slot;   /* 0x08 */
    signed char player;   /* 0x09 */
    short h;   /* 0x0A */
    unsigned short fade;   /* 0x0C */
    signed char count;   /* 0x0E */
    unsigned char busy;   /* 0x0F */
    signed char saveFailed;   /* 0x10 Save_Pack's result */
    unsigned char slotFree[4];   /* 0x11 Save_DescribeCurrent: 1 free, 0 used */
    unsigned char pad15[79];
} TurnOptionsWork;

/* work area of SavePrompt_Task */
typedef struct SavePromptWork
{
    unsigned char pad00[4];
    unsigned char shown;   /* 0x04 */
    unsigned char page;   /* 0x05 SAVE_PAGE_* */
    unsigned char redraw;   /* 0x06 set, never read */
    signed char slot;   /* 0x07 the slot at the cursor */
    short h;   /* 0x08 height of the darkened band */
    unsigned short fade;   /* 0x0A height of the band the list shows through (0-0xC0) */
    unsigned char busy;   /* 0x0C the card is watched (Card_Check every frame) */
    signed char saveFailed;   /* 0x0D Save_Pack's result */
    unsigned char slotFree[4];   /* 0x0E Save_DescribeCurrent: 1 free, 0 used */
    unsigned char pad12[82];
} SavePromptWork;

/* ---- Functions ---- */

/* src/options.c */
int ControllerSelect_Task(short *step, ControllerSelectWork *work); /* 0x80062268 */
int TurnOptions_Task(short *step, TurnOptionsWork *work); /* 0x80062458 */
int SavePrompt_Task(short *step, SavePromptWork *work);   /* 0x80064054 */

#endif /* OPTIONS_H */
