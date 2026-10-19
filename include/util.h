/*
 * util.h - small helpers
 *
 * Fixed-point math, angles, sine-wave tables, memory copies, packed tables, random permutations.
 * The declarations of src/util.c: its globals and its functions.
 * See docs/system.md.
 */

#ifndef UTIL_H
#define UTIL_H

#include "common.h"

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern unsigned int g_bitMask32[32];  /* 0x800A56D0 */
extern int g_tableBase;               /* 0x800B2518 */
extern u16 g_tableEntrySize;          /* 0x800B251C */
extern u16 g_tableOffsetBytes;        /* 0x800B251E */
extern u16 g_tableValueBytes;         /* 0x800B2520 */

/* ---- Functions ---- */

/* src/util.c */
u32  Util_ToBcd(u32 value);                               /* 0x8001AABC */
int  Math_MulFixed12(int a, int b);                       /* 0x8001AB24 */
void Mem_Copy(void *dst, const void *src, int n);         /* 0x8001AB8C */
void Mem_CopySwap16(void *dst, const void *src, int nbytes); /* 0x8001ABBC */
void Table_Init(int base, u16 offsetBytes, u16 valueBytes); /* 0x8002FE44 */
int Table_GetPtr(u32 i);                                  /* 0x8002FE68 */
u32 Table_GetValue(u32 i);                                /* 0x8002FEC0 */
void Util_RandomPermutation(u8 n, u8 *out);               /* 0x8007B0C8 */
int Util_DecimalTier(u32 value, u16 tier);                /* 0x8007B988 */
u32 Util_True(void);                                      /* 0x8007BAF0 */
int Math_Angle(int *from, int *to);                       /* 0x8007BAF8 */
int Math_AngleDist(int *from, int *to, int *dist);        /* 0x8007BB60 */
void Math_Step(short angle, u32 dist, int *pos);          /* 0x8007BCB0 */
void Wave_FillShort(short *out, int period, int step, int div, int amp, int phase, short offset); /* 0x8007BD30 */
void Wave_FillInt(int *out, int period, int step, int div, int amp, int phase, int offset, short tilt); /* 0x8007BEA0 */

#endif /* UTIL_H */
