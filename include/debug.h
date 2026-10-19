/*
 * debug.h - debug tools left in the game
 *
 * The VRAM viewer, VRAM dumps to the PC, outlined boxes.
 * The declarations of src/debug.c: its globals and its functions.
 * See docs/system.md.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "common.h"
#include "psyq.h"

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_vramViewerState;  /* 0x800B2508 */
extern short g_vramViewerDepth;  /* 0x800B250A */
extern short g_vramViewerX;      /* 0x800B250C */
extern short g_vramViewerY;      /* 0x800B250E */
extern short g_vramViewerClutX;  /* 0x800B2510 */
extern short g_vramViewerClutY;  /* 0x800B2512 */
extern u16 g_vramDumpHeader[3];  /* 0x800B2570 */
extern u16 g_vramDumpBuf[2080];  /* 0x800FEEB0 VRAM dump line buffer (debug) */

/* ---- Functions ---- */

/* src/debug.c */
void Debug_DrawBox(short *box, int angle, GsOT *ot, unsigned short pri); /* 0x8001A588 */
void VramViewer_Input(void);                              /* 0x8002BED8 */
void VramViewer_Draw(void);                               /* 0x8002C070 */
void VramViewer_Update(void);                             /* 0x8002C368 */
u32 Debug_DumpVram(short *rect, char *path);              /* 0x80042D28 */

#endif /* DEBUG_H */
