/*
 * game.h - the default settings and board rules, the start-up loading screen
 *
 * The declarations of src/game.c: its types and constants, its globals and its functions.
 * See docs/modes.md.
 */

#ifndef GAME_H
#define GAME_H

#include "common.h"

/* Rules for the current board (g_boardRules, 0x800FE068), filled from GameConfig by
 * BoardRules_Init when a game starts. enable[] indices, see docs/board.md. */
#define RULE_STORES        0    /* store squares active */
#define RULE_ELEVATOR      1    /* elevator squares active */
#define RULE_EVENTS        2    /* special dice result (8) triggers an event */
#define RULE_BANK          3    /* bank squares active */
#define RULE_FREE_BUYING   4    /* debug: lots and deliveries bought without the money check */
#define RULE_STORES_OPEN   5    /* debug: every store open from the start, shared out among the players */
#define RULE_POST_EVENT    6    /* event after every turn (step 9000) */
#define RULE_PICK_EVENT    7    /* choose the event (EventPick_Task) instead of a random one */
#define RULE_PICKED_EVENT  8    /* not a flag: the event EventPick_Task last chose (0-99) */
#define RULE_SLOW_DICE     9    /* the pencil (Dice_Task) rolls slowly from a fixed start, as DICE_SLOW */
#define RULE_RESET_DIR     10   /* clear Player.dirX / dirFloor before moving */
#define RULE_DEBUG_POINTS  11   /* debug: L1 / R1 on the player panels (PlayerPanel_Task) change the current player's points */
#define RULE_ALWAYS_TALK   12   /* character turn scripts for every player, not only CPU */
/* (13 is never read; BoardRules_Init leaves 8, 13 and 21 as they were) */
#define RULE_AUTO_VACANT   14   /* RULE_AUTO_*: force isCpu = 1 during that dialog */
#define RULE_AUTO_STORE    15
#define RULE_AUTO_MINUS    16
#define RULE_AUTO_OWN      17
#define RULE_AUTO_OTHERS   18
#define RULE_AUTO_EXCHANGE 19   /* the points exchange at the bank */
#define RULE_AUTO_EVENT    20   /* the event bought there */
#define RULE_AUTO_SCRIPT   21   /* the store dialogs an event script opens (opcodes 0x184, 0x185) */
#define RULE_AUTO_MOVE     22
#define RULE_AUTO_ELEVATOR 23

/* BoardRules.unused0E: six 3-byte records. */
typedef struct BoardRuleSlot
{
    unsigned char index;
    unsigned char flag;
    unsigned char pad;
} BoardRuleSlot;                  /* size 3 */

typedef struct BoardRules
{
    int startMoney;              /* 0x00 GameConfig.startMoney */
    int goalMoney;               /* 0x04 GameConfig.goalMoney */
    signed char bankruptLimit; /* 0x08 GameConfig.bankruptLimit */
    unsigned char startPoints;   /* 0x09 event points each player starts with; 0xFF = 99 and no exchange */
    unsigned char unused0A;      /* 0x0A set to 0, not read */
    unsigned char unused0B;      /* 0x0B set to 1, not read */
    signed char startGoods;    /* 0x0C GameConfig.startGoods */
    unsigned char unused0D;      /* 0x0D set to 2, not read */
    BoardRuleSlot unused0E[6];   /* 0x0E set to {i, 1} ({0, 0} for the first), not read */
    unsigned char enable[24];    /* 0x20 RULE_* flags; defaults: [0..3] = 1, rest 0 */
    unsigned char unused38;      /* 0x38 copied from g_optRule38, not read */
    unsigned char unused39[0x0F]; /* 0x39 cleared, not read */
} BoardRules;                    /* size 0x48; GameConfig follows at 0x800FE0B0 */

#define GAME_MAX_PLAYERS 6

/* One player slot in GameConfig (0x12 bytes). */
typedef struct PlayerSetup
{
    unsigned char charId;        /* 0x00 0xFF = empty slot; character index (CHR?P0.J / KAO?P.J) */
    unsigned char isCpu;         /* 0x01 a CPU player (1 for every player in the attract demo) */
    unsigned char pad;           /* 0x02 controller (Pad_Assign) */
    char name[13];               /* 0x03 strcpy'd from g_cpuNames in the demo */
    unsigned char wasOut;        /* 0x10 Player.out copied back at game end */
    unsigned char finalRank;     /* 0x11 Player.rank copied back at game end */
} PlayerSetup;

/* GameConfig.playMode */
#define PLAY_NORMAL  0           /* a normal game */
#define PLAY_STAGE   1           /* stage mode: 10 stages, saves */
#define PLAY_DEMO    2           /* the attract demo */

/* g_gameConfig (0x800FE0B0). Fields without names are written at setup but not traced yet. */
typedef struct GameConfig
{
    unsigned char playMode;      /* 0x00 PLAY_* */
    unsigned char unused01;      /* 0x01 10 by default; saved, not read */
    signed char numPlayers;    /* 0x02 1 by default, 4 in demo: numHumans + numCpus */
    unsigned char numHumans;     /* 0x03 */
    signed char numCpus;       /* 0x04 */
    unsigned char year;          /* 0x05 incremented every 12 months (Calendar_Advance) */
    unsigned char month;         /* 0x06 0..11, 0 = April; one board round = one month */
    unsigned char stage;         /* 0x07 0..9 (STG00..STG09); demo picks rand() % 8 */
    signed char bankruptLimit; /* 0x08 game ends when this many players are out (default 1) */
    signed char startGoods;    /* 0x09 goods slots of a newly opened store (3 by default) */
    signed char maxBuy;        /* 0x0A most goods bought at once (2 by default) */
    unsigned char unused0B;      /* 0x0B not used by the game code */
    int monthsElapsed;           /* 0x0C total months played (Calendar_Advance) */
    int startMoney;              /* 0x10 from g_stageMoneyTable */
    int goalMoney;               /* 0x14 from g_stageMoneyTable; win when Player.assets >= this */
    int demoTimer;               /* 0x18 decremented every Board_Task frame; demo ends when < 0 */
    unsigned char turnOrder[GAME_MAX_PLAYERS];  /* 0x1C demo: identity */
    PlayerSetup players[GAME_MAX_PLAYERS];      /* 0x22 */
} GameConfig;                    /* size 0x8E (+ padding) */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char g_optEventScenes;    /* 0x800B2534 */
extern char g_optSkipCpuTalk;    /* 0x800B2535 */
extern char g_optCpuFast;        /* 0x800B2536 */
extern char g_optMono;           /* 0x800B2537 */
extern u8 g_optUnused;           /* 0x800B2538 */
extern u8 g_optRule38;           /* 0x800B2539 */
extern BoardRules g_boardRules;  /* 0x800FE068 */
extern GameConfig g_gameConfig;  /* 0x800FE0B0 */
extern u32 g_loadScreenBuf;      /* 0x80150000 */

/* ---- Functions ---- */

/* src/game.c */
void Game_Unused1(void);             /* 0x80036848 */
u32 Game_LoadCommonTask(u16 *step);  /* 0x80036850 */
void Game_Unused2(void);             /* 0x80036AC4 */
void Game_InitDefaults(void);        /* 0x80036ACC */
void BoardRules_Init(void);          /* 0x80036DAC */

#endif /* GAME_H */
