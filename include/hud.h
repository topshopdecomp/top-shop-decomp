/*
 * hud.h - the board's display tasks
 *
 * The square name, steps left, calendar and goal, the players' panels, the minimap, the points
 * window.
 * The declarations of src/hud.c: its types and constants, the work areas of its tasks, its globals
 * and its functions.
 * See docs/board.md.
 */

#ifndef HUD_H
#define HUD_H

#include "common.h"
#include "gfx.h"

/* Where PlayerPanel_Task puts the player panels for a number of players (k_playerPanelLayout). */
typedef struct PanelLayout
{
    short width;                 /* 0x00 panel width */
    short pos[6][2];             /* 0x02 each panel's x, y (y from the top of the panel area, 0xA8) */
} PanelLayout;                   /* size 0x1A */

/* PlayerPanel_Task's steps (Move_Task waits for PANEL_HIDDEN) */
enum PlayerPanelStep
{
    PANEL_INIT             =   0,
    PANEL_HIDDEN           =  10,  /* the shown values follow the real ones */
    PANEL_SLIDE_IN         =  20,  /* DISP_SHOW: from the right, all to the left edge */
    PANEL_SLIDE_TO_EDGE    =  21,
    PANEL_SLIDE_TOP_ROW    =  22,  /* then the top row to its places */
    PANEL_SLIDE_BOTTOM_ROW =  23,  /* then the bottom row */
    PANEL_SLIDE_OUT_BOTTOM =  30,  /* DISP_HIDE: the bottom row to the left edge */
    PANEL_SLIDE_OUT_TOP    =  31,  /* then the top row */
    PANEL_SLIDE_OUT_RIGHT  =  32,  /* then all to the right */
    PANEL_RISE             =  40,  /* DISP_SHOW_LOW: rise from below */
    PANEL_COUNT_DROP       =  50,  /* DISP_COUNT_DROP: when the counting is done, drop below the screen */
    PANEL_DROP             =  51,
    PANEL_SHOW_NOW         =  60,  /* DISP_SHOW_NOW */
    PANEL_SHOWN            =  90,  /* in place */
    PANEL_END              = 100   /* DISP_END */
};

/* Messages to the board's display tasks (SquareInfo_Task, Hud_CalendarTask, Minimap_Task,
 * PlayerPanel_Task) */
#define DISP_END         0x00   /* slide out and end */
#define DISP_SHOW        0x01   /* slide in */
#define DISP_HIDE        0x02   /* slide out */
#define DISP_REFRESH     0x03   /* SquareInfo_Task, Hud_CalendarTask: out, and in again with the new value */
#define DISP_SHOW_LOW    0x06   /* Minimap_Task: slide in lower down; PlayerPanel_Task: rise from the bottom */
#define DISP_COUNT_DROP  0x07   /* PlayerPanel_Task: count the money, then drop below the screen */
#define DISP_MOVE_DOWN   0x08   /* Minimap_Task: to the lower place */
#define DISP_MOVE_UP     0x09   /* Minimap_Task: back up */
#define DISP_ROUTES_ON   0x0c   /* Minimap_Task: the ends of the current player's routes shown */
#define DISP_ROUTES_OFF  0x0d
#define DISP_SHOW_NOW    0x10   /* in place at once */

/* Minimap_Draw flags (bits 0-3: how the stores show) */
#define MINIMAP_OWNERS        0x00  /* in their owner's color */
#define MINIMAP_BLINK_STORE   0x01  /* vacant, but store `highlight` blinking in its owner's color */
#define MINIMAP_BLINK_PLAYER  0x02  /* vacant, but the stores of player `highlight` blinking */
#define MINIMAP_PLAYERS     0x10
#define MINIMAP_CAMERA      0x20
#define MINIMAP_ROUTE_ENDS  0x40

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of SquareInfo_Task */
typedef struct SquareInfoWork
{
    signed char shown;   /* 0x00 */
    signed char x;   /* 0x01 */
    signed char floor;   /* 0x02 */
    signed char palette;   /* 0x03 of the panel: the store owner's, 6 = neutral */
    signed char width;   /* 0x04 of the label, in characters */
    unsigned char pad05[1];
    short cell;   /* 0x06 Board_GetCell of the square */
    unsigned short panelX;   /* 0x08 */
    short panelY;   /* 0x0A slides between -0x18 (hidden) and 0x10 */
    unsigned char pad0C[4];
    short text;   /* 0x10 the label, -1 = none */
    unsigned char pad12[82];
} SquareInfoWork;

/* work area of StepsLeft_Task */
typedef struct StepsLeftWork
{
    int win;   /* 0x00 */
    short stepsLeft;   /* 0x04 the number shown */
    unsigned char pad06[94];
} StepsLeftWork;

/* work area of Minimap_Task */
typedef struct MinimapWork
{
    signed char shown;   /* 0x00 */
    signed char showRoutes;   /* 0x01 the ends of the current player's routes drawn (DISP_ROUTES_ON) */
    short x;   /* 0x02 */
    short y;   /* 0x04 the bottom */
    short width;   /* 0x06 */
    short height;   /* 0x08 */
    unsigned char pad0A[90];
} MinimapWork;

/* work area of PlayerPanel_Task */
typedef struct PlayerPanelWork
{
    int highlightCur;   /* 0x00 */
    int curPlayer;   /* 0x04 */
    unsigned char visible;   /* 0x08 */
    unsigned char counting;   /* 0x09 */
    unsigned char pulseDown;   /* 0x0A */
    unsigned char frame;   /* 0x0B */
    short bobAngle;   /* 0x0C */
    short bobX;   /* 0x0E */
    short bobY;   /* 0x10 */
    unsigned char unused12;   /* 0x12 */
    unsigned char unused13;   /* 0x13 */
    short pulseSpeed;   /* 0x14 */
    short unused16;   /* 0x16 not used */
    unsigned short pulse;   /* 0x18 */
    ScreenPos panels[6];   /* 0x1A each player's panel */
    unsigned char pad32[50];
} PlayerPanelWork;

/* work area of Hud_CalendarTask */
typedef struct HudCalendarWork
{
    signed char shown;   /* 0x00 */
    signed char month;   /* 0x01 */
    short x;   /* 0x02 */
    short y;   /* 0x04 */
    signed char unused06;   /* 0x06 */
    signed char unused07;   /* 0x07 */
    short height;   /* 0x08 */
    unsigned char pad0A[90];
} HudCalendarWork;

/* work area of Points_DisplayTask */
typedef struct PointsDisplayWork
{
    int win;   /* 0x00 */
    int x;   /* 0x04 */
    int y;   /* 0x08 */
    signed char points;   /* 0x0C */
    signed char shown;   /* 0x0D */
    signed char closing;   /* 0x0E */
    unsigned char unused0F;   /* 0x0F */
    int delay;   /* 0x10 */
    unsigned char pad14[80];
} PointsDisplayWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern PanelLayout k_playerPanelLayout[6];  /* 0x80015038 the player panels for 1-6 players */
extern u8 g_playerNameState[16];            /* 0x800B2628 */
extern short g_playerNameTexts[6];          /* 0x80111F60 */

/* ---- Functions ---- */

/* src/hud.c */
u32 SquareInfo_Task(u16 *step, SquareInfoWork *work);     /* 0x80056B0C */
u32 StepsLeft_Task(u16 *step, StepsLeftWork *work);       /* 0x80057380 */
u32 Minimap_Task(short *step, MinimapWork *work);         /* 0x800686CC */
void Minimap_Draw(short x, short y, short w, short h, u16 flags, u16 highlight); /* 0x800689EC */
u32 PlayerPanel_Task(short *step, PlayerPanelWork *work); /* 0x800697A8 */
u32 Hud_CalendarTask(short *step, HudCalendarWork *work); /* 0x8006B35C */
void Hud_CreateNameTexts(void);                           /* 0x8006BB2C */
void Hud_FreeNameTexts(void);                             /* 0x8006BBE8 */
u32 Points_DisplayTask(u16 *step, PointsDisplayWork *work); /* 0x8006F60C */

#endif /* HUD_H */
