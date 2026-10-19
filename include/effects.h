/*
 * effects.h - the event cards' effects on the stores and the players
 *
 * The declarations of src/effects.c: the work areas of its tasks, its globals and its functions.
 * See docs/board.md.
 */

#ifndef EFFECTS_H
#define EFFECTS_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Effect_CloseChain */
typedef struct EffectCloseChainWork
{
    signed char first;   /* 0x00 */
    unsigned char pad01[1];
    unsigned char width;   /* 0x02 */
    unsigned char x;   /* 0x03 */
    unsigned char floor;   /* 0x04 */
    unsigned char pad05[95];
} EffectCloseChainWork;

/* work area of Effect_OpenStoreNewSquare */
typedef struct EffectOpenStoreNewSquareWork
{
    signed char store;   /* 0x00 */
    unsigned char square;   /* 0x01 */
    signed char storeType;   /* 0x02 */
    unsigned char x;   /* 0x03 */
    unsigned char floor;   /* 0x04 */
    unsigned char pad05[95];
} EffectOpenStoreNewSquareWork;

/* work area of Effect_TakeOverChain */
typedef struct EffectTakeOverChainWork
{
    signed char store;   /* 0x00 */
    unsigned char square;   /* 0x01 */
    signed char storeType;   /* 0x02 */
    unsigned char x;   /* 0x03 */
    unsigned char floor;   /* 0x04 */
    unsigned char pad05[95];
} EffectTakeOverChainWork;

/* work area of Effect_StealChain */
typedef struct EffectStealChainWork
{
    signed char store;   /* 0x00 */
    unsigned char pad01[2];
    unsigned char width;   /* 0x03 */
    unsigned char x;   /* 0x04 */
    unsigned char floor;   /* 0x05 */
    unsigned char pad06[94];
} EffectStealChainWork;

/* work area of Effect_Delivery */
typedef struct EffectDeliveryWork
{
    unsigned char isCpu;   /* 0x00 */
    unsigned char pad01[99];
} EffectDeliveryWork;

/* work area of Effect_RebuildClosed */
typedef struct EffectRebuildClosedWork
{
    signed char store;   /* 0x00 */
    unsigned char squareId;   /* 0x01 */
    signed char storeType;   /* 0x02 */
    unsigned char x;   /* 0x03 */
    unsigned char floor;   /* 0x04 */
    unsigned char pad05[95];
} EffectRebuildClosedWork;

/* work area of Effect_TakeOverClosed */
typedef struct EffectTakeOverClosedWork
{
    signed char k;   /* 0x00 */
    signed char store;   /* 0x01 */
    signed char squareId;   /* 0x02 */
    signed char storeType;   /* 0x03 */
    signed char x;   /* 0x04 */
    signed char floor;   /* 0x05 */
    unsigned char pad06[94];
} EffectTakeOverClosedWork;

/* work area of Effect_DemolishClosed */
typedef struct EffectDemolishClosedWork
{
    signed char k;   /* 0x00 */
    signed char store;   /* 0x01 */
    signed char square;   /* 0x02 */
    unsigned char pad03[1];
    signed char x;   /* 0x04 */
    signed char floor;   /* 0x05 */
    unsigned char pad06[94];
} EffectDemolishClosedWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u32 g_debugEffectStep;       /* 0x80117D04 */
extern u32 g_debugEffectStore;      /* 0x80117D08 */
extern u32 g_debugEffectStoreType;  /* 0x80117D0C */

/* ---- Functions ---- */

/* src/effects.c */
u32 Effect_CloseChain(short *step, EffectCloseChainWork *work, char who, char length); /* 0x80070974 */
u32 Effect_OpenStoreNewSquare(short *step, EffectOpenStoreNewSquareWork *work, char payer); /* 0x80070C88 */
u32 Effect_TakeOverChain(short *step, EffectTakeOverChainWork *work); /* 0x80071098 */
u32 Effect_StealChain(short *step, EffectStealChainWork *work, char who); /* 0x80071508 */
u32 Effect_Delivery(short *step, EffectDeliveryWork *work, char event, char who); /* 0x800717EC */
u32 Effect_RebuildClosed(short *step, EffectRebuildClosedWork *work, char who); /* 0x80071B58 */
u32 Effect_TakeOverClosed(short *step, EffectTakeOverClosedWork *work); /* 0x80071E7C */
u32 Effect_DemolishClosed(short *step, EffectDemolishClosedWork *work); /* 0x800721A0 */
u32 Effect_ShuffleMoney(void);                            /* 0x8007243C */
u32 Effect_SwitchPlaces(short *step, u32 work, char who); /* 0x800726FC */
u32 Effect_BuyVacant(short *step);                        /* 0x800729A4 */

#endif /* EFFECTS_H */
