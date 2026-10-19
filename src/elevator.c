/*
 * elevator.c - riding the elevator
 *
 * Elevator_Task: the doors open, the piece steps in, the doors close, the car goes to a random
 * one of the floors the elevator serves, and the piece steps out there.
 */
#include "common.h"
#include "topshop.h"

/* Screen y of a piece inside the elevator of floor */
#define PIECE_IN_ELEVATOR_Y(floor)  (PIECE_SCREEN_Y(floor) - 0x10)

/* Elevator_Task steps */
enum ElevatorStep
{
    ELEV_OPEN             =   0,  /* the doors open (Board_DrawStoresTask) */
    ELEV_WAIT_OPEN        =   1,
    ELEV_STEP_IN          =  10,
    ELEV_STEPPING_IN      =  11,
    ELEV_CLOSE            =  20,  /* the doors close */
    ELEV_WAIT_CLOSE       =  21,
    ELEV_HUM              =  30,  /* the looped hum; the piece hidden */
    ELEV_HUM_WAIT         =  31,  /* 15 frames */
    ELEV_PICK_FLOOR       =  32,  /* a random one of the elevator's floors */
    ELEV_RIDE             =  33,
    ELEV_ARRIVED          =  34,
    ELEV_ARRIVED_WAIT     =  35,  /* 15 frames, the hum stops */
    ELEV_OPEN_AGAIN       =  40,  /* the doors open there */
    ELEV_WAIT_OPEN_AGAIN  =  41,
    ELEV_STEP_OUT         =  50,
    ELEV_STEPPING_OUT     =  51,
    ELEV_CLOSE_AGAIN      =  60,  /* (the doors close, as ELEV_CLOSE) */
    ELEV_WAIT_CLOSE_AGAIN =  61,
    ELEV_FACE             =  70,  /* down from the top floor, up from the bottom one */
    ELEV_DONE             = 100 
};

/* Task: ride the elevator: the doors open (Board_DrawStoresTask), the piece steps in, the doors
 * close, the car goes to a random one of the elevator's floors (StageDef.elevators: count, then
 * the floors) to the hum of a looped sound, the doors open, the piece steps out and they close;
 * the piece then faces up or down the board (0x800552A4) */
/* 0x800552A4 */
u32 Elevator_Task(short *step, ElevatorWork *work)
{
    Player *cur = &g_players[g_curPlayer];
    int r;

    switch(*step)
    {
    case ELEV_OPEN:
        cur->inElevator = 1;
        cur->pieceAnim = 6;
        cur->pieceFlip = 0;
        Task_SendMessage(g_storesTaskId,ELEVATOR_DOORS_OPEN);
        *step += 1;
        break;
    case ELEV_WAIT_OPEN:
        work->slide += 4;
        if (work->slide >= 0x20)
        {
            work->slide = 0x20;
            *step = ELEV_STEP_IN;
        }
        if (Task_ReceiveMessageIf(g_storesTaskId,ELEVATOR_DOORS_DONE) != 0)
        {
            return 0;
        }
        *step = ELEV_STEP_IN;
        break;
    case ELEV_STEP_IN:
        *step += 1;
        break;
    case ELEV_STEPPING_IN:
        cur->screenY -= 2;
        if (cur->screenY <= PIECE_IN_ELEVATOR_Y(cur->floor))
        {
            cur->screenY = PIECE_IN_ELEVATOR_Y(cur->floor);
            *step = ELEV_CLOSE;
            cur->pieceState = 0xde;
        }
        break;
    case ELEV_CLOSE:
    case ELEV_CLOSE_AGAIN:        /* the doors close */
        Se_Play(0x22,0x7f,0x7f,1);
        Task_SendMessage(g_storesTaskId,ELEVATOR_DOORS_CLOSE);
        *step += 1;
        break;
    case ELEV_WAIT_CLOSE:
        if (Task_ReceiveMessageIf(g_storesTaskId,ELEVATOR_DOORS_DONE) != 0)
        {
            return 0;
        }
        *step = ELEV_HUM;
        break;
    case ELEV_HUM:
        work->slot = Se_Play(0x23,0x7f,0x7f,1);
        cur->pieceShown = 0;
        work->delay = 15;
        *step += 1;
        break;
    case ELEV_HUM_WAIT:
        if (--work->delay != 0)
        {
            return 0;
        }
        *step += 1;
        break;
    case ELEV_PICK_FLOOR:         /* where to */
        work->elevator = Board_GetCell(cur->x,cur->floor) & 0xf;
        r = rand();
        work->floor = g_stageDefs[g_boardStage].elevators[(char)work->elevator]
                      [r % g_stageDefs[g_boardStage].elevators[(char)work->elevator][0] + 1];
        if ((char)work->floor == cur->floor)
        {
            *step = ELEV_ARRIVED;
            return 0;
        }
        work->dy = ((char)work->floor > cur->floor) ? 2 : -2;
        *step += 1;
        break;
    case ELEV_RIDE:               /* the ride */
        cur->screenY += work->dy;
        Board_FocusCurrentPlayer();
        if (cur->screenY != PIECE_IN_ELEVATOR_Y((char)work->floor))
        {
            return 0;
        }
        *step += 1;
        break;
    case ELEV_ARRIVED:
        work->delay = 15;
        *step += 1;
        break;
    case ELEV_ARRIVED_WAIT:
        if (--work->delay == 0)
        {
            Se_StopSlot((char)work->slot);
            *step = ELEV_OPEN_AGAIN;
        }
        break;
    case ELEV_OPEN_AGAIN:         /* arrived: the doors open */
        Se_Play(0x26,0x7f,0x7f,1);
        cur->pieceShown = 1;
        cur->pieceAnim = 5;
        cur->pieceFlip = 0;
        cur->floor = work->floor;
        Task_SendMessage(g_storesTaskId,ELEVATOR_DOORS_OPEN);
        *step += 1;
        break;
    case ELEV_WAIT_OPEN_AGAIN:
        if (Task_ReceiveMessageIf(g_storesTaskId,ELEVATOR_DOORS_DONE) != 0)
        {
            return 0;
        }
        *step = ELEV_STEP_OUT;
        break;
    case ELEV_STEP_OUT:
        cur->pieceState = 0xd2;
        *step += 1;
        break;
    case ELEV_STEPPING_OUT:
        cur->screenY += 2;
        if (cur->screenY >= PIECE_SCREEN_Y(cur->floor))
        {
            cur->screenY = PIECE_SCREEN_Y(cur->floor);
            *step = ELEV_CLOSE_AGAIN;
        }
        break;
    case ELEV_WAIT_CLOSE_AGAIN:
        if (Task_ReceiveMessageIf(g_storesTaskId,ELEVATOR_DOORS_DONE) != 0)
        {
            return 0;
        }
        *step = ELEV_FACE;
        break;
    case ELEV_FACE:               /* down from the top floor, up from the bottom one */
        cur->dirX = 0;
        if (cur->floor == 0)
        {
            cur->dirFloor = 1;
        }
        else if (cur->floor == g_stageDefs[g_boardStage].floors - 1)
        {
            cur->dirFloor = -1;
        }
        else
        {
            cur->dirFloor = 0;
        }
        *step = ELEV_DONE;
        break;
    case ELEV_DONE:
        cur->inElevator = 0;
        return 1;
    }
    return 0;
}
