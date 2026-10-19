/*
 * player.c - the players
 *
 * Money (paying, earning, paying another player), assets (money and the value of the stores),
 * ranks and the asset history behind the graph, the check for the end of the game, backing a
 * player up and restoring it, and resolving player selectors (everybody, the current player, the
 * others).
 */
#include "common.h"
#include "topshop.h"

/* Total assets: the money, and for each of the player's stores its type's value and the cost of
 * its goods (0x80076490) */
/* 0x80076490 */
int Player_CalcAssets(short player)
{
    Store *st;
    int assets;
    int i;
    int j;

    assets = g_players[player].money;
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        if (st->owner != player)
        {
            continue;
        }
        assets += st->type->value;
        for (j = 0; j < st->numGoods; j++)
        {
            assets += st->type->items[st->goods[j].level].cost;
        }
    }
    return assets;
}

/* Recompute all assets; 1 if a player is bankrupt (assets < 0) or has reached the goal (0x80076B38) */
/* 0x80076B38 */
u32 Players_CheckGameEnd(void)
{
    int i;

    for (i = 0; i < g_numPlayers; i++)
    {
        g_players[i].assets = Player_CalcAssets(i);
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].assets < 0)
        {
            return 1;
        }
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_gameConfig.goalMoney <= g_players[i].assets)
        {
            return 1;
        }
    }
    return 0;
}

/* Sort the player indices in order by key, highest first (equal: more money first); the players
 * who are out go last, by rankKey */
static void Players_SortBy(char *order, const int *key)
{
    char a;
    char b;
    int i;
    int j;

    for (i = 0; i < g_numPlayers - 1; i++)
    {
        for (j = i + 1; j < g_numPlayers; j++)
        {
            a = order[i];
            b = order[j];
            if (g_players[a].out == 0)
            {
                if (g_players[b].out != 0 ||
                    (key[a] >= key[b] &&
                     (key[a] != key[b] || g_players[a].money >= g_players[b].money)))
                {
                    continue;
                }
            }
            else if (g_players[b].out != 0 &&
                     (u16)g_players[a].rankKey > (u16)g_players[b].rankKey)
            {
                continue;
            }
            order[i] = b;
            order[j] = a;
        }
    }
}

/* Rank the players by assets (Player.rank, rankOrder; ties share a rank) and by value: the goods
 * in the first six slots of their stores, plus a fifth, their money and 50 per point
 * (Player.valueRank); the players who are out last (0x80076E7C) */
/* 0x80076E7C */
void Players_Rank(void)
{
    char order[6];
    int assets[6];
    int value[6];
    Store *st;
    char player;
    int i;
    int k;

    for (i = 0; i < g_numPlayers; i++)
    {
        Player_CalcAssets(i);                   /* (the result is not used) */
        order[i] = i;
        assets[i] = g_players[i].assets;
    }
    Players_SortBy(order,assets);
    for (i = 0; i < g_numPlayers; i++)
    {
        g_players[order[i]].rankOrder = i;
    }
    g_players[order[0]].rank = 0;
    for (i = 1; i < g_numPlayers; i++)
    {
        player = order[i];
        if (g_players[player].out == 0 && assets[order[i - 1]] == g_players[player].assets)
        {
            g_players[player].rank = g_players[order[i - 1]].rank;
        }
        else
        {
            g_players[player].rank = i;
        }
    }

    for (i = 5; i >= 0; i--)
    {
        value[i] = 0;
    }
    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
    {
        st = &g_stores[i];
        if (st->owner == -1)
        {
            continue;
        }
        for (k = 0; k < 6; k++)
        {
            if (st->goods[k].active != 0)
            {
                value[st->owner] += g_storeTypes[(char)st->storeType].items[st->goods[k].level].cost;
            }
        }
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        value[i] = value[i] * 6 / 5;
        value[i] += g_players[i].money + g_players[i].points * 50;
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        order[i] = i;
    }
    Players_SortBy(order,value);
    g_players[order[0]].valueRank = 0;
    for (i = 1; i < g_numPlayers; i++)
    {
        player = order[i];
        if (g_players[player].out == 0 && value[order[i - 1]] == value[player])
        {
            g_players[player].valueRank = g_players[order[i - 1]].valueRank;
        }
        else
        {
            g_players[player].valueRank = i;
        }
    }
    for (i = 0; i < 6; i++)
    {
        g_cpuDebugVals[i] = value[i];
    }
    for (i = 0; i < 6; i++)
    {
        g_cpuDebugVals[6 + i] = (char)g_players[i].valueRank + 1;
    }
}

/* Append the player's assets to their 13-entry asset history (the oldest drops out) (0x80077690) */
/* 0x80077690 */
void Player_RecordAssets(short player)
{
    Player *pl = &g_players[player];
    int i;

    if ((char)pl->historyLen < 13)
    {
        pl->assetHistory[(char)pl->historyLen] = pl->assets;
        pl->historyLen++;
        return;
    }
    for (i = 0; i < 12; i++)
    {
        pl->assetHistory[i] = pl->assetHistory[i + 1];
    }
    pl->assetHistory[12] = pl->assets;
}

/* Save a player's position, steps and money in backup slot slot (0x80114980, 0x18 bytes)
 * (0x80077B10) */
/* 0x80077B10 */
void Player_Backup(short player, short slot)
{
    g_playerBackups[slot].x = g_players[player].x;
    g_playerBackups[slot].floor = g_players[player].floor;
    g_playerBackups[slot].dirX = g_players[player].dirX;
    g_playerBackups[slot].dirFloor = g_players[player].dirFloor;
    g_playerBackups[slot].stepsLeft = g_players[player].stepsLeft;
    g_playerBackups[slot].stepsSincePayday = (u16)g_players[player].stepsSincePayday;
    g_playerBackups[slot].totalSteps = g_players[player].totalSteps;
    g_playerBackups[slot].paydays = (u16)g_players[player].paydays;
    g_playerBackups[slot].money = g_players[player].money;
}

/* Restore what Player_Backup saved and put the piece back on its square (0x80077BB8) */
/* 0x80077BB8 */
void Player_Restore(short player, short slot)
{
    Player *pl = &g_players[player];
    PlayerBackup *backup = &g_playerBackups[slot];

    pl->x = backup->x;
    pl->floor = backup->floor;
    pl->dirX = backup->dirX;
    pl->dirFloor = backup->dirFloor;
    pl->stepsLeft = backup->stepsLeft;
    pl->stepsSincePayday = backup->stepsSincePayday;
    pl->totalSteps = backup->totalSteps;
    pl->paydays = backup->paydays;
    pl->money = backup->money;
    pl->moneyShown = backup->money;
    pl->screenX = PIECE_SCREEN_X(pl->x);
    pl->screenY = PIECE_SCREEN_Y(pl->floor);
}

/* Player index for a selector: WHO_CURRENT the current player, WHO_OTHERS a random other active
 * player, else who. WHO_ANY was meant to be a random active player: the random number is not
 * stepped, so it is the first active player one time in g_numActivePlayers, else player 0
 * (0x8007AF1C) */
/* 0x8007AF1C */
u32 Player_Resolve(short who)
{
    int r;
    short n;
    int i;

    switch (who)
    {
    case WHO_CURRENT:
        return g_curPlayer;
    case WHO_OTHERS:
        r = rand();
        n = 0;
        for (i = 0; i < g_numPlayers; i++)
        {
            if (i != g_curPlayer && g_players[i].out == 0 && n++ == r % (g_numActivePlayers - 1))
            {
                return i;
            }
        }
        return 0;
    case WHO_ANY:
        r = rand();
        for (i = 0; i < g_numPlayers; i++)
        {
            if (g_players[i].out == 0 && r % g_numActivePlayers == 0)
            {
                return i;
            }
        }
        return 0;
    default:
        return who;
    }
}

/* Highest assets in any player's asset history (Player_RecordAssets) (0x8007B440) */
/* 0x8007B440 */
int Players_GetMaxAssetsEver(void)
{
    int most;
    int month;
    int i;

    most = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        for (month = 0; month < (char)g_players[i].historyLen; month++)
        {
            if (most < g_players[i].assetHistory[month])
            {
                most = g_players[i].assetHistory[month];
            }
        }
    }
    return most;
}

/* Set a player's money and recompute the assets (0x8007B4D0) */
/* 0x8007B4D0 */
void Player_SetMoney(short player, int money)
{
    int assets;

    g_players[player].money = money;
    assets = Player_CalcAssets(player);
    g_players[player].assets = assets;
}

/* Take amount from a player's money and recompute the assets (0x8007B518) */
/* 0x8007B518 */
void Player_Pay(short player, int amount)
{
    int assets;

    g_players[player].money -= amount;
    assets = Player_CalcAssets(player);
    g_players[player].assets = assets;
}

/* Give amount to a player and recompute the assets (0x8007B56C) */
/* 0x8007B56C */
void Player_Earn(short player, int amount)
{
    int assets;

    g_players[player].money += amount;
    assets = Player_CalcAssets(player);
    g_players[player].assets = assets;
}

/* Move amount of money from one player to another and recompute both assets (0x8007B5C0) */
/* 0x8007B5C0 */
void Player_PayTo(short from, short to, int amount)
{
    int assets;

    g_players[from].money -= amount;
    g_players[to].money += amount;
    assets = Player_CalcAssets(from);
    g_players[from].assets = assets;
    assets = Player_CalcAssets(to);
    g_players[to].assets = assets;
}
