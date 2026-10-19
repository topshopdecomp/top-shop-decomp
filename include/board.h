/*
 * board.h - game mode 6, the board game: Board_Task and its turn phases
 *
 * Game mode 6: Board_Task, the turn in five phases.
 * The declarations of src/board.c: its constants, the work areas of its tasks, its globals and its
 * functions.
 * See docs/board.md.
 */

#ifndef BOARD_H
#define BOARD_H

#include "common.h"

/* Board_Task's steps, one phase function each (see docs/board.md) */
enum BoardStep
{
    /* Board_PhaseSetup: load the board, the stage intro */
    STEP_SETUP                   =     0,  /* create the board's tasks, load the common data */
    STEP_SETUP_WAIT_COMMON       =     1,
    STEP_SETUP_NEXT              =     2,  /* (goes on) */
    STEP_LOAD_BOARD              =     3,  /* Board_InitGame, Board_LoadData */
    STEP_LOAD_SEASON             =     4,  /* the season's graphics */
    STEP_START_DISPLAY           =   100,  /* the display tasks, Board_FrameEnd */
    STEP_INTRO                   =   200,  /* stage mode, new game: the stage intro; else the music */
    STEP_INTRO_LOAD              =   201,  /* background, CNT<stage>.J */
    STEP_INTRO_WAIT_LOAD         =   210,
    STEP_INTRO_SCRIPT            =   211,  /* the intro script (START skips) */
    STEP_INTRO_FADE_OUT          =   212,
    STEP_INTRO_WAIT_FADE         =   213,
    STEP_INTRO_END               =   220,  /* the season's graphics, the display tasks back */
    STEP_BOARD_MUSIC             =   300,

    /* Board_PhaseTurn: a round, a turn, the command menu, the dice, events */
    STEP_TURN                    =  2000,  /* a turn; player 0 not resumed: a new round (month) */
    STEP_CALENDAR                =  2100,  /* the month's calendar scene */
    STEP_CALENDAR_GRAPHICS       =  2101,
    STEP_CALENDAR_LOAD           =  2105,  /* CAL_<month>.J */
    STEP_CALENDAR_WAIT_LOAD      =  2110,
    STEP_CALENDAR_SCRIPT         =  2111,
    STEP_CALENDAR_END            =  2120,  /* the season's graphics, the display tasks back */
    STEP_RECORD_ASSETS           =  2200,  /* every player's assets into its history */
    STEP_TURN_START              =  2300,  /* skip a player who is out; clear g_turnFlags */
    STEP_TURN_WIPE               =  2400,  /* the HUD, the opening wipe */
    STEP_TURN_WAIT_WIPE          =  2401,
    STEP_SAVE_PROMPT             =  2500,  /* stage mode, January: SavePrompt_Task */
    STEP_WAIT_SAVE_PROMPT        =  2501,
    STEP_TURN_BEGIN              =  3000,
    STEP_ANNOUNCE                =  3100,  /* "<name>'s turn." */
    STEP_WAIT_ANNOUNCE           =  3101,
    STEP_BANK_DISTANCE           =  3110,  /* "There are N steps to the bank." (nothing sets this step) */
    STEP_WAIT_BANK_DISTANCE      =  3111,
    STEP_START_TALK              =  3200,  /* the character talk at the start of the turn? */
    STEP_START_TALK_SCRIPT       =  3201,
    STEP_START_TALK_WAIT         =  3202,
    STEP_START_TALK_WAIT_XA      =  3203,
    STEP_COMMANDS                =  3300,  /* a fast CPU rolls straight away */
    STEP_COMMAND_MENU            =  3310,  /* TurnMenu_Task */
    STEP_WAIT_COMMAND_MENU       =  3311,
    STEP_COMMAND_CHOSEN          =  3320,  /* on g_turnMenuChoice (TURNMENU_*) */
    STEP_CLOSED_TODAY            =  3350,  /* "You are closed today." (nothing sets this step) */
    STEP_WAIT_CLOSED_TODAY       =  3351,
    STEP_MAP_VIEW                =  3400,  /* Board_MapViewTask */
    STEP_WAIT_MAP_VIEW           =  3401,
    STEP_STATUS                  =  3500,  /* Status_Task */
    STEP_WAIT_STATUS             =  3501,
    STEP_ASSET_GRAPH             =  3600,  /* AssetGraph_Task */
    STEP_WAIT_ASSET_GRAPH        =  3601,
    STEP_OPTIONS                 =  3700,  /* TurnOptions_Task */
    STEP_WAIT_OPTIONS            =  3701,
    STEP_ROLL                    =  3800,  /* Dice_Task */
    STEP_WAIT_ROLL               =  3801,  /* canceled, 8: an event, else the steps to move */
    STEP_EVENT                   =  4000,  /* an event (RULE_EVENTS) */
    STEP_EVENT_PICK              =  4010,  /* drawn at random, or EventPick_Task */
    STEP_EVENT_WAIT_PICK         =  4011,
    STEP_EVENT_FLAGS             =  4020,  /* TURN_EVENT / TURN_QUIET_EVENT by the event */
    STEP_EVENT_CARD              =  4040,  /* EventCard_Task */
    STEP_EVENT_WAIT_CARD         =  4041,
    STEP_EVENT_SCRIPT            =  4050,  /* EVE_<id>.J */
    STEP_EVENT_WAIT_SCRIPT       =  4051,  /* then on its EVENT_RESULT_* */

    /* Board_PhaseMove */
    STEP_MOVE                    =  6000,  /* Move_Task */
    STEP_WAIT_MOVE               =  6001,

    /* Board_PhaseSquare: the square the move ended on */
    STEP_SQUARE                  =  8000,
    STEP_SQUARE_DISPATCH         =  8200,  /* on the square's type */
    STEP_ELEVATOR                =  8300,  /* "Use the elevator?" */
    STEP_ELEVATOR_ANSWER         =  8301,
    STEP_ELEVATOR_RIDE           =  8310,  /* Elevator_Task */
    STEP_ELEVATOR_WAIT_RIDE      =  8311,
    STEP_ELEVATOR_FLOOR          =  8312,  /* "<n>F." */
    STEP_ELEVATOR_WAIT_FLOOR     =  8313,
    STEP_ELEVATOR_BROKEN         =  8350,  /* "The elevator is out of order." (noElevator) */
    STEP_ELEVATOR_WAIT_BROKEN    =  8351,
    STEP_BANK                    =  8400,  /* the move ended on the bank */
    STEP_SQUARE_UNUSED           =  8500,  /* (nothing sets this step) */
    STEP_VACANT                  =  8600,  /* a vacant lot */
    STEP_VACANT_LOT              =  8610,  /* VacantLot_Task: none of the player's stores on the square */
    STEP_VACANT_WAIT_LOT         =  8611,
    STEP_VACANT_OFFER            =  8620,  /* StoreOffer_Task: one of its stores is on the square */
    STEP_VACANT_WAIT_OFFER       =  8621,
    STEP_VACANT_SHOP_OPEN        =  8650,  /* ShopOpen_Task */
    STEP_VACANT_WAIT_SHOP_OPEN   =  8651,
    STEP_VACANT_VANDALIZED       =  8660,  /* "This store can't be opened due to vandalism." */
    STEP_VACANT_WAIT_VANDALIZED  =  8661,
    STEP_RIVAL_STORE             =  8700,  /* someone else's store */
    STEP_RIVAL_SHOP              =  8710,  /* Shopping_Task */
    STEP_RIVAL_WAIT_SHOP         =  8711,  /* sold out: it can be bought out */
    STEP_RIVAL_BUYOUT            =  8720,  /* a store without goods: buy it out */
    STEP_RIVAL_BUYOUT_LOT        =  8730,  /* VacantLot_Task (LOT_BUYOUT) */
    STEP_RIVAL_WAIT_BUYOUT_LOT   =  8731,
    STEP_RIVAL_BUYOUT_OFFER      =  8740,  /* StoreOffer_Task (buy) */
    STEP_RIVAL_WAIT_BUYOUT_OFFER =  8741,
    STEP_RIVAL_SHOP_OPEN         =  8750,  /* ShopOpen_Task */
    STEP_RIVAL_WAIT_SHOP_OPEN    =  8751,
    STEP_RIVAL_STEAL             =  8760,  /* StealItem_Task (Player.exempt, never set) */
    STEP_RIVAL_WAIT_STEAL        =  8761,
    STEP_RIVAL_CLOSED            =  8770,  /* "Closed" */
    STEP_RIVAL_WAIT_CLOSED       =  8771,
    STEP_RIVAL_VANDALIZED        =  8775,  /* "This store can't be opened due to vandalism." */
    STEP_RIVAL_WAIT_VANDALIZED   =  8776,
    STEP_RIVAL_TEMP_CLOSED       =  8780,  /* "Temporary Closed." */
    STEP_RIVAL_WAIT_TEMP_CLOSED  =  8781,
    STEP_RIVAL_DONE              =  8790,
    STEP_OWN_STORE               =  8800,  /* its own store: DeliveryStore_Task */
    STEP_OWN_WAIT_STORE          =  8801,
    STEP_OWN_DELIVERY            =  8810,  /* Delivery_Task */
    STEP_OWN_WAIT_DELIVERY       =  8811,
    STEP_OWN_DONE                =  8820,
    STEP_OWN_NO_DELIVERY         =  8850,  /* "Delivery is prohibited by an unseen force." */
    STEP_OWN_WAIT_NO_DELIVERY    =  8851,
    STEP_SETTLE                  =  8900,  /* wait for the money counters */
    STEP_SETTLE_CHECK            =  8901,  /* money below 0? */
    STEP_MINUS                   =  8910,  /* "Money went to minus." */
    STEP_WAIT_MINUS              =  8911,
    STEP_MINUS_SELL              =  8920,  /* SellStore_Task, forced */
    STEP_WAIT_MINUS_SELL         =  8921,
    STEP_SETTLE_UNUSED           =  8930,  /* (nothing sets this step) */
    STEP_WAIT_SETTLE_UNUSED      =  8931,

    /* Board_PhaseEndTurn: the end of a turn, the win check, the next player, the end */
    STEP_END_TURN                =  9000,  /* an event after the turn (RULE_POST_EVENT) */
    STEP_POST_EVENT_WAIT_PICK    =  9001,
    STEP_POST_EVENT_LOAD         =  9010,
    STEP_POST_EVENT_WAIT_LOAD    =  9011,
    STEP_POST_EVENT_SCRIPT       =  9020,
    STEP_POST_EVENT_WAIT_SCRIPT  =  9021,  /* then on its EVENT_RESULT_* */
    STEP_END_TALK                =  9100,  /* the character talk at the end of the turn? */
    STEP_END_TALK_SCRIPT         =  9200,
    STEP_END_TALK_WAIT           =  9201,
    STEP_END_TALK_WAIT_XA        =  9202,
    STEP_ASSETS                  = 10000,  /* the assets; money below 0: bankrupt */
    STEP_BANKRUPT                = 10100,  /* (nothing starts a fade here) */
    STEP_BANKRUPT_WAIT_FADE      = 10101,
    STEP_BANKRUPT_SKIP1          = 10102,  /* (goes on) */
    STEP_BANKRUPT_SKIP2          = 10103,  /* (goes on) */
    STEP_BANKRUPT_LOAD           = 10104,  /* EVE_000.J */
    STEP_BANKRUPT_WAIT_LOAD      = 10105,
    STEP_BANKRUPT_SCRIPT         = 10106,
    STEP_BANKRUPT_WAIT_SCRIPT    = 10107,
    STEP_BANKRUPT_OUT            = 10108,  /* the player is out */
    STEP_WIN_CHECK               = 10200,
    STEP_TURN_END_WIPE           = 10300,
    STEP_TURN_END_WAIT_WIPE      = 10301,
    STEP_NEXT_PLAYER             = 10400,  /* (after the last: the next month) */
    STEP_GAME_OVER               = 11000,  /* the results into GameConfig.players, fade out */
    STEP_GAME_OVER_WAIT_FADE     = 11001,
    STEP_GAME_OVER_EXIT          = 11002,  /* reset the CD and the heap, the next game mode */
    STEP_STAGE_LOST              = 12000,  /* stage mode, lost: EVE_001.J */
    STEP_STAGE_LOST_WAIT_FADE    = 12001,
    STEP_STAGE_LOST_SUSPEND      = 12002,
    STEP_STAGE_LOST_BLACK        = 12003,
    STEP_STAGE_LOST_LOAD         = 12004,
    STEP_STAGE_LOST_WAIT_LOAD    = 12005,
    STEP_STAGE_LOST_SCRIPT       = 12006,
    STEP_STAGE_LOST_WAIT_SCRIPT  = 12007,
    STEP_STAGE_WON               = 12100,  /* stage mode, won: CNT<10 + stage>.J */
    STEP_STAGE_WON_WAIT_FADE     = 12101,
    STEP_STAGE_WON_SUSPEND       = 12102,
    STEP_STAGE_WON_LOAD          = 12103,
    STEP_STAGE_WON_SCRIPT        = 12104,
    STEP_STAGE_WON_WAIT_SCRIPT   = 12105
};

#define SEASON(month)     ((((signed char)(month)) + 1) / 3 % 4)

/* What happened during the current player's turn: g_turnFlags[TURN_*] = 1. At its end
 * TurnEnd_PickScript picks the character's comment from them (the talk script entry is the
 * flag's index; the wording is in the talk script, TALK.J). Each name says where the flag is set. */
enum
{
    TURN_QUIET_EVENT     = 0x00,  /* an event card outside the commented groups: no comment */
    TURN_CHAIN_4         = 0x01,  /* a store opened or bought makes a chain of 4 */
    TURN_CHAIN_2_3       = 0x02,  /* ... a chain of 2 or 3 */
    TURN_STORE_CLOSED    = 0x03,  /* landed on someone's store that is closed for now */
    TURN_STORE_SALE      = 0x04,  /* ... on someone's store with lowered prices */
    TURN_OWN_STORE       = 0x05,  /* landed on its own store, or opened a single store */
    TURN_NO_ELEVATOR     = 0x06,  /* did not take the elevator */
    TURN_BANK            = 0x07,  /* landed on the bank (CELL_BANK) */
    TURN_ELEVATOR        = 0x08,  /* took the elevator */
    TURN_BIG_PURCHASE    = 0x09,  /* shopping cost a fifth of its money or more (no cash back) */
    TURN_CASH_BACK       = 0x0A,  /* shopping: got the money back */
    TURN_SHOPPED         = 0x0B,  /* shopping: no cash back */
    TURN_BOUGHT_STORE    = 0x0C,  /* bought someone else's store */
    TURN_SHOPPED_AGAIN   = 0x0D,  /* shopped without cash back this turn and the last */
    TURN_HALF_GOAL       = 0x0E,  /* assets passed half the goal */
    TURN_SOLD_STORE      = 0x0F,
    TURN_LOT_UNAFFORDABLE = 0x10, /* declined a vacant lot it could not afford */
    TURN_LOT_DECLINED    = 0x11,  /* declined a vacant lot it could afford */
    TURN_EVENT           = 0x12   /* an event card of the commented groups; also the default */
};

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* Mode task work area while Board_Task runs (Task.work, 100 bytes) */
typedef struct BoardWork
{
    unsigned char unused00[2];   /* 0x00 not used */
    unsigned char rollCancelable; /* 0x02 Dice_Task's cancelable: 1 a roll from the command menu, 0 one an event asks for */
    signed char eventId;         /* 0x03 event chosen in steps 4010-4020 */
    unsigned char savedIsCpu;    /* 0x04 Player.isCpu saved while a RULE_AUTO_* flag forces it */
    unsigned char unused05[7];   /* 0x05 not used */
    unsigned char savePromptArmed; /* 0x0C 0 on the first round, then 1 */
} BoardWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u32 g_debugComUseA;                    /* 0x800B23B0 */
extern u32 g_debugComUseB;                    /* 0x800B23B4 */
extern char g_resumedGame;                    /* 0x800B253A */
extern unsigned char g_winners[6];            /* 0x800B2578 players at the goal (CollectWinners) */
extern unsigned char g_numWinners;            /* 0x800B257E */
extern unsigned char g_winner;                /* 0x800B257F */
extern char g_stageCleared;                   /* 0x800B2580 story stage won (mode 8 reads it) */
extern int g_boardUnused88, g_boardUnused8C;  /* 0x800B2588, 0x800B258C set, never read */
extern short g_scriptBlock;                   /* 0x800B259E */
extern char g_showEventPicture;               /* 0x800B25A6 */
extern short g_scriptEntry;                   /* 0x800B2680 */
extern unsigned short g_boardStepCopy;        /* 0x800B26C4 copy of Board_Task's step */
extern int *g_loaderWork;                     /* 0x800B26CC */
extern unsigned char g_turnFlags[19];         /* 0x80115590 what happened this turn, TURN_* */

/* ---- Functions ---- */

/* src/board.c */
int  Board_Task(short *step, void *work);                 /* 0x800432F0 */
int  Board_PhaseSetup(unsigned short *step, void *work);  /* 0x800435E4 */
int  Board_PhaseTurn(unsigned short *step, void *work);   /* 0x80043A98 */
int  Board_PhaseMove(short *step, void *work);            /* 0x80044F24 */
int  Board_PhaseSquare(unsigned short *step, void *work); /* 0x800450B4 */
int  Board_PhaseEndTurn(unsigned short *step, void *work); /* 0x80046E60 */

#endif /* BOARD_H */
