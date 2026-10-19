/*
 * stores.c - the stores
 *
 * Opening and closing stores and changing their owners, the chains of neighboring stores of one
 * owner, the goods and their prices (the store types' price tables, price changes), a store's
 * value, and the lists of stores (by owner, closed, vacant, ...) that the dialogs, the event
 * cards and the CPU players choose from.
 */
#include "common.h"
#include "topshop.h"

/* Clear the goods of a store (all its slots) */
void Store_ClearGoods(int store)
{
    int i;

    for (i = 0; i < g_stores[store].maxGoods; i++)
    {
        g_stores[store].goods[i].active = 0;
    }
    Store_SortGoods(store);
}

/* Value of a store: its type's base value plus the value of each goods slot in use (0x8007657C) */
/* 0x8007657C */
int Store_GetValue(short store)
{
    Store *st;
    int result;
    int i;

    st = &g_stores[store];
    result = st->type->value;
    for (i = 0; i < g_kindMaxGoods[st->chainLen]; i++)
    {
        if (st->goods[i].active != 0)
        {
            result += st->type->items[st->goods[i].level].cost;
        }
    }
    return result;
}

/* Recompute the selling price of each goods slot (value * rate / 100), discounted while the store
 * is closed (0x8007661C) */
/* 0x8007661C */
void Store_UpdatePrices(short store)
{
    Store *st = &g_stores[store];
    StoreGoods *goods;
    u16 price;
    int i;

    if (st->owner == -1)
    {
        return;
    }
    for (i = 0; i < 8; i++)
    {
        goods = &st->goods[i];
        if (goods->active == 0)
        {
            goods->price = 0;
            continue;
        }
        price = st->type->items[goods->level].cost * st->type->items[goods->level].markup / 100;
        goods->basePrice = price;
        if (st->tempClosed != 0 || st->priceChanged != 0)
        {
            price = (short)price * st->pricePercent / 100;
        }
        goods->price = price;
    }
}

/* Find the chains of adjacent stores with the same owner (a chain allows more goods slots), drop
 * goods beyond the new limit and re-sort every owned store (0x80076728) */
/* 0x80076728 */
void Stores_UpdateChains(void)
{
    StageDef *stage = &g_stageDefs[g_boardStage];
    Store *st;
    short owner;
    short square;
    short run;
    u16 cell;
    int floor;
    int x;
    int active;
    int m;
    int i;

    for (i = 0; i < stage->numStores; i++)
    {
        g_stores[i].chainLen = 0;
        g_stores[i].chainTypes = 0;
        g_stores[i].chainPos = 0;
    }
    /* along each floor (the run goes on from one floor's end to the next's start), stores next to
     * each other with one owner and squareId make a chain */
    run = 0;
    owner = -1;
    square = -1;
    for (floor = 0; floor < stage->floors; floor++)
    {
        for (x = 0; x < stage->width; x++)
        {
            cell = Board_GetCell(x,floor);
            if (CELL_TYPE(cell) != CELL_STORE)
            {
                run = 0;
                owner = -1;
                square = -1;
                continue;
            }
            st = &g_stores[cell];
            if (st->owner == -1)
            {
                run = 0;
            }
            else if (owner == st->owner && square == st->squareId)
            {
                /* the chain grows: its stores, back from this one, get the new length and positions */
                for (m = 0; m < run + 1; m++)
                {
                    g_stores[cell - m].chainPos = run - m;
                    g_stores[cell - m].chainLen = run;
                    g_stores[cell - m].chainTypes |= 1 << (char)st->storeType;
                }
                run++;
            }
            else
            {
                run = 1;
                st->chainLen = 0;
                st->chainPos = 0;
                st->chainTypes |= 1 << (char)st->storeType;
            }
            owner = st->owner;
            square = st->squareId;
        }
    }
    /* the goods slots the chains allow; the goods beyond go */
    for (i = 0; i < stage->numStores; i++)
    {
        st = &g_stores[i];
        if (st->owner == -1)
        {
            continue;
        }
        st->maxGoods = g_kindMaxGoods[st->chainLen];
        active = 0;
        for (m = 0; m < 8; m++)
        {
            if (st->goods[m].active != 0)
            {
                active++;
            }
            if (st->maxGoods < active)
            {
                st->goods[m].active = 0;
            }
        }
        Store_SortGoods(i);
    }
}

/* Sort a store's goods slots: the active ones first, among them the lower levels first (only
 * active and level move; an inactive slot is cleared when it comes up), count the active ones
 * and update the prices (0x80076C2C) */
/* 0x80076C2C */
void Store_SortGoods(int store)
{
    Store *s = &g_stores[store];
    StoreGoods *a;
    StoreGoods *b;
    u8 active;
    signed char level;
    int i;
    int j;

    for (i = 0; i < g_kindMaxGoods[s->chainLen] - 1; i++)
    {
        a = &s->goods[i];
        if (a->active == 0)
        {
            a->level = 0;
            a->basePrice = 0;
            a->price = 0;
        }
        for (j = i + 1; j < g_kindMaxGoods[s->chainLen]; j++)
        {
            b = &s->goods[j];
            if (a->active != 1 || (b->active == 1 && b->level < a->level))
            {
                active = a->active;
                level = a->level;
                a->active = b->active;
                a->level = b->level;
                b->active = active;
                b->level = level;
            }
        }
    }
    for (i = 0; i < g_kindMaxGoods[s->chainLen]; i++)
    {
        if (s->goods[i].active != 1)
        {
            break;
        }
    }
    s->numGoods = i;
    Store_UpdatePrices(store);
}

/* A player opens a store of a type on a vacant lot: initial goods, prices, chains (0x80077754) */
/* 0x80077754 */
void Store_Open(short store, short storeType, short owner)
{
    Store *st = &g_stores[store];
    char neighbors;
    int i;

    st->owner = owner;
    st->storeType = storeType;
    st->type = &g_storeTypes[storeType];
    st->tempClosed = 0;
    st->closedUntil = 0;
    /* the start goods, one more per neighboring store of the owner, from level 5 down */
    neighbors = Store_CountOwnedNeighbors(owner,store);
    for (i = 0; i < 8; i++)
    {
        st->goods[i].active = (i < g_gameConfig.startGoods + neighbors);
        if (st->goods[i].active)
        {
            st->goods[i].level = 5 - i;
        }
    }
    Store_SortGoods(store);
    g_storeTypes[storeType].available = 0;
    g_storeTypes[storeType].store = st;
    Stores_UpdateChains();
}

/* Give a store to another owner (goods reset to the initial ones) (0x800778A8) */
/* 0x800778A8 */
void Store_SetOwner(short store, char owner)
{
    Store *st = &g_stores[store];
    u8 type;
    int i;

    type = st->storeType;
    st->owner = owner;
    for (i = 0; i < 8; i++)
    {
        st->goods[i].active = (i < g_gameConfig.startGoods);
        if (st->goods[i].active)
        {
            st->goods[i].level = 5 - i;
        }
    }
    Store_SortGoods(store);
    g_storeTypes[(char)type].available = 0;
    g_storeTypes[(char)type].store = st;
    Stores_UpdateChains();
}

/* The store's lot becomes vacant again (and its type available if no other store has it)
 * (0x800779AC) */
/* 0x800779AC */
void Store_Close(short store)
{
    Store *st = &g_stores[store];
    short type;
    int i;

    if (st->owner == -1)
    {
        return;
    }
    type = (char)st->storeType;
    st->owner = -1;
    st->storeType = 0xff;
    st->type = NULL;
    for (i = 0; i < 8; i++)
    {
        st->goods[i].active = 0;
    }
    Store_SortGoods(store);
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        if (g_stores[i].owner != -1 && (char)g_stores[i].storeType == type)
        {
            break;
        }
    }
    if (i == g_stageDefs[g_boardStage].numStores)
    {
        g_storeTypes[type].available = 1;
        g_storeTypes[type].store = NULL;
    }
    Stores_UpdateChains();
}

/* Whether st's owner is who: WHO_CURRENT the current player, WHO_OTHERS another player, WHO_ANY
 * any player (a vacant lot is no one's), else player who */
static int Store_OwnedBy(Store *st, short who)
{
    switch (who)
    {
    case WHO_CURRENT:
        return st->owner == g_curPlayer;
    case WHO_OTHERS:
        return st->owner != g_curPlayer && st->owner != -1;
    case WHO_ANY:
        return st->owner != -1;
    default:
        return st->owner == who;
    }
}

static void StoreList_Add(int store)
{
    g_storeList[g_storeListCount] = store;
    g_storeListCount++;
}

/* List into g_storeList the stores of who (Store_OwnedBy). Returns the count (0x800789C8) */
/* 0x800789C8 */
u32 Stores_ListByOwner(short who)
{
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who))
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to the stores on floor (floors - 1 - level), level counted from
 * the top (0x80078D10) */
/* 0x80078D10 */
u32 Stores_ListByOwnerFloor(short who, int level)
{
    short floor;
    int i;

    g_storeListCount = 0;
    floor = g_stageDefs[g_boardStage].floors - level - 1;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who) && g_stores[i].floor == floor)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to the first store of chains of length length (0x8007908C) */
/* 0x8007908C */
u32 Stores_ListChainHeads(short who, short length)
{
    Store *st;
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if (Store_OwnedBy(st,who) && st->chainLen == length && st->chainPos == 0)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListChainHeads for chains of length length that are at most maxLength long; for WHO_ANY
 * chains of length length or longer (0x8007946C) */
/* 0x8007946C */
u32 Stores_ListChainHeadsUpTo(short who, short length, short maxLength)
{
    Store *st;
    int lengthOk;
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        lengthOk = (who == WHO_ANY) ? st->chainLen >= length : st->chainLen == length;
        if (Store_OwnedBy(st,who) && lengthOk && st->chainLen <= maxLength && st->chainPos == 0)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to stores whose chain can still grow: not all the stores of its
 * square are in it. g_squareStoreCount gets the number of stores of each square (0x800798B4) */
/* 0x800798B4 */
u32 Stores_ListExtendable(short who)
{
    Store *st;
    int i;

    for (i = 0; i < 11; i++)
    {
        g_squareStoreCount[i] = 0;
    }
    for (i = 0; i < NUM_STORES; i++)
    {
        g_squareStoreCount[g_stores[i].squareId]++;
    }
    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if (Store_OwnedBy(st,who) && st->chainLen != g_squareStoreCount[st->squareId] - 1)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to stores with goods (operating) (0x80079D50) */
/* 0x80079D50 */
u32 Stores_ListOperating(short who)
{
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who) && g_stores[i].numGoods != 0)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to stores with free goods slots (0x8007A0DC) */
/* 0x8007A0DC */
u32 Stores_ListNotFull(short who)
{
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who) && g_stores[i].numGoods != g_stores[i].maxGoods)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to stores without goods (not operating) (0x8007A478) */
/* 0x8007A478 */
u32 Stores_ListClosed(short who)
{
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who) && g_stores[i].numGoods == 0)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Stores_ListByOwner restricted to stores in chains of length length (0x8007A800) */
/* 0x8007A800 */
u32 Stores_ListByChainLength(short who, short length)
{
    int i;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (Store_OwnedBy(&g_stores[i],who) && g_stores[i].chainLen == length)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* Where squareId is in the list of squares Stores_ListVacantNewSquares builds, or
 * g_ownSquareCount if it is not in it */
static int OwnSquares_Find(int squareId)
{
    int k;

    for (k = 0; k < g_ownSquareCount; k++)
    {
        if (g_squareStoreCount[k] == squareId)
        {
            break;
        }
    }
    return k;
}

/* The vacant lots on the squares where player who has no store yet. The squares with who's
 * stores are listed on the way (g_ownSquareCount of them, in g_squareStoreCount) (0x8007ABA0) */
/* 0x8007ABA0 */
u32 Stores_ListVacantNewSquares(short who)
{
    int i;

    g_ownSquareCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (g_stores[i].owner == who && OwnSquares_Find(g_stores[i].squareId) == g_ownSquareCount)
        {
            g_squareStoreCount[g_ownSquareCount] = g_stores[i].squareId;
            g_ownSquareCount++;
        }
    }
    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        if (g_stores[i].owner == -1 && OwnSquares_Find(g_stores[i].squareId) == g_ownSquareCount)
        {
            StoreList_Add(i);
        }
    }
    return g_storeListCount;
}

/* The stores of owner's chains next to store (left and right), counted as chainLen + 1 each
 * (0x8007ADD0) */
/* 0x8007ADD0 */
int Store_CountOwnedNeighbors(short owner, short store)
{
    char x;
    char floor;
    short cell;
    int count;

    count = 0;
    x = g_stores[store].x;
    floor = g_stores[store].floor;
    cell = Board_GetCell(x - 1,floor);
    if ((cell & 0xff00) == CELL_STORE && g_stores[cell].owner == owner)
    {
        count = (u8)g_stores[cell].chainLen + 1;
    }
    cell = Board_GetCell(x + 1,floor);
    if ((cell & 0xff00) == CELL_STORE && g_stores[cell].owner == owner)
    {
        count += (u8)g_stores[cell].chainLen + 1;
    }
    return (signed char)count;
}

/* Price of a store of type type: its base value plus the value of its first (GameConfig.startGoods
 * + extra) goods (0x8007B310) */
/* 0x8007B310 */
int StoreType_GetPrice(short storeType, short extra)
{
    int i;
    int value;

    value = g_storeTypes[storeType].value;
    for (i = 0; i < g_gameConfig.startGoods + extra; i++)
    {
        value += g_storeTypes[storeType].items[5 - i].cost;
    }
    return value;
}

/* Store_Update (0x8007661C) for every store that has an owner (0x8007B38C) */
/* 0x8007B38C */
void Stores_UpdateOwned(void)
{
    int i;
    Store *st;

    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if (st->owner != -1)
        {
            Store_UpdatePrices(i);
        }
    }
}

/* Number of stores of type type owned by owner (0x8007B7D0) */
/* 0x8007B7D0 */
int Stores_CountOwnedOfType(short owner, short storeType)
{
    Store *st;
    int result;
    int i;

    result = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if ((st->owner == owner) && ((char)st->storeType == storeType))
        {
            result++;
        }
    }
    return result;
}

/* Type of owner's store on square squareId, -1 if none (0x8007B860) */
/* 0x8007B860 */
int Store_GetOwnedTypeAt(short owner, short squareId)
{
    Store *st;
    int i;

    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if ((st->squareId == squareId) && (st->owner == owner))
        {
            return (char)st->storeType;
        }
    }
    return -1;
}

/* List the store types of a category (4 per category) that exist into g_storeTypeList; returns the
 * count (0x8007B8F4) */
/* 0x8007B8F4 */
u8 StoreTypes_List(int category)
{
    int first;
    int i;

    g_storeTypeCount = 0;
    first = (short)category * 4;
    for (i = 0; i < 4; i++)
    {
        if (g_storeTypes[first + i].available != 0)
        {
            g_storeTypeList[g_storeTypeCount] = first + i;
            g_storeTypeCount++;
        }
    }
    return g_storeTypeCount;
}

/* List the stores without an owner into g_storeList; returns the count (0x8007BA24) */
/* 0x8007BA24 */
u32 Stores_ListVacant(void)
{
    int i;
    Store *st;

    g_storeListCount = 0;
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if (st->owner == -1)
        {
            g_storeList[g_storeListCount] = i;
            g_storeListCount++;
        }
    }
    return g_storeListCount;
}
