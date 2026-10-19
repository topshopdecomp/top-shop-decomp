/*
 * dice.c - the dice roll
 *
 * The game's die is a rolling pencil: Dice_Task turns a reel of faces until cross is pressed (a
 * CPU player waits half a second), slows it down and gives the result, including the variants
 * event cards cause (only 1 and 2, always the event face E, a slow roll).
 */
#include "common.h"
#include "topshop.h"

/* The pencil's reel position in cells (20.12) */
#define REEL_CELL(reel)  ((short)((reel) / 0x1000))

/* While the pencil spins, its reel (length cells of 0x1000) turns back by reelSpeed; a tick
 * sound every 6 cells turned */
static void Dice_Spin(DiceWork *work, int length)
{
    if (work->spinning == 0)
    {
        return;
    }
    work->phase += work->reelSpeed;
    work->prevReel = work->reel;
    work->reel = (work->reel - work->reelSpeed + length) % length;
    if (work->phase >= 0x6000)
    {
        work->phase %= 0x6000;
        Se_Play(0x1b,0x7f,0x7f,1);
    }
}

/* Dice_Task steps */
enum DiceStep
{
    DICE_STEP_START      =   0,
    DICE_STEP_WAIT_HUMAN =  10,  /* cross rolls; circle cancels (when cancelable) */
    DICE_STEP_CPU        =  20,  /* a CPU player rolls after half a second */
    DICE_STEP_CPU_WAIT   =  21,
    DICE_STEP_CPU_ROLL   =  22,
    DICE_STEP_BOUNCE     =  30,  /* 0-5 more frames in place */
    DICE_STEP_ROLL       =  31,  /* rolling away, slowing down, until it lies on a face */
    DICE_STEP_STOPPED    =  40,
    DICE_STEP_RESULT     =  41,  /* the result (g_diceResult) */
    DICE_STEP_DONE       = 100 
};

/* The dice roll: a rolling pencil (a reel of faces, 3 cells a face; mode = Player.diceEffect).
 * Cross rolls it (a CPU player: after half a second; circle: no roll if cancelable); it spins a
 * few frames, then rolls away to the lower left slowing down, stops on a face, and after half a
 * second or a button gives g_diceResult: -1 not rolled, 8 the event face (E), else the number of
 * steps - 1. DICE_SLOW (and RULE_SLOW_DICE) starts it slowly from a fixed place, so that it can
 * be aimed (0x800545E4) */
/* 0x800545E4 */
int Dice_Task(short *step, DiceWork *work)
{
    PadState *pad = &g_playerPads[g_curPlayer];
    u8 effect;
    int face;
    u16 id;

    FntPrint("SLOT ST : %08X %08X\n", work->reelSpeed, work->reel);
    FntPrint("%8X\n", (REEL_CELL(work->reel) % 3));
    switch(*step)
    {
    case DICE_STEP_START:
        effect = g_players[g_curPlayer].diceEffect;
        work->started = 1;
        work->spinning = 1;
        work->posX = 192 << 12;
        work->posY = 64 << 12;
        work->phase = 0;
        work->shown = 0;
        work->mode = (char)effect;
        if (g_boardRules.enable[RULE_SLOW_DICE] == 0 && (char)effect != DICE_SLOW)
        {
            work->reelSpeed = 0x2000;
            work->reel = (rand() % 8) * 0x3000;
        }
        else
        {
            work->reelSpeed = 0x400;
            work->reel = 0x3000;
        }
        *step = (g_players[g_curPlayer].isCpu == 0) ? DICE_STEP_WAIT_HUMAN : DICE_STEP_CPU;
        break;
    case DICE_STEP_WAIT_HUMAN:    /* waiting for the human player */
        if (pad->pressed & PADRdown)
        {
            if (g_boardRules.enable[RULE_SLOW_DICE] == 0 && work->mode != DICE_SLOW)
            {
                work->bounces = rand() % 6;
                *step = DICE_STEP_BOUNCE;
            }
            else
            {
                work->reelSpeed = 0x332;
                *step = DICE_STEP_ROLL;
            }
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if ((pad->pressed & PADRright) && work->cancelable != 0)
        {
            g_diceResult = DICE_RESULT_CANCELED;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = DICE_STEP_DONE;
        }
        break;
    case DICE_STEP_CPU:           /* a CPU player rolls after half a second */
        work->timer = 30;
        *step += 1;
        break;
    case DICE_STEP_CPU_WAIT:
        if (--work->timer == 0)
        {
            *step += 1;
        }
        break;
    case DICE_STEP_CPU_ROLL:
        work->bounces = rand() % 6;
        Se_Play(SE_OK,0x7f,0x7f,1);
        *step = DICE_STEP_BOUNCE;
        break;
    case DICE_STEP_BOUNCE:        /* 0-5 more frames in place */
        if (work->bounces == 0)
        {
            *step += 1;
        }
        work->bounces--;
        work->reel = (work->reel - work->reelSpeed + 0x1b000) % 0x1b000;
        break;
    case DICE_STEP_ROLL:          /* rolling away, slowing down, until it lies on a face */
        work->shown = 1;
        work->reelSpeed -= 0x147;
        if (work->reelSpeed < 0x332)
        {
            work->reelSpeed = 0x332;
        }
        if (work->reelSpeed == 0x332 && REEL_CELL(work->reel) % 3 == 0)
        {
            work->spinning = 0;
            work->reelSpeed = 0;
            *step = DICE_STEP_STOPPED;
            break;
        }
        work->posX -= work->reelSpeed * 5;
        work->posY += work->reelSpeed * 3;
        break;
    case DICE_STEP_STOPPED:
        work->reelSpeed = 0;
        work->timer = 30;
        *step += 1;
        break;
    case DICE_STEP_RESULT:        /* the result */
        work->timer--;
        if ((g_players[g_curPlayer].isCpu == 0 && (pad->pressed & (PADRright | PADRdown)) != 0) || work->timer == 0)
        {
            face = REEL_CELL(work->reel) / 3;
            switch (work->mode)
            {
            case DICE_NORMAL:
            case DICE_SLOW:
                g_diceResult = face;
                break;
            case DICE_1_2:
                g_diceResult = face % 2;
                break;
            case DICE_E:
                g_diceResult = DICE_RESULT_EVENT;
                break;
            }
            *step = DICE_STEP_DONE;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        break;
    case DICE_STEP_DONE:
        return 1;
    }
    if (work->started == 0)
    {
        return 0;
    }
    /* the pencil's face: 9 (27 cells), 1 and 2 (6 cells) or E (3 cells) */
    switch (work->mode)
    {
    case DICE_NORMAL:
    case DICE_SLOW:
        Dice_Spin(work,0x1b000);
        id = GROUP_SPRITE(0,REEL_CELL(work->reel) + 4);
        break;
    case DICE_1_2:
        Dice_Spin(work,0x6000);
        id = GROUP_SPRITE(0,REEL_CELL(work->reel) % 6 + 0xed);
        break;
    case DICE_E:
        Dice_Spin(work,0x3000);
        id = GROUP_SPRITE(0,REEL_CELL(work->reel) % 3 + 0xea);
        break;
    default:
        return 0;
    }
    /* the pencil (3 frames of rolling), the face to its right */
    g_drawPos[0] = work->posX / 0x1000;
    g_drawPos[1] = work->posY / 0x1000;
    Sprite_Draw(GROUP_SPRITE(0,REEL_CELL(work->reel) % 3 + 1),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,1);
    g_drawPos[0] = work->posX / 0x1000 + 0x40;
    Sprite_Draw(id,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,1);
    return 0;
}
