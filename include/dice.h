/*
 * dice.h - the dice roll
 *
 * The dice roll (the rolling pencil).
 * The declarations of src/dice.c: its constants, the work areas of its tasks, its globals and its
 * functions.
 * See docs/board.md.
 */

#ifndef DICE_H
#define DICE_H

#include "common.h"

/* Player.diceEffect: what the pencil (Dice_Task) can show */
#define DICE_NORMAL   0
#define DICE_1_2      1   /* EV_SLOW_DICE: 1 and 2 only */
#define DICE_E        2   /* EV_DICE_E: E only (an event) */
#define DICE_SLOW     3   /* EV_LOWER_PENCIL: rolls slowly from a fixed start, can be aimed */

#define DICE_RESULT_CANCELED  (-1)             /* circle: back to the command menu */
#define DICE_RESULT_EVENT     8                /* the E face: an event */

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Dice_Task */
typedef struct DiceWork
{
    int cancelable;   /* 0x00 1: a roll from the command menu, circle goes back to it (g_diceResult -1); 0: a roll an event script asks for */
    int mode;   /* 0x04 DICE_* (Player.diceEffect) */
    unsigned char started;   /* 0x08 */
    signed char spinning;   /* 0x09 */
    unsigned char shown;   /* 0x0A set, never read */
    unsigned char pad0B[1];
    int reel;   /* 0x0C 20.12: the face showing, 3 cells a face */
    unsigned int prevReel;   /* 0x10 set, never read */
    int reelSpeed;   /* 0x14 */
    int phase;   /* 0x18 cells turned, for the tick sound */
    int posX;   /* 0x1C 20.12 screen position of the pencil */
    int posY;   /* 0x20 */
    int bounces;   /* 0x24 frames to spin in place */
    int timer;   /* 0x28 */
    unsigned char pad2C[56];
} DiceWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char g_diceResult;  /* 0x800B2672 the face rolled, 0-5 (1-6 squares) */

/* ---- Functions ---- */

/* src/dice.c */
int Dice_Task(short *step, DiceWork *work);  /* 0x800545E4 */

#endif /* DICE_H */
