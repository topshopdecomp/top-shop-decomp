/*
 * effects.c - the event cards' effects on the stores and the players
 *
 * The event card effects that take several frames: stores and chains closed, opened, taken over,
 * stolen or demolished, deliveries, the players' money handed around, two players switching
 * places. Each shows what changed (the camera goes to the stores, the shop-open animation
 * plays). Event_ApplyTask (src/events.c) runs them.
 */
#include "common.h"
#include "topshop.h"

/* Camera_MoveTo (mode) the stores from square (x, floor) on, `width` pixels wide: in the middle of
 * the screen, 60 lines down; 1 once there */
static u32 Effect_CameraTo(int mode, char x, char floor, char width)
{
    return Camera_MoveTo(mode,CAMERA_CELL_X(x,0x78 - width),CAMERA_CELL_Y(floor,0x3c));
}

/* Effect_ShowStores's steps, from `first` on */
enum ShowStoresStep
{
    SHOW_CAMERA_TO      = 0,  /* the camera to the stores */
    SHOW_SHOP_OPEN      = 1,  /* the shop-open animation */
    SHOW_WAIT_SHOP_OPEN = 2,
    SHOW_CAMERA_BACK    = 3 
};

/* The last steps of the store effects, from step `first` on: the camera goes to the stores, the
 * shop-open animation shows them change (with the frame cursor in the middle when `cursor`), the
 * camera goes back; 1 once done */
static u32 Effect_ShowStores(short *step, int first, char x, char floor, char width, int cursor)
{
    switch (*step - first)
    {
    case SHOW_CAMERA_TO:
        if (!Effect_CameraTo(2,x,floor,width))
        {
            return 0;
        }
        *step += 1;
        break;
    case SHOW_SHOP_OPEN:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        *step += 1;
        break;
    case SHOW_WAIT_SHOP_OPEN:
        if (cursor)
        {
            Menu_DrawFrameCursor(0x9f,0x3c);
        }
        if (!Task_IsDone(g_actionTaskIds[1]))
        {
            return 0;
        }
        *step += 1;
        break;
    case SHOW_CAMERA_BACK:
        if (Effect_CameraTo(1,x,floor,width))
        {
            return 1;
        }
        break;
    }
    return 0;
}

/* The type of the store the current player gets on square `square`: the type of its own store
 * there, else the last of the square's four types that is not on the board yet (square * 4 - 1
 * when they all are) */
static int Effect_TypeForSquare(char square)
{
    int type;
    int i;

    type = Store_GetOwnedTypeAt(g_curPlayer,square);
    if ((u8)type != 0xff)
    {
        return type;
    }
    for (i = 3; i >= 0 && g_storeTypes[square * 4 + i].available == 0; i--)
    {
    }
    return square * 4 + i;
}

/* Effect_CloseChain steps */
enum CloseChainStep
{
    CLOSECHAIN_PICK   = 0,  /* a random chain closes */
    CLOSECHAIN_TARGET = 1,  /* where the camera goes */
    CLOSECHAIN_SHOW   = 2   /* Effect_ShowStores */
};

/* Event effect: a random chain of who whose chainLen is `length` closes (the chain head's store
 * twice, as in the original); the camera shows it (0x80070974) */
/* 0x80070974 */
u32 Effect_CloseChain(short *step, EffectCloseChainWork *work, char who, char length)
{
    int i;

    switch (*step)
    {
    case CLOSECHAIN_PICK:
        Stores_ListChainHeads(who,length);
        if (g_storeListCount == 0)
        {
            return 1;
        }
        work->first = g_storeList[rand() % g_storeListCount];
        for (i = 0; i < length + 1; i++)
        {
            Store_Close((work->first + i));
        }
        *step += 1;
        break;
    case CLOSECHAIN_TARGET:
        work->x = g_stores[work->first].x;
        work->width = length * 40;
        work->floor = g_stores[work->first].floor;
        Store_Close(work->first);
        *step += 1;
        break;
    default:
        return Effect_ShowStores(step,CLOSECHAIN_SHOW,work->x,work->floor,work->width,0);
    }
    return 0;
}

/* Effect_OpenStoreNewSquare steps */
enum OpenStoreNewSquareStep
{
    NEWSQ_OPEN = 0,  /* the store opens */
    NEWSQ_SHOW = 1   /* Effect_ShowStores */
};

/* Event effect: a store for the current player on a random vacant lot on a square where it has no
 * store yet (Stores_ListVacantNewSquares), of the first of the square's four types that no open
 * store on that square has; payer (-1: nobody) pays its price (0x80070C88) */
/* 0x80070C88 */
u32 Effect_OpenStoreNewSquare(short *step, EffectOpenStoreNewSquareWork *work, char payer)
{
    char taken[4];
    Store *st;
    int i;

    g_debugEffectStep = *step;
    if (*step != NEWSQ_OPEN)
    {
        return Effect_ShowStores(step,NEWSQ_SHOW,work->x,work->floor,0,1);
    }
    Stores_ListVacantNewSquares(g_curPlayer);
    work->store = g_storeList[rand() % g_storeListCount];
    work->x = g_stores[work->store].x;
    work->floor = g_stores[work->store].floor;
    work->square = g_stores[work->store].squareId;
    for (i = 3; i >= 0; i--)
    {
        taken[i] = 0;
    }
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        if (st->owner != -1 && st->squareId == work->square)
        {
            taken[((char)st->storeType % 4)] = 1;
        }
    }
    for (i = 0; i < 4 && taken[i] != 0; i++)
    {
    }
    work->storeType = (char)work->square * 4 + i;
    Store_Open(work->store,work->storeType,g_curPlayer);
    if (payer != -1)
    {
        Player_Pay(payer,StoreType_GetPrice(work->storeType,0));
    }
    g_debugEffectStore = work->store;
    g_debugEffectStoreType = work->storeType;
    *step += 1;
    return 0;
}

/* Effect_TakeOverChain steps */
enum TakeOverChainStep
{
    TAKECHAIN_PICK = 0,  /* the stores become the player's */
    TAKECHAIN_SHOW = 1   /* Effect_ShowStores */
};

/* Event effect: one of the stores the current player could extend a chain with
 * (Stores_ListExtendable), at random: every store with its squareId becomes the player's (the
 * others' closed first), of that store's type, with goods of levels 5, 4 and 3 (0x80071098) */
/* 0x80071098 */
u32 Effect_TakeOverChain(short *step, EffectTakeOverChainWork *work)
{
    Store *st;
    int i;
    int k;

    if (*step != TAKECHAIN_PICK)
    {
        return Effect_ShowStores(step,TAKECHAIN_SHOW,work->x,work->floor,0,1);
    }
    Stores_ListExtendable(g_curPlayer);
    work->store = g_storeList[rand() % g_storeListCount];
    st = &g_stores[work->store];
    work->x = st->x;
    work->floor = st->floor;
    work->square = st->squareId;
    work->storeType = st->storeType;
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        if (work->square == st->squareId)
        {
            if (st->owner != -1 && st->owner != g_curPlayer)
            {
                Store_Close(i);
            }
            Store_Open(i,work->storeType,g_curPlayer);
        }
    }
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        if (work->square == st->squareId)
        {
            for (k = 0; k < 8; k++)
            {
                st->goods[k].active = (k < 3);
                st->goods[k].level = (k < 3) ? 5 - k : 0;
            }
        }
        Store_SortGoods(i);                     /* (every store) */
    }
    *step += 1;
    return 0;
}

/* Effect_StealChain steps */
enum StealChainStep
{
    STEALCHAIN_PICK   = 0,  /* a random chain of four becomes the current player's */
    STEALCHAIN_TARGET = 1,  /* where the camera goes */
    STEALCHAIN_SHOW   = 2   /* Effect_ShowStores */
};

/* Event effect: a random chain of four of who becomes the current player's (0x80071508) */
/* 0x80071508 */
u32 Effect_StealChain(short *step, EffectStealChainWork *work, char who)
{
    int i;

    switch (*step)
    {
    case STEALCHAIN_PICK:
        Stores_ListChainHeads(who,3);
        if (g_storeListCount == 0)
        {
            return 1;
        }
        work->store = g_storeList[rand() % g_storeListCount];
        for (i = 0; i < 4; i++)
        {
            g_stores[work->store + i].owner = g_curPlayer;
        }
        *step += 1;
        break;
    case STEALCHAIN_TARGET:
        work->x = g_stores[work->store].x;
        work->width = 120;
        work->floor = g_stores[work->store].floor;
        *step += 1;
        break;
    default:
        return Effect_ShowStores(step,STEALCHAIN_SHOW,work->x,work->floor,work->width,0);
    }
    return 0;
}

/* Effect_Delivery steps */
enum EffectDeliveryStep
{
    EFFDELIV_START      =   0,
    EFFDELIV_STORE      =   1,  /* DeliveryStore_Task: which store */
    EFFDELIV_WAIT_STORE =   2,  /* none chosen: done */
    EFFDELIV_GOODS      =  10,  /* Delivery_Task: the goods */
    EFFDELIV_WAIT_GOODS =  11,  /* EV_DELIVER_ALL_OWN: another store */
    EFFDELIV_DONE       = 100 
};

/* Event effect: the delivery dialogs: the store (DeliveryStore_Task: one of the current player's,
 * or of `who` for EV_DELIVER_RIVAL), then the goods (Delivery_Task, free for EV_DELIVER_FREE); for
 * EV_DELIVER_ALL_OWN again until no store is chosen. CPU-driven with RULE_AUTO_OWN (0x800717EC) */
/* 0x800717EC */
u32 Effect_Delivery(short *step, EffectDeliveryWork *work, char event, char who)
{
    Player *pl = &g_players[g_curPlayer];

    switch (*step)
    {
    case EFFDELIV_START:
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        *step += 1;
        return 0;
    case EFFDELIV_STORE:
        if (g_boardRules.enable[RULE_AUTO_OWN] != 0)
        {
            work->isCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        if (event == EV_DELIVER_RIVAL)
        {
            g_uiTaskWork[7][1] = who;
        }
        else if (event == EV_DELIVER_OWN || event == EV_DELIVER_ALL_OWN || event == EV_DELIVER_FREE)
        {
            g_uiTaskWork[7][1] = g_curPlayer;
        }
        Task_Start(g_uiTaskIds[7],DeliveryStore_Task);
        *step += 1;
        return 0;
    case EFFDELIV_WAIT_STORE:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        *step = (Task_GetUserData(g_uiTaskIds[7]) < 0) ? EFFDELIV_DONE : EFFDELIV_GOODS;
        if (g_boardRules.enable[RULE_AUTO_OWN] != 0)
        {
            pl->isCpu = work->isCpu;
        }
        return 0;
    case EFFDELIV_GOODS:
        if (g_boardRules.enable[RULE_AUTO_OWN] != 0)
        {
            work->isCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        g_uiTaskWork[7][1] = Task_GetUserData(g_uiTaskIds[7]);
        g_uiTaskWork[7][2] = (event == EV_DELIVER_FREE) ? DELIVERY_FREE : DELIVERY_PAID;
        Task_Start(g_uiTaskIds[7],Delivery_Task);
        *step += 1;
        return 0;
    case EFFDELIV_WAIT_GOODS:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_OWN] != 0)
        {
            pl->isCpu = work->isCpu;
        }
        *step = (event == EV_DELIVER_ALL_OWN) ? EFFDELIV_STORE : EFFDELIV_DONE;
        return 0;
    case EFFDELIV_DONE:
        return 1;
    }
    return 0;
}

/* Effect_RebuildClosed steps */
enum RebuildClosedStep
{
    REBUILD_PICK = 0,  /* the store becomes the player's */
    REBUILD_SHOW = 1   /* Effect_ShowStores */
};

/* Event effect: a random store of who without goods becomes the current player's
 * (Effect_TypeForSquare) (0x80071B58) */
/* 0x80071B58 */
u32 Effect_RebuildClosed(short *step, EffectRebuildClosedWork *work, char who)
{
    if (*step != REBUILD_PICK)
    {
        return Effect_ShowStores(step,REBUILD_SHOW,work->x,work->floor,0,1);
    }
    Stores_ListClosed(who);
    if (g_storeListCount == 0)
    {
        return 1;
    }
    work->store = g_storeList[rand() % g_storeListCount];
    work->squareId = g_stores[work->store].squareId;
    work->x = g_stores[work->store].x;
    work->floor = g_stores[work->store].floor;
    Store_Close(work->store);
    work->storeType = Effect_TypeForSquare(work->squareId);
    Store_Open(work->store,work->storeType,g_curPlayer);
    *step += 1;
    return 0;
}

/* Effect_TakeOverClosed steps */
enum TakeOverClosedStep
{
    TAKEOVER_LIST           = 0,  /* the other players' stores without goods */
    TAKEOVER_NEXT           = 1,  /* the next one changes (all done: back) */
    TAKEOVER_CAMERA         = 2,  /* the camera to it */
    TAKEOVER_SHOP_OPEN      = 3,  /* the shop-open animation shows it */
    TAKEOVER_WAIT_SHOP_OPEN = 4,  /* then the next one */
    TAKEOVER_CAMERA_BACK    = 5 
};

/* Event effect: every store of the other players without goods becomes the current player's
 * (Effect_TypeForSquare), shown one after the other (0x80071E7C) */
/* 0x80071E7C */
u32 Effect_TakeOverClosed(short *step, EffectTakeOverClosedWork *work)
{
    switch (*step)
    {
    case TAKEOVER_LIST:
        Stores_ListClosed(WHO_OTHERS);
        if (g_storeListCount == 0)
        {
            return 1;
        }
        work->k = 0;
        *step += 1;
        break;
    case TAKEOVER_NEXT:
        if (work->k == g_storeListCount)
        {
            *step = TAKEOVER_CAMERA_BACK;
            return 0;
        }
        work->store = g_storeList[work->k];
        work->squareId = g_stores[work->store].squareId;
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
        Store_Close(work->store);
        work->storeType = Effect_TypeForSquare(work->squareId);
        Store_Open(work->store,work->storeType,g_curPlayer);
        *step += 1;
        break;
    case TAKEOVER_CAMERA:
        if (!Effect_CameraTo(2,work->x,work->floor,0))
        {
            return 0;
        }
        *step += 1;
        break;
    case TAKEOVER_SHOP_OPEN:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        *step += 1;
        break;
    case TAKEOVER_WAIT_SHOP_OPEN:
        Menu_DrawFrameCursor(0x9f,0x3c);
        if (Task_IsDone(g_actionTaskIds[1]))
        {
            work->k++;
            *step = TAKEOVER_NEXT;
        }
        return 0;
    case TAKEOVER_CAMERA_BACK:
        if (Effect_CameraTo(1,work->x,work->floor,0))
        {
            return 1;
        }
        break;
    }
    return 0;
}

/* Effect_DemolishClosed steps */
enum DemolishClosedStep
{
    DEMOLISH_LIST           = 0,  /* the other players' stores without goods */
    DEMOLISH_NEXT           = 1,  /* the next one changes (all done: back) */
    DEMOLISH_CAMERA         = 2,  /* the camera to it */
    DEMOLISH_SHOP_OPEN      = 3,  /* the shop-open animation shows it */
    DEMOLISH_WAIT_SHOP_OPEN = 4,  /* then the next one */
    DEMOLISH_CAMERA_BACK    = 5 
};

/* Event effect: every store of the other players without goods closes (its lot vacant), shown one
 * after the other (0x800721A0) */
/* 0x800721A0 */
u32 Effect_DemolishClosed(short *step, EffectDemolishClosedWork *work)
{
    switch (*step)
    {
    case DEMOLISH_LIST:
        Stores_ListClosed(WHO_OTHERS);
        if (g_storeListCount == 0)
        {
            return 1;
        }
        work->k = 0;
        *step += 1;
        break;
    case DEMOLISH_NEXT:
        if (work->k == g_storeListCount)
        {
            *step = DEMOLISH_CAMERA_BACK;
            return 0;
        }
        work->store = g_storeList[work->k];
        work->square = g_stores[work->store].squareId;
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
        Store_Close(work->store);
        *step += 1;
        break;
    case DEMOLISH_CAMERA:
        if (!Effect_CameraTo(2,work->x,work->floor,0))
        {
            return 0;
        }
        *step += 1;
        break;
    case DEMOLISH_SHOP_OPEN:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        *step += 1;
        break;
    case DEMOLISH_WAIT_SHOP_OPEN:
        Menu_DrawFrameCursor(0x9f,0x3c);
        if (Task_IsDone(g_actionTaskIds[1]))
        {
            work->k++;
            *step = DEMOLISH_NEXT;
        }
        return 0;
    case DEMOLISH_CAMERA_BACK:
        if (Effect_CameraTo(1,work->x,work->floor,0))
        {
            return 1;
        }
        break;
    }
    return 0;
}

/* Event effect: the players' money is handed around in a random order: each player still in gets
 * the money another one had (a random permutation in which nobody keeps its own) (0x8007243C) */
/* 0x8007243C */
u32 Effect_ShuffleMoney(void)
{
    struct
    {
        char player;
        char from;                              /* whose money it gets */
    } give[8];
    char perm[8];                             /* give[i] gets give[perm[i]]'s money */
    int money[BOARD_MAX_PLAYERS];
    int count;
    int left;
    int pick;
    int seen;
    int r;
    int i;
    int k;

    count = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].out == 0)
        {
            give[count].player = i;
            give[count].from = -1;
            count++;
        }
    }
    for (i = 0; i < count; i++)
    {
        perm[i] = -1;
    }
    /* each one in turn goes to a random free place other than its own */
    for (i = 0; i < count; i++)
    {
        left = count - i;
        if (perm[i] == -1)
        {
            left--;
        }
        pick = (i != count - 1) ? rand() % left : 0;
        seen = 0;
        for (k = 0; k < count; k++)
        {
            if (k != i && perm[k] == -1 && seen++ == pick)
            {
                perm[k] = i;
                break;
            }
        }
    }
    if (perm[count - 1] == -1)
    {
        /* the last one was left in its own place: it swaps with a random other one */
        r = rand();
        perm[count - 1] = perm[r % (count - 1)];
        perm[r % (count - 1)] = count - 1;
    }
    for (i = 0; i < count; i++)
    {
        give[i].from = give[perm[i]].player;
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        money[i] = g_players[i].money;
    }
    for (i = 0; i < count; i++)
    {
        g_players[give[i].player].money = money[give[i].from];
    }
    return 1;
}

/* Effect_SwitchPlaces steps */
enum SwitchPlacesStep
{
    SWITCH_WIPE_CLOSE      =  0,
    SWITCH_WAIT_WIPE_CLOSE =  1,
    SWITCH_SWAP            = 10,  /* the two pieces swap squares */
    SWITCH_WIPE_OPEN       = 20,
    SWITCH_WAIT_WIPE_OPEN  = 21 
};

/* Event effect: the current player and `who` swap squares behind a wipe; both lose their
 * direction (0x800726FC) */
/* 0x800726FC */
u32 Effect_SwitchPlaces(short *step, u32 work, char who)
{
    Player *cur = &g_players[g_curPlayer];
    Player *rival = &g_players[who];
    char x;
    char floor;

    switch (*step)
    {
    case SWITCH_WIPE_CLOSE:
        Task_Start(g_turnFxTaskId,Wipe_CloseTask);
        *step += 1;
        break;
    case SWITCH_WAIT_WIPE_CLOSE:
        if (Task_IsDone(g_turnFxTaskId))
        {
            *step = SWITCH_SWAP;
        }
        break;
    case SWITCH_SWAP:
        x = rival->x;
        floor = rival->floor;
        rival->x = cur->x;
        rival->floor = cur->floor;
        cur->x = x;
        cur->floor = floor;
        cur->dirX = 0;
        cur->dirFloor = 0;
        rival->dirX = 0;
        rival->dirFloor = 0;
        cur->screenX = PIECE_SCREEN_X(cur->x);
        cur->screenY = PIECE_SCREEN_Y(cur->floor);
        rival->screenX = PIECE_SCREEN_X(rival->x);
        rival->screenY = PIECE_SCREEN_Y(rival->floor);
        Board_FocusCurrentPlayer();
        *step = SWITCH_WIPE_OPEN;
        break;
    case SWITCH_WIPE_OPEN:
        Task_Start(g_turnFxTaskId,Wipe_OpenTask);
        *step += 1;
        break;
    case SWITCH_WAIT_WIPE_OPEN:
        if (Task_IsDone(g_turnFxTaskId))
        {
            return 1;
        }
        break;
    }
    return 0;
}

/* Effect_BuyVacant steps */
enum BuyVacantStep
{
    BUYVACANT_LOT            = 0,  /* VacantLot_Task (LOT_PICK_FREE) */
    BUYVACANT_WAIT_LOT       = 1,
    BUYVACANT_SHOP_OPEN      = 2,
    BUYVACANT_WAIT_SHOP_OPEN = 3 
};

/* Not called: an event effect: the vacant lot dialog for a free store on a lot along
 * the route (VacantLot_Task, LOT_PICK_FREE), then the shop-open animation (0x800729A4) */
/* 0x800729A4 */
u32 Effect_BuyVacant(short *step)
{
    switch (*step)
    {
    case BUYVACANT_LOT:
        g_uiTaskWork[7][1] = LOT_PICK_FREE;
        Task_Start(g_uiTaskIds[7],VacantLot_Task);
        break;
    case BUYVACANT_WAIT_LOT:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        break;
    case BUYVACANT_SHOP_OPEN:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        break;
    case BUYVACANT_WAIT_SHOP_OPEN:
        return Task_IsDone(g_actionTaskIds[1]) != 0;
    default:
        return 0;
    }
    *step += 1;
    return 0;
}
