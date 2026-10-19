/*
 * card.c - memory card access
 *
 * The game's layer over the BIOS memory card functions: the card's status with retries, reading
 * its directory, creating, opening, reading, writing and deleting save files a sector at a time,
 * formatting, and the BIOS card events all of these wait on. The save and load screens
 * (src/save.c) are built on it.
 */
#include "common.h"
#include "topshop.h"

/* Card status with retries: up to 3 equal error/timeout results in a row (15
 * attempts at most, then CARD_ERROR); CARD_OK and CARD_NEW return at once. (0x8001AD24) */
short Card_Info(long chan)
{
    short result;
    int tries = 0;
    int n = 0, next;
    short seen[4];

    for (;;)
    {
        Card_ClearSwEvents();
        _card_info(chan);
        result = Card_WaitSwEvent();
        if (result == CARD_OK)
            return CARD_OK;
        if (result == CARD_NEW)
            return CARD_NEW;
        seen[n] = result;
        next = n + 1;
        if (next < 2 || seen[0] == seen[n])
        {
            if (next == 3)
                return result;
            VSync(0);
        }
        else
        {
            next = 1;
            seen[0] = seen[n];
        }
        tries++;
        n = next;
        if (tries > 14)
            return CARD_ERROR;
    }
}

/* Read the card's directory into the BIOS (_card_load); on failure retry up to 64
 * times, clearing the "new card" state when reported. (0x8001AEB0) */
short Card_Load(long chan)
{
    int changed = 0;
    short first, result;
    int i;

    Card_ClearSwEvents();
    _card_load(chan);
    first = Card_WaitSwEvent();
    result = CARD_OK;
    if (first != CARD_OK)
    {
        for (i = 0; i < 0x40; i++)
        {
            Card_ClearSwEvents();
            _card_load(chan);
            result = Card_WaitSwEvent();
            if (result == CARD_OK)
                return CARD_OK;
            if (first != result)
                changed = 1;
            if (result == CARD_NEW)
                Card_Clear(chan);
            VSync(0);
        }
        if (changed)
            result = CARD_ERROR;
    }
    return result;
}

/* lseek with retries; 0 ok, 2 failed. (0x8001B06C) */
int Card_Seek(long fd, long offset)
{
    int fails = 0;
    int r;

    for (;;)
    {
        if (lseek(fd, offset, 0) != -1)
            return 0;
        r = Card_Info(0);
        if (r != CARD_OK)
        {
            if (r == CARD_NEW)
                Card_Clear(0);
            fails++;
        }
        if (fails > 14)
            return 2;
    }
}

/* read with retries; 0 ok, 1 failed. (0x8001B1AC) */
int Card_Read(long fd, void *buf, long n)
{
    int fails = 0;
    int r;

    for (;;)
    {
        if (read(fd, buf, n) == n)
            return 0;
        r = Card_Info(0);
        if (r != CARD_OK)
        {
            if (r == CARD_NEW)
                Card_Clear(0);
            fails++;
        }
        if (fails > 14)
            return 1;
    }
}

/* seek + read with retries; Card_Seek's error, 0 ok, 1 failed. (0x8001B2F4) */
int Card_ReadAt(long fd, void *buf, long offset, long n)
{
    int fails = 0;
    int r;

    for (;;)
    {
        r = Card_Seek(fd, offset);
        if (r != 0)
            return r;
        if (read(fd, buf, n) == n)
            return 0;
        r = Card_Info(0);
        if (r != CARD_OK)
        {
            if (r == CARD_NEW)
                Card_Clear(0);
            fails++;
        }
        if (fails > 59)
            return 1;
    }
}

/* write with retries; 0 ok, 1 failed. (0x8001B460) */
int Card_Write(long fd, void *buf, long n)
{
    int fails = 0;
    int r;

    for (;;)
    {
        if (write(fd, buf, n) == n)
            return 0;
        r = Card_Info(0);
        if (r != CARD_OK)
        {
            if (r == CARD_NEW)
                Card_Clear(0);
            fails++;
            VSync(0);
        }
        if (fails > 14)
            return 1;
    }
}

/* seek + write with retries; Card_Seek's error, 0 ok, 1 failed. (0x8001B5AC) */
int Card_WriteAt(long fd, void *buf, long offset, long n)
{
    int fails = 0;
    int r;

    for (;;)
    {
        r = Card_Seek(fd, offset);
        if (r != 0)
            return r;
        if (write(fd, buf, n) == n)
            return 0;
        r = Card_Info(0);
        if (r != CARD_OK)
        {
            if (r == CARD_NEW)
                Card_Clear(0);
            fails++;
            VSync(0);
        }
        if (fails > 59)
            return 1;
    }
}

/* The card's directory read (g_cardFiles): g_cardStatus CARD_OK */
static int Card_Listed(void)
{
    Card_ListFiles();
    g_cardInfoValid = 1;
    g_cardStatus = CARD_OK;
    return g_cardStatus;
}

/* Check the card and (re)read its directory when it is new; sets and returns
 * g_cardStatus: CARD_OK (directory in g_cardFiles), CARD_ERROR / CARD_TIMEOUT
 * (no card), CARD_NEW (unformatted). (0x8001B71C) */
int Card_Check(void)
{
    g_cardStatus = Card_Info(0);
    if (CARD_FAILED(g_cardStatus))
    {
        return g_cardStatus;
    }
    if (g_cardStatus == CARD_OK)
    {
        if (g_cardInfoValid != 0)
        {
            return g_cardStatus;
        }
    }
    else if (g_cardStatus != CARD_NEW)
    {
        g_cardInfoValid = 0;
        return g_cardStatus;
    }
    else
    {
        /* a new card: cleared first */
        g_cardInfoValid = 0;
        g_cardStatus = CARD_NEW;
        g_cardStatus = Card_Clear(0);
        if (CARD_FAILED(g_cardStatus))
        {
            return g_cardStatus;
        }
    }
    g_cardStatus = Card_Load(0);
    if (CARD_FAILED(g_cardStatus))
    {
        return g_cardStatus;
    }
    if (g_cardStatus == CARD_NEW)
    {
        /* unformatted */
        g_cardNumFiles = 0;
        g_cardUsedBlocks = 0;
        return g_cardStatus;
    }
    return Card_Listed();
}

/* Card_Check with more patience: up to 16 status checks, and up to 16 load or
 * clear failures, before giving up (returns the last event result then).
 * (0x8001B8FC) */
int Card_CheckRetry(void)
{
    short loads = 0;               /* failed directory loads */
    short clears = 0;              /* failed "new card" clears */
    int infos = 0;                 /* status checks (never reset) */
    short count;
    int r;

    for (;;)
    {
        do
        {
            Card_ClearSwEvents();
            _card_info(0);
            g_cardStatus = Card_WaitSwEvent();
        } while (g_cardStatus != CARD_OK && g_cardStatus != CARD_NEW && (short)++infos < 16);
        r = g_cardStatus;
        if (g_cardStatus == CARD_OK)
        {
            if (g_cardInfoValid != 0)
            {
                return g_cardStatus;
            }
            Card_ClearSwEvents();
            _card_load(0);
            r = Card_WaitSwEvent();
            g_cardStatus = r;
            if (!CARD_FAILED(r))
            {
                return (r == CARD_NEW) ? g_cardStatus : Card_Listed();
            }
            count = ++loads;
        }
        else if (g_cardStatus != CARD_NEW)
        {
            g_cardInfoValid = 0;
            return g_cardStatus;
        }
        else
        {
            /* a new card: cleared, then read */
            g_cardInfoValid = 0;
            Card_ClearHwEvents();
            _card_clear(0);
            r = Card_WaitHwEvent();
            g_cardStatus = r;
            if (CARD_FAILED(r))
            {
                count = ++clears;
            }
            else
            {
                Card_ClearSwEvents();
                _card_load(0);
                r = Card_WaitSwEvent();
                g_cardStatus = r;
                if (!CARD_FAILED(r))
                {
                    if (r == CARD_NEW)
                    {
                        g_cardNumFiles = 0;
                        g_cardUsedBlocks = 0;
                        g_cardStatus = CARD_NEW;
                        return g_cardStatus;
                    }
                    return Card_Listed();
                }
                count = ++loads;
            }
        }
        if (count > 15)
        {
            return r;
        }
    }
}

/* Read the directory into g_cardFiles / g_cardNumFiles / g_cardUsedBlocks. (0x8001BCCC) */
void Card_ListFiles(void)
{
    struct DIRENTRY dir;
    struct DIRENTRY *entry;
    int n;

    g_cardNumFiles = 0;
    g_cardUsedBlocks = 0;
    for (entry = firstfile(g_cardRoot, &dir); entry != NULL; entry = nextfile(&dir))
    {
        strcpy(g_cardFiles[g_cardNumFiles].name, dir.name);
        n = g_cardNumFiles;
        g_cardFiles[n].blocks = (dir.size + 0x1fff) / 0x2000;
        g_cardNumFiles = g_cardNumFiles + 1;
        g_cardUsedBlocks = g_cardUsedBlocks + g_cardFiles[n].blocks;
    }
}

/* A save file header's icon byte (data[2]): how many icon frames follow the title */
#define CARD_ICON_1  0x11
#define CARD_ICON_2  0x12
#define CARD_ICON_3  0x13

/* Create a save file of `blocks` 8 KB blocks and write its header (title and
 * 1-3 icon frames, size by data[2] = 0x11/0x12/0x13). Sets and returns
 * g_cardCreateResult: 0 ok, 1 create failed, 2 not enough space, 3 header write
 * failed, 4 open failed. (0x8001BD9C) */
int Card_CreateFile(char *name, int blocks, u8 *data)
{
    short tries;
    int size;
    int n;

    memcpy(g_cardPathCreate, g_cardRoot, 6);
    strcat(g_cardPathCreate, name);
    if (15 - (u16)g_cardUsedBlocks < blocks)
    {
        g_cardCreateResult = 2;
        return 2;
    }
    for (tries = 0; tries < 15; tries++)
    {
        g_cardFd = open(g_cardPathCreate, blocks << 16 | 0x200);   /* O_CREAT */
        if (g_cardFd != -1)
        {
            close(g_cardFd);
            break;
        }
    }
    if (tries == 15)
    {
        g_cardCreateResult = 1;
        return 1;
    }
    strcpy(g_cardFiles[g_cardNumFiles].name, name);
    n = g_cardNumFiles;
    g_cardFiles[n].blocks = blocks;
    g_cardNumFiles = g_cardNumFiles + 1;
    g_cardUsedBlocks = g_cardUsedBlocks + g_cardFiles[n].blocks;

    /* the header: title and 1-3 icon frames */
    for (tries = 0; tries < 15; tries++)
    {
        g_cardFd = open(g_cardPathCreate, 2);                      /* O_WRONLY */
        if (g_cardFd != -1)
        {
            switch ((char)data[2])
            {
            case CARD_ICON_1: size = 0x100; break;
            case CARD_ICON_2: size = 0x180; break;
            case CARD_ICON_3: size = 0x200; break;
            default:          size = 0;     break;
            }
            if (size != 0 && Card_WriteFile(data, 0, size) != 0)
            {
                g_cardCreateResult = 3;
                return 3;
            }
            break;
        }
    }
    if (tries == 15)
    {
        g_cardCreateResult = 4;
        return 4;
    }
    g_cardCreateResult = 0;
    return 0;
}

/* Delete a save file; 0 ok, 1 erase failed, 2 not found. (0x8001C014) */
int Card_DeleteFile(char *name)
{
    int i;

    memcpy(g_cardPathDelete, g_cardRoot, 6);
    strcat(g_cardPathDelete, name);
    for (i = 0; (short)i < g_cardNumFiles; i++)
    {
        if (strcmp(g_cardFiles[(short)i].name, name) == 0)
            break;
    }
    if ((short)i == g_cardNumFiles)
        return 2;
    if (erase(g_cardPathDelete) != 1)
        return 1;
    g_cardUsedBlocks = g_cardUsedBlocks - g_cardFiles[(short)i].blocks;
    for (i = i + 1; (short)i < g_cardNumFiles; i++)
        g_cardFiles[(short)i - 1] = g_cardFiles[(short)i];
    g_cardNumFiles = g_cardNumFiles - 1;
    return 0;
}

/* 0x8001C1E8 */
void Card_OpenEvents(void)
{
    g_cardInfoValid = 0;
    EnterCriticalSection();
    g_evSwIOE = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, NULL);
    g_evSwError = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, NULL);
    g_evSwTimeout = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    g_evSwNew = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, NULL);
    g_evHwIOE = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, NULL);
    g_evHwError = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, NULL);
    g_evHwTimeout = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    g_evHwNew = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, NULL);
    ExitCriticalSection();
    Card_DisableEvents();
}

/* 0x8001C2FC */
void Card_CloseEvents(void)
{
    EnterCriticalSection();
    CloseEvent(g_evSwIOE);
    CloseEvent(g_evSwError);
    CloseEvent(g_evSwTimeout);
    CloseEvent(g_evSwNew);
    CloseEvent(g_evHwIOE);
    CloseEvent(g_evHwError);
    CloseEvent(g_evHwTimeout);
    CloseEvent(g_evHwNew);
    ExitCriticalSection();
}

/* 0x8001C384 */
void Card_EnableEvents(void)
{
    EnableEvent(g_evSwIOE);
    EnableEvent(g_evSwError);
    EnableEvent(g_evSwTimeout);
    EnableEvent(g_evSwNew);
    EnableEvent(g_evHwIOE);
    EnableEvent(g_evHwError);
    EnableEvent(g_evHwTimeout);
    EnableEvent(g_evHwNew);
}

/* 0x8001C3FC */
void Card_DisableEvents(void)
{
    DisableEvent(g_evSwIOE);
    DisableEvent(g_evSwError);
    DisableEvent(g_evSwTimeout);
    DisableEvent(g_evSwNew);
    DisableEvent(g_evHwIOE);
    DisableEvent(g_evHwError);
    DisableEvent(g_evHwTimeout);
    DisableEvent(g_evHwNew);
}

/* Wait for one of the software card events. (0x8001C474) */
int Card_WaitSwEvent(void)
{
    for (;;)
    {
        if (TestEvent(g_evSwIOE) == 1)
            return CARD_OK;
        if (TestEvent(g_evSwError) == 1)
            return CARD_ERROR;
        if (TestEvent(g_evSwTimeout) == 1)
            return CARD_TIMEOUT;
        if (TestEvent(g_evSwNew) == 1)
            return CARD_NEW;
    }
}

/* Wait for one of the hardware card events. (0x8001C4E4) */
int Card_WaitHwEvent(void)
{
    for (;;)
    {
        if (TestEvent(g_evHwIOE) == 1)
            return CARD_OK;
        if (TestEvent(g_evHwError) == 1)
            return CARD_ERROR;
        if (TestEvent(g_evHwTimeout) == 1)
            return CARD_TIMEOUT;
        if (TestEvent(g_evHwNew) == 1)
            return CARD_NEW;
    }
}

/* 0x8001C554 */
void Card_ClearSwEvents(void)
{
    TestEvent(g_evSwIOE);
    TestEvent(g_evSwError);
    TestEvent(g_evSwTimeout);
    TestEvent(g_evSwNew);
}

/* 0x8001C59C */
void Card_ClearHwEvents(void)
{
    TestEvent(g_evHwIOE);
    TestEvent(g_evHwError);
    TestEvent(g_evHwTimeout);
    TestEvent(g_evHwNew);
}

/* Format the card (15 attempts); sets and returns g_cardFormatResult: 0 ok, 1 failed. (0x8001C5E4) */
int Card_Format(void)
{
    int i;

    for (i = 0; i < 15; i++)
    {
        if (_card_format(0) != 0)
        {
            g_cardFormatResult = 0;
            g_cardNumFiles = 0;
            g_cardUsedBlocks = 0;
            return 0;
        }
    }
    g_cardFormatResult = 1;
    return 1;
}

/* Open a listed save file into g_cardFd; 0 ok, 1 open failed (g_cardFd = -1),
 * 2 not listed. (0x8001C648) */
int Card_OpenFile(char *name, unsigned long mode)
{
    int i;
    short found;

    memcpy(g_cardPathOpen, g_cardRoot, 6);
    strcat(g_cardPathOpen, name);
    for (found = 0; found < g_cardNumFiles; found++)
    {
        if (strcmp(g_cardFiles[found].name, name) == 0)
            break;
    }
    if (found == g_cardNumFiles)
        return 2;
    for (i = 0; i < 15; i++)
    {
        g_cardFd = open(g_cardPathOpen, mode);
        if (g_cardFd != -1)
            return 0;
        g_cardFd = -1;
    }
    g_cardFd = -1;
    return 1;
}

/* 0x8001C780 */
void Card_CloseFile(void)
{
    close(g_cardFd);
}

/* Read n bytes (a multiple of 128) at offset of the open file, a sector at a time. (0x8001C7A4) */
int Card_ReadFile(u8 *buf, int offset, int n)
{
    int r;

    while (n != 0)
    {
        r = Card_ReadAt(g_cardFd, buf, offset, 0x80);
        n -= 0x80;
        if (r != 0)
            return r;
        buf += 0x80;
        offset += 0x80;
    }
    return 0;
}

/* Write n bytes (a multiple of 128) at offset of the open file, a sector at a time. (0x8001C80C) */
int Card_WriteFile(u8 *buf, int offset, int n)
{
    int r;

    while (n != 0)
    {
        r = Card_WriteAt(g_cardFd, buf, offset, 0x80);
        n -= 0x80;
        if (r != 0)
            return r;
        buf += 0x80;
        offset += 0x80;
    }
    return 0;
}

/* Acknowledge a new card (_card_clear), up to 15 attempts; the last event result. (0x8001C874) */
int Card_Clear(long chan)
{
    int tries = 0;
    int r;

    for (;;)
    {
        Card_ClearHwEvents();
        _card_clear(chan);
        r = Card_WaitHwEvent();
        tries++;
        if (r == CARD_OK || tries > 14)
            return r;
    }
}
