/*
 * setup.h - the free-play and story set-up screens
 *
 * Player count, the players, characters, names, stage, money rules, turn order.
 * The declarations of src/setup.c: its types, the work areas of its tasks, its globals and its
 * functions.
 * See docs/modes.md.
 */

#ifndef SETUP_H
#define SETUP_H

#include "common.h"
#include "gfx.h"
#include "menu.h"

/* A character on the character select wheel (g_charSelectSlots, CharSelect_Task). */
typedef struct CharSlot
{
    signed char shown;           /* 0x00 */
    unsigned char pad01[2];
    signed char charId;        /* 0x03 */
    unsigned short anim;         /* 0x04 index into k_charAnims* */
    unsigned short lastAnim;     /* 0x06 anim the frame counter belongs to */
    unsigned short frame;        /* 0x08 CharAnimFrame index */
    unsigned short timer;        /* 0x0A frames the current picture has been shown */
    short unused0C;              /* 0x0C set to 0, not read */
    unsigned char pad0E[2];
    int radius;                  /* 0x10 distance from the center of the wheel */
    int angle;                   /* 0x14 place on the wheel, 0x1000 = a full turn */
    unsigned char pad18[2];
    short unused1A;              /* 0x1A set to 120, not read */
} CharSlot;                      /* size 0x1C */

/* A player's character in the turn order draw (g_turnOrderSlots, TurnOrder_Task): it
 * circles on the wheel, then walks to its place in the list. */
typedef struct TurnOrderSlot
{
    signed char shown;           /* 0x00 */
    signed char placed;          /* 0x01 drawn at x, y instead of on the wheel */
    signed char started;         /* 0x02 */
    unsigned char anim;          /* 0x03 index into k_charAnims* */
    unsigned char lastAnim;      /* 0x04 */
    unsigned char pad05;
    unsigned short frame;        /* 0x06 CharAnimFrame index */
    unsigned short timer;        /* 0x08 */
    short x, y;                  /* 0x0A */
    short angle;                 /* 0x0E place on the wheel */
    int radius;                  /* 0x10 20.12-ish distance from the center */
} TurnOrderSlot;                 /* size 0x14 */

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of PlayerCount_Task */
typedef struct PlayerCountWork
{
    signed char shown;   /* 0x00 */
    signed char moving;   /* 0x01 the CPU count is being chosen */
    signed char minCpus;   /* 0x02 */
    signed char maxCpus;   /* 0x03 */
    signed char running[2];   /* 0x04 the counter's character is on; [0] humans, [1] CPUs */
    signed char value[2];   /* 0x06 humans - 1, CPUs */
    signed char shownValue[2];   /* 0x08 the number the character holds up */
    short fade[2];   /* 0x0A the counters' bands */
    short item;   /* 0x0E */
    short count;   /* 0x10 menu items */
    ScreenPos pos[2];   /* 0x12 the characters */
    unsigned char pad1A[2];
    int timer[2];   /* 0x1C frames before the character comes back with the new number */
    int delay;   /* 0x24 */
    unsigned char pad28[60];
} PlayerCountWork;

/* work area of PlayerMenu_Task */
typedef struct PlayerMenuWork
{
    unsigned int cursor;   /* 0x00 the row: a player slot, or players for "OK" */
    unsigned char shown;   /* 0x04 */
    unsigned char pad05[7];
    short players;   /* 0x0C */
    short fade;   /* 0x0E */
    short item;   /* 0x10 */
    short items;   /* 0x12 */
    unsigned int delay;   /* 0x14 */
    unsigned char pad18[76];
} PlayerMenuWork;

/* work area of CharSelect_Task */
typedef struct CharSelectWork
{
    int slot;   /* 0x00 */
    unsigned char pad04[4];
    signed char pick;   /* 0x08 */
    signed char charId;   /* 0x09 */
    signed char chars;   /* 0x0A */
    signed char confirming;   /* 0x0B */
    unsigned char done;   /* 0x0C */
    unsigned char changed;   /* 0x0D */
    unsigned char moving;   /* 0x0E */
    signed char item;   /* 0x0F */
    unsigned char count;   /* 0x10 */
    unsigned char unused11;   /* 0x11 */
    unsigned short zoom;   /* 0x12 */
    unsigned short spinStep;   /* 0x14 */
    unsigned char pad16[2];
    int spin;   /* 0x18 */
    int target;   /* 0x1C */
    int base;   /* 0x20 */
    int delay;   /* 0x24 */
    unsigned char pad28[60];
} CharSelectWork;

/* work area of NameEntry_Task */
typedef struct NameEntryWork
{
    int slot;   /* 0x00 */
    unsigned char page;   /* 0x04 */
    signed char done;   /* 0x05 */
    signed char len;   /* 0x06 */
    signed char pos;   /* 0x07 */
    signed char col;   /* 0x08 */
    signed char row;   /* 0x09 */
    short fade;   /* 0x0A */
    short item;   /* 0x0C */
    short items;   /* 0x0E */
    unsigned short gridX;   /* 0x10 */
    short gridY;   /* 0x12 */
    unsigned char pad14[4];
    int delay;   /* 0x18 */
    unsigned char name[14];   /* 0x1C */
    short id;   /* 0x2A */
    unsigned char pad2C[56];
} NameEntryWork;

/* work area of StageSelect_Task */
typedef struct StageSelectWork
{
    int story;   /* 0x00 */
    unsigned char shown;   /* 0x04 */
    unsigned char picked;   /* 0x05 */
    signed char stage;   /* 0x06 */
    signed char progress;   /* 0x07 */
    short fade;   /* 0x08 */
    short item;   /* 0x0A */
    unsigned short count;   /* 0x0C */
    signed char moving;   /* 0x0E */
    unsigned char unused0F;   /* 0x0F */
    unsigned short scrollSpeed;   /* 0x10 */
    short scrollX;   /* 0x12 */
    unsigned short y;   /* 0x14 */
    short savedX;   /* 0x16 copy of scrollX, y (memcpy) */
    short savedY;   /* 0x18 */
    unsigned char pad1A[2];
    int scaleX;   /* 0x1C */
    int scaleY;   /* 0x20 */
    int angle;   /* 0x24 */
    int delay;   /* 0x28 */
    unsigned char pad2C[28];
    unsigned char src[16];   /* 0x48 */
    unsigned char dst[12];   /* 0x58 */
} StageSelectWork;

/* work area of MoneyRules_Task */
typedef struct MoneyRulesWork
{
    int story;   /* 0x00 */
    unsigned char shown;   /* 0x04 */
    signed char row;   /* 0x05 */
    unsigned char unused06;   /* 0x06 */
    unsigned char pad07[1];
    short fade;   /* 0x08 */
    short item;   /* 0x0A */
    unsigned short count;   /* 0x0C */
    unsigned char unused0E;   /* 0x0E */
    unsigned char unused0F;   /* 0x0F */
    int delay;   /* 0x10 */
    unsigned char pad14[80];
} MoneyRulesWork;

/* work area of TurnOrder_Task */
typedef struct TurnOrderWork
{
    signed char state;   /* 0x00 TORDER_STATE_*: what is drawn */
    signed char unused01;   /* 0x01 */
    signed char order[6];   /* 0x02 */
    signed char players;   /* 0x08 */
    signed char pick;   /* 0x09 */
    short rowH;   /* 0x0A */
    short spin;   /* 0x0C */
    short spinSpeed;   /* 0x0E */
    short spinStep;   /* 0x10 */
    unsigned char pad12[6];
    short fade;   /* 0x18 */
    short item;   /* 0x1A */
    short count;   /* 0x1C */
    unsigned char pad1E[2];
    int delay;   /* 0x20 */
    unsigned char pad24[64];
} TurnOrderWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char *g_cpuNames[14];                /* 0x800A8AA8 */
extern u8 k_nameEntryChars[3][6][14][2];    /* 0x800A8CB8 name entry grid: 3 pages of 6 x 14 Shift-JIS characters */
extern CharAnimFrame *k_charAnims[2];       /* 0x800A90C8 */
extern CharAnimFrame *k_charAnimsChar0[2];  /* 0x800A90D0 */
extern CharAnimFrame *k_charAnimsChar4[2];  /* 0x800A90D8 */
extern u8 k_storyOpponents[40];             /* 0x800A91F4 */
extern u16 k_charPalettes[16];              /* 0x800A921C */
extern short g_nameEntryBlink;              /* 0x800B2558 */
extern char g_setupLastSlot;                /* 0x800B2561 */
extern u8 g_slotFilled[6];                  /* 0x800B2568 */
extern MenuChar g_playerMenuChars[6];       /* 0x800FEB20 */
extern CharSlot g_charSelectSlots[14];      /* 0x800FEB80 */
extern MenuChar g_nameEntryChar[1];         /* 0x800FED10 */
extern TurnOrderSlot g_turnOrderSlots[6];   /* 0x800FED20 */
extern u8 g_charTaken[14];                  /* 0x800FEDA0 */

/* ---- Functions ---- */

/* src/setup.c */
u32 PlayerCount_Task(short *step, PlayerCountWork *work); /* 0x800392CC */
u32 PlayerMenu_Task(short *step, PlayerMenuWork *work);   /* 0x80039CA4 */
u32 CharSelect_Task(short *step, CharSelectWork *work);   /* 0x8003AC38 */
u32 NameEntry_Task(short *step, NameEntryWork *work);     /* 0x8003BFC8 */
u32 StageSelect_Task(short *step, StageSelectWork *work); /* 0x8003D3AC */
u32 MoneyRules_Task(short *step, MoneyRulesWork *work);   /* 0x8003DC78 */
u32 TurnOrder_Task(short *step, TurnOrderWork *work);     /* 0x8003E51C */
void Story_SetupPlayers(void);                            /* 0x80042900 */

#endif /* SETUP_H */
