/*
 * newgame.h - game mode 5: the new-game set-up
 *
 * Game mode 5: the new game set-up: the main menu, then the screens in turn; options; loading a
 * saved game.
 * The declarations of src/newgame.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/modes.md.
 */

#ifndef NEWGAME_H
#define NEWGAME_H

#include "common.h"
#include "boardmap.h"

/* g_mainMenuChoice: the main menu's choice (MainMenu_Task) */
#define MAINMENU_FREE_PLAY  0
#define MAINMENU_STORY      1
#define MAINMENU_LOAD       2
#define MAINMENU_OPTIONS    3

/* g_menuResult: how a set-up screen ended (NewGame_MenuTask goes on by it) */
#define MENU_RESULT_NEXT       0   /* on to the next screen */
#define MENU_RESULT_BACK       1   /* back to the one before */
#define MENU_RESULT_CHARACTER  2   /* PlayerMenu_Task: choose a player's character */

/* g_stageMoneyTable (0x800A8AE0): copied into GameConfig at new game / next stage. */
typedef struct StageMoney
{
    unsigned short startMoney;   /* 3000 (stage 0) .. 5000 (stage 9) */
    unsigned short goalMoney;    /* 10000 .. 30000 */
} StageMoney;

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of MainMenu_Task */
typedef struct MainMenuWork
{
    signed char shown;   /* 0x00 */
    signed char inStory;   /* 0x01 the story mode rows are open */
    signed char started;   /* 0x02 the running character is on */
    signed char mainMenuChoice;   /* 0x03 0 free play, 1 story mode, 2 load, 3 options */
    signed char storyRow;   /* 0x04 0 from the start, 1 from the stage chosen */
    signed char stage;   /* 0x05 */
    signed char progress;   /* 0x06 the last stage open (Progress.storyStage, at most 9) */
    signed char shownChoice;   /* 0x07 the choice the running character shows */
    short fade;   /* 0x08 the main rows' band */
    short storyFade;   /* 0x0A the story rows' band */
    short item;   /* 0x0C */
    short count;   /* 0x0E menu items */
    short x;   /* 0x10 the running character */
    short y;   /* 0x12 */
    int timer;   /* 0x14 frames before it comes back after a change */
    int delay;   /* 0x18 */
    unsigned char pad1C[72];
} MainMenuWork;

/* work area of Options_Task */
typedef struct OptionsWork
{
    signed char shown;   /* 0x00 */
    signed char moving;   /* 0x01 */
    signed char row;   /* 0x02 */
    signed char optEventScenes;   /* 0x03 */
    signed char optSkipCpuTalk;   /* 0x04 */
    signed char optCpuFast;   /* 0x05 */
    signed char optMono;   /* 0x06 */
    signed char song;   /* 0x07 */
    signed char seTest;   /* 0x08 */
    signed char track;   /* 0x09 */
    signed char slot;   /* 0x0A */
    signed char playing;   /* 0x0B */
    short fade;   /* 0x0C */
    short item;   /* 0x0E */
    short count;   /* 0x10 */
    unsigned char pad12[2];
    int delay;   /* 0x14 */
    unsigned char pad18[76];
} OptionsWork;

/* work area of LoadGame_Task */
typedef struct LoadGameWork
{
    signed char shown;   /* 0x00 the list is drawn */
    signed char slot;   /* 0x01 0-3 a save, 4 the system data only */
    signed char unused02;   /* 0x02 set to 0, not read */
    signed char unused03;   /* 0x03 */
    signed char unused04;   /* 0x04 */
    signed char saves;   /* 0x05 rows that can be chosen (the saves and the system data) */
    short fade;   /* 0x06 */
    short item;   /* 0x08 */
    short count;   /* 0x0A menu items */
    int delay;   /* 0x0C */
    unsigned char slotUsed[5];   /* 0x10 (Save_DescribeSlot); [4] the system data row, always 1 */
    signed char checkCard;   /* 0x15 the card is checked every frame (taken out: "Load failed.") */
    unsigned char pad16[78];
} LoadGameWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern StageMoney g_stageMoneyTable[GAME_NUM_STAGES];  /* 0x800A8AE0 */
extern u8 g_mainMenuChoice;                            /* 0x800B255C */
extern u8 g_menuResult;                                /* 0x800B255D */
extern u8 g_newGameStarted;                            /* 0x800B255F */
extern char g_setupSlot;                               /* 0x800B2560 */

/* ---- Functions ---- */

/* src/newgame.c */
int NewGame_Task(short *step, void *work);           /* 0x80036F1C */
void NewGame_LoadGraphics(void);                     /* 0x80037128 */
void NewGame_LoadCharGraphics(void);                 /* 0x80037370 */
u32 NewGame_MenuTask(u16 *step);                     /* 0x80037648 */
u32 MainMenu_Task(short *step, MainMenuWork *work);  /* 0x800385A8 */
u32 Options_Task(short *step, OptionsWork *work);    /* 0x8003F424 */
u32 LoadGame_Task(u16 *step, LoadGameWork *work);    /* 0x8004006C */

#endif /* NEWGAME_H */
