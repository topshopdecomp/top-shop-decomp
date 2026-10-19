/*
 * turnmenu.c - the turn's command menu and its screens
 *
 * The menu a human player gets at the start of its turn, and the screens it opens: the map view,
 * the status of the players and their stores, and the asset graph.
 */
#include "common.h"
#include "topshop.h"

/* Board_MapViewTask steps */
enum MapViewStep
{
    MAPVIEW_OPEN      =   0,
    MAPVIEW_ARROWS_IN =   1,  /* the arrows slide in */
    MAPVIEW_MOVE      =  10,  /* the d-pad moves the camera, circle leaves */
    MAPVIEW_CLOSE     = 100   /* the arrows slide out, the camera goes back */
};

/* Task, turn menu option 1: move the camera freely over the board with the d-pad (16 pixels a
 * frame, kept over the stage), arrows sliding in at the screen edges; circle brings it back to
 * the current player (0x8004B4C0) */
/* 0x8004B4C0 */
u32 Board_MapViewTask(u16 *step, BoardMapViewWork *work)
{
    StageDef *stage;
    PadState *pad;
    int pos[2];
    int target[2];
    int dist;
    int angle;
    int limit;
    int frame;

    switch (*step)
    {
    case MAPVIEW_OPEN:
        work->open = 1;
        work->slide = 0x20;
        *step += 1;
        break;
    case MAPVIEW_ARROWS_IN:                   /* the arrows slide in */
        work->slide -= 4;
        if (work->slide <= 0)
        {
            work->slide = 0;
            *step = MAPVIEW_MOVE;
        }
        break;
    case MAPVIEW_MOVE:
        pad = &g_playerPads[g_curPlayer];
        if (pad->pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = MAPVIEW_CLOSE;
            break;
        }
        if (pad->held & PADLleft)
        {
            g_cameraTargetX += 0x10000;
        }
        else if (pad->held & PADLright)
        {
            g_cameraTargetX -= 0x10000;
        }
        if (pad->held & PADLup)
        {
            g_cameraTargetY += 0x10000;
        }
        else if (pad->held & PADLdown)
        {
            g_cameraTargetY -= 0x10000;
        }
        stage = &g_stageDefs[g_boardStage];
        limit = (0x80 - stage->originX * 16) * 0x1000;
        if (g_cameraTargetX > limit)
        {
            g_cameraTargetX = limit;
        }
        limit = (short)(0xc0 - (stage->originX + stage->width * 5) * 16) * 0x1000;
        if (g_cameraTargetX < limit)
        {
            g_cameraTargetX = limit;
        }
        limit = (0x80 - stage->originY * 16) * 0x1000;
        if (g_cameraTargetY > limit)
        {
            g_cameraTargetY = limit;
        }
        limit = (short)(0x70 - (stage->originY + stage->floors * 5) * 16) * 0x1000;
        if (g_cameraTargetY < limit)
        {
            g_cameraTargetY = limit;
        }
        break;
    case MAPVIEW_CLOSE:                       /* the arrows slide out, the camera goes back */
        work->slide += 4;
        if (work->slide > 0x1f)
        {
            work->open = 0;
        }
        pos[0] = g_cameraTargetX;
        pos[1] = g_cameraTargetY;
        Board_FocusCurrentPlayer();
        target[0] = g_cameraTargetX;
        target[1] = g_cameraTargetY;
        angle = Math_AngleDist(pos,target,&dist);
        if (dist > 0x40000)
        {
            Math_Step(angle,0x40000,pos);
            g_cameraTargetX = pos[0];
            g_cameraTargetY = pos[1];
        }
        else
        {
            g_cameraTargetX = target[0];
            g_cameraTargetY = target[1];
            if (work->slide > 0x1f)
            {
                return 1;
            }
        }
        break;
    }
    if (work->open != 0)
    {
        /* the arrows at the four edges, animated every 8 frames */
        frame = g_frameVsync / 8 % 4;
        g_drawPos[0] = 0xa0;
        g_drawPos[1] = 0x20 - work->slide;
        Sprite_DrawAligned(g_spriteGroups[0].base + frame + 0xe6,0,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM,
                           g_ot + g_otIndex,8);
        g_drawPos[0] = 0xa0;
        g_drawPos[1] = work->slide + 0xd0;
        Sprite_DrawAligned(g_spriteGroups[0].base + frame + 0xe6,0x800000,PAL_DEFAULT,g_drawPos,ALIGN_TOP,
                           g_ot + g_otIndex,8);
        g_drawPos[1] = 0x78;
        g_drawPos[0] = 0x20 - work->slide;
        Sprite_DrawAligned(g_spriteGroups[0].base + frame + 0xe2,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT,
                           g_ot + g_otIndex,8);
        g_drawPos[1] = 0x78;
        g_drawPos[0] = work->slide + 0x120;
        Sprite_DrawAligned(g_spriteGroups[0].base + frame + 0xe2,0x400000,PAL_DEFAULT,g_drawPos,ALIGN_RIGHT,
                           g_ot + g_otIndex,8);
    }
    return 0;
}

/* AssetGraph_Task steps */
enum AssetGraphStep
{
    GRAPH_INIT      =   0,
    GRAPH_SLIDE_IN  =   1,
    GRAPH_SHOWN     =  10,  /* one more month each frame, then a button leaves */
    GRAPH_CLOSE     = 100,
    GRAPH_SLIDE_OUT = 101,
    GRAPH_DONE      = 102 
};

/* Command menu option 3: the players' assets month by month as a graph (0x800655A0) */
/* 0x800655A0 */
u32 AssetGraph_Task(u16 *step, AssetGraphWork *work)
{
    /* Moving the highlighted player with the d-pad, for 2..6 players [players - 2][player]:
     * the player panels are rows (6: 0 1 2 / 3 4 5; 5: 0 1 / 2 3 4; 4: 0 1 / 2 3);
     * (-1: no move.) Left/right always click, up/down only with 4 or more players.
     * (The original spells each player count out in switch statements.) */
    static const signed char moveLeft[5][6] = {
        {1, 0, -1, -1, -1, -1}, {2, 0, 1, -1, -1, -1}, {1, 0, 3, 2, -1, -1},
        {1, 0, 4, 2, 3, -1}, {2, 0, 1, 5, 3, 4}};
    static const signed char moveRight[5][6] = {
        {1, 0, -1, -1, -1, -1}, {1, 2, 0, -1, -1, -1}, {1, 0, 3, 2, -1, -1},
        {1, 0, 3, 4, 2, -1}, {1, 2, 0, 4, 5, 3}};
    static const signed char moveUpDown[5][6] = {
        {-1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1}, {2, 3, 0, 1, -1, -1},
        {3, 4, 0, 0, 1, -1}, {3, 4, 5, 0, 1, 2}};
    const signed char (*moves)[6];
    PlayerPanelWork *panel;
    u16 repeat;
    int players;
    int cur;

    switch (*step)
    {
    case GRAPH_INIT:
        Task_GetWork(g_playerPanelTaskId,(void **)&work->panelWork);
        work->curPlayer = g_curPlayer;
        panel = (PlayerPanelWork *)work->panelWork;
        panel->highlightCur = 1;
        panel->curPlayer = work->curPlayer;
        work->frame = 0;
        work->shown = 1;
        work->months = 0;
        work->x = 0;
        work->y = -0x88;
        TurnMenu_Unused1();
        *step += 1;
        break;
    case GRAPH_SLIDE_IN:
        work->y += 0x20;
        if (0x18 <= work->y)
        {
            work->y = 0x18;
            *step = GRAPH_SHOWN;
        }
        break;
    case GRAPH_SHOWN:             /* shown: one more month each frame, then input */
        if (work->months < (char)g_players[0].historyLen)
        {
            work->months++;
        }
        if ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) != 0)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = GRAPH_CLOSE;
            break;
        }
        if ((g_playerPads[g_curPlayer].pressed & PADRdown) != 0)
        {
            break;
        }
        repeat = g_playerPads[g_curPlayer].repeat;
        if ((repeat & PADLleft) != 0)
        {
            moves = moveLeft;
        }
        else if ((repeat & PADLright) != 0)
        {
            moves = moveRight;
        }
        else if ((repeat & (PADLup | PADLdown)) != 0)
        {
            moves = moveUpDown;
        }
        else
        {
            break;
        }
        players = g_numPlayers;
        cur = work->curPlayer;
        if (2 <= players && players <= 6 && 0 <= cur && cur < 6 && 0 <= moves[players - 2][cur])
        {
            work->curPlayer = moves[players - 2][cur];
        }
        if (moves != moveUpDown || (4 <= players && players <= 6))
        {
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case GRAPH_CLOSE:
        *step = GRAPH_SLIDE_OUT;
        break;
    case GRAPH_SLIDE_OUT:
        work->y -= 0x20;
        if (work->y <= -0x88)
        {
            work->y = -0x88;
            work->shown = 0;
            *step += 1;
        }
        break;
    case GRAPH_DONE:
        ((PlayerPanelWork *)work->panelWork)->highlightCur = 0;
        return 1;
    }
    ((PlayerPanelWork *)work->panelWork)->curPlayer = work->curPlayer;
    if (work->shown != 0)
    {
        AssetGraph_Draw(work->months,work->x,work->y,work->curPlayer);
    }
    work->frame++;
    return 0;
}

/* The graph line color: base (0-255 per component) moved by the screen tint */
static void AssetGraph_LineColor(int r, int g, int b)
{
    r += (g_tintR - 0x80) * 2;
    g += (g_tintG - 0x80) * 2;
    b += (g_tintB - 0x80) * 2;
    g_line.r = TINT_CLAMP(r);
    g_line.g = TINT_CLAMP(g);
    g_line.b = TINT_CLAMP(b);
}

/* A white line of the graph's frame */
static void AssetGraph_FrameLine(short x0, short y0, short x1, short y1)
{
    g_line.attribute = 0;
    g_line.x0 = x0;
    g_line.y0 = y0;
    g_line.x1 = x1;
    g_line.y1 = y1;
    AssetGraph_LineColor(0xff,0xff,0xff);
    GsSortLine(&g_line,g_ot + g_otIndex,8);
}

/* Screen y of an amount on a graph at y whose 104-pixel axis goes up to top (0 and debts on the axis) */
static short AssetGraph_Y(int assets, int top, u16 y)
{
    if (assets < 1)
    {
        return y + 0x6f;
    }
    return y + 0x6f - (assets * 104 / top);
}

/* Draw the graph of AssetGraph_Task: every player's assets at the end of each of the last 13
 * months (Player.assetHistory), the current player's line blinking and in front; the money axis
 * is marked every 5000 (10000 from 50000 up) up to two marks above the most anyone had, the
 * month axis with the months (the latest 12 months once the history is full) (0x80065CE0) */
/* 0x80065CE0 */
void AssetGraph_Draw(u32 months /* unused */, short x, u16 y, char current)
{
    Player *pl;
    int most;
    int top;
    int step;
    u32 markGap;                  /* 16.16 pixels between money marks */
    u32 markY;
    int value;
    int marks;
    short month;
    short label;
    short lineX;
    short i;
    u16 color;
    int k;

    /* the scale */
    most = 0;
    for (k = 0; k < g_numPlayers; k++)
    {
        for (i = 0; i < (char)g_players[k].historyLen; i++)
        {
            if (most < g_players[k].assetHistory[i])
            {
                most = g_players[k].assetHistory[i];
            }
        }
    }
    step = (most >= 50000) ? 10000 : 5000;
    top = (most / 5000 + 2) * 5000;
    markGap = (104 << 16) / (top / step);

    /* the axes */
    AssetGraph_FrameLine(x + 0x30,y + 8,x + 0x30,y + 0x70);
    AssetGraph_FrameLine(x + 0x30,y + 0x70,x + 0x120,y + 0x70);

    /* money marks and amounts, from the top down */
    marks = top / step;
    markY = marks * markGap;
    value = marks * step;
    for (; marks > 0; marks--)
    {
        AssetGraph_FrameLine(x + 0x30,y + 0x70 - (markY >> 16),x + 0x34,y + 0x70 - (markY >> 16));
        g_drawPos[0] = x + 8;
        g_drawPos[1] = y - (markY >> 16) + 0x68;
        Gfx_DrawNumber(value,g_drawPos[0],g_drawPos[1],6,6,g_spriteGroups->base + 0x35,0,g_ot + g_otIndex,8);
        markY -= markGap;
        value -= step;
    }

    /* month marks: the first month of the game is 4 (April) */
    month = 0;
    if (g_players[0].historyLen == 13)
    {
        month = (char)g_gameConfig.month;
    }
    for (i = 0, lineX = 0x30; i < 13; i++, lineX += 0x14)
    {
        AssetGraph_FrameLine(x + lineX,y + 0x6c,x + lineX,y + 0x70);
        g_drawPos[0] = x + lineX - 8;
        g_drawPos[1] = y + 0x6f;
        label = (month + 4) % 12;
        if (label == 0)
        {
            label = 12;
        }
        Gfx_DrawNumber(label,g_drawPos[0],g_drawPos[1],2,6,g_spriteGroups->base + 0x35,0,g_ot + g_otIndex,8);
        month++;
        if (month == 12)
        {
            month = 0;
        }
    }

    /* each player's line, in the player's color (the current player's blinks, drawn in front) */
    for (k = 0, pl = g_players; k < g_numPlayers; k++, pl++)
    {
        if (current == k && (g_frameVsync & 8) != 0)
        {
            continue;
        }
        g_drawPos[0] = x + 0x31;
        g_drawPos[1] = AssetGraph_Y(pl->assetHistory[0],top,y);
        for (i = 1, lineX = 0x45; i < (char)pl->historyLen; i++, lineX += 0x14)
        {
            g_line.y1 = g_drawPos[1];
            g_drawPos[2] = g_drawPos[0];
            g_drawPos[3] = g_drawPos[1];
            g_line.x0 = x + lineX;
            g_drawPos[1] = AssetGraph_Y(pl->assetHistory[i],top,y);
            color = k_playerColors[(char)pl->color];
            AssetGraph_LineColor((color & 0x1f) * 8,color >> 2 & 0xf8,color >> 7 & 0xf8);
            g_line.attribute = 0;
            g_line.x1 = g_drawPos[0];
            g_line.y0 = g_drawPos[1];
            g_drawPos[0] = g_line.x0;
            GsSortLine(&g_line,g_ot + g_otIndex,(current == k) ? 7 : 8);
        }
    }

    /* the background: a translucent box of the tint */
    g_boxFill.attribute = 0x40000000;
    g_boxFill.w = 320;
    g_boxFill.h = 0x88;
    g_boxFill.r = TINT_CLAMP((g_tintR - 0x80) * 2);
    g_boxFill.g = TINT_CLAMP((g_tintG - 0x80) * 2);
    g_boxFill.b = TINT_CLAMP((g_tintB - 0x80) * 2);
    g_boxFill.x = x;
    g_boxFill.y = y;
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,8);
}

/* One line of the status screen's store list in g_msgBuf: name, goods / slots, value. (Inline,
 * twice, in Status_Task in the original.) */
static void Status_AddStoreLine(char store)
{
    strcat(g_msgBuf,g_stores[store].type->name);
    Str_FillSpaces(21 - strlen(g_stores[store].type->name),g_msgBuf + strlen(g_msgBuf));
    Str_FormatInt(g_stores[store].numGoods,1,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"/");
    Str_FormatInt(g_stores[store].maxGoods,1,g_msgBuf + strlen(g_msgBuf));
    Str_FillSpaces(6,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"  ");
    Str_FormatInt(Store_GetValue(store),5,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf,"%K");
}

/* Status_Task steps */
enum StatusStep
{
    STATUS_INIT            =   0,
    STATUS_ROW_IN          =   1,  /* the rows drop in one after the other (a button: all at once) */
    STATUS_ROW_DROP        =   2,
    STATUS_ROWS_PLACED     =   5,
    STATUS_CHOOSE          =  10,  /* cross lists a player's stores, circle leaves */
    STATUS_LIST_START      =  20,  /* the other rows slide down out of the way */
    STATUS_ROWS_DOWN       =  21,
    STATUS_LIST_OPEN       =  25,  /* the list window opens */
    STATUS_WAIT_LIST       =  26,
    STATUS_LIST            =  30,  /* up / down a store, L1 / R1 the player, cross the prices, circle closes */
    STATUS_LIST_CLOSE      =  40,
    STATUS_WAIT_LIST_CLOSE =  41,
    STATUS_ROWS_BACK       =  45,  /* the rows come back up */
    STATUS_ROWS_UP         =  46,
    STATUS_PRICES          =  50,  /* RetailPricesInput_Task for the store picked */
    STATUS_WAIT_PRICES     =  51,
    STATUS_LEAVE           = 100,  /* the rows drop out from the last one */
    STATUS_ROW_OUT         = 101,
    STATUS_ROW_OUT_DROP    = 102,
    STATUS_DONE            = 105 
};

/* Command menu option 2: each player's money, assets, points and stores; cross on a player lists
 * their stores (name, goods, value) and moves the camera to the one picked, cross again sets
 * its retail prices (debug name "status scr") (0x80066A30) */
/* 0x80066A30 */
u32 Status_Task(short *step, StatusWork *work)
{
    StatusRow *row;
    Player *pl;
    short pri;
    short palette;
    u16 sprite;
    int count;
    int k;
    int i;

    FntPrint("exec task status scr\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch (*step)
    {
    case STATUS_INIT:
        work->shown = 1;
        work->listing = 0;
        work->mode = 0;
        work->arrived = 1;
        work->who = 0;
        work->store = 0;
        work->redraw = 0;
        Hud_CreateNameTexts();
        for (i = 0; i < g_numPlayers; i++)
        {
            work->order[i] = i;
        }
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].shown = 1;
            work->rows[i].pri = i == work->who ? 7 : 8;
            work->rows[i].x = 16;
            work->rows[i].y = -32;
        }
        work->row = 0;
        *step += 1;
        break;

    /* the rows drop in one after the other (a button: all at once) */
    case STATUS_ROW_IN:
        if ((work->row == g_numPlayers) || ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) != 0))
        {
            *step = STATUS_ROWS_PLACED;
            break;
        }
        work->slideSpeed = 0;
        work->slideY = work->rows[work->row].y << 12;
        *step += 1;
        break;
    case STATUS_ROW_DROP:
        if ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) != 0)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = STATUS_ROWS_PLACED;
            break;
        }
        work->slideSpeed += 0x4000;
        work->slideY += work->slideSpeed;
        work->rows[work->row].y = work->slideY / 4096;
        if ((short)(work->row * 32 + 24) <= work->rows[work->row].y)
        {
            work->rows[work->row].y = work->row * 32 + 24;
            work->row++;
            *step = STATUS_ROW_IN;
        }
        break;
    case STATUS_ROWS_PLACED:
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].y = 24 + i * 32;
        }
        *step = STATUS_CHOOSE;
        break;

    /* pick a player: cross lists their stores, circle leaves */
    case STATUS_CHOOSE:
        if ((g_playerPads[g_curPlayer].pressed & PADRdown) != 0)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = STATUS_LIST_START;
        }
        else if ((g_playerPads[g_curPlayer].pressed & PADRright) != 0)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = STATUS_LEAVE;
        }
        else if ((g_playerPads[g_curPlayer].repeat & PADLup) != 0)
        {
            work->who = work->who == 0 ? g_numPlayers - 1 : work->who - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if ((g_playerPads[g_curPlayer].repeat & PADLdown) != 0)
        {
            work->who = work->who == g_numPlayers - 1 ? 0 : work->who + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        if (g_boardRules.enable[RULE_DEBUG_POINTS] != 0)
        {
            /* debug rule: L1 / R1 change the current player's points */
            pl = &g_players[g_curPlayer];
            if ((g_playerPads[g_curPlayer].repeat & PADL1) != 0)
            {
                if (pl->points < 99)
                {
                    pl->points++;
                }
            }
            else if (((g_playerPads[g_curPlayer].repeat & PADR1) != 0) && (pl->points != 0))
            {
                pl->points--;
            }
        }
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].pri = i == work->who ? 7 : 8;
        }
        break;

    /* the other rows slide down out of the way, the list window opens */
    case STATUS_LIST_START:
        work->rows[work->who].pri = 7;
        work->panelX = 16;
        work->targetY = 24;
        *step += 1;
        break;
    case STATUS_ROWS_DOWN:
        work->targetY += 0x40;
        if (0xbf < work->targetY)
        {
            work->targetY = 0xc0;
            for (i = 0; i < g_numPlayers; i++)
            {
                work->rows[i].y = work->targetY;
            }
            *step = STATUS_LIST_OPEN;
        }
        for (i = 0; i < g_numPlayers; i++)
        {
            if (work->rows[i].y <= work->targetY)
            {
                work->rows[i].y = work->targetY;
            }
        }
        break;
    case STATUS_LIST_OPEN:
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].shown = 0;
            work->rows[i].pri = 8;
        }
        work->rows[work->who].shown = 1;
        work->mode = Stores_ListByOwner(work->who) == 0 ? 0 : 2;
        Stores_ListByOwner(work->who);
        work->redraw = 2;
        work->listPos = 0;
        work->listing = 1;
        work->id = TextWin_Create(20,16,38,5);
        TextWin_Open(work->id);
        TextWin_SetFocus(work->id,1);
        *step += 1;
        break;
    case STATUS_WAIT_LIST:
        if (g_textWins[work->id].state == TEXTWIN_SHOWN)
        {
            *step = STATUS_LIST;
        }
        break;

    /* the store list: up / down pick a store (scrolling past 4), L1 / R1 the previous / next
     * player, cross sets the prices, circle closes */
    case STATUS_LIST:
        if ((g_playerPads[g_curPlayer].pressed & PADRdown) != 0)
        {
            if (Stores_ListByOwner(work->who) != 0)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = STATUS_PRICES;
            }
            break;
        }
        if ((g_playerPads[g_curPlayer].pressed & PADRright) != 0)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = STATUS_LIST_CLOSE;
            break;
        }
        if ((g_playerPads[g_curPlayer].repeat & (PADL1 | PADR1)) != 0)
        {
            work->rows[work->who].shown = 0;
            if ((g_playerPads[g_curPlayer].repeat & PADL1) != 0)
            {
                work->who = (work->who == 0 ? g_numPlayers : work->who) - 1;
            }
            else
            {
                work->who = work->who == g_numPlayers - 1 ? 0 : work->who + 1;
            }
            work->rows[work->who].shown = 1;
            Stores_ListByOwner(work->who);
            work->redraw = 2;
            work->mode = g_storeListCount == 0 ? 0 : 2;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            break;
        }
        if ((g_playerPads[g_curPlayer].repeat & (PADLup | PADLdown)) == 0)
        {
            break;
        }
        if (g_storeListCount == 0)
        {
            work->mode = 0;
            break;
        }
        if ((g_playerPads[g_curPlayer].repeat & PADLup) != 0)
        {
            if (g_storeListCount < 5)
            {
                TextWin_CursorUp(work->id);
            }
            else if (g_textWins[work->id].menuCursor < 1)
            {
                work->listPos--;
                if (work->listPos < 0)
                {
                    work->listPos += g_storeListCount;
                }
                work->redraw = 1;
            }
            else
            {
                g_textWins[work->id].menuCursor--;
            }
        }
        else
        {
            if (g_storeListCount < 5)
            {
                TextWin_CursorDown(work->id);
            }
            else if (g_textWins[work->id].menuCursor < 3)
            {
                g_textWins[work->id].menuCursor++;
            }
            else
            {
                work->listPos++;
                if (g_storeListCount <= work->listPos)
                {
                    work->listPos -= g_storeListCount;
                }
                work->redraw = 1;
            }
        }
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        work->mode = 2;
        break;

    /* close the list, the rows come back up */
    case STATUS_LIST_CLOSE:
        work->mode = 1;
        TextWin_Close(work->id);
        *step += 1;
        break;
    case STATUS_WAIT_LIST_CLOSE:
        if (g_textWins[work->id].state == TEXTWIN_FREEING)
        {
            work->listing = 0;
            *step = STATUS_ROWS_BACK;
        }
        break;
    case STATUS_ROWS_BACK:
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].shown = 1;
        }
        work->panelX = 16;
        work->targetY = 0xc0;
        *step += 1;
        break;
    case STATUS_ROWS_UP:
        work->targetY -= 0x40;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].y = work->targetY;
            if (work->rows[i].y <= 24 + i * 32)
            {
                work->rows[i].y = 24 + i * 32;
            }
        }
        if (work->targetY <= 24)
        {
            work->targetY = 24;
            *step = STATUS_CHOOSE;
        }
        break;

    /* the retail prices of the store picked */
    case STATUS_PRICES:
        TextWin_SetFocus(work->id,0);
        g_uiTaskWork[6][1] = 0x37;
        g_uiTaskWork[6][2] = 0x20;
        g_uiTaskWork[6][3] = work->store;
        Task_Start(g_uiTaskIds[6],RetailPricesInput_Task);
        *step += 1;
        break;
    case STATUS_WAIT_PRICES:
        if (Task_IsDone(g_uiTaskIds[6]) != 0)
        {
            TextWin_SetFocus(work->id,1);
            *step = STATUS_LIST;
        }
        break;

    /* leave: the rows drop out from the last one */
    case STATUS_LEAVE:
        work->row = g_numPlayers - 1;
        *step += 1;
        break;
    case STATUS_ROW_OUT:
        if ((work->row < 0) || ((g_playerPads[g_curPlayer].pressed & PADRdown) != 0))
        {
            *step = STATUS_DONE;
            break;
        }
        work->rows[work->row].pri = 7;
        work->slideSpeed = 0;
        work->slideY = work->rows[work->row].y << 12;
        *step += 1;
        break;
    case STATUS_ROW_OUT_DROP:
        if ((g_playerPads[g_curPlayer].pressed & (PADRright | PADRdown)) != 0)
        {
            *step = STATUS_DONE;
            break;
        }
        work->slideSpeed += 0x4000;
        work->slideY += work->slideSpeed;
        work->rows[work->row].y = work->slideY / 4096;
        if (0xff < work->rows[work->row].y)
        {
            work->rows[work->row].y = 0x100;
            work->row--;
            *step = STATUS_ROW_OUT;
        }
        break;
    case STATUS_DONE:
        for (i = 0; i < g_numPlayers; i++)
        {
            work->rows[i].y = 0x100;
        }
        Hud_FreeNameTexts();
        return 1;
    }

    /* the list window's text */
    if ((work->redraw != 0) && (work->listing != 0))
    {
        if (g_storeListCount == 0)
        {
            TextWin_SetArrowUp(work->id,0);
            TextWin_SetArrowDown(work->id,0);
            memcpy(g_msgBuf, "%K%K%CThere are no stores.%E", 29);
        }
        else if (g_storeListCount < 5)
        {
            TextWin_SetArrowUp(work->id,0);
            TextWin_SetArrowDown(work->id,0);
            memcpy(g_msgBuf, "%LStore Name    # of Items  Store Assets%K", 43);
            for (i = 0; i < g_storeListCount; i++)
            {
                Status_AddStoreLine(g_storeList[i]);
            }
        }
        else
        {
            TextWin_SetArrowUp(work->id,1);
            TextWin_SetArrowDown(work->id,1);
            memcpy(g_msgBuf, "%LStore Name    # of Items  Store Assets%K", 43);
            for (i = 0; i < 4; i++)
            {
                k = (char)(work->listPos + i);
                if (g_storeListCount <= k)
                {
                    k -= g_storeListCount;
                }
                Status_AddStoreLine(g_storeList[(char)k]);
            }
        }
        TextWin_ClearText(work->id);
        TextWin_AddText(work->id,g_msgBuf);
        if (work->redraw == 2)
        {
            TextWin_SetMenu(work->id,1,g_storeListCount < 4 ? g_storeListCount : 4,0);
        }
        work->redraw = 0;
    }

    /* the player rows: piece, rank, name, money, assets, points, stores */
    if (work->shown != 0)
    {
        for (i = g_numPlayers - 1; i >= 0; i--)
        {
            row = &work->rows[i];
            if (row->shown == 0)
            {
                continue;
            }
            pri = row->pri;
            if ((pri == 8) && (i == work->who))
            {
                pri = 7;
            }
            g_drawPos[0] = row->x;
            g_drawPos[1] = row->y + 32;
            if (g_players[i].out == 0)
            {
                /* (the original passes what is left in a register as the base; Piece_GetSprite only
                 * uses it for poses the pieces do not have) */
                sprite = Piece_GetSprite(i,g_players[(char)i].pieceSprite,-100);
                palette = (char)i;
            }
            else
            {
                palette = -101;
                sprite = g_spriteGroups[3].base + i * 5;
            }
            Sprite_DrawAligned(sprite,0,palette,g_drawPos,ALIGN_LEFT | ALIGN_BOTTOM,g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 32;
            g_drawPos[1] = row->y + 1;
            Sprite_DrawAligned(g_players[(char)i].rank + g_spriteGroups[0].base + 0x75,0,PAL_DEFAULT,g_drawPos,
                               ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 32;
            g_drawPos[1] = row->y + 14;
            Text_Draw(g_playerNameTexts[(char)i],g_drawPos[0],g_drawPos[1],g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 33;
            g_drawPos[1] = row->y + 15;
            Text_DrawClut(g_playerNameTexts[(char)i],g_drawPos[0],g_drawPos[1],g_ot + g_otIndex,pri,0,0x1fe);
            g_drawPos[0] = row->x + 0x88;
            g_drawPos[1] = row->y + 1;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x84,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0xa0;
            g_drawPos[1] = row->y + 1;
            Gfx_DrawNumber(g_players[(char)i].money,g_drawPos[0],g_drawPos[1],6,8,g_spriteGroups[0].base + 0x4b,
                           0,g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0x88;
            g_drawPos[1] = row->y + 15;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x87,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0xa0;
            g_drawPos[1] = row->y + 15;
            Gfx_DrawNumber(g_players[(char)i].assets,g_drawPos[0],g_drawPos[1],6,8,g_spriteGroups[0].base + 0x4b,
                           0,g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0xe8;
            g_drawPos[1] = row->y + 1;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x85,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0x108;
            g_drawPos[1] = row->y + 1;
            Gfx_DrawNumber(g_players[(char)i].points,g_drawPos[0],g_drawPos[1],2,8,g_spriteGroups[0].base + 0x4b,
                           0,g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0xe8;
            g_drawPos[1] = row->y + 15;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x86,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x + 0x108;
            g_drawPos[1] = row->y + 15;
            /* (this also leaves the player's stores in g_storeList) */
            count = Stores_ListByOwner((char)i);
            Gfx_DrawNumber(count,g_drawPos[0],g_drawPos[1],2,8,g_spriteGroups[0].base + 0x4b,0,
                           g_ot + g_otIndex,pri);
            g_drawPos[0] = row->x;
            g_drawPos[1] = row->y;
            g_drawPos[2] = 0x120;
            g_drawPos[3] = 0x20;
            if (i == work->who)
            {
                Frame_DrawPlain(g_drawPos,g_ot + g_otIndex,pri);
            }
            Frame_DrawPanel(g_drawPos,i,g_ot + g_otIndex,pri);
        }
    }
    if ((work->mode == 2) && (work->arrived != 0))
    {
        Menu_DrawFrameCursor(0x9f,0x6a);
    }
    /* the store picked, for the camera */
    if ((work->listing != 0) && (g_storeListCount != 0))
    {
        k = (work->listPos + g_textWins[work->id].menuCursor) % g_storeListCount;
        work->whoIndex = k;
        work->store = g_storeList[(char)k];
        work->storeType = g_stores[work->store].storeType;
        work->x = g_stores[work->store].x;
        work->floor = g_stores[work->store].floor;
    }
    work->arrived = Camera_MoveTo(work->mode,CAMERA_CELL_X(work->x,0x78),CAMERA_CELL_Y(work->floor,0x6a));
    return 0;
}

/* TurnMenu_Task steps */
enum TurnMenuStep
{
    TMENU_INIT     =   0,  /* the ring of command icons above the player */
    TMENU_DROP     =   1,  /* drop down onto the player */
    TMENU_SPREAD   =   2,
    TMENU_TURN_IN  =   3,
    TMENU_READY    =   4,
    TMENU_CHOOSE   =  10,  /* left / right turn the ring, cross picks */
    TMENU_TURNING  =  11,
    TMENU_CPU      =  20,  /* a CPU player rolls after 15 frames */
    TMENU_CPU_WAIT =  21,
    TMENU_CLOSE    = 100,
    TMENU_FLY_OFF  = 101   /* then g_turnMenuChoice */
};

/* Turn command menu; choice in g_turnMenuChoice: 0 roll dice, 1 Board_MapViewTask, 2 Status_Task, 3
 * AssetGraph_Task, 4 TurnOptions_Task (options / save). (0x8006ACD4) */
/* 0x8006ACD4 */
int TurnMenu_Task(short *step, TurnMenuWork *work)
{
    short spread;
    short angle;
    short dx;
    short dy;
    short sprite;
    int targetY;
    u8 choice;
    int i;

    for (;;)
    {
        switch (*step)
        {
        case TMENU_INIT:            /* the ring of command icons above the player */
            work->items = 5;
            for (i = 0; i < work->items; i++)
            {
                work->itemAngle[i] = -((i << 0xc) / work->items);
            }
            work->turnMenuChoice = TURNMENU_ROLL;
            work->active = 1;
            work->spread = 0;
            work->radius = 0x28;
            work->angle = -work->itemAngle[work->turnMenuChoice];
            work->y = -0x40000;
            work->dy = 0;
            work->x = g_cameraX + g_players[g_curPlayer].screenX * 0x1000;
            *step += 1;
            break;
        case TMENU_DROP:            /* drop down onto the player */
            work->dy += 0x1800;
            work->y += work->dy;
            targetY = g_cameraY + (g_players[g_curPlayer].screenY - 0x18) * 0x1000;
            if (targetY <= work->y)
            {
                work->y = targetY;
                *step = TMENU_READY;
            }
            break;
        case TMENU_SPREAD:
            spread = work->spread - 0xb;
            work->spread = spread;
            if (spread <= 0x88)
            {
                work->spread = 0x88;
                *step += 1;
            }
            break;
        case TMENU_TURN_IN:
            if (0 <= work->angle && work->angle < work->spread)
            {
                work->angle = 0;
                work->spread = 0;
                *step += 1;
            }
            break;
        case TMENU_READY:
            if (g_players[g_curPlayer].isCpu != 0)
            {
                *step = TMENU_CPU;
            }
            else
            {
                *step = TMENU_CHOOSE;
            }
            break;
        case TMENU_CHOOSE:          /* the player chooses: left / right turn the ring */
            if ((g_playerPads[g_curPlayer].pressed & PADRdown) != 0)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = TMENU_CLOSE;
                break;
            }
            if ((g_playerPads[g_curPlayer].pressed & PADRright) != 0 && work->turnMenuChoice != TURNMENU_ROLL)
            {
                work->turnMenuChoice = TURNMENU_ROLL;
                work->angle = -work->itemAngle[0];
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
                break;
            }
            if ((g_playerPads[g_curPlayer].held & PADLright) != 0)
            {
                if (work->turnMenuChoice == work->items - 1)
                {
                    work->turnMenuChoice = TURNMENU_ROLL;
                }
                else
                {
                    work->turnMenuChoice++;
                }
                work->turned = 0;
                work->spread = 0x88;
            }
            else if ((g_playerPads[g_curPlayer].held & PADLleft) != 0)
            {
                choice = work->turnMenuChoice;
                if (choice == 0)
                {
                    choice = work->items;
                }
                work->turnMenuChoice = choice - 1;
                work->turned = 0;
                work->spread = -0x88;
            }
            else
            {
                break;
            }
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            *step += 1;
            continue;                 /* start turning in the same frame */
        case TMENU_TURNING:
            spread = work->spread;
            if (spread < 0)
            {
                spread = -spread;
            }
            work->turned += spread;
            work->angle += work->spread;
            if (0x333 <= work->turned)
            {
                work->spread = 0;
                work->angle = -work->itemAngle[work->turnMenuChoice];
                *step = TMENU_CHOOSE;
            }
            break;
        case TMENU_CPU:           /* a CPU player: roll after 15 frames */
            work->delay = 0xf;
            *step += 1;
            break;
        case TMENU_CPU_WAIT:
            work->delay--;
            if (work->delay == 0)
            {
                work->turnMenuChoice = TURNMENU_ROLL;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = TMENU_CLOSE;
            }
            break;
        case TMENU_CLOSE:
            work->dy = 0;
            *step += 1;
            break;
        case TMENU_FLY_OFF:
            work->dy += 0x1800;
            work->y += work->dy;
            if (0x12ffff < work->y)
            {
                g_turnMenuChoice = work->turnMenuChoice;
                return 1;
            }
            break;
        }
        break;
    }
    if (work->active != 0)
    {
        g_frontOtUsed = 1;
        for (i = 0; i < work->items; i++)
        {
            angle = (work->angle + work->itemAngle[i]);
            dx = (work->radius * -rsin(angle) / 4096);
            dy = (work->radius * rcos(angle) / 4096);
            g_drawPos[0] = (dx * 0x12 / 10) + (work->x / 4096);
            g_drawPos[1] = dy + (work->y / 4096) - 8;
            g_drawPos[2] = 0x30;
            g_drawPos[3] = 0x20;
            if (i == work->turnMenuChoice)
            {
                sprite = g_spriteGroups->base + 0x109;
            }
            else
            {
                sprite = g_spriteGroups->base + 0x6c;
            }
            Sprite_DrawAligned(sprite + i,0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,1);
        }
    }
    return 0;
}

/* Empty (0x8006BB24) */
/* 0x8006BB24 */
void TurnMenu_Unused1(void)
{
}

/* Empty (0x8006BC34) */
/* 0x8006BC34 */
void TurnMenu_Unused2(void)
{
}
