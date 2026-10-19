/*
 * lzss.h - the LZSS decompressor
 *
 * The declarations of src/lzss.c: its globals and its functions.
 * See docs/fileio.md.
 */

#ifndef LZSS_H
#define LZSS_H

#include "common.h"

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern unsigned char g_lzssRing[0x1000];  /* 0x800B2890 */

/* ---- Functions ---- */

/* src/lzss.c */
void Lzss_Decompress(int *src, unsigned char *dst);  /* 0x8001A8A8 */

#endif /* LZSS_H */
