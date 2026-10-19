/*
 * fileio.c - the CD request queue, file reads, XA streaming, the sector heap
 *
 * Files are addressed by FileId (include/fileid.h): an index into a static table of disc
 * positions and sector counts, in disc directory order. No filenames are used at runtime.
 *
 * Reads are queued (16-entry ring) and serviced by Cd_Update, a state machine pumped from the
 * VSync callback and by File_ReadSync. The same driver streams XA audio when g_cdMode == 1.
 *
 * Buffers usually come from the sector heap: 296 x 2 KB sectors at 0x8015C000, tracked by a
 * bitmap, handed out as up to 32 blocks. File_LoadPackedSprites / _Images / File_LoadTim load
 * whole image files into VRAM.
 */
#include "fileio.h"
#include "lzss.h"
#include "gfx.h"
#include "util.h"

/* Data (g_cdQueue, g_fileHandles, g_secHeap*, g_xa*, ...) is declared in
 * fileio.h and defined in src/data.c / src/bss.c. */

#define NEXT_SLOT(i)      ((i) == CD_QUEUE_LEN - 1 ? 0 : (i) + 1)
#define SECTOR_USED(s)    (g_secHeapBitmap[(s) / 32] & g_bitMask32[(s) % 32])
#define SECTOR_SET(s)     (g_secHeapBitmap[(s) / 32] |= g_bitMask32[(s) % 32])
#define SECTOR_CLEAR(s)   (g_secHeapBitmap[(s) / 32] &= ~g_bitMask32[(s) % 32])
#define SECTOR_OF(addr)   ((unsigned int)((unsigned char *)(addr) - SECHEAP_BASE) >> 11)

/* 0x8001C958
 * Queue a read of up to maxSectors of fileId into dst (NULL = allocate from
 * the sector heap). Returns a handle for File_IsDone, or
 * -1 no free handle, -2 queue full, -3 heap full.
 * Note: on -3 the queue tail has already been advanced (as in the original),
 * leaving a stale request in the slot. */
int File_ReadAsync(FileId fileId, void *dst, int maxSectors, short keep)
{
    char slot = g_cdQueueTail;
    char next;
    short h, block;
    CdRequest *q;

    if (g_fileIoDisabled)
        return 0;

    for (h = 0; h < FILE_MAX_HANDLES; h++)
    {
        if (g_fileHandles[h].flags == 0)
            break;
    }
    if (h == FILE_MAX_HANDLES)
        return -1;

    next = NEXT_SLOT(slot);
    if (g_cdQueueHead == next)
        return -2;

    if (g_fileSizes[fileId] < maxSectors)
        maxSectors = g_fileSizes[fileId];

    block = -1;
    g_cdQueueTail = next;
    if (dst == NULL)
    {
        block = SecHeap_Alloc(maxSectors);
        if (block < 0)
            return -3;
        dst = g_secHeapBlocks[block].addr;
    }

    q = &g_cdQueue[slot];
    q->buf = dst;
    q->nsectors = maxSectors;
    q->kind = CDREQ_READ;
    q->handle = h;
    q->loc = &g_fileLocs[fileId];

    g_fileHandles[h].block = block;
    g_fileHandles[h].flags = keep ? (FILEH_KEEP | FILEH_PENDING) : FILEH_PENDING;
    g_fileHandles[h].queueSlot = slot;
    return h;
}

/* 0x8001CB5C  Queue a seek-only request (CdRequest.kind 1). Returns a handle,
 * -1 no handle, -2 queue full. (Unlike File_ReadAsync it leaves .block alone.) */
int File_SeekAsync(FileId fileId, short flag)
{
    CdlLOC *locs = g_fileLocs;
    char slot = g_cdQueueTail;
    char next;
    short h;

    if (g_fileIoDisabled)
        return 0;
    for (h = 0; h < FILE_MAX_HANDLES; h++)
    {
        if (g_fileHandles[h].flags == 0)
            break;
    }
    if (h == FILE_MAX_HANDLES)
        return -1;
    next = NEXT_SLOT(slot);
    if (g_cdQueueHead == next)
        return -2;
    g_cdQueueTail = next;
    g_cdQueue[slot].kind = CDREQ_SEEK;
    g_cdQueue[slot].handle = h;
    g_cdQueue[slot].loc = &locs[fileId];
    g_fileHandles[h].flags = flag ? (FILEH_KEEP | FILEH_PENDING) : FILEH_PENDING;
    g_fileHandles[h].queueSlot = slot;
    return h;
}

#ifdef CPU_TALK
/* The disc positions of the 15 XA files, which would be the file indices 932-946 (after
 * TIT_A.TIM 930 and WAIWAI.TIM 931, in the disc's order) if the location table went on past
 * 929. A build with CPU_TALK defined plays the voices from these (C-only, no original address). */
#define XA_FIRST_FILE 932
static CdlLOC k_xaFileLocs[15] = {
    {0x01, 0x37, 0x62, 1}, /* 932 DM00.XA, lba 7187 */
    {0x01, 0x55, 0x24, 1}, /* 933 DM01.XA, lba 8499 */
    {0x02, 0x23, 0x36, 1}, /* 934 DM02.XA, lba 10611 */
    {0x03, 0x08, 0x21, 1}, /* 935 DM03.XA, lba 13971 */
    {0x03, 0x18, 0x07, 1}, /* 936 GM00.XA, lba 14707 */
    {0x03, 0x26, 0x28, 1}, /* 937 GM01.XA, lba 15328 */
    {0x03, 0x32, 0x53, 1}, /* 938 GM02.XA, lba 15803 */
    {0x03, 0x40, 0x74, 1}, /* 939 GM03.XA, lba 16424 */
    {0x03, 0x47, 0x60, 1}, /* 940 GM04.XA, lba 16935 */
    {0x03, 0x58, 0x38, 1}, /* 941 GM05.XA, lba 17738 */
    {0x04, 0x05, 0x24, 1}, /* 942 GM06.XA, lba 18249 */
    {0x04, 0x13, 0x08, 1}, /* 943 TL00.XA, lba 18833 */
    {0x04, 0x20, 0x27, 1}, /* 944 TL01.XA, lba 19377 */
    {0x04, 0x29, 0x56, 1}, /* 945 TL02.XA, lba 20081 */
    {0x04, 0x37, 0x00, 1}, /* 946 TL03.XA, lba 20625 */
};
#endif

/* 0x8001CCB4  Start streaming XA track g_xaSets[set][track] unless already streaming.
 * Original bug kept: the file indices in g_xaSets (932, 936, 943) are past the
 * 930-entry location table, so g_xaLoc points into g_fileSectorTable, and the voice
 * plays from no real position on the disc. A build with CPU_TALK defined takes the
 * positions from k_xaFileLocs instead. */
int Xa_Play(int set, int track)
{
    XaTrack *t;

    if (g_fileIoDisabled)
        return 1;
    if (g_cdMode == CD_MODE_XA)
        return 0;
    t = &g_xaSets[(short)set][track];
    g_xaState = XA_SEEK;
#ifdef CPU_TALK
    g_xaLoc = &k_xaFileLocs[t->file - XA_FIRST_FILE];
#else
    g_xaLoc = &g_fileLocs[t->file];
#endif
    g_xaLength = t->length;
    g_cdMode = CD_MODE_XA;
    g_xaChannel = t->channel;
    return 1;
}

/* Cd_Update, g_cdMode == 0: service the head of the request queue. */
static void Cd_UpdateQueue(void)
{
    CdRequest *q = &g_cdQueue[g_cdQueueHead];
    int r;

    switch (g_cdState)
    {
    case CDQ_IDLE: /* start the next request */
        if (g_cdQueueHead == g_cdQueueTail)
        {
            g_cdBusy = 0;
            return;
        }
        g_cdBusy = 0;
        g_cdState = (q->kind == CDREQ_SEEK) ? CDQ_SEEK_ONLY : CDQ_SEEK;
        return;

    case CDQ_SEEK:
        CdControlF(CdlSeekL, (u_char *)q->loc);
        g_cdState = CDQ_WAIT_SEEK;
        /* fall through */
    case CDQ_WAIT_SEEK:
        r = CdSync(1, NULL);
        if (r == CdlDiskError)
        {
            g_cdBusy = 0;
            g_cdState = CDQ_SEEK;       /* retry seek */
            return;
        }
        if (r != CdlComplete)
        {
            g_cdBusy = 0;
            return;
        }
        g_cdState = CDQ_READ;
        /* fall through */
    case CDQ_READ:
        if (CdRead(q->nsectors, (u_long *)q->buf, CdlModeSpeed) == 0)
        {
            g_cdBusy = 0;
            return;
        }
        g_cdState = CDQ_WAIT_READ;
        /* fall through */
    case CDQ_WAIT_READ:
        r = CdReadSync(1, NULL);
        if (r == -1)
        {
            g_cdBusy = 0;
            g_cdState = CDQ_READ;       /* retry read */
            return;
        }
        if (r != 0)                     /* sectors still to come */
        {
            g_cdBusy = 0;
            return;
        }
        g_fileHandles[q->handle].flags &= ~FILEH_PENDING;
        break;

    case CDQ_SEEK_ONLY:
        CdControlF(CdlSeekL, (u_char *)q->loc);
        g_cdState++;
        /* fall through */
    case CDQ_WAIT_SEEK_ONLY:
        r = CdSync(1, NULL);
        if (r == CdlDiskError)
        {
            g_cdBusy = 0;
            g_cdState--;
            return;
        }
        if (r != CdlComplete)
        {
            g_cdBusy = 0;
            return;
        }
        g_fileHandles[q->handle].flags &= 0xFE;   /* sic: byte mask, also clears bits 8-15 */
        break;

    default:
        g_cdBusy = 0;
        return;
    }

    g_cdBusy = 0;
    g_cdState = CDQ_IDLE;
    g_cdQueueHead = NEXT_SLOT(g_cdQueueHead);
}

/* Cd_Update, g_cdMode == 1: stream XA audio from g_xaLoc. */
static void Cd_UpdateXa(void)
{
    u_char result[8];
    CdlFILTER filter;
    int r;

    switch (g_xaState)
    {
    case XA_SEEK:
        g_xaCurPos = 0;
        r = DsPosToInt((DslLOC *)g_xaLoc);
        g_cdState = CDQ_IDLE;
        g_xaEndPos = g_xaLength * 32 + r;
        CdControlF(CdlSeekL, (u_char *)g_xaLoc);
        g_xaState++;
        /* fall through */
    case XA_WAIT_SEEK:
        r = CdSync(1, NULL);
        if (r == CdlDiskError)
        {
            g_cdBusy = 0;
            g_xaState--;
            return;
        }
        if (r != CdlComplete)
        {
            g_cdBusy = 0;
            return;
        }
        break;

    case XA_FILTER:
        filter.file = 1;
        filter.chan = (u_char)g_xaChannel;
        CdControlF(CdlSetfilter, (u_char *)&filter);
        g_xaState++;
        SsSetSerialVol(SS_SERIAL_A, 0x7F, 0x7F);
        g_cdBusy = 0;
        return;

    case XA_READ:
        if (CdRead2(CdlModeSpeed | CdlModeRT | CdlModeSF) == 0)
        {
            g_cdBusy = 0;
            return;
        }
        break;

    case XA_WAIT_READY:
        if (CdReady(1, result) != 0)
        {
            g_cdBusy = 0;
            return;
        }
        break;

    case XA_PLAYING: /* poll position until past the end */
        while (CdControl(CdlGetlocP, NULL, NULL) == 0)
            ;
        r = CdSync(1, result);
        if (r == CdlDiskError)
        {
            /* reading again, muted as in case 6 (which then runs too) */
            CdControl(CdlReadS, NULL, NULL);
            SsSetSerialVol(SS_SERIAL_A, 0, 0);
            break;
        }
        if (r != CdlComplete)
        {
            g_cdBusy = 0;
            return;
        }
        if (g_xaCurPos <= g_xaEndPos)
        {
            /* GetlocP result bytes 5..7 = absolute min/sec/sector */
            g_xaCurPos = DsPosToInt((DslLOC *)&result[5]);
            g_cdBusy = 0;
            return;
        }
        break;

    case XA_MUTE:
        SsSetSerialVol(SS_SERIAL_A, 0, 0);
        break;

    case XA_PAUSE:
        if (CdControl(CdlPause, NULL, NULL) != 0)
            g_cdMode = CD_MODE_DATA;
        /* fall through */
    default:
        g_cdBusy = 0;
        return;
    }
    g_xaState++;
    g_cdBusy = 0;
}

/* 0x8001CD60 (one function in the original; split in two here for clarity) */
void Cd_Update(void)
{
    if (g_fileIoDisabled || g_cdBusy)
        return;
    g_cdBusy = 1;

    if (g_cdMode == CD_MODE_DATA)
        Cd_UpdateQueue();
    else if (g_cdMode == CD_MODE_XA)
        Cd_UpdateXa();
    else
        g_cdBusy = 0;
}

/* 0x8001D25C */
void Cd_InitQueue(void)
{
    short h;

    g_cdState = CDQ_IDLE;
    g_fileIoDisabled = 1;               /* Sys_Init clears this right after */
    g_fileLocs = g_fileLocTable;
    g_cdQueueHead = 0;
    g_cdQueueTail = 0;
    g_fileSizes = g_fileSectorTable;
    g_cdBusy = 0;
    g_cdMode = CD_MODE_DATA;
    for (h = 0; h < FILE_MAX_HANDLES; h++)
        g_fileHandles[h].flags = 0;
}

/* 0x8001D2E0 */
int File_ReadSync(FileId fileId, void *dst, int maxSectors)
{
    int h;

    if (g_fileIoDisabled)
        return 0;
    h = File_ReadAsync(fileId, dst, maxSectors, 0);
    if (h == -1)                        /* sic: -2 / -3 are not checked */
        return -1;
    while (File_IsDone(h) == 0)
        Cd_Update();
    return h;
}

/* 0x8001D344 */
int File_SeekSync(FileId fileId)
{
    int h;

    if (g_fileIoDisabled)
        return 0;
    h = File_SeekAsync(fileId, 0);
    if (h == -1)
        return -1;
    while (File_IsDone(h) == 0)
        Cd_Update();
    return h;
}

/* 0x8001D3A8 */
int File_IsDone(int handle)
{
    if (g_fileIoDisabled)
        return 1;
    return (g_fileHandles[handle].flags & FILEH_PENDING) ^ 1;
}

/* 0x8001D3E8 */
void File_ReleaseHandle(int handle)
{
    g_fileHandles[handle].flags = 0;
}

/* 0x8001D408 */
int File_GetSectors(FileId fileId)
{
    return g_fileSizes[fileId];
}

/* 0x8001D424  Make the XA stream state machine wind down (volume off, pause). */
int Xa_RequestStop(void)
{
    if (g_fileIoDisabled)
        return 1;
    if (g_xaState < XA_MUTE)
        g_xaState = XA_MUTE;
    return 1;
}

/* 0x8001D460 */
int Cd_IsXaPlaying(void)
{
    if (g_fileIoDisabled)
        return 0;
    return g_cdMode == CD_MODE_XA;
}

/* 0x8001D48C
 * First-fit allocation of nsectors contiguous sectors. Returns a block id or -1.
 * Quirk (original): a run is only accepted while extending onto a following
 * free sector, so sector 295 can never end an allocation. */
int SecHeap_Alloc(int nsectors)
{
    short b, i, j;

    for (b = 0; b < SECHEAP_MAX_BLOCKS; b++)
    {
        if (g_secHeapBlocks[b].nsectors == 0)
            break;
    }
    if (b == SECHEAP_MAX_BLOCKS)
        return -1;

    i = 0;
    while (i < SECHEAP_SECTORS)
    {
        j = i + 1;
        if (!SECTOR_USED(i))
        {
            while (j < SECHEAP_SECTORS)         /* run [i, j) is free */
            {
                if (nsectors <= j - i)
                {
                    g_secHeapBlocks[b].addr = SECHEAP_BASE + i * SECTOR_SIZE;
                    g_secHeapBlocks[b].nsectors = nsectors;
                    for (; i < j; i++)
                        SECTOR_SET(i);
                    g_secHeapUsed += nsectors;
                    if (g_secHeapPeak < g_secHeapUsed)
                        g_secHeapPeak = g_secHeapUsed;
                    return b;
                }
                if (SECTOR_USED(j))
                    break;
                j++;
            }
            j++;
        }
        i = j;
    }
    return -1;
}

/* 0x8001D6B4 */
void SecHeap_Free(short block)
{
    short s, end;
    int n;

    if (block == -1 || g_secHeapBlocks[block].nsectors == 0)
        return;
    s = SECTOR_OF(g_secHeapBlocks[block].addr);
    end = s + g_secHeapBlocks[block].nsectors;
    for (; s < end; s++)
        SECTOR_CLEAR(s);
    n = g_secHeapBlocks[block].nsectors;
    g_secHeapBlocks[block].nsectors = 0;
    g_secHeapUsed -= n;
}

/* 0x8001D7BC */
void SecHeap_Shrink(short block, int nsectors)
{
    short s, end;
    int n;

    if (block == -1 || g_secHeapBlocks[block].nsectors == 0)
        return;
    s = SECTOR_OF(g_secHeapBlocks[block].addr);
    end = s + g_secHeapBlocks[block].nsectors;
    for (s += nsectors; s < end; s++)
        SECTOR_CLEAR(s);
    n = g_secHeapBlocks[block].nsectors;
    g_secHeapBlocks[block].nsectors = nsectors;
    g_secHeapUsed = g_secHeapUsed - n + nsectors;
}

/* 0x8001D8D4 */
void SecHeap_Init(void)
{
    int i;

    for (i = 9; i >= 0; i--)
        g_secHeapBitmap[i] = 0;
    for (i = SECHEAP_MAX_BLOCKS - 1; i >= 0; i--)
        g_secHeapBlocks[i].nsectors = 0;
    g_secHeapUsed = 0;
    g_secHeapPeak = 0;
}

/* Not a function of the original, which repeats this inline: read the LZSS-packed file fileId
 * (its first word is the unpacked size) and unpack it into a new section heap block; returns
 * that block. */
short File_ReadUnpacked(FileId fileId)
{
    short packed = g_fileHandles[File_ReadSync(fileId, NULL, FILE_WHOLE)].block;
    int size = *(int *)g_secHeapBlocks[packed].addr;
    short block = SecHeap_Alloc((size + 0x7FF) / 0x800);

    Lzss_Decompress(g_secHeapBlocks[packed].addr, g_secHeapBlocks[block].addr);
    SecHeap_Free(packed);
    return block;
}

/* Not a function of the original (repeated inline there): load the packed sprite set file
 * fileId into sprite group `group`. */
void File_LoadPackedSprites(FileId fileId, short group)
{
    short block = File_ReadUnpacked(fileId);

    Sprite_LoadSet(g_secHeapBlocks[block].addr, group, NULL);
    DrawSync(0);
    SecHeap_Free(block);
}

/* Not a function of the original (repeated inline there): upload the packed image stream file
 * fileId (TIMs and palettes) to VRAM. */
void File_LoadPackedImages(FileId fileId)
{
    short block = File_ReadUnpacked(fileId);

    Tim_UploadStream(g_secHeapBlocks[block].addr, NULL);
    DrawSync(0);
    SecHeap_Free(block);
}

/* Not a function of the original (repeated inline there): upload the TIM file fileId (not
 * packed) to VRAM. */
void File_LoadTim(FileId fileId)
{
    short block = g_fileHandles[File_ReadSync(fileId, NULL, FILE_WHOLE)].block;

    Tim_UploadStream(g_secHeapBlocks[block].addr, NULL);
    DrawSync(0);
    SecHeap_Free(block);
}
