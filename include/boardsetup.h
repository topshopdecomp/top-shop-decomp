/*
 * boardsetup.h - setting up the board
 *
 * Its tasks, data, players and stores, season and month graphics, pausing and resuming the display
 * tasks.
 * The declarations of src/boardsetup.c: its constants, its globals and its functions.
 * See docs/board.md.
 */

#ifndef BOARDSETUP_H
#define BOARDSETUP_H

#include "common.h"
#include "psyq.h"
#include "bgmap.h"
#include "tilesheet.h"

/* Dates of timed effects are turn numbers: monthsElapsed * numPlayers + player. */
#define TURN_NUMBER(months, player)  ((months) * g_numPlayers + (player))

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern int g_pieceSpriteFiles[16];   /* 0x800A923C */
extern char g_boardOtOn;             /* 0x800B253B */
extern char g_frontOtOn;             /* 0x800B253F */
extern short g_charScriptsBlock;     /* 0x800B2592 */
extern short g_stageBlock;           /* 0x800B2598 */
extern short g_seasonBlock;          /* 0x800B259A */
extern u16 g_boardExtraBlock;        /* 0x800B259C */
extern short g_faceBlock;            /* 0x800B25A0 */
extern u16 g_portraitFace;           /* 0x800B25A4 */
extern short g_farBgTaskId;          /* 0x800B26A8 */
extern short g_loaderTaskId;         /* 0x800B26C2 */
extern short g_turnFxTaskId;         /* 0x800B26D0 */
extern short g_stageBgTaskId;        /* 0x800B26D2 */
extern short g_actionTaskIds[4];     /* 0x800B26D8 */
extern u16 g_boardMiscTaskId;        /* 0x800B26E2 */
extern short g_scriptTaskId;         /* 0x800B26E6 */
extern char *g_squareInfoWork;       /* 0x800B26EC */
extern short g_storesTaskId;         /* 0x800B26F0 */
extern u8 *g_scriptWork;             /* 0x800B26F4 */
extern short g_mapViewTaskId;        /* 0x800B2708 */
extern short g_diceTaskId;           /* 0x800B270C */
extern short g_piecesTaskId;         /* 0x800B2710 */
extern int *g_playerPanelWork;       /* 0x800B2714 */
extern short g_moveTaskId;           /* 0x800B271A */
extern void *g_faceData[6];          /* 0x800FFEF0 */
extern BgCell g_bg0Visible[374];     /* 0x800FFFC0 plane 0 visible cells (22 x 17) */
extern GsOT_TAG g_bgOt0Tags0[4];     /* 0x80101720 */
extern GsOT_TAG g_bgOt0Tags1[4];     /* 0x80101730 */
extern GsOT_TAG g_bgOt1Tags0[4];     /* 0x80101740 */
extern GsOT_TAG g_bgOt1Tags1[4];     /* 0x80101750 */
extern BgCell g_bg1Visible[374];     /* 0x80101760 plane 1 visible cells */
extern POLY_FT4 g_bg0Prims0[374];    /* 0x80102F10 plane 0 primitives (22 x 17 cells), buffer 0 */
extern POLY_FT4 g_bg0Prims1[374];    /* 0x80106980 */
extern GsOT_TAG g_bg0OtTags0[2];     /* 0x8010A3F0 plane 0 ordering table, buffer 0 */
extern GsOT_TAG g_bg0OtTags1[2];     /* 0x8010A3F8 */
extern POLY_FT4 g_bg1Prims0[374];    /* 0x8010A400 plane 1 primitives, buffer 0 */
extern POLY_FT4 g_bg1Prims1[374];    /* 0x8010DE70 */
extern GsOT g_bgOt0[2];              /* 0x801118E0 */
extern GsOT_TAG g_bg1OtTags0[2];     /* 0x80111908 plane 1 ordering table, buffer 0 */
extern GsOT_TAG g_bg1OtTags1[2];     /* 0x80111910 */
extern GsOT g_bgOt1[2];              /* 0x80111918 */
extern GsOT_TAG g_boardOtTags0[4];   /* 0x80111940 */
extern GsOT_TAG g_boardOtTags1[4];   /* 0x80111950 */
extern BgPlane g_bgPlanes[2];        /* 0x80111D60 */
extern GsOT_TAG g_frontOtTags0[16];  /* 0x80111E70 */
extern GsOT_TAG g_frontOtTags1[16];  /* 0x80111EB0 */
extern void *g_actionTaskWork[4];    /* 0x80115580 */
extern u32 g_sceneOtTags[128];       /* 0x80117910 */
extern TileFrame g_sheetFrames[3];   /* 0x80117C30 */
extern TileSheet g_sceneSheet;       /* 0x80117C60 */
/* display tasks (functions not identified yet); Task_SendMessage values seen:
 * 0x10 turn start, 1/6 show, 2/7 hide, 8/9 used around sub-screens */
extern short g_minimapTaskId, g_playerPanelTaskId, g_squareInfoTaskId, g_calendarTaskId, g_frameEndTaskId;

/* ---- Functions ---- */

/* src/boardsetup.c */
void Board_CreateTasks(void);                        /* 0x80047DC0 */
void Board_LoadData(void);                           /* 0x800480D4 */
void Board_InitGame(void);                           /* 0x80048C28 */
void Board_StartDisplayTasks(void);                  /* 0x80049404 */
void Board_LoadSeasonGraphics(unsigned int season);  /* 0x800494F4 */
void Board_LoadMonthGraphics(u32 month);             /* 0x800496AC */
void Board_LoadPieceSprites(u32 charId);             /* 0x800497BC */
void Board_ExpireEffects(void);                      /* 0x800498C8 */
void Board_FreeScripts(void);                        /* 0x80049B38 */
void Board_ResetOverlays(void);                      /* 0x80049B5C */
void Board_SuspendDisplayTasks(void);                /* 0x80049B84 */
void Board_SuspendDisplayTasksWith(short task);      /* 0x80049B8C */
void Board_ResumeDisplayTasks(void);                 /* 0x80049C1C */
void Board_ResumeDisplayTasksWith(short task);       /* 0x80049C24 */
void Board_Unused();                                 /* 0x80049F00 empty; Board_PhaseTurn passes the month, Board_LoadMonthGraphics nothing */
void Board_LoadBackground(unsigned int stage);       /* 0x80049F08 */
void Calendar_Advance(int months);                   /* 0x8007B2A8 */

#endif /* BOARDSETUP_H */
