/*
 * lzss.c - the LZSS decompressor
 *
 * Classic Okumura LZSS: 4096-byte ring buffer, initial write position 0xFEE, ring zero-filled (not space-filled). Stream layout:
 *   u32  total size in bytes, including this word (the input end marker)
 *   u8   flag byte; then 8 items, LSB first:
 *          bit 1 -> 1 literal byte
 *          bit 0 -> 2 bytes b0 b1: offset = b0 | (b1 & 0xF0) << 4,
 *                   length = (b1 & 0x0F) + 3, copied from the ring
 *   ...repeat flag byte + items until the input end.
 * Used for packed images, the event card pictures, the character talk scripts and the last entry of some .J archives (e.g. LoadJWithTim_Task).
 */
#include "lzss.h"

#define LZSS_N 4096
#define LZSS_F 18

/* g_lzssRing[LZSS_N] (0x800B2890): declared in fileio.h, defined in bss.c */

/* 0x8001A8A8 */
void Lzss_Decompress(int *src, unsigned char *dst)
{
    unsigned char *in = (unsigned char *)src + 5;
    unsigned char *end = (unsigned char *)src + src[0];
    unsigned int r = LZSS_N - LZSS_F;
    unsigned int flags;
    unsigned char c, c2, b;
    int i, k, ofs, len;

    for (i = 0; i < LZSS_N - LZSS_F; i++)
        g_lzssRing[i] = 0;

    c = ((unsigned char *)src)[4];
    for (;;)
    {
        if (end < in)
            return;
        /* the 0xFF00 sentinel bits shift down to mark when 8 items are done */
        for (flags = c | 0xFF00; flags & 0x100; flags >>= 1)
        {
            if (flags & 1)
            {
                c = *in++;
                if (end < in)
                    return;
                *dst++ = c;
                g_lzssRing[r] = c;
                r = (r + 1) & (LZSS_N - 1);
            }
            else
            {
                c = in[0];
                c2 = in[1];
                in += 2;
                if (end < in)
                    return;
                ofs = c | ((c2 & 0xF0) << 4);
                len = (c2 & 0x0F) + 2;
                for (k = 0; k <= len; k++)
                {
                    b = g_lzssRing[(ofs + k) & (LZSS_N - 1)];
                    *dst++ = b;
                    g_lzssRing[r] = b;
                    r = (r + 1) & (LZSS_N - 1);
                }
            }
        }
        c = *in++;
    }
}
