/*
 * common.h - basic types and helpers
 *
 * The integer types (s8 ... u64), NULL, true / false, and small helpers for byte access
 * (MAKE_U16, SUB21, ...) that every C file uses.
 */
#ifndef COMMON_H
#define COMMON_H

typedef signed char        s8;
typedef unsigned char      u8;
typedef signed short       s16;
typedef unsigned short     u16;
typedef signed int         s32;
typedef unsigned int       u32;
typedef signed long long   s64;
typedef unsigned long long u64;

/* Ghidra's "code": something called through a pointer */
typedef int code();

#ifndef NULL
#define NULL ((void *)0)
#endif
#define true  1
#define false 0

/* A 16-bit value from two bytes, the high one first (big-endian data: the event scripts, the
 * pads' replies) */
#define MAKE_U16(hi, lo) ((u16)(((u32)(u8)(hi) << 8) | (u8)(lo)))
#define SUB21(x, n)      ((u8)((u16)(x) >> ((n) * 8)))

/* Halves of an unaligned 32-bit load/store (MIPS lwl/lwr, swl/swr, little-endian),
 * which GCC emits when copying byte-aligned data. Ghidra writes each half as a masked
 * shift of the aligned word; these give the same result without its shifts by 32.
 * u = address & 3 of the instruction's own address; w = the aligned word in memory,
 * v = the register value, in = the register before an lwl. */
static __inline__ u32 Mips_SwlMerge(u32 w, u32 v, u32 u)   /* new memory word */
{
    return u == 3 ? v : (w & (0xFFFFFFFFU << ((u + 1) * 8))) | (v >> ((3 - u) * 8));
}
static __inline__ u32 Mips_SwrMerge(u32 w, u32 v, u32 u)
{
    return u == 0 ? v : (w & (0xFFFFFFFFU >> ((4 - u) * 8))) | (v << (u * 8));
}
static __inline__ u32 Mips_LwlMerge(u32 in, u32 w, u32 u)  /* new register value */
{
    return u == 3 ? w : (in & (0xFFFFFFFFU >> ((u + 1) * 8))) | (w << ((3 - u) * 8));
}
static __inline__ u32 Mips_LwrMerge(u32 in, u32 w, u32 u)
{
    return u == 0 ? w : (in & (0xFFFFFFFFU << ((4 - u) * 8))) | (w >> (u * 8));
}

#endif /* COMMON_H */
