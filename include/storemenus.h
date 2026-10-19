/*
 * storemenus.h - the store dialogs
 *
 * Shopping, the vacant lot, deliveries, retail prices, selling, the store offer, their money
 * windows.
 * The declarations of src/storemenus.c: the work areas of its tasks, its globals and its functions.
 * See docs/board.md.
 */

#ifndef STOREMENUS_H
#define STOREMENUS_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of MoneyWin_Task */
typedef struct MoneyWinWork
{
    int win;   /* 0x00 */
    unsigned char active;   /* 0x04 */
    unsigned char redraw;   /* 0x05 */
    unsigned char pad06[2];
    int timer;   /* 0x08 */
    unsigned char pad0C[88];
} MoneyWinWork;

/* work area of Shopping_Task */
typedef struct ShoppingWork
{
    int win;   /* 0x00 */
    int minBuy;   /* 0x04 */
    int maxBuy;   /* 0x08 */
    unsigned char listShown;   /* 0x0C */
    unsigned char camMode;   /* 0x0D */
    signed char arrived;   /* 0x0E */
    unsigned char refresh;   /* 0x0F */
    unsigned char costDirty;   /* 0x10 */
    signed char canDecline;   /* 0x11 */
    signed char cursor;   /* 0x12 */
    signed char store;   /* 0x13 */
    signed char level;   /* 0x14 */
    signed char bought;   /* 0x15 */
    signed char cpuChoice;   /* 0x16 */
    signed char points;   /* 0x17 */
    unsigned char pad18[2];
    signed char x;   /* 0x1A */
    signed char floor;   /* 0x1B */
    int paid;   /* 0x1C */
    int hintTimer;   /* 0x20 */
    int delay;   /* 0x24 */
    unsigned char pad28[60];
} ShoppingWork;

/* work area of VacantLot_Task; kind: what the dialog is for */
#define LOT_VACANT     0   /* landed on a vacant lot: open a store there */
#define LOT_BUYOUT     1   /* buy someone else's store and open another type there */
#define LOT_PICK       2   /* event: open a store on one of the vacant lots on squares where the player has no store (Stores_ListVacantNewSquares; left / right) */
#define LOT_PICK_FREE  3   /* event: the same, for free */
typedef struct VacantLotWork
{
    int win;   /* 0x00 the list window, -1 = none yet */
    int kind;   /* 0x04 LOT_* */
    unsigned char listShown;   /* 0x08 */
    unsigned char camMode;   /* 0x09 Camera_MoveTo mode */
    signed char arrived;   /* 0x0A camera there */
    unsigned char refresh;   /* 0x0B rebuild the list */
    unsigned char costDirty;   /* 0x0C update the cost window */
    unsigned char types[4];   /* 0x0D store types offered (of the square's four) */
    signed char numTypes;   /* 0x11 entries in types; that row is "Do not open" / "Cancel purchase" */
    signed char lot;   /* 0x12 LOT_PICK*: index in g_storeList */
    signed char store;   /* 0x13 */
    signed char squareId;   /* 0x14 */
    signed char storeType;   /* 0x15 type in the selected row, -1 = none */
    signed char choice;   /* 0x16 row chosen */
    signed char cpuChoice;   /* 0x17 row a CPU player goes for */
    signed char x;   /* 0x18 */
    signed char floor;   /* 0x19 */
    short buyPrice;   /* 0x1A LOT_BUYOUT: paid to the owner */
    short ownerGain;   /* 0x1C LOT_BUYOUT: buyPrice minus the store's value */
    short prices[5];   /* 0x1E of each row (buyPrice included) */
    int hintTimer;   /* 0x28 */
    int delay;   /* 0x2C */
    unsigned char pad30[52];
} VacantLotWork;

/* work area of ItemList_Task */
typedef struct ItemListWork
{
    int win;   /* 0x00 */
    int x;   /* 0x04 */
    int y;   /* 0x08 */
    int type;   /* 0x0C */
    int limit;   /* 0x10 */
    unsigned char busy;   /* 0x14 */
    unsigned char pad15[79];
} ItemListWork;

/* work area of Delivery_Task */
#define DELIVERY_PAID  0
#define DELIVERY_FREE  1   /* event 34: the goods cost nothing */
typedef struct DeliveryWork
{
    int win;   /* 0x00 the goods list window */
    int store;   /* 0x04 */
    int kind;   /* 0x08 DELIVERY_* */
    unsigned char listShown;   /* 0x0C */
    unsigned char camMode;   /* 0x0D Camera_MoveTo mode */
    signed char arrived;   /* 0x0E camera there */
    unsigned char rebuild;   /* 0x0F 1: rebuild the list text, 2: and reset its menu */
    unsigned char costDirty;   /* 0x10 update the cost window */
    unsigned char pad11[1];
    signed char storeType;   /* 0x12 copied, not read */
    signed char level;   /* 0x13 goods level of the selected row */
    signed char cursor;   /* 0x14 */
    signed char firstLevel;   /* 0x15 most expensive level the store may sell: 6 - numLevels */
    signed char cpuIndex;   /* 0x16 next entry of cpuLevels */
    signed char numLevels;   /* 0x17 rows of goods (k_chainLevels by chain); that row is "Complete Delivery" */
    signed char cpuCount;   /* 0x18 entries in cpuLevels */
    unsigned char cpuLevels[8];   /* 0x19 goods a CPU player delivers (Cpu_ChooseDelivery) */
    signed char x;   /* 0x21 */
    signed char floor;   /* 0x22 */
    unsigned char pad23[1];
    int hintTimer;   /* 0x24 */
    int delay;   /* 0x28 */
    unsigned char pad2C[56];
} DeliveryWork;

/* work area of RetailPrices_Task */
typedef struct RetailPricesWork
{
    int win;   /* 0x00 */
    int x;   /* 0x04 */
    int y;   /* 0x08 */
    int store;   /* 0x0C */
    unsigned char busy;   /* 0x10 */
    unsigned char pad11[83];
} RetailPricesWork;

/* work area of RetailPricesInput_Task */
typedef struct RetailPricesInputWork
{
    int win;   /* 0x00 */
    int x;   /* 0x04 */
    int y;   /* 0x08 */
    int store;   /* 0x0C */
    signed char storeType;   /* 0x10 */
    unsigned char busy;   /* 0x11 */
    unsigned char done;   /* 0x12 */
    unsigned char pad13[81];
} RetailPricesInputWork;

/* work area of SellStore_Task; kind: */
#define SELL_CHOICE         0   /* sell one of the stores, or not (event script) */
#define SELL_FORCED         1   /* the balance is negative: sell until it is not */
#define SELL_CLOSED         2   /* a store that is closed for now (Stores_ListClosed), or not */
#define SELL_CLOSED_FORCED  3   /* the same without "Do not sell" (a script's choice) */
typedef struct SellStoreWork
{
    int win;   /* 0x00 the store list window */
    int percent;   /* 0x04 of the store's value paid */
    int kind;   /* 0x08 SELL_* */
    unsigned char listShown;   /* 0x0C */
    unsigned char rebuild;   /* 0x0D 1: rebuild the list text, 2: and the list and its menu */
    unsigned char camMode;   /* 0x0E Camera_MoveTo mode */
    signed char arrived;   /* 0x0F camera there */
    unsigned char canDecline;   /* 0x10 the last row is "Do not sell" */
    signed char top;   /* 0x11 row at the top of the window (more than 4 rows: the list scrolls around) */
    unsigned char unused12;   /* 0x12 set to 0, not read */
    signed char index;   /* 0x13 g_storeList entry of the selected row; g_storeListCount = "Do not sell" */
    signed char cpuChoice;   /* 0x14 store a CPU player sells, -1 = none */
    signed char rows;   /* 0x15 */
    signed char store;   /* 0x16 */
    signed char x;   /* 0x17 */
    signed char floor;   /* 0x18 */
    unsigned char pad19[3];
    int hintTimer;   /* 0x1C */
    int delay;   /* 0x20 */
    unsigned char pad24[64];
} SellStoreWork;

/* work area of StoreOffer_Task */
typedef struct StoreOfferWork
{
    int win;   /* 0x00 the "It will cost ..." window */
    int buy;   /* 0x04 0: open a store here, 1: buy this store (from its owner) */
    unsigned char shown;   /* 0x08 */
    unsigned char camMode;   /* 0x09 Camera_MoveTo mode */
    signed char arrived;   /* 0x0A camera there */
    signed char cursor;   /* 0x0B answer (0 = yes) */
    unsigned char expand;   /* 0x0C a neighboring store on this square is the player's */
    signed char storeType;   /* 0x0D that of the player's store on this square */
    signed char store;   /* 0x0E */
    unsigned char costDirty;   /* 0x0F update the cost window */
    signed char x;   /* 0x10 */
    signed char floor;   /* 0x11 */
    short buyPrice;   /* 0x12 paid to the owner */
    short price;   /* 0x14 of the store type, by the player's neighboring stores */
    short ownerGain;   /* 0x16 buyPrice minus the store's value */
    short total;   /* 0x18 */
    unsigned char unused1A;   /* 0x1A */
    unsigned char unused1B;   /* 0x1B */
    int hintTimer;   /* 0x1C */
    int delay;   /* 0x20 */
    unsigned char pad24[64];
} StoreOfferWork;

/* work area of DeliveryStore_Task */
typedef struct DeliveryStoreWork
{
    int win;   /* 0x00 the store list window */
    int player;   /* 0x04 */
    unsigned char listShown;   /* 0x08 */
    unsigned char rebuild;   /* 0x09 1: rebuild the list text, 2: and reset its menu */
    unsigned char unused0A;   /* 0x0A set to 0, not read */
    signed char chosen;   /* 0x0B store in the selected row, -1 = "Do not deliver" */
    signed char storeType;   /* 0x0C copied, not read */
    unsigned char camMode;   /* 0x0D Camera_MoveTo mode */
    signed char arrived;   /* 0x0E camera there */
    signed char cpuChoice;   /* 0x0F store a CPU player delivers to, -1 = none */
    signed char top;   /* 0x10 row at the top of the window (more than 3 stores: the list scrolls around) */
    signed char x;   /* 0x11 */
    signed char floor;   /* 0x12 */
    unsigned char pad13[1];
    int hintTimer;   /* 0x14 */
    int delay;   /* 0x18 */
    unsigned char pad1C[72];
} DeliveryStoreWork;

/* work area of CostWin_Task */
typedef struct CostWinWork
{
    int win;   /* 0x00 */
    int dirty;   /* 0x04 */
    int money;   /* 0x08 */
    int cost;   /* 0x0C */
    int subtract;   /* 0x10 */
    int result;   /* 0x14 */
    unsigned char shown;   /* 0x18 */
    unsigned char pad19[75];
} CostWinWork;

/* work area of StealItem_Task */
typedef struct StealItemWork
{
    int win;   /* 0x00 */
    unsigned char listShown;   /* 0x04 */
    unsigned char mode;   /* 0x05 */
    signed char arrived;   /* 0x06 */
    unsigned char refresh;   /* 0x07 */
    signed char count;   /* 0x08 */
    signed char store;   /* 0x09 */
    signed char player;   /* 0x0A */
    signed char item;   /* 0x0B */
    signed char level;   /* 0x0C */
    signed char x;   /* 0x0D */
    signed char floor;   /* 0x0E */
    unsigned char pad0F[1];
    short frame;   /* 0x10 */
    short stopFrame;   /* 0x12 */
    int amount;   /* 0x14 */
    unsigned char pad18[4];
    int delay;   /* 0x1C */
    unsigned char pad20[68];
} StealItemWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char k_storeListHeader[];      /* 0x8001356C */
extern char *k_storeOfferTexts[4];    /* 0x800135A8 "1.0".."1.6"; loaded by StoreOffer_Task, not used */
extern u16 k_chainLevels[4];          /* 0x800AD9B4 goods levels a store sells by Store.chainLen: 3, 4, 5, 6 (the cheapest ones) */
extern u8 k_cashBackChance[8];        /* 0x800B1F74 chance in % of a cash back when shopping, by goods level */
extern u32 k_storeOfferInit;          /* 0x800B1FB8 bytes 10, 12, 14, 16: price factors x 10 (as k_storeOfferTexts); StoreOffer_Task copies them into an unused local */
extern char g_offerMsgText[80];       /* 0x80111F10 */
extern u8 g_itemShopTable[8][0x250];  /* 0x80112A40 never written: StealItem_Task reads [window][7], which is 0 */

/* ---- Functions ---- */

/* src/storemenus.c */
u32 MoneyWin_Task(short *step, MoneyWinWork *work);       /* 0x8005760C */
u32 Shopping_Task(short *step, ShoppingWork *work);       /* 0x80057D20 */
u32 VacantLot_Task(short *step, VacantLotWork *work);     /* 0x800598DC */
u32 ItemList_Task(u16 *step, ItemListWork *work);         /* 0x8005B434 */
u32 Delivery_Task(short *step, DeliveryWork *work);       /* 0x8005B788 */
u32 RetailPrices_Task(u16 *step, RetailPricesWork *work); /* 0x8005CB18 */
u32 RetailPricesInput_Task(short *step, RetailPricesInputWork *work); /* 0x8005CEE0 */
u32 SellStore_Task(short *step, SellStoreWork *work);     /* 0x8005D4D8 */
u32 StoreOffer_Task(short *step, StoreOfferWork *work);   /* 0x8005ECE4 */
void StoreOffer_Question(char buy, char expand, char *name); /* 0x8005FC78 */
u32 DeliveryStore_Task(short *step, DeliveryStoreWork *work); /* 0x8005FDC0 */
u32 CostWin_Task(u16 *step, CostWinWork *work);           /* 0x800613AC */
int StealItem_Task(short *step, StealItemWork *work);     /* 0x80061658 */

#endif /* STOREMENUS_H */
