/*
 * main.h - the entry point and the game-mode loop
 *
 * The game modes (MODE_*, g_gameMode) and the tasks every mode starts with (Mode_CreateSubTasks).
 * The declarations of src/main.c: its constants, its globals and its functions.
 * See docs/system.md.
 */

#ifndef MAIN_H
#define MAIN_H

#include "common.h"

/* g_gameMode values */
#define MODE_LOGO        0
#define MODE_TITLE       2
#define MODE_TITLE_INTRO 3   /* marker while Title_Task runs its intro; main has no task for it */
#define MODE_NEW_GAME    5
#define MODE_BOARD       6
#define MODE_ENDING      7
#define MODE_STAGE_CLEAR 8
#define MODE_STAFF_ROLL  9
#define MODE_UNUSED10    10
#define MODE_CONTINUE    0xB /* keep main's frame loop running after the mode task ends */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_auxTaskId;        /* 0x800B23BC */
extern short g_modeTaskId;       /* 0x800B23E8 */
extern int g_gameMode;           /* 0x800B23F8 */
extern short g_cardTaskId;       /* 0x800B26AA */
extern short g_uiTaskIds[8];     /* 0x801155A8 */
extern int *g_uiTaskWork[8];     /* 0x801155B8 */
extern short g_subTaskIds[16];   /* 0x80117E50 */
extern void *g_subTaskWork[16];  /* 0x80117E70 */

/* ---- Functions ---- */

/* src/main.c */
int Mode10_Stub(short *step, void *work);  /* 0x8001C950 */
void Mode_CreateSubTasks(void);            /* 0x80042A3C */

#endif /* MAIN_H */
