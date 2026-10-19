/*
 * events.h - event cards
 *
 * Drawing one at random or by choice, showing it, the points exchange at the bank, carrying a card
 * out, choosing its target.
 * The declarations of src/events.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/board.md.
 */

#ifndef EVENTS_H
#define EVENTS_H

#include "common.h"
#include "gfx.h"

/* The 53 event cards (g_eventCards, 0x800ABF9C): bought with points in the points exchange and
 * carried out by Event_ApplyTask. Not the board's events (EVE_*, below), which have scripts. */
#define EVENT_COUNT 53

/* Event card ids (g_eventCards) */
enum
{
    EV_EARN_POINT,          /* Earn 1 Point. */
    EV_MONEY_200,           /* Receive 200. */
    EV_STOP_DELIVERY,       /* Stop rival delivery. (2 turns) */
    EV_STOP_NEW_STORES,     /* Stop rival new stores. (2 turns) */
    EV_MONEY_400,           /* Receive 400. */
    EV_DELIVER_OWN,         /* Deliver your store. (1 store) */
    EV_DELIVER_RIVAL,       /* Deliver rival store. (1 store) */
    EV_REVERSE,             /* Reverse direction. (1 rival) */
    EV_CLOSE_STORE,         /* Close rival store. (1 store) */
    EV_MONEY_700,           /* Receive 700. */
    EV_FREE_STORE,          /* Open a store for free. (Random) */
    EV_SLOW_DICE,           /* Rival pencil 1 and 2. (2 turns) */
    EV_DICE_E,              /* Rival pencil E. (2 turns) */
    EV_MONEY_STORES_150,    /* Receive money. (Stores x 150) */
    EV_CAPTURE_STORE,       /* Capture rival store. (No items) */
    EV_LOWER_PENCIL,        /* Lower pencil movement. (2 turns) */
    EV_MONEY_3_4,           /* Reduce rival money. (3/4) */
    EV_OUT_OF_STOCK_1,      /* Rival out of stock. (1 store) */
    EV_MONEY_1000,          /* Receive 1000. */
    EV_SWITCH_PLACES,       /* Switch places with rival. */
    EV_ALL_REVERSE,         /* All players reverse. (Next turn) */
    EV_MONEY_DOUBLE,        /* Money double. (Next turn) */
    EV_STOP_RIVAL_MONEY,    /* Stop rival money. (Next turn) */
    EV_SHOES,               /* Equip "Chrip-Chrip" shoes. */
    EV_MONEY_1300,          /* Receive 1300. */
    EV_STORE_RIVAL_MONEY,   /* Open new store with rival money. */
    EV_DELIVER_ALL_OWN,     /* Deliver to all your stores. */
    EV_CUT_BY_STORES_150,   /* Cut rival money. (stores x 150) */
    EV_CLOSE_OUT_OF_STOCK,  /* Stores out of stock close. (All) */
    EV_CLOSE_STORE_LV2,     /* Close one rival store. (Lvl 2) */
    EV_MONEY_1600,          /* Receive 1600. */
    EV_ITEMS_DOUBLE,        /* Store items double. (4 turns) */
    EV_OUT_OF_STOCK_4,      /* Rival out of stock. (4 stores) */
    EV_MONEY_STORES_300,    /* Receive Money. (Stores x 300) */
    EV_DELIVER_FREE,        /* Deliver to store free. */
    EV_DELIVER_CHEAP,       /* Deliver cheap items. */
    EV_DELIVER_EXPENSIVE,   /* Deliver expensive item. */
    EV_HALVE_POINTS,        /* Reduce rival points. (1/2) */
    EV_MONEY_HALF,          /* Reduce rival money. (1/2) */
    EV_MONEY_SHUFFLE,       /* Players money exchange. (Random) */
    EV_ALL_OUT_OF_STOCK,    /* Receive all stores out of stock. */
    EV_CLOSE_STORE_LV3,     /* Close one rival store. (Lvl 3) */
    EV_HALVE_ALL,           /* Reduce all rivals' money. (1/2) */
    EV_CUT_BY_STORES_300,   /* Cut rival money. (stores x 300) */
    EV_MONEY_TO_1,          /* Reduce rival money to 1. */
    EV_TAKE_HALF_MONEY,     /* Take rival money. (1/2) */
    EV_CLOSE_STORE_LV4,     /* Close one rival store. (Lvl 4) */
    EV_UPGRADE_STORE,       /* Upgrade your store. (Lvl 4) */
    EV_DELIVER_COSTLY,      /* Deliver costly items your stores. */
    EV_ALL_MONEY_TO_1,      /* Reduce all rivals's money to 1. */
    EV_TAKE_STORE_LV4,      /* Take one rival store. (Lvl 4) */
    EV_RIVALS_MONEY,        /* Receive money amount of rival. */
    EV_CLAIM_VICTORY        /* Claim Victory! */
};
typedef struct EventCardDef
{
    int points;                  /* 0x00 cost in the points exchange (99 = "Claim Victory!") */
    char *name;                  /* 0x04 e.g. "Receive 200.(Money)" */
} EventCardDef;

/* The random event of a player's turn (g_eventWeights, 0x800ADC64; Event_PickRandom): the first
 * row whose threshold for the player's bracket is above a random number 0-199. Brackets: 0 the
 * first year, 1 leading, 2-6 behind the leader by up to 1500 / 3000 / 6000 / 10000 / more, 7 half
 * the goal reached. */
typedef struct EventWeights
{
    unsigned char event;         /* 0x00 event id; 99 = the month's event (g_monthEvents) */
    unsigned char below[8];      /* 0x01 cumulative thresholds out of 200, by bracket */
} EventWeights;                  /* size 9 */

/* The event card pictures (k_eventCardArchive, 0x800AB924): LZSS-packed TIMs, unpacked by
 * EventCard_Task (picture 0 for event 0x4C, else a random one of 1-3). */
typedef struct EventCardArchive
{
    unsigned int count;          /* 0x00 4 */
    unsigned int offsets[5];     /* 0x04 from the start of the archive: the 4 pictures, then the end */
    unsigned char data[0x678 - 0x18];
} EventCardArchive;              /* size 0x678 */

/* The events (EVE_<id>.J, Board_Task's w->eventId, g_eventId; the picture C_<id>_.TIM): 24-37,
 * 40-54, 58-72, 75 and 76 drawn after a roll of 8 (Event_PickRandom, g_eventWeights), 80-88 the
 * months' events (g_monthEvents); "it": the current player. The ids not listed are never played
 * (see scripts/events/). */
enum EventScriptId
{
    EVE_BANKRUPT           =  0,  /* a player went bankrupt (Board_Task) */
    EVE_STAGE_LOST         =  1,  /* stage mode: the stage is lost (Board_Task) */
    EVE_SELL_150           = 24,  /* the player may sell a store for 150% of its value */
    EVE_SELL_200           = 25,  /* ... for 200% */
    EVE_SELL_250           = 26,  /* ... for 250% */
    EVE_FORCED_SELL_200    = 27,  /* it sells stores for 200% until its money is not negative */
    EVE_FORCED_SELL_250    = 28,  /* ... for 250% */
    EVE_FORCED_SELL_300    = 29,  /* ... for 300% */
    EVE_OWN_CLOSED         = 30,  /* its stores closed for 1 month */
    EVE_ALL_CLOSED         = 31,  /* every player's stores closed for 1 month */
    EVE_RIVALS_CLOSED      = 32,  /* the other players' stores closed for 1 month */
    EVE_ALL_CLOSED_2       = 33,  /* every player's stores closed for 2 months */
    EVE_RIVALS_TAKE_HALF   = 34,  /* each other player takes half the year's amount from it */
    EVE_RIVALS_TAKE        = 35,  /* each other player takes the year's amount from it */
    EVE_RIVALS_GIVE        = 36,  /* each other player gives it the year's amount */
    EVE_RIVALS_GIVE_1_5    = 37,  /* each other player gives it 1.5 times the year's amount */
    EVE_SANTA_X3           = 40,  /* Santa gives it 3 times the year's amount */
    EVE_SANTA_X5           = 41,  /* ... 5 times */
    EVE_SANTA_X7           = 42,  /* ... 7 times */
    EVE_STOP               = 43,  /* the player stops (no direction) */
    EVE_TURN_AROUND        = 44,  /* the player turns around */
    EVE_ROLL_AGAIN         = 45,  /* the player rolls again */
    EVE_ANOTHER            = 46,  /* another event follows */
    EVE_VACANT_LOT         = 47,  /* it may open a store on a vacant lot along the route */
    EVE_GOODS_LEVEL_4      = 48,  /* its stores: 2 goods slots at level 4 */
    EVE_GOODS_CHEAPER      = 49,  /* its stores' goods 1 level cheaper */
    EVE_GOODS_LEVEL_0      = 50,  /* its stores: 1 goods slot at level 0 (the most expensive) */
    EVE_DROPPED_X3         = 51,  /* it dropped 3 times the year's amount */
    EVE_DROPPED_X6         = 52,  /* ... 6 times */
    EVE_DROPPED_THIRD      = 53,  /* it dropped 1/3 of its money */
    EVE_DROPPED_HALF       = 54,  /* it dropped 1/2 of its money */
    EVE_CALENDAR_12        = 55,  /* the calendar moves on 12 months (never played) */
    EVE_CALENDAR_4         = 56,  /* ... 4 months (never played) */
    EVE_CALENDAR_1         = 57,  /* ... 1 month (never played) */
    EVE_OWN_EMPTY_CLOSE    = 58,  /* its stores without goods close; it gets 150% of their value */
    EVE_ALL_EMPTY_CLOSE    = 59,  /* every player's stores without goods close */
    EVE_RIVALS_EMPTY_CLOSE = 60,  /* the other players' stores without goods close */
    EVE_MOVE_1             = 61,  /* the player moves 1 square */
    EVE_MOVE_2             = 62,
    EVE_MOVE_3             = 63,
    EVE_MOVE_4             = 64,
    EVE_MOVE_5             = 65,
    EVE_MOVE_6             = 66,
    EVE_MOVE_7             = 67,
    EVE_MOVE_8             = 68,  /* ... 8 squares */
    EVE_NO_ELEVATOR        = 69,  /* it can't take the elevator for 1 month */
    EVE_ALL_NO_ELEVATOR    = 70,  /* every player still in can't take the elevator for 1 month */
    EVE_RIVALS_NO_ELEVATOR = 71,  /* every other player still in can't... */
    EVE_SAME_MONEY         = 72,  /* everyone now has as much money as the player */
    EVE_CHANCE_QUIZ        = 73,  /* a quiz about the goods (never played) */
    EVE_VACANT_LOT_2       = 74,  /* as EVE_VACANT_LOT (never played) */
    EVE_SHOES              = 75,  /* it puts on the Chirp-Chirp shoes */
    EVE_MISS               = 76,  /* "Miss": nothing */
    EVE_SPEED_QUIZ         = 77,  /* a timed quiz about the goods (never played) */
    EVE_SALE_JANUARY       = 80,  /* every store sells at 50% for 1 month: January's event */
    EVE_SALE_SUMMER        = 81,  /* ... June, July and September */
    EVE_SALE_OCTOBER       = 82,  /* ... at 70%: October */
    EVE_SALE_DECEMBER      = 83,  /* ... December */
    EVE_SALE_MARCH         = 84,  /* ... at 30%: March */
    EVE_SALE_SPRING        = 85,  /* ... April and May */
    EVE_SALE_AUGUST        = 86,  /* ... August */
    EVE_SALE_NOVEMBER      = 87,  /* ... November */
    EVE_SALE_FEBRUARY      = 88,  /* ... February */
    EVE_PRICES_UP          = 99   /* its goods' prices up 1% (never played) */
};
#define EVE_NOT_DRAWN  99    /* Event_PickRandom: no row of g_eventWeights matched */

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of EventCard_Task */
typedef struct EventCardWork
{
    unsigned char pad00[4];
    int eventId;   /* 0x04 */
    signed char points;   /* 0x08 the points the card gives (g_eventCardPoints) */
    unsigned char loaded;   /* 0x09 the cards are dealt */
    signed char choice;   /* 0x0A the card chosen, 0-2 */
    signed char cpuChoice;   /* 0x0B a CPU player's (random) */
    unsigned char disp;   /* 0x0C what is drawn: CARDS_* */
    signed char slot;   /* 0x0D sound slot */
    short angle;   /* 0x0E */
    int scaleX;   /* 0x10 */
    int scaleY;   /* 0x14 */
    short x;   /* 0x18 the chosen card's shadow, flying off */
    short y;   /* 0x1A */
    ScreenPos cards[3];   /* 0x1C */
    int timer;   /* 0x28 */
    unsigned char pad2C[56];
} EventCardWork;

/* work area of EventPick_Task */
typedef struct EventPickWork
{
    int win;   /* 0x00 */
    int id;   /* 0x04 */
    unsigned char done;   /* 0x08 */
    unsigned char pad09[91];
} EventPickWork;

/* work area of EventCard_LoadTask */
typedef struct EventCardLoadWork
{
    short handle;   /* 0x00 */
    short block;   /* 0x02 */
    unsigned char pad04[96];
} EventCardLoadWork;

/* work area of PointsExchange_Task */
typedef struct PointsExchangeWork
{
    int win;   /* 0x00 */
    unsigned char shown;   /* 0x04 */
    unsigned char refresh;   /* 0x05 */
    unsigned char mode;   /* 0x06 */
    signed char arrived;   /* 0x07 */
    signed char scroll;   /* 0x08 */
    signed char card;   /* 0x09 */
    signed char cpuCard;   /* 0x0A the card a CPU player chose (Cpu_ChooseExchange) */
    signed char target;   /* 0x0B */
    unsigned char unusable[7];   /* 0x0C [1 + row]: the card of that row has no use now */
    unsigned char isCpu;   /* 0x13 saved while RULE_AUTO_EVENT plays the dialogs */
    signed char x;   /* 0x14 */
    signed char floor;   /* 0x15 */
    unsigned char pad16[2];
    int timeout;   /* 0x18 */
    int delay;   /* 0x1C */
    unsigned char pad20[68];
} PointsExchangeWork;

/* work area of Event_ApplyTask */
typedef struct EventApplyWork
{
    short event;   /* 0x00 EV_* (with player: the points exchange's result) */
    short player;   /* 0x02 the rival it is played on */
    signed char showShops;   /* 0x04 never set: the shop-open steps 20-21 are not reached */
    signed char result;   /* 0x05 user data: 1 for EV_SWITCH_PLACES and EV_CLAIM_VICTORY */
    signed char store;   /* 0x06 */
    unsigned char pad07[5];
    short effectStep;   /* 0x0C step of the Effect_* function */
    unsigned char pad0E[2];
    unsigned char effectWork[84];   /* 0x10 its work area */
} EventApplyWork;

/* work area of PlayerSelect_Task */
typedef struct PlayerSelectWork
{
    int id;   /* 0x00 */
    int event;   /* 0x04 */
    int exclude;   /* 0x08 */
    unsigned char shown;   /* 0x0C */
    unsigned char skipSelf;   /* 0x0D */
    unsigned char panelMode;   /* 0x0E */
    unsigned char choices[5];   /* 0x0F player of each menu row; 0x80 = cannot be picked */
    signed char count;   /* 0x14 */
    signed char result;   /* 0x15 */
    unsigned char unused16;   /* 0x16 */
    unsigned char unused17;   /* 0x17 */
    int delay;   /* 0x18 */
    unsigned char pad1C[72];
} PlayerSelectWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 k_eventApplyTable[12];             /* 0x80016EB4 */
extern char *k_eventPickTexts[100];          /* 0x800AB794 the event pick window's text per event (BoardRules.enable[8]) */
extern EventCardArchive k_eventCardArchive;  /* 0x800AB924 */
extern EventCardDef g_eventCards[53];        /* 0x800ABF9C */
extern EventWeights g_eventWeights[49];      /* 0x800ADC64 */
extern u8 g_monthEvents[12];                 /* 0x800ADE20 */
extern int g_eventId;                        /* 0x800B2638 */
extern char g_eventDrawn;                    /* 0x800B263C */
extern u32 g_eventCardPoints;                /* 0x800B2640 the points the event card gives (1-3), also its frame picture; 0 for event 0x4C */
extern u16 g_debugMsgCnt;                    /* 0x800B2648 */
extern u8 g_eventTargets[53];                /* 0x80111F70 */
extern u32 g_debugStep;                      /* 0x80117D00 */

/* ---- Functions ---- */

/* src/events.c */
void EventCard_DrawFrame(short a, short b);               /* 0x8006BC3C */
u32 EventCard_Task(short *step, EventCardWork *work);     /* 0x8006BC80 */
u32 EventPick_Task(u16 *step, EventPickWork *work);       /* 0x8006CECC */
u32 EventCard_LoadTask(u16 *step, EventCardLoadWork *work); /* 0x8006D2E4 */
u32 PointsExchange_Task(u16 *step, PointsExchangeWork *work); /* 0x8006D4F4 */
u32 Event_ApplyTask(u16 *step, EventApplyWork *work, int arg); /* 0x8006E3EC */
u32 PlayerSelect_Task(short *step, PlayerSelectWork *work); /* 0x8006FA50 */
void Event_ListTargets(void);                             /* 0x80070060 */
u32 Event_PickRandom(u32 player);                         /* 0x8008CC4C */

#endif /* EVENTS_H */
