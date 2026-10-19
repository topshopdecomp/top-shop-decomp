/*
 * boardmap.h - the board's map
 *
 * Cells, squares, the ways on from a square, steps, the routes ahead and the bank on them.
 * The declarations of src/boardmap.c: its types and constants, its globals and its functions.
 * See docs/board.md.
 */

#ifndef BOARDMAP_H
#define BOARDMAP_H

#include "common.h"

#define GAME_NUM_STAGES  10

/* Board cell value from Board_GetCell: high byte = type, low byte = store index. */
#define CELL_TYPE(c)     ((c) & 0xFF00)
#define CELL_STORE       0x0000
#define CELL_NONE        0x0100   /* also returned for out-of-range coordinates */
#define CELL_ELEVATOR    0x0200
#define CELL_BANK        0x0400   /* the bank ("A1 Bank", 0x80013140); clears Player.dirX / dirFloor */
#define CELL_UNUSED5     0x0500   /* on no board; Board_PhaseSquare would end the turn on it */
#define CELL_CONNECTOR   0x0300   /* stairs between floors: bits 4-7 its shape (0x10, 0x20, 0x30), bits 0-3
                                     the height of the piece on it at its foot (0, 8, 16 pixels) */
/* A connector's shape: cell & 0xFFF0 (Board_DrawConnector, the minimap, Move_Connector) */
#define CONNECTOR_STAIRS          (CELL_CONNECTOR | 0x10)   /* the stairs sprite as it is */
#define CONNECTOR_STAIRS_FLIPPED  (CELL_CONNECTOR | 0x20)   /* mirrored */
#define CONNECTOR_STAIRS_CROSSED  (CELL_CONNECTOR | 0x30)   /* both, crossing */
#define CELL_LANDING     0x0600   /* a landing beside the middle stairs (boards 1, 4, 5): walked over
                                     without counting a step (Board_StepAlong); minimap sprite 0xE0 */

/* Board_GetExits: the ways a piece can go on from a square */
#define EXIT_LEFT              0x01
#define EXIT_RIGHT             0x02
#define EXIT_LEFT_FLOOR_MINUS  0x04   /* a connector on the left to floor - 1 (drawn above) */
#define EXIT_LEFT_FLOOR_PLUS   0x08   /* ... to floor + 1 */
#define EXIT_RIGHT_FLOOR_MINUS 0x10
#define EXIT_RIGHT_FLOOR_PLUS  0x20

/* Per-stage board definition (g_stageDefs, 0x800ACD14). */
typedef struct StageDef
{
    signed char numStores;       /* 0x00 entries used in g_stores */
    signed char payScale;        /* 0x01 payday scale (Player_PaydayAmount): base pay x payScale / board 0's,
                                    squares walked / payScale; on boards 1, 6, 7, 9 the number of store,
                                    elevator and bank squares */
    signed char originX;         /* 0x02 board position: square x is at (originX + x * 5) * 16 */
    signed char originY;         /* 0x03 floor f is at (originY + f * 5) * -16 (floor labels, camera) */
    signed char width;           /* 0x04 squares per floor */
    signed char floors;          /* 0x05 */
    signed char startX, startFloor; /* 0x06 where the pieces start */
    short *cells;                /* 0x08 [floor * width + x] -> cell value (Board_GetCell) */
    short *squareIds;            /* 0x0C [floor * width + x] -> square id (Board_GetSquareId) */
    unsigned char *squareFlags;  /* 0x10 [floor * width + x] (Board_GetSquareFlag) */
    signed char elevators[2][14]; /* 0x14 per elevator: number of stops, then the floors it stops at */
} StageDef;                      /* size 0x30 */

/* The routes a player can take (g_routes, 0x80114168), built by Player_BuildRoutes: every
 * branch of the board from the player's square, up to 4 routes of maxSteps squares. */
typedef struct RouteStep
{
    signed char x, floor;        /* 0x00 square reached */
    signed char dx, dfloor;      /* 0x02 direction of the move */
} RouteStep;

typedef struct Routes
{
    short maxSteps;              /* 0x000 */
    short count;                 /* 0x002 */
    RouteStep step[4][128];      /* 0x004 */
} Routes;                        /* size 0x804 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern StageDef g_stageDefs[10];    /* 0x800ACD14 */
extern unsigned char g_boardStage;  /* 0x800B2671 */
extern short g_bankRoute;           /* 0x800B2678 */
extern short g_bankDistance;        /* 0x800B2698 */
extern short g_routeBankDist[4];    /* 0x800B2700 */
extern u16 g_lastCell;              /* 0x800B2720 */
extern u8 g_routeVisited[128];      /* 0x801129C0 */
extern Routes g_routes;             /* 0x80114168 */

/* ---- Functions ---- */

/* src/boardmap.c */
short Board_GetCell(short x, short floor);                /* 0x80077CC8 */
void Player_FindNearestBank(char player);                 /* 0x80077D80 */
void Player_BuildRoutes(short player, short maxSteps);    /* 0x80077EE0 */
u32 Board_GetExits(char x, char floor, char dir, char vdir); /* 0x80078374 */
void Board_Step(char *x, char *floor, char *dir, char *vdir); /* 0x80078778 */
short Board_GetSquareId(short x, short floor);            /* 0x8007B6B4 */
int Board_GetSquareFlag(short x, short floor);            /* 0x8007B744 */

#endif /* BOARDMAP_H */
