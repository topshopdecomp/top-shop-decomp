/*
 * bank.h - the bank: payday
 *
 * The declarations of src/bank.c: the work areas of its tasks and its functions.
 * See docs/board.md.
 */

#ifndef BANK_H
#define BANK_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Payday_Task */
typedef struct PaydayWork
{
    int win;   /* 0x00 */
    int bonus;   /* 0x04 */
    unsigned char busy;   /* 0x08 */
    unsigned char pad09[3];
    int amount;   /* 0x0C */
    int timer;   /* 0x10 */
    unsigned char pad14[80];
} PaydayWork;

/* ---- Functions ---- */

/* src/bank.c */
u32 Payday_Task(u16 *step, PaydayWork *work);             /* 0x800579BC */
int Player_PaydayAmount(short player, char bonus);        /* 0x80076270 */
int Player_PaydayAfter(short player, short steps, char bonus); /* 0x8007640C */

#endif /* BANK_H */
