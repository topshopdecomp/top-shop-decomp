/*
 * sound.h - music and sound effects
 *
 * Music (SEQ), sound effects, the sound set-up.
 * The declarations of src/sound.c: its types and constants, its globals and its functions.
 * See docs/sound.md.
 */

#ifndef SOUND_H
#define SOUND_H

#include "common.h"
#include "psyq.h"
#include "fileid.h"

/* Sound effects play as SEQ sequences in 5 round-robin slots (g_seSlots). */
#define SE_SLOTS  5

/* Sound effect numbers (Se_Play) used all over the menus */
#define SE_CURSOR  0x17   /* a cursor moves */
#define SE_OK      0x18   /* a choice is taken */
#define SE_CANCEL  0x19   /* back, or a window closed */
#define BGM_SEQS  5

typedef struct SeSlot
{
    short seq;       /* 0x00 SsSeqOpen access number */
    short keep;      /* 0x02 1 = not taken over while another slot is free (Se_Play's 4th argument) */
    short id;        /* 0x04 sound effect playing, -1 = none */
} SeSlot;            /* size 6 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_bgmCur;               /* 0x800B24E0 */
extern short g_seNext;               /* 0x800B24E2 */
extern char g_bgmBufCur;             /* 0x800B24E4 */
extern int *g_seqArchive;            /* 0x800B24E8 */
extern void *g_bgmBufs[2];           /* 0x800B24F0 */
extern u_long **g_seqTable;          /* 0x800B24F8 the sequence archive's entries (after its count); songs 0-22 LZSS-packed, the sound effects not */
extern short g_vabId;                /* 0x800B2500 */
extern char g_ssSeqTable[32 * 176];  /* 0x800D4A38 libsnd SEQ table (SsSetTableSize: 32 x SS_SEQ_TABSIZ) */
extern SeSlot g_seSlots[5];          /* 0x800D6038 */
extern u8 g_vabHeader[0x28000];      /* 0x800D6058 sound buffer: the VAB header, then the sequence archive */
extern short g_bgmSeqs[5];           /* 0x800FE058 */

/* ---- Functions ---- */

/* src/sound.c */
void Snd_Init(u32 vabFile, FileId seqFile);               /* 0x8002B418 */
void Snd_LoadVab(FileId file, char setSeqBuf);            /* 0x8002B5E0 */
void Bgm_Play(short song, short mode, short volL, short volR); /* 0x8002B738 */
int Se_Play(u16 id, short volL, short volR, u16 keep);    /* 0x8002B8D8 */
void Snd_StopAll(void);                                   /* 0x8002BC08 */
void Snd_RelocateTable(int *table);                       /* 0x8002BCDC */
void Bgm_Stop(void);                                      /* 0x8002BD14 */
void Se_StopSlot(short slot);                             /* 0x8002BD70 */
void Se_Stop(short id);                                   /* 0x8002BDC8 */
void Snd_SetMono(char mono);                              /* 0x8002BE64 */
int Snd_Unused(void);                                     /* 0x8002BED0 */

#endif /* SOUND_H */
