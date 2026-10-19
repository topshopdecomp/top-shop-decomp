/*
 * storemenus.c - the store dialogs
 *
 * Shopping in a store, the vacant lot (opening a store there), deliveries of goods, the retail
 * prices, selling a store, the offer to buy a rival's store, and the money windows beside them.
 * Human players use the menus; CPU players decide through src/cpu.c.
 */
#include "common.h"
#include "topshop.h"

/* MoneyWin_Task steps */
enum MoneyWinStep
{
    MONEYWIN_OPEN        =   0,
    MONEYWIN_COUNTING    =  10,  /* the money counts to its new value */
    MONEYWIN_FINISH      =  20,  /* message 0 */
    MONEYWIN_HUMAN_COUNT =  30,  /* a human player: until counted or a button */
    MONEYWIN_HUMAN_SHOW  =  31,  /* 150 frames or a button */
    MONEYWIN_HUMAN_WAIT  =  32,
    MONEYWIN_CPU         =  40,  /* a CPU player: 15 frames */
    MONEYWIN_CPU_UNUSED  =  41,  /* (not used) */
    MONEYWIN_CPU_COUNTED =  42,
    MONEYWIN_CPU_WAIT    =  43,
    MONEYWIN_CLOSE       = 100 
};

/* Window with the current player's money counting to its new value; message 0: finish (a human
 * player may skip with circle / cross), then close (0x8005760C) */
/* 0x8005760C */
u32 MoneyWin_Task(short *step, MoneyWinWork *work)
{
    Player *pl;
    int msg[2];

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = MONEYWIN_FINISH;
    }
    switch(*step)
    {
    case MONEYWIN_OPEN:
        work->active = 1;
        work->win = TextWin_Create(0xa8,0x90,0x10,1);
        TextWin_Open(work->win);
        work->redraw = 1;
        g_players[g_curPlayer].moneyShown = g_players[g_curPlayer].money;
        *step = MONEYWIN_COUNTING;
        break;
    case MONEYWIN_FINISH:
        *step = (g_players[g_curPlayer].isCpu == 0) ? MONEYWIN_HUMAN_COUNT : MONEYWIN_CPU;
        break;
    /* 0x1e-0x20: a human player: until counted or a button, then 150 frames or a button */
    case MONEYWIN_HUMAN_COUNT:
        if ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) == 0 &&
            g_players[g_curPlayer].moneyShown != g_players[g_curPlayer].money) break;
        *step += 1;
        break;
    case MONEYWIN_HUMAN_SHOW:
        g_players[g_curPlayer].moneyShown = g_players[g_curPlayer].money;
        work->redraw = 1;
        work->timer = 0x96;
        *step += 1;
        break;
    case MONEYWIN_HUMAN_WAIT:
        work->timer--;
        if ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) != 0 || work->timer == 0)
        {
            *step = MONEYWIN_CLOSE;
        }
        break;
    /* 0x28-0x2b: a CPU player: 15 frames */
    case MONEYWIN_CPU_UNUSED:               /* not used */
        if (--work->timer == 0)
        {
            *step += 1;
        }
        break;
    case MONEYWIN_CPU_COUNTED:
        g_players[g_curPlayer].moneyShown = g_players[g_curPlayer].money;
        work->redraw = 1;
        /* fall through */
    case MONEYWIN_CPU:
        work->timer = 0xf;
        *step += 1;
        break;
    case MONEYWIN_CPU_WAIT:
        if (--work->timer == 0)
        {
            *step = MONEYWIN_CLOSE;
        }
        break;
    case MONEYWIN_CLOSE:
        TextWin_Close(work->win);
        return 1;
    }
    if (work->active == 0)
    {
        return 0;
    }
    /* the amount shown runs toward the money: down by 0x40 a frame, up by 1 (sic) while more than
     * 0x40 below */
    pl = &g_players[g_curPlayer];
    if (pl->money < pl->moneyShown)
    {
        pl->moneyShown = (pl->money < pl->moneyShown - 0x40) ? pl->moneyShown - 0x40 : pl->money;
        work->redraw = 1;
    }
    else if (pl->money > pl->moneyShown)
    {
        pl->moneyShown = (pl->moneyShown + 0x40 < pl->money) ? pl->moneyShown + 1 : pl->money;
        work->redraw = 1;
    }
    if (work->redraw != 0)
    {
        Str_FormatInt(g_players[g_curPlayer].moneyShown,8,g_msgBuf);
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        work->redraw = 0;
    }
    return 0;
}

/* Shopping_Task steps */
enum ShoppingStep
{
    SHOPPING_INIT           =   0,
    SHOPPING_WAIT_MSG       =   1,
    SHOPPING_LIST           =  10,  /* the store's list of goods (with CostWin_Task) */
    SHOPPING_WAIT_LIST      =  11,
    SHOPPING_HUMAN          =  20,  /* a human player chooses */
    SHOPPING_HUMAN_CHOOSE   =  21,  /* a hint after 300 frames without input */
    SHOPPING_CPU            =  29,  /* Cpu_ChooseItemToBuy */
    SHOPPING_CPU_TARGET     =  30,  /* the cursor down to the last item */
    SHOPPING_CPU_MOVE       =  31,
    SHOPPING_CPU_MOVE_WAIT  =  32,
    SHOPPING_CPU_PICK       =  33,  /* 15 frames */
    SHOPPING_CPU_PICK_WAIT  =  34,
    SHOPPING_CONFIRM        =  40,  /* after the first item: one the player cannot pay? */
    SHOPPING_CONFIRM_ANSWER =  41,
    SHOPPING_BUY            =  43,  /* pay the owner */
    SHOPPING_POINTS         =  45,  /* 65% of the time: points */
    SHOPPING_WAIT_POINTS    =  46,
    SHOPPING_CASH_BACK      =  50,  /* the owner pays twice the price back (k_cashBackChance) */
    SHOPPING_WAIT_CASH_BACK =  51,
    SHOPPING_MIN_BUY        =  60,  /* at least minBuy items */
    SHOPPING_WAIT_MIN_BUY   =  61,
    SHOPPING_MORE           =  70,  /* up to maxBuy on request */
    SHOPPING_MORE_ANSWER    =  71,
    SHOPPING_DECLINE        =  80,  /* "Do not purchase": "Are you sure?" */
    SHOPPING_DECLINE_ANSWER =  81,
    SHOPPING_CLOSE          = 100,
    SHOPPING_WAIT_CLOSE     = 101,
    SHOPPING_WAIT_CAMERA    = 102   /* the camera back to the player */
};

/* Shopping in someone else's store: choose items from its list (a CPU player as many as
 * Cpu_ChooseItemToBuy says), pay the owner, maybe win points and get a cash back (debug name
 * task_shoping_win) (0x80057D20) */
/* 0x80057D20 */
u32 Shopping_Task(short *step, ShoppingWork *work)
{
    Store *st;
    PadState *pad;
    u8 isCpu;
    char maxRank;
    char rank;
    char lo;
    char hi;
    int count;
    int bonus;
    int r;
    int i;

    FntPrint("task_shoping_win\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch(*step)
    {
    case SHOPPING_INIT:
        work->arrived = 1;
        work->minBuy = 1;
        work->listShown = 0;
        work->camMode = 0;
        work->refresh = 0;
        work->costDirty = 0;
        work->bought = 0;
        work->maxBuy = g_gameConfig.maxBuy;
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 "%CPlease shop for an item.");
        *step += 1;
        break;
    case SHOPPING_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = SHOPPING_LIST;
        }
        break;
    /* 10, 0xb: the store's list of goods (with CostWin_Task) */
    case SHOPPING_LIST:
        work->store = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        st = &g_stores[work->store];
        if (st->numGoods == 0)
        {
            Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                     0x96,MSG_ANY_BUTTON,"%CThis store is out of stock.%E");
            *step = SHOPPING_CLOSE;
            break;
        }
        work->listShown = 1;
        work->camMode = 2;
        work->canDecline = 0;
        work->x = st->x;
        work->refresh = 1;
        work->floor = st->floor;
        work->win = TextWin_Create(0x5c,0x18,0x1c,st->maxGoods + 1);
        TextWin_Open(work->win);
        TextWin_SetMenu(work->win,1,st->numGoods,st->numGoods - 1);
        g_uiTaskWork[3][1] = 0;
        Task_Start(g_uiTaskIds[3],CostWin_Task);
        *step += 1;
        break;
    case SHOPPING_WAIT_LIST:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            *step = (g_players[g_curPlayer].isCpu == 0) ? SHOPPING_HUMAN : SHOPPING_CPU;
            TextWin_SetFocus(work->win,1);
        }
        break;
    /* 0x14, 0x15: a human player chooses; a hint after 300 frames without input */
    case SHOPPING_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case SHOPPING_HUMAN_CHOOSE:
        pad = &g_playerPads[g_curPlayer];
        if (pad->pressed & PADRdown)
        {
            if (work->canDecline == 0 ||
                g_textWins[work->win].menuCursor != g_textWins[work->win].menuCount - 1)
            {
                work->cursor = TextWin_GetCursor(work->win);
                *step = SHOPPING_CONFIRM;
            }
            else
            {
                *step = SHOPPING_DECLINE;         /* "Do not purchase" */
            }
            Se_Play(SE_OK,0x7f,0x7f,1);
            break;
        }
        if (pad->pressed & PADRright)
        {
            if (work->canDecline != 0)
            {
                /* circle: to "Do not purchase" */
                g_textWins[work->win].menuCursor = g_textWins[work->win].menuCount - 1;
                work->costDirty = 1;
                *step = SHOPPING_DECLINE;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                TextWin_CursorUp(work->win);
            }
            else
            {
                TextWin_CursorDown(work->win);
            }
            work->cursor = TextWin_GetCursor(work->win);
            work->costDirty = 1;
            work->hintTimer = 300;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CPlease select a item%K you wish to buy.");
        *step = SHOPPING_HUMAN;
        break;
    /* 0x1d-0x22: a CPU player moves the cursor down to the last item and takes it */
    case SHOPPING_CPU:
        work->cpuChoice = Cpu_ChooseItemToBuy(g_curPlayer,work->store);
        *step += 1;
        break;
    case SHOPPING_CPU_TARGET:
        work->cursor = g_stores[work->store].numGoods - 1;
        *step += 1;
        break;
    case SHOPPING_CPU_MOVE:
        if (TextWin_GetCursor(work->win) == work->cursor)
        {
            *step = SHOPPING_CPU_PICK;
            break;
        }
        TextWin_CursorDown(work->win);
        work->delay = 2;
        work->costDirty = 1;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case SHOPPING_CPU_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step = SHOPPING_CPU_MOVE;
        }
        break;
    case SHOPPING_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case SHOPPING_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = SHOPPING_CONFIRM;
        }
        break;
    /* 0x28, 0x29: after the first item, confirm one the player cannot pay */
    case SHOPPING_CONFIRM:
        if (work->bought != 0 &&
            g_players[g_curPlayer].money < g_stores[work->store].goods[work->cursor].price)
        {
            Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                         MSG_AUTO_SIZE,"%CWill you purchase these items?%KYes%KNo%E");
            TextWin_SetFocus(work->win,0);
            *step += 1;
            break;
        }
        *step = SHOPPING_BUY;
        break;
    case SHOPPING_CONFIRM_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        if (g_msgAnswer == 0)
        {
            *step = SHOPPING_BUY;
            break;
        }
        *step = (g_players[g_curPlayer].isCpu != 0) ? SHOPPING_CPU_TARGET : SHOPPING_HUMAN;
        break;
    /* 0x2b: buy it */
    case SHOPPING_BUY:
        st = &g_stores[work->store];
        work->level = st->goods[work->cursor].level;
        work->paid = st->goods[work->cursor].price;
        Player_PayTo(g_curPlayer,st->owner,work->paid);
        st->goods[work->cursor].active = 0;
        Store_SortGoods(work->store);
        work->refresh = 1;
        work->bought++;
        g_players[g_curPlayer].purchases++;
        g_players[st->owner].customers++;
        *step = SHOPPING_POINTS;
        break;
    /* 0x2d, 0x2e: 65% of the time, points: about paid / 200, more for the leader (value rank) */
    case SHOPPING_POINTS:
        maxRank = 0;
        for (i = 0; i < g_numPlayers; i++)
        {
            if (maxRank < (char)g_players[i].valueRank)
            {
                maxRank = g_players[i].valueRank;
            }
        }
        r = rand();
        bonus = work->paid / 200;
        rank = g_players[g_curPlayer].valueRank;
        if (rank == maxRank || rank > 2)
        {
            bonus += 1;
        }
        else if (rank == 1)
        {
            bonus -= 1;
        }
        else if (rank != 2)
        {
            bonus -= 2;
        }
        /* between bonus - 4 and bonus, at least 1 */
        hi = bonus;
        lo = (bonus - 4);
        if (hi < 1)
        {
            hi = 1;
        }
        if (lo < 1)
        {
            lo = 1;
        }
        if ((char)(r % 100) <= 64)
        {
            if (lo == hi)
            {
                work->points = lo;
            }
            else
            {
                work->points = lo + (rand() % (hi - lo + 1));
            }
        }
        else
        {
            work->points = 0;
        }
        if (work->points == 0)
        {
            *step = SHOPPING_CASH_BACK;
            break;
        }
        strcpy(g_msgBuf,"%L");
        strcat(g_msgBuf,"Received ");
        Str_FormatInt(work->points,0,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf," point.");
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,g_msgBuf);
        if (g_players[g_curPlayer].points + work->points > 99)
        {
            g_players[g_curPlayer].points = 99;
        }
        else
        {
            g_players[g_curPlayer].points += work->points;
        }
        *step += 1;
        break;
    case SHOPPING_WAIT_POINTS:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = SHOPPING_CASH_BACK;
        break;
    /* 0x32, 0x33: the cash back (k_cashBackChance by level): the owner pays twice the price back */
    case SHOPPING_CASH_BACK:
        if (k_cashBackChance[work->level] <= rand() % 100)
        {
            g_players[g_curPlayer].shopHistory |= 1;
            g_turnFlags[TURN_SHOPPED] = 1;
            if (g_players[g_curPlayer].money / 5 <= work->paid)
            {
                g_turnFlags[TURN_BIG_PURCHASE] = 1;
            }
            *step = SHOPPING_MIN_BUY;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CReceived cash back.%E");
        g_turnFlags[TURN_CASH_BACK] = 1;
        *step += 1;
        break;
    case SHOPPING_WAIT_CASH_BACK:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        Player_PayTo(g_stores[work->store].owner,g_curPlayer,work->paid * 2);
        Store_SortGoods(work->store);
        *step = SHOPPING_MIN_BUY;
        break;
    /* 0x3c-0x47: at least minBuy items; up to maxBuy on request */
    case SHOPPING_MIN_BUY:
        if (work->minBuy <= work->bought || g_stores[work->store].numGoods == 0)
        {
            *step = SHOPPING_MORE;
            break;
        }
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CPlease buy another.%E");
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case SHOPPING_WAIT_MIN_BUY:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = (g_players[g_curPlayer].isCpu != 0) ? SHOPPING_CPU_TARGET : SHOPPING_HUMAN;
        break;
    case SHOPPING_MORE:
        if (work->maxBuy <= work->bought || g_stores[work->store].numGoods == 0)
        {
            *step = SHOPPING_CLOSE;
            break;
        }
        /* a CPU player buys the number of items it chose */
        isCpu = g_players[g_curPlayer].isCpu;
        Msg_AskYesNo(isCpu,(isCpu != 0) && (work->bought >= work->cpuChoice),MSG_AUTO_POS,MSG_AUTO_POS,
                     MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%CPurchase another item?%KYes%KNo%E");
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case SHOPPING_MORE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        if (g_msgAnswer != 0)
        {
            *step = SHOPPING_CLOSE;
            break;
        }
        work->refresh = 1;
        work->canDecline = 1;
        *step = (g_players[g_curPlayer].isCpu != 0) ? SHOPPING_CPU_TARGET : SHOPPING_HUMAN;
        break;
    /* 0x50, 0x51: "Do not purchase" */
    case SHOPPING_DECLINE:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CAre you sure?%KYes%KNo%E");
        *step += 1;
        break;
    case SHOPPING_DECLINE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        *step = (g_msgAnswer == 0) ? SHOPPING_CLOSE : SHOPPING_HUMAN;
        break;
    /* 100-0x66: close, the camera back to the player */
    case SHOPPING_CLOSE:
        work->camMode = 1;
        TextWin_SetFocus(work->win,0);
        Task_SendMessage(g_uiTaskIds[4],0);
        Task_SendMessage(g_uiTaskIds[3],0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case SHOPPING_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->listShown = 0;
        *step += 1;
        break;
    case SHOPPING_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }

    /* the list: "Item Name   Price", then name and price of each item */
    if (work->refresh != 0 && work->listShown != 0)
    {
        st = &g_stores[work->store];
        strcpy(g_msgBuf,"%LItem Name          Price%K");
        for (i = 0; i < st->numGoods; i++)
        {
            strcat(g_msgBuf,st->type->items[st->goods[i].level].name);
            Str_FillSpaces(22 - strlen(st->type->items[st->goods[i].level].name),
                           g_msgBuf + strlen(g_msgBuf));
            Str_FormatInt(st->goods[i].price,6,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"%K");
        }
        if (work->canDecline != 0)
        {
            strcat(g_msgBuf,"Do not purchase");
        }
        strcat(g_msgBuf,"%E");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        count = st->numGoods;
        TextWin_SetMenu(work->win,1,(work->canDecline != 0) ? count + 1 : count,count - 1);
        work->cursor = TextWin_GetCursor(work->win);
        work->costDirty = 1;
        work->refresh = 0;
    }
    /* the cost window: the player's money and the selected item's price */
    if (work->costDirty != 0 && work->listShown != 0)
    {
        g_uiTaskWork[3][1] = 1;
        g_uiTaskWork[3][2] = g_players[g_curPlayer].money;
        if (g_textWins[work->win].menuCursor == g_stores[work->store].numGoods)
        {
            g_uiTaskWork[3][3] = 0;
        }
        else
        {
            work->level = g_stores[work->store].goods[work->cursor].level;
            g_uiTaskWork[3][3] = g_stores[work->store].goods[work->cursor].price;
        }
        g_uiTaskWork[3][4] = 1;
        work->costDirty = 0;
    }
    work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    if (work->camMode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x2f,0x46);
    }
    return 0;
}

/* Select the lot at index work->lot of g_storeList (LOT_PICK*) */
static void VacantLot_SelectLot(VacantLotWork *work)
{
    work->store = g_storeList[work->lot];
    work->squareId = g_stores[work->store].squareId;
    work->x = g_stores[work->store].x;
    work->floor = g_stores[work->store].floor;
    work->hintTimer = 300;
    work->refresh = 1;
    Se_Play(SE_CURSOR,0x7f,0x7f,1);
}

/* The store type in the list's selected row (-1 on the last row) */
static void VacantLot_UpdateChoice(VacantLotWork *work)
{
    work->choice = TextWin_GetCursor(work->win);
    work->storeType = (work->choice == work->numTypes) ? -1 : work->types[work->choice];
}

/* VacantLot_Task steps */
enum VacantLotStep
{
    VLOT_INIT                  =   0,
    VLOT_WAIT_MSG              =   1,
    VLOT_LIST                  =  10,  /* the lot (here, or the first of the new squares') and the list window */
    VLOT_WAIT_LIST             =  11,
    VLOT_START                 =  15,
    VLOT_HUMAN                 =  20,  /* a human player chooses */
    VLOT_HUMAN_CHOOSE          =  21,  /* a hint after 300 frames without input */
    VLOT_CPU                   =  30,  /* a CPU player: Cpu_ChooseStoreToOpen, the cursor down to it */
    VLOT_CPU_MOVE              =  31,
    VLOT_CPU_MOVE_WAIT         =  32,
    VLOT_CPU_PICK              =  33,  /* 15 frames */
    VLOT_CPU_PICK_WAIT         =  34,
    VLOT_PAY                   =  40,  /* enough money (unless free or the rule says so)? */
    VLOT_CONFIRM               =  50,  /* "Are you sure?" */
    VLOT_CONFIRM_ANSWER        =  51,
    VLOT_OPEN                  =  52,  /* pay and open it */
    VLOT_NO_MONEY              =  60,  /* "Insufficient money." */
    VLOT_EXISTS                =  70,  /* "Store already exists." (not used) */
    VLOT_DECLINE               =  80,  /* "Are you sure?" */
    VLOT_DECLINE_ANSWER        =  81,
    VLOT_NONE_LEFT             =  85,  /* "No stores left for sale." */
    VLOT_WAIT_NONE_LEFT        =  86,
    VLOT_ITEMS                 =  90,  /* the goods of the store type (ItemList_Task) until a button */
    VLOT_ITEMS_WAIT_BUTTON     =  91,
    VLOT_ITEMS_CLOSE           =  92,
    VLOT_CLOSE                 = 100,
    VLOT_CLOSE_LIST            = 101,
    VLOT_WAIT_CLOSE            = 102,
    VLOT_WAIT_CAMERA           = 103,  /* the camera back to the player */
    VLOT_CPU_DECLINE           = 130,  /* a CPU player declines: the cursor to the last row */
    VLOT_CPU_DECLINE_MOVE      = 131,
    VLOT_CPU_DECLINE_MOVE_WAIT = 132,
    VLOT_CPU_DECLINE_PICK      = 133,
    VLOT_CPU_DECLINE_PICK_WAIT = 134 
};

/* The vacant lot dialog (kind: LOT_*): choose one of the square's store types (L1 / R1 show its
 * goods) and pay, or decline (0x800598DC) */
/* 0x800598DC */
u32 VacantLot_Task(short *step, VacantLotWork *work)
{
    Store *st;
    PadState *pad;
    char *row;
    int onlyOfType;
    int type;
    int j;

    switch(*step)
    {
    case VLOT_INIT:
        work->win = -1;
        work->listShown = 0;
        work->camMode = 0;
        work->arrived = 1;
        work->refresh = 0;
        work->costDirty = 0;
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 (work->kind == LOT_VACANT || work->kind == LOT_PICK || work->kind == LOT_PICK_FREE)
                     ? "%CPlease select a store%K you wish to open." : "%CDo you wish to buy store?");
        *step += 1;
        break;
    case VLOT_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = VLOT_LIST;
        }
        break;
    /* 10, 0xb: the lot (the one here, or the first of Stores_ListVacantNewSquares) and the list window */
    case VLOT_LIST:
        if (work->kind == LOT_PICK || work->kind == LOT_PICK_FREE)
        {
            Stores_ListVacantNewSquares(g_curPlayer);
            if (g_storeListCount == 0)
            {
                work->camMode = 0;
                work->refresh = 0;
                *step = VLOT_NONE_LEFT;
                break;
            }
            work->lot = 0;
            work->store = g_storeList[0];
        }
        else
        {
            work->store = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        }
        work->listShown = 1;
        work->refresh = 1;
        work->camMode = 2;
        work->squareId = g_stores[work->store].squareId;
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
        work->win = TextWin_Create(0x5c,0x18,0x1c,6);
        TextWin_Open(work->win);
        g_uiTaskWork[3][1] = 0;
        Task_Start(g_uiTaskIds[3],CostWin_Task);
        *step += 1;
        break;
    case VLOT_WAIT_LIST:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            TextWin_SetFocus(work->win,1);
            *step = VLOT_START;
        }
        break;
    case VLOT_START:
        if ((work->kind == LOT_PICK || work->kind == LOT_PICK_FREE) && g_storeListCount == 0)
        {
            *step = VLOT_NONE_LEFT;
        }
        else if (g_players[g_curPlayer].isCpu == 0)
        {
            *step = VLOT_HUMAN;
        }
        else if (work->kind == LOT_PICK || work->kind == LOT_PICK_FREE)
        {
            *step = VLOT_CPU_DECLINE;            /* a CPU player declines these */
        }
        else
        {
            *step = VLOT_CPU;
        }
        break;
    /* 0x14, 0x15: a human player chooses; a hint after 300 frames without input */
    case VLOT_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case VLOT_HUMAN_CHOOSE:
        pad = &g_playerPads[g_curPlayer];
        if (pad->pressed & PADRdown)
        {
            *step = (work->storeType == -1) ? VLOT_DECLINE : VLOT_PAY;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (pad->pressed & PADRright)
        {
            /* circle: to the last row */
            g_textWins[work->win].menuCursor = work->numTypes;
            work->costDirty = 1;
            *step = VLOT_DECLINE;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (pad->pressed & (PADL1 | PADR1))
        {
            /* the goods of the store type */
            if (work->storeType != -1)
            {
                *step = VLOT_ITEMS;
                Se_Play(SE_OK,0x7f,0x7f,1);
            }
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                TextWin_CursorUp(work->win);
            }
            else
            {
                TextWin_CursorDown(work->win);
            }
            work->hintTimer = 300;
            work->costDirty = 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (work->kind == LOT_PICK || work->kind == LOT_PICK_FREE)
        {
            /* left / right: the previous / next lot of the list */
            if (g_playerPads[g_curPlayer].repeat & PADLleft)
            {
                work->lot = ((work->lot == 0) ? g_storeListCount : work->lot) - 1;
                VacantLot_SelectLot(work);
            }
            if (g_playerPads[g_curPlayer].repeat & PADLright)
            {
                if (work->lot == g_storeListCount - 1)
                {
                    work->lot = 0;
                }
                else
                {
                    work->lot++;
                }
                VacantLot_SelectLot(work);
            }
        }
        VacantLot_UpdateChoice(work);
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CPlease select a store%K you wish to open.");
        *step = VLOT_HUMAN;
        break;
    /* 0x1e-0x22 (0x82-0x86: to the last row): a CPU player moves the cursor down to its choice */
    case VLOT_CPU:
        if (work->kind == LOT_BUYOUT)
        {
            work->cpuChoice = Cpu_ChooseStoreToOpen(g_curPlayer,work->store,(u32)work->types,work->prices,
                                                    work->numTypes,work->ownerGain,
                                                    g_stores[work->store].owner);
        }
        else
        {
            work->cpuChoice = Cpu_ChooseStoreToOpen(g_curPlayer,work->store,(u32)work->types,work->prices,
                                                    work->numTypes,0,0);
        }
        *step += 1;
        break;
    case VLOT_CPU_DECLINE:
        work->cpuChoice = work->numTypes;
        *step += 1;
        break;
    case VLOT_CPU_MOVE:
    case VLOT_CPU_DECLINE_MOVE:
        if (TextWin_GetCursor(work->win) == work->cpuChoice)
        {
            *step += 2;
            break;
        }
        TextWin_CursorDown(work->win);
        work->delay = 4;
        work->costDirty = 1;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case VLOT_CPU_MOVE_WAIT:
    case VLOT_CPU_DECLINE_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step -= 1;
        }
        break;
    case VLOT_CPU_PICK:
    case VLOT_CPU_DECLINE_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case VLOT_CPU_PICK_WAIT:
    case VLOT_CPU_DECLINE_PICK_WAIT:
        if (--work->delay != 0) break;
        Se_Play(SE_OK,0x7f,0x7f,1);
        work->choice = work->cpuChoice;
        if (work->cpuChoice != work->numTypes)
        {
            work->storeType = work->types[work->cpuChoice];
            *step = VLOT_PAY;
            break;
        }
        work->storeType = -1;
        *step = VLOT_DECLINE;
        break;
    /* 0x28-0x34: pay and open it (enough money needed, unless free or the rule says so) */
    case VLOT_PAY:
        if (work->kind == LOT_PICK_FREE || g_boardRules.enable[RULE_FREE_BUYING] != 0 ||
            work->prices[work->choice] <= g_players[g_curPlayer].money)
        {
            *step = VLOT_CONFIRM;
        }
        else
        {
            *step = VLOT_NO_MONEY;
        }
        break;
    case VLOT_CONFIRM:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CAre you sure?%KYes%KNo%E");
        *step += 1;
        break;
    case VLOT_CONFIRM_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = (g_msgAnswer == 0) ? *step + 1 : VLOT_HUMAN;
            TextWin_SetFocus(work->win,1);
        }
        break;
    case VLOT_OPEN:
        if (work->kind == LOT_BUYOUT)
        {
            g_turnFlags[TURN_BOUGHT_STORE] = 1;
            Player_Earn(g_stores[work->store].owner,work->buyPrice);
            Store_Close(work->store);
        }
        else
        {
            g_turnFlags[TURN_OWN_STORE] = 1;
        }
        Store_Open(work->store,work->storeType,g_curPlayer);
        Player_Pay(g_curPlayer,work->prices[work->choice]);
        *step = VLOT_CLOSE;
        break;
    case VLOT_NO_MONEY:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CInsufficient money.%E");
        *step = VLOT_HUMAN;
        break;
    case VLOT_EXISTS:                       /* not used */
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CStore already exists.%E");
        *step = VLOT_HUMAN;
        break;
    /* 0x50, 0x51: decline */
    case VLOT_DECLINE:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CAre you sure?%KYes%KNo%E");
        *step += 1;
        break;
    case VLOT_DECLINE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = (g_msgAnswer == 0) ? VLOT_CLOSE : VLOT_HUMAN;
            TextWin_SetFocus(work->win,1);
            /* (whatever the answer) could it have paid one of them? */
            for (j = 0; j < work->numTypes; j++)
            {
                if (work->prices[j] <= g_players[g_curPlayer].money) break;
            }
            g_turnFlags[(j == work->numTypes) ? TURN_LOT_UNAFFORDABLE : TURN_LOT_DECLINED] = 1;
        }
        break;
    case VLOT_NONE_LEFT:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CNo stores left for sale.%E");
        *step += 1;
        break;
    case VLOT_WAIT_NONE_LEFT:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        *step = VLOT_CLOSE;
        break;
    /* 0x5a-0x5c: the goods of the selected store type (ItemList_Task) until a button */
    case VLOT_ITEMS:
        g_uiTaskWork[5][1] = 100;
        g_uiTaskWork[5][2] = 0x20;
        g_uiTaskWork[5][3] = work->storeType;
        g_uiTaskWork[5][4] = 6 - k_chainLevels[0];
        Task_Start(g_uiTaskIds[5],ItemList_Task);
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case VLOT_ITEMS_WAIT_BUTTON:
        if ((g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON) == 0) break;
        Se_Play(SE_OK,0x7f,0x7f,1);
        Task_SendMessage(g_uiTaskIds[5],0);
        TextWin_SetFocus(work->win,1);
        *step += 1;
        break;
    case VLOT_ITEMS_CLOSE:
        if (Task_IsDone(g_uiTaskIds[5]) == 0) break;
        *step = VLOT_HUMAN;
        break;
    /* 100-0x67: close, the camera back to the player */
    case VLOT_CLOSE:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        *step += 1;
        break;
    case VLOT_CLOSE_LIST:
        work->camMode = 1;
        Task_SendMessage(g_uiTaskIds[3],0);
        if (work->win == -1)
        {
            *step = VLOT_WAIT_CAMERA;
            break;
        }
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case VLOT_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->listShown = 0;
        *step += 1;
        break;
    case VLOT_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }

    /* the list: the square's available store types (buying out a store that is its owner's only
     * one of its type: that type too) with their prices, text palette 1 (%P01) when too expensive */
    if (work->refresh != 0 && work->listShown != 0)
    {
        st = &g_stores[work->store];
        strcpy(g_msgBuf,"%LStore Name         Price%K");
        work->numTypes = 0;
        onlyOfType = (work->kind == LOT_BUYOUT) &&
                     (Stores_CountOwnedOfType(st->owner,(char)st->storeType) == 1);
        for (j = 0; j < 4; j++)
        {
            type = work->squareId * 4 + j;
            if (g_storeTypes[(short)type].available == 0 &&
                !(onlyOfType && (char)st->storeType == (short)type))
            {
                continue;
            }
            work->types[work->numTypes] = type;
            if (work->kind == LOT_VACANT || work->kind == LOT_PICK)
            {
                work->buyPrice = 0;
                work->prices[work->numTypes] = StoreType_GetPrice(type,0);
            }
            else if (work->kind == LOT_PICK_FREE)
            {
                work->buyPrice = 0;
                work->ownerGain = 0;
                work->prices[work->numTypes] = 0;
            }
            else
            {
                /* the store's value times 1.5, 2, 2.5 or 3 by the length of its chain */
                switch (st->chainLen)
                {
                case 0:
                    work->buyPrice = (st->type->value * 3) / 2;
                    break;
                case 1:
                    work->buyPrice = st->type->value * 2;
                    break;
                case 2:
                    work->buyPrice = (st->type->value * 5) / 2;
                    break;
                case 3:
                    work->buyPrice = st->type->value * 3;
                    break;
                }
                work->ownerGain = work->buyPrice - st->type->value;
                work->prices[work->numTypes] = work->buyPrice + StoreType_GetPrice(type,0);
            }
            work->numTypes++;
        }
        for (j = 0; j < work->numTypes; j++)
        {
            strcat(g_msgBuf,(g_players[g_curPlayer].money < work->prices[j]) ? "%P01" : "%P00");
            row = g_storeTypes[(char)work->types[j]].name;
            strcat(g_msgBuf,row);
            Str_FillSpaces(22 - strlen(row),g_msgBuf + strlen(g_msgBuf));
            Str_FormatInt(work->prices[j],6,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"%K");
        }
        strcat(g_msgBuf,(work->kind == LOT_VACANT || work->kind == LOT_PICK) ? "%P00Do not open"
                                                                             : "%P00Cancel purchase");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_SetMenu(work->win,1,work->numTypes + 1,0);
        VacantLot_UpdateChoice(work);
        work->costDirty = 1;
        work->refresh = 0;
    }
    /* the cost window: the player's money and the selected row's price */
    if (work->costDirty != 0 && work->listShown != 0)
    {
        g_uiTaskWork[3][1] = 1;
        g_uiTaskWork[3][2] = g_players[g_curPlayer].money;
        if (g_textWins[work->win].menuCursor == g_textWins[work->win].menuCount - 1)
        {
            g_uiTaskWork[3][3] = 0;
        }
        else
        {
            g_uiTaskWork[3][3] = work->prices[g_textWins[work->win].menuCursor];
        }
        g_uiTaskWork[3][4] = 1;
        work->costDirty = 0;
    }
    work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    if (work->camMode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x2f,0x46);
    }
    return 0;
}

/* ItemList_Task steps */
enum ItemListStep
{
    ITEMLIST_OPEN       =   0,  /* the goods and their prices */
    ITEMLIST_WAIT_OPEN  =   1,
    ITEMLIST_SHOWN      =  10,  /* until message 0 */
    ITEMLIST_CLOSE      = 100,
    ITEMLIST_WAIT_CLOSE = 101 
};

/* Window listing the goods of store type `type` and their costs, from level `limit` on in
 * palette 0 (the more expensive ones above it in palette 2) (0x8005B434) */
/* 0x8005B434 */
u32 ItemList_Task(u16 *step, ItemListWork *work)
{
    StoreItem *item;
    int msg[2];
    int level;

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = ITEMLIST_CLOSE;
    }
    switch (*step)
    {
    case ITEMLIST_OPEN:
        work->busy = 1;
        strcpy(g_msgBuf,"%LItem Name          Price%K");
        strcat(g_msgBuf,"%P02");
        for (level = 0; level < 6; level++)
        {
            item = &g_storeTypes[work->type].items[level];
            if (work->limit <= level)
            {
                strcat(g_msgBuf,"%P00");
            }
            strcat(g_msgBuf,item->name);
            Str_FillSpaces(22 - strlen(item->name),g_msgBuf + strlen(g_msgBuf));
            Str_FormatInt(item->cost,6,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"%K");
        }
        strcat(g_msgBuf,"%E");
        work->win = TextWin_Create(work->x,work->y,0x1c,7);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        *step += 1;
        break;
    case ITEMLIST_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            TextWin_SetFocus(work->win,1);
            *step = ITEMLIST_SHOWN;
        }
        break;
    case ITEMLIST_CLOSE:
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case ITEMLIST_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->busy = 0;
            return 1;
        }
        break;
    }
    return 0;
}

/* Delivery_Task steps */
enum DeliveryStep
{
    DELIV_INIT                    =   0,
    DELIV_WAIT_MSG                =   1,
    DELIV_LIST                    =  10,  /* the list window, the CPU player's choice, the cost window */
    DELIV_WAIT_LIST               =  11,
    DELIV_NEXT                    =  20,  /* the next delivery, unless the store is full */
    DELIV_FULL                    =  21,  /* "Store has full stock." */
    DELIV_WAIT_FULL               =  22,
    DELIV_CHOOSE                  =  30,
    DELIV_HUMAN                   =  40,  /* a human player chooses (L1 / R1: the retail prices) */
    DELIV_HUMAN_CHOOSE            =  41,  /* a hint after 300 frames without input */
    DELIV_PRICES                  =  45,  /* the store's retail prices until a button */
    DELIV_PRICES_WAIT_BUTTON      =  46,
    DELIV_PRICES_CLOSE            =  47,
    DELIV_CPU                     =  50,  /* a CPU player: the cursor to its next choice (or the end) */
    DELIV_CPU_MOVE                =  51,
    DELIV_CPU_MOVE_WAIT           =  52,
    DELIV_CPU_PICK                =  53,  /* 15 frames */
    DELIV_CPU_PICK_WAIT           =  54,
    DELIV_DELIVER                 =  60,  /* the first empty goods slot gets the level */
    DELIV_SHOW_PRICES             =  70,  /* the retail prices until a button */
    DELIV_SHOW_PRICES_WAIT_BUTTON =  71,
    DELIV_SHOW_PRICES_CPU         =  75,  /* a CPU player: 30 frames */
    DELIV_SHOW_PRICES_CPU_WAIT    =  76,
    DELIV_SHOW_PRICES_CLOSE       =  78,
    DELIV_SHOW_PRICES_WAIT_CLOSE  =  79,
    DELIV_FINISH                  =  80,  /* "Finish delivery?" */
    DELIV_FINISH_ANSWER           =  81,
    DELIV_NO_MONEY                =  90,  /* "Insufficient money." */
    DELIV_WAIT_NO_MONEY           =  91,
    DELIV_CLOSE                   = 100,
    DELIV_WAIT_CLOSE              = 101,
    DELIV_WAIT_CAMERA             = 102   /* the camera back to the player */
};

/* Delivering goods to an own store: choose levels from the list (the more expensive ones need a
 * longer chain), each paid for unless DELIVERY_FREE, until "Complete Delivery" (debug name
 * buyingin win) (0x8005B788) */
/* 0x8005B788 */
u32 Delivery_Task(short *step, DeliveryWork *work)
{
    Store *st;
    PadState *pad;
    char *name;
    int level;
    int cost;
    int i;

    FntPrint("exec task buyingin win\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch(*step)
    {
    case DELIV_INIT:
        work->arrived = 1;
        work->listShown = 0;
        work->camMode = 0;
        work->rebuild = 0;
        work->costDirty = 0;
        strcpy(g_msgBuf,"%CDelivery will be made to%K\201w");
        strcat(g_msgBuf,g_stores[work->store].type->name);
        strcat(g_msgBuf,"%C\201x.");
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 g_msgBuf);
        *step += 1;
        break;
    case DELIV_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = DELIV_LIST;
        }
        break;
    /* 10, 0xb: the list window, the CPU player's choice, the cost window */
    case DELIV_LIST:
        st = &g_stores[work->store];
        work->listShown = 1;
        work->camMode = 2;
        work->x = st->x;
        work->floor = st->floor;
        work->storeType = st->storeType;
        work->numLevels = k_chainLevels[st->chainLen];
        work->firstLevel = 6 - work->numLevels;
        work->win = TextWin_Create(0x5c,0x18,0x1c,work->numLevels + 1);
        TextWin_Open(work->win);
        work->rebuild = 2;
        work->cpuIndex = 0;
        work->cpuCount = Cpu_ChooseDelivery(g_curPlayer,work->store,work->cpuLevels);
        g_uiTaskWork[3][1] = 0;
        Task_Start(g_uiTaskIds[3],CostWin_Task);
        *step += 1;
        break;
    case DELIV_WAIT_LIST:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN) break;
        TextWin_SetFocus(work->win,1);
        *step = DELIV_NEXT;
        break;
    /* 0x14-0x1e: next delivery, unless the store is full */
    case DELIV_NEXT:
        if (g_stores[work->store].numGoods != g_stores[work->store].maxGoods)
        {
            *step = DELIV_CHOOSE;
            break;
        }
        *step += 1;
        break;
    case DELIV_FULL:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CStore has full stock.%E");
        *step += 1;
        break;
    case DELIV_WAIT_FULL:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            TextWin_SetFocus(work->win,1);
            *step = DELIV_FINISH;
        }
        break;
    case DELIV_CHOOSE:
        *step = (g_players[g_curPlayer].isCpu != 0) ? DELIV_CPU : DELIV_HUMAN;
        break;
    /* 0x28, 0x29: a human player chooses (L1 / R1: the retail prices); a hint after 300 frames */
    case DELIV_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case DELIV_HUMAN_CHOOSE:
        work->cursor = TextWin_GetCursor(work->win);
        work->level = work->firstLevel + work->cursor;
        pad = &g_playerPads[g_curPlayer];
        if (pad->pressed & PADRdown)
        {
            st = &g_stores[work->store];
            if (work->cursor == work->numLevels)
            {
                *step = DELIV_FINISH;             /* "Complete Delivery" */
            }
            else if (st->numGoods == g_kindMaxGoods[st->chainLen])
            {
                *step = DELIV_NEXT;               /* full */
            }
            else if (work->kind != DELIVERY_FREE &&
                     g_players[g_curPlayer].money < st->type->items[work->firstLevel + work->cursor].cost)
            {
                *step = DELIV_NO_MONEY;
            }
            else
            {
                *step = DELIV_DELIVER;
            }
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (pad->pressed & PADRright)
        {
            /* circle: to "Complete Delivery" */
            g_textWins[work->win].menuCursor = work->numLevels;
            work->costDirty = 1;
            work->cursor = work->numLevels;
            *step = DELIV_FINISH;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (pad->pressed & (PADL1 | PADR1))
        {
            *step = DELIV_PRICES;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                TextWin_CursorUp(work->win);
            }
            else
            {
                TextWin_CursorDown(work->win);
            }
            work->hintTimer = 300;
            work->costDirty = 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CPlease select a item%K you want to deliver.");
        *step = DELIV_HUMAN;
        break;
    /* 0x2d-0x2f: the store's retail prices until a button */
    case DELIV_PRICES:
        TextWin_SetFocus(work->win,0);
        g_uiTaskWork[6][1] = 100;
        g_uiTaskWork[6][2] = 0x20;
        g_uiTaskWork[6][3] = work->store;
        Task_Start(g_uiTaskIds[6],RetailPrices_Task);
        *step += 1;
        break;
    case DELIV_PRICES_WAIT_BUTTON:
        if ((g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON) == 0) break;
        Se_Play(SE_CANCEL,0x7f,0x7f,1);
        Task_SendMessage(g_uiTaskIds[6],0);
        *step += 1;
        break;
    case DELIV_PRICES_CLOSE:
        if (Task_IsDone(g_uiTaskIds[6]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = DELIV_HUMAN;
        break;
    /* 0x32-0x36: a CPU player moves the cursor down to its next choice (or to the end) */
    case DELIV_CPU:
        if (work->cpuIndex == work->cpuCount)
        {
            work->cursor = work->numLevels;
        }
        else
        {
            work->level = work->cpuLevels[work->cpuIndex];
            work->cursor = work->level - work->firstLevel;
        }
        *step += 1;
        break;
    case DELIV_CPU_MOVE:
        if (g_textWins[work->win].menuCursor == work->cursor)
        {
            *step = DELIV_CPU_PICK;
            break;
        }
        g_textWins[work->win].menuCursor++;
        work->delay = 4;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case DELIV_CPU_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step = DELIV_CPU_MOVE;
        }
        break;
    case DELIV_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case DELIV_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            *step = (work->cursor == work->numLevels) ? DELIV_SHOW_PRICES : DELIV_DELIVER;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        break;
    /* 0x3c: deliver: the first empty goods slot gets the level */
    case DELIV_DELIVER:
        work->cpuIndex++;
        st = &g_stores[work->store];
        for (i = 0; i < st->maxGoods; i++)
        {
            if (st->goods[i].active == 0)
            {
                st->goods[i].active = 1;
                st->goods[i].level = work->level;
                Store_SortGoods(work->store);
                break;
            }
        }
        if (work->kind == DELIVERY_PAID)
        {
            Player_Pay(g_curPlayer,g_stores[work->store].type->items[work->level].cost);
        }
        *step = DELIV_SHOW_PRICES;
        break;
    /* 0x46-0x4f: the retail prices, until a button (a CPU player: 30 frames) */
    case DELIV_SHOW_PRICES:
        TextWin_SetFocus(work->win,0);
        g_uiTaskWork[6][1] = 100;
        g_uiTaskWork[6][2] = 0x20;
        g_uiTaskWork[6][3] = work->store;
        Task_Start(g_uiTaskIds[6],RetailPrices_Task);
        *step = (g_players[g_curPlayer].isCpu == 0) ? DELIV_SHOW_PRICES_WAIT_BUTTON : DELIV_SHOW_PRICES_CPU;
        break;
    case DELIV_SHOW_PRICES_WAIT_BUTTON:
        if ((g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON) == 0) break;
        Se_Play(SE_CANCEL,0x7f,0x7f,1);
        *step = DELIV_SHOW_PRICES_CLOSE;
        break;
    case DELIV_SHOW_PRICES_CPU:
        work->delay = 0x1e;
        *step += 1;
        break;
    case DELIV_SHOW_PRICES_CPU_WAIT:
        if (--work->delay != 0) break;
        *step = DELIV_SHOW_PRICES_CLOSE;
        break;
    case DELIV_SHOW_PRICES_CLOSE:
        Task_SendMessage(g_uiTaskIds[6],0);
        *step += 1;
        break;
    case DELIV_SHOW_PRICES_WAIT_CLOSE:
        if (Task_IsDone(g_uiTaskIds[6]) != 0)
        {
            TextWin_SetFocus(work->win,1);
            *step = (work->cursor == work->numLevels) ? DELIV_FINISH : DELIV_NEXT;
            work->rebuild = 1;
        }
        break;
    /* 0x50, 0x51: "Finish delivery?" */
    case DELIV_FINISH:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CFinish delivery?%KYes%KNo%E");
        *step += 1;
        break;
    case DELIV_FINISH_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = (g_msgAnswer == 0) ? DELIV_CLOSE : DELIV_CHOOSE;
        break;
    case DELIV_NO_MONEY:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CInsufficient money.%E");
        *step += 1;
        break;
    case DELIV_WAIT_NO_MONEY:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = DELIV_NEXT;
        break;
    /* 100-0x66: close, the camera back to the player */
    case DELIV_CLOSE:
        work->camMode = 1;
        Task_SendMessage(g_uiTaskIds[4],0);
        Task_SendMessage(g_uiTaskIds[3],0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case DELIV_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->listShown = 0;
            *step += 1;
        }
        /* fall through (the original ends as soon as the camera is back) */
    case DELIV_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }

    if (work->listShown != 0)
    {
        /* the list: the levels from firstLevel on, name and price, palette 1 when too expensive */
        if (work->rebuild != 0)
        {
            st = &g_stores[work->store];
            strcpy(g_msgBuf,"%L");
            for (level = work->firstLevel; level < 6; level++)
            {
                cost = (work->kind != DELIVERY_FREE) ? st->type->items[level].cost : 0;
                strcat(g_msgBuf,(g_players[g_curPlayer].money < cost) ? "%P01" : "%P00");
                name = st->type->items[level].name;
                strcat(g_msgBuf,name);
                Str_FillSpaces(23 - strlen(name),g_msgBuf + strlen(g_msgBuf));
                Str_FormatInt(cost,5,g_msgBuf + strlen(g_msgBuf));
                strcat(g_msgBuf,"%K");
            }
            strcat(g_msgBuf,"%P00Complete Delivery");
            strcat(g_msgBuf,"%E");
            TextWin_ClearText(work->win);
            TextWin_AddText(work->win,g_msgBuf);
            if (work->rebuild == 2)
            {
                TextWin_SetMenu(work->win,0,work->numLevels + 1,0);
            }
            work->costDirty = 1;
            work->rebuild = 0;
        }
        /* the cost window (the original compares the cursor with maxGoods, not numLevels) */
        if (work->costDirty != 0)
        {
            g_uiTaskWork[3][1] = 1;
            g_uiTaskWork[3][2] = g_players[g_curPlayer].money;
            cost = 0;
            if (g_textWins[work->win].menuCursor != g_stores[work->store].maxGoods &&
                work->kind != DELIVERY_FREE)
            {
                cost = g_stores[work->store].type->items[work->firstLevel +
                                                          g_textWins[work->win].menuCursor].cost;
            }
            g_uiTaskWork[3][3] = cost;
            g_uiTaskWork[3][4] = 1;
            work->costDirty = 0;
        }
    }
    work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    if (work->camMode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x2f,0x46);
    }
    return 0;
}

/* Open a window at (x, y) listing the goods of store `store` and their retail prices; returns it */
static int RetailPrices_Open(int store, int x, int y)
{
    Store *st;
    char *name;
    int win;
    int i;

    Store_UpdatePrices(store);
    strcpy(g_msgBuf,"%LItem Name     Retail Price%K");
    st = &g_stores[store];
    for (i = 0; i < st->numGoods; i++)
    {
        name = st->type->items[st->goods[i].level].name;
        strcat(g_msgBuf,name);
        Str_FillSpaces(22 - strlen(name),g_msgBuf + strlen(g_msgBuf));
        Str_FormatInt(st->goods[i].basePrice,6,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,"%K");
    }
    strcat(g_msgBuf,"%E");
    win = TextWin_Create(x,y,0x1c,g_kindMaxGoods[st->chainLen] + 1);
    TextWin_AddText(win,g_msgBuf);
    TextWin_Open(win);
    return win;
}

/* RetailPrices_Task steps */
enum RetailPricesStep
{
    PRICES_OPEN       =   0,
    PRICES_WAIT_OPEN  =   1,
    PRICES_SHOWN      =  10,  /* until message 0 */
    PRICES_CLOSE      = 100,
    PRICES_WAIT_CLOSE = 101 
};

/* Window listing a store's goods and retail prices, until message 0 (0x8005CB18) */
/* 0x8005CB18 */
u32 RetailPrices_Task(u16 *step, RetailPricesWork *work)
{
    int msg[2];

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = PRICES_CLOSE;
    }
    switch (*step)
    {
    case PRICES_OPEN:
        work->busy = 1;
        work->win = RetailPrices_Open(work->store,work->x,work->y);
        *step += 1;
        break;
    case PRICES_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            TextWin_SetFocus(work->win,1);
            *step = PRICES_SHOWN;
        }
        break;
    case PRICES_CLOSE:
        TextWin_Close(work->win);
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case PRICES_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->busy = 0;
            return 1;
        }
        break;
    }
    return 0;
}

/* RetailPricesInput_Task steps */
enum RetailPricesInputStep
{
    PRICEIN_OPEN                 =   0,
    PRICEIN_WAIT_OPEN            =   1,
    PRICEIN_CHOOSE               =  10,  /* cross: the goods, circle: done */
    PRICEIN_GOODS                =  20,  /* the store type's goods (ItemList_Task) */
    PRICEIN_GOODS_WAIT_BUTTON    =  21,
    PRICEIN_GOODS_CLOSE          =  22,
    PRICEIN_CLOSE_FOR_GOODS      =  50,  /* cross: close, then the goods */
    PRICEIN_WAIT_CLOSE_FOR_GOODS =  51,
    PRICEIN_CLOSE                =  60,  /* circle: close and end */
    PRICEIN_WAIT_CLOSE           =  61,
    PRICEIN_DONE                 = 100 
};

/* As RetailPrices_Task, with input: cross closes it and shows the store type's goods (ItemList_Task;
 * then circle comes back here, cross ends), circle ends (0x8005CEE0) */
/* 0x8005CEE0 */
u32 RetailPricesInput_Task(short *step, RetailPricesInputWork *work)
{
    u16 pressed;
    int msg[2];

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = PRICEIN_DONE;
    }
    switch(*step)
    {
    case PRICEIN_OPEN:
        work->busy = 1;
        work->storeType = g_stores[work->store].storeType;
        work->win = RetailPrices_Open(work->store,work->x,work->y);
        *step += 1;
        break;
    case PRICEIN_WAIT_OPEN:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN) break;
        TextWin_SetFocus(work->win,1);
        *step = PRICEIN_CHOOSE;
        break;
    case PRICEIN_CHOOSE:
        if (g_playerPads[g_curPlayer].pressed & PADRdown)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = PRICEIN_CLOSE_FOR_GOODS;
        }
        else if (g_playerPads[g_curPlayer].pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = PRICEIN_CLOSE;
        }
        break;
    /* 0x14-0x16: the store type's goods */
    case PRICEIN_GOODS:
        g_uiTaskWork[5][1] = work->x + 8;
        g_uiTaskWork[5][2] = work->y + 8;
        g_uiTaskWork[5][3] = work->storeType;
        g_uiTaskWork[5][4] = 6 - k_chainLevels[g_stores[work->store].chainLen];
        Task_Start(g_uiTaskIds[5],ItemList_Task);
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case PRICEIN_GOODS_WAIT_BUTTON:
        pressed = g_playerPads[g_curPlayer].pressed;
        if ((pressed & (PADRright | PADRdown)) == 0) break;
        if (pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            work->done = 0;
        }
        else
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            work->done = 1;
        }
        Task_SendMessage(g_uiTaskIds[5],0);
        TextWin_SetFocus(work->win,1);
        *step += 1;
        break;
    case PRICEIN_GOODS_CLOSE:
        if (Task_IsDone(g_uiTaskIds[5]) == 0) break;
        *step = (work->done == 0) ? PRICEIN_OPEN : PRICEIN_DONE;
        break;
    /* 0x32, 0x33 (cross): close, then the goods; 0x3c, 0x3d (circle): close and end */
    case PRICEIN_CLOSE_FOR_GOODS:
    case PRICEIN_CLOSE:
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case PRICEIN_WAIT_CLOSE_FOR_GOODS:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->busy = 0;
        *step = PRICEIN_GOODS;
        break;
    case PRICEIN_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->busy = 0;
        return 1;
    case PRICEIN_DONE:
        return 1;
    }
    return 0;
}

/* The store in the selected row (top + cursor, wrapped around at `wrap`); the camera goes there,
 * or back to the player on "Do not sell" */
static void SellStore_SelectRow(SellStoreWork *work, int wrap)
{
    int index = (u8)work->top + (u8)g_textWins[work->win].menuCursor;

    work->index = index;
    if ((signed char)index >= wrap)
    {
        work->index = index - wrap;
    }
    if (work->canDecline == 0 || work->index != g_storeListCount)
    {
        work->store = g_storeList[work->index];
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
        work->camMode = 2;
    }
    else
    {
        work->camMode = 1;
    }
}

/* Append the list row of g_storeList entry `entry`: name, goods / slots, sale price (the
 * original leaves it in work->store) */
static void SellStore_AddRow(SellStoreWork *work, int entry)
{
    Store *st;

    work->store = g_storeList[entry];
    st = &g_stores[work->store];
    strcat(g_msgBuf,st->type->name);
    Str_FillSpaces(21 - strlen(st->type->name),g_msgBuf + strlen(g_msgBuf));
    Str_FormatInt(st->numGoods,1,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"/");
    Str_FormatInt(st->maxGoods,1,g_msgBuf + strlen(g_msgBuf));
    Str_FillSpaces(7,g_msgBuf + strlen(g_msgBuf));
    Str_FormatInt((Store_GetValue(work->store) * work->percent) / 100,6,g_msgBuf + strlen(g_msgBuf));
}

/* SellStore_Task steps */
enum SellStoreStep
{
    SELL_STEP_INIT                =   0,
    SELL_STEP_WAIT_MSG            =   1,
    SELL_STEP_LIST                =   5,  /* the player's stores */
    SELL_STEP_WAIT_LIST           =   6,  /* none: "You have no stores to sell." */
    SELL_STEP_CHOOSE              =   8,
    SELL_STEP_HUMAN               =  10,  /* a human player chooses */
    SELL_STEP_HUMAN_CHOOSE        =  11,  /* a hint after 300 frames without input */
    SELL_STEP_CPU                 =  20,  /* a CPU player: Cpu_ChooseStoreToSell, the cursor down to it */
    SELL_STEP_CPU_MOVE            =  21,
    SELL_STEP_CPU_MOVE_WAIT       =  22,
    SELL_STEP_CPU_PICK            =  23,  /* 15 frames */
    SELL_STEP_CPU_PICK_WAIT       =  24,
    SELL_STEP_CPU_DECLINE         =  25,  /* the cursor to "Do not sell" */
    SELL_STEP_CONFIRM             =  30,  /* "Sell this store?" */
    SELL_STEP_CONFIRM_ANSWER      =  31,
    SELL_STEP_SELL                =  40,  /* the player gets percent of its value, it closes */
    SELL_STEP_SHOP_OPEN           =  41,
    SELL_STEP_WAIT_SHOP_OPEN      =  42,
    SELL_STEP_REBUILD             =  45,
    SELL_STEP_BALANCE             =  46,  /* the money must get positive: another */
    SELL_STEP_STILL_NEGATIVE      =  50,  /* "You still have a negative balance. Please sell a store." */
    SELL_STEP_WAIT_STILL_NEGATIVE =  51,
    SELL_STEP_PRICES              =  60,  /* the store's retail prices until a button */
    SELL_STEP_PRICES_WAIT_BUTTON  =  61,
    SELL_STEP_PRICES_CLOSE        =  62,
    SELL_STEP_DECLINE             =  70,  /* "Do not sell": "Complete the store sale?" */
    SELL_STEP_DECLINE_ANSWER      =  71,
    SELL_STEP_NONE                =  90,  /* "You have no stores to sell." */
    SELL_STEP_WAIT_NONE           =  91,
    SELL_STEP_CLOSE               = 100,
    SELL_STEP_WAIT_CLOSE          = 101,
    SELL_STEP_WAIT_CAMERA         = 102 
};

/* Selling a store (kind: SELL_*) for percent % of its value: a list of the player's stores (more
 * than 4 rows: a window of 4 scrolling around), L1 / R1 show a store's retail prices
 * (0x8005D4D8) */
/* 0x8005D4D8 */
u32 SellStore_Task(short *step, SellStoreWork *work)
{
    PadState *pad;
    TextWin *win;
    int entry;
    int i;

    switch(*step)
    {
    case SELL_STEP_INIT:
        work->listShown = 0;
        work->camMode = 0;
        work->arrived = 1;
        work->rebuild = 0;
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 "%CPlease sell a store.");
        *step += 1;
        break;
    case SELL_STEP_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = SELL_STEP_LIST;
        }
        break;
    case SELL_STEP_LIST:
        work->canDecline = (work->kind == SELL_CHOICE || work->kind == SELL_CLOSED);
        work->listShown = 1;
        work->top = 0;
        work->unused12 = 0;
        work->rebuild = 2;
        work->win = TextWin_Create(0x14,0x10,0x26,5);
        TextWin_Open(work->win);
        *step += 1;
        break;
    case SELL_STEP_WAIT_LIST:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN) break;
        *step = (g_storeListCount != 0) ? SELL_STEP_CHOOSE : SELL_STEP_NONE;
        break;
    case SELL_STEP_CHOOSE:
        *step = (g_players[g_curPlayer].isCpu != 0) ? SELL_STEP_CPU : SELL_STEP_HUMAN;
        break;
    /* 10, 0xb: a human player chooses; a hint after 300 frames without input */
    case SELL_STEP_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case SELL_STEP_HUMAN_CHOOSE:
        pad = &g_playerPads[g_curPlayer];
        win = &g_textWins[work->win];
        if (pad->pressed & PADRdown)
        {
            *step = (work->canDecline != 0 && work->index == g_storeListCount) ? SELL_STEP_DECLINE : SELL_STEP_CONFIRM;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (pad->pressed & PADRright)
        {
            if (work->canDecline != 0)
            {
                /* circle: to "Do not sell" */
                if (work->rows < 5)
                {
                    win->menuCursor = work->rows - 1;
                }
                else
                {
                    win->menuCursor = 0;
                    work->top = work->rows - 1;
                    work->rebuild = 1;
                    work->index = work->top + win->menuCursor;
                }
                *step = SELL_STEP_DECLINE;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        else if (pad->pressed & (PADL1 | PADR1))
        {
            if (work->canDecline == 0 || work->index != g_storeListCount)
            {
                *step = SELL_STEP_PRICES;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                if (work->rows < 5)
                {
                    TextWin_CursorUp(work->win);
                }
                else if (win->menuCursor > 0)
                {
                    win->menuCursor--;
                }
                else
                {
                    /* scroll up, wrapping around to the end */
                    if (--work->top < 0)
                    {
                        work->top += work->rows;
                    }
                    work->rebuild = 1;
                }
            }
            else
            {
                if (work->rows < 5)
                {
                    TextWin_CursorDown(work->win);
                }
                else if (win->menuCursor < 3)
                {
                    win->menuCursor++;
                }
                else
                {
                    /* scroll down, wrapping around to the start */
                    if (++work->top >= work->rows)
                    {
                        work->top -= work->rows;
                    }
                    work->rebuild = 1;
                }
            }
            work->hintTimer = 300;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        SellStore_SelectRow(work,work->rows);
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CPlease sell a store.");
        *step = SELL_STEP_HUMAN;
        break;
    /* 0x14-0x19: a CPU player moves the cursor down to its store, or declines */
    case SELL_STEP_CPU:
        work->cpuChoice = Cpu_ChooseStoreToSell(g_curPlayer,
                                                work->kind == SELL_CHOICE || work->kind == SELL_CLOSED,
                                                work->percent);
        *step = (work->cpuChoice == -1) ? SELL_STEP_CPU_DECLINE : *step + 1;
        break;
    case SELL_STEP_CPU_MOVE:
        if (g_storeList[work->index] == work->cpuChoice)
        {
            *step = SELL_STEP_CPU_PICK;
            break;
        }
        win = &g_textWins[work->win];
        if (win->menuCursor < 3)
        {
            win->menuCursor++;
        }
        else
        {
            /* (scrolls around at g_storeListCount) */
            if (++work->top >= g_storeListCount)
            {
                work->top -= g_storeListCount;
            }
            work->rebuild = 1;
        }
        SellStore_SelectRow(work,g_storeListCount);
        work->delay = 4;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case SELL_STEP_CPU_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step = SELL_STEP_CPU_MOVE;
        }
        break;
    case SELL_STEP_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case SELL_STEP_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = (work->cpuChoice == -1) ? SELL_STEP_DECLINE : SELL_STEP_CONFIRM;
        }
        break;
    case SELL_STEP_CPU_DECLINE:
        work->camMode = 1;
        g_textWins[work->win].menuCursor = 0;
        work->index = g_storeListCount;
        work->rebuild = 1;
        work->top = g_storeListCount;
        *step = SELL_STEP_CPU_PICK;
        break;
    /* 0x1e-0x33: sell it; when the balance must get positive and still is not, another */
    case SELL_STEP_CONFIRM:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CSell this store?%KYes%KNo%E");
        *step += 1;
        break;
    case SELL_STEP_CONFIRM_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = (g_msgAnswer == 0) ? SELL_STEP_SELL : SELL_STEP_CHOOSE;
        break;
    case SELL_STEP_SELL:
        g_turnFlags[TURN_SOLD_STORE] = 1;
        Player_Earn(g_curPlayer,(Store_GetValue(work->store) * work->percent) / 100);
        Store_Close(work->store);
        *step += 1;
        break;
    case SELL_STEP_SHOP_OPEN:
        Task_Start(g_actionTaskIds[0],ShopOpen_Task);
        *step += 1;
        break;
    case SELL_STEP_WAIT_SHOP_OPEN:
        if (Task_IsDone(g_actionTaskIds[0]) != 0)
        {
            *step = SELL_STEP_REBUILD;
        }
        break;
    case SELL_STEP_REBUILD:
        work->rebuild = 2;
        *step += 1;
        break;
    case SELL_STEP_BALANCE:
        if (work->canDecline != 0 || g_players[g_curPlayer].money >= 0)
        {
            *step = SELL_STEP_CLOSE;
        }
        else
        {
            *step = (g_storeListCount != 0) ? SELL_STEP_STILL_NEGATIVE : SELL_STEP_NONE;
        }
        break;
    case SELL_STEP_STILL_NEGATIVE:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CYou still have a negative balance.%KPlease sell a store.%E");
        *step += 1;
        break;
    case SELL_STEP_WAIT_STILL_NEGATIVE:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = SELL_STEP_CHOOSE;
        break;
    /* 0x3c-0x3e: the store's retail prices until a button */
    case SELL_STEP_PRICES:
        TextWin_SetFocus(work->win,0);
        g_uiTaskWork[6][1] = 100;
        g_uiTaskWork[6][2] = 0x20;
        g_uiTaskWork[6][3] = work->store;
        Task_Start(g_uiTaskIds[6],RetailPrices_Task);
        *step += 1;
        break;
    case SELL_STEP_PRICES_WAIT_BUTTON:
        if ((g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON) == 0) break;
        Se_Play(SE_CANCEL,0x7f,0x7f,1);
        Task_SendMessage(g_uiTaskIds[6],0);
        *step += 1;
        break;
    case SELL_STEP_PRICES_CLOSE:
        if (Task_IsDone(g_uiTaskIds[6]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = SELL_STEP_HUMAN;
        break;
    /* 0x46, 0x47: "Do not sell" */
    case SELL_STEP_DECLINE:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CComplete the store sale?%KYes%KNo%E");
        *step += 1;
        break;
    case SELL_STEP_DECLINE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = (g_msgAnswer == 0) ? SELL_STEP_CLOSE : SELL_STEP_CHOOSE;
        break;
    case SELL_STEP_NONE:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CYou have no stores to sell.%E");
        *step += 1;
        break;
    case SELL_STEP_WAIT_NONE:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = SELL_STEP_CLOSE;
        break;
    /* 100-0x66: close */
    case SELL_STEP_CLOSE:
        work->camMode = 1;
        Task_SendMessage(g_uiTaskIds[4],0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case SELL_STEP_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->listShown = 0;
        *step += 1;
        break;
    case SELL_STEP_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }

    /* the list: the stores (and "Do not sell"), 4 rows from `top` when there are more */
    if (work->rebuild != 0)
    {
        if (work->rebuild == 2)
        {
            work->top = 0;
            if (work->kind < SELL_CLOSED)
            {
                Stores_ListByOwner(g_curPlayer);
            }
            else
            {
                Stores_ListClosed(g_curPlayer);
            }
            work->rows = g_storeListCount + (work->canDecline != 0 ? 1 : 0);
            TextWin_SetMenu(work->win,1,(work->rows < 5) ? work->rows : 4,0);
        }
        strcpy(g_msgBuf,"%LStore Name    # of Items  Store Assets%K");
        if (work->rows < 5)
        {
            for (i = 0; i < g_storeListCount; i++)
            {
                SellStore_AddRow(work,i);
                strcat(g_msgBuf,"%K");
            }
            if (work->canDecline != 0)
            {
                strcat(g_msgBuf,"Do not sell%K");
            }
        }
        else
        {
            TextWin_SetArrowUp(work->win,1);
            TextWin_SetArrowDown(work->win,1);
            for (i = 0; i < 4; i++)
            {
                entry = work->top + i;
                if (work->canDecline != 0 && entry == g_storeListCount)
                {
                    strcat(g_msgBuf,"Do not sell%K");
                    continue;
                }
                if (entry >= g_storeListCount)
                {
                    entry -= work->rows;
                }
                SellStore_AddRow(work,entry);
                strcat(g_msgBuf,"%K");
            }
        }
        strcat(g_msgBuf,"%E");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        SellStore_SelectRow(work,g_storeListCount);
        work->rebuild = 0;
    }
    if (g_storeListCount != 0)
    {
        work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,0x78),
                                      CAMERA_CELL_Y(work->floor,0x6a));
        if (work->camMode == 2 && work->arrived != 0)
        {
            Menu_DrawFrameCursor(0x9f,0x6a);
        }
    }
    return 0;
}

/* Store `cell` (a Board_GetCell result) is the current player's, on the square it stands on */
static int StoreOffer_IsOwnHere(u16 cell)
{
    u32 owner;

    if (CELL_TYPE(cell) != CELL_STORE)
    {
        return 0;
    }
    owner = g_stores[cell].owner;
    return owner == g_curPlayer &&
           g_stores[cell].squareId == Board_GetSquareId(g_players[owner].x,g_players[owner].floor);
}

/* StoreOffer_Task steps */
enum StoreOfferStep
{
    OFFER_INIT          =   0,
    OFFER_WAIT_MSG      =   1,
    OFFER_QUESTION      =   5,  /* the prices, "It will cost ..." and the cost window */
    OFFER_WAIT_QUESTION =   6,
    OFFER_START         =  10,  /* not enough money: OFFER_NO_MONEY */
    OFFER_HUMAN         =  20,  /* a human player answers */
    OFFER_HUMAN_ANSWER  =  21,  /* the question again after 300 frames without input */
    OFFER_CPU           =  30,  /* a CPU player decides (Cpu_RefuseStoreOffer), the cursor moves */
    OFFER_CPU_MOVE      =  31,
    OFFER_CPU_MOVE_WAIT =  32,
    OFFER_CPU_PICK      =  33,  /* 15 frames */
    OFFER_CPU_PICK_WAIT =  34,
    OFFER_BUY           =  40,  /* buy it out, open the store, pay */
    OFFER_NO_MONEY      =  60,  /* "Insufficient money." */
    OFFER_WAIT_NO_MONEY =  61,
    OFFER_CLOSE         = 100,
    OFFER_WAIT_CLOSE    = 101,
    OFFER_WAIT_CAMERA   = 102   /* the camera back to the player */
};

/* On a square where the player has a store: open another of its type on this store square (buy:
 * buying it from its owner first), for the type's price and the buy-out price; "It will cost
 * ..." Yes / No (0x8005ECE4) */
/* 0x8005ECE4 */
u32 StoreOffer_Task(short *step, StoreOfferWork *work)
{
    Store *st;
    PadState *pad;
    int i;

    /* (the original also copies k_storeOfferInit and k_storeOfferTexts into unused locals) */
    switch(*step)
    {
    case OFFER_INIT:
        work->arrived = 1;
        work->shown = 0;
        work->camMode = 0;
        work->costDirty = 0;
        work->store = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
        /* the type of the player's (first) store on this square */
        for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
        {
            if (g_stores[i].owner == g_curPlayer &&
                g_stores[i].squareId == Board_GetSquareId(g_players[g_curPlayer].x,g_players[g_curPlayer].floor))
            {
                break;
            }
        }
        work->expand = 0;
        work->storeType = g_stores[i].storeType;
        if (StoreOffer_IsOwnHere(Board_GetCell(work->x - 1,work->floor)))
        {
            work->expand = 1;
        }
        if (StoreOffer_IsOwnHere(Board_GetCell(work->x + 1,work->floor)))
        {
            work->expand = 1;
        }
        Stores_CountOwnedOfType(g_curPlayer,work->storeType);    /* result not used */
        StoreOffer_Question(work->buy,work->expand,g_storeTypes[work->storeType].name);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 g_offerMsgText);
        *step += 1;
        break;
    case OFFER_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = OFFER_QUESTION;
        }
        break;
    /* 5, 6: the prices, "It will cost ..." and the cost window */
    case OFFER_QUESTION:
        work->shown = 1;
        work->camMode = 2;
        if (work->buy == 0)
        {
            work->buyPrice = 0;
            work->ownerGain = 0;
        }
        else
        {
            /* the store's value times 1.5, 2, 2.5 or 3 by the length of its chain */
            st = &g_stores[work->store];
            switch (st->chainLen)
            {
            case 0:
                work->buyPrice = (st->type->value * 3) / 2;
                break;
            case 1:
                work->buyPrice = st->type->value * 2;
                break;
            case 2:
                work->buyPrice = (st->type->value * 5) / 2;
                break;
            case 3:
                work->buyPrice = st->type->value * 3;
                break;
            }
            work->ownerGain = work->buyPrice - st->type->value;
        }
        work->price = StoreType_GetPrice(work->storeType,
                                         Store_CountOwnedNeighbors(g_curPlayer,work->store));
        work->total = work->buyPrice + work->price;
        strcpy(g_msgBuf,"%CIt will cost ");
        Str_FormatInt(work->total,0,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,".%KYes%KNo%E");
        work->win = TextWin_Create(0x5c,0x18,0x1c,-1);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        TextWin_SetMenu(work->win,1,2,0);
        g_uiTaskWork[3][1] = 0;
        Task_Start(g_uiTaskIds[3],CostWin_Task);
        work->costDirty = 1;
        *step += 1;
        break;
    case OFFER_WAIT_QUESTION:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN) break;
        *step = OFFER_START;
        break;
    case OFFER_START:
        if (g_boardRules.enable[RULE_FREE_BUYING] == 0 && g_players[g_curPlayer].money < work->total)
        {
            g_textWins[work->win].menuCursor = 1;
            *step = OFFER_NO_MONEY;             /* "Insufficient money." */
        }
        else
        {
            *step = (g_players[g_curPlayer].isCpu != 0) ? OFFER_CPU : OFFER_HUMAN;
        }
        break;
    /* 0x14, 0x15: a human player answers; the question again after 300 frames without input */
    case OFFER_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case OFFER_HUMAN_ANSWER:
        pad = &g_playerPads[g_curPlayer];
        if (pad->pressed & PADRdown)
        {
            work->cursor = TextWin_GetCursor(work->win);
            *step = (work->cursor == 0) ? OFFER_BUY : OFFER_CLOSE;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (pad->pressed & PADRright)
        {
            g_textWins[work->win].menuCursor = 1;
            work->costDirty = 1;
            *step = OFFER_CLOSE;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                TextWin_CursorUp(work->win);
            }
            else
            {
                TextWin_CursorDown(work->win);
            }
            work->hintTimer = 300;
            work->costDirty = 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Stores_CountOwnedOfType(g_curPlayer,work->storeType);    /* result not used */
        StoreOffer_Question(work->buy,work->expand,g_storeTypes[work->storeType].name);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 g_offerMsgText);
        *step = OFFER_HUMAN;
        break;
    /* 0x1e-0x22: a CPU player decides and moves the cursor */
    case OFFER_CPU:
        work->cursor = Cpu_RefuseStoreOffer(g_curPlayer,work->store,work->total,work->ownerGain,
                                            g_stores[work->store].owner);
        *step += 1;
        break;
    case OFFER_CPU_MOVE:
        if (g_textWins[work->win].menuCursor != work->cursor)
        {
            work->costDirty = 1;
            TextWin_CursorDown(work->win);
            work->delay = 4;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            *step += 1;
            break;
        }
        *step = OFFER_CPU_PICK;
        break;
    case OFFER_CPU_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step = OFFER_CPU_MOVE;
        }
        break;
    case OFFER_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case OFFER_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            *step = (work->cursor == 0) ? OFFER_BUY : OFFER_CLOSE;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        break;
    /* 0x28: buy, open, pay */
    case OFFER_BUY:
        if (work->buy == 1)
        {
            Player_Earn(g_stores[work->store].owner,work->buyPrice);
            Store_Close(work->store);
            g_turnFlags[TURN_BOUGHT_STORE] = 1;
        }
        Store_Open(work->store,work->storeType,g_curPlayer);
        Player_Pay(g_curPlayer,work->total);
        switch (g_stores[work->store].chainLen)
        {
        case 0:
            g_turnFlags[TURN_OWN_STORE] = 1;
            break;
        case 1:
        case 2:
            g_turnFlags[TURN_CHAIN_2_3] = 1;
            break;
        case 3:
            g_turnFlags[TURN_CHAIN_4] = 1;
            break;
        }
        *step = OFFER_CLOSE;
        break;
    case OFFER_NO_MONEY:
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CInsufficient money.%E");
        *step += 1;
        break;
    case OFFER_WAIT_NO_MONEY:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        *step = OFFER_CLOSE;
        break;
    /* 100-0x66: close, the camera back to the player */
    case OFFER_CLOSE:
        work->camMode = 1;
        Task_SendMessage(g_uiTaskIds[4],0);
        Task_SendMessage(g_uiTaskIds[3],0);
        TextWin_Close(work->win);
        *step += 1;
        /* fall through */
    case OFFER_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->shown = 0;
        *step += 1;
        break;
    case OFFER_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }
    work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    if (work->camMode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x2f,0x46);
    }
    /* the cost window: the player's money and, on "Yes", the total */
    if (work->costDirty != 0 && work->shown != 0)
    {
        g_uiTaskWork[3][1] = 1;
        g_uiTaskWork[3][2] = g_players[g_curPlayer].money;
        g_uiTaskWork[3][3] = (g_textWins[work->win].menuCursor == 0) ? work->total : 0;
        g_uiTaskWork[3][4] = 1;
        work->costDirty = 0;
    }
    return 0;
}

/* The question of StoreOffer_Task into g_offerMsgText: "Will you open <name> here?", "Purchase
 * this store and open / expand <name>?" (0x8005FC78) */
/* 0x8005FC78 */
void StoreOffer_Question(char buy, char expand, char *name)
{
    strcpy(g_offerMsgText,"%C");
    if (buy == 0 && expand == 0)
    {
        strcat(g_offerMsgText,"Will you open%K");
        strcat(g_offerMsgText,"\201w");
        strcat(g_offerMsgText,name);
        strcat(g_offerMsgText,"\201x%K");
        strcat(g_offerMsgText,"here?");
        return;
    }
    strcat(g_offerMsgText,"Purchase this store%K");
    strcat(g_offerMsgText,(buy != 0 && expand != 0) ? "and expand%K" : "and open%K");
    strcat(g_offerMsgText,"\201w");
    strcat(g_offerMsgText,name);
    strcat(g_offerMsgText,"\201x?");
}

/* Append the list row of own store `store` to g_msgBuf: name, goods / slots, value; palette 2 when
 * full, palette 1 when the cheapest goods are too expensive */
static void DeliveryStore_AddRow(int store)
{
    Store *st = &g_stores[store];

    if (st->numGoods == st->maxGoods)
    {
        strcat(g_msgBuf,"%P02");
    }
    else if (g_players[g_curPlayer].money < g_storeTypes[(char)st->storeType].items[5].cost)
    {
        strcat(g_msgBuf,"%P01");
    }
    else
    {
        strcat(g_msgBuf,"%P00");
    }
    strcat(g_msgBuf,st->type->name);
    Str_FillSpaces(21 - strlen(st->type->name),g_msgBuf + strlen(g_msgBuf));
    Str_FormatInt(st->numGoods,1,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"/");
    Str_FormatInt(st->maxGoods,1,g_msgBuf + strlen(g_msgBuf));
    Str_FillSpaces(6,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"  ");
    Str_FormatInt(Store_GetValue(store),5,g_msgBuf + strlen(g_msgBuf));
}

/* DeliveryStore_Task steps */
enum DeliveryStoreStep
{
    DSTORE_INIT               =   0,
    DSTORE_WAIT_MSG           =   1,
    DSTORE_LIST               =  10,  /* "Do not deliver" and the player's stores */
    DSTORE_WAIT_LIST          =  11,
    DSTORE_HUMAN              =  20,  /* a human player chooses (L1 / R1: the store's retail prices) */
    DSTORE_HUMAN_CHOOSE       =  21,  /* a hint after 300 frames without input */
    DSTORE_CPU                =  30,  /* a CPU player: Cpu_ChooseStoreToDeliver, the cursor down to it */
    DSTORE_CPU_MOVE           =  31,
    DSTORE_CPU_MOVE_WAIT      =  32,
    DSTORE_CPU_PICK           =  33,  /* 15 frames */
    DSTORE_CPU_PICK_WAIT      =  34,
    DSTORE_CPU_DECLINE        =  35,  /* 15 frames */
    DSTORE_CPU_DECLINE_WAIT   =  36,
    DSTORE_PRICES             =  50,  /* the retail prices of the selected store until a button */
    DSTORE_PRICES_WAIT_BUTTON =  51,
    DSTORE_PRICES_CLOSE       =  52,
    DSTORE_CONFIRM            =  60,  /* "Deliver to this store?" */
    DSTORE_CONFIRM_ANSWER     =  61,  /* the store: the user data */
    DSTORE_CANCEL             =  70,  /* "Cancel delivery?" */
    DSTORE_CANCEL_ANSWER      =  71,
    DSTORE_FULL               =  80,  /* "This store is full." */
    DSTORE_WAIT_FULL          =  81,
    DSTORE_CLOSE              = 100,
    DSTORE_WAIT_CLOSE         = 101,
    DSTORE_WAIT_CAMERA        = 102 
};

/* Choosing which own store to deliver to: a list of "Do not deliver" and the player's stores
 * (more than 3: a window of 4 rows scrolling around); returns the store, or -1, as user data
 * (debug name shopview win) (0x8005FDC0) */
/* 0x8005FDC0 */
u32 DeliveryStore_Task(short *step, DeliveryStoreWork *work)
{
    PadState *pad;
    TextWin *win;
    int rows;
    int row;
    int i;

    FntPrint("exec task shopview win\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch(*step)
    {
    case DSTORE_INIT:
        work->listShown = 0;
        work->camMode = 0;
        work->arrived = 1;
        work->rebuild = 0;
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 "%CPlease select a store%K you want to deliver.");
        *step += 1;
        break;
    case DSTORE_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = DSTORE_LIST;
        }
        break;
    case DSTORE_LIST:
        work->listShown = 1;
        Stores_ListByOwner(work->player);
        work->unused0A = 0;
        work->chosen = -1;
        work->top = 0;
        work->rebuild = 2;
        work->listShown = 1;
        work->camMode = (g_storeListCount == 0) ? 0 : 2;
        work->win = TextWin_Create(0x14,0x10,0x26,5);
        TextWin_Open(work->win);
        TextWin_SetFocus(work->win,1);
        *step += 1;
        break;
    case DSTORE_WAIT_LIST:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN) break;
        *step = (g_players[g_curPlayer].isCpu != 0) ? DSTORE_CPU : DSTORE_HUMAN;
        break;
    /* 0x14, 0x15: a human player chooses (L1 / R1: the store's retail prices); a hint after 300
     * frames without input */
    case DSTORE_HUMAN:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        work->hintTimer = 300;
        *step += 1;
        break;
    case DSTORE_HUMAN_CHOOSE:
        pad = &g_playerPads[g_curPlayer];
        win = &g_textWins[work->win];
        rows = g_storeListCount + 1;            /* "Do not deliver" and the stores */
        if (pad->pressed & PADRdown)
        {
            if (work->chosen == -1)
            {
                *step = DSTORE_CANCEL;
            }
            else if (g_stores[work->chosen].numGoods < g_stores[work->chosen].maxGoods)
            {
                *step = DSTORE_CONFIRM;
            }
            else
            {
                *step = DSTORE_FULL;
            }
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (pad->pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = DSTORE_CANCEL;
        }
        else if (pad->pressed & (PADL1 | PADR1))
        {
            if (g_storeListCount != 0 && (work->top + win->menuCursor) % rows != 0)
            {
                *step = DSTORE_PRICES;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            if (pad->repeat & PADLup)
            {
                if (g_storeListCount < 4)
                {
                    TextWin_CursorUp(work->win);
                }
                else if (win->menuCursor > 0)
                {
                    win->menuCursor--;
                }
                else
                {
                    /* scroll up, wrapping around to the end */
                    if (--work->top < 0)
                    {
                        work->top += rows;
                    }
                    work->rebuild = 1;
                }
            }
            else
            {
                if (g_storeListCount < 4)
                {
                    TextWin_CursorDown(work->win);
                }
                else if (win->menuCursor < 3)
                {
                    win->menuCursor++;
                }
                else
                {
                    /* scroll down, wrapping around to the start */
                    if ((signed char)++work->top >= rows)
                    {
                        work->top -= rows;
                    }
                    work->rebuild = 1;
                }
            }
            work->camMode = (g_storeListCount == 0) ? 0 : 2;
            work->hintTimer = 300;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (work->hintTimer != 0)
        {
            work->hintTimer--;
            break;
        }
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CSelect a store to deliver.");
        *step = DSTORE_HUMAN;
        break;
    /* 0x1e-0x24: a CPU player moves the cursor down to its store, or declines */
    case DSTORE_CPU:
        work->cpuChoice = Cpu_ChooseStoreToDeliver(work->player);
        Stores_ListByOwner(work->player);
        if (work->cpuChoice == -1)
        {
            *step = DSTORE_CPU_DECLINE;
            break;
        }
        *step += 1;
        break;
    case DSTORE_CPU_MOVE:
        win = &g_textWins[work->win];
        /* (no wrap-around here: row 0 reads the byte before g_storeList) */
        if (g_storeList[work->top + win->menuCursor - 1] == work->cpuChoice)
        {
            *step = DSTORE_CPU_PICK;
            break;
        }
        if (win->menuCursor < 3)
        {
            win->menuCursor++;
        }
        else
        {
            /* (scrolls around at g_storeListCount, one row early) */
            if ((signed char)++work->top >= g_storeListCount)
            {
                work->top -= g_storeListCount;
            }
            work->rebuild = 1;
        }
        work->delay = 2;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case DSTORE_CPU_MOVE_WAIT:
        if (--work->delay == 0)
        {
            *step = DSTORE_CPU_MOVE;
        }
        break;
    case DSTORE_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case DSTORE_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            *step = DSTORE_CONFIRM;
        }
        break;
    case DSTORE_CPU_DECLINE:
        work->delay = 0xf;
        *step += 1;
        break;
    case DSTORE_CPU_DECLINE_WAIT:
        if (--work->delay == 0)
        {
            *step = DSTORE_CANCEL;
        }
        break;
    /* 0x32-0x34: the retail prices of the selected store until a button */
    case DSTORE_PRICES:
        TextWin_SetFocus(work->win,0);
        g_uiTaskWork[6][1] = -1000;
        g_uiTaskWork[6][2] = -1000;
        g_uiTaskWork[6][3] = work->chosen;
        Task_Start(g_uiTaskIds[6],RetailPrices_Task);
        *step += 1;
        break;
    case DSTORE_PRICES_WAIT_BUTTON:
        if ((g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON) == 0) break;
        Se_Play(SE_CANCEL,0x7f,0x7f,1);
        Task_SendMessage(g_uiTaskIds[6],0);
        *step += 1;
        break;
    case DSTORE_PRICES_CLOSE:
        if (Task_IsDone(g_uiTaskIds[6]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = DSTORE_HUMAN;
        break;
    /* 0x3c-0x47: confirm; the answer is the task's user data */
    case DSTORE_CONFIRM:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CDeliver to this store?%KYes%KNo%E");
        *step += 1;
        break;
    case DSTORE_CONFIRM_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = (g_msgAnswer == 0) ? DSTORE_CLOSE : DSTORE_HUMAN;
            Task_SetUserData(-1,work->chosen);
        }
        break;
    case DSTORE_CANCEL:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(g_players[g_curPlayer].isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,
                     MSG_AUTO_SIZE,"%CCancel delivery?%KYes%KNo%E");
        *step += 1;
        break;
    case DSTORE_CANCEL_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]) != 0)
        {
            *step = (g_msgAnswer == 0) ? DSTORE_CLOSE : DSTORE_HUMAN;
            Task_SetUserData(-1,-1);
        }
        break;
    case DSTORE_FULL:
        TextWin_SetFocus(work->win,0);
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,
                 MSG_ANY_BUTTON,"%CThis store is full.%E");
        *step += 1;
        break;
    case DSTORE_WAIT_FULL:
        if (Task_IsDone(g_uiTaskIds[0]) == 0) break;
        TextWin_SetFocus(work->win,1);
        *step = DSTORE_HUMAN;
        break;
    /* 100-0x66: close */
    case DSTORE_CLOSE:
        Task_SendMessage(g_uiTaskIds[4],0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case DSTORE_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING) break;
        work->listShown = 0;
        *step += 1;
        break;
    case DSTORE_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }

    if (work->listShown != 0)
    {
        /* the list: "Do not deliver" and the stores, 4 rows from `top` when there are more */
        if (work->rebuild != 0)
        {
            if (g_storeListCount == 0)
            {
                strcpy(g_msgBuf,"%K%K%CNo stores deliver.%E");
            }
            else if (g_storeListCount < 4)
            {
                strcpy(g_msgBuf,"%LStore Name    # of Items  Store Assets%K");
                strcat(g_msgBuf,"%P00Do not deliver%K");
                for (i = 0; i < g_storeListCount; i++)
                {
                    DeliveryStore_AddRow((char)g_storeList[i]);
                    strcat(g_msgBuf,"%K");
                }
            }
            else
            {
                TextWin_SetArrowUp(work->win,1);
                TextWin_SetArrowDown(work->win,1);
                strcpy(g_msgBuf,"%LStore Name    # of Items  Store Assets%K");
                for (i = 0; i < 4; i++)
                {
                    row = (char)(work->top + i);
                    if (row >= g_storeListCount + 1)
                    {
                        row = (char)(row - (g_storeListCount + 1));
                    }
                    if (row == 0)
                    {
                        strcat(g_msgBuf,"%P00Do not deliver%K");
                    }
                    else
                    {
                        DeliveryStore_AddRow((char)g_storeList[row - 1]);
                        strcat(g_msgBuf,"%K");
                    }
                }
            }
            TextWin_ClearText(work->win);
            TextWin_AddText(work->win,g_msgBuf);
            if (work->rebuild == 2)
            {
                TextWin_SetMenu(work->win,1,(g_storeListCount + 1 < 4) ? g_storeListCount + 1 : 4,0);
            }
            work->rebuild = 0;
        }
        /* the store in the selected row; the camera goes there */
        if (work->camMode != 0)
        {
            row = (char)((work->top + g_textWins[work->win].menuCursor) % (g_storeListCount + 1));
            work->chosen = (row == 0) ? -1 : g_storeList[row - 1];
            /* (for -1 this reads the bytes before g_stores, like the original; not used then) */
            work->storeType = g_stores[work->chosen].storeType;
            work->x = g_stores[work->chosen].x;
            work->floor = g_stores[work->chosen].floor;
            if (g_storeListCount == 0)
            {
                work->camMode = 0;
            }
            else if (row == 0)
            {
                work->camMode = 1;
            }
            else
            {
                work->camMode = 2;
            }
        }
    }
    work->arrived = Camera_MoveTo((char)work->camMode,CAMERA_CELL_X(work->x,0x78),CAMERA_CELL_Y(work->floor,0x6a));
    if (work->camMode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x9f,0x6a);
    }
    return 0;
}

/* CostWin_Task steps */
enum CostWinStep
{
    COSTWIN_OPEN       =   0,
    COSTWIN_WAIT_OPEN  =   1,
    COSTWIN_SHOWN      =  10,  /* until message 0 */
    COSTWIN_CLOSE      = 100,
    COSTWIN_WAIT_CLOSE = 101 
};

/* Window "money - cost = rest" (or + cost), redrawn when its work area says so (dirty), while a
 * purchase is chosen; message: close (0x800613AC) */
/* 0x800613AC */
u32 CostWin_Task(u16 *step, CostWinWork *work)
{
    if (Task_ReceiveMessageIf(-3,0) == 0)
    {
        *step = COSTWIN_CLOSE;
    }
    switch (*step)
    {
    case COSTWIN_OPEN:
        work->shown = 1;
        work->win = TextWin_Create(0xa2,0x90,0x12,1);
        TextWin_Open(work->win);
        *step += 1;
        break;
    case COSTWIN_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            *step = COSTWIN_SHOWN;
        }
        break;
    case COSTWIN_CLOSE:
        TextWin_Close(work->win);
        *step += 1;
        break;
    case COSTWIN_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->shown = 0;
            return 1;
        }
        break;
    }
    if (work->dirty != 0 && work->shown != 0)
    {
        work->result = (work->subtract == 0) ? work->money + work->cost : work->money - work->cost;
        strcpy(g_msgBuf,"%L");
        Str_FormatInt(work->money,6,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,(work->subtract == 0) ? "+" : "-");
        Str_FormatInt(work->cost,4,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,"=");
        if (work->result < 0)
        {
            strcat(g_msgBuf,"%P02");              /* text palette 2 */
        }
        Str_FormatInt(work->result,6,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,"%E");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        work->dirty = 0;
    }
    return 0;
}

/* StealItem_Task steps */
enum StealItemStep
{
    STEAL_INIT          =   0,
    STEAL_WAIT_MSG      =   1,
    STEAL_LIST          =  10,  /* the store's goods and "Faild" */
    STEAL_WAIT_LIST     =  11,
    STEAL_ROULETTE      =  30,  /* 20 or more steps, a frame each */
    STEAL_ROULETTE_STEP =  31,
    STEAL_ROULETTE_WAIT =  32,
    STEAL_STOPPED       =  33,  /* 15 frames */
    STEAL_STOPPED_WAIT  =  34,
    STEAL_RESULT        =  40,  /* the item stolen, or none */
    STEAL_WAIT_RESULT   =  41,
    STEAL_CLOSE         = 100,
    STEAL_WAIT_CLOSE    = 101,
    STEAL_WAIT_CAMERA   = 102 
};

/* Unused leftover of the Japanese version: Board_Task starts it (step 8760) for a player with
 * Player.exempt landing on a rival's store, and nothing sets exempt. "Please shop for an item.",
 * then a roulette over the store's goods list and "Faild": the player gets an item's price
 * (Player_Earn) and the store loses the item ("<owner>さんのお店から『item』を盗んだ": stole
 * 'item' from <owner>'s store), or on "Faild" "何も盗めませんでした" (couldn't steal anything).
 * The item taken is always the first: its index is read from g_itemShopTable, never written
 * (0x80061658) */
/* 0x80061658 */
int StealItem_Task(short *step, StealItemWork *work)
{
    Store *store;
    char *name;
    int i;

    switch(*step)
    {
    case STEAL_INIT:
        work->listShown = 0;
        work->mode = 0;
        work->arrived = 1;
        work->refresh = 0;
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,"%CPlease shop for an item.");
        *step += 1;
        break;
    case STEAL_WAIT_MSG:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = STEAL_LIST;
        }
        break;
    case STEAL_LIST:              /* the list: the store's goods and "Faild" */
        work->listShown = 1;
        work->refresh = 1;
        work->mode = 2;
        work->store = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        store = &g_stores[work->store];
        work->player = store->owner;
        work->x = store->x;
        work->floor = store->floor;
        work->count = store->numGoods + 1;
        work->win = TextWin_Create(0x5c,0x18,0x1c,store->maxGoods + 2);
        TextWin_Open(work->win);
        TextWin_SetMenu(work->win,1,work->count,0);
        *step += 1;
        break;
    case STEAL_WAIT_LIST:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            *step = STEAL_ROULETTE;
            TextWin_SetFocus(work->win,1);
        }
        break;
    case STEAL_ROULETTE:          /* the roulette: 20 or more steps, a frame each */
        work->frame = 0;
        work->stopFrame = rand() % work->count + 20;
        *step += 1;
        break;
    case STEAL_ROULETTE_STEP:
        if (work->frame == work->stopFrame)
        {
            *step = STEAL_STOPPED;
            break;
        }
        work->frame++;
        g_textWins[work->win].menuCursor = work->frame % work->count;
        work->delay = 1;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case STEAL_ROULETTE_WAIT:
        if (--work->delay == 0)
        {
            *step = STEAL_ROULETTE_STEP;
        }
        break;
    case STEAL_STOPPED:
        work->delay = 15;
        *step += 1;
        break;
    case STEAL_STOPPED_WAIT:
        if (--work->delay == 0)
        {
            if (g_textWins[work->win].menuCursor < work->count - 1)
            {
                work->item = g_itemShopTable[work->win][7];   /* always 0: nothing writes the table */
                work->level = g_stores[work->store].goods[work->item].level;
            }
            else
            {
                work->item = -1;        /* "Faild" */
                work->level = -1;
            }
            *step = STEAL_RESULT;
        }
        break;
    case STEAL_RESULT:
        if (work->level == -1)
        {
            strcpy(g_msgBuf,"%L\211\275\202\340\223\220\202\337\202\334\202\271\202\361\202\305\202\265\202\275");
        }
        else
        {
            store = &g_stores[work->store];
            work->amount = store->type->items[work->level].cost;
            Player_Earn(g_curPlayer,work->amount);
            store->goods[work->item].active = 0;
            Store_SortGoods(work->store);
            work->refresh = 1;
            strcpy(g_msgBuf,"%L");
            strcat(g_msgBuf,g_players[work->player].name);
            strcat(g_msgBuf,"\202\263\202\361\202\314\202\250\223X\202\251\202\347%K\201w");
            strcat(g_msgBuf,store->type->items[work->level].name);
            strcat(g_msgBuf,"\201x\202\360\223\220\202\361\202\276");
        }
        Msg_Show(g_players[g_curPlayer].isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,MSG_ANY_BUTTON,g_msgBuf);
        TextWin_SetFocus(work->win,0);
        *step += 1;
        break;
    case STEAL_WAIT_RESULT:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            TextWin_SetFocus(work->win,1);
            *step = STEAL_CLOSE;
        }
        break;
    case STEAL_CLOSE:
        work->mode = 1;
        TextWin_SetFocus(work->win,0);
        Task_SendMessage(g_uiTaskIds[4],0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case STEAL_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->listShown = 0;
            *step += 1;
        }
        break;
    case STEAL_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }
    if (work->refresh != 0 && work->listShown != 0)
    {
        store = &g_stores[work->store];
        strcpy(g_msgBuf,"%LItem Name          Price%K");
        for (i = 0; i < store->numGoods; i++)
        {
            name = store->type->items[store->goods[i].level].name;
            strcat(g_msgBuf,name);
            Str_FillSpaces(14 - strlen(name),g_msgBuf + strlen(g_msgBuf));
            Str_FormatInt(store->goods[i].price,6,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"%K");
        }
        strcat(g_msgBuf,"Faild");
        strcat(g_msgBuf,"%E");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_SetMenu(work->win,1,store->numGoods + 1,store->numGoods - 1);
        work->refresh = 0;
    }
    work->arrived = Camera_MoveTo((char)work->mode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    if (work->mode == 2 && work->arrived != 0)
    {
        Menu_DrawFrameCursor(0x2f,0x46);
    }
    return 0;
}
