/*
 * sound.c - music and sound effects
 *
 * Setting up the sound (the VAB sample banks), playing and stopping the music (SEQ sequences)
 * and the sound effects in their voice slots, and mono output.
 */
#include "common.h"
#include "topshop.h"

/* Set up libsnd (tables, reverb, volumes), load the VAB (Snd_LoadVab) and the sequence archive,
 * open the 5 BGM and 5 sound-effect sequences on entry 0x1e (0x8002B418) */
/* 0x8002B418 */
void Snd_Init(u32 vabFile, FileId seqFile)
{
    DslATV vol[2];
    int i;

    SsSetTableSize(g_ssSeqTable,0x20,1);
    SsSetTickMode(2);
    Snd_LoadVab(vabFile,1);
    SpuInitHot();
    SsSetSerialAttr(0,0,1);
    SsSetSerialVol(0,0x7f,0x7f);
    SsSetMVol(0x7f,0x7f);
    vol[0].val0 = 127;
    vol[0].val1 = 0;
    vol[0].val2 = 127;
    vol[0].val3 = 0;
    DsMix(vol);
    SsUtSetReverbType(1);
    SsUtSetReverbDepth(0x1e,0x1e);
    SsUtReverbOn();
    SsSetRVol(0x32,0x32);
    /* the sequence archive after the VAB header; two buffers for unpacked songs after it */
    File_ReadSync(seqFile,g_seqArchive,FILE_WHOLE);
    Snd_RelocateTable(g_seqArchive);
    g_seqTable = (u_long **)(g_seqArchive + 1);
    g_bgmBufCur = 0;
    g_bgmBufs[0] = (u8 *)g_seqArchive + File_GetSectors(seqFile) * 0x800;
    g_bgmBufs[1] = (u8 *)g_bgmBufs[0] + 0x6800;
    for (i = 0; i < BGM_SEQS; i++)
    {
        g_bgmSeqs[i] = SsSeqOpen(g_seqTable[0x1e],g_vabId);
    }
    g_bgmCur = 0;
    for (i = 0; i < SE_SLOTS; i++)
    {
        g_seSlots[i].seq = SsSeqOpen(g_seqTable[0x1e],g_vabId);
        g_seSlots[i].id = 0;
        g_seSlots[i].keep = 0;
    }
    g_seNext = 0;
}

/* Load a VAB archive (entry 0 the body, entry 1 the header): the header into g_vabHeader, the
 * body to SPU RAM. setSeqBuf: the sequence archive goes after the header (0x8002B5E0) */
/* 0x8002B5E0 */
void Snd_LoadVab(FileId file, char setSeqBuf)
{
    short block;
    u8 *vab;
    int size;

    block = g_fileHandles[File_ReadSync(file,NULL,FILE_WHOLE)].block;
    vab = g_secHeapBlocks[block].addr;
    size = ARCHIVE_ENTRY(vab,2) - ARCHIVE_ENTRY(vab,1);
    Mem_Copy(g_vabHeader,ARCHIVE_ENTRY(vab,1),size);
    if (setSeqBuf == 1)
    {
        g_seqArchive = (int *)(g_vabHeader + ((size + 3) / 4) * 4);
    }
    do
    {
        g_vabId = SsVabOpenHead(g_vabHeader,-1);
    } while (g_vabId == -1);
    while (SsVabTransBody(ARCHIVE_ENTRY(g_secHeapBlocks[block].addr,0),g_vabId) != g_vabId)
    {
    }
    while (SsVabTransCompleted(1) == 0)
    {
    }
    SecHeap_Free(block);
}

/* Play song (0-22) on the next of the 5 BGM sequences: unpacked into the other of the 2 buffers;
 * looping (mode 0), song 6 once (0x8002B738) */
/* 0x8002B738 */
void Bgm_Play(short song, short mode, short volL, short volR)
{
    if (song > 22)
    {
        song = 22;
    }
    SsSeqClose(g_bgmSeqs[g_bgmCur]);
    g_bgmCur = (g_bgmCur == BGM_SEQS - 1) ? 0 : g_bgmCur + 1;
    Lzss_Decompress((int *)g_seqTable[song],g_bgmBufs[g_bgmBufCur]);
    g_bgmSeqs[g_bgmCur] = SsSeqOpen(g_bgmBufs[g_bgmBufCur],g_vabId);
    g_bgmBufCur = (g_bgmBufCur == 1) ? 0 : g_bgmBufCur + 1;
    SsSeqSetVol(g_bgmSeqs[g_bgmCur],volL,volR);
    if (song == 6)
    {
        mode = 1;
    }
    SsSeqPlay(g_bgmSeqs[g_bgmCur],1,mode);
}

/* Play sound effect id (an entry of the sequence archive): an id already playing is stopped first;
 * the next slot whose sequence has ended, or when none, the last slot of a round not marked keep
 * (all marked: the one before the current). Returns the slot (0x8002B8D8) */
/* 0x8002B8D8 */
int Se_Play(u16 id, short volL, short volR, u16 keep)
{
    int slot;
    int tries;
    int i;

    for (i = 0; i < SE_SLOTS; i++)
    {
        if (g_seSlots[i].id == id)
        {
            g_seSlots[i].id = -1;
            SsSeqSetVol(g_seSlots[i].seq,0,0);
            SsSeqStop(g_seSlots[i].seq);
            break;
        }
    }
    for (tries = 1; ; tries++)
    {
        g_seNext = (g_seNext == SE_SLOTS - 1) ? 0 : g_seNext + 1;
        if (SsIsEos(g_seSlots[g_seNext].seq,0) == 0)
        {
            slot = g_seNext;
            break;
        }
        if (tries > SE_SLOTS - 1)
        {
            slot = -1;
            for (i = 0; i < SE_SLOTS; i++)
            {
                if (g_seSlots[g_seNext].keep == 0 || (i == SE_SLOTS - 1 && slot == -1))
                {
                    slot = g_seNext;
                }
                g_seNext = (g_seNext == SE_SLOTS - 1) ? 0 : g_seNext + 1;
            }
            break;
        }
    }
    SsSeqClose(g_seSlots[slot].seq);
    g_seSlots[slot].seq = SsSeqOpen(g_seqTable[id],g_vabId);
    SsSeqSetVol(g_seSlots[slot].seq,volL,volR);
    SsSeqPlay(g_seSlots[slot].seq,1,1);
    g_seSlots[slot].keep = keep;
    g_seSlots[slot].id = id;
    return slot;
}

/* Silence and stop all 5 BGM sequences (0x800FE058) and 5 SE sequences (0x800D6038). (0x8002BC08) */
/* 0x8002BC08 */
void Snd_StopAll(void)
{
    int i;

    for (i = 0; i < BGM_SEQS; i++)
    {
        SsSeqSetVol(g_bgmSeqs[i],0,0);
        SsSeqStop(g_bgmSeqs[i]);
    }
    for (i = 0; i < SE_SLOTS; i++)
    {
        SsSeqSetVol(g_seSlots[i].seq,0,0);
        SsSeqStop(g_seSlots[i].seq);
    }
}

/* Turn the offsets of an archive table {count, offset...} into pointers (0x8002BCDC) */
/* 0x8002BCDC */
void Snd_RelocateTable(int *table)
{
    int i;

    for (i = 0; i <= table[0]; i++)
    {
        table[1 + i] += (int)table;
    }
}

/* Volume 0 and SsSeqStop on the current BGM sequence. (0x8002BD14) */
/* 0x8002BD14 */
void Bgm_Stop(void)
{
    SsSeqSetVol(g_bgmSeqs[g_bgmCur],0,0);
    SsSeqStop(g_bgmSeqs[g_bgmCur]);
}

/* Stop the sound effect in a slot (0x8002BD70) */
/* 0x8002BD70 */
void Se_StopSlot(short slot)
{
    SsSeqSetVol(g_seSlots[slot].seq,0,0);
    SsSeqStop(g_seSlots[slot].seq);
}

/* Stop sound effect id if it is playing (0x8002BDC8) */
/* 0x8002BDC8 */
void Se_Stop(short id)
{
    int i;

    for (i = 0; i < SE_SLOTS; i++)
    {
        if (g_seSlots[i].id == id)
        {
            g_seSlots[i].id = -1;
            SsSeqSetVol(g_seSlots[i].seq,0,0);
            SsSeqStop(g_seSlots[i].seq);
            return;
        }
    }
}

/* Sound output: 0 stereo, 1 mono (0x8002BE64) */
/* 0x8002BE64 */
void Snd_SetMono(char mono)
{
    DslATV vol [2];

    if (mono == 0)
    {
        SsSetStereo();
    }
    else
    {
        if (mono != 1)
        {
            return;
        }
        SsSetMono();
    }
    vol[0].val0 = 127;
    vol[0].val1 = 0;
    vol[0].val2 = 127;
    vol[0].val3 = 0;
    DsMix(vol);
}

/* Returns 0 (no callers) (0x8002BED0) */
/* 0x8002BED0 */
int Snd_Unused(void)
{
    return 0;
}
