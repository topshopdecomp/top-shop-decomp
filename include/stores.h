/*
 * stores.h - the stores
 *
 * Opening and closing, chains, prices and goods, store types, the lists of stores the dialogs and
 * the CPU players choose from.
 * The declarations of src/stores.c: its types and constants, its globals and its functions.
 * See docs/board.md.
 */

#ifndef STORES_H
#define STORES_H

#include "common.h"

/* Goods a store type sells, from the most expensive (level 0) down. */
typedef struct StoreItem
{
    char *name;                  /* 0x00 e.g. "Diet Food" */
    short cost;                  /* 0x04 buying-in price */
    short markup;                /* 0x06 retail price = cost * markup / 100 (200 everywhere) */
} StoreItem;                     /* size 8 */

#define NUM_STORE_TYPES 44
#define MAX_STORES      40   /* g_stores; a stage uses StageDef.numStores of them */
#define NUM_STORES      (g_stageDefs[g_boardStage].numStores)   /* stores of the current stage */

/* Selectors of the Stores_List* functions and Player_Resolve (the script's SCRIPT_*_PLAYERS) */
#define WHO_ANY       (-1)   /* any player (a store: owned by anyone) */
#define WHO_CURRENT   (-2)   /* the current player */
#define WHO_OTHERS    (-3)   /* the players but the current one */

/* A kind of store (g_storeTypes[44], 0x800ACEF4), e.g. "Pharmacy". */
typedef struct StoreType
{
    unsigned char available;     /* 0x00 cleared once a store of this type is opened */
    unsigned char pad01;
    short value;                 /* 0x02 base value of the store */
    char *name;                  /* 0x04 */
    StoreItem items[6];          /* 0x08 indexed by StoreGoods.level */
    struct Store *store;         /* 0x38 the store of this type on the board, NULL if none (Store_Open, Store_Close) */
} StoreType;                     /* size 0x3C */

/* One goods slot of a store. */
typedef struct StoreGoods
{
    unsigned char active;        /* 0x00 */
    signed char level;           /* 0x01 index into the store type's price table */
    short basePrice;             /* 0x02 price * percentage from the store type (Store_UpdatePrices) */
    short price;                 /* 0x04 basePrice, times pricePercent / 100 while the price is changed or the store closed */
} StoreGoods;                    /* size 6 */

/* One store on the board (g_stores, 0x80114A40). */
typedef struct Store
{
    unsigned char index;         /* 0x00 position in g_stores */
    signed char owner;           /* 0x01 -1 = none (tenant wanted), else player index */
    signed char squareId;        /* 0x02 matched against Board_GetSquareId */
    unsigned char storeType;     /* 0x03 index into g_storeTypes, 0xFF = none */
    unsigned char squareFlag;    /* 0x04 1 when Board_GetSquareFlag of its square is 2; set, never read */
    unsigned char tempClosed;    /* 0x05 "Temporary Closed." until closedUntil */
    signed char numGoods;        /* 0x06 goods slots in use; 0 = not operating ("Closed") */
    signed char maxGoods;        /* 0x07 slots the store can have (table 0x800AD9AC by chainLen) */
    signed char chainLen;        /* 0x08 length - 1 of the chain of neighboring stores of one owner (0-3; tables 0x800AD9AC, 0x800AD9B4) */
    unsigned char chainTypes;    /* 0x09 one bit per store type in the chain */
    unsigned char chainPos;      /* 0x0A position in the chain, counted from its end */
    signed char x, floor;        /* 0x0B square of the store (Board_GetCell) */
    unsigned char priceChanged;  /* 0x0D prices are pricePercent % until priceUntil */
    short pricePercent;          /* 0x0E 100 = normal */
    unsigned short priceUntil;   /* 0x10 turn number */
    unsigned short closedUntil;  /* 0x12 turn number */
    StoreGoods goods[8];         /* 0x14 */
    StoreType *type;             /* 0x44 */
} Store;                         /* size 0x48 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern StoreType g_storeTypes[NUM_STORE_TYPES];  /* 0x800ACEF4 */
extern u16 g_kindMaxGoods[4];                    /* 0x800AD9AC */
extern u8 g_ownSquareCount;                      /* 0x800B2670 */
extern u8 g_storeTypeList[4];                    /* 0x800B2674 */
extern u8 g_storeListCount;                      /* 0x800B26F8 */
extern u8 g_storeTypeCount;                      /* 0x800B2719 */
extern u8 g_storeList[40];                       /* 0x80113CC0 */
extern u8 g_squareStoreCount[11];                /* 0x80114970 */
extern Store g_stores[MAX_STORES];               /* 0x80114A40 */

/* ---- Functions ---- */

/* src/stores.c */
void Store_ClearGoods(int store);                         /* C-only helper, no original address */
int Store_GetValue(short store);                          /* 0x8007657C */
void Store_UpdatePrices(short store);                     /* 0x8007661C */
void Stores_UpdateChains(void);                           /* 0x80076728 */
void Store_SortGoods(int store);                          /* 0x80076C2C */
void Store_Open(short store, short storeType, short owner); /* 0x80077754 */
void Store_SetOwner(short store, char owner);             /* 0x800778A8 */
void Store_Close(short store);                            /* 0x800779AC */
u32 Stores_ListByOwner(short who);                        /* 0x800789C8 */
u32 Stores_ListByOwnerFloor(short who, int level);        /* 0x80078D10 */
u32 Stores_ListChainHeads(short who, short length);       /* 0x8007908C */
u32 Stores_ListChainHeadsUpTo(short who, short length, short maxLength); /* 0x8007946C */
u32 Stores_ListExtendable(short who);                     /* 0x800798B4 */
u32 Stores_ListOperating(short who);                      /* 0x80079D50 */
u32 Stores_ListNotFull(short who);                        /* 0x8007A0DC */
u32 Stores_ListClosed(short who);                         /* 0x8007A478 */
u32 Stores_ListByChainLength(short who, short length);    /* 0x8007A800 */
u32 Stores_ListVacantNewSquares(short who);               /* 0x8007ABA0 */
int Store_CountOwnedNeighbors(short owner, short store);  /* 0x8007ADD0 */
int StoreType_GetPrice(short storeType, short extra);     /* 0x8007B310 */
void Stores_UpdateOwned(void);                            /* 0x8007B38C */
int Stores_CountOwnedOfType(short owner, short storeType); /* 0x8007B7D0 */
int Store_GetOwnedTypeAt(short owner, short squareId);    /* 0x8007B860 */
u8 StoreTypes_List(int category);                         /* 0x8007B8F4 */
u32 Stores_ListVacant(void);                              /* 0x8007BA24 */

#endif /* STORES_H */
