/*
 * shopopen.c - the stores' pictures and the shop-open animation
 *
 * The picture of each store type, uploaded to VRAM for the stores on the board, and the shop-
 * open animation that shows, after a move or an event, the stores that opened, changed hands or
 * closed, with a banner.
 */
#include "common.h"
#include "topshop.h"

/* The picture file of a store: SH<type><tier>_<n>.TIM by its type, chain length and position in
 * the chain */
static FileId Store_ImageFile(short store)
{
    Store *st = &g_stores[store];

    return (short)((char)st->chainPos + Util_DecimalTier((char)st->storeType,st->chainLen)) +
           FILE_SH00L1_1_TIM;
}

/* Remember every store's owner and chain (ShopOpen_Task shows what changed since) */
void Stores_TakeSnapshot(void)
{
    int i;

    for (i = 0; i < MAX_STORES; i++)
    {
        g_storeSnapshot[i].owner = g_stores[i].owner;
        g_storeSnapshot[i].chainLen = g_stores[i].chainLen;
        g_storeSnapshot[i].chainPos = g_stores[i].chainPos;
    }
}

/* The "open" banner over a store, slid down by slide pixels, clipped to the store's square */
static void ShopOpen_DrawBanner(int store, short slide)
{
    StageDef *stage = &g_stageDefs[g_boardStage];
    short x = (g_cameraX / 4096) + (stage->originX + g_stores[store].x * 5) * 16;
    short y = (g_cameraY / 4096) + (stage->originY + g_stores[store].floor * 5) * 16;

    Gfx_SetDrawArea(NULL,g_ot + g_otIndex,0x10);
    g_drawPos[0] = x;
    g_drawPos[1] = slide + y;
    Sprite_DrawAligned(g_spriteGroups[2].base + 2,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                       g_ot + g_otIndex,0x10);
    g_drawPos[0] = x;
    g_drawPos[2] = 0x50;
    g_drawPos[3] = 0x30;
    g_drawPos[1] = y;
    Gfx_SetDrawArea(g_drawPos,g_ot + g_otIndex,0x10);
}

/* ShopOpenWork.state: the banners drawn */
#define BANNER_NONE         0
#define BANNER_CHANGED     (-1)  /* over the changed stores (sliding off) */
#define BANNER_OWNED        1    /* over the owned and the marked vacant ones (sliding back) */
#define BANNER_ALL_CHANGED  2    /* over every store changed since the snapshot */

/* ShopOpen_Task steps */
enum ShopOpenStep
{
    SHOP_INIT                =   0,  /* the stores changed since the last snapshot */
    SHOP_BANNER_OFF          =  10,  /* the banner slides off the changed stores */
    SHOP_BANNER_SLIDING_OFF  =  11,
    SHOP_PICTURES            =  20,  /* the owned ones' new pictures */
    SHOP_LOAD_NEXT           =  21,  /* read one after the other */
    SHOP_WAIT_LOAD           =  22,
    SHOP_LOADED              =  23,
    SHOP_READ_NEXT           =  25,  /* read already (up to 4 at once) */
    SHOP_WAIT_READ           =  26,
    SHOP_READ_DONE           =  27,
    SHOP_BANNER_BACK         =  30,  /* the banner slides back over the owned and the marked vacant ones */
    SHOP_BANNER_SLIDING_BACK =  31,
    SHOP_SNAPSHOT            = 100,  /* a new snapshot */
    SHOP_WAIT                = 101   /* 30 frames */
};

/* After a move: the stores whose owner or chain changed since the last snapshot. The banner
 * slides off the changed ones, the new pictures of the owned ones are loaded (up to 4 at
 * once, else one after the other) and uploaded, the banner slides back over them, and a new
 * snapshot is taken (0x8004BAC8) */
/* 0x8004BAC8 */
u32 ShopOpen_Task(short *step, ShopOpenWork *work)
{
    Store *st;
    int store;
    int i;

    work->ownedList = g_shopOpenOwned;
    work->changedList = g_shopOpenChanged;
    FntPrint("exec task shop open\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch(*step)
    {
    case SHOP_INIT:
        work->state = BANNER_NONE;
        work->store = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        work->storeType = g_stores[work->store].storeType;
        work->owned = 0;
        work->changed = 0;
        work->vacantMarked = 0;
        work->vacant = 0;
        for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
        {
            st = &g_stores[i];
            if (st->owner == g_storeSnapshot[i].owner && st->chainLen == g_storeSnapshot[i].chainLen &&
                st->chainPos == g_storeSnapshot[i].chainPos)
            {
                continue;
            }
            if (st->owner == -1)
            {
                g_shopOpenVacant[work->vacant++] = i;
                if ((char)Board_GetSquareFlag(st->x,st->floor) == 1)
                {
                    g_shopOpenVacantMarked[work->vacantMarked++] = i;
                }
            }
            else
            {
                work->ownedList[work->owned++] = i;
            }
            if (g_storeSnapshot[i].owner != -1 || (char)Board_GetSquareFlag(st->x,st->floor) == 1)
            {
                work->changedList[work->changed++] = i;
            }
            if (work->owned > 39)
            {
                break;
            }
        }
        if (work->owned < 5)
        {
            /* start reading all the new pictures */
            work->loadAll = 1;
            for (i = 0; i < work->owned; i++)
            {
                work->store = work->ownedList[i];
                work->storeType = g_stores[work->store].storeType;
                work->handles[i] = File_ReadAsync(Store_ImageFile(work->store),NULL,FILE_WHOLE,1);
            }
        }
        else
        {
            work->loadAll = 0;
            File_SeekAsync(FILE_SH00L1_1_TIM,0);
        }
        *step = SHOP_BANNER_OFF;
        break;
    /* 10, 0xb: the banner slides off the changed stores */
    case SHOP_BANNER_OFF:
        if (work->changed == 0)
        {
            *step = SHOP_PICTURES;
            break;
        }
        Se_Play(0x27,0x7f,0x7f,1);
        work->state = BANNER_CHANGED;
        work->slide = -0x30;
        *step += 1;
        break;
    case SHOP_BANNER_SLIDING_OFF:
        work->slide += 4;
        if (work->slide >= 0)
        {
            work->slide = 0;
            *step = SHOP_PICTURES;
            for (i = 0; i < work->vacant; i++)
            {
                g_storeImageState[(char)g_shopOpenVacant[i]] = STORE_IMAGE_NONE;
            }
        }
        break;
    /* 0x14-0x1b: the pictures */
    case SHOP_PICTURES:
        work->state = BANNER_ALL_CHANGED;
        work->slide = 0;
        work->loadIndex = 0;
        *step = (work->loadAll == 1) ? SHOP_READ_NEXT : SHOP_LOAD_NEXT;
        break;
    case SHOP_LOAD_NEXT:                    /* one after the other */
        if (work->loadIndex >= work->owned)
        {
            *step = SHOP_BANNER_BACK;
            break;
        }
        work->store = work->ownedList[work->loadIndex];
        if (g_stores[work->store].owner == -1)
        {
            work->loadIndex++;
            break;
        }
        work->storeType = g_stores[work->store].storeType;
        work->handles[0] = File_ReadAsync(Store_ImageFile(work->store),NULL,FILE_WHOLE,1);
        *step += 1;
        /* fall through */
    case SHOP_WAIT_LOAD:
        if (File_IsDone(work->handles[0]) != 1) break;
        StoreImage_Upload(work->store,g_secHeapBlocks[g_fileHandles[work->handles[0]].block].addr);
        *step += 1;
        break;
    case SHOP_LOADED:
        g_storeImageState[work->store] = STORE_IMAGE_READY;
        SecHeap_Free(g_fileHandles[work->handles[0]].block);
        File_ReleaseHandle(work->handles[0]);
        work->loadIndex++;
        *step = SHOP_LOAD_NEXT;
        break;
    case SHOP_READ_NEXT:                    /* read already */
        if (work->loadIndex >= work->owned)
        {
            *step = SHOP_BANNER_BACK;
            break;
        }
        *step += 1;
        break;
    case SHOP_WAIT_READ:
        if (File_IsDone(work->handles[work->loadIndex]) != 1) break;
        work->store = work->ownedList[work->loadIndex];
        StoreImage_Upload(work->store,
                          g_secHeapBlocks[g_fileHandles[work->handles[work->loadIndex]].block].addr);
        *step += 1;
        break;
    case SHOP_READ_DONE:
        g_storeImageState[work->store] = STORE_IMAGE_READY;
        SecHeap_Free(g_fileHandles[work->handles[work->loadIndex]].block);
        File_ReleaseHandle(work->handles[work->loadIndex]);
        work->loadIndex++;
        *step = SHOP_READ_NEXT;
        break;
    /* 0x1e, 0x1f: the banner slides back over the owned and the marked vacant ones */
    case SHOP_BANNER_BACK:
        work->state = BANNER_OWNED;
        work->slide = 0;
        if (work->owned == 0 && work->vacantMarked == 0)
        {
            *step = SHOP_SNAPSHOT;
            break;
        }
        work->state = BANNER_OWNED;
        Se_Play(0x27,0x7f,0x7f,1);
        *step += 1;
        break;
    case SHOP_BANNER_SLIDING_BACK:
        work->slide -= 4;
        if (work->slide <= -0x30)
        {
            work->slide = -0x30;
            work->state = BANNER_NONE;
            *step = SHOP_SNAPSHOT;
        }
        break;
    case SHOP_SNAPSHOT:
        Stores_TakeSnapshot();
        work->delay = 0x1e;
        *step += 1;
        /* fall through */
    case SHOP_WAIT:
        if (--work->delay == 0)
        {
            return 1;
        }
        break;
    }

    switch (work->state)
    {
    case BANNER_CHANGED:
        for (i = 0; i < work->changed; i++)
        {
            ShopOpen_DrawBanner((char)work->changedList[i],work->slide);
        }
        break;
    case BANNER_OWNED:
        for (i = 0; i < work->owned; i++)
        {
            ShopOpen_DrawBanner((char)work->ownedList[i],work->slide);
        }
        for (i = 0; i < work->vacantMarked; i++)
        {
            ShopOpen_DrawBanner((char)g_shopOpenVacantMarked[i],work->slide);
        }
        break;
    case BANNER_ALL_CHANGED:
        for (store = 0; store < g_stageDefs[g_boardStage].numStores; store++)
        {
            st = &g_stores[store];
            if (st->owner != g_storeSnapshot[store].owner || st->chainLen != g_storeSnapshot[store].chainLen ||
                st->chainPos != g_storeSnapshot[store].chainPos)
            {
                ShopOpen_DrawBanner(store,work->slide);
            }
        }
        for (i = 0; i < work->vacantMarked; i++)
        {
            ShopOpen_DrawBanner((char)g_shopOpenVacantMarked[i],work->slide);
        }
        break;
    }
    return 0;
}

/* Upload the picture of every owned store (Store_ImageFile) to its VRAM slot (0x8004CFE0) */
/* 0x8004CFE0 */
void Stores_LoadImages(void)
{
    short block;
    short store;

    for (store = 0; store < g_stageDefs[g_boardStage].numStores; store++)
    {
        if (g_stores[store].owner == -1)
        {
            continue;
        }
        block = g_fileHandles[File_ReadSync(Store_ImageFile(store),NULL,FILE_WHOLE)].block;
        StoreImage_Upload(store,g_secHeapBlocks[block].addr);
        DrawSync(0);
        g_storeImageState[store] = STORE_IMAGE_READY;
        SecHeap_Free(block);
    }
}

/* VRAM position of a store's picture slot (5 per column from x 0x140, 40 x 48 each) (0x8004DE3C) */
/* 0x8004DE3C */
void StoreImage_VramPos(short store, short *pos)
{
    pos[0] = (store / 5) * 0x28 + 0x140;
    pos[1] = (store % 5) * 0x30;
}

/* CLUT position of a store's picture (g_storeClutPos) (0x8004DEA4) */
/* 0x8004DEA4 */
void StoreImage_ClutPos(int store, u16 *pos)
{
    pos[0] = g_storeClutPos[(short)store * 2];
    pos[1] = g_storeClutPos[(short)store * 2 + 1];
}

/* Upload a store picture (TIM stream) to its VRAM slot and CLUT; its state becomes 1 (uploading)
 * (0x8004E000) */
/* 0x8004E000 */
void StoreImage_Upload(short store, u32 *tim)
{
    short vramOffsets[4];

    vramOffsets[1] = (store % 5) * 0x30;
    vramOffsets[2] = g_storeClutPos[store * 2];
    vramOffsets[3] = g_storeClutPos[store * 2 + 1];
    vramOffsets[0] = (store / 5) * 0x28 + 0x140;
    Tim_UploadStream((unsigned long *)tim,vramOffsets);
    g_storeImageState[store] = STORE_IMAGE_UPLOADING;
}
