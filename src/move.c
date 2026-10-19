/*
 * move.c - moving a piece
 *
 * Move_Task walks the current player's piece the steps of its roll: along the floors and up and
 * down the stairs, paying out at a bank on the way (src/bank.c), asking a human player which way
 * to go at a fork and, at the end, when another way was possible, whether to stop there ("Stop
 * in this square?").
 */
#include "common.h"
#include "topshop.h"

#define KEEP_FLOOR  2           /* Move_Go: dirFloor stays */

/* Take a way: the piece's direction (dirFloor unless KEEP_FLOOR); 2 at a run (R1), else 1 */
static u32 Move_Go(Player *pl, u16 held, int dirX, int dirFloor)
{
    pl->dirX = dirX;
    if (dirFloor != KEEP_FLOOR)
    {
        pl->dirFloor = dirFloor;
    }
    return (held & PADR1) ? 2 : 1;
}

/* Both ways of a connector on side dirX: up the d-pad goes to floor - 1, down to floor + 1 */
static u32 Move_GoUpOrDown(Player *pl, u16 held, int dirX)
{
    if (held & PADLup)
    {
        return Move_Go(pl,held,dirX,-1);
    }
    if (held & PADLdown)
    {
        return Move_Go(pl,held,dirX,1);
    }
    return 0;
}

/* The direction the human player chooses with the d-pad among the ways the piece can go on
 * (Player.exits, EXIT_*): sets Player.dirX / dirFloor and returns 1 (a step), 2 (at a run: R1
 * held; R1 alone also takes the only way on), or 0 (no choice yet). A plain way along the floor
 * is taken with left / right, a connector with the diagonal's two directions, or up / down where
 * it goes both ways (0x8004F1FC) */
/* 0x8004F1FC */
u32 Move_GetInputDir(void)
{
    Player *pl = &g_players[g_curPlayer];
    u16 held = g_playerPads[g_curPlayer].held;
    u8 exits = pl->exits;

    if (exits & EXIT_LEFT)
    {
        /* left along the floor, or one of the ways on the right */
        if (held & PADLleft)
        {
            return Move_Go(pl,held,-1,KEEP_FLOOR);
        }
        if ((exits & (EXIT_RIGHT_FLOOR_MINUS | EXIT_RIGHT_FLOOR_PLUS)) == (EXIT_RIGHT_FLOOR_MINUS | EXIT_RIGHT_FLOOR_PLUS))
        {
            return Move_GoUpOrDown(pl,held,1);
        }
        if (exits & EXIT_RIGHT_FLOOR_MINUS)
        {
            return (held & (PADLup | PADLright)) ? Move_Go(pl,held,1,-1) : 0;
        }
        if (exits & EXIT_RIGHT_FLOOR_PLUS)
        {
            return (held & (PADLright | PADLdown)) ? Move_Go(pl,held,1,1) : 0;
        }
        if (exits & EXIT_RIGHT)
        {
            return (held & PADLright) ? Move_Go(pl,held,1,KEEP_FLOOR) : 0;
        }
        return (held & PADR1) ? Move_Go(pl,held,-1,KEEP_FLOOR) : 0;
    }
    if (exits & EXIT_RIGHT)
    {
        /* right along the floor, or one of the ways on the left */
        if (held & PADLright)
        {
            return Move_Go(pl,held,1,KEEP_FLOOR);
        }
        if ((exits & (EXIT_LEFT_FLOOR_MINUS | EXIT_LEFT_FLOOR_PLUS)) == (EXIT_LEFT_FLOOR_MINUS | EXIT_LEFT_FLOOR_PLUS))
        {
            return Move_GoUpOrDown(pl,held,-1);
        }
        if (exits & EXIT_LEFT_FLOOR_MINUS)
        {
            return (held & (PADLup | PADLleft)) ? Move_Go(pl,held,-1,-1) : 0;
        }
        if (exits & EXIT_LEFT_FLOOR_PLUS)
        {
            return (held & (PADLdown | PADLleft)) ? Move_Go(pl,held,-1,1) : 0;
        }
        return (held & PADR1) ? Move_Go(pl,held,1,KEEP_FLOOR) : 0;
    }
    /* connectors only */
    if ((exits & (EXIT_LEFT_FLOOR_MINUS | EXIT_LEFT_FLOOR_PLUS)) == (EXIT_LEFT_FLOOR_MINUS | EXIT_LEFT_FLOOR_PLUS))
    {
        return Move_GoUpOrDown(pl,held,-1);
    }
    if (exits & EXIT_LEFT_FLOOR_MINUS)
    {
        return (held & (PADR1 | PADLup | PADLleft)) ? Move_Go(pl,held,-1,-1) : 0;
    }
    if (exits & EXIT_LEFT_FLOOR_PLUS)
    {
        return (held & (PADR1 | PADLdown | PADLleft)) ? Move_Go(pl,held,-1,1) : 0;
    }
    if ((exits & (EXIT_RIGHT_FLOOR_MINUS | EXIT_RIGHT_FLOOR_PLUS)) == (EXIT_RIGHT_FLOOR_MINUS | EXIT_RIGHT_FLOOR_PLUS))
    {
        return Move_GoUpOrDown(pl,held,1);
    }
    if (exits & EXIT_RIGHT_FLOOR_MINUS)
    {
        return (held & (PADR1 | PADLup | PADLright)) ? Move_Go(pl,held,1,-1) : 0;
    }
    if (exits & EXIT_RIGHT_FLOOR_PLUS)
    {
        return (held & (PADR1 | PADLright | PADLdown)) ? Move_Go(pl,held,1,1) : 0;
    }
    return 0;
}

/* Where each of the routes (g_routes) ends, into the current player's routeEnds */
static void Move_RecordRouteEnds(void)
{
    Player *pl = &g_players[g_curPlayer];
    int i;

    for (i = 0; i < 4; i++)
    {
        if (i < g_routes.count)
        {
            pl->routeEnds[i * 2] = g_routes.step[i][g_routes.maxSteps - 1].x;
            pl->routeEnds[i * 2 + 1] = g_routes.step[i][g_routes.maxSteps - 1].floor;
        }
        else
        {
            pl->routeEnds[i * 2] = 0xff;
            pl->routeEnds[i * 2 + 1] = 0xff;
        }
    }
}

/* Move_Task's steps, one phase function each */
enum MoveStep
{
    MOVE_START                =    0,  /* StepsLeft_Task; a human player: MOVE_ROUTES, a CPU: MOVE_CPU_START */
    MOVE_ROUTES               =  100,  /* Move_Start: the routes for the dice roll */
    MOVE_AFTER_STEP           =  105,  /* the square info 15 frames later */
    MOVE_EXITS                =  106,  /* the ways on; a backup for a step back */
    MOVE_CHOOSE               =  110,  /* the player chooses a direction (circle: back, square: the map) */
    MOVE_STEP                 =  120,  /* a step forward, then MOVE_STEPPED */
    MOVE_STEPPED              =  121,  /* there: the bank, the end of the move, or the next choice */
    MOVE_PAYDAY               =  130,  /* the bank: Payday_Task */
    MOVE_WAIT_PAYDAY          =  131,
    MOVE_EXCHANGE             =  140,  /* PointsExchange_Task */
    MOVE_WAIT_EXCHANGE        =  141,
    MOVE_EVENT                =  145,  /* the event card bought there: Event_ApplyTask */
    MOVE_WAIT_EVENT           =  146,
    MOVE_WAIT_PANEL           =  147,  /* wait for the player panel to settle */
    MOVE_BANK_DONE            =  150,  /* on: the routes again for the steps left */
    MOVE_STEP_BACK            =  160,  /* a step back, then MOVE_STEPPED_BACK */
    MOVE_STEPPED_BACK         =  161,  /* the backup restored */
    MOVE_MAP_VIEW             =  180,  /* Board_MapViewTask */
    MOVE_WAIT_MAP_VIEW        =  181,
    MOVE_CPU_START            =  200,  /* Move_Walk: the route the CPU player chose */
    MOVE_CPU_EXITS            =  210,
    MOVE_CPU_STEP             =  220,  /* the next step of the route, then MOVE_CPU_STEPPED */
    MOVE_CPU_STEPPED          =  221,
    MOVE_CPU_PAYDAY           =  230,
    MOVE_CPU_WAIT_PAYDAY      =  231,
    MOVE_CPU_EXCHANGE         =  240,
    MOVE_CPU_WAIT_EXCHANGE    =  241,
    MOVE_CPU_EVENT            =  245,
    MOVE_CPU_WAIT_EVENT       =  246,
    MOVE_CPU_WAIT_PANEL       =  247,
    MOVE_CPU_BANK_DONE        =  250,
    MOVE_WALK                 = 1000,  /* Move_EnterConnector: one step to the next square */
    MOVE_REACHED              = 1001,  /* a landing, a connector or a square */
    MOVE_WALK_ON              = 1100,  /* the piece walks onto the square */
    MOVE_WALKING_ON           = 1101,  /* then the step in work->timer */
    MOVE_CONNECTOR_RIGHT_UP   = 1200,  /* Move_ConnectorRightA (+ CONN_*) */
    MOVE_CONNECTOR_LEFT_UP    = 1300,  /* Move_ConnectorLeftA */
    MOVE_CONNECTOR_LEFT_DOWN  = 1400,  /* Move_ConnectorLeftB */
    MOVE_CONNECTOR_RIGHT_DOWN = 1500,  /* Move_ConnectorRightB */
    MOVE_END                  = 2000,  /* Move_AskStop: the move is over */
    MOVE_END_PANEL            = 2001,  /* StepsLeft_Task's window closes */
    MOVE_END_WAIT_PANEL       = 2002,
    MOVE_ASK_STOP             = 2010,  /* "Stop in this square?" (No: MOVE_STEP_BACK) */
    MOVE_ASK_STOP_ANSWER      = 2011,
    MOVE_STOP                 = 2020,  /* "Stop in this square." */
    MOVE_WAIT_STOP            = 2021,
    MOVE_DONE                 = 3000,  /* the routes off the map */
    MOVE_FINISHED             = 3001   /* work->done */
};

/* Move_Task steps 0-199: the human player's move: the routes for the dice roll (shown on the map),
 * a direction chosen a square at a time (circle steps back, restoring the backup; square opens
 * the map view), the bank on the way (payday, the points exchange and the event bought there);
 * a CPU player goes to step 200 (Move_Walk) (0x8004F7E4) */
/* 0x8004F7E4 */
u32 Move_Start(short *step, MoveWork *work)
{
    Player *pl = &g_players[g_curPlayer];
    u32 dir;

    switch(*step)
    {
    case MOVE_START:
        work->done = 0;
        Task_SetUserData(-1,0);
        work->busy = 1;
        work->waitMode = 0;
        work->frame = 0;
        Task_Start(g_uiTaskIds[1],StepsLeft_Task);
        *step = (pl->isCpu == 0) ? MOVE_ROUTES : MOVE_CPU_START;
        break;
    case MOVE_ROUTES:                         /* the routes for the dice roll */
        Task_SendMessage(g_minimapTaskId,DISP_ROUTES_ON);
        Player_BuildRoutes(g_curPlayer,(char)pl->steps);
        Task_SendMessage(g_minimapTaskId,DISP_ROUTES_ON);
        Move_RecordRouteEnds();
        work->infoDelay = -1;
        *step = MOVE_EXITS;
        return 1;
    case MOVE_AFTER_STEP:                    /* after a step */
        work->infoDelay = 15;
        *step += 1;
        return 1;
    case MOVE_EXITS:
        pl->exits = Board_GetExits(pl->x,pl->floor,pl->dirX,pl->dirFloor);
        Player_Backup(g_curPlayer,(char)pl->steps - (char)pl->stepsLeft);
        *step = MOVE_CHOOSE;
        return 1;
    case MOVE_CHOOSE:                        /* the player chooses a direction */
        pl->choosing = 1;
        /* the square info shows again 15 frames after a step */
        if (work->infoDelay == 0)
        {
            Task_SendMessage(g_squareInfoTaskId,DISP_REFRESH);
            work->infoDelay = -1;
        }
        else if (work->infoDelay > 0)
        {
            work->infoDelay--;
        }
        if (g_playerPads[g_curPlayer].pressed & PADRleft)
        {
            *step = MOVE_MAP_VIEW;               /* square: the map view */
            break;
        }
        dir = Move_GetInputDir();
        if (dir != 0)
        {
            work->dash = (dir == 2);
            *step = MOVE_STEP;
            return 1;
        }
        if ((g_playerPads[g_curPlayer].held & PADRright) == 0)
        {
            /* no input: standing at the start, else walking on the spot */
            if (pl->stepsLeft == pl->steps)
            {
                pl->pieceAnim = 0;
                pl->pieceFlip = 0;
            }
            else
            {
                pl->pieceAnim = 2;
                pl->pieceFlip = (pl->dirX == 1) ? 0x10 : 0;
            }
            return 0;
        }
        /* circle held: a step back, when there is a choice of routes, not on the bank and not at
         * the start */
        if (g_routes.count < 2)
        {
            return 0;
        }
        if (CELL_TYPE(Board_GetCell(pl->x,pl->floor)) == CELL_BANK)
        {
            return 0;
        }
        if ((char)pl->steps <= (char)pl->stepsLeft)
        {
            return 0;
        }
        *step = MOVE_STEP_BACK;
        return 0;
    case MOVE_STEP:                          /* a step forward (Move_Walk, then 0x79) */
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        pl->choosing = 0;
        pl->stepsLeft--;
        pl->stepsSincePayday++;
        pl->totalSteps++;
        work->fromX = pl->x;
        work->fromFloor = pl->floor;
        work->dirX = pl->dirX;
        work->dirFloor = pl->dirFloor;
        work->timer = MOVE_STEPPED;
        work->stepBack = 0;
        *step = MOVE_WALK;
        return 1;
    case MOVE_STEPPED:                       /* there */
        pl->x = work->fromX;
        pl->floor = work->fromFloor;
        pl->dirX = work->dirX;
        pl->dirFloor = work->dirFloor;
        if (Board_GetCell(pl->x,pl->floor) == CELL_BANK)
        {
            *step = (g_boardRules.enable[RULE_BANK] == 0) ? MOVE_EXCHANGE : MOVE_PAYDAY;
        }
        else if (pl->stepsLeft == 0)
        {
            *step = MOVE_END;
        }
        else
        {
            *step = MOVE_AFTER_STEP;
        }
        return 1;

    /* 0x82-0x96: the bank: payday, the points exchange and its event, then on */
    case MOVE_PAYDAY:
        Task_SendMessage(g_squareInfoTaskId,DISP_REFRESH);
        pl->pieceAnim = 0;
        pl->pieceFlip = 0;
        pl->paydays++;
        Task_SendMessage(g_uiTaskIds[1],0);
        g_uiTaskWork[7][1] = (pl->stepsLeft == 0) ? 1 : 0;      /* stopping on it: the bonus */
        Task_Start(g_uiTaskIds[7],Payday_Task);
        *step += 1;
        break;
    case MOVE_WAIT_PAYDAY:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        pl->stepsSincePayday = 0;
        *step = MOVE_EXCHANGE;
        break;
    case MOVE_EXCHANGE:
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        g_uiTaskWork[7][1] = 0;
        if (g_boardRules.enable[RULE_AUTO_EXCHANGE] != 0)
        {
            work->savedIsCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        Task_Start(g_uiTaskIds[7],PointsExchange_Task);
        *step += 1;
        break;
    case MOVE_WAIT_EXCHANGE:
        if (!Task_IsDone(g_uiTaskIds[7]))
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_EXCHANGE] != 0)
        {
            pl->isCpu = work->savedIsCpu;
        }
        *step = MOVE_EVENT;
        break;
    case MOVE_EVENT:                         /* the event card bought (user data of the exchange) */
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            work->savedIsCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        work->busy = 0;
        *(int *)g_actionTaskWork[0] = Task_GetUserData(g_uiTaskIds[7]);
        Task_Start(g_actionTaskIds[0],Event_ApplyTask);
        *step += 1;
        break;
    case MOVE_WAIT_EVENT:
        if (!Task_IsDone(g_actionTaskIds[0]))
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            pl->isCpu = work->savedIsCpu;
        }
        if (Task_GetUserData(g_actionTaskIds[0]) == 1)
        {
            /* the event ended the move */
            work->busy = 1;
            Task_SetUserData(-1,1);
            *step = MOVE_DONE;
        }
        else
        {
            Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
            work->busy = 1;
            *step += 1;
        }
        break;
    case MOVE_WAIT_PANEL:
        if (Task_GetStep(g_playerPanelTaskId) != PANEL_HIDDEN)
        {
            return 0;
        }
        if (pl->stepsLeft != 0)
        {
            Task_Start(g_uiTaskIds[1],StepsLeft_Task);
            Task_SendMessage(g_calendarTaskId,DISP_SHOW);
            Task_SendMessage(g_minimapTaskId,DISP_SHOW_LOW);
        }
        *step = MOVE_BANK_DONE;
        break;
    case MOVE_BANK_DONE:                     /* on: the routes again for the steps left */
        if (pl->stepsLeft == 0)
        {
            *step = MOVE_END_PANEL;
        }
        else
        {
            pl->steps = pl->stepsLeft;
            *step = MOVE_ROUTES;
        }
        return 1;

    case MOVE_STEP_BACK:                     /* a step back (Move_Walk, then 0xa1) */
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        pl->stepsLeft++;
        work->fromX = pl->x;
        work->fromFloor = pl->floor;
        work->dirX = -pl->dirX;
        work->stepBack = 1;
        work->timer = MOVE_STEPPED_BACK;
        work->dash = 0;
        work->dirFloor = -pl->dirFloor;
        *step = MOVE_WALK;
        return 1;
    case MOVE_STEPPED_BACK:
        Player_Restore(g_curPlayer,(char)pl->steps - (char)pl->stepsLeft);
        *step = MOVE_AFTER_STEP;
        return 1;

    case MOVE_MAP_VIEW:                      /* the map view */
        work->busy = 0;
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_SendMessage(g_uiTaskIds[1],0);
        Task_Start(g_mapViewTaskId,Board_MapViewTask);
        *step += 1;
        break;
    case MOVE_WAIT_MAP_VIEW:
        if (!Task_IsDone(g_mapViewTaskId))
        {
            return 0;
        }
        work->busy = 1;
        Task_SendMessage(g_calendarTaskId,DISP_SHOW);
        Task_Start(g_uiTaskIds[1],StepsLeft_Task);
        *step = MOVE_AFTER_STEP;
        break;
    }
    return 0;
}

/* Move_Task steps 200-999: a CPU player walks the route it chose (Cpu_ChooseRoute) square by
 * square; the bank on the way: payday (when the rule is on) and the points exchange, and its
 * event (0x800504F0) */
/* 0x800504F0 */
u32 Move_Walk(short *step, MoveWork *work)
{
    Player *pl;
    int n;

    pl = &g_players[g_curPlayer];
    switch(*step)
    {
    case MOVE_CPU_START:
        Task_SendMessage(g_minimapTaskId,DISP_ROUTES_ON);
        work->route = Cpu_ChooseRoute(g_curPlayer,pl->steps);
        Player_BuildRoutes(g_curPlayer,(char)pl->steps);
        Task_SendMessage(g_minimapTaskId,DISP_ROUTES_ON);
        Move_RecordRouteEnds();
        pl->routeEnds[8] = g_routes.step[work->route][g_routes.maxSteps - 1].x;
        pl->routeEnds[9] = g_routes.step[work->route][g_routes.maxSteps - 1].floor;
        *step = MOVE_CPU_EXITS;
        return 1;
    case MOVE_CPU_EXITS:
        pl->choosing = 1;
        pl->exits = Board_GetExits(pl->x,pl->floor,pl->dirX,pl->dirFloor);
        Player_Backup(g_curPlayer,(char)pl->steps - (char)pl->stepsLeft);
        *step = MOVE_CPU_STEP;
        return 1;
    case MOVE_CPU_STEP:                      /* the next step of the route */
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        pl->choosing = 0;
        n = (char)pl->steps - (char)pl->stepsLeft;
        pl->dirX = g_routes.step[work->route][n].dx;
        pl->dirFloor = g_routes.step[work->route][n].dfloor;
        pl->stepsLeft--;
        pl->stepsSincePayday++;
        pl->totalSteps++;
        work->fromX = pl->x;
        work->fromFloor = pl->floor;
        work->dirX = pl->dirX;
        work->dirFloor = pl->dirFloor;
        work->stepBack = 0;
        /* fast CPU players, and long moves, run */
        work->dash = (g_optCpuFast < 1 && (char)pl->steps < 4) ? 0 : 1;
        work->timer = MOVE_CPU_STEPPED;
        *step = MOVE_WALK;
        return 1;
    case MOVE_CPU_STEPPED:                   /* there */
        pl->x = work->fromX;
        pl->floor = work->fromFloor;
        pl->dirX = work->dirX;
        pl->dirFloor = work->dirFloor;
        if (Board_GetCell(pl->x,pl->floor) == CELL_BANK)
        {
            *step = (g_boardRules.enable[RULE_BANK] == 0) ? MOVE_CPU_EXCHANGE : MOVE_CPU_PAYDAY;
        }
        else if (pl->stepsLeft == 0)
        {
            *step = MOVE_END;
        }
        else
        {
            *step = MOVE_CPU_EXITS;
        }
        return 1;
    /* 0xe6-0xfa: the bank: payday, the points exchange and its event, then on */
    case MOVE_CPU_PAYDAY:
        Task_SendMessage(g_squareInfoTaskId,DISP_REFRESH);
        pl->pieceAnim = 0;
        pl->pieceFlip = 0;
        pl->paydays++;
        Task_SendMessage(g_uiTaskIds[1],0);
        g_uiTaskWork[7][1] = (pl->stepsLeft == 0) ? 1 : 0;    /* stopping on it: the bonus */
        Task_Start(g_uiTaskIds[7],Payday_Task);
        *step += 1;
        break;
    case MOVE_CPU_WAIT_PAYDAY:
        if (Task_IsDone(g_uiTaskIds[7]) == 0)
        {
            return 0;
        }
        pl->stepsSincePayday = 0;
        *step = MOVE_CPU_EXCHANGE;
        return 0;
    case MOVE_CPU_EXCHANGE:
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        if (g_boardRules.enable[RULE_AUTO_EXCHANGE] != 0)
        {
            work->savedIsCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        g_uiTaskWork[7][1] = 0;
        Task_Start(g_uiTaskIds[7],PointsExchange_Task);
        *step += 1;
        break;
    case MOVE_CPU_WAIT_EXCHANGE:
        if (Task_IsDone(g_uiTaskIds[7]) == 0)
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_EXCHANGE] != 0)
        {
            pl->isCpu = work->savedIsCpu;
        }
        *step = MOVE_CPU_EVENT;
        return 0;
    case MOVE_CPU_EVENT:
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            work->savedIsCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        work->busy = 0;
        *(int *)g_actionTaskWork[0] = Task_GetUserData(g_uiTaskIds[7]);
        Task_Start(g_actionTaskIds[0],Event_ApplyTask);
        *step += 1;
        break;
    case MOVE_CPU_WAIT_EVENT:
        if (Task_IsDone(g_actionTaskIds[0]) == 0)
        {
            return 0;
        }
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            pl->isCpu = work->savedIsCpu;
        }
        if (Task_GetUserData(g_actionTaskIds[0]) == 1)
        {
            work->busy = 1;
            Task_SetUserData(-1,1);
            *step = MOVE_DONE;
            return 0;
        }
        Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
        work->busy = 1;
        *step += 1;
        break;
    case MOVE_CPU_WAIT_PANEL:
        if (Task_GetStep(g_playerPanelTaskId) == PANEL_HIDDEN)
        {
            if (pl->stepsLeft != 0)
            {
                Task_Start(g_uiTaskIds[1],StepsLeft_Task);
                Task_SendMessage(g_calendarTaskId,DISP_SHOW);
                Task_SendMessage(g_minimapTaskId,DISP_SHOW_LOW);
            }
            *step = MOVE_CPU_BANK_DONE;
        }
        return 0;
    case MOVE_CPU_BANK_DONE:
        if (pl->stepsLeft == 0)
        {
            *step = MOVE_END_PANEL;
        }
        else
        {
            *step = MOVE_CPU_START;               /* steps left: choose again */
            pl->steps = pl->stepsLeft;
        }
        return 1;
    }
    return 0;
}

/* Player.pieceAnim values of the piece walking the board */
#define PIECE_CLIMB    1   /* on a connector */
#define PIECE_WALK     2
#define PIECE_BACKWARD 3   /* a step back (MoveWork.stepBack): facing the other way */
#define PIECE_DASH     4   /* dashing (MoveWork.dash) */

/* The piece walks along the floor in direction dirX: 8 pixels a frame, dashing 16; a step back
 * (walking backwards, 16) shows the picture facing the other way */
static void Move_SetWalk(MoveWork *work)
{
    Player *pl = &g_players[g_curPlayer];

    if (work->stepBack == 0)
    {
        if (work->dash == 0)
        {
            pl->pieceAnim = PIECE_WALK;
            work->dx = (char)work->dirX * 8;
        }
        else
        {
            pl->pieceAnim = PIECE_DASH;
            work->dx = (char)work->dirX * 16;
        }
        pl->pieceFlip = (work->dirX == 1) ? 0x10 : 0;
    }
    else
    {
        pl->pieceAnim = PIECE_BACKWARD;
        work->dx = (char)work->dirX * 16;
        pl->pieceFlip = (work->dirX == 1) ? 0 : 0x10;
    }
}

/* The piece starts up / down a connector in direction dir (1 right, -1 left) */
static void Move_SetClimb(MoveWork *work, int dir)
{
    Player *pl = &g_players[g_curPlayer];

    if (work->stepBack == 0)
    {
        if (work->dash == 0)
        {
            pl->pieceAnim = PIECE_CLIMB;
            work->dx = dir * 8;
        }
        else
        {
            pl->pieceAnim = PIECE_DASH;
            work->dx = dir * 16;
        }
        pl->pieceFlip = (dir == 1) ? 0x10 : 0;
    }
    else
    {
        pl->pieceAnim = PIECE_CLIMB;
        pl->pieceFlip = (dir == 1) ? 0 : 0x10;
        work->dx = dir * 16;
    }
}

/* Move the piece by dx; 1 once it is at targetX going in direction dir (1 right, -1 left;
 * 0: never) */
static int Move_WalkX(MoveWork *work, int dir)
{
    Player *pl = &g_players[g_curPlayer];

    pl->screenX += work->dx;
    if ((dir == 1 && pl->screenX >= work->targetX) || (dir == -1 && pl->screenX <= work->targetX))
    {
        pl->screenX = work->targetX;
        return 1;
    }
    return 0;
}

/* Screen x of square column x */
#define MOVE_CELL_X(x)  (((short)g_stageDefs[g_boardStage].originX + (x) * 5) * 16)

/* Face direction dir (1, -1), as Move_SetWalk faces */
static void Move_Face(MoveWork *work, int dir)
{
    work->dirX = dir;
    g_players[g_curPlayer].pieceFlip = ((dir == 1) == (work->stepBack == 0)) ? 0x10 : 0;
}

/* The end of a connector: the walk on to the next square (connector squares: step 1001) */
static void Move_LeaveConnector(short *step, MoveWork *work)
{
    u16 cell;

    if (work->stepBack != 0)
    {
        g_players[g_curPlayer].pieceAnim = PIECE_BACKWARD;
        work->dx = (char)work->dirX * 16;
    }
    else if (work->dash == 0)
    {
        g_players[g_curPlayer].pieceAnim = PIECE_WALK;
        work->dx = (char)work->dirX * 8;
    }
    else
    {
        g_players[g_curPlayer].pieceAnim = PIECE_DASH;
        work->dx = (char)work->dirX * 16;
    }
    work->targetX = MOVE_CELL_X(work->fromX) + 0x28;
    cell = Board_GetCell(work->fromX,work->fromFloor);
    *step = (CELL_TYPE(cell) == CELL_CONNECTOR || CELL_TYPE(cell) == CELL_LANDING) ? MOVE_REACHED : *step + 1;
}

/* One of the four ways along a connector (Move_Connector*) */
typedef struct ConnectorPath
{
    short base;               /* its first Move_Task step */
    signed char dir;          /* x direction on the connector */
    signed char up;           /* 1: up a floor, 0: down */
    short foot, start, end;   /* x offsets (with baseY) of the foot, the start of the climb, its end */
    signed char check330;     /* shape 0x330 connectors (checked on this floor: 1, the one above: 2) draw
                               * the piece as 0xBE during the climb; 0: no check */
    signed char deadEnd310;   /* direction off a dead end of shape 0x310 (0x320: the other way) */
    signed char deadEndFacing;/* 1: there the piece faces as walking, also when running */
} ConnectorPath;

/* Move_Connector's steps, from its path's first step (ConnectorPath.base) */
enum ConnectorStep
{
    CONN_TO_FOOT       =  0,  /* walk to the foot of the connector */
    CONN_WALK_TO_FOOT  =  1,
    CONN_TO_START      = 10,  /* climbing (sound 0x24): to the start of the climb */
    CONN_WALK_TO_START = 11,
    CONN_CLIMB         = 20,  /* to the other floor */
    CONN_CLIMBING      = 21,
    CONN_TO_END        = 30,  /* to the end of the climb */
    CONN_WALK_TO_END   = 31,
    CONN_LEAVE         = 40,  /* off the connector, to a side with a square */
    CONN_WALK_OFF      = 41   /* then the step in work->timer */
};

/* Along a connector: walk to its foot, climb (sound 0x24) to the other floor, walk off and on to
 * a side with a square (a dead end: by the connector's shape), then to that square */
static u32 Move_Connector(short *step, MoveWork *work, const ConnectorPath *c)
{
    Player *pl = &g_players[g_curPlayer];
    int speed;
    int dir;

    switch (*step - c->base)
    {
    case CONN_TO_FOOT:                        /* to the foot of the connector */
        Move_SetWalk(work);
        work->targetX = MOVE_CELL_X(work->fromX) + work->baseY + c->foot;
        *step += 1;
        return 1;
    case CONN_WALK_TO_FOOT:
        if (Move_WalkX(work,(char)work->dirX))
        {
            *step = c->base + CONN_TO_START;
        }
        break;
    case CONN_TO_START:
        work->seSlot = Se_Play(0x24,0x7f,0x7f,1);
        work->waitMode = 0;
        Move_SetClimb(work,c->dir);
        work->targetX = MOVE_CELL_X(work->fromX) + work->baseY + c->start;
        *step += 1;
        return 1;
    case CONN_WALK_TO_START:
        if (Move_WalkX(work,c->dir))
        {
            *step = c->base + CONN_CLIMB;
        }
        break;
    case CONN_CLIMB:                          /* to the other floor */
        if (c->check330 != 0 &&
            (Board_GetCell(work->fromX,work->fromFloor + (c->check330 - 1)) & 0xfff0) == CONNECTOR_STAIRS_CROSSED)
        {
            pl->pieceState = 0xbe;
        }
        work->fromFloor += work->dirFloor;
        work->targetY = (g_stageDefs[g_boardStage].originY + work->fromFloor * 5) * 16 + 0x3a;
        speed = (work->stepBack == 0 && work->dash == 0) ? 8 : 16;
        work->dx = c->dir * speed;
        work->dy = c->up ? -speed : speed;
        *step += 1;
        return 1;
    case CONN_CLIMBING:
        pl->screenX += work->dx;
        pl->screenY += work->dy;
        if (c->up ? pl->screenY > work->targetY : pl->screenY < work->targetY)
        {
            return 0;
        }
        if (c->check330 != 0)
        {
            pl->pieceState = 0xd2;
        }
        pl->screenY = work->targetY;
        *step = c->base + CONN_TO_END;
        break;
    case CONN_TO_END:
        work->dx = c->dir * ((work->stepBack == 0) ? 8 : 16);
        work->targetX = MOVE_CELL_X(work->fromX) + work->baseY + c->end;
        *step += 1;
        return 1;
    case CONN_WALK_TO_END:
        if (Move_WalkX(work,c->dir))
        {
            *step = c->base + CONN_LEAVE;
        }
        break;
    case CONN_LEAVE:                          /* off the connector: on to a side with a square */
        Se_StopSlot(work->seSlot);
        work->waitMode = 1;
        if (Board_GetCell(work->fromX + c->dir,work->fromFloor) != CELL_NONE)
        {
            Move_Face(work,c->dir);
            work->fromX += work->dirX;
        }
        else if (Board_GetCell(work->fromX - c->dir,work->fromFloor) != CELL_NONE)
        {
            Move_Face(work,-c->dir);
            work->fromX += work->dirX;
        }
        else
        {
            /* a dead end: turn by the connector's shape */
            dir = 0;
            if ((Board_GetCell(work->fromX,work->fromFloor) & 0xfff0) == CONNECTOR_STAIRS)
            {
                dir = c->deadEnd310;
            }
            else if ((Board_GetCell(work->fromX,work->fromFloor) & 0xfff0) == CONNECTOR_STAIRS_FLIPPED)
            {
                dir = -c->deadEnd310;
            }
            if (dir != 0)
            {
                Move_Face(work,dir);
                if (c->deadEndFacing)
                {
                    pl->pieceFlip = (dir == 1) ? 0x10 : 0;
                }
            }
        }
        Move_LeaveConnector(step,work);
        return 1;
    case CONN_WALK_OFF:
        if (Move_WalkX(work,(char)work->dirX))
        {
            work->waitMode = 0;
            *step = work->timer;
        }
        break;
    }
    return 0;
}

/* Move_Task steps 1000-1199: one step along the floor (1000: to the next square; 1100, 1101:
 * the piece walks there), and onto a connector square (CELL_CONNECTOR, stairs between floors:
 * on to Move_Connector*, by the connector's shape and the way up or down) (0x80050F98) */
/* 0x80050F98 */
u32 Move_EnterConnector(u16 *step, MoveWork *work)
{
    u16 cell;
    u16 shape;
    int height;

    switch (*step)
    {
    case MOVE_WALK:
        g_players[g_curPlayer].pieceState = 0xd2;
        work->waitMode = (work->dash == 0) ? 1 : 2;
        work->fromX += work->dirX;
        *step += 1;
        return 1;
    case MOVE_REACHED:
        break;
    case MOVE_WALK_ON:
        Move_SetWalk(work);
        work->targetX = MOVE_CELL_X(work->fromX) + 0x28;
        *step += 1;
        return 1;
    case MOVE_WALKING_ON:
        if (Move_WalkX(work,(char)work->dirX) == 0)
        {
            return 0;
        }
        work->waitMode = 0;
        *step = work->timer;
        return 1;
    default:
        return 0;
    }

    /* 1001: the square reached */
    cell = Board_GetCell(work->fromX,work->fromFloor);
    if (CELL_TYPE(cell) == CELL_LANDING)
    {
        *step = MOVE_WALK;
        return 1;
    }
    if (CELL_TYPE(cell) != CELL_CONNECTOR)
    {
        *step = MOVE_WALK_ON;
        return 1;
    }
    /* a connector: the height of the piece on it, from the low bits of the square at its foot
     * (going down: this one, up: the one above) */
    if (work->dirFloor == 0xff)
    {
        height = Board_GetCell(work->fromX,work->fromFloor) & 0xf;
    }
    else
    {
        height = Board_GetCell(work->fromX,work->fromFloor + 1) & 0xf;
    }
    switch (height)
    {
    case 0:
        work->baseY = 0;
        break;
    case 1:
        work->baseY = 8;
        break;
    case 2:
        work->baseY = 0x10;
        break;
    }
    /* no way chosen yet: up when the square above is a connector too */
    if (work->dirFloor == 0)
    {
        work->dirFloor = (CELL_TYPE(Board_GetCell(work->fromX,work->fromFloor + 1)) == CELL_CONNECTOR) ? 1 : 0xff;
    }
    if (work->dirFloor == 0xff)
    {
        shape = cell & 0xfff0;
        if (shape == CONNECTOR_STAIRS)
        {
            *step = MOVE_CONNECTOR_RIGHT_UP;
        }
        else if (shape == CONNECTOR_STAIRS_FLIPPED)
        {
            *step = MOVE_CONNECTOR_LEFT_UP;
        }
        else if (shape == CONNECTOR_STAIRS_CROSSED)
        {
            *step = (work->dirX == 1) ? MOVE_CONNECTOR_RIGHT_UP : MOVE_CONNECTOR_LEFT_UP;
        }
    }
    else
    {
        shape = Board_GetCell(work->fromX,work->fromFloor + 1) & 0xfff0;
        if (shape == CONNECTOR_STAIRS)
        {
            *step = MOVE_CONNECTOR_LEFT_DOWN;
        }
        else if (shape == CONNECTOR_STAIRS_FLIPPED)
        {
            *step = MOVE_CONNECTOR_RIGHT_DOWN;
        }
        else if (shape == CONNECTOR_STAIRS_CROSSED)
        {
            *step = (work->dirX == 0xff) ? MOVE_CONNECTOR_LEFT_DOWN : MOVE_CONNECTOR_RIGHT_DOWN;
        }
    }
    return 1;
}

/* Move_Task steps 1200-1299: along a connector, right and up (0x80051498) */
/* 0x80051498 */
u32 Move_ConnectorRightA(short *step, MoveWork *work)
{
    static const ConnectorPath path = {MOVE_CONNECTOR_RIGHT_UP, 1, 1, -0x20, -0x10, 0x60, 1, -1, 1};

    return Move_Connector(step,work,&path);
}

/* Move_Task steps 1300-1399: along a connector, left and up (0x80051FF0) */
/* 0x80051FF0 */
u32 Move_ConnectorLeftA(short *step, MoveWork *work)
{
    static const ConnectorPath path = {MOVE_CONNECTOR_LEFT_UP, -1, 1, 0x60, 0x50, -0x20, 0, -1, 0};

    return Move_Connector(step,work,&path);
}

/* Move_Task steps 1400-1499: along a connector, left and down (0x80052AF0) */
/* 0x80052AF0 */
u32 Move_ConnectorLeftB(short *step, MoveWork *work)
{
    static const ConnectorPath path = {MOVE_CONNECTOR_LEFT_DOWN, -1, 0, 0x60, 0x40, -0x10, 2, 1, 0};

    return Move_Connector(step,work,&path);
}

/* Move_Task steps 1500-1599: along a connector, right and down (0x8005361C) */
/* 0x8005361C */
u32 Move_ConnectorRightB(short *step, MoveWork *work)
{
    static const ConnectorPath path = {MOVE_CONNECTOR_RIGHT_DOWN, 1, 0, -0x20, 0, 0x60, 0, 1, 0};

    return Move_Connector(step,work,&path);
}

/* Move_Task steps 2000-: the move is over: "Stop in this square?" when the player could have
 * gone another way and is not on the bank (No: on, StepsLeft_Task again, step 0xA0), else "Stop
 * in this square."; fast CPU players skip both (0x800540D8) */
/* 0x800540D8 */
u32 Move_AskStop(u16 *step, MoveWork *work)
{
    Player *cur = &g_players[g_curPlayer];

    switch (*step)
    {
    case MOVE_END:
        Task_SendMessage(g_squareInfoTaskId,DISP_REFRESH);
        *step += 1;
        return 0;
    case MOVE_END_PANEL:
        cur->pieceAnim = 0;
        cur->pieceFlip = 0;
        Task_SendMessage(g_uiTaskIds[1],0);     /* StepsLeft_Task's window closes */
        *step += 1;
        return 0;
    case MOVE_END_WAIT_PANEL:
        if (!Task_IsDone(g_uiTaskIds[1]))
        {
            return 0;
        }
        if (g_routes.count != 1 && Board_GetCell(cur->x,cur->floor) != CELL_BANK)
        {
            *step = MOVE_ASK_STOP;
        }
        else
        {
            *step = MOVE_STOP;
        }
        return 0;
    case MOVE_ASK_STOP:
        if (g_optCpuFast < 1 || cur->isCpu == 0)
        {
            Msg_AskYesNo(cur->isCpu,0,MSG_AUTO_POS,0x80,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%CStop in this square?%KYes%KNo%E");
            *step += 1;
            return 0;
        }
        *step = MOVE_DONE;
        return 0;
    case MOVE_ASK_STOP_ANSWER:
        if (!Msg_IsAnswered())
        {
            return 0;
        }
        if (g_msgAnswer != 0)
        {
            Task_Start(g_uiTaskIds[1],StepsLeft_Task);
            *step = MOVE_STEP_BACK;
            return 0;
        }
        *step = MOVE_DONE;
        return 0;
    case MOVE_STOP:
        if (g_optCpuFast > 0 && cur->isCpu != 0)
        {
            *step = MOVE_DONE;
            return 0;
        }
        Msg_Show(cur->isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,PADRright | PADRdown,"%CStop in this square.%E");
        *step += 1;
        return 0;
    case MOVE_WAIT_STOP:
        if (Msg_IsDone())
        {
            *step = MOVE_DONE;
        }
        return 0;
    case MOVE_DONE:
        Task_SendMessage(g_minimapTaskId,DISP_ROUTES_OFF);
        *step += 1;
        return 0;
    case MOVE_FINISHED:
        work->done = 1;
        return 0;
    }
    return 0;
}

/* Moves the current player's piece Player.stepsLeft squares. userData 0 = landed normally.
 * The step's range picks the phase; a phase returns non-zero to be run again at once
 * (0x80054410) */
/* 0x80054410 */
int Move_Task(short *step, MoveWork *work)
{
    int again;
    u16 phase;
    u16 sound;

    FntPrint("exec task char walk : %d\n", (u16)*step);
    do
    {
        phase = *step;
        if (phase < MOVE_CPU_START)
        {
            again = Move_Start(step,work);
        }
        else if (phase < MOVE_WALK)
        {
            again = Move_Walk(step,work);
        }
        else if (phase < MOVE_CONNECTOR_RIGHT_UP)
        {
            again = Move_EnterConnector((u16 *)step,work);
        }
        else if (phase < MOVE_CONNECTOR_LEFT_UP)
        {
            again = Move_ConnectorRightA(step,work);
        }
        else if (phase < MOVE_CONNECTOR_LEFT_DOWN)
        {
            again = Move_ConnectorLeftA(step,work);
        }
        else if (phase < MOVE_CONNECTOR_RIGHT_DOWN)
        {
            again = Move_ConnectorLeftB(step,work);
        }
        else if (phase < MOVE_END)
        {
            again = Move_ConnectorRightB(step,work);
        }
        else
        {
            again = Move_AskStop((u16 *)step,work);
        }
    } while (again != 0);
    if (work->busy != 0)
    {
        Board_FocusCurrentPlayer();
    }
    /* footsteps: the character's own sound (squeaks with the shoes), every 8 frames walking
     * (waitMode 1) or 4 (waitMode 2) */
    if (work->waitMode != 0)
    {
        sound = (g_players[g_curPlayer].shoes != 0) ? 0x36 : (char)g_players[g_curPlayer].charId + 0x28;
        work->frame++;
        if ((work->waitMode == 1 && work->frame >= 8) || (work->waitMode == 2 && work->frame >= 4))
        {
            Se_Play(sound,0x7f,0x7f,1);
            work->frame = 0;
        }
    }
    return work->done != 0;
}
