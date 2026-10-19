/*
 * cpu.h - the CPU players' decisions
 *
 * Routes, stores, prices, deliveries, the elevator, the points exchange and the targets of event
 * cards.
 * The declarations of src/cpu.c: its types, its globals and its functions.
 * See docs/cpu.md.
 */

#ifndef CPU_H
#define CPU_H

#include "common.h"

/* Per character: when a CPU player uses the points exchange (k_cpuExchange, 0x800ADBBC). */
typedef struct CpuExchangePref
{
    unsigned char chance;        /* 0x00 percent */
    unsigned char pad01[3];
    int moneyNeeded;             /* 0x04 */
    unsigned char *cards;        /* 0x08 event cards it buys */
} CpuExchangePref;               /* size 0x0C */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 k_cpuBankDistance[16];           /* 0x800AD9C8 */
extern u8 k_cpuOpenChance[14][4][3];       /* 0x800AD9D8 */
extern u8 k_cpuPriceRange[14][2];          /* 0x800ADA80 */
extern u8 k_cpuOfferAccept[14][4];         /* 0x800ADA9C */
extern u8 k_cpuDeliveryRatio[14][3];       /* 0x800ADAD4 per character: % of the money spent on goods (bank near, far, farther) */
extern u8 k_cpuDeliveryChance[14][4];      /* 0x800ADB00 per character: cumulative % chances of the 4 Cpu_ChooseDelivery strategies */
extern CpuExchangePref k_cpuExchange[14];  /* 0x800ADBBC */
extern int g_cpuDebugVals[16];             /* 0x80117C90 */

/* ---- Functions ---- */

/* src/cpu.c */
int Cpu_ChooseRoute(u16 player, u8 steps);                /* 0x80085CB4 */
int Cpu_ChooseStoreToOpen(u32 player, u32 store, u32 list, short *prices, int count, int ownerGain, char owner); /* 0x80086934 */
u32 Cpu_ChooseStoreToSell(u16 player, char cancel, char percent); /* 0x80087024 */
int Cpu_RefuseStoreOffer(u32 player, u32 store, int price, int ownerGain, char owner); /* 0x800874A0 */
int Cpu_ChooseItemToBuy(u16 player, u16 store);           /* 0x80087870 */
int Cpu_ChooseStoreToDeliver(u32 player);                 /* 0x80087E70 */
int Cpu_ChooseDelivery(u32 player, u32 store, u8 *items); /* 0x80088570 */
int Cpu_ElevatorChoice(u16 player, u32 elevator);         /* 0x80088FFC */
int Cpu_ChooseExchange(u32 player, u8 *target);           /* 0x80089360 */
u32 Cpu_ExchangeForAssets(u16 player, int shortfall, u8 *target); /* 0x80089560 */
u32 Cpu_ExchangeForMoney(u32 player, int needed);         /* 0x80089910 */
u32 Cpu_EventTarget(u8 player, u32 event);                /* 0x80089B84 */
u32 CpuTarget_HumanUnused(u32 player, u8 *cands, char n); /* 0x80089F40 */
u32 CpuTarget_StopDelivery(u32 player, u8 *cands, u8 n);  /* 0x8008A0AC */
u32 CpuTarget_StopNewStores(u32 player, u8 *cands, u8 n); /* 0x8008A368 */
u32 CpuTarget_SlowDice(u32 player, u8 *cands, u8 n);      /* 0x8008A624 */
u32 CpuTarget_CaptureStore(u16 player, u8 *cands, u8 n);  /* 0x8008A8C8 */
u32 CpuTarget_DiceE(u32 player, u8 *cands, char n);       /* 0x8008AD5C */
int CpuTarget_SwitchPlaces(u32 player, char *cands, char n); /* 0x8008AED4 */
int CpuTarget_MoneyThreeQuarters(u32 player, char *cands, char n); /* 0x8008B0F8 */
int CpuTarget_MoneyHalf(u32 player, char *cands, char n); /* 0x8008B2D0 */
int CpuTarget_MoneyToOne(u32 player, char *cands, char n); /* 0x8008B4A8 */
int CpuTarget_TakeHalfMoney(u32 player, char *cands, char n); /* 0x8008B680 */
int CpuTarget_RivalsMoney(u32 player, char *cands, char n); /* 0x8008B848 */
int CpuTarget_HalvePoints(u32 player, char *cands, char n); /* 0x8008BA10 */
u32 CpuTarget_DeliverFree(u16 player);                    /* 0x8008BB68 */
u32 CpuTarget_RandomUnused(u32 player, int cands);        /* 0x8008CE38 */
u32 CpuTarget_LeastUnused(u32 player, u8 *cands, char n); /* 0x8008CEB0 */
u32 CpuTarget_RichestUnused(u32 player, u8 *cands, char n); /* 0x8008D00C */
u32 CpuTarget_DeliverCheap(short player);                 /* 0x8008D144 */
u32 CpuTarget_DeliverCostly(short player);                /* 0x8008D1D4 */
int CpuTarget_Reverse(u32 player, char *cands, char n);   /* 0x8008D264 */
int CpuTarget_CloseStore(u32 player, char *cands, char n); /* 0x8008D350 */
int CpuTarget_CloseStoreLv2(u32 player, char *cands, char n); /* 0x8008D4D0 */
int CpuTarget_CloseStoreLv3(u32 player, char *cands, char n); /* 0x8008D5F8 */
int CpuTarget_CloseStoreLv4(u32 player, char *cands, char n); /* 0x8008D720 */
u32 CpuTarget_ItemsDouble(short player);                  /* 0x8008D81C */
u32 CpuTarget_CutByStores(u32 player);                    /* 0x8008D89C */
u32 Cpu_WantsMoneyShuffle(u32 player);                    /* 0x8008D904 */
u32 Cpu_WantsHalveAll(u32 player);                        /* 0x8008D9A0 */
u32 CpuTarget_DeliverOwn(short player);                   /* 0x8008DA50 */
u32 CpuTarget_DeliverExpensive(short player);             /* 0x8008DA8C */
u32 CpuTarget_OutOfStock4(void);                          /* 0x8008DB08 */
u32 Cpu_WantsEmptyStores(void);                           /* 0x8008DBC0 */

#endif /* CPU_H */
