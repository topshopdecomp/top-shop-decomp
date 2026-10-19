/*
 * talk.c - which comment a character makes
 *
 * At the start and the end of a CPU player's turn its character comments in the talk panel (the
 * scripts in scripts/talk/). TurnStart_PickScript and TurnEnd_PickScript pick the situation (the
 * talk script's entry) from the player's rank and money, the squares ahead, and what happened
 * during the turn.
 */
#include "common.h"
#include "topshop.h"

/* Whether the player's rank (prev: the one at the start of the round) is strictly better (or, last:
 * worse) than every other player's */
static int Player_RankedAlone(u16 player, int prev, int last)
{
    char mine;
    char other;
    int i;

    mine = prev ? g_players[player].prevRank : g_players[player].rank;
    for (i = 0; i < g_numPlayers; i++)
    {
        other = prev ? g_players[i].prevRank : g_players[i].rank;
        if (i != player && (last ? mine <= other : other <= mine))
        {
            return 0;
        }
    }
    return 1;
}

/* TurnStart_PickScript: the situations, tried in this order (k_startChance: % of the time) */
enum
{
    START_FIRST_MONTH,      /* the first month */
    START_NEAR_GOAL,        /* assets at 7/8 of the goal */
    START_LAST_TO_FIRST,    /* last at the start of the round, first now */
    START_FIRST_TO_LAST,    /* first at the start of the round, last now */
    START_CANT_AFFORD,      /* ahead: a rival store whose last goods slot costs more than its money */
    START_RIVAL_CHAIN_4,    /* ahead: a rival store in a chain of 4 */
    START_RIVAL_CLOSED,     /* ahead: a rival store closed for now, without goods */
    START_OWN_EMPTY,        /* ahead: its own store without goods */
    START_BANK_NEXT,        /* the bank on the next square */
    START_RANK_UP,          /* ranked better than at the start of the round */
    START_RANK_DOWN,        /* ranked worse */
    START_RIVAL_CHAIN,      /* ahead: a rival store in a chain */
    START_VACANT_AHEAD,     /* ahead: a vacant lot */
    START_EXPENSIVE_AHEAD,  /* ahead: a rival store whose last goods slot costs over 1/4 of its money */
    START_BANK_AHEAD,       /* ahead: the bank */
    START_NO_OWN_AHEAD,     /* none of its stores ahead */
    START_OWN_ROOM,         /* ahead: its own store with free goods slots */
    START_NO_DIRECTION,     /* no direction (Player.dirX 0, as after the bank) */
    START_ON_ELEVATOR,      /* ... and on an elevator square */
    START_NONE,
    START_COUNT = START_NONE
  };

static const u8 k_startChance[START_COUNT] = {
    100, 60, 80, 80, 100, 80, 50, 80, 100, 100, 100, 70, 70, 80, 40, 60, 70, 100, 100
  };

/* Entry of the character's talk script run at the start of the turn (START_*): the first of the
 * situations that holds and passes its chance; "ahead" is any square of the routes of 8
 * (0x8008BCA8) */
/* 0x8008BCA8 */
u32 TurnStart_PickScript(u16 player)
{
    Player *pl = &g_players[player];
    u8 holds[20];
    int ownAhead;
    int route;
    int i;
    u16 cell;
    Store *st;

    memset(holds,0,sizeof(holds));
    holds[START_LAST_TO_FIRST] = Player_RankedAlone(player,1,1) && Player_RankedAlone(player,0,0);
    holds[START_FIRST_TO_LAST] = Player_RankedAlone(player,1,0) && Player_RankedAlone(player,0,1);
    ownAhead = 0;
    Player_BuildRoutes(player,8);
    for (route = 0; route < g_routes.count; route++)
    {
        for (i = 0; i < 8; i++)
        {
            cell = Board_GetCell(g_routes.step[route][i].x,g_routes.step[route][i].floor);
            if (CELL_TYPE(cell) == CELL_STORE)
            {
                st = &g_stores[cell];
                if (st->owner == -1)
                {
                    holds[START_VACANT_AHEAD] = 1;
                }
                else if (st->owner == player)
                {
                    ownAhead = 1;
                    if (st->numGoods == 0)
                    {
                        holds[START_OWN_EMPTY] = 1;
                    }
                    if (st->numGoods < st->maxGoods)
                    {
                        holds[START_OWN_ROOM] = 1;
                    }
                }
                else if (st->tempClosed == 0 || st->numGoods != 0)
                {
                    /* (a store without goods: goods[-1], the bytes before goods[0]) */
                    if (pl->money < st->goods[st->numGoods - 1].price)
                    {
                        holds[START_CANT_AFFORD] = 1;
                    }
                    if (pl->money / 4 < st->goods[st->numGoods - 1].price)
                    {
                        holds[START_EXPENSIVE_AHEAD] = 1;
                    }
                    if (st->chainLen == 3)
                    {
                        holds[START_RIVAL_CHAIN_4] = 1;
                    }
                    if (st->chainLen > 0)
                    {
                        holds[START_RIVAL_CHAIN] = 1;
                    }
                }
                else
                {
                    holds[START_RIVAL_CLOSED] = 1;
                }
            }
            else if (CELL_TYPE(cell) == CELL_BANK)
            {
                if (i == 0)
                {
                    holds[START_BANK_NEXT] = 1;
                }
                holds[START_BANK_AHEAD] = 1;
            }
        }
    }
    holds[START_FIRST_MONTH] = (g_gameConfig.monthsElapsed == 0);
    holds[START_NEAR_GOAL] = (g_gameConfig.goalMoney * 7 / 8 <= pl->assets);
    holds[START_RANK_UP] = (pl->rank < (char)pl->prevRank);
    holds[START_RANK_DOWN] = ((char)pl->prevRank < pl->rank);
    holds[START_NO_OWN_AHEAD] = !ownAhead;
    if (pl->dirX == 0)
    {
        holds[START_NO_DIRECTION] = 1;
        cell = Board_GetCell(g_players[g_curPlayer].x,g_players[g_curPlayer].floor);
        holds[START_ON_ELEVATOR] = (CELL_TYPE(cell) == CELL_ELEVATOR);
    }
    for (i = 0; i < START_COUNT; i++)
    {
        if (holds[i] && (u8)(rand() % 100) < k_startChance[i])
        {
            return i;
        }
    }
    return START_NONE;
}

/* Entry of the talk script for the character's comment at the end of the turn: the first of the
 * turn's events (g_turnFlags) in the order below, TURN_EVENT when none; 0 (no comment) after a
 * quiet event card, and 80% of the time when far behind and short of money (0x8008C95C) */
/* 0x8008C95C */
u32 TurnEnd_PickScript(u32 player)
{
    static const unsigned char order[] = {
        TURN_HALF_GOAL, TURN_CHAIN_4, TURN_CHAIN_2_3, TURN_STORE_CLOSED, TURN_STORE_SALE,
        TURN_OWN_STORE, TURN_NO_ELEVATOR, TURN_BANK, TURN_ELEVATOR, TURN_LOT_UNAFFORDABLE,
        TURN_LOT_DECLINED, TURN_SOLD_STORE, TURN_CASH_BACK, TURN_BIG_PURCHASE, TURN_BOUGHT_STORE,
        TURN_SHOPPED_AGAIN, TURN_SHOPPED
      };
    Player *pl = &g_players[player & 0xffff];
    int best;
    int i;

    best = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (best < g_players[i].assets)
        {
            best = g_players[i].assets;
        }
    }
    if (pl->passedHalfGoal == 0 &&
        pl->assetHistory[(char)pl->historyLen - 1] < g_gameConfig.goalMoney / 2 &&
        g_gameConfig.goalMoney / 2 <= pl->assets)
    {
        pl->passedHalfGoal = 1;
        g_turnFlags[TURN_HALF_GOAL] = 1;
    }
    if (pl->assets < best - 5000 && pl->money < 1000 && (u8)(rand() % 100) < 80)
    {
        return 0;
    }
    if ((pl->shopHistory & 3) == 3)
    {
        g_turnFlags[TURN_SHOPPED_AGAIN] = 1;
    }
    if (g_turnFlags[TURN_QUIET_EVENT] != 0)
    {
        return 0;
    }
    for (i = 0; i < (int)sizeof(order); i++)
    {
        if (g_turnFlags[order[i]] != 0)
        {
            return order[i];
        }
    }
    return TURN_EVENT;
}
