/*
 * sys.h - boot initialization and the end of every frame
 *
 * Boot initialization, the VSync callback, the end of every frame, fonts and the common sprites
 * loaded at start-up.
 * The declarations of src/sys.c: its constants, its globals and its functions.
 * See docs/system.md.
 */

#ifndef SYS_H
#define SYS_H

#include "common.h"
#include "psyq.h"
#include "gfx.h"

#define CUR_OT (&g_ot[(int)g_otIndex])

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern int g_frameVsync;                   /* 0x800B23B8 */
extern u32 g_padHistoryPos;                /* 0x800B23D0 */
extern char g_renderEnabled;               /* 0x800B23D4 */
extern int g_vsyncCount;                   /* 0x800B23D8 */
extern char g_debugPrint;                  /* 0x800B23EA */
extern int g_vsyncInterval;                /* 0x800B23EC VSync() argument in Sys_EndFrame */
extern char g_otIndex;                     /* 0x800B23F6 */
extern char g_renderEnabledLast;           /* 0x800B23FC */
extern char g_skipFrameClearLast;          /* 0x800B23FE */
extern u8 g_fadeUnused;                    /* 0x800B23FF */
extern DR_ENV g_drEnvPackets[2];           /* 0x800B38D0 frame start packets, one per buffer */
extern DRAWENV g_drawEnv;                  /* 0x800B3970 */
extern GsOT_TAG g_otTags0[32];             /* 0x800B3A88 g_ot[0] (length 5) */
extern GsOT_TAG g_otTags1[32];             /* 0x800B3B08 g_ot[1] */
extern u8 g_packetArea[2][0xC000];         /* 0x800B3CF0 GsSetWorkBase packet area per buffer */
extern GsOT g_ot[2];                       /* 0x800CBCF0 */
extern SpriteGroup g_spriteGroupTable[7];  /* 0x800FE140 */
extern u16 g_otIndexInit;                  /* 0x80119364 */

/* ---- Functions ---- */

/* src/sys.c */
void Sys_VSyncCallback(void);      /* 0x800196E8 */
void Sys_Init(void);               /* 0x80019AF4 */
void Sys_EndFrame(void);           /* 0x80019DEC */
void Sys_LoadLoadingScreen(void);  /* 0x80036A98 */
void Sys_LoadFonts(void);          /* 0x80036BC0 */
void Sys_LoadCommonSprites(void);  /* 0x80036CB8 */
void Sys_ResetFonts(void);         /* 0x80036EC0 */
void Sys_Set30Fps(void);           /* 0x8004A078 */

#endif /* SYS_H */
