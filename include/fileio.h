/*
 * fileio.h - the CD request queue, file reads, XA streaming, the sector heap
 *
 * The declarations of src/fileio.c: its types and constants, its globals and its functions.
 * See docs/fileio.md.
 */

#ifndef FILEIO_H
#define FILEIO_H

#include "common.h"
#include "psyq.h"
#include "fileid.h"

/* One queued CD request (g_cdQueue, 16 entries, ring buffer). */
typedef struct CdRequest
{
    void *loc;                /* 0x00 CdlLOC * into g_fileLocTable */
    void *buf;                /* 0x04 destination */
    int nsectors;             /* 0x08 2048-byte sectors to read */
    short kind;               /* 0x0C 0 = seek+read, 1 = seek only */
    short handle;             /* 0x0E index into g_fileHandles */
} CdRequest;                  /* size 0x10 */

/* FileHandle.flags */
#define FILEH_PENDING 0x01    /* request queued or in flight */
#define FILEH_KEEP    0x10    /* File_ReadAsync keep != 0: handle stays allocated after completion
                                 until File_ReleaseHandle; otherwise it frees itself (flags -> 0) */

/* Read handle returned by File_ReadAsync (g_fileHandles, 32 entries). */
typedef struct FileHandle
{
    short block;              /* 0x00 SecHeap block holding the data, -1 if caller gave a buffer */
    unsigned short flags;     /* 0x02 FILEH_* ; 0 = free */
    short queueSlot;          /* 0x04 g_cdQueue slot used */
} FileHandle;                 /* size 6 */

/* Sector heap block (g_secHeapBlocks, 32 entries). nsectors == 0 means free. */
typedef struct SecBlock
{
    void *addr;               /* 0x00 inside 0x8015C000..0x801F0000 */
    int nsectors;             /* 0x04 */
} SecBlock;                   /* size 8 */

#define CD_QUEUE_LEN        16
#define FILE_MAX_HANDLES    32
#define SECHEAP_MAX_BLOCKS  32
#define SECHEAP_SECTORS     296                     /* 0x128 */
#define SECHEAP_BASE        ((unsigned char *)0x8015C000)
#define SECTOR_SIZE         0x800
#define FILE_WHOLE          0x514C8                 /* "read whole file" sentinel used by callers */

#define CD_MODE_DATA  0          /* Cd_Update serves the file request queue */
#define CD_MODE_XA    1          /* it streams XA audio */
/* CdRequest.kind */
#define CDREQ_READ    0          /* seek and read */
#define CDREQ_SEEK    1          /* seek only */
/* g_cdState: Cd_UpdateQueue's steps for the request at the head of the queue */
enum CdState
{
    CDQ_IDLE,                    /* start the next request */
    CDQ_SEEK,
    CDQ_WAIT_SEEK,
    CDQ_READ,
    CDQ_WAIT_READ,
    CDQ_SEEK_ONLY,               /* a CDREQ_SEEK request */
    CDQ_WAIT_SEEK_ONLY
};

/* XA streaming (Xa_Play -> Cd_Update with g_cdMode == 1) */
typedef struct XaTrack
{
    int file;                   /* index into g_fileLocs (see the bug note at Xa_Play) */
    int length;                 /* in units of 32 sectors */
    int channel;                /* XA channel for CdlSetfilter */
} XaTrack;

/* g_xaState: Cd_UpdateXa's steps */
enum XaState
{
    XA_SEEK,                     /* to the track */
    XA_WAIT_SEEK,
    XA_FILTER,                   /* the channel, the volume up */
    XA_READ,                     /* CdRead2, real-time audio */
    XA_WAIT_READY,
    XA_PLAYING,                  /* the position polled until past the end */
    XA_MUTE,                     /* (Xa_RequestStop goes here) */
    XA_PAUSE                     /* then the data queue again */
};

/* A .J archive in memory: u32 count, u32 offset[count + 1] from the start of the file (the
 * last one = the file size), then the entries (docs/disc-formats.md). The address of entry i: */
#define ARCHIVE_ENTRY(file, i)  ((u8 *)(file) + ((int *)(file))[1 + (i)])

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern CdlLOC g_fileLocTable[FILE_COUNT];             /* 0x800A5750 */
extern int g_fileSectorTable[FILE_COUNT];             /* 0x800A65D8 */
extern XaTrack *g_xaSets[3];                          /* 0x800A881C */
extern int g_xaEndPos;                                /* 0x800B2460 */
extern int g_xaCurPos;                                /* 0x800B2464 */
extern char g_fileIoDisabled;                         /* 0x800B2468 */
extern int g_cdBusy;                                  /* 0x800B246C */
extern char g_cdMode;                                 /* 0x800B2470 CD_MODE_* */
extern int g_secHeapUsed, g_secHeapPeak;              /* 0x800B2478, 0x800B247C */
extern CdRequest g_cdQueue[CD_QUEUE_LEN];             /* 0x800CBEF0 */
extern char g_cdState, g_cdQueueHead, g_cdQueueTail;  /* 0x800CBFF0..2 */
extern CdlLOC *g_fileLocs;                            /* 0x800CBFF4 */
extern int *g_fileSizes;                              /* 0x800CBFF8 */
extern FileHandle g_fileHandles[FILE_MAX_HANDLES];    /* 0x800CC000 */
extern CdlLOC *g_xaLoc;                               /* 0x800CC0C0 */
extern int g_xaLength;                                /* 0x800CC0C4 */
extern int g_xaChannel;                               /* 0x800CC0C8 */
extern short g_xaState;                               /* 0x800CC0CC (lh/sh in the original) */
extern unsigned int g_secHeapBitmap[10];              /* 0x800CC0D0 */
extern SecBlock g_secHeapBlocks[SECHEAP_MAX_BLOCKS];  /* 0x800CC0F8 */

/* ---- Functions ---- */

/* src/fileio.c */
int  File_ReadAsync(FileId fileId, void *dst, int maxSectors, short keep); /* 0x8001C958 */
int  File_SeekAsync(FileId fileId, short flag);           /* 0x8001CB5C */
int  Xa_Play(int set, int track);                         /* 0x8001CCB4 */
void Cd_Update(void);                                     /* 0x8001CD60 */
void Cd_InitQueue(void);                                  /* 0x8001D25C */
int  File_ReadSync(FileId fileId, void *dst, int maxSectors); /* 0x8001D2E0 */
int  File_SeekSync(FileId fileId);                        /* 0x8001D344 */
int  File_IsDone(int handle);                             /* 0x8001D3A8 */
void File_ReleaseHandle(int handle);                      /* 0x8001D3E8 */
int  File_GetSectors(FileId fileId);                      /* 0x8001D408 */
int  Xa_RequestStop(void);                                /* 0x8001D424 */
int  Cd_IsXaPlaying(void);                                /* 0x8001D460 */
int  SecHeap_Alloc(int nsectors);                         /* 0x8001D48C */
void SecHeap_Free(short block);                           /* 0x8001D6B4 */
void SecHeap_Shrink(short block, int nsectors);           /* 0x8001D7BC */
void SecHeap_Init(void);                                  /* 0x8001D8D4 */
/* helpers of the rebuild (inline code in the original) */
short File_ReadUnpacked(FileId fileId);
void File_LoadPackedSprites(FileId fileId, short group);
void File_LoadPackedImages(FileId fileId);
void File_LoadTim(FileId fileId);

#endif /* FILEIO_H */
