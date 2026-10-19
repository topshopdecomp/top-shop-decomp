/*
 * card.h - memory card access
 *
 * Memory card access through the BIOS, after the PsyQ memory card sample: files, events, checking
 * the card.
 * The declarations of src/card.c: its types and constants, its globals and its functions.
 * See docs/card.md.
 */

#ifndef CARD_H
#define CARD_H

#include "common.h"

typedef struct CardFile
{
    char name[20];               /* 0x00 */
    int blocks;                  /* 0x14 8 KB blocks */
} CardFile;                      /* size 0x18 */

/* card event result: 0 done (IOE), 1 error, 2 timeout, 3 new card */
#define CARD_OK      0
#define CARD_ERROR   1
#define CARD_TIMEOUT 2
#define CARD_NEW     3
/* no card, or no answer (CARD_ERROR, CARD_TIMEOUT) */
#define CARD_FAILED(status)  ((u16)((status) - 1) < 2)

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char g_cardRoot[6];                                /* 0x800B1E78 "bu00:" */
extern char g_cardInfoValid;                              /* 0x800B2408 directory read since the card was inserted */
extern long g_cardFd;                                     /* 0x800B240C */
extern short g_cardStatus;                                /* 0x800B2410 last Card_Check result */
extern short g_cardCreateResult;                          /* 0x800B2412 */
extern short g_cardFormatResult;                          /* 0x800B2414 */
extern short g_cardNumFiles;                              /* 0x800B2416 */
extern long g_evHwIOE, g_evHwError, g_evHwTimeout, g_evHwNew; /* 0x800B2428/2434/2418/2438 */
extern short g_cardUsedBlocks;                            /* 0x800B242C */
extern long g_evSwIOE, g_evSwError, g_evSwTimeout, g_evSwNew; /* 0x800B2430/243C/241C/2440 */
extern char g_cardPathOpen[32];                           /* 0x800CBD28 */
extern char g_cardPathCreate[32];                         /* 0x800CBD48 */
extern char g_cardPathDelete[32];                         /* 0x800CBD68 */
extern CardFile g_cardFiles[15];                          /* 0x800CBD88 */

/* ---- Functions ---- */

/* src/card.c */
short Card_Info(long chan);                               /* 0x8001AD24 */
short Card_Load(long chan);                               /* 0x8001AEB0 */
int   Card_Seek(long fd, long offset);                    /* 0x8001B06C */
int   Card_Read(long fd, void *buf, long n);              /* 0x8001B1AC */
int   Card_ReadAt(long fd, void *buf, long offset, long n); /* 0x8001B2F4 */
int   Card_Write(long fd, void *buf, long n);             /* 0x8001B460 */
int   Card_WriteAt(long fd, void *buf, long offset, long n); /* 0x8001B5AC */
int   Card_Check(void);                                   /* 0x8001B71C */
int   Card_CheckRetry(void);                              /* 0x8001B8FC */
void  Card_ListFiles(void);                               /* 0x8001BCCC */
int   Card_CreateFile(char *name, int blocks, u8 *data);  /* 0x8001BD9C */
int   Card_DeleteFile(char *name);                        /* 0x8001C014 */
void  Card_OpenEvents(void);                              /* 0x8001C1E8 */
void  Card_CloseEvents(void);                             /* 0x8001C2FC */
void  Card_EnableEvents(void);                            /* 0x8001C384 */
void  Card_DisableEvents(void);                           /* 0x8001C3FC */
int   Card_WaitSwEvent(void);                             /* 0x8001C474 */
int   Card_WaitHwEvent(void);                             /* 0x8001C4E4 */
void  Card_ClearSwEvents(void);                           /* 0x8001C554 */
void  Card_ClearHwEvents(void);                           /* 0x8001C59C */
int   Card_Format(void);                                  /* 0x8001C5E4 */
int   Card_OpenFile(char *name, unsigned long mode);      /* 0x8001C648 */
void  Card_CloseFile(void);                               /* 0x8001C780 */
int   Card_ReadFile(u8 *buf, int offset, int n);          /* 0x8001C7A4 */
int   Card_WriteFile(u8 *buf, int offset, int n);         /* 0x8001C80C */
int   Card_Clear(long chan);                              /* 0x8001C874 */

#endif /* CARD_H */
