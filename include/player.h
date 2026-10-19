/*
 * player.h - the players
 *
 * Money, assets, ranks, the asset history, backups of a move, player selectors.
 * The declarations of src/player.c: its types and constants, its globals and its functions.
 * See docs/board.md.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"

#define BOARD_MAX_PLAYERS 6

/* Player.dirX: the direction a piece moves along a floor */
#define DIR_LEFT   (-1)
#define DIR_NONE   0             /* none chosen yet (after the bank, or RULE_RESET_DIR) */
#define DIR_RIGHT  1

/* One player during a board game (g_players, 0x80113CE8), indexed by g_curPlayer. */
typedef struct Player
{
    unsigned char index;         /* 0x00 position in the turn order */
    unsigned char isCpu;         /* 0x01 a CPU player: the game answers its dialogs. RULE_AUTO_* set it for a while */
    unsigned char pad;           /* 0x02 controller (PlayerSetup.pad) */
    unsigned char charId;        /* 0x03 character index (g_cpuNames order) */
    unsigned char out;           /* 0x04 1 = out of the game (went bankrupt) */
    char name[13];               /* 0x05 */
    char title[13];              /* 0x12 a second copy of the name; the save file's title */
    unsigned char pieceShown;    /* 0x1F the piece is drawn on the board (0 while riding an elevator) */
    unsigned char choosing;      /* 0x20 1 while a human player picks a direction: the arrows of the ways are drawn */
    unsigned char color;         /* 0x21 index into k_playerColors (the turn index) */
    signed char x;               /* 0x22 square on the floor */
    signed char floor;           /* 0x23 */
    unsigned char dirX;          /* 0x24 direction of movement: DIR_* (0xFF = -1) */
    unsigned char dirFloor;      /* 0x25 -1, 0, 1 (up / down in an elevator?) */
    unsigned char stepsLeft;     /* 0x26 set to dice + 1 */
    unsigned char steps;         /* 0x27 set to dice + 1 */
    unsigned char panel;         /* 0x28 panel palette slot (the turn index) */
    unsigned char exits;         /* 0x29 Board_GetExits of the square the move starts from */
    unsigned char rankOrder;     /* 0x2A position in the order Players_Rank sorts by assets */
    signed char rank;          /* 0x2B rank by assets, ties share a rank (PlayerSetup.finalRank) */
    unsigned char valueRank;     /* 0x2C rank by Players_Rank's second value */
    unsigned char prevRankOrder; /* 0x2D rankOrder at the start of the round */
    unsigned char prevRank;      /* 0x2E rank at the start of the round */
    unsigned char unused2F[3];   /* 0x2F */
    unsigned char noTargeting;   /* 0x32 CpuTarget_HumanUnused picks nobody while a player has it; nothing sets it */
    unsigned char paydayEffect;  /* 0x33 next payday: 1 = nothing, 2 = doubled (event cards); reset at payday */
    unsigned char diceEffect;    /* 0x34 DICE_*: an event card changes the dice until diceEffectUntil */
    unsigned char noElevator;    /* 0x35 "The elevator is out of order." until noElevatorUntil */
    unsigned char noDelivery;    /* 0x36 "Delivery is prohibited by an unseen force." */
    unsigned char vandalized;    /* 0x37 "This store can't be opened due to vandalism." */
    unsigned char exempt;        /* 0x38 when set, Board_PhaseSquare skips the vacant store steps and the minus balance check; never set */
    unsigned char historyLen;    /* 0x39 entries used in assetHistory (13 at most) */
    unsigned char points;        /* 0x3A event points, spent in the points exchange */
    unsigned char shoes;         /* 0x3B wears the "Chrip-Chrip" shoes (EV_SHOES, script opcode 0x193): squeaky steps */
    unsigned char shopHistory;   /* 0x3C bit 0: shopped without cash back this turn (Shopping_Task); shifted left
                                    every turn, so bit 1 is the last turn (TURN_SHOPPED_AGAIN) */
    unsigned char passedHalfGoal;/* 0x3D the assets have passed half the goal (commented on once) */
    unsigned char inElevator;    /* 0x3E riding the elevator (Elevator_Task): the piece is drawn smaller the
                                    higher it is above its floor (Board_PieceZoom) */
    unsigned char bankDistance;  /* 0x3F squares to the nearest bank, every turn */
    unsigned char routeEnds[10]; /* 0x40 x, floor where each of the 4 routes ends (0xFF = none), then the chosen one */
    short screenX;               /* 0x4A camera focus */
    short screenY;               /* 0x4C */
    short rankKey;               /* 0x4E set when the player goes out */
    unsigned short customers;    /* 0x50 goods other players bought in this player's stores (Shopping_Task) */
    unsigned short purchases;    /* 0x52 goods this player bought in other players' stores (Shopping_Task) */
    short paydays;               /* 0x54 paydays so far (bank visits) */
    short stepsSincePayday;      /* 0x56 squares walked since the last payday */
    unsigned short diceEffectUntil; /* 0x58 turn numbers the timed effects end */
    unsigned short noElevatorUntil; /* 0x5A */
    unsigned short noDeliveryUntil; /* 0x5C */
    unsigned short vandalizedUntil; /* 0x5E */
    short cpuPlan;               /* 0x60 CPU: a decision made in Shopping_Task for the next dialog: 1 take the
                                    store over (VacantLot_Task's buy-out list), 2 accept the store offer */
    short cpuPlanArg;            /* 0x62 */
    int totalSteps;              /* 0x64 steps taken in the whole game */
    int money;                   /* 0x68 */
    int moneyShown;              /* 0x6C animated display value */
    int assets;                  /* 0x70 compared with GameConfig.goalMoney */
    int assetsShown;             /* 0x74 */
    int unused78;                /* 0x78 not used by the game code */
    int assetHistory[13];        /* 0x7C assets at the end of each month (Player_RecordAssets) */
    int pieceState;              /* 0xB0 0xD2, 0xBE or 0xDE (how the piece is drawn) */
    unsigned short pieceSprite;  /* 0xB4 current sprite of the piece animation */
    short pieceFlip;             /* 0xB6 0x10 = facing the other way (xor'ed into the sprite attributes) */
    short pieceAnim;             /* 0xB8 animation to play: k_pieceAnims[charId * 7 + pieceAnim] */
    short animShown;             /* 0xBA animation the frame counter belongs to */
    short animFrame;             /* 0xBC */
    short animTimer;             /* 0xBE */
} Player;                        /* size 0xC0 */

/* Player_Backup / Player_Restore slots (g_playerBackups, 0x80114980). */
typedef struct PlayerBackup
{
    signed char x, floor;        /* 0x00 */
    unsigned char dirX, dirFloor;/* 0x02 */
    signed char stepsLeft;     /* 0x04 */
    unsigned char pad05[3];
    unsigned int stepsSincePayday;      /* 0x08 Player.stepsSincePayday */
    unsigned int totalSteps;     /* 0x0C Player.totalSteps */
    unsigned int paydays;        /* 0x10 Player.paydays */
    int money;                   /* 0x14 */
} PlayerBackup;                  /* size 0x18 */

/* Screen position of a piece standing on square (x, floor) (Player.screenX, screenY) */
#define PIECE_SCREEN_X(x)      ((g_stageDefs[g_boardStage].originX + (x) * 5) * 16 + 0x28)
#define PIECE_SCREEN_Y(floor)  ((g_stageDefs[g_boardStage].originY + (floor) * 5) * 16 + 0x3a)

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern unsigned char g_numActivePlayers;     /* 0x800B2658 */
extern unsigned char g_numPlayers;           /* 0x800B2718 */
extern unsigned char g_curPlayer;            /* 0x800B271C */
extern Player g_players[BOARD_MAX_PLAYERS];  /* 0x80113CE8 */
extern PlayerBackup g_playerBackups[8];      /* 0x80114980 */

/* ---- Functions ---- */

/* src/player.c */
int Player_CalcAssets(short player);                  /* 0x80076490 */
u32 Players_CheckGameEnd(void);                       /* 0x80076B38 */
void Players_Rank(void);                              /* 0x80076E7C */
void Player_RecordAssets(short player);               /* 0x80077690 */
void Player_Backup(short player, short slot);         /* 0x80077B10 */
void Player_Restore(short player, short slot);        /* 0x80077BB8 */
u32 Player_Resolve(short who);                        /* 0x8007AF1C */
int Players_GetMaxAssetsEver(void);                   /* 0x8007B440 */
void Player_SetMoney(short player, int money);        /* 0x8007B4D0 */
void Player_Pay(short player, int amount);            /* 0x8007B518 */
void Player_Earn(short player, int amount);           /* 0x8007B56C */
void Player_PayTo(short from, short to, int amount);  /* 0x8007B5C0 */

#endif /* PLAYER_H */
