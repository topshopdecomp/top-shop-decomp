/*
 * gameops.c - the script opcodes that act on the board game
 *
 * The handlers of the event script interpreter's (src/script.c) opcodes 0x140-0x194: the stores
 * a player owns, money for the players a script names, the goods, prices and closing of the
 * stores it names, the store dialogs a script opens (a vacant lot, selling a store), turning the
 * player around, the elevator ban, the Chirp-Chirp shoes, moving on, advancing the calendar, and
 * the results a script hands back to the board. scripts/README.md lists every opcode.
 */
#include "common.h"
#include "topshop.h"

/* The players a script names (SCRIPT_ALL_PLAYERS, SCRIPT_CUR_PLAYER, SCRIPT_OTHER_PLAYERS, else
 * that player, not checked), into list; returns how many */
static int Script_SelectPlayers(short who, short *list)
{
    int count;
    int i;

    if (who == SCRIPT_CUR_PLAYER)
    {
        list[0] = g_curPlayer;
        return 1;
    }
    if (who != SCRIPT_ALL_PLAYERS && who != SCRIPT_OTHER_PLAYERS)
    {
        list[0] = who;
        return 1;
    }
    count = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].out == 0 && (who == SCRIPT_ALL_PLAYERS || i != g_curPlayer))
        {
            list[count++] = i;
        }
    }
    return count;
}

/* Opcode 0x180: the selected players earn amount (Player_Earn; a player number beyond the
 * players: nobody) (0x80081C5C) */
/* 0x80081C5C */
u32 Script_OpPlayerEarn(void)
{
    short who;
    short amount;
    short players[BOARD_MAX_PLAYERS];
    int count;
    int i;

    SCRIPT_READ_OPS(2);
    who = SCRIPT_ARG(1);
    amount = SCRIPT_ARG(2);
    count = Script_SelectPlayers(who,players);
    if (count == 1 && players[0] >= g_numPlayers)
    {
        count = 0;
    }
    for (i = 0; i < count; i++)
    {
        Player_Earn(players[i],amount);
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x18E: money of the selected players = money * percent / 100 (0x80081E04) */
/* 0x80081E04 */
u32 Script_OpMoneyPercent(void)
{
    short percent;
    short players[BOARD_MAX_PLAYERS];
    int count;
    int i;

    SCRIPT_READ_OPS(2);
    percent = SCRIPT_ARG(2);
    count = Script_SelectPlayers(SCRIPT_ARG(1),players);
    for (i = 0; i < count; i++)
    {
        g_players[players[i]].money = g_players[players[i]].money * percent / 100;
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x18F: money of the selected players = money of player src (0 for all / the others)
 * (0x80082030) */
/* 0x80082030 */
u32 Script_OpCopyMoney(void)
{
    short from;
    int money;
    short players[BOARD_MAX_PLAYERS];
    int count;
    int i;

    SCRIPT_READ_OPS(2);
    from = SCRIPT_ARG(2);
    if (from == SCRIPT_CUR_PLAYER)
    {
        money = g_players[g_curPlayer].money;
    }
    else if (from == SCRIPT_ALL_PLAYERS || from == SCRIPT_OTHER_PLAYERS)
    {
        money = 0;
    }
    else
    {
        money = g_players[from].money;
    }
    count = Script_SelectPlayers(SCRIPT_ARG(1),players);
    for (i = 0; i < count; i++)
    {
        g_players[players[i]].money = money;
    }
    Script_Advance(6);
    return 1;
}

/* The stores a script names (into g_storeList): -1 anyone's, -2 the current player's, -3 the
 * other players', -4 - n player n's (n 0-5), else that store */
static void Script_ListStores(short which)
{
    if (which >= -9 && which <= -4)
    {
        Stores_ListByOwner(-4 - which);
    }
    else if (which >= -3 && which <= -1)
    {
        Stores_ListByOwner(which);
    }
    else
    {
        g_storeListCount = 1;
        g_storeList[0] = which;
    }
}

/* The lowest goods level (the most expensive goods) a store's chain allows */
#define STORE_MIN_LEVEL(st)  (6 - k_chainLevels[(st)->chainLen])

/* Opcode 0x181: the selected stores get count goods slots (at most their maxGoods) at level (no
 * more expensive than their chain allows) (0x80082244) */
/* 0x80082244 */
u32 Script_OpStoresSetGoods(void)
{
    short count;
    short wanted;
    int level;
    Store *st;
    int goods;
    int k;
    int i;

    SCRIPT_READ_OPS(3);
    count = SCRIPT_ARG(2);
    wanted = SCRIPT_ARG(3);
    Script_ListStores(SCRIPT_ARG(1));
    for (k = 0; k < g_storeListCount; k++)
    {
        st = &g_stores[g_storeList[k]];
        level = (wanted < STORE_MIN_LEVEL(st)) ? STORE_MIN_LEVEL(st) : wanted;
        goods = (count <= st->maxGoods) ? count : st->maxGoods;
        st->numGoods = goods;
        for (i = 0; i < 8; i++)
        {
            st->goods[i].active = (i < (short)goods);
            st->goods[i].level = (i < (short)goods) ? level : 0;
        }
        Store_SortGoods(g_storeList[k]);
    }
    Script_Advance(8);
    return 1;
}

/* Opcode 0x194: subtract n from the level of the goods of the selected stores (level 0 the
 * most expensive), kept between the chain's lowest level and 5 (0x800824E8) */
/* 0x800824E8 */
u32 Script_OpStoresGoodsLevel(void)
{
    short by;
    short level;
    Store *st;
    int k;
    int i;

    SCRIPT_READ_OPS(2);
    by = SCRIPT_ARG(2);
    Script_ListStores(SCRIPT_ARG(1));
    for (k = 0; k < g_storeListCount; k++)
    {
        st = &g_stores[g_storeList[k]];
        for (i = 0; i < 8; i++)
        {
            if (st->goods[i].active == 0)
            {
                continue;
            }
            level = st->goods[i].level - by;
            if (level < STORE_MIN_LEVEL(st))
            {
                level = STORE_MIN_LEVEL(st);
            }
            else if (level > 5)
            {
                level = 5;
            }
            st->goods[i].level = level;
        }
        Store_SortGoods(g_storeList[k]);
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x18D: the selected stores sell at percent for n months (Store.priceChanged,
 * pricePercent, priceUntil) (0x80082748) */
/* 0x80082748 */
u32 Script_OpStoresSetPrice(void)
{
    short percent;
    short months;
    Store *st;
    int k;

    SCRIPT_READ_OPS(3);
    percent = SCRIPT_ARG(2);
    months = SCRIPT_ARG(3);
    Script_ListStores(SCRIPT_ARG(1));
    for (k = 0; k < g_storeListCount; k++)
    {
        st = &g_stores[g_storeList[k]];
        st->priceChanged = 1;
        st->pricePercent = percent;
        st->priceUntil = TURN_NUMBER((short)g_gameConfig.monthsElapsed + months,g_curPlayer);
    }
    Script_Advance(8);
    return 1;
}

/* Opcode 0x191: the selected stores (Script_ListStores, or -10 - n: player 0's on the floor n from
 * the top) are temporarily closed for n months (0x80082934) */
/* 0x80082934 */
u32 Script_OpStoresCloseFor(void)
{
    short which;
    short months;
    Store *st;
    int k;

    SCRIPT_READ_OPS(2);
    which = SCRIPT_ARG(1);
    months = SCRIPT_ARG(2);
    if (which >= -15 && which <= -10)
    {
        Stores_ListByOwnerFloor(0,-10 - which);
    }
    else
    {
        Script_ListStores(which);
    }
    for (k = 0; k < g_storeListCount; k++)
    {
        st = &g_stores[g_storeList[k]];
        st->tempClosed = 1;
        st->closedUntil = TURN_NUMBER((short)g_gameConfig.monthsElapsed + months,g_curPlayer);
    }
    Script_Advance(6);
    return 1;
}

/* Script_StoreDialog's steps (g_scriptWait) */
enum StoreDialogWait
{
    STOREDLG_START          = 0,  /* start the dialog */
    STOREDLG_WAIT           = 1,  /* wait for it, give the player back its isCpu */
    STOREDLG_SHOP_OPEN      = 2,  /* the shop-open animation */
    STOREDLG_WAIT_SHOP_OPEN = 3   /* wait for it, then the next opcode */
};

/* Opcodes 0x184 and 0x185: a store dialog for the current player (CPU-driven with
 * RULE_AUTO_SCRIPT), then the shop-open animation; bytes: the opcode's length */
static u32 Script_StoreDialog(ScriptScratch *scratch, void *dialog, int bytes)
{
    Player *pl = &g_players[g_curPlayer];

    switch (g_scriptWait)
    {
    case STOREDLG_START:
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        if (g_boardRules.enable[RULE_AUTO_SCRIPT] != 0)
        {
            scratch->savedIsCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        Task_Start(g_uiTaskIds[7],dialog);
        break;
    case STOREDLG_WAIT:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_SCRIPT] != 0)
        {
            pl->isCpu = scratch->savedIsCpu;
        }
        break;
    case STOREDLG_SHOP_OPEN:
        Task_Start(g_actionTaskIds[0],ShopOpen_Task);
        break;
    case STOREDLG_WAIT_SHOP_OPEN:
        if (Task_IsDone(g_actionTaskIds[0]))
        {
            Script_Advance(bytes);
        }
        return 0;
    default:
        return 0;
    }
    g_scriptWait++;
    return 0;
}

/* Opcode 0x184: open a store on a lot along the route (VacantLot_Task, LOT_PICK) (the operand is
 * not used) (0x80082B94) */
/* 0x80082B94 */
u32 Script_OpBuyLot(ScriptScratch *scratch)
{
    Script_Value(SCRIPT_WORD(2));
    if (g_scriptWait == 0)
    {
        g_uiTaskWork[7][1] = LOT_PICK;
    }
    return Script_StoreDialog(scratch,VacantLot_Task,4);
}

/* Opcode 0x185: percent, kind: sell a store (SellStore_Task: SELL_*, for percent of its value)
 * (0x80082D50) */
/* 0x80082D50 */
u32 Script_OpSellStore(ScriptScratch *scratch)
{
    short percent;
    short kind;

    SCRIPT_READ_OPS(2);
    percent = SCRIPT_ARG(1);
    kind = SCRIPT_ARG(2);
    if (g_scriptWait == 0)
    {
        g_uiTaskWork[7][1] = percent;
        g_uiTaskWork[7][2] = kind;
    }
    return Script_StoreDialog(scratch,SellStore_Task,6);
}

/* Script_OpStoresClose's steps (g_scriptWait) */
enum StoresCloseWait
{
    CLOSE_LIST        =   0,  /* list the stores to close */
    CLOSE_FIRST       =   1,  /* start with the first */
    CLOSE_NEXT        =   2,  /* close the next one (pay its owner), or done */
    CLOSE_CAMERA      =   3,  /* the camera to it */
    CLOSE_SHOW        =   4,  /* the shop-open animation shows it closing */
    CLOSE_WAIT_SHOW   =   5,  /* wait for it, then the next store */
    CLOSE_CAMERA_BACK =   6,  /* (not reached) */
    CLOSE_DONE        = 100   /* the next opcode */
};

/* Opcode 0x192: which, percent, only closed: close the selected stores (Script_ListStores; with
 * the flag only those without goods); the owner gets percent of the value; the camera goes to
 * each store and the shop-open animation shows it closing (0x80082F4C) */
/* 0x80082F4C */
u32 Script_OpStoresClose(ScriptScratch *scratch)
{
    short which;
    short percent;
    short onlyClosed;
    short owner;
    Store *st;

    SCRIPT_READ_OPS(3);
    which = SCRIPT_ARG(1);
    percent = SCRIPT_ARG(2);
    onlyClosed = SCRIPT_ARG(3);
    switch (g_scriptWait)
    {
    case CLOSE_LIST:
        if (which >= -9 && which <= -1)
        {
            owner = (which <= -4) ? -4 - which : which;
            if (onlyClosed == 0)
            {
                Stores_ListByOwner(owner);
            }
            else
            {
                Stores_ListClosed(owner);
            }
        }
        else if (onlyClosed == 0 || g_stores[which].numGoods == 0)
        {
            g_storeListCount = 1;
            g_storeList[0] = which;
        }
        else
        {
            g_storeListCount = 0;
            g_storeList[0] = 0;
        }
        break;
    case CLOSE_FIRST:
        scratch->close.next = 0;
        break;
    case CLOSE_NEXT:
        if (scratch->close.next == g_storeListCount)
        {
            g_scriptWait = CLOSE_DONE;
            return 0;
        }
        scratch->close.store = g_storeList[scratch->close.next];
        st = &g_stores[scratch->close.store];
        scratch->close.squareId = st->squareId;
        scratch->close.x = st->x;
        scratch->close.floor = st->floor;
        Player_Earn(st->owner,Store_GetValue(scratch->close.store) * percent / 100);
        Store_Close(scratch->close.store);
        break;
    case CLOSE_CAMERA:
        if (!Camera_MoveTo(2,CAMERA_CELL_X(scratch->close.x,0x78),
                           CAMERA_CELL_Y(scratch->close.floor,0x7c)))
        {
            return 0;
        }
        break;
    case CLOSE_SHOW:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        break;
    case CLOSE_WAIT_SHOW:
        Menu_DrawFrameCursor(0x9f,0x7c);
        if (Task_IsDone(g_actionTaskIds[1]))
        {
            scratch->close.next++;
            g_scriptWait = CLOSE_NEXT;
        }
        return 0;
    case CLOSE_CAMERA_BACK:                   /* (not reached) */
        if (Camera_MoveTo(1,CAMERA_CELL_X(scratch->close.x,0x78),
                          CAMERA_CELL_Y(scratch->close.floor,0x7c)))
        {
            g_scriptWait = CLOSE_DONE;
        }
        return 0;
    case CLOSE_DONE:
        Script_Advance(8);
        return 0;
    default:
        return 0;
    }
    g_scriptWait++;
    return 0;
}

/* Opcode 0x18B: 0 stops the current player's movement direction (Player.dirX / dirFloor), else
 * reverses it (0x800834D0) */
/* 0x800834D0 */
u32 Script_OpPlayerTurnBack(void)
{
    Player *pl = &g_players[g_curPlayer];

    if (SCRIPT_ARG0 == 0)
    {
        pl->dirX = 0;
        pl->dirFloor = 0;
    }
    else
    {
        pl->dirX = -pl->dirX;
        pl->dirFloor = -pl->dirFloor;
    }
    Script_Advance(4);
    return 1;
}

/* Opcode 0x18C: the selected players can't use the elevator for n months (0x800835B8) */
/* 0x800835B8 */
u32 Script_OpNoElevator(void)
{
    short months;
    short players[BOARD_MAX_PLAYERS];
    int count;
    int i;

    SCRIPT_READ_OPS(2);
    months = SCRIPT_ARG(2);
    count = Script_SelectPlayers(SCRIPT_ARG(1),players);
    for (i = 0; i < count; i++)
    {
        g_players[players[i]].noElevator = 1;
        g_players[players[i]].noElevatorUntil =
             TURN_NUMBER((short)g_gameConfig.monthsElapsed + months,g_curPlayer);
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x193: the selected players wear the "Chrip-Chrip" shoes or not (Player.shoes)
 * (0x80083854) */
/* 0x80083854 */
u32 Script_OpSetShoes(void)
{
    short value;
    short players[BOARD_MAX_PLAYERS];
    int count;
    int i;

    SCRIPT_READ_OPS(2);
    value = SCRIPT_ARG(2);
    count = Script_SelectPlayers(SCRIPT_ARG(1),players);
    for (i = 0; i < count; i++)
    {
        g_players[players[i]].shoes = value;
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x140: var = number of stores of who (Stores_ListByOwner) (0x80085768) */
/* 0x80085768 */
u32 Script_OpCountStores(void)
{
    short count;

    SCRIPT_READ_OPS(2);
    count = Stores_ListByOwner(SCRIPT_ARG(1));
    SCRIPT_VAR_SET(g_scriptOps[2],count);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x141: var = the n-th store of who; out of range the original stores whatever its $s2
 * held (a value of Script_Run's), here 0 (0x800857F0) */
/* 0x800857F0 */
u32 Script_OpGetStore(void)
{
    short n;
    short count;
    u16 store;

    SCRIPT_READ_OPS(3);
    n = SCRIPT_ARG(2);
    count = Stores_ListByOwner(SCRIPT_ARG(1));
    store = 0;
    if (n < count)
    {
        store = g_storeList[n];
    }
    SCRIPT_VAR_SET(g_scriptOps[3],store);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x182: two operands, no effect (0x800858B8) */
/* 0x800858B8 */
u32 Script_OpNop182(void)
{
    Script_Advance(6);
    return 1;
}

/* Opcode 0x183: two operands, no effect (0x800858D8) */
/* 0x800858D8 */
u32 Script_OpNop183(void)
{
    Script_Advance(6);
    return 1;
}

/* Opcode 0x186: two operands, no effect (0x800858F8) */
/* 0x800858F8 */
u32 Script_OpNop186(void)
{
    Script_Advance(6);
    return 1;
}

/* Opcode 0x187: two operands, no effect (0x80085918) */
/* 0x80085918 */
u32 Script_OpNop187(void)
{
    Script_Advance(6);
    return 1;
}

/* Opcode 0x188: the script's result (task user data) = 1 (0x80085938) */
/* 0x80085938 */
u32 Script_OpResult1(void)
{
    Task_SetUserData(-1,EVENT_RESULT_ANOTHER);
    Script_Advance(2);
    return 1;
}

/* Opcode 0x190: result = 2 (0x80085964) */
/* 0x80085964 */
u32 Script_OpResult2(void)
{
    Task_SetUserData(-1,EVENT_RESULT_REROLL);
    Script_Advance(2);
    return 1;
}

/* Opcode 0x189: advance the calendar by n months (0x80085990) */
/* 0x80085990 */
u32 Script_OpAdvanceMonths(void)
{
    Calendar_Advance(SCRIPT_ARG0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x18A: the current player moves n squares (result 3) (0x800859DC) */
/* 0x800859DC */
u32 Script_OpSetSteps(void)
{
    short steps = SCRIPT_ARG0;
    Player *pl = &g_players[g_curPlayer];

    pl->stepsLeft = steps;
    pl->steps = steps;
    Task_SetUserData(-1,EVENT_RESULT_MOVE);
    Script_Advance(4);
    return 1;
}
