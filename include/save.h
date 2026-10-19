/*
 * save.h - game saves on the memory card
 *
 * The slots, packing and unpacking a game, the story progress, the card tasks of the save and load
 * screens.
 * The declarations of src/save.c: its types and constants, the work areas of its tasks, its globals
 * and its functions.
 * See docs/card.md.
 */

#ifndef SAVE_H
#define SAVE_H

#include "common.h"
#include "stores.h"

/* g_cardTaskState when Card_SaveTask / Card_ReadSavesTask is done */
#define CARDTASK_CANCELED   0  /* no card / quit, not formatted, no save, a bad one */
#define CARDTASK_READY      1
#define CARDTASK_REMOVED    2   /* the card was taken out: start again */

typedef struct SavePlayer
{
    unsigned char isCpu, pad, charId, out;     /* 0x00 */
    char name[13];                             /* 0x04 */
    char title[13];                            /* 0x11 Player.title */
    signed char x, floor;                      /* 0x1E */
    unsigned char dirX, dirFloor;              /* 0x20 */
    unsigned char noTargeting, paydayEffect;   /* 0x22 */
    unsigned char diceEffect, noElevator, noDelivery, vandalized;  /* 0x24 */
    unsigned char exempt, historyLen, points, shoes; /* 0x28 */
    short screenX, screenY;                    /* 0x2C */
    short rankKey;                             /* 0x30 */
    unsigned short customers, purchases;       /* 0x32 */
    short paydays, stepsSincePayday;           /* 0x36 */
    unsigned short diceEffectUntil, noElevatorUntil, noDeliveryUntil, vandalizedUntil;  /* 0x3A */
    unsigned char pad42[2];
    int totalSteps;                            /* 0x44 */
    int money;                                 /* 0x48 */
    int assets;                                /* 0x4C */
    int assetHistory[13];                      /* 0x50 */
    int pieceState;                            /* 0x84 */
} SavePlayer;                                  /* size 0x88 */

typedef struct SaveStore
{
    signed char owner;           /* 0x00 */
    signed char storeType;     /* 0x01 */
    unsigned char pad02;
    unsigned char tempClosed;    /* 0x03 */
    signed char numGoods;        /* 0x04 */
    signed char maxGoods;        /* 0x05 */
    unsigned char priceChanged;  /* 0x06 */
    unsigned char pad07;
    short pricePercent;          /* 0x08 */
    unsigned short priceUntil;   /* 0x0A */
    unsigned short closedUntil;  /* 0x0C */
    unsigned char goods[8][2];   /* 0x0E StoreGoods.active, .level */
} SaveStore;                     /* size 0x1E */

typedef struct SaveStoreType
{
    unsigned char available;     /* 0x00 */
    unsigned char pad01[3];
    struct Store *store;         /* 0x04 StoreType.store */
} SaveStoreType;                 /* size 8 */

/* Story progress (g_progress, 0x800FE180), kept in the save file's system block. */
typedef struct Progress
{
    signed char storyStage;      /* 0x00 how far story mode got: the next stage to play, 10 when all are cleared */
    signed char freeStages;      /* 0x01 stages free play offers: 8, then 9 and 10 once story mode got there */
    unsigned char charOpen[14];  /* 0x02 characters that can be picked: 0-10 always; 11 and 12 once story mode
                                    got to stage 9 / 10; 13 after 15 stage clears with story mode finished */
    unsigned char unused10;      /* 0x10 loaded and saved, not used otherwise */
    signed char clears;          /* 0x11 story stage clears, counted up to 15 */
} Progress;                      /* size 0x12 */

/* The system block of the save file (file offset 0x200, 0x80 bytes written). g_cardBuf points at
 * it inside the save buffer, which mirrors the file. */
typedef struct CardSystem
{
    unsigned char slotUsed[4];   /* 0x00 game slots that hold a save */
    unsigned char eventScenes;   /* 0x04 the options: g_optEventScenes */
    unsigned char mono;          /* 0x05 g_optMono */
    unsigned char skipCpuTalk;   /* 0x06 g_optSkipCpuTalk */
    unsigned char cpuFast;       /* 0x07 g_optCpuFast */
    Progress progress;           /* 0x08 */
    unsigned char pad1A[2];
    unsigned int checksum;       /* 0x1C Save_Checksum of the first 0x1A bytes */
} CardSystem;                    /* size 0x20 */

typedef struct SaveImage
{
    char title[13];              /* 0x000 Player.title of the player who saved */
    unsigned char boardStage;    /* 0x00D */
    unsigned char boardPlayers;  /* 0x00E g_numPlayers */
    unsigned char numActivePlayers; /* 0x00F */
    unsigned char curPlayer;     /* 0x010 */
    unsigned char pad011[3];
    unsigned char playMode, unused01, numPlayers, numHumans, numCpus, year, month, stage;  /* 0x014 GameConfig */
    unsigned char bankruptLimit, startGoods, maxBuy, pad01F;                /* 0x01C */
    int monthsElapsed;           /* 0x020 */
    int startMoney;              /* 0x024 */
    int goalMoney;               /* 0x028 */
    unsigned char turnOrder[6];  /* 0x02C */
    unsigned char pad032[2];
    SavePlayer players[6];       /* 0x034 */
    SaveStore stores[MAX_STORES];          /* 0x364 */
    SaveStoreType storeTypes[NUM_STORE_TYPES]; /* 0x814 */
    unsigned int checksum;       /* 0x974 Save_Checksum of the bytes before it */
} SaveImage;                     /* size 0x978 */

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Card_LoadSystemTask */
typedef struct CardLoadSystemWork
{
    short retries;   /* 0x00 */
    unsigned char pad02[98];
} CardLoadSystemWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char k_saveTitle[17];      /* 0x80010564 save file title, Shift-JIS "TOP SHOP" */
extern u32 k_saveSlotOffsets[4];  /* 0x800A8B08 */
extern u16 k_saveIconClut[16];    /* 0x800A8B18 save file icon palette */
extern u8 k_saveIconFrame0[128];  /* 0x800A8B38 save file icon, 16x16 4bpp, 3 frames */
extern u8 k_saveIconFrame1[128];  /* 0x800A8BB8 */
extern u8 k_saveIconFrame2[128];  /* 0x800A8C38 */
extern char k_saveMagic[2];       /* 0x800B1F0C */
extern char k_saveIconFlag;       /* 0x800B1F0E */
extern char g_saveConfirmed;      /* 0x800B2540 */
extern CardSystem *g_cardBuf;     /* 0x800B2544 the system block in the save buffer (g_saveBuf + 0x200) */
extern char g_cardTaskState;      /* 0x800B2548 */
extern u8 *g_saveBuf;             /* 0x800B254C */
extern short g_cardBufBlock;      /* 0x800B2550 */
extern Progress g_progress;       /* 0x800FE180 (Progress, include/save.h) */
extern u8 *g_saveSlots[4];        /* 0x800FE198 */
extern SaveImage g_saveImage;     /* 0x800FE1A8 */

/* ---- Functions ---- */

/* src/save.c */
u32 Card_SaveTask(u16 *step);                             /* 0x80032954 */
u32 Card_ReadSavesTask(u16 *step);                        /* 0x80033724 */
u32 Card_LoadSystemTask(u16 *step, CardLoadSystemWork *work); /* 0x80033CEC */
void Save_DescribeCurrent(u8 *out);                       /* 0x800342F8 */
void Save_DescribeSlot(u8 *slot, char *out);              /* 0x80034524 */
int Save_Pack(u8 slot, char player);                      /* 0x80034774 */
int Save_WriteSlot(short slot);                           /* 0x80034C70 */
int Save_CreateFile(void);                                /* 0x80034FC4 */
int Save_Unpack(void);                                    /* 0x80035108 */
u32 Card_SaveProgressTask(short *step);                   /* 0x8003558C */
void Progress_Reset(void);                                /* 0x800365E0 */
void Save_FreeBuffer(void);                               /* 0x80036638 */
void Save_GetSlotInfo(SaveImage **image, u8 *stage, int *year, int *month, int *playMode, short slot); /* 0x8003665C */
u32 Save_Checksum(u8 *data, int len);                     /* 0x800366D0 */
u32 Save_SelectSlot(char slot);                           /* 0x80036720 */

#endif /* SAVE_H */
