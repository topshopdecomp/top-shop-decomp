/*
 * util.c - small helpers
 *
 * Fixed-point arithmetic, angles and stepping along an angle, sine-wave tables, memory copies
 * (with byte swapping for the scripts' big-endian words), the packed number tables, random
 * permutations, BCD.
 */
#include "common.h"
#include "topshop.h"

/* Packed BCD of the low 8 decimal digits of value. (0x8001AABC) */
u32 Util_ToBcd(u32 value)
{
    u32 div = 10000000;
    u32 shift = 28;
    u32 bcd = 0;
    u32 digit;

    do
    {
        digit = value / div;
        value = value % div;
        div = (int)div / 10;
        bcd += digit << (shift & 0x1f);
        shift -= 4;
    } while (div != 0);
    return bcd;
}

/* (a * b) >> 12 for 20.12 fixed point, split into integer and fraction parts so the
 * product does not overflow; the fraction product is rounded. (0x8001AB24) */
int Math_MulFixed12(int a, int b)
{
    int ai = a / 4096;
    int bi = b / 4096;
    int af = a - ai * 4096;
    int bf = b - bi * 4096;

    return (af * bf + 0x800) / 4096 + ai * bf + a * bi;
}

/* 0x8001AB8C */
void Mem_Copy(void *dst, const void *src, int n)
{
    u8 *d = dst;
    const u8 *s = src;
    int i;

    for (i = 0; i < n; i++)
        d[i] = s[i];
}

/* Copy nbytes swapping each byte pair (16-bit endian swap). (0x8001ABBC) */
void Mem_CopySwap16(void *dst, const void *src, int nbytes)
{
    u8 *d = dst;
    const u8 *s = src;
    int i;

    for (i = 0; i < nbytes; i += 2)
    {
        d[i + 1] = s[i];
        d[i] = s[i + 1];
    }
}

/* Select a packed table: entries of offsetBytes (big-endian offset from base) + valueBytes
 * (0x8002FE44) */
/* 0x8002FE44 */
void Table_Init(int base, u16 offsetBytes, u16 valueBytes)
{
    g_tableOffsetBytes = offsetBytes & 0xff;
    g_tableBase = base;
    g_tableValueBytes = valueBytes & 0xff;
    g_tableEntrySize = (offsetBytes & 0xff) + (valueBytes & 0xff);
}

/* A big-endian number of `bytes` bytes */
static u32 Table_ReadNumber(u8 *at, u16 bytes)
{
    u32 value;
    u16 k;

    value = 0;
    for (k = 0; k < bytes; k++)
    {
        value = value << 8 | at[k];
    }
    return value;
}

/* base + the offset stored in entry i of the packed table (0x8002FE68) */
/* 0x8002FE68 */
int Table_GetPtr(u32 i)
{
    u8 *entry = (u8 *)(g_tableBase + (i & 0xffff) * g_tableEntrySize);

    return Table_ReadNumber(entry,g_tableOffsetBytes) + g_tableBase;
}

/* The big-endian value stored in entry i of the packed table (0x8002FEC0) */
/* 0x8002FEC0 */
u32 Table_GetValue(u32 i)
{
    u8 *entry = (u8 *)(g_tableBase + (i & 0xffff) * g_tableEntrySize);

    return Table_ReadNumber(entry + g_tableOffsetBytes,g_tableValueBytes);
}

/* A random permutation of 0..n-1 into the bytes at out (factorial number system, table 0x800186A0)
 * (0x8007B0C8) */
/* 0x8007B0C8 */
void Util_RandomPermutation(u8 n, u8 *out)
{
    short factorial[7] = {1, 1, 2, 6, 24, 120, 720};   /* the table at 0x800186A0 */
    char used[8];
    int code, digit;
    int i, j, k;

    for (i = 0; i < n; i++)
    {
        used[i] = 0;
    }
    /* one of the n! orders, its digits picking each element among the unused ones */
    code = rand() % factorial[n];
    for (j = 0; j < n; j++)
    {
        digit = code / factorial[n - j - 1];
        for (k = 0; k < n; k++)
        {
            if (used[k] == 0)
            {
                if ((u8)digit == 0) break;
                digit--;
            }
        }
        out[j] = k;
        used[k] = 1;
        code %= factorial[n - j - 1];
    }
}

/* value * 10 + {0, 1, 3, 6}[tier] (tier 0..3), 0 for other tiers (0x8007B988) */
/* 0x8007B988 */
int Util_DecimalTier(u32 value, u16 tier)
{
    short result;

    if (tier == 1)
    {
        result = (short)value * 10 + 1;
    }
    else
    {
        if (tier < 2)
        {
            if (tier != 0)
            {
                return 0;
            }
            return (int)((value & 0xffff) * 0xa0000) >> 0x10;
        }
        if (tier == 2)
        {
            result = (short)value * 10 + 3;
        }
        else
        {
            if (tier != 3)
            {
                return 0;
            }
            result = (short)value * 10 + 6;
        }
    }
    return result;
}

/* Returns 1 (0x8007BAF0) */
/* 0x8007BAF0 */
u32 Util_True(void)
{
    return 1;
}

/* Direction from one point to another, 4096 = 360 degrees (0 = +x, 0x400 = +y) (0x8007BAF8) */
/* 0x8007BAF8 */
int Math_Angle(int *from, int *to)
{
    int dx = to[0] - from[0];
    int dy = to[1] - from[1];

    if (dx == 0)
    {
        if (dy == 0)
        {
            return 0;
        }
        return (dy > 0) ? 0x400 : 0xc00;
    }
    if (dy == 0)
    {
        return (dx < 0) ? 0x800 : 0;
    }
    return (short)ratan2(dy,dx);
}

/* Math_Angle, and the distance in *dist: |dx| cos a + |dy| sin a, a the angle folded into the
 * first quadrant (0x8007BB60) */
/* 0x8007BB60 */
int Math_AngleDist(int *from, int *to, int *dist)
{
    int dx;
    int dy;
    int quadrantAngle;
    short angle;

    dx = to[0] - from[0];
    dy = to[1] - from[1];
    if (dx == 0)
    {
        if (dy == 0)
        {
            angle = 0;
            *dist = 0;
        }
        else
        {
            angle = (dy > 0) ? 0x400 : 0xc00;   /* straight down / up */
            *dist = (dy < 0) ? -dy : dy;
        }
    }
    else if (dy == 0)
    {
        angle = (dx < 0) ? 0x800 : 0;
        *dist = (dx < 0) ? -dx : dx;
    }
    else
    {
        angle = ratan2(dy,dx);
        if (dy < 0)
        {
            dy = -dy;
        }
        if (dx < 0)
        {
            dx = -dx;
        }
        quadrantAngle = ratan2(dy,dx) % 0x400;
        *dist = Math_MulFixed12(dx,rcos((short)quadrantAngle)) + Math_MulFixed12(dy,rcos((short)(0x400 - quadrantAngle)));
    }
    return angle;
}

/* Move pos {x, y} by dist (4.12 units) in direction angle (0x8007BCB0) */
/* 0x8007BCB0 */
void Math_Step(short angle, u32 dist, int *pos)
{
    int cs;
    int prod;
    int sn;

    cs = rcos(angle);
    prod = Math_MulFixed12(cs,dist);
    *pos += prod;
    sn = rsin(angle);
    prod = Math_MulFixed12(sn,dist);
    pos[1] += prod;
}

/* Fill out with period / step samples of a sine wave (amplitude amp, plus offset) (0x8007BD30) */
/* 0x8007BD30 */
void Wave_FillShort(short *out, int period, int step, int div, int amp, int phase, short offset)
{
    int i;

    for (i = 0; i < period / step; i++)
    {
        *out++ = rsin((phase % period << 11) / div) * amp / 0x1000 + offset;
        phase += step;
    }
}

/* Fill out with period / step samples (20.12) of a sine wave plus a slope sin(tilt) per sample
 * (0x8007BEA0) */
/* 0x8007BEA0 */
void Wave_FillInt(int *out, int period, int step, int div, int amp, int phase, int offset, short tilt)
{
    int i;
    int angle;

    for (i = 0; i < period / step; i++)
    {
        angle = (phase % period << 11) / div;
        phase += step;
        *out++ = rsin(angle) * amp + offset * 0x1000 + i * rsin(tilt);
    }
}
