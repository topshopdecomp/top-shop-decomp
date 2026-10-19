/*
 * hud.c - the board's display tasks
 *
 * What the board shows while a game is played: the name of the square under the piece, the steps
 * left, the calendar and the goal, the players' panels, the minimap and the points window. Each
 * is a task the board shows or hides with messages (DISP_*).
 */
#include "common.h"
#include "topshop.h"

/* The label of the square the current player is on: "Elevator", "Tenant Wanted", "A1 Bank" or
 * the store's name in its owner's color (other squares: g_msgBuf as it is) */
static void SquareInfo_SetLabel(SquareInfoWork *work)
{
    work->x = g_players[g_curPlayer].x;
    work->floor = g_players[g_curPlayer].floor;
    work->cell = Board_GetCell(work->x,work->floor);
    work->palette = 6;
    switch (CELL_TYPE((u16)work->cell))
    {
    case CELL_ELEVATOR:
        strcpy(g_msgBuf,"Elevator ");
        break;
    case CELL_STORE:
        if (g_stores[(u16)work->cell].owner == -1)
        {
            strcpy(g_msgBuf,"Tenant Wanted ");
        }
        else
        {
            work->palette = g_stores[(u16)work->cell].owner;
            strcpy(g_msgBuf,g_stores[work->cell].type->name);
        }
        break;
    case CELL_BANK:
        strcpy(g_msgBuf,"A1 Bank ");
        break;
    }
    if (work->text != -1)
    {
        Text_Free(work->text);
    }
    work->text = Text_Create(g_msgBuf);
    work->width = Text_GetWidth(work->text);
}

/* SquareInfo_Task steps */
enum SquareInfoStep
{
    SQINFO_INIT       =   0,
    SQINFO_REFRESH    =  10,  /* DISP_REFRESH: slide up, then the new label */
    SQINFO_LABEL      =  20,
    SQINFO_SLIDE_DOWN =  30,  /* DISP_SHOW */
    SQINFO_IDLE       =  40,
    SQINFO_HIDE       =  50,  /* DISP_HIDE: slide up */
    SQINFO_SHOW_NOW   =  60,  /* DISP_SHOW_NOW */
    SQINFO_END        = 100   /* DISP_END: slide up and end */
};

/* Display task: the name of the square the player is on, on a panel at the top. Messages: 0 hide
 * and end, 1 show, 2 hide, 3 hide then show the current square, 0x10 show the current square
 * at once (0x80056B0C) */
/* 0x80056B0C */
u32 SquareInfo_Task(u16 *step, SquareInfoWork *work)
{
    short x;
    int msg[2];

    if (*step != SQINFO_INIT && Task_ReceiveMessage(-3,msg) == 0)
    {
        switch (msg[0])
        {
        case DISP_END:
            *step = SQINFO_END;
            break;
        case DISP_SHOW:
            *step = SQINFO_SLIDE_DOWN;
            break;
        case DISP_HIDE:
            *step = SQINFO_HIDE;
            break;
        case DISP_REFRESH:
            *step = SQINFO_REFRESH;
            break;
        case DISP_SHOW_NOW:
            *step = SQINFO_SHOW_NOW;
            break;
        }
    }
    switch (*step)
    {
    case SQINFO_INIT:
        work->panelX = 0x10;
        work->panelY = -0x18;
        work->text = -1;
        work->shown = 0;
        work->width = 0;
        work->palette = 0;
        *step = SQINFO_IDLE;
        break;
    case SQINFO_REFRESH:                      /* slide up, then the new label */
        work->panelY -= 8;
        if (work->panelY <= -0x18)
        {
            work->panelY = -0x18;
            work->shown = 0;
            if (work->text != -1)
            {
                Text_Free(work->text);
            }
            *step = SQINFO_LABEL;
        }
        break;
    case SQINFO_LABEL:
        SquareInfo_SetLabel(work);
        *step = SQINFO_SLIDE_DOWN;
        break;
    case SQINFO_SLIDE_DOWN:
        work->shown = 1;
        work->panelY += 8;
        if (work->panelY >= 0x10)
        {
            work->panelY = 0x10;
            *step = SQINFO_IDLE;
        }
        break;
    case SQINFO_HIDE:                       /* slide up */
        work->panelY -= 8;
        if (work->panelY <= -0x18)
        {
            work->panelY = -0x18;
            work->shown = 0;
            *step = SQINFO_IDLE;
        }
        break;
    case SQINFO_SHOW_NOW:
        SquareInfo_SetLabel(work);
        work->shown = 1;
        work->panelY = 0x10;
        *step = SQINFO_IDLE;
        break;
    case SQINFO_END:                          /* slide up and end */
        work->panelY -= 8;
        if (work->panelY <= -0x18)
        {
            if (work->text != -1)
            {
                Text_Free(work->text);
            }
            return 1;
        }
        break;
    }
    if (work->shown != 0)
    {
        /* the label centered on the panel, with a black outline */
        x = work->panelX + (160 - work->width * 7) / 2;
        Text_Draw(work->text,x,work->panelY + 3,g_ot + g_otIndex,2);
        Text_DrawClut(work->text,x - 1,work->panelY + 3,g_ot + g_otIndex,2,0,0x1fe);
        Text_DrawClut(work->text,x + 1,work->panelY + 3,g_ot + g_otIndex,2,0,0x1fe);
        Text_DrawClut(work->text,x,work->panelY + 4,g_ot + g_otIndex,2,0,0x1fe);
        Text_DrawClut(work->text,x,work->panelY + 2,g_ot + g_otIndex,2,0,0x1fe);
        g_drawPos[0] = work->panelX;
        g_drawPos[1] = work->panelY;
        g_drawPos[3] = 0x14;
        g_drawPos[2] = 0xa0;
        Frame_DrawPanel(g_drawPos,work->palette,g_ot + g_otIndex,8);
    }
    return 0;
}

/* "<n> steps left." into g_msgBuf, for the current player */
static void StepsLeft_Format(StepsLeftWork *work)
{
    work->stepsLeft = (char)g_players[g_curPlayer].stepsLeft;
    strcpy(g_msgBuf,"%C");
    Str_FormatInt((char)g_players[g_curPlayer].stepsLeft,1,g_msgBuf + strlen(g_msgBuf));
    strcat(g_msgBuf," steps left.");
}

/* StepsLeft_Task steps */
enum StepsLeftStep
{
    STEPSLEFT_OPEN  =   0,
    STEPSLEFT_SHOWN =   1,  /* follows the steps left */
    STEPSLEFT_CLOSE = 100   /* message 0 */
};

/* Window "n steps left." while the current player moves; message 0 closes it (0x80057380) */
/* 0x80057380 */
u32 StepsLeft_Task(u16 *step, StepsLeftWork *work)
{
    int msg[2];

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = STEPSLEFT_CLOSE;
    }
    switch (*step)
    {
    case STEPSLEFT_OPEN:
        StepsLeft_Format(work);
        work->win = TextWin_Create(TEXTWIN_AUTO,0x90,0xc,1);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        *step = STEPSLEFT_SHOWN;
        break;
    case STEPSLEFT_SHOWN:
        if ((char)g_players[g_curPlayer].stepsLeft != work->stepsLeft)
        {
            StepsLeft_Format(work);
            TextWin_ClearText(work->win);
            TextWin_AddText(work->win,g_msgBuf);
        }
        break;
    case STEPSLEFT_CLOSE:
        TextWin_Close(work->win);
        return 1;
    }
    return 0;
}

/* Minimap_Task steps */
enum MinimapStep
{
    MMAP_INIT           =   0,
    MMAP_IDLE           =  10,
    MMAP_SLIDE_IN       =  20,  /* DISP_SHOW */
    MMAP_SLIDING_IN     =  21,
    MMAP_SLIDE_OUT      =  30,  /* DISP_HIDE */
    MMAP_MOVE_UP        =  40,  /* DISP_MOVE_UP */
    MMAP_MOVE_DOWN      =  50,  /* DISP_MOVE_DOWN */
    MMAP_SHOW_NOW       =  60,  /* DISP_SHOW_NOW */
    MMAP_SLIDE_IN_LOW   =  70,  /* DISP_SHOW_LOW: slide in lower down */
    MMAP_SLIDING_IN_LOW =  71,
    MMAP_END            = 100   /* DISP_END */
};

/* Display task: the board map in the lower left corner (Minimap_Draw: the players and the camera
 * frame, and while showRoutes the ends of the current player's routes), sliding in and out from
 * the left and up and down between two places on the DISP_* messages (0x800686CC) */
/* 0x800686CC */
u32 Minimap_Task(short *step, MinimapWork *work)
{
    int msg[2];

    switch(*step)
    {
    case MMAP_INIT:
        work->shown = 0;
        work->showRoutes = 0;
        work->width = g_stageDefs[g_boardStage].width * 8 + 0x10;
        work->y = 0xa7;
        work->height = g_stageDefs[g_boardStage].floors * 8 + 8;
        work->x = -work->width;
        *step = MMAP_IDLE;
        break;
    case MMAP_SLIDE_IN:
        work->y = 0xa7;
        work->showRoutes = 0;
        work->shown = 1;
        *step += 1;
        break;
    case MMAP_SLIDING_IN:
    case MMAP_SLIDING_IN_LOW:
        work->x += 0x10;
        if (work->x >= 0x10)
        {
            work->x = 0x10;
            *step = MMAP_IDLE;
        }
        break;
    case MMAP_SLIDE_OUT:
        work->showRoutes = 0;
        work->x -= 0x10;
        if (work->x <= -work->width)
        {
            work->x = -work->width;
            work->shown = 0;
            *step = MMAP_IDLE;
        }
        break;
    case MMAP_MOVE_UP:
        work->showRoutes = 0;
        work->y -= 0x10;
        if (work->y < 0xa8)
        {
            work->y = 0xa7;
            *step = MMAP_IDLE;
        }
        break;
    case MMAP_MOVE_DOWN:
        work->showRoutes = 0;
        work->x = 0x10;
        work->y += 0x10;
        if (work->y >= 0xdf)
        {
            work->y = 0xdf;
            *step = MMAP_IDLE;
        }
        break;
    case MMAP_SHOW_NOW:           /* in place */
        work->shown = 1;
        work->x = 0x10;
        work->y = 0xa7;
        work->showRoutes = 0;
        *step = MMAP_IDLE;
        break;
    case MMAP_SLIDE_IN_LOW:       /* slide in lower down */
        work->y = 0xdf;
        work->showRoutes = 0;
        work->shown = 1;
        *step += 1;
        break;
    case MMAP_END:
        work->showRoutes = 0;
        return 1;
    }
    if (Task_ReceiveMessage(-3,msg) == 0)
    {
        switch(msg[0])
        {
        case DISP_END:
            *step = MMAP_END;
            break;
        case DISP_SHOW:
            *step = MMAP_SLIDE_IN;
            break;
        case DISP_HIDE:
            *step = MMAP_SLIDE_OUT;
            break;
        case DISP_SHOW_LOW:
            *step = MMAP_SLIDE_IN_LOW;
            break;
        case DISP_MOVE_DOWN:
            *step = MMAP_MOVE_DOWN;
            break;
        case DISP_MOVE_UP:
            *step = MMAP_MOVE_UP;
            break;
        case DISP_ROUTES_ON:
            work->showRoutes = 1;
            break;
        case DISP_ROUTES_OFF:
            work->showRoutes = 0;
            break;
        case DISP_SHOW_NOW:
            *step = MMAP_SHOW_NOW;
            break;
        }
    }
    if (work->shown != 0)
    {
        Minimap_Draw(work->x,work->y - work->height,0,0,
                     MINIMAP_PLAYERS | MINIMAP_CAMERA | (work->showRoutes ? MINIMAP_ROUTE_ENDS : 0),0);
    }
    return 0;
}

/* Draw the board scaled into a rectangle (w, h 0: one minimap sprite per square). flags: low nibble
 * how stores show (0 their owner's color, 1 vacant but store highlight blinking in its owner's
 * color, 2 vacant but the stores of player highlight blinking), MINIMAP_PLAYERS,
 * MINIMAP_CAMERA (the camera frame), MINIMAP_ROUTE_ENDS (of the current player's routes)
 * (0x800689EC) */
/* 0x800689EC */
void Minimap_Draw(short x, short y, short w, short h, u16 flags, u16 highlight)
{
    StageDef *stage;
    Player *pl;
    u16 cellW;
    u16 cellH;
    short pos[2];
    short rect[4];
    short col;
    short row;
    short cell;
    u32 attr;
    int ownerColor;
    int c;
    u16 sprite;
    int i;

    stage = &g_stageDefs[g_boardStage];
    cellW = w == 0 ? g_sprites[g_spriteGroups[0].base + 0x7d].w : w / (stage->width + 2);
    cellH = h == 0 ? g_sprites[g_spriteGroups[0].base + 0x7d].h : h / (stage->floors + 1);

    if ((flags & MINIMAP_CAMERA) != 0)
    {
        /* the camera frame: 2 x 1.5 squares around the camera position (80 pixels per square) */
        g_rect[2] = cellW * 2;
        g_rect[3] = (short)cellH * 3 / 2;
        g_rect[0] = (short)((-stage->originX * 16 - g_cameraX / 4096) * cellW) / 80 + x + cellW * 3;
        g_rect[1] = (short)((-stage->originY * 16 - g_cameraY / 4096) * cellH) / 80 + y + g_rect[3] +
                    (short)cellH / 2;
        c = (g_tintR - 0x80) * 2 + 0xff;
        g_line.r = c < 0 ? 0 : (0xff < c ? 0xff : c);
        c = (g_tintG - 0x80) * 2 + 0xff;
        g_line.g = c < 0 ? 0 : (0xff < c ? 0xff : c);
        c = (g_tintB - 0x80) * 2 + 0xff;
        g_line.b = c < 0 ? 0 : (0xff < c ? 0xff : c);
        g_line.attribute = 0;
        Debug_DrawBox(g_rect,0,g_ot + g_otIndex,9);
    }
    if ((flags & MINIMAP_ROUTE_ENDS) != 0)
    {
        /* the ends of the current player's routes, blinking */
        pl = &g_players[g_curPlayer];
        for (i = 0; i < 4; i++)
        {
            if ((pl->routeEnds[i * 2] != 0xff) && (pl->routeEnds[i * 2 + 1] != 0xff))
            {
                pos[0] = cellW + x + (char)pl->routeEnds[i * 2] * cellW;
                pos[1] = y + (char)pl->routeEnds[i * 2 + 1] * cellH + (short)cellH / 2;
                if ((g_frameVsync & 8) != 0)
                {
                    Sprite_Draw(g_spriteGroups[0].base + 0x74,0,PAL_DEFAULT,pos,g_ot + g_otIndex,9);
                }
            }
        }
    }
    if ((flags & MINIMAP_PLAYERS) != 0)
    {
        /* the players' pieces, facing their direction (blinking when standing); the current player
         * larger, on top. (attr keeps the previous one for another direction, as in the original) */
        attr = 0;
        pl = &g_players[g_curPlayer];
        if (pl->out == 0)
        {
            pos[0] = cellW + x + pl->x * cellW;
            pos[1] = y + pl->floor * cellH + (short)cellH / 2;
            sprite = g_curPlayer * 2 + ((g_frameVsync & 8) != 0 ? 1 : 0);
            switch ((char)pl->dirX)
            {
            case DIR_NONE:
                attr = (g_frameVsync & 0x10) == 0 ? GFX_FLIP_X : 0;
                break;
            case DIR_LEFT:
                attr = 0;
                break;
            case DIR_RIGHT:
                attr = GFX_FLIP_X;
                break;
            }
            rect[0] = pos[0] + 3;
            rect[1] = pos[1] + 3;
            rect[2] = 12;
            rect[3] = 12;
            Sprite_DrawStretched(g_spriteGroups[0].base + sprite + 0xc9,attr,PAL_DEFAULT,rect,0,g_ot + g_otIndex,9);
        }
        for (i = 0; i < g_numPlayers; i++)
        {
            pl = &g_players[(short)i];
            if (pl->out != 0)
            {
                continue;
            }
            pos[0] = cellW + x + pl->x * cellW;
            pos[1] = y + pl->floor * cellH + (short)cellH / 2;
            sprite = (short)i * 2 + ((g_frameVsync & 8) != 0 ? 1 : 0);
            switch ((char)pl->dirX)
            {
            case DIR_NONE:
                attr = (g_frameVsync & 0x10) == 0 ? GFX_FLIP_X : 0;
                break;
            case DIR_LEFT:
                attr = 0;
                break;
            case DIR_RIGHT:
                attr = GFX_FLIP_X;
                break;
            }
            if ((u16)i != g_curPlayer)
            {
                Sprite_Draw(g_spriteGroups[0].base + sprite + 0xc9,attr,PAL_DEFAULT,pos,g_ot + g_otIndex,9);
            }
        }
    }

    /* the squares */
    ownerColor = 0;                    /* (kept from the previous store, as in the original) */
    for (row = 0; row < g_stageDefs[g_boardStage].floors; row++)
    {
        for (col = 0; col < g_stageDefs[g_boardStage].width; col++)
        {
            pos[0] = cellW + x + col * cellW;
            pos[1] = y + row * cellH + (short)cellH / 2;
            cell = Board_GetCell(col,row);
            if (CELL_TYPE(cell) == CELL_ELEVATOR)
            {
                sprite = 0xdc;
            }
            else if (CELL_TYPE(cell) == CELL_BANK)
            {
                sprite = 0xe1;
            }
            else if ((cell & 0xfff0) == CONNECTOR_STAIRS)
            {
                sprite = 0xdd;
            }
            else if ((cell & 0xfff0) == CONNECTOR_STAIRS_FLIPPED)
            {
                sprite = 0xde;
            }
            else if ((cell & 0xfff0) == CONNECTOR_STAIRS_CROSSED)
            {
                sprite = 0xdf;
            }
            else if (CELL_TYPE(cell) == CELL_LANDING)
            {
                sprite = 0xe0;
            }
            else if (CELL_TYPE(cell) == CELL_STORE)
            {
                /* a store: 0xd5 vacant, 0xd6 + owner */
                if (g_stores[cell].owner == -1)
                {
                    ownerColor = 0;
                }
                else
                {
                    switch (flags & 0xf)
                    {
                    case MINIMAP_OWNERS:
                        ownerColor = g_stores[cell].owner + 1;
                        break;
                    case MINIMAP_BLINK_STORE:
                        ownerColor = (highlight == cell) && ((g_frameVsync & 7) == 0) ? g_stores[cell].owner + 1 : 0;
                        break;
                    case MINIMAP_BLINK_PLAYER:
                        ownerColor = (highlight == g_stores[cell].owner) && ((g_frameVsync & 7) == 0) ?
                                      g_stores[cell].owner + 1 : 0;
                        break;
                    }
                }
                sprite = ownerColor + 0xd5;
            }
            else
            {
                continue;
            }
            Sprite_Draw(g_spriteGroups[0].base + sprite,0,PAL_DEFAULT,pos,g_ot + g_otIndex,9);
        }
    }

    /* the background */
    g_boxFill.attribute = 0x40000000;
    g_boxFill.w = g_stageDefs[g_boardStage].width * cellW + (short)cellW * 2;
    g_boxFill.h = cellH + g_stageDefs[g_boardStage].floors * cellH;
    c = (g_tintR - 0x80) * 2;
    g_boxFill.r = c < 0 ? 0 : (0xff < c ? 0xff : c);
    c = (g_tintG - 0x80) * 2;
    g_boxFill.g = c < 0 ? 0 : (0xff < c ? 0xff : c);
    c = (g_tintB - 0x80) * 2;
    g_boxFill.b = c < 0 ? 0 : (0xff < c ? 0xff : c);
    g_boxFill.x = x;
    g_boxFill.y = y;
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,9);
}

/* Display task: the players' panels (piece, money, assets), sliding in and out on messages: 0
 * end, 1 slide in from the right, 2 slide out to the right, 6 rise from the bottom, 7 count
 * the money up then drop, 0x10 show at once (0x800697A8) */
/* 0x800697A8 */
u32 PlayerPanel_Task(short *step, PlayerPanelWork *work)
{
    PanelLayout *layout;
    Player *pl;
    ScreenPos *panel;
    short next;
    short palette;
    short px;
    short py;
    u16 sprite;
    int done;
    int diff;
    int msg[2];
    int i;

    if (Task_ReceiveMessage(-3,msg) == 0)
    {
        next = -1;
        switch (msg[0])
        {
        case DISP_END:
            next = PANEL_END;
            break;
        case DISP_SHOW:
            next = PANEL_SLIDE_IN;
            break;
        case DISP_HIDE:
            next = PANEL_SLIDE_OUT_BOTTOM;
            break;
        case DISP_SHOW_LOW:
            next = PANEL_RISE;
            break;
        case DISP_COUNT_DROP:
            next = PANEL_COUNT_DROP;
            break;
        case DISP_SHOW_NOW:
            next = PANEL_SHOW_NOW;
            break;
        }
        if (next != -1)
        {
            *step = next;
        }
    }
    layout = &k_playerPanelLayout[g_numPlayers - 1];
    switch (*step)
    {
    case PANEL_INIT:
        work->visible = 0;
        work->counting = 0;
        work->pulseDown = 0;
        work->frame = 0;
        work->highlightCur = 0;
        work->bobAngle = 0;
        work->pulse = 0;
        work->curPlayer = g_curPlayer;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].x = 0x140;
            work->panels[i].y = layout->pos[i][1] + 0xa8;
        }
        *step = PANEL_HIDDEN;
        break;
    case PANEL_HIDDEN:
        /* hidden; the shown values follow the real ones */
        work->visible = 0;
        for (i = 0; i < g_numPlayers; i++)
        {
            g_players[i].moneyShown = g_players[i].money;
            g_players[i].assetsShown = g_players[i].assets;
        }
        break;

    /* slide in from the right: all to the left edge, then the top row, then the bottom row to
     * their places */
    case PANEL_SLIDE_IN:
        work->visible = 1;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].x = 0x140;
            work->panels[i].y = layout->pos[i][1] + 0xa8;
        }
        *step += 1;
        break;
    case PANEL_SLIDE_TO_EDGE:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].x -= 24;
            if (work->panels[i].x < 0x11)
            {
                work->panels[i].x = 16;
            }
            else
            {
                done = false;
            }
        }
        if (done)
        {
            *step += 1;
        }
        break;
    case PANEL_SLIDE_TOP_ROW:
    case PANEL_SLIDE_BOTTOM_ROW:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            if (layout->pos[i][1] == (*step == PANEL_SLIDE_TOP_ROW ? 0 : 28))
            {
                work->panels[i].x += layout->pos[i][0] / 6;
                if (work->panels[i].x < layout->pos[i][0] + 16)
                {
                    done = false;
                }
                else
                {
                    work->panels[i].x = layout->pos[i][0] + 16;
                }
            }
        }
        if (done)
        {
            *step = *step == PANEL_SLIDE_TOP_ROW ? PANEL_SLIDE_BOTTOM_ROW : PANEL_SHOWN;
        }
        break;

    /* slide out: the bottom row, then the top row to the left edge, then all to the right */
    case PANEL_SLIDE_OUT_BOTTOM:
    case PANEL_SLIDE_OUT_TOP:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            if (layout->pos[i][1] == (*step == PANEL_SLIDE_OUT_BOTTOM ? 28 : 0))
            {
                work->panels[i].x -= layout->pos[i][0] / 6;
                if (work->panels[i].x < 0x11)
                {
                    work->panels[i].x = 16;
                }
                else
                {
                    done = false;
                }
            }
        }
        if (done)
        {
            *step += 1;
        }
        break;
    case PANEL_SLIDE_OUT_RIGHT:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].x += 24;
            if (work->panels[i].x < 0xf0)
            {
                done = false;
            }
            else
            {
                work->panels[i].x = 0xf0;
            }
        }
        if (done)
        {
            work->visible = 0;
            *step = PANEL_HIDDEN;
        }
        break;

    /* rise from below */
    case PANEL_RISE:
        done = true;
        work->visible = 1;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].y -= 8;
            if (layout->pos[i][1] + 0xa8 < work->panels[i].y)
            {
                done = false;
            }
            else
            {
                work->panels[i].y = layout->pos[i][1] + 0xa8;
            }
        }
        if (done)
        {
            *step = PANEL_SHOWN;
        }
        break;

    /* when the counting is done, drop below the screen */
    case PANEL_COUNT_DROP:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            if ((g_players[i].moneyShown != g_players[i].money) || (g_players[i].assetsShown != g_players[i].assets))
            {
                done = false;
            }
        }
        if (done)
        {
            *step += 1;
        }
        break;
    case PANEL_DROP:
        done = true;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].y += 8;
            if (work->panels[i].y < 0xf1)
            {
                done = false;
            }
        }
        if (done)
        {
            for (i = 0; i < g_numPlayers; i++)
            {
                work->panels[i].x = layout->pos[i][0] + 16;
            }
            work->visible = 0;
            *step = PANEL_HIDDEN;
        }
        break;

    case PANEL_SHOW_NOW:
        work->visible = 1;
        for (i = 0; i < g_numPlayers; i++)
        {
            work->panels[i].x = layout->pos[i][0] + 16;
            work->panels[i].y = layout->pos[i][1] + 0xa8;
        }
        *step = PANEL_SHOWN;
        break;
    case PANEL_END:
        return 1;
    }

    /* the bobbing arrow and the pulse */
    work->bobAngle += 0x80;
    if (work->pulseDown == 0)
    {
        work->pulseSpeed += 0x60;
        work->pulse += work->pulseSpeed;
        if (0xfff < (short)work->pulse)
        {
            work->pulseDown = 1;
            work->pulse = 0x1000;
        }
    }
    else
    {
        work->pulse -= 0x60;
        if ((short)work->pulse < 1)
        {
            work->pulseDown = 0;
            work->pulse = 0;
            work->pulseSpeed = 0;
        }
    }
    /* the shown money and assets count toward the real ones, a fifteenth at a time */
    work->counting = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        pl = &g_players[(short)i];
        if (pl->moneyShown != pl->money)
        {
            diff = (pl->money - pl->moneyShown) / 15;
            if (diff == 0)
            {
                diff = pl->moneyShown < pl->money ? 1 : -1;
            }
            pl->moneyShown += diff;
            work->counting = 1;
        }
        pl->assets = Player_CalcAssets(i);
        if (pl->assetsShown != pl->assets)
        {
            diff = (pl->assets - pl->assetsShown) / 15;
            if (diff == 0)
            {
                diff = pl->assetsShown < pl->assets ? 1 : -1;
            }
            pl->assetsShown += diff;
            work->counting = 1;
        }
    }
    if (work->visible == 0)
    {
        return 0;
    }

    if (work->highlightCur == 0)
    {
        work->curPlayer = g_curPlayer;
    }
    work->bobX = rcos(work->bobAngle) / 1024;
    work->bobY = rsin(work->bobAngle) / 1024;
    for (i = g_numPlayers - 1; i >= 0; i--)
    {
        pl = &g_players[i];
        panel = &work->panels[i];
        px = panel->x;
        py = panel->y;
        /* the assets going up or down */
        if (pl->assetsShown < pl->assets)
        {
            g_drawPos[0] = px + 8;
            g_drawPos[1] = py + work->bobY + 12;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x8a,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,7);
        }
        else if (pl->assets < pl->assetsShown)
        {
            g_drawPos[0] = px + 8;
            g_drawPos[1] = py + (char)(work->pulse >> 8) + 4;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x8b,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,7);
        }
        /* the piece (gray when out); on the narrow panels the current player shows the labels in
         * its place */
        palette = i;
        if ((g_numPlayers == 3 || g_numPlayers == 5 || g_numPlayers == 6) && (pl->out == 0) &&
            (i == work->curPlayer))
        {
            g_drawPos[0] = px + 8;
            g_drawPos[1] = py + 1;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x84,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,8);
            g_drawPos[0] = px + 8;
            g_drawPos[1] = py + 12;
            Sprite_DrawAligned(g_spriteGroups[0].base + 0x87,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                               g_ot + g_otIndex,8);
        }
        else if (1 <= g_numPlayers && g_numPlayers <= 6)
        {
            g_drawPos[0] = px + 8;
            g_drawPos[1] = py + 0x1b;
            g_drawPos[2] = 0x1a;
            g_drawPos[3] = 0x27;
            if (pl->out == 0)
            {
                sprite = Piece_GetSprite(i,pl->pieceSprite,-100);
            }
            else
            {
                palette = -101;
                sprite = g_spriteGroups[3].base + i * 5;
            }
            Sprite_DrawStretched(sprite,0,palette,g_drawPos,ALIGN_LEFT | ALIGN_BOTTOM,g_ot + g_otIndex,7);
            if (g_numPlayers == 1 || g_numPlayers == 2 || g_numPlayers == 4)
            {
                /* the wide panels have the labels next to the piece */
                g_drawPos[0] = px + 0x28;
                g_drawPos[1] = py + 1;
                Sprite_DrawAligned(g_spriteGroups[0].base + 0x84,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                                   g_ot + g_otIndex,8);
                g_drawPos[0] = px + 0x28;
                g_drawPos[1] = py + 12;
                Sprite_DrawAligned(g_spriteGroups[0].base + 0x87,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                                   g_ot + g_otIndex,8);
            }
        }
        /* money and assets (the current player's blink) */
        if ((i != work->curPlayer) || ((g_frameVsync & 8) != 0))
        {
            g_drawPos[0] = px + layout->width - 0x4c;
            g_drawPos[1] = py - 1;
            Gfx_DrawNumber(pl->moneyShown,g_drawPos[0],g_drawPos[1],7,10,g_spriteGroups[0].base + 0x4b,0,
                           g_ot + g_otIndex,8);
            g_drawPos[0] = px + layout->width - 0x4c;
            g_drawPos[1] = py + 11;
            Gfx_DrawNumber(pl->assetsShown,g_drawPos[0],g_drawPos[1],7,10,g_spriteGroups[0].base + 0x4b,0,
                           g_ot + g_otIndex,8);
        }
        g_drawPos[0] = px;
        g_drawPos[1] = py;
        g_drawPos[2] = layout->width;
        g_drawPos[3] = 28;
        if ((work->highlightCur != 0) && (work->curPlayer == i))
        {
            Frame_DrawPlain(g_drawPos,g_ot + g_otIndex,8);
        }
        Frame_DrawPanel(g_drawPos,i,g_ot + g_otIndex,8);
    }
    work->frame++;
    if ((work->counting != 0) && ((work->frame & 3) == 0))
    {
        Se_Play(0x1c,0x7f,0x7f,1);
    }
    return 0;
}

/* Sprites of the calendar panel, in sprite group 0 */
#define HUD_DIGIT0       0xFA   /* the digits 0-9 */
#define HUD_COMMA        0x104
#define HUD_YEAR_LABEL   0x106
#define HUD_MONTH_LABEL  0x107
#define HUD_GOAL_LABEL   0x108

/* Hud_CalendarTask steps */
enum HudCalendarStep
{
    HUDCAL_INIT       =   0,
    HUDCAL_IDLE       =  10,
    HUDCAL_REFRESH    =  20,  /* DISP_REFRESH: slide out, unless the month is the one shown */
    HUDCAL_NEW_MONTH  =  21,
    HUDCAL_SLIDE_IN   =  22,
    HUDCAL_SHOW       =  30,  /* DISP_SHOW */
    HUDCAL_SLIDING_IN =  31,
    HUDCAL_HIDE       =  40,  /* DISP_HIDE */
    HUDCAL_HIDDEN     =  41,
    HUDCAL_SHOW_NOW   =  60,  /* DISP_SHOW_NOW */
    HUDCAL_END        = 100   /* DISP_END: slide out and end */
};

/* Display task: the year / month and target assets panel (top right). Messages: 0 slide out and
 * end, 1 slide in, 2 slide out, 3 slide out and in again if the month changed, 0x10 show at once
 * (0x8006B35C) */
/* 0x8006B35C */
u32 Hud_CalendarTask(short *step, HudCalendarWork *work)
{
    int message[2];
    int month;

    if (Task_ReceiveMessage(TASK_ALL,message) == 0)
    {
        switch (message[0])
        {
        case DISP_END:
            *step = HUDCAL_END;
            break;
        case DISP_SHOW:
            *step = HUDCAL_SHOW;
            break;
        case DISP_HIDE:
            *step = HUDCAL_HIDE;
            break;
        case DISP_REFRESH:
            *step = HUDCAL_REFRESH;
            break;
        case DISP_SHOW_NOW:
            *step = HUDCAL_SHOW_NOW;
            break;
        }
    }
    switch (*step)
    {
    case HUDCAL_INIT:
        work->shown = 0;
        work->unused06 = 80;
        work->unused07 = 0;
        work->height = 0x30;
        work->x = 0xb0;
        work->y = -0x30;
        work->month = g_gameConfig.month;
        *step = HUDCAL_IDLE;
        break;
    case HUDCAL_REFRESH:
        /* slide out 8 lines a frame, unless the month is still the one shown */
        if (work->month == g_gameConfig.month)
        {
            *step = HUDCAL_NEW_MONTH;
            break;
        }
        work->y -= 8;
        if (work->y > -work->height)
        {
            break;
        }
        work->y = -work->height;
        *step += 1;
        break;
    case HUDCAL_NEW_MONTH:
    case HUDCAL_SHOW:
        work->shown = 1;
        work->month = g_gameConfig.month;
        *step += 1;
        break;
    case HUDCAL_SLIDE_IN:
    case HUDCAL_SLIDING_IN:
        /* slide in */
        work->y += 8;
        if (work->y > 15)
        {
            work->y = 0x10;
            *step = HUDCAL_IDLE;
        }
        break;
    case HUDCAL_HIDE:
        work->y -= 8;
        if (work->y > -work->height)
        {
            break;
        }
        work->y = -work->height;
        work->shown = 0;
        *step += 1;
        break;
    case HUDCAL_SHOW_NOW:
        work->shown = 1;
        work->y = 0x10;
        work->month = g_gameConfig.month;
        *step = HUDCAL_IDLE;
        break;
    case HUDCAL_END:
        work->y -= 8;
        if (work->y <= -work->height)
        {
            work->y = -work->height;
            work->shown = 0;
            return 1;
        }
        break;
    }
    if (work->shown == 0)
    {
        return 0;
    }
    /* "<year> year <month> month", "goal <thousands>,<hundreds>00" */
    g_drawPos[1] = work->y + 2;
    g_drawPos[0] = work->x + 0x38;
    Gfx_DrawNumber(((char)g_gameConfig.year + 1) % 99,g_drawPos[0],g_drawPos[1],2,8,
                   GROUP_SPRITE(0,HUD_DIGIT0),0,g_ot + g_otIndex,8);
    g_drawPos[0] += 0x10;
    Sprite_Draw(GROUP_SPRITE(0,HUD_YEAR_LABEL),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[0] += 0x10;
    month = ((char)g_gameConfig.month + 4) % 12;
    if (month == 0)
    {
        month = 12;
    }
    Gfx_DrawNumber(month,g_drawPos[0],g_drawPos[1],2,8,GROUP_SPRITE(0,HUD_DIGIT0),0,g_ot + g_otIndex,8);
    g_drawPos[0] += 0x10;
    Sprite_Draw(GROUP_SPRITE(0,HUD_MONTH_LABEL),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[1] = work->y + 0x14;
    g_drawPos[0] = work->x + 8;
    Sprite_Draw(GROUP_SPRITE(0,HUD_GOAL_LABEL),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[0] += 0x40;
    Gfx_DrawNumber((g_gameConfig.goalMoney / 1000) % 100,g_drawPos[0],g_drawPos[1],2,8,
                   GROUP_SPRITE(0,HUD_DIGIT0),0,g_ot + g_otIndex,8);
    g_drawPos[0] += 0x10;
    Sprite_Draw(GROUP_SPRITE(0,HUD_COMMA),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[0] += 8;
    Gfx_DrawNumber((g_gameConfig.goalMoney / 100) % 100,g_drawPos[0],g_drawPos[1],1,8,
                   GROUP_SPRITE(0,HUD_DIGIT0),0,g_ot + g_otIndex,8);
    g_drawPos[0] += 8;
    Sprite_Draw(GROUP_SPRITE(0,HUD_DIGIT0),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[0] += 8;
    Sprite_Draw(GROUP_SPRITE(0,HUD_DIGIT0),0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    g_drawPos[2] = 0x80;
    g_drawPos[3] = 0x28;
    g_drawPos[0] = work->x;
    g_drawPos[1] = work->y;
    Frame_DrawPanel(g_drawPos,6,g_ot + g_otIndex,8);
    return 0;
}

/* Rank the players and create the text of each player's name (g_playerNameTexts). The old texts
 * are forgotten first, so the Text_Free calls free nothing (as in the original) (0x8006BB2C) */
/* 0x8006BB2C */
void Hud_CreateNameTexts(void)
{
    int i;

    Players_Rank();
    for (i = 5; i >= 0; i--)
    {
        g_playerNameTexts[i] = -1;
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        Text_Free(g_playerNameTexts[i]);
        g_playerNameTexts[i] = Text_Create(g_players[i].name);
        g_playerNameState[i] = 2;
    }
}

/* Free the six player name texts (0x8006BBE8) */
/* 0x8006BBE8 */
void Hud_FreeNameTexts(void)
{
    int i;

    for (i = 0; i < 6; i++)
    {
        Text_Free(g_playerNameTexts[i]);
    }
}

/* Points_DisplayTask steps */
enum PointsDisplayStep
{
    POINTSWIN_OPEN       =   0,
    POINTSWIN_WAIT_OPEN  =   1,
    POINTSWIN_SHOWN      =  10,  /* counts down to the player's points */
    POINTSWIN_CLOSE      = 100,  /* message 0 */
    POINTSWIN_WAIT_COUNT = 101,  /* once the count has caught up */
    POINTSWIN_PAUSE      = 102,
    POINTSWIN_WAIT_PAUSE = 103,  /* 15 frames */
    POINTSWIN_CLOSE_WIN  = 104,
    POINTSWIN_WAIT_CLOSE = 105 
};

/* Task: a small window with the current player's points (∞ when the rules have no points,
 * BoardRules.startPoints 0xFF); while the points are going down it counts down 2 a frame;
 * message 0 closes it, a quarter of a second after the count has caught up (0x8006F60C) */
/* 0x8006F60C */
u32 Points_DisplayTask(u16 *step, PointsDisplayWork *work)
{
    int msg[2];
    int points;

    if (Task_ReceiveMessage(-3,msg) == 0 && msg[0] == 0)
    {
        *step = POINTSWIN_CLOSE;
    }
    switch (*step)
    {
    case POINTSWIN_OPEN:
        work->shown = 1;
        work->points = g_players[g_curPlayer].points;
        strcpy(g_msgBuf,"%LPoints:     ");
        if (g_boardRules.startPoints == 0xff)
        {
            strcat(g_msgBuf,"\201\207");          /* ∞ */
        }
        else
        {
            Str_FormatInt(work->points,2,g_msgBuf + strlen(g_msgBuf));
        }
        strcat(g_msgBuf,"%E");
        work->win = TextWin_Create(work->x,work->y,0xe,1);
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        *step += 1;
        break;
    case POINTSWIN_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            *step = POINTSWIN_SHOWN;
        }
        break;
    case POINTSWIN_CLOSE:         /* closing: once the count has caught up */
        work->closing = 0;
        *step += 1;
        break;
    case POINTSWIN_WAIT_COUNT:
        if (g_boardRules.startPoints == 0xff || work->points == g_players[g_curPlayer].points)
        {
            *step = POINTSWIN_PAUSE;
        }
        else
        {
            work->closing = 1;
        }
        break;
    case POINTSWIN_PAUSE:
        if (work->closing == 0)
        {
            *step = POINTSWIN_CLOSE_WIN;
            break;
        }
        work->delay = 15;
        *step += 1;
        break;
    case POINTSWIN_WAIT_PAUSE:
        if (--work->delay == 0)
        {
            *step += 1;
        }
        break;
    case POINTSWIN_CLOSE_WIN:
        TextWin_Close(work->win);
        *step = POINTSWIN_WAIT_CLOSE;
        /* fall through */
    case POINTSWIN_WAIT_CLOSE:
        if (g_textWins[work->win].state == TEXTWIN_FREEING)
        {
            work->shown = 0;
            return 1;
        }
        break;
    }
    /* counting down to the player's points */
    if (work->shown != 0 && g_boardRules.startPoints != 0xff && work->points != g_players[g_curPlayer].points)
    {
        points = (signed char)(work->points - 2);
        work->points = points;
        if (points < g_players[g_curPlayer].points)
        {
            work->points = g_players[g_curPlayer].points;
        }
        strcpy(g_msgBuf,"%LPoints:     ");
        Str_FormatInt(work->points,2,g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf,"%E");
        TextWin_ClearText(work->win);
        TextWin_AddText(work->win,g_msgBuf);
    }
    return 0;
}
