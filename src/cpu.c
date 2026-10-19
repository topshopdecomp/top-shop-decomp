/*
 * cpu.c - the CPU players' decisions
 *
 * How a computer player plays: which route it takes after its roll (valued by the bank, the rent
 * it would pay and its own stores on the way), which store it opens on a vacant lot, what it
 * buys, delivers or sells, whether it accepts a store offer or rides the elevator, which event
 * card it buys in the points exchange, and whom it plays each event card on (CpuTarget_*, one
 * per card).
 */
#include "common.h"
#include "topshop.h"

/* A route as Cpu_ChooseRoute sees it */
typedef struct RouteEval
{
    signed char x, floor;     /* 0x00 the square it ends on */
    signed char kind;         /* 0x02 ROUTE_* */
    signed char bankDist;     /* 0x03 g_routeBankDist */
    u16 cell;                 /* 0x04 Board_GetCell of the end */
    u16 pad06;
    int score;                /* 0x08 */
    int money;                /* 0x0C payday on the way (if the bank is on it), less the rent at the end */
    int rent;                 /* 0x10 rents between the end and the bank */
} RouteEval;

#define ROUTE_AT_BANK        0   /* ends on the bank */
#define ROUTE_PAST_BANK      1   /* passes the bank (payday) */
#define ROUTE_SHORT          2   /* does not reach it */
#define ROUTE_PAST_BANK_DEBT 3   /* as 1, and the rent at the end leaves the money below 0 */
#define ROUTE_SHORT_DEBT     4   /* as 2, the same */

/* The rent a player pays when ending on `cell` (the price of the last goods slot); -1 unless it
 * is a rival's store with goods */
static int Cpu_RentAt(u16 cell, u16 player)
{
    if (CELL_TYPE(cell) != CELL_STORE || g_stores[cell].owner == -1 ||
        g_stores[cell].owner == player || g_stores[cell].numGoods <= 0)
    {
        return -1;
    }
    return g_stores[cell].goods[g_stores[cell].numGoods - 1].price;
}

/* Squares of route `route` (g_routes) that are stores of a kind for `player` */
#define ON_ROUTE_OWN     0   /* its own stores */
#define ON_ROUTE_VACANT  1   /* vacant lots */
#define ON_ROUTE_RIVAL   2   /* other players' stores */
static int Cpu_CountOnRoute(int route, int player, int kind)
{
    u16 cell;
    int owner;
    int count;
    int i;

    count = 0;
    for (i = 0; i < g_routes.maxSteps; i++)
    {
        cell = Board_GetCell(g_routes.step[route][i].x,g_routes.step[route][i].floor);
        if (CELL_TYPE(cell) != CELL_STORE)
        {
            continue;
        }
        owner = g_stores[cell & 0xff].owner;
        if ((kind == ON_ROUTE_OWN && owner == player) || (kind == ON_ROUTE_VACANT && owner == -1) ||
            (kind == ON_ROUTE_RIVAL && owner != -1 && owner != player))
        {
            count++;
        }
    }
    return count;
}

/* Route index (g_routes) a CPU player takes for the dice roll `steps`. Every route is valued by
 * where it ends relative to the bank (the payday), the rent there and the rents between it and
 * the bank. A route ending on the bank, else one passing it, else the best of the others: one
 * that keeps the money at or above 0, then the higher score (ties: a coin toss). The routes
 * are built again afterwards, and the choice found among them by its end; -1 if it is not
 * there (0x80085CB4) */
/* 0x80085CB4 */
int Cpu_ChooseRoute(u16 player, u8 steps)
{
    RouteEval routes[4];
    RouteEval *r;
    RouteEval *best;
    signed char *elevator;
    u16 bankLeft;
    int maxRent;
    int rent;
    int payday;
    int money;
    int score;
    int choice;
    int keep;
    int t;
    int i;

    if (g_routes.count == 1)
    {
        return 0;
    }
    Player_FindNearestBank(player);
    g_bankDistance = steps;
    payday = (steps == 0) ? 0 : Player_PaydayAfter(player,steps,1);

    for (i = 0; i < g_routes.count; i++)
    {
        r = &routes[i];
        r->x = g_routes.step[i][steps - 1].x;
        r->floor = g_routes.step[i][steps - 1].floor;
        r->cell = Board_GetCell(r->x,r->floor);
        r->bankDist = g_routeBankDist[i];
        if (g_routeBankDist[i] == steps)
        {
            r->kind = ROUTE_AT_BANK;
            r->money = payday;
        }
        else if (g_routeBankDist[i] < steps)
        {
            r->kind = ROUTE_PAST_BANK;
            r->money = payday;
            rent = Cpu_RentAt(r->cell,player);
            if (rent >= 0)
            {
                r->money -= rent;
                if (r->money < 0)
                {
                    r->kind = ROUTE_PAST_BANK_DEBT;
                }
            }
        }
        else
        {
            r->money = 0;
            r->kind = ROUTE_SHORT;
            rent = Cpu_RentAt(r->cell,player);
            if (rent >= 0)
            {
                r->money -= rent;
                if (r->money < 0)
                {
                    r->kind = ROUTE_SHORT_DEBT;
                }
            }
        }
        r->rent = 0;
        for (t = steps - 1; t < g_routeBankDist[i]; t++)
        {
            rent = Cpu_RentAt(Board_GetCell(g_routes.step[i][t].x,g_routes.step[i][t].floor),player);
            if (rent >= 0)
            {
                r->rent += rent;
            }
        }
        /* debug values: per route the end square, its store's goods (read for any square) and the
         * money */
        g_cpuDebugVals[i * 3 + 4] = r->x * 100 + r->floor;
        g_cpuDebugVals[i * 3 + 5] = r->cell * 10000 + g_stores[r->cell].numGoods;
        g_cpuDebugVals[i * 3 + 6] = r->money;
    }

    maxRent = 0;
    for (i = 0; i < g_routes.count; i++)
    {
        if (maxRent < routes[i].rent)
        {
            maxRent = routes[i].rent;
        }
    }
    /* the score: closer to the bank (an elevator: the distance from its record), the money
     * kept, fewer rents to pay than the worst route */
    for (i = 0; i < g_routes.count; i++)
    {
        r = &routes[i];
        bankLeft = g_routeBankDist[i] - steps;
        if (CELL_TYPE(r->cell) == CELL_ELEVATOR && g_players[g_curPlayer].noElevator == 0)
        {
            elevator = g_stageDefs[g_boardStage].elevators[r->cell & 0xff];
            if ((u8)r->floor == elevator[elevator[0]])
            {
                bankLeft = elevator[elevator[0] + 6];
            }
            else
            {
                bankLeft = elevator[13];
            }
        }
        money = g_players[player].money;
        if (money == 0 || money + r->money < 0)
        {
            score = 0;
        }
        else
        {
            score = 4000 + r->money * 4000 / money;
        }
        r->score = (5000 - (int)(bankLeft * 5000U) / g_stageDefs[g_boardStage].payScale) + score +
                   (maxRent - r->rent) / 10;
    }

    /* the choice */
    choice = -1;
    for (i = 0; i < g_routes.count && choice < 0; i++)
    {
        if (routes[i].kind == ROUTE_AT_BANK)
        {
            choice = i;
        }
    }
    for (i = 0; i < g_routes.count && choice < 0; i++)
    {
        if (routes[i].kind == ROUTE_PAST_BANK)
        {
            choice = i;
        }
    }
    if (choice < 0)
    {
        choice = 0;
        for (i = 1; i < g_routes.count; i++)
        {
            best = &routes[choice];
            r = &routes[i];
            money = g_players[player].money;
            if (money + best->money < 0 && money + r->money >= 0)
            {
                choice = i;                         /* keeps the money at or above 0 */
                continue;
            }
            if (money + best->money >= 0 && money + r->money < 0)
            {
                continue;
            }
            if (money + best->money < 0)
            {
                /* both below 0: the smaller debt */
                if (money + r->money > money + best->money)
                {
                    choice = i;
                    continue;
                }
                keep = money + r->money < money + best->money;
            }
            else
            {
                if (r->score > best->score)
                {
                    choice = i;
                    continue;
                }
                keep = r->score < best->score;
            }
            if (!keep && (rand() & 1) != 0)
            {
                choice = i;                         /* a tie */
            }
        }
    }

    /* find it among the routes built again */
    Player_BuildRoutes(player,steps);
    for (i = 0; i < g_routes.count; i++)
    {
        if (routes[choice].x == g_routes.step[i][steps - 1].x &&
            routes[choice].floor == g_routes.step[i][steps - 1].floor)
        {
            return i;
        }
    }
    for (i = 15; i >= 0; i--)
    {
        g_cpuDebugVals[i] = -1;
    }
    return -1;
}

/* The most expensive goods (price in the last used goods slot) of the other players' stores on
 * the first squares of the way to the nearest bank, as many as the character looks ahead
 * (k_cpuBankDistance): what the CPU player may have to spend before its next payday */
static short Cpu_MostExpensiveOnWayToBank(u32 player)
{
    char charId = g_players[player & 0xffff].charId;
    short squares;
    short mostExpensive;
    short cell;
    Store *st;
    int i;

    Player_FindNearestBank(player);
    squares = g_bankDistance;
    if (k_cpuBankDistance[charId] < g_bankDistance)
    {
        squares = k_cpuBankDistance[charId];
    }
    mostExpensive = 0;
    for (i = 0; i < squares; i++)
    {
        cell = Board_GetCell(g_routes.step[g_bankRoute][i].x,g_routes.step[g_bankRoute][i].floor);
        if (CELL_TYPE(cell) != CELL_STORE)
        {
            continue;
        }
        st = &g_stores[cell];
        if (st->owner != -1 && st->owner != g_curPlayer && st->numGoods > 0 &&
            mostExpensive < st->goods[st->numGoods - 1].price)
        {
            mostExpensive = st->goods[st->numGoods - 1].price;
        }
    }
    return mostExpensive;
}

/* Entry of the vacant lot's store list (prices: count entries) a CPU player opens; count for
 * none. A plan made earlier (cpuPlan 1) decides. Otherwise none when buying the store out would
 * bring its owner (ownerGain: what it gets) to 80% of the goal, or the last entry is too
 * expensive; else, with the money left after the last entry's price and the most expensive goods
 * on the way to the bank, a chance by that money and the bank's distance (k_cpuOpenChance), then
 * the first entry whose price is within the last one's plus a random k_cpuPriceRange % of the
 * money left (0x80086934) */
/* 0x80086934 */
int Cpu_ChooseStoreToOpen(u32 player, u32 store, u32 list, short *prices, int count, int ownerGain, char owner)
{
    Player *pl = &g_players[player & 0xffff];
    char charId;
    short plan;
    int lastPrice;
    int left;
    int moneyBand;
    int distanceBand;
    int budget;
    int i;

    if (pl->cpuPlan == 1)
    {
        plan = pl->cpuPlanArg;
        pl->cpuPlan = 0;
        pl->cpuPlanArg = 0;
        return plan;
    }
    charId = pl->charId;
    if (ownerGain > 0 && g_gameConfig.goalMoney * 8 / 10 <= g_players[owner].assets + ownerGain)
    {
        return count;
    }
    lastPrice = prices[count - 1];
    if (pl->money < lastPrice)
    {
        return count;
    }
    left = pl->money - lastPrice - Cpu_MostExpensiveOnWayToBank(player);
    if (left < 0)
    {
        moneyBand = 3;
    }
    else if (left < pl->assets / 10)
    {
        moneyBand = 2;
    }
    else if (left < pl->assets / 2)
    {
        moneyBand = 1;
    }
    else
    {
        moneyBand = 0;
    }
    if (g_bankDistance < 9)
    {
        distanceBand = 0;
    }
    else if (g_bankDistance <= g_stageDefs[g_boardStage].payScale / 2)
    {
        distanceBand = 1;
    }
    else
    {
        distanceBand = 2;
    }
    if ((short)(rand() % 100) >= k_cpuOpenChance[charId][moneyBand][distanceBand])
    {
        return count;
    }
    budget = lastPrice + left * (k_cpuPriceRange[charId][0] +
                                 rand() % (k_cpuPriceRange[charId][1] - k_cpuPriceRange[charId][0])) / 100;
    for (i = 0; i < count; i++)
    {
        if (prices[i] <= budget)
        {
            return i;
        }
    }
    return count;
}

/* Store a CPU player sells (-1 when cancel is 1, i.e. it may decline: it does). Among its stores,
 * in this order: the shortest chain; one whose sale price (percent % of its value) covers the
 * debt; in a chain of 3 one not at position 1, in a chain of 4 one not at position 1 or 2; the
 * one next to the shorter chains of rivals; the fewer goods; the lower price. Ties keep the
 * earlier store (0x80087024) */
/* 0x80087024 */
u32 Cpu_ChooseStoreToSell(u16 player, char cancel, char percent)
{
    u8 rivalChains[MAX_STORES];               /* length of the rivals' chains next to the store */
    u16 salePrice[MAX_STORES];
    Store *st;
    Store *a;
    Store *b;
    short cell;
    short rival;
    int debt;
    int best;
    int cand;
    int i;

    if (cancel == 1)
    {
        return -1;
    }
    debt = -g_players[player].money;
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        rivalChains[i] = 0;
        salePrice[i] = 0;
        if (st->owner != player)
        {
            continue;
        }
        rival = -1;
        cell = Board_GetCell(st->x - 1,st->floor);
        if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner != -1 &&
            g_stores[cell].owner != st->owner)
        {
            rivalChains[i] = g_stores[cell].chainLen + 1;
            rival = g_stores[cell].owner;
        }
        cell = Board_GetCell(st->x + 1,st->floor);
        if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner != -1 &&
            g_stores[cell].owner != player)
        {
            if (rival == -1)
            {
                rivalChains[i] = g_stores[cell].chainLen + 1;
            }
            else if (rival == g_stores[cell].owner)
            {
                rivalChains[i] += g_stores[cell].chainLen + 1;          /* one rival on both sides */
            }
            else if (rivalChains[i] < g_stores[cell].chainLen + 1)
            {
                rivalChains[i] = g_stores[cell].chainLen + 1;
            }
        }
        salePrice[i] = (Store_GetValue(i) * percent) / 100;
    }

    Stores_ListByOwner(player);
    best = g_storeList[0];
    for (i = 1; i < g_storeListCount; i++)
    {
        cand = g_storeList[i];
        a = &g_stores[best];
        b = &g_stores[cand];
        if (b->chainLen != a->chainLen)
        {
            if (b->chainLen < a->chainLen)
            {
                best = cand;
            }
            continue;
        }
        if ((salePrice[best] >= debt) < (salePrice[cand] >= debt))
        {
            best = cand;
            continue;
        }
        if (a->chainLen == 2)
        {
            if ((a->chainPos != 1) < (b->chainPos != 1))
            {
                best = cand;
                continue;
            }
        }
        else if (a->chainLen == 3)
        {
            if (!(a->chainPos - 1U < 2) < !(b->chainPos - 1U < 2))
            {
                best = cand;
                continue;
            }
        }
        if (rivalChains[cand] != rivalChains[best])
        {
            if (rivalChains[cand] < rivalChains[best])
            {
                best = cand;
            }
            continue;
        }
        if (b->numGoods != a->numGoods)
        {
            if (b->numGoods < a->numGoods)
            {
                best = cand;
            }
            continue;
        }
        if (salePrice[cand] < salePrice[best])
        {
            best = cand;
        }
    }
    return best;
}

/* Whether a CPU player turns down buying a store for price: not when a plan made earlier
 * (cpuPlan 2) says to buy; yes when it lacks the money or the owner (ownerGain: what it gets)
 * would reach 80% of the goal; else when the price is over a share (k_cpuOfferAccept: by how many
 * of its own stores are next to it) of its money less the most expensive goods on the way to the
 * bank (0x800874A0) */
/* 0x800874A0 */
int Cpu_RefuseStoreOffer(u32 player, u32 store, int price, int ownerGain, char owner)
{
    Player *pl = &g_players[player & 0xffff];
    Store *st = &g_stores[store & 0xffff];
    char charId;
    short mostExpensive;
    short ownNext;
    short cell;

    if (pl->cpuPlan == 2)
    {
        pl->cpuPlan = 0;
        pl->cpuPlanArg = 0;
        return 0;
    }
    charId = pl->charId;
    if (pl->money < price ||
        (ownerGain > 0 && g_gameConfig.goalMoney * 8 / 10 <= g_players[owner].assets + ownerGain))
    {
        return 1;
    }
    mostExpensive = Cpu_MostExpensiveOnWayToBank(player);
    /* its stores in the chains on either side (a sum above 3 reads the next character's row) */
    ownNext = 0;
    cell = Board_GetCell(st->x - 1,st->floor);
    if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner == (player & 0xffff))
    {
        ownNext = g_stores[cell].chainLen + 1;
    }
    cell = Board_GetCell(st->x + 1,st->floor);
    if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner == (player & 0xffff))
    {
        ownNext += g_stores[cell].chainLen + 1;
    }
    return (pl->money - mostExpensive) * k_cpuOfferAccept[charId][ownNext] / 100 < price;
}

/* Store type `type` is offered when buying store `st` out: one not on the board yet, or the
 * store's own type when it is its owner's only store of that type */
static int Cpu_BuyOutOffers(Store *st, int type)
{
    int onlyOne = Stores_CountOwnedOfType(st->owner,(char)st->storeType) == 1;

    return g_storeTypes[type].available != 0 || (onlyOne && (char)st->storeType == type);
}

/* How many goods a CPU player buys in someone else's store: all of them when it can and then
 * means to take the store over (the buy-out list of VacantLot_Task, or StoreOffer_Task when it has
 * a store on this square), else 1. The decision for that dialog goes to Player.cpuPlan (1: the
 * buy-out list, 2: the store offer) and cpuPlanArg (the row, or the answer) (0x80087870) */
/* 0x80087870 */
int Cpu_ChooseItemToBuy(u16 player, u16 store)
{
    Store *st = &g_stores[store];
    u8 types[4];
    short prices[4];
    int total;                                /* what the goods cost here */
    int cost;                                 /* what they cost the owner */
    int buyPrice;
    int ownerGain;
    int hasOwn;
    int plan;
    int refuse;
    int count;
    int type;
    int i;
    char planArg;
    u8 ownType;

    total = 0;
    cost = 0;
    for (i = 0; i < st->maxGoods; i++)
    {
        if (st->goods[i].active != 0)
        {
            total += st->goods[i].price;
            cost += st->type->items[st->goods[i].level].cost;
        }
    }
    /* a store of the current player on this square? */
    hasOwn = 0;
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        if (g_stores[i].owner == g_curPlayer && g_stores[i].squareId == st->squareId)
        {
            ownType = g_stores[i].storeType;
            hasOwn = 1;
            break;
        }
    }
    /* the default: decline (the last row of the list, or "No") */
    plan = 2;
    if (hasOwn == 0)
    {
        planArg = 0;
        for (i = 0; i < 4; i++)
        {
            if (Cpu_BuyOutOffers(st,(signed char)(st->squareId * 4 + i)))
            {
                planArg++;
            }
            plan = 1;
        }
    }
    else
    {
        planArg = 1;
    }
    if (g_gameConfig.maxBuy < st->numGoods || g_players[player].money < total)
    {
        g_players[player].cpuPlan = plan;
        g_players[player].cpuPlanArg = planArg;
        return 1;
    }

    /* the buy-out price: the store's value times 1.5, 2, 2.5 or 3 by its chain */
    buyPrice = 0;                             /* (other chain lengths: unset in the original) */
    switch (st->chainLen)
    {
    case 0:
        buyPrice = (st->type->value * 2 + st->type->value) / 2;
        break;
    case 1:
        buyPrice = st->type->value * 2;
        break;
    case 2:
        buyPrice = (st->type->value * 4 + st->type->value) / 2;
        break;
    case 3:
        buyPrice = st->type->value * 3;
        break;
    }
    ownerGain = buyPrice - st->type->value;
    if (hasOwn == 0)
    {
        /* the buy-out list as VacantLot_Task will show it, each price with the goods' total */
        count = 0;
        for (i = 0; i < 4; i++)
        {
            type = (signed char)(st->squareId * 4 + i);
            if (Cpu_BuyOutOffers(st,type))
            {
                types[count] = type;
                prices[count] = StoreType_GetPrice(type,0) + buyPrice;
                count++;
            }
        }
        for (i = 0; i < count; i++)
        {
            prices[i] += total;
        }
        planArg = Cpu_ChooseStoreToOpen(player,store,(u32)types,prices,count,
                                        ownerGain + (total - cost),st->owner);
        refuse = (planArg == count);
        plan = 1;
    }
    else
    {
        refuse = Cpu_RefuseStoreOffer(player,store,
                                      total + buyPrice +
                                      StoreType_GetPrice((char)ownType,Store_CountOwnedNeighbors(player,store)),
                                      ownerGain + (total - cost),st->owner);
        planArg = refuse;
        plan = 2;
    }
    g_players[player].cpuPlan = plan;
    g_players[player].cpuPlanArg = planArg;
    return ((refuse & 0xff) == 0) ? st->numGoods : 1;
}

/* The store a CPU player delivers goods to, or -1. Candidates are its stores at the lowest fill
 * level any of them has (0-1 goods, exactly 2, or more) that have room, if the money (less
 * what it may spend on the way to the bank; above 1 goods only its k_cpuDeliveryRatio share)
 * covers their cheapest goods. Among them: at fill level 0-2 the longest chain, then the
 * emptiest; above that the most room, then the longest chain; then the most valuable store,
 * a coin toss when equal (0x80087E70) */
/* 0x80087E70 */
int Cpu_ChooseStoreToDeliver(u32 player)
{
    Store *st;
    Store *a;
    Store *b;
    u8 charId;
    u8 distance;
    char minGoods;
    short cell;
    short routePrice;
    short cheapest;
    int money;
    int ratio;
    int best;
    int store;
    int better;
    int value;
    int candidateValue;
    int i;
    int j;

    /* the most expensive goods on the way to the bank, as in Cpu_ChooseDelivery */
    charId = g_players[player & 0xffff].charId;
    Player_FindNearestBank(player);
    routePrice = 0;
    distance = k_cpuBankDistance[(char)charId];
    if (g_bankDistance <= distance)
    {
        distance = g_bankDistance;
    }
    for (i = 0; i < (char)distance; i++)
    {
        cell = Board_GetCell(g_routes.step[g_bankRoute][i].x,g_routes.step[g_bankRoute][i].floor);
        if (CELL_TYPE(cell) == CELL_STORE)
        {
            st = &g_stores[cell];
            if ((st->owner != -1) && (st->owner != (player & 0xffff)) && (0 < st->numGoods) &&
                (routePrice < st->goods[st->numGoods - 1].price))
            {
                routePrice = st->goods[st->numGoods - 1].price;
            }
        }
    }
    money = g_players[player & 0xffff].money - routePrice;
    if (g_bankDistance < 9)
    {
        ratio = k_cpuDeliveryRatio[(char)charId][0];
    }
    else if (g_stageDefs[g_boardStage].payScale / 2 < g_bankDistance)
    {
        ratio = k_cpuDeliveryRatio[(char)charId][2];
    }
    else
    {
        ratio = k_cpuDeliveryRatio[(char)charId][1];
    }

    /* the fewest goods any of its stores has */
    minGoods = 8;
    for (j = 0, st = g_stores; j < g_stageDefs[g_boardStage].numStores; j++, st++)
    {
        if ((st->owner == (player & 0xffff)) && (st->numGoods < minGoods))
        {
            minGoods = st->numGoods;
        }
    }

    /* the candidates */
    g_storeListCount = 0;
    for (j = 0, st = g_stores; j < g_stageDefs[g_boardStage].numStores; j++, st++)
    {
        if (st->owner != (player & 0xffff))
        {
            continue;
        }
        cheapest = st->type->items[5].cost;
        if (st->maxGoods <= st->numGoods)
        {
            continue;
        }
        if ((0 <= minGoods) && (minGoods <= 1))
        {
            if ((1 < st->numGoods) || (money < cheapest))
            {
                continue;
            }
        }
        else if (minGoods == 2)
        {
            if ((st->numGoods != 2) || ((money * ratio) / 100 < cheapest))
            {
                continue;
            }
        }
        else if ((st->numGoods <= 2) || ((money * ratio) / 100 < cheapest))
        {
            continue;
        }
        g_storeList[g_storeListCount] = j;
        g_storeListCount++;
    }
    if (g_storeListCount == 0)
    {
        return -1;
    }

    /* the best of them */
    best = (char)g_storeList[0];
    for (i = 1; i < g_storeListCount; i++)
    {
        store = (char)g_storeList[i];
        a = &g_stores[best];
        b = &g_stores[store];
        if ((0 <= minGoods) && (minGoods < 3))
        {
            better = a->chainLen < b->chainLen ? 1 : a->chainLen > b->chainLen ? -1 :
                     a->numGoods > b->numGoods ? 1 : a->numGoods < b->numGoods ? -1 : 0;
        }
        else
        {
            better = a->maxGoods - a->numGoods < b->maxGoods - b->numGoods ? 1 :
                     a->maxGoods - a->numGoods > b->maxGoods - b->numGoods ? -1 :
                     a->chainLen < b->chainLen ? 1 : a->chainLen > b->chainLen ? -1 : 0;
        }
        if (better == 0)
        {
            value = Store_GetValue(best);
            candidateValue = Store_GetValue(store);
            better = (value < candidateValue) || ((value == candidateValue) && (rand() & 1) != 0);
        }
        if (0 < better)
        {
            best = store;
        }
    }
    return best;
}

/* Cpu_ChooseDelivery's ways to spend the budget (picked by a roll against k_cpuDeliveryChance) */
enum CpuDeliveryStrategy
{
    CPUDELIV_FILL,               /* the slots up to 3 at least the cheapest goods, better ones while
                                    the money lasts */
    CPUDELIV_BEST_UP_TO_3,       /* the most expensive level a share of the budget (shared by the
                                    slots up to 3) pays for, as many as the budget allows */
    CPUDELIV_BEST_EVERY_SLOT,    /* the same, the budget shared by the free slots */
    CPUDELIV_ONE_EXPENSIVE       /* one expensive item, the cheapest goods for the other slots */
};

/* Goods a CPU player delivers to its store: fills items[] with goods levels (0xFF = none) and
 * returns how many. The budget is a share of the player's money (k_cpuDeliveryRatio; the share
 * depends on how far the bank is) after the most expensive goods it might buy in other players'
 * stores on the way to the bank. A roll against the character's k_cpuDeliveryChance picks one of
 * four ways to spend it (0x80088570) */
/* 0x80088570 */
int Cpu_ChooseDelivery(u32 player, u32 store, u8 *items)
{
    Store *st;
    Store *other;
    StoreItem *goods;
    u8 charId;
    u8 distance;
    char freeSlots;
    short cell;
    short routePrice;
    short cheapest;
    int fillCost;
    int budget;
    int perSlot;
    int extra;
    int money;
    int strategy;
    int first;
    int level;
    int lowest;
    int slots;
    int count;
    int roll;
    int i;

    charId = g_players[player & 0xffff].charId;
    for (i = 7; i >= 0; i--)
    {
        items[i] = 0xff;
    }
    st = &g_stores[store & 0xffff];
    freeSlots = st->maxGoods - st->numGoods;
    if (freeSlots == 0)
    {
        return 0;
    }

    /* the most expensive goods on the way to the bank (CPU players shop in other players' stores) */
    Player_FindNearestBank(player);
    routePrice = 0;
    distance = k_cpuBankDistance[(char)charId];
    if (g_bankDistance <= distance)
    {
        distance = g_bankDistance;
    }
    for (i = 0; i < (char)distance; i++)
    {
        cell = Board_GetCell(g_routes.step[g_bankRoute][i].x,g_routes.step[g_bankRoute][i].floor);
        if (CELL_TYPE(cell) == CELL_STORE)
        {
            other = &g_stores[cell];
            if ((other->owner != -1) && (other->owner != (player & 0xffff)) && (0 < other->numGoods) &&
                (routePrice < other->goods[other->numGoods - 1].price))
            {
                routePrice = other->goods[other->numGoods - 1].price;
            }
        }
    }
    money = g_players[player & 0xffff].money - routePrice;
    if (g_bankDistance < 9)
    {
        budget = k_cpuDeliveryRatio[(char)charId][0] * money;
    }
    else if (g_stageDefs[g_boardStage].payScale / 2 < g_bankDistance)
    {
        budget = k_cpuDeliveryRatio[(char)charId][2] * money;
    }
    else
    {
        budget = k_cpuDeliveryRatio[(char)charId][1] * money;
    }
    budget = budget / 100;

    goods = st->type->items;
    first = 6 - k_chainLevels[st->chainLen];      /* the most expensive level this store may sell */
    fillCost = (short)(goods[5].cost * (g_gameConfig.maxBuy - st->numGoods));
    if (budget < fillCost)
    {
        /* short of money: the cheapest goods, as many as the money allows */
        count = 0;
        money = g_players[player & 0xffff].money;
        for (i = 0; i < g_gameConfig.maxBuy; i++)
        {
            if (money < fillCost)
            {
                break;
            }
            count++;
            items[i] = 5;
            money -= fillCost;
        }
        return count;
    }

    roll = rand() % 100;
    for (strategy = 0; strategy < 3; strategy++)
    {
        if (roll < k_cpuDeliveryChance[(char)charId][strategy])
        {
            break;
        }
    }
    count = 0;
    switch (strategy)
    {
    case CPUDELIV_FILL:
        /* the slots up to 3 get at least the cheapest goods; better goods while the money lasts,
         * after those at most one level above the best bought so far */
        slots = 3 - st->numGoods;
        if (slots < 0)
        {
            slots = 0;
        }
        lowest = 5;
        if (0 < st->numGoods)
        {
            lowest = st->goods[0].level;
        }
        cheapest = goods[5].cost;
        budget -= cheapest * slots;
        for (i = 0; i < freeSlots; i++)
        {
            for (level = first; level < 6; level++)
            {
                if (count < slots ? goods[level].cost <= budget + cheapest
                                  : level <= lowest + 1 && goods[level].cost <= budget)
                {
                    items[count] = level;
                    budget -= goods[level].cost - cheapest;
                    count++;
                    if (level < lowest)
                    {
                        lowest = level;
                    }
                    break;
                }
            }
            if (level == 6)
            {
                break;
            }
        }
        return count;
    case CPUDELIV_BEST_UP_TO_3:
    case CPUDELIV_BEST_EVERY_SLOT:
        /* the most expensive level whose price fits a share of the budget (1: shared by the slots up
         * to 3, 2: by the free slots), then as many of it as the budget pays for */
        perSlot = budget;
        if (strategy == CPUDELIV_BEST_UP_TO_3)
        {
            if (0 < 3 - st->numGoods)
            {
                perSlot = budget / (3 - st->numGoods);
            }
        }
        else if (0 < freeSlots)
        {
            perSlot = budget / freeSlots;
        }
        for (level = first; level < 6; level++)
        {
            if (goods[level].cost <= perSlot)
            {
                break;
            }
        }
        if (level > 5)
        {
            level = 5;
        }
        for (i = 0; i < freeSlots; i++)
        {
            if (goods[level].cost <= budget)
            {
                items[count] = level;
                count++;
                budget -= goods[level].cost;
            }
        }
        return count;
    default:
        /* one item as expensive as the money left over after the cheapest goods allows, then the
         * cheapest goods for the other slots */
        cheapest = goods[5].cost;
        extra = budget - cheapest * freeSlots;
        if (0 < extra)
        {
            for (level = first; level < 6; level++)
            {
                if (goods[level].cost <= extra + cheapest)
                {
                    items[0] = level;
                    count = 1;
                    budget -= goods[level].cost - cheapest;
                    break;
                }
            }
        }
        for (i = 1; i < freeSlots; i++)
        {
            if (cheapest <= budget)
            {
                items[count] = 5;
                budget -= cheapest;
                count++;
            }
        }
        return count;
    }
}

/* Cursor of a CPU player at an elevator (0 Yes: ride, 1 No). With the bank far (over half the
 * stage's payScale away) it rides 90% of the time, 70% when its routes of 8 pass more than 2 vacant
 * lots in all and it has 2000 in money and 5000 in assets; with the bank near, as often as the
 * elevator's stops are at or above its floor (never at the last stop) (0x80088FFC) */
/* 0x80088FFC */
int Cpu_ElevatorChoice(u16 player, u32 elevator)
{
    Player *pl = &g_players[player];
    signed char *stops = g_stageDefs[g_boardStage].elevators[elevator & 0xff];
    short vacant;
    int chance;
    int above;
    int route;
    int i;

    Player_FindNearestBank(player);
    chance = 0;
    if (g_stageDefs[g_boardStage].payScale / 2 < g_bankDistance)
    {
        Player_BuildRoutes(player,8);
        vacant = 0;
        for (route = 0; route < g_routes.count; route++)
        {
            vacant += Cpu_CountOnRoute(route,player,ON_ROUTE_VACANT);
        }
        chance = 90;
        if (vacant > 2 && pl->money >= 2000 && pl->assets >= 5000)
        {
            chance = 70;
        }
    }
    else if (pl->floor != stops[stops[0]])
    {
        above = 0;
        for (i = 1; i <= stops[0]; i++)
        {
            if (pl->floor <= stops[i])
            {
                above++;
            }
        }
        chance = above * 100 / stops[0];
    }
    return (short)(rand() % 100) >= chance;
}

/* The Receive money cards, cheapest first, and their amounts */
static const u8 k_moneyCards[6] = {
    EV_MONEY_200, EV_MONEY_400, EV_MONEY_700, EV_MONEY_1000, EV_MONEY_1300, EV_MONEY_1600
  };
static const short k_moneyCardAmounts[6] = {200, 400, 700, 1000, 1300, 1600};

/* Whether the player has the points for event card `card` */
static int Cpu_CanBuy(int player, int card)
{
    return (char)g_eventCards[card].points <= g_players[player].points;
}

/* Event card a CPU player buys in the points exchange (EV_EARN_POINT for none) and its target.
 * Only while its assets are below the goal: by the character's chance (k_cpuExchange) a money card
 * covering what it lacks to the goal; then, with the character's money wanted at hand, the first
 * of its cards that has a use now (Cpu_EventTarget), else a money card for the money it lacks
 * (0x80089360) */
/* 0x80089360 */
int Cpu_ChooseExchange(u32 player, u8 *target)
{
    Player *pl = &g_players[player & 0xffff];
    CpuExchangePref *pref = &k_cpuExchange[(char)pl->charId];
    char card;
    u32 who;
    int needed;
    u8 *wanted;

    *target = 0;
    if (pl->assets >= g_gameConfig.goalMoney)
    {
        return EV_EARN_POINT;
    }
    if ((char)(rand() % 100) < pref->chance)
    {
        card = Cpu_ExchangeForAssets(player,g_gameConfig.goalMoney - pl->assets,target);
        if (card != -1)
        {
            return card;
        }
    }
    needed = pref->moneyNeeded - pl->money;
    if (needed <= 0)
    {
        for (wanted = pref->cards; *wanted != 0xff; wanted++)
        {
            who = Cpu_EventTarget(player,*wanted);
            if (who != -1)
            {
                *target = who;
                return (char)*wanted;
            }
        }
    }
    else
    {
        card = Cpu_ExchangeForMoney(player & 0xffff,needed);
        if (card != -1)
        {
            *target = 0;
            return card;
        }
    }
    return EV_EARN_POINT;
}

/* Card that makes up what the player lacks to reach the goal (shortfall), the first that does
 * and that it has the points for: the Receive money cards from the cheapest, stores x 150, stores
 * x 300, then (the target: the richest rival still in) half that rival's money, as much as it
 * has; -1 for none (0x80089560) */
/* 0x80089560 */
u32 Cpu_ExchangeForAssets(u16 player, int shortfall, u8 *target)
{
    char stores;
    u8 richest;
    int money;
    int i;

    Stores_ListByOwner(player);
    stores = g_storeListCount;
    richest = 0;
    money = -0x80000000;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (i != player && g_players[i].out == 0 && money < g_players[i].money)
        {
            money = g_players[i].money;
            richest = i;
        }
    }
    *target = 0;
    for (i = 0; i < 6; i++)
    {
        if (Cpu_CanBuy(player,k_moneyCards[i]) && shortfall <= k_moneyCardAmounts[i])
        {
            return k_moneyCards[i];
        }
    }
    if (Cpu_CanBuy(player,EV_MONEY_STORES_150) && shortfall <= stores * 150)
    {
        return EV_MONEY_STORES_150;
    }
    if (Cpu_CanBuy(player,EV_MONEY_STORES_300) && shortfall <= stores * 300)
    {
        return EV_MONEY_STORES_300;
    }
    if (Cpu_CanBuy(player,EV_TAKE_HALF_MONEY) && shortfall <= money / 2)
    {
        *target = richest;
        return EV_TAKE_HALF_MONEY;
    }
    if (Cpu_CanBuy(player,EV_RIVALS_MONEY) && shortfall <= money)
    {
        *target = richest;
        return EV_RIVALS_MONEY;
    }
    return -1;
}

/* Receive money card for the money needed: the cheapest that covers it (the most expensive when
 * none does), or the next cheaper one the player has the points for; -1 for none (0x80089910) */
/* 0x80089910 */
u32 Cpu_ExchangeForMoney(u32 player, int needed)
{
    int i;

    i = 0;
    while (i < 5 && k_moneyCardAmounts[i] < needed)
    {
        i++;
    }
    for (; i >= 0; i--)
    {
        if (Cpu_CanBuy(player & 0xffff,k_moneyCards[i]))
        {
            return k_moneyCards[i];
        }
    }
    return -1;
}

/* Whether every route of `steps` squares from the player's square has more than `more` squares of
 * that kind (no routes: yes) */
static int Cpu_EveryRouteHas(int player, int steps, int kind, int more)
{
    int route;

    Player_BuildRoutes(player,steps);
    for (route = 0; route < g_routes.count; route++)
    {
        if (Cpu_CountOnRoute(route,player,kind) <= more)
        {
            return 0;
        }
    }
    return 1;
}

/* The players of list (n) with the best (lowest) rankOrder, into best; returns how many */
static int Cpu_BestRanked(const u8 *list, int n, u8 *best)
{
    u8 rank;
    int count;
    int i;

    rank = 5;
    for (i = 0; i < n; i++)
    {
        if ((char)g_players[list[i]].rankOrder < (char)rank)
        {
            rank = g_players[list[i]].rankOrder;
        }
    }
    count = 0;
    for (i = 0; i < n; i++)
    {
        if (rank == g_players[list[i]].rankOrder)
        {
            best[count++] = list[i];
        }
    }
    return count;
}

/* The richest players of list (n), into best; returns how many */
static int Cpu_Richest(const u8 *list, int n, u8 *best)
{
    int money;
    int count;
    int i;

    money = 0;
    for (i = 0; i < n; i++)
    {
        if (money < g_players[list[i]].money)
        {
            money = g_players[list[i]].money;
        }
    }
    count = 0;
    for (i = 0; i < n; i++)
    {
        if (money == g_players[list[i]].money)
        {
            best[count++] = list[i];
        }
    }
    return count;
}

/* A random one of the first n entries of list; -1 (no rand() call) when n is 0 */
static int Cpu_PickRandom(const u8 *list, int n)
{
    if (n == 0)
    {
        return -1;
    }
    return list[rand() % n];
}

/* For the money events: the candidates with at least `money`; a random one of them, picked among
 * as many of the first ones as there are best ranked (byRank) or richest ones among them (the
 * original builds that list but draws from the first one) */
static int Cpu_TargetWithMoney(const u8 *cands, int n, int money, int byRank)
{
    u8 rich[8];
    u8 best[8];
    int count;
    int i;

    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].money >= money)
        {
            rich[count++] = cands[i];
        }
    }
    if (count == 0)
    {
        return -1;
    }
    count = byRank ? Cpu_BestRanked(rich,count,best) : Cpu_Richest(rich,count,best);
    return Cpu_PickRandom(rich,count);
}

/* For the store closing events: the candidates (ranked first, unless anyRank) with stores whose
 * chainLen is `length` (chains of length + 1 stores) */
static int Cpu_ListWithChains(const u8 *cands, int n, int length, int anyRank, u8 *list)
{
    int count;
    int i;

    count = 0;
    for (i = 0; i < n; i++)
    {
        Stores_ListByChainLength(cands[i],length);
        if (g_storeListCount != 0 && (anyRank || g_players[cands[i]].rankOrder == 0))
        {
            list[count++] = cands[i];
        }
    }
    return count;
}

/* Target a CPU player picks for an event card (see CpuTarget_*): a player, 0 for cards without a
 * target (or -1 when the card is not of use now), -1 when it cannot pay its points (0x80089B84) */
/* 0x80089B84 */
u32 Cpu_EventTarget(u8 player, u32 event)
{
    u8 cands[8];
    u8 n;
    int i;

    /* the players the card may target */
    n = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if ((g_eventTargets[event & 0xff] >> i) & 1)
        {
            cands[n++] = i;
        }
    }
    event &= 0xff;
    if (g_players[player].points < (char)g_eventCards[event].points)
    {
        return -1;
    }
    switch (event)
    {
    case EV_EARN_POINT:
    case EV_MONEY_200:
    case EV_MONEY_400:
    case EV_MONEY_700:
    case EV_FREE_STORE:
    case EV_MONEY_1000:
    case EV_MONEY_1300:
    case EV_MONEY_1600:
    case EV_UPGRADE_STORE:
    case EV_CLAIM_VICTORY:
        return (g_eventTargets[event] != 0) ? 0 : -1;
    case EV_STOP_DELIVERY:
        return CpuTarget_StopDelivery(player,cands,n);
    case EV_STOP_NEW_STORES:
        return CpuTarget_StopNewStores(player,cands,n);
    case EV_DELIVER_OWN:
        return CpuTarget_DeliverOwn(player);
    case EV_REVERSE:
        return CpuTarget_Reverse(player,(char *)cands,n);
    case EV_CLOSE_STORE:
        return CpuTarget_CloseStore(player,(char *)cands,n);
    case EV_SLOW_DICE:
        return CpuTarget_SlowDice(player,cands,n);
    case EV_DICE_E:
        return CpuTarget_DiceE(player,cands,n);
    case EV_CAPTURE_STORE:
        return CpuTarget_CaptureStore(player,cands,n);
    case EV_MONEY_3_4:
        return CpuTarget_MoneyThreeQuarters(player,(char *)cands,n);
    case EV_SWITCH_PLACES:
        return CpuTarget_SwitchPlaces(player,(char *)cands,n);
    case EV_CUT_BY_STORES_150:
    case EV_CUT_BY_STORES_300:
        return CpuTarget_CutByStores(player);
    case EV_CLOSE_STORE_LV2:
        return CpuTarget_CloseStoreLv2(player,(char *)cands,n);
    case EV_ITEMS_DOUBLE:
        return CpuTarget_ItemsDouble(player);
    case EV_OUT_OF_STOCK_4:
        return CpuTarget_OutOfStock4();
    case EV_DELIVER_FREE:
        return CpuTarget_DeliverFree(player);
    case EV_DELIVER_CHEAP:
        return CpuTarget_DeliverCheap(player);
    case EV_DELIVER_EXPENSIVE:
        return CpuTarget_DeliverExpensive(player);
    case EV_HALVE_POINTS:
        return CpuTarget_HalvePoints(player,(char *)cands,n);
    case EV_MONEY_HALF:
        return CpuTarget_MoneyHalf(player,(char *)cands,n);
    case EV_MONEY_SHUFFLE:
        return Cpu_WantsMoneyShuffle(player);
    case EV_ALL_OUT_OF_STOCK:
        return Cpu_WantsEmptyStores();
    case EV_CLOSE_STORE_LV3:
        return CpuTarget_CloseStoreLv3(player,(char *)cands,n);
    case EV_HALVE_ALL:
        return Cpu_WantsHalveAll(player);
    case EV_MONEY_TO_1:
        return CpuTarget_MoneyToOne(player,(char *)cands,n);
    case EV_TAKE_HALF_MONEY:
        return CpuTarget_TakeHalfMoney(player,(char *)cands,n);
    case EV_CLOSE_STORE_LV4:
        return CpuTarget_CloseStoreLv4(player,(char *)cands,n);
    case EV_DELIVER_COSTLY:
        return CpuTarget_DeliverCostly(player);
    case EV_RIVALS_MONEY:
        return CpuTarget_RivalsMoney(player,(char *)cands,n);
    }
    return -1;
}

/* Not called: half of the time a random human candidate, unless a player may not be targeted
 * (0x80089F40) */
/* 0x80089F40 */
u32 CpuTarget_HumanUnused(u32 player, u8 *cands, char n)
{
    u8 humans[8];
    int count;
    int i;

    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].out == 0 && g_players[i].noTargeting != 0)
        {
            return -1;
        }
    }
    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].isCpu == 0)
        {
            humans[count++] = cands[i];
        }
    }
    if ((u8)(rand() % 100) < 50)
    {
        return -1;
    }
    return Cpu_PickRandom(humans,count);
}

/* Event 2, stop a rival's deliveries: the best ranked of the rivals whose every route of 8 squares
 * passes more than 2 of their own stores (0x8008A0AC) */
/* 0x8008A0AC */
u32 CpuTarget_StopDelivery(u32 player, u8 *cands, u8 n)
{
    u8 list[8];
    u8 best[8];
    int count;
    int i;

    count = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (Cpu_EveryRouteHas(cands[i],8,ON_ROUTE_OWN,2))
        {
            list[count++] = cands[i];
        }
    }
    if (count == 0)
    {
        return -1;
    }
    return Cpu_PickRandom(best,Cpu_BestRanked(list,count,best));
}

/* Event 3, stop a rival's new stores: the best ranked of the rivals whose every route of 8 squares
 * passes more than 2 vacant lots (0x8008A368) */
/* 0x8008A368 */
u32 CpuTarget_StopNewStores(u32 player, u8 *cands, u8 n)
{
    u8 list[8];
    u8 best[8];
    int count;
    int i;

    count = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (Cpu_EveryRouteHas(cands[i],8,ON_ROUTE_VACANT,2))
        {
            list[count++] = cands[i];
        }
    }
    if (count == 0)
    {
        return -1;
    }
    return Cpu_PickRandom(best,Cpu_BestRanked(list,count,best));
}

/* Event 11, a rival's pencil (die) 1 and 2: half of the time, the leader if all its routes of 5
 * squares are other players' stores only (0x8008A624) */
/* 0x8008A624 */
u32 CpuTarget_SlowDice(u32 player, u8 *cands, u8 n)
{
    u8 list[8];
    u8 leaders[8];
    int count;
    int leading;
    int i;

    count = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (Cpu_EveryRouteHas(cands[i],5,ON_ROUTE_RIVAL,4))
        {
            list[count++] = cands[i];
        }
    }
    if ((u8)(rand() % 100) >= 50 || count == 0)
    {
        return -1;
    }
    leading = 0;
    for (i = 0; i < count; i++)
    {
        if (g_players[list[i]].rankOrder == 0)
        {
            leaders[leading++] = list[i];
        }
    }
    return Cpu_PickRandom(leaders,leading);
}

/* Event 14, capture a rival store (one without goods): the rivals whose such stores sit best
 * next to the player's own (the most neighboring chain, then the best worst case), best ranked
 * (0x8008A8C8) */
/* 0x8008A8C8 */
u32 CpuTarget_CaptureStore(u16 player, u8 *cands, u8 n)
{
    u8 maxNext[8];                            /* per candidate: of its stores, the most and the */
    u8 minNext[8];                            /* fewest neighboring stores of the player's chains */
    u8 list[8];
    u8 best[8];
    u8 most;
    u8 worst;
    u8 adjacent;
    u16 cell;
    Store *st;
    int count;
    int i;
    int k;

    for (i = 0; i < (char)n; i++)
    {
        Stores_ListClosed(cands[i]);
        maxNext[i] = 0;
        minNext[i] = 3;
        for (k = 0; k < g_storeListCount; k++)
        {
            st = &g_stores[g_storeList[k]];
            adjacent = 0;
            cell = Board_GetCell(st->x - 1,st->floor);
            if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner == player)
            {
                adjacent = g_stores[cell].chainLen + 1;
            }
            cell = Board_GetCell(st->x + 1,st->floor);
            if (CELL_TYPE(cell) == CELL_STORE && g_stores[cell].owner == player)
            {
                adjacent += g_stores[cell].chainLen + 1;
            }
            if (maxNext[i] < adjacent)
            {
                maxNext[i] = adjacent;
            }
            if (adjacent < minNext[i])
            {
                minNext[i] = adjacent;
            }
        }
    }
    most = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (most < maxNext[i])
        {
            most = maxNext[i];
        }
    }
    worst = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (most == maxNext[i] && worst < minNext[i])
        {
            worst = minNext[i];
        }
    }
    count = 0;
    for (i = 0; i < (char)n; i++)
    {
        if (most == maxNext[i] && worst == minNext[i])
        {
            list[count++] = cands[i];
        }
    }
    if (count == 0)
    {
        return -1;
    }
    return Cpu_PickRandom(best,Cpu_BestRanked(list,count,best));
}

/* Event 12, a rival's pencil E: half of the time the leader, when it has 2000 more assets and the
 * player is past half the goal (0x8008AD5C) */
/* 0x8008AD5C */
u32 CpuTarget_DiceE(u32 player, u8 *cands, char n)
{
    u8 list[8];
    int count;
    int i;

    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].rankOrder == 0 &&
            g_players[player & 0xffff].assets + 2000 <= g_players[cands[i]].assets &&
            g_gameConfig.goalMoney / 2 <= g_players[player & 0xffff].assets)
        {
            list[count++] = cands[i];
        }
    }
    if ((u8)(rand() % 100) >= 50)
    {
        return -1;
    }
    return Cpu_PickRandom(list,count);
}

/* Event 19, switch places with a rival: 80% of the time, the best ranked rival less than 9 squares
 * from the bank (0x8008AED4) */
/* 0x8008AED4 */
int CpuTarget_SwitchPlaces(u32 player, char *cands, char n)
{
    u8 list[8];
    u8 best[8];
    int count;
    int i;

    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].bankDistance < 9)
        {
            list[count++] = cands[i];
        }
    }
    if ((char)(rand() % 100) >= 80 || count == 0)
    {
        return -1;
    }
    return (char)Cpu_PickRandom(best,Cpu_BestRanked(list,count,best));
}

/* Event 16, reduce a rival's money to 3/4: one with 10000 or more (Cpu_TargetWithMoney)
 * (0x8008B0F8) */
/* 0x8008B0F8 */
int CpuTarget_MoneyThreeQuarters(u32 player, char *cands, char n)
{
    return (char)Cpu_TargetWithMoney((u8 *)cands,n,10000,1);
}

/* Event 38, halve a rival's money: one with 8000 or more (0x8008B2D0) */
/* 0x8008B2D0 */
int CpuTarget_MoneyHalf(u32 player, char *cands, char n)
{
    return (char)Cpu_TargetWithMoney((u8 *)cands,n,8000,1);
}

/* Event 44, reduce a rival's money to 1: one with 3000 or more (0x8008B4A8) */
/* 0x8008B4A8 */
int CpuTarget_MoneyToOne(u32 player, char *cands, char n)
{
    return (char)Cpu_TargetWithMoney((u8 *)cands,n,3000,1);
}

/* Event 45, take half of a rival's money: one with 3000 or more, as many choices as the richest
 * (0x8008B680) */
/* 0x8008B680 */
int CpuTarget_TakeHalfMoney(u32 player, char *cands, char n)
{
    return (char)Cpu_TargetWithMoney((u8 *)cands,n,3000,0);
}

/* Event 51, receive as much money as a rival has: one with 5000 or more, as many choices as the
 * richest (0x8008B848) */
/* 0x8008B848 */
int CpuTarget_RivalsMoney(u32 player, char *cands, char n)
{
    return (char)Cpu_TargetWithMoney((u8 *)cands,n,5000,0);
}

/* Event 37, halve a rival's points: one with the most points, when that is 40 or more
 * (0x8008BA10) */
/* 0x8008BA10 */
int CpuTarget_HalvePoints(u32 player, char *cands, char n)
{
    u8 list[8];
    char most;
    int count;
    int i;

    most = 0;
    for (i = 0; i < n; i++)
    {
        if (most < g_players[cands[i]].points)
        {
            most = g_players[cands[i]].points;
        }
    }
    if (most < 40)
    {
        return -1;
    }
    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].points == most)
        {
            list[count++] = cands[i];
        }
    }
    return (char)Cpu_PickRandom(list,count);
}

/* Event 34, a free delivery: when the player has less than 2000 and a store in a chain of 4 with
 * less than 3 goods, or in a chain of 3 with less than 2 (0x8008BB68) */
/* 0x8008BB68 */
u32 CpuTarget_DeliverFree(u16 player)
{
    int i;

    if (g_players[player].money < 2000)
    {
        Stores_ListByChainLength(player,3);
        for (i = 0; i < g_storeListCount; i++)
        {
            if (g_stores[(char)g_storeList[i]].numGoods < 3)
            {
                return 0;
            }
        }
        Stores_ListByChainLength(player,2);
        for (i = 0; i < g_storeListCount; i++)
        {
            if (g_stores[(char)g_storeList[i]].numGoods < 2)
            {
                return 0;
            }
        }
    }
    return -1;
}

/* Not called; the count it uses is a register no caller sets (always 0 here: -1) (0x8008CE38) */
/* 0x8008CE38 */
u32 CpuTarget_RandomUnused(u32 player, int cands)
{
    return -1;
}

/* Not called: a random best ranked candidate (0x8008CEB0) */
/* 0x8008CEB0 */
u32 CpuTarget_LeastUnused(u32 player, u8 *cands, char n)
{
    u8 best[8];

    return Cpu_PickRandom(best,Cpu_BestRanked(cands,n,best));
}

/* Not called: a random one of the richest candidates (0x8008D00C) */
/* 0x8008D00C */
u32 CpuTarget_RichestUnused(u32 player, u8 *cands, char n)
{
    u8 best[8];

    return Cpu_PickRandom(best,Cpu_Richest(cands,n,best));
}

/* Goods slots free in the player's stores */
static int Cpu_FreeSlots(short player)
{
    int free;
    int i;

    Stores_ListByOwner(player);
    free = 0;
    for (i = 0; i < g_storeListCount; i++)
    {
        free += g_stores[g_storeList[i]].maxGoods - g_stores[g_storeList[i]].numGoods;
    }
    return free;
}

/* Event 35, deliver cheap goods: with 15 free goods slots or more (0x8008D144) */
/* 0x8008D144 */
u32 CpuTarget_DeliverCheap(short player)
{
    return (Cpu_FreeSlots(player) < 15) ? -1 : 0;
}

/* Event 48, deliver costly goods to the player's stores: with 20 free slots or more (0x8008D1D4) */
/* 0x8008D1D4 */
u32 CpuTarget_DeliverCostly(short player)
{
    return (Cpu_FreeSlots(player) < 20) ? -1 : 0;
}

/* Event 7, reverse a rival's direction: the leader when it is less than 5 squares from the bank
 * (0x8008D264) */
/* 0x8008D264 */
int CpuTarget_Reverse(u32 player, char *cands, char n)
{
    u8 list[16];
    int count;
    int i;

    count = 0;
    for (i = 0; i < n; i++)
    {
        if (g_players[cands[i]].bankDistance < 5 && g_players[cands[i]].rankOrder == 0)
        {
            list[count++] = cands[i];
        }
    }
    return (char)Cpu_PickRandom(list,count);
}

/* Event 8, close a rival's store (one not in a chain): half of the time the leader, if it has one
 * (0x8008D350) */
/* 0x8008D350 */
int CpuTarget_CloseStore(u32 player, char *cands, char n)
{
    u8 list[8];
    int count;

    count = Cpu_ListWithChains((u8 *)cands,n,0,0,list);
    if ((char)(rand() % 100) >= 50)
    {
        return -1;
    }
    return (char)Cpu_PickRandom(list,count);
}

/* Event 29, close one of a rival's stores (level 2: a chain of 2): the leader if it has one
 * (0x8008D4D0) */
/* 0x8008D4D0 */
int CpuTarget_CloseStoreLv2(u32 player, char *cands, char n)
{
    u8 list[8];

    return (char)Cpu_PickRandom(list,Cpu_ListWithChains((u8 *)cands,n,1,0,list));
}

/* Event 41, the same for a chain of 3 (0x8008D5F8) */
/* 0x8008D5F8 */
int CpuTarget_CloseStoreLv3(u32 player, char *cands, char n)
{
    u8 list[8];

    return (char)Cpu_PickRandom(list,Cpu_ListWithChains((u8 *)cands,n,2,0,list));
}

/* Event 46, the same for a chain of 4, any rival (0x8008D720) */
/* 0x8008D720 */
int CpuTarget_CloseStoreLv4(u32 player, char *cands, char n)
{
    u8 list[8];

    return (char)Cpu_PickRandom(list,Cpu_ListWithChains((u8 *)cands,n,3,1,list));
}

/* Event 31, the goods of the player's stores double: when its open stores are a quarter of the
 * stage's or more (0x8008D81C) */
/* 0x8008D81C */
u32 CpuTarget_ItemsDouble(short player)
{
    u32 owned;
    u32 closed;

    owned = Stores_ListByOwner(player);
    closed = Stores_ListClosed(player);
    return ((int)((owned - closed) & 0xff) < g_stageDefs[g_boardStage].numStores / 4) ? -1 : 0;
}

/* Events 27 and 43, cut every rival's money by its stores: when no lot is vacant and the player
 * is in the first half of the ranking (0x8008D89C) */
/* 0x8008D89C */
u32 CpuTarget_CutByStores(u32 player)
{
    if (Stores_ListVacant() != 0)
    {
        return -1;
    }
    return ((char)g_players[player & 0xffff].rankOrder < (g_numPlayers >> 1)) ? -1 : 0;
}

/* Event 39, the players' money shuffled: when every other player still in has 3000 more than the
 * player at least (0x8008D904) */
/* 0x8008D904 */
u32 Cpu_WantsMoneyShuffle(u32 player)
{
    int i;

    if (g_numPlayers == 1)
    {
        return -1;
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        if (i != (player & 0xffff) && g_players[i].out == 0 &&
            g_players[i].money < g_players[player & 0xffff].money + 3000)
        {
            return -1;
        }
    }
    return 0;
}

/* Event 42, halve every rival's money: when they have 5000 on average or more (0x8008D9A0) */
/* 0x8008D9A0 */
u32 Cpu_WantsHalveAll(u32 player)
{
    char rivals;
    int money;
    int i;

    rivals = 0;
    money = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (i != (player & 0xffff) && g_players[i].out == 0)
        {
            rivals++;
            money += g_players[i].money;
        }
    }
    return (money / rivals < 5000) ? -1 : 0;
}

/* Event 5, deliver to the player's store: when one of them has no goods (0x8008DA50) */
/* 0x8008DA50 */
u32 CpuTarget_DeliverOwn(short player)
{
    Stores_ListClosed(player);
    return (g_storeListCount == 0) ? -1 : 0;
}

/* Event 36, deliver an expensive item: when stores with free slots are a fifth of the stage's
 * (rounded up) or more (0x8008DA8C) */
/* 0x8008DA8C */
u32 CpuTarget_DeliverExpensive(short player)
{
    Stores_ListNotFull(player);
    return (g_storeListCount < (g_stageDefs[g_boardStage].numStores + 4) / 5) ? -1 : 0;
}

/* Event 32, a rival out of stock (4 stores): when 4 or more of the rivals' stores have an item of
 * level 0 or 1 in their first goods slot (0x8008DB08) */
/* 0x8008DB08 */
u32 CpuTarget_OutOfStock4(void)
{
    Store *st;
    char count;
    int i;

    Stores_ListByOwner(WHO_OTHERS);
    count = 0;
    for (i = 0; i < g_storeListCount; i++)
    {
        st = &g_stores[(char)g_storeList[i]];
        if (st->goods[0].active != 0 && st->goods[0].level < 2)
        {
            count++;
        }
    }
    return (count < 4) ? -1 : 0;
}

/* Event 40, every store out of stock: when 4 or more are without goods already (0x8008DBC0) */
/* 0x8008DBC0 */
u32 Cpu_WantsEmptyStores(void)
{
    Stores_ListClosed(WHO_OTHERS);
    return (g_storeListCount < 4) ? -1 : 0;
}
