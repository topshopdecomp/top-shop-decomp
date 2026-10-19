/*
 * gameops.h - the script opcodes that act on the board game
 *
 * Money, stores, the store dialogs, moves, results.
 * The declarations of src/gameops.c: its constants and its functions.
 * See docs/script.md.
 */

#ifndef GAMEOPS_H
#define GAMEOPS_H

#include "common.h"
#include "script.h"

/* Selections of players in script operands; any other value is a player index */
#define SCRIPT_ALL_PLAYERS    (-1)   /* the players still in */
#define SCRIPT_CUR_PLAYER     (-2)
#define SCRIPT_OTHER_PLAYERS  (-3)   /* the players still in but the current one */

/* ---- Functions ---- */

/* src/gameops.c */
u32 Script_OpPlayerEarn(void);                     /* 0x80081C5C */
u32 Script_OpMoneyPercent(void);                   /* 0x80081E04 */
u32 Script_OpCopyMoney(void);                      /* 0x80082030 */
u32 Script_OpStoresSetGoods(void);                 /* 0x80082244 */
u32 Script_OpStoresGoodsLevel(void);               /* 0x800824E8 */
u32 Script_OpStoresSetPrice(void);                 /* 0x80082748 */
u32 Script_OpStoresCloseFor(void);                 /* 0x80082934 */
u32 Script_OpBuyLot(ScriptScratch *scratch);       /* 0x80082B94 */
u32 Script_OpSellStore(ScriptScratch *scratch);    /* 0x80082D50 */
u32 Script_OpStoresClose(ScriptScratch *scratch);  /* 0x80082F4C */
u32 Script_OpPlayerTurnBack(void);                 /* 0x800834D0 */
u32 Script_OpNoElevator(void);                     /* 0x800835B8 */
u32 Script_OpSetShoes(void);                       /* 0x80083854 */
u32 Script_OpCountStores(void);                    /* 0x80085768 */
u32 Script_OpGetStore(void);                       /* 0x800857F0 */
u32 Script_OpNop182(void);                         /* 0x800858B8 */
u32 Script_OpNop183(void);                         /* 0x800858D8 */
u32 Script_OpNop186(void);                         /* 0x800858F8 */
u32 Script_OpNop187(void);                         /* 0x80085918 */
u32 Script_OpResult1(void);                        /* 0x80085938 */
u32 Script_OpResult2(void);                        /* 0x80085964 */
u32 Script_OpAdvanceMonths(void);                  /* 0x80085990 */
u32 Script_OpSetSteps(void);                       /* 0x800859DC */

#endif /* GAMEOPS_H */
