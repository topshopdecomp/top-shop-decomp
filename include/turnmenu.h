/*
 * turnmenu.h - the turn's command menu and its screens
 *
 * Map view, status, asset graph.
 * The declarations of src/turnmenu.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/board.md.
 */

#ifndef TURNMENU_H
#define TURNMENU_H

#include "common.h"
#include "psyq.h"

/* g_turnMenuChoice: the command TurnMenu_Task's menu picked */
#define TURNMENU_ROLL     0      /* roll the dice */
#define TURNMENU_MAP      1      /* Board_MapViewTask */
#define TURNMENU_STATUS   2      /* Status_Task */
#define TURNMENU_GRAPH    3      /* AssetGraph_Task */
#define TURNMENU_OPTIONS  4      /* TurnOptions_Task (options / save) */

/* one player's row on the status screen (StatusWork.rows) */
typedef struct StatusRow
{
    unsigned char shown;   /* 0x00 */
    unsigned char pri;       /* 0x01 ordering table slot: 7 = the selected player, else 8 */
    short x;   /* 0x02 */
    short y;   /* 0x04 */
} StatusRow;

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Board_MapViewTask */
typedef struct BoardMapViewWork
{
    signed char open;   /* 0x00 */
    unsigned char pad01[1];
    short slide;   /* 0x02 */
    unsigned char pad04[96];
} BoardMapViewWork;

/* work area of AssetGraph_Task */
typedef struct AssetGraphWork
{
    signed char shown;   /* 0x00 */
    signed char months;   /* 0x01 */
    signed char curPlayer;   /* 0x02 */
    unsigned char pad03[1];
    short x;   /* 0x04 */
    short y;   /* 0x06 */
    int panelWork;   /* 0x08 */
    int frame;   /* 0x0C */
    unsigned char pad10[84];
} AssetGraphWork;

/* work area of Status_Task */
typedef struct StatusWork
{
    int id;   /* 0x00 */
    unsigned char shown;   /* 0x04 */
    unsigned char listing;   /* 0x05 */
    unsigned char redraw;   /* 0x06 */
    signed char who;   /* 0x07 */
    signed char whoIndex;   /* 0x08 */
    signed char store;   /* 0x09 */
    signed char storeType;   /* 0x0A */
    unsigned char mode;   /* 0x0B */
    unsigned char arrived;   /* 0x0C */
    signed char listPos;   /* 0x0D */
    signed char x;   /* 0x0E */
    signed char floor;   /* 0x0F */
    signed char order[6];   /* 0x10 players in display order */
    unsigned short panelX;   /* 0x16 */
    short targetY;   /* 0x18 */
    unsigned char unused1A;   /* 0x1A */
    unsigned char unused1B;   /* 0x1B */
    int slideY;   /* 0x1C */
    int slideSpeed;   /* 0x20 */
    int row;   /* 0x24 */
    StatusRow rows[6];   /* 0x28 */
    unsigned char pad4C[24];
} StatusWork;

/* work area of TurnMenu_Task */
typedef struct TurnMenuWork
{
    unsigned char active;   /* 0x00 */
    unsigned char items;   /* 0x01 */
    unsigned char turnMenuChoice;   /* 0x02 */
    unsigned char pad03[1];
    int x;   /* 0x04 */
    int y;   /* 0x08 */
    int dy;   /* 0x0C */
    short radius;   /* 0x10 */
    short angle;   /* 0x12 */
    short spread;   /* 0x14 */
    short turned;   /* 0x16 */
    int delay;   /* 0x18 */
    short itemAngle[7];   /* 0x1C angle of each item on the ring (0x1000 = full turn) */
    unsigned char pad2A[58];
} TurnMenuWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char k_statusStoreListHeader[];  /* 0x8001500C */
extern u16 k_playerColors[6];           /* 0x800AD9BC */
extern char g_turnMenuChoice;           /* 0x800B270A TURNMENU_* */

/* ---- Functions ---- */

/* src/turnmenu.c */
u32 Board_MapViewTask(u16 *step, BoardMapViewWork *work); /* 0x8004B4C0 */
u32 AssetGraph_Task(u16 *step, AssetGraphWork *work);     /* 0x800655A0 */
void AssetGraph_Draw(u32 months, short x, u16 y, char current); /* 0x80065CE0 */
u32 Status_Task(short *step, StatusWork *work);           /* 0x80066A30 */
int TurnMenu_Task(short *step, TurnMenuWork *work);       /* 0x8006ACD4 */
void TurnMenu_Unused1(void);                              /* 0x8006BB24 */
void TurnMenu_Unused2(void);                              /* 0x8006BC34 */

#endif /* TURNMENU_H */
