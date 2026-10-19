/*
 * elevator.h - riding the elevator
 *
 * The declarations of src/elevator.c: the work areas of its tasks and its functions.
 * See docs/board.md.
 */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Elevator_Task */
typedef struct ElevatorWork
{
    unsigned char floor;   /* 0x00 */
    unsigned char elevator;   /* 0x01 */
    unsigned char slot;   /* 0x02 */
    unsigned char pad03[1];
    short slide;   /* 0x04 */
    unsigned char pad06[2];
    unsigned int dy;   /* 0x08 */
    int delay;   /* 0x0C */
    unsigned char pad10[84];
} ElevatorWork;

/* ---- Functions ---- */

/* src/elevator.c */
u32 Elevator_Task(short *step, ElevatorWork *work);  /* 0x800552A4 */

#endif /* ELEVATOR_H */
