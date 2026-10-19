/*
 * shopopen.h - the stores' pictures and the shop-open animation
 *
 * The stores' pictures in VRAM and the shop-open animation after a move.
 * The declarations of src/shopopen.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/board.md.
 */

#ifndef SHOPOPEN_H
#define SHOPOPEN_H

#include "common.h"

/* A store's owner and chain when ShopOpen_Task last looked (g_storeSnapshot). */
typedef struct StoreSnapshot
{
    signed char owner;
    signed char chainLen;
    unsigned char chainPos;
} StoreSnapshot;                 /* size 3 */

/* g_storeImageState: a store's picture in VRAM */
#define STORE_IMAGE_NONE       0
#define STORE_IMAGE_UPLOADING  1
#define STORE_IMAGE_READY      2

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of ShopOpen_Task */
typedef struct ShopOpenWork
{
    signed char state;   /* 0x00 BANNER_*: the banners drawn */
    signed char store;   /* 0x01 */
    signed char storeType;   /* 0x02 */
    signed char owned;   /* 0x03 entries in ownedList */
    signed char changed;   /* 0x04 entries in changedList */
    signed char vacantMarked;   /* 0x05 entries in g_shopOpenVacantMarked */
    signed char vacant;   /* 0x06 entries in g_shopOpenVacant */
    unsigned char pad07[1];
    short slide;   /* 0x08 of the banners */
    short loadIndex;   /* 0x0A */
    short handles[4];   /* 0x0C picture files being read, one per owned store */
    short loadAll;   /* 0x14 1: all pictures read at once (4 at most) */
    unsigned char pad16[2];
    unsigned char *ownedList;   /* 0x18 g_shopOpenOwned */
    unsigned char *changedList;   /* 0x1C g_shopOpenChanged */
    int delay;   /* 0x20 */
    unsigned char pad24[64];
} ShopOpenWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_storeClutPos[80];           /* 0x800AA7C8 */
extern u8 g_shopOpenOwned[48];             /* 0x800FFF08 */
extern u8 g_shopOpenChanged[48];           /* 0x800FFF38 */
extern u8 g_shopOpenVacantMarked[48];      /* 0x800FFF68 */
extern u8 g_shopOpenVacant[40];            /* 0x800FFF98 */
extern short g_storeImageState[40];        /* 0x80102EC0 STORE_IMAGE_* of each store's picture */
extern StoreSnapshot g_storeSnapshot[40];  /* 0x80112948 Stores_TakeSnapshot */

/* ---- Functions ---- */

/* src/shopopen.c */
void Stores_TakeSnapshot(void);                      /* (inline code in the original) */
u32 ShopOpen_Task(short *step, ShopOpenWork *work);  /* 0x8004BAC8 */
void Stores_LoadImages(void);                        /* 0x8004CFE0 */
void StoreImage_VramPos(short store, short *pos);    /* 0x8004DE3C */
void StoreImage_ClutPos(int store, u16 *pos);        /* 0x8004DEA4 */
void StoreImage_Upload(short store, u32 *tim);       /* 0x8004E000 */

#endif /* SHOPOPEN_H */
