/*
 * move.h - moving a piece
 *
 * The human player's choice of way, walking, stairs, "Stop in this square?".
 * The declarations of src/move.c: the work areas of its tasks and its functions.
 * See docs/board.md.
 */

#ifndef MOVE_H
#define MOVE_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Move_Task */
typedef struct MoveWork
{
    unsigned char busy;   /* 0x00 */
    unsigned char stepBack;   /* 0x01 a step back (circle held): the piece walks backwards */
    unsigned char dash;   /* 0x02 R1 held (a CPU player: fast mode or 4 steps or more): 16 pixels a frame */
    signed char route;   /* 0x03 g_routes route a CPU player takes (Cpu_ChooseRoute) */
    signed char fromX;   /* 0x04 square and direction before the step */
    signed char fromFloor;   /* 0x05 */
    unsigned char dirX;   /* 0x06 Player.dirX, 0xFF = -1 */
    unsigned char dirFloor;   /* 0x07 */
    unsigned char waitMode;   /* 0x08 */
    unsigned char savedIsCpu;   /* 0x09 */
    signed char seSlot;   /* 0x0A */
    unsigned char pad0B[1];
    short targetX;   /* 0x0C screen position the piece walks to */
    short targetY;   /* 0x0E */
    short dx;   /* 0x10 step per frame (low byte set alone at times) */
    short dy;   /* 0x12 */
    short baseY;   /* 0x14 */
    short timer;   /* 0x16 */
    int frame;   /* 0x18 */
    int infoDelay;   /* 0x1C frames before the square info shows, -1 = shown */
    unsigned char done;   /* 0x20 */
    unsigned char pad21[67];
} MoveWork;

/* ---- Functions ---- */

/* src/move.c */
u32 Move_GetInputDir(void);                             /* 0x8004F1FC */
u32 Move_Start(short *step, MoveWork *work);            /* 0x8004F7E4 */
u32 Move_Walk(short *step, MoveWork *work);             /* 0x800504F0 */
u32 Move_EnterConnector(u16 *step, MoveWork *work);     /* 0x80050F98 */
u32 Move_ConnectorRightA(short *step, MoveWork *work);  /* 0x80051498 */
u32 Move_ConnectorLeftA(short *step, MoveWork *work);   /* 0x80051FF0 */
u32 Move_ConnectorLeftB(short *step, MoveWork *work);   /* 0x80052AF0 */
u32 Move_ConnectorRightB(short *step, MoveWork *work);  /* 0x8005361C */
u32 Move_AskStop(u16 *step, MoveWork *work);            /* 0x800540D8 */
int Move_Task(short *step, MoveWork *work);             /* 0x80054410 */

#endif /* MOVE_H */
