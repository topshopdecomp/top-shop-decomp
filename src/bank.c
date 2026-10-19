/*
 * bank.c - the bank: payday
 *
 * A player is paid when its piece reaches a bank square on its move: Payday_Task shows "Payday!"
 * ("Bonus!!" when it stops there) with the amount and pays it. Player_PaydayAmount works the
 * amount out from the player's assets and the paydays it has had, scaled by the board;
 * Player_PaydayAfter is the same for a square further on, which the CPU players use to plan
 * their route.
 */
#include "common.h"
#include "topshop.h"

/* Payday_Task steps */
enum PaydayStep
{
    PAYDAY_OPEN        =   0,  /* pay, open the window */
    PAYDAY_WAIT_OPEN   =   1,
    PAYDAY_WAIT_BUTTON =  10,  /* a human player closes it with cross */
    PAYDAY_CPU         =  20,  /* a CPU player: 15 frames */
    PAYDAY_CPU_WAIT    =  21,
    PAYDAY_CLOSE       = 100,
    PAYDAY_WAIT_CLOSE  = 101 
};

/* "Payday!" / "Bonus!!" and the amount (Player_PaydayAmount), which the player earns; a human
 * player closes it with cross (0x800579BC) */
/* 0x800579BC */
u32 Payday_Task(u16 *step, PaydayWork *work)
{
    switch (*step)
    {
    case PAYDAY_OPEN:
        Se_Play((work->bonus != 0) ? 0x1e : 0x25,0x7f,0x7f,1);
        work->busy = 1;
        work->amount = Player_PaydayAmount(g_curPlayer,work->bonus);
        Player_Earn(g_curPlayer,work->amount);
        g_players[g_curPlayer].paydayEffect = 0;
        strcpy(g_msgBuf,(work->bonus == 0) ? "%CPayday!%K" : "%CBonus!!%K");
        Str_FormatInt(work->amount,0,g_msgBuf + strlen(g_msgBuf));
        work->win = TextWin_Create(TEXTWIN_AUTO,TEXTWIN_AUTO,-1,-1);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        *step += 1;
        break;
    case PAYDAY_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            TextWin_SetFocus(work->win,1);
            *step = (g_players[g_curPlayer].isCpu == 0) ? PAYDAY_WAIT_BUTTON : PAYDAY_CPU;
        }
        break;
    case PAYDAY_WAIT_BUTTON:
        if (g_playerPads[g_curPlayer].pressed & PADRdown)
        {
            *step = PAYDAY_CLOSE;
        }
        break;
    case PAYDAY_CPU:                        /* a CPU player: 15 frames */
        work->timer = 0xf;
        *step += 1;
        break;
    case PAYDAY_CPU_WAIT:
        if (--work->timer == 0)
        {
            *step = PAYDAY_CLOSE;
        }
        break;
    case PAYDAY_CLOSE:
        TextWin_Close(work->win);
        *step = PAYDAY_WAIT_CLOSE;
        break;
    case PAYDAY_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->busy = 0;
            return 1;
        }
        break;
    }
    return 0;
}

/* Money paid at the bank: (assets / 10 + (paydays so far + 1) * 100), scaled by the board
 * (StageDef.payScale against the first board's), times (squares walked since the last payday * 75
 * / payScale + 25) %; x1.5 when the move ends on the bank (bonus); Player.paydayEffect: 1 nothing,
 * 2 doubled (0x80076270) */
/* 0x80076270 */
int Player_PaydayAmount(short player, char bonus)
{
    Player *pl = &g_players[player];
    int payScale;
    int base;
    int walked;
    int amount;

    base = pl->assets / 10;
    if (base < 0)
    {
        base = 0;
    }
    payScale = g_stageDefs[g_boardStage].payScale;
    walked = (u16)pl->stepsSincePayday * 75;
    base = (base + ((u16)pl->paydays + 1) * 100) * payScale;
    amount = base / g_stageDefs[0].payScale * (walked / payScale + 25) / 100;
    if (bonus != 0)
    {
        amount = amount * 3 / 2;
    }
    if (pl->paydayEffect == 1)
    {
        amount = 0;
    }
    else if (pl->paydayEffect == 2)
    {
        amount *= 2;
    }
    return amount;
}

/* Player_PaydayAmount as if the player had walked steps more squares (0x8007640C) */
/* 0x8007640C */
int Player_PaydayAfter(short player, short steps, char bonus)
{
    int pay;

    g_players[player].paydays++;
    g_players[player].stepsSincePayday += steps;
    pay = Player_PaydayAmount(player,bonus);
    g_players[player].paydays--;
    g_players[player].stepsSincePayday -= steps;
    return pay;
}
