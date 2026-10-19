/*
 * boardmap.c - the board's map
 *
 * Reading a stage's board: the cell at a square (its type and, for a store, the store's index),
 * the square ids and flags, the ways on from a square, stepping along a floor and up or down the
 * stairs between floors; the routes ahead of a player (up to four) and the nearest bank on them,
 * which the moves, the CPU players and the talk scripts use.
 */
#include "common.h"
#include "topshop.h"

/* Cell value of the current stage's board at (x, floor) (CELL_* in the high byte; a store's
 * index in the low byte); CELL_NONE outside the board. Also kept in g_lastCell (0x80077CC8) */
/* 0x80077CC8 */
short Board_GetCell(short x, short floor)
{
    StageDef *stage = &g_stageDefs[g_boardStage];

    if (x < 0 || x > stage->width - 1 || floor < 0 || floor > stage->floors - 1)
    {
        return CELL_NONE;
    }
    g_lastCell = stage->cells[floor * stage->width + x];
    return stage->cells[floor * stage->width + x];
}

/* Build the player's routes and find the bank square nearest along them: g_bankDistance = steps
 * + 1, g_bankRoute = the route, g_routeBankDist[route] = steps + 1 on each route (a route without
 * the bank keeps the value it had, plus 1) (0x80077D80) */
/* 0x80077D80 */
void Player_FindNearestBank(char player)
{
    RouteStep *at;
    int route;
    int n;

    Player_BuildRoutes(player,0x80);
    g_bankRoute = 0;
    g_bankDistance = 0x80;
    for (route = 0; route < g_routes.count; route++)
    {
        for (n = 0; n < 0x80; n++)
        {
            at = &g_routes.step[route][n];
            if (Board_GetCell(at->x,at->floor) == CELL_BANK)
            {
                g_routeBankDist[route] = n;
                if (n < g_bankDistance)
                {
                    g_bankRoute = route;
                    g_bankDistance = n;
                }
                break;
            }
        }
    }
    g_bankDistance++;
    for (route = 0; route < g_routes.count; route++)
    {
        g_routeBankDist[route]++;
    }
}

/* All routes of up to maxSteps squares from the player's square and direction (g_routes, at most
 * 4): one walk, the branches met on the way left for later (latest first) and walked from where
 * they were found, each new route starting as a copy of the one before (0x80077EE0) */
/* 0x80077EE0 */
void Player_BuildRoutes(short player, short maxSteps)
{
    struct
    {
        short step;                     /* steps taken when it was met */
        signed char x, floor;
        signed char dx, dfloor;         /* the way it goes */
    } branches[8];
    int pending;
    int n;
    int i;
    u32 exits;
    char x;
    char floor;
    char dx;
    char dfloor;

#define BRANCH(bdx, bdfloor)  (branches[pending].step = n, branches[pending].x = x,        \
                               branches[pending].floor = floor, branches[pending].dx = (bdx), \
                               branches[pending].dfloor = (bdfloor), pending++)

    g_routes.count = 0;
    g_routes.maxSteps = maxSteps;
    memset(g_routeVisited,0,sizeof(g_routeVisited));
    x = g_players[player].x;
    floor = g_players[player].floor;
    dx = g_players[player].dirX;
    dfloor = g_players[player].dirFloor;
    pending = 0;
    n = 0;
    if (maxSteps == 0)
    {
        g_routes.count++;
        return;
    }
    for (;;)
    {
        exits = Board_GetExits(x,floor,dx,dfloor);
        if (exits & EXIT_LEFT)
        {
            if (exits & EXIT_RIGHT)
            {
                BRANCH(1,dfloor);
            }
            if (exits & EXIT_RIGHT_FLOOR_MINUS)
            {
                BRANCH(1,-1);
            }
            if (exits & EXIT_RIGHT_FLOOR_PLUS)
            {
                BRANCH(1,1);
            }
            dx = -1;
        }
        else if (exits & EXIT_RIGHT)
        {
            if (exits & EXIT_LEFT_FLOOR_MINUS)
            {
                BRANCH(-1,-1);
            }
            if (exits & EXIT_LEFT_FLOOR_PLUS)
            {
                BRANCH(-1,1);
            }
            dx = 1;
        }
        else if (exits & EXIT_LEFT_FLOOR_MINUS)
        {
            if (exits & EXIT_LEFT_FLOOR_PLUS)
            {
                BRANCH(-1,1);
            }
            dx = -1;
            dfloor = -1;
        }
        else if (exits & EXIT_RIGHT_FLOOR_MINUS)
        {
            if (exits & EXIT_RIGHT_FLOOR_PLUS)
            {
                BRANCH(1,1);
            }
            dx = 1;
            dfloor = -1;
        }
        else if (exits & EXIT_LEFT_FLOOR_PLUS)
        {
            dx = -1;
            dfloor = 1;
        }
        else if (exits & EXIT_RIGHT_FLOOR_PLUS)
        {
            dx = 1;
            dfloor = 1;
        }
        /* one step */
        g_routes.step[g_routes.count][n].dx = dx;
        g_routes.step[g_routes.count][n].dfloor = dfloor;
        Board_Step(&x,&floor,&dx,&dfloor);
        g_routes.step[g_routes.count][n].x = x;
        g_routes.step[g_routes.count][n].floor = floor;
        n++;
        if (n == maxSteps)
        {
            /* a route done: on with the latest branch */
            g_routes.count++;
            if (pending == 0)
            {
                return;
            }
            pending--;
            if (g_routes.count > 3)
            {
                return;
            }
            n = branches[pending].step;
            x = branches[pending].x;
            floor = branches[pending].floor;
            dx = branches[pending].dx;
            dfloor = branches[pending].dfloor;
            for (i = 0; i < branches[pending].step; i++)
            {
                g_routes.step[g_routes.count][i] = g_routes.step[g_routes.count - 1][i];
            }
        }
    }
#undef BRANCH
}

/* The ways along the connector on side `side` of (x, floor): to floor - 1 (minus) and / or floor
 * + 1 (plus). A connector square 0x300 beside the square leads to floor + 1, one at floor + 1
 * to floor - 1; else by where the connector goes on, and when it goes both ways by vdir (0:
 * both) */
static u32 Board_ConnectorExits(char x, char floor, int side, char vdir, u32 minus, u32 plus)
{
    if (Board_GetCell(x + side,floor) == CELL_CONNECTOR)
    {
        return plus;
    }
    if (Board_GetCell(x + side,floor + 1) == CELL_CONNECTOR)
    {
        return minus;
    }
    if (CELL_TYPE(Board_GetCell(x + side,floor - 1)) != CELL_CONNECTOR)
    {
        return plus;
    }
    if (CELL_TYPE(Board_GetCell(x + side,floor + 1)) != CELL_CONNECTOR)
    {
        return minus;
    }
    if (vdir == 1)
    {
        return plus;
    }
    if (vdir != -1)
    {
        return minus | plus;
    }
    return minus;
}

/* The ways (EXIT_*) a piece moving in dir (1 right, -1 left, 0 not yet) can go on from (x, floor):
 * along the floor, or up / down a connector beside it (0x80078374) */
/* 0x80078374 */
u32 Board_GetExits(char x, char floor, char dir, char vdir)
{
    if (dir == 1)
    {
        if (CELL_TYPE(Board_GetCell(x + 1,floor)) != CELL_CONNECTOR)
        {
            return EXIT_RIGHT;
        }
        return Board_ConnectorExits(x,floor,1,vdir,EXIT_RIGHT_FLOOR_MINUS,EXIT_RIGHT_FLOOR_PLUS);
    }
    if (dir == -1)
    {
        if (CELL_TYPE(Board_GetCell(x - 1,floor)) != CELL_CONNECTOR)
        {
            return EXIT_LEFT;
        }
        return Board_ConnectorExits(x,floor,-1,vdir,EXIT_LEFT_FLOOR_MINUS,EXIT_LEFT_FLOOR_PLUS);
    }
    /* no direction yet: both ways, a connector on the right taking the place of that way (the
     * left is then not looked at) */
    if (CELL_TYPE(Board_GetCell(x + 1,floor)) == CELL_CONNECTOR)
    {
        return EXIT_LEFT | Board_ConnectorExits(x,floor,1,vdir,EXIT_RIGHT_FLOOR_MINUS,EXIT_RIGHT_FLOOR_PLUS);
    }
    if (CELL_TYPE(Board_GetCell(x - 1,floor)) != CELL_CONNECTOR)
    {
        return EXIT_LEFT | EXIT_RIGHT;
    }
    return EXIT_RIGHT | Board_ConnectorExits(x,floor,-1,vdir,EXIT_LEFT_FLOOR_MINUS,EXIT_LEFT_FLOOR_PLUS);
}

/* Move x one square in dir along the floor, over the squares a connector passes over */
static void Board_StepAlong(char *x, char floor, char dir)
{
    do
    {
        *x += dir;
    } while (CELL_TYPE(Board_GetCell(*x,floor)) == CELL_LANDING);
}

/* Move one square in dir. On a connector the piece climbs (vdir; when 0, up if the square above
 * is connector too) until a floor has a square beside it, ahead first, and leaves that way; a dir
 * other than 1 / -1 leaves at once (0x80078778) */
/* 0x80078778 */
void Board_Step(char *x, char *floor, char *dir, char *vdir)
{
    Board_StepAlong(x,*floor,*dir);
    while (CELL_TYPE(Board_GetCell(*x,*floor)) == CELL_CONNECTOR)
    {
        for (;;)
        {
            if (*vdir == 0)
            {
                *vdir = (CELL_TYPE(Board_GetCell(*x,*floor + 1)) == CELL_CONNECTOR) ? 1 : -1;
            }
            *floor += *vdir;
            if (*dir == 1)
            {
                if (CELL_TYPE(Board_GetCell(*x + 1,*floor)) != CELL_NONE)
                {
                    break;
                }
                if (CELL_TYPE(Board_GetCell(*x - 1,*floor)) != CELL_NONE)
                {
                    *dir = -1;
                    break;
                }
            }
            else if (*dir == -1)
            {
                if (CELL_TYPE(Board_GetCell(*x - 1,*floor)) != CELL_NONE)
                {
                    break;
                }
                if (CELL_TYPE(Board_GetCell(*x + 1,*floor)) != CELL_NONE)
                {
                    *dir = 1;
                    break;
                }
            }
            else
            {
                break;
            }
        }
        Board_StepAlong(x,*floor,*dir);
    }
}

/* Square id at (x, floor) from StageDef.squareIds; 0x100 if out of range. Matched against
 * Store.squareId. (0x8007B6B4) */
/* 0x8007B6B4 */
short Board_GetSquareId(short x, short floor)
{
    StageDef *stage = &g_stageDefs[g_boardStage];

    if (x < 0 || x > stage->width - 1 || floor < 0 || floor > stage->floors - 1)
    {
        return 0x100;
    }
    return stage->squareIds[floor * stage->width + x];
}

/* Per-square byte of the stage (StageDef.squareFlags); 0 outside the board. On store squares 1
 * or 2: when a lot with 1 becomes vacant the shutter goes up over it as over an opened store
 * (ShopOpen_Task); 2 (boards 5 and 8) skips that. Store.squareFlag keeps "2", unread
 * (0x8007B744) */
/* 0x8007B744 */
int Board_GetSquareFlag(short x, short floor)
{
    StageDef *stage = &g_stageDefs[g_boardStage];

    if (x < 0 || x > stage->width - 1 || floor < 0 || floor > stage->floors - 1)
    {
        return 0;
    }
    return (char)stage->squareFlags[floor * stage->width + x];
}
