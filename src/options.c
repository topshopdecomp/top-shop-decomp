/*
 * options.c - the turn options and the save prompt
 *
 * The options screen a player opens from the turn's command menu (event scenes, CPU speed,
 * controllers, saving the game) and, in story mode, the prompt to save between stages.
 */
#include "common.h"
#include "topshop.h"

/* ControllerSelect_Task steps */
enum ControllerSelectStep
{
    CTRLSEL_OPEN       =   0,
    CTRLSEL_WAIT_OPEN  =   1,
    CTRLSEL_WAIT_START =  10,  /* the pad whose START is pressed */
    CTRLSEL_CLOSE      = 100,
    CTRLSEL_WAIT_CLOSE = 101,
    CTRLSEL_DONE       = 102 
};

/* "Press the start button on the controller you wish to use." (0x80062268) */
/* 0x80062268 */
int ControllerSelect_Task(short *step, ControllerSelectWork *work)
{
    short win;
    int player;

    switch ((u16)*step)
    {
    case CTRLSEL_OPEN:
        win = TextWin_Create(TEXTWIN_AUTO,TEXTWIN_AUTO,-1,-1);
        work->win = win;
        TextWin_AddText(work->win,"%CPress the start button%Kon the controller you wish to use.");
        TextWin_Open(work->win);
        *step += 1;
        if (g_textWins[work->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        TextWin_SetFocus(work->win,1);
        *step = CTRLSEL_WAIT_START;
        break;
    case CTRLSEL_WAIT_OPEN:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        TextWin_SetFocus(work->win,1);
        *step = CTRLSEL_WAIT_START;
        break;
    case CTRLSEL_WAIT_START:
        player = Pad_AssignPressed(work->player,0x800);
        if ((short)player != -1)
        {
            g_players[*(int *)&work->player].pad = player;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = CTRLSEL_CLOSE;
        }
        break;
    case CTRLSEL_CLOSE:
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        break;
    case CTRLSEL_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING)
        {
            return 0;
        }
        *step = CTRLSEL_DONE;
        break;
    case CTRLSEL_DONE:
        return 1;
    default:
        return 0;
    }
    return 0;
}

/* TurnOptionsWork.result: the page shown */
#define OPTS_PAGE_OPTIONS      1
#define OPTS_PAGE_SAVE         2
#define OPTS_PAGE_CONTROLLERS  3

/* Rows of the options page (sprites of group 0: the labels); in stage mode (playMode 1) only the
 * first two and "back" */
static const u8 k_optionRows[2][5] = {
    {0xba, 0xbe, 0xaf, 0xae, 0xb0},           /* event scenes, CPU fast, controllers, save, back */
    {0xba, 0xbe, 0xb0, 0, 0}
};
#define OPTS_ROW_SCENES       0
#define OPTS_ROW_CPU_FAST     1
#define OPTS_ROW_CONTROLLERS  2
#define OPTS_ROW_SAVE         3

/* A component of the band's color: the tint (0x80 = none) doubled about 0x80, kept in 0-255 */
static u8 TurnOptions_TintColor(short tint)
{
    int value = (tint - 0x80) * 2 + 0x80;

    if (value < 0)
    {
        return 0;
    }
    if (value > 0xff)
    {
        return 0xff;
    }
    return value;
}

/* Forget the texts of the save slots (g_menuRowItems) */
static void TurnOptions_FreeSlotTexts(void)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        Text_Free(g_menuRowItems[i]);
        g_menuRowItems[i] = -1;
    }
}

/* The page fades in 32 lines a frame; 1 once it is all there */
static int TurnOptions_FadeIn(TurnOptionsWork *work)
{
    work->fade += 0x20;
    if ((short)work->fade > 0xbf)
    {
        work->fade = 0xc0;
        return 1;
    }
    return 0;
}

/* The page fades out; 1 once it is gone */
static int TurnOptions_FadeOut(TurnOptionsWork *work)
{
    work->fade -= 0x20;
    if ((short)work->fade < 1)
    {
        work->fade = 0;
        return 1;
    }
    return 0;
}

/* The turn index of the character chosen first (GameConfig.turnOrder); none: the original reads
 * an unset register, here 0 */
static int TurnOptions_FirstChosen(void)
{
    int k;

    for (k = 0; k < g_numPlayers; k++)
    {
        if (g_gameConfig.turnOrder[k] == 0)
        {
            return k;
        }
    }
    return 0;
}

/* The options page: the rows, the values of the first two, the hand at the cursor */
static void TurnOptions_DrawOptions(TurnOptionsWork *work)
{
    int stage = (g_gameConfig.playMode == PLAY_STAGE);
    int value;
    int i;

    g_drawPos[0] = 0x20;
    g_drawPos[1] = 0x20;
    Sprite_DrawAligned(GROUP_SPRITE(0,0xad),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    for (i = 0; i < work->count; i++)
    {
        g_drawPos[0] = 0x30;
        g_drawPos[1] = 0x38 + i * 0x18;
        Sprite_DrawAligned(GROUP_SPRITE(0,k_optionRows[stage][i]),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                           g_ot + g_otIndex,8);
        if (i == OPTS_ROW_SCENES || i == OPTS_ROW_CPU_FAST)
        {
            if (i == OPTS_ROW_SCENES)
            {
                value = (g_optEventScenes != 0) ? 0xbb : 0xbc;
            }
            else
            {
                value = (g_optCpuFast != 0) ? 0xc0 : 0xbf;
            }
            g_drawPos[0] = 0xc0;
            g_drawPos[1] = 0x38 + i * 0x18;
            Sprite_DrawAligned(GROUP_SPRITE(0,value),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        }
    }
    g_drawPos[1] = work->cursor * 0x18 + 0x38;
    g_drawPos[0] = 0x20;
    Sprite_DrawAligned(MENU_HAND(g_frameVsync),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
}

/* The save screen (TurnOptions_Task, SavePrompt_Task): four slots with their descriptions, the
 * hand at the slot */
static void Save_DrawSlots(int slot)
{
    int i;

    g_drawPos[0] = 0x20;
    g_drawPos[1] = 0x20;
    Sprite_DrawAligned(GROUP_SPRITE(0,0xae),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    for (i = 0; i < 4; i++)
    {
        g_drawPos[0] = 0x30;
        g_drawPos[1] = 0x38 + i * 0x28;
        Sprite_DrawAligned(GROUP_SPRITE(0,0xb5),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        Text_Draw(g_menuRowItems[i],0x30,0x48 + i * 0x28,g_ot + g_otIndex,8);
    }
    g_drawPos[1] = slot * 0x28 + 0x38;
    g_drawPos[0] = 0x20;
    Sprite_DrawAligned(MENU_HAND(g_frameVsync),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
}

/* The controllers page: each player's piece (gray when out) in a zigzag, its pad (animated for a
 * human, still for a CPU player) and the pad's shadow (the other way around), the hand over the
 * chosen player */
static void TurnOptions_DrawControllers(TurnOptionsWork *work)
{
    Player *pl;
    short palette;
    u16 id;
    int animated;
    int j;

    g_drawPos[0] = 0x20;
    g_drawPos[1] = 0x20;
    Sprite_DrawAligned(GROUP_SPRITE(0,0xaf),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    g_drawPos[0] = work->player * 0x30 + 0x28;
    g_drawPos[1] = (work->player & 1) ? 0x48 : 0x58;
    Sprite_DrawAligned(MENU_HAND(g_frameVsync),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    for (j = 0; j < g_numPlayers; j++)
    {
        pl = &g_players[j];
        g_drawPos[0] = j * 0x30 + 0x20;
        g_drawPos[1] = (j & 1) ? 0x58 : 0x68;
        if (pl->out == 0)
        {
            id = Piece_GetSprite(j,pl->pieceSprite,PAL_DEFAULT);
            palette = j;
        }
        else
        {
            palette = PAL_BLANK;
            id = GROUP_SPRITE(3,j * 5);
        }
        Sprite_DrawAligned(id,0,palette,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        animated = GROUP_SPRITE(0,0xc3) + g_frameVsync / 8 % 3;
        g_drawPos[0] = j * 0x30 + 0x18;
        g_drawPos[1] = (j & 1) ? 0x78 : 0x88;
        id = (pl->isCpu == 0) ? animated : GROUP_SPRITE(0,0xc2);
        Sprite_DrawAligned(id,0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,7);
        g_drawPos[0] = j * 0x30 + 0x14;
        g_drawPos[1] = (j & 1) ? 0x7c : 0x8c;
        id = (pl->isCpu == 0) ? GROUP_SPRITE(0,0xc2) : animated;
        Sprite_DrawAligned(id,GFX_SEMITRANS,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,7);
    }
}

/* TurnOptions_Task steps */
enum TurnOptionsStep
{
    TOPT_INIT                   =   0,
    TOPT_OPEN                   =   5,  /* the band grows in with the options page */
    TOPT_OPTIONS                =  10,  /* the options page fades in */
    TOPT_OPTIONS_CHOOSE         =  11,
    TOPT_OPTIONS_FADE_OUT       =  12,  /* (not reached) */
    TOPT_OPTIONS_LEAVE          =  13,
    TOPT_SAVE_PAGE              =  20,  /* the slot list */
    TOPT_SAVE_FADE_IN           =  21,
    TOPT_SAVE_CHOOSE            =  22,  /* a free slot is saved at once, a used one after "Overwrite?" */
    TOPT_OVERWRITE              =  23,  /* "Overwrite?" */
    TOPT_OVERWRITE_ANSWER       =  24,
    TOPT_SAVE_WRITE             =  25,  /* Save_Pack */
    TOPT_SAVE_RESULT            =  26,  /* "Save completed." / "Save failed." */
    TOPT_WAIT_SAVE_RESULT       =  27,
    TOPT_SAVE_BACK              =  29,  /* the list closes */
    TOPT_CONTROLLERS            =  30,  /* the controllers page */
    TOPT_CONTROLLERS_CHOOSE     =  31,  /* left / right the player, up / down CPU or human */
    TOPT_CONTROLLER_SELECT      =  32,  /* ControllerSelect_Task */
    TOPT_WAIT_CONTROLLER_SELECT =  33,
    TOPT_FIRST_NOT_CPU          =  34,  /* "The character chosen first cannot be CPU." */
    TOPT_WAIT_FIRST_NOT_CPU     =  35,
    TOPT_CONTROLLERS_BACK       =  39,  /* back to the options page */
    TOPT_CLOSE                  = 100,
    TOPT_DONE                   = 101,
    TOPT_CARD_CHECK             = 200,  /* Card_SaveTask */
    TOPT_WAIT_CARD_CHECK        = 201,
    TOPT_CARD_REMOVED           = 210,  /* the card was taken out */
    TOPT_CARD_REMOVED_MSG       = 211,  /* "Memory Card was removed. Save failed." */
    TOPT_WAIT_CARD_REMOVED_MSG  = 212,
    TOPT_SAVING                 = 213,  /* "Saving... Please do not remove Memory Card." */
    TOPT_WAIT_SAVING_SHOWN      = 214,
    TOPT_SAVING_SHOWN           = 215,
    TOPT_SAVING_CLOSE           = 216,
    TOPT_WAIT_SAVING_CLOSE      = 217,
    TOPT_CARD_REMOVED_MSG2      = 250,  /* (not reached) as TOPT_CARD_REMOVED_MSG */
    TOPT_WAIT_CARD_REMOVED_MSG2 = 251   /* (not reached) */
};

/* Turn menu option 4, the options: event scenes and CPU speed (left / right), the controllers
 * (who is a CPU player, and the pads: ControllerSelect_Task; "The character chosen first cannot
 * be CPU"), saving the game in one of four memory card slots ("Overwrite?"); the card is checked
 * every frame while a save page is up (busy). Over a darkened band that grows in (0x80062458) */
/* 0x80062458 */
int TurnOptions_Task(short *step, TurnOptionsWork *work)
{
    PadState *pad = &g_playerPads[g_curPlayer];
    Player *pl;
    MsgWork *msg;
    int i;

    FntPrint("save p : %d\n", (u16)*step);
    switch (*step)
    {
    case TOPT_INIT:
        work->shown = 0;
        work->result = 0;
        work->h = 0;
        work->fade = 0;
        work->cursor = 0;
        work->slot = 0;
        work->player = 0;
        work->busy = 0;
        work->count = (g_gameConfig.playMode == PLAY_STAGE) ? 3 : 5;
        for (i = 3; i >= 0; i--)
        {
            g_menuRowItems[i] = -1;
        }
        *step = TOPT_OPEN;
        break;
    case TOPT_OPEN:
        /* the band grows in with the page */
        work->shown = 1;
        work->result = OPTS_PAGE_OPTIONS;
        work->redraw = 0;
        work->h += 0x20;
        if (TurnOptions_FadeIn(work))
        {
            work->h = 0xc0;
            *step = TOPT_OPTIONS_CHOOSE;
        }
        break;
    case TOPT_OPTIONS:
        work->shown = 1;
        work->result = OPTS_PAGE_OPTIONS;
        work->redraw = 0;
        if (TurnOptions_FadeIn(work))
        {
            *step += 1;
        }
        break;
    case TOPT_OPTIONS_CHOOSE:
        if (pad->pressed & PADRdown)
        {
            if (work->cursor == work->count - 1)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);          /* back */
                *step = TOPT_CLOSE;
            }
            else if (work->cursor == OPTS_ROW_CONTROLLERS)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = TOPT_CONTROLLERS;
            }
            else if (work->cursor == OPTS_ROW_SAVE)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = TOPT_CARD_CHECK;
            }
        }
        else if (pad->pressed & PADRright)
        {
            *step = TOPT_CLOSE;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (pad->repeat & PADLup)
        {
            work->cursor = (work->cursor == 0) ? work->count - 1 : work->cursor - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (pad->repeat & PADLdown)
        {
            work->cursor = (work->cursor == work->count - 1) ? 0 : work->cursor + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (pad->repeat & (PADLleft | PADLright))
        {
            /* (an unreachable row -1 toggles g_optSkipCpuTalk) */
            if (work->cursor == OPTS_ROW_SCENES)
            {
                g_optEventScenes = (g_optEventScenes == 0);
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            else if (work->cursor == OPTS_ROW_CPU_FAST)
            {
                g_optCpuFast = (g_optCpuFast == 0);
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
        }
        break;
    case TOPT_OPTIONS_FADE_OUT:               /* (not reached) */
        if (TurnOptions_FadeOut(work))
        {
            *step += 1;
        }
        break;
    case TOPT_OPTIONS_LEAVE:
        if (work->cursor == OPTS_ROW_CONTROLLERS)
        {
            *step = TOPT_CONTROLLERS;
        }
        else if (work->cursor == OPTS_ROW_SAVE)
        {
            *step = TOPT_CARD_CHECK;
        }
        break;

    /* 20-29: the save page */
    case TOPT_SAVE_PAGE:
        work->busy = 1;
        work->shown = 1;
        work->result = OPTS_PAGE_SAVE;
        work->redraw = 0;
        work->slot = 0;
        TurnOptions_FreeSlotTexts();
        Save_DescribeCurrent(work->slotFree);
        *step += 1;
        break;
    case TOPT_SAVE_FADE_IN:
        if (TurnOptions_FadeIn(work))
        {
            *step += 1;
        }
        break;
    case TOPT_SAVE_CHOOSE:
        /* a free slot is saved at once, a used one after "Overwrite?" */
        if (pad->pressed & PADRdown)
        {
            if (work->slotFree[work->slot] == 1)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = TOPT_SAVING;
            }
            else if (work->slotFree[work->slot] == 0)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step += 1;
            }
        }
        else if (pad->pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            work->redraw = 1;
            work->busy = 0;
            *step = TOPT_SAVE_BACK;
            Save_FreeBuffer();
        }
        else if (pad->repeat & PADLup)
        {
            work->slot = (work->slot == 0) ? 3 : work->slot - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (pad->repeat & PADLdown)
        {
            work->slot = (work->slot == 3) ? 0 : work->slot + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case TOPT_OVERWRITE:
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%COverwrite?%KYes%KNo%E");
        *step += 1;
        break;
    case TOPT_OVERWRITE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = (g_msgAnswer == 0) ? TOPT_SAVING : TOPT_SAVE_CHOOSE;
        }
        break;
    case TOPT_SAVE_WRITE:
        work->busy = 0;
        work->saveFailed = Save_Pack(work->slot,g_curPlayer);
        *step = TOPT_SAVING_CLOSE;
        break;
    case TOPT_SAVE_RESULT:
        work->busy = 1;
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,
                 (work->saveFailed == 0) ? "%LSave completed.%E" : "%LSave failed.%E");
        *step += 1;
        break;
    case TOPT_WAIT_SAVE_RESULT:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->busy = 1;
            TurnOptions_FreeSlotTexts();
            Save_DescribeCurrent(work->slotFree);
            *step = TOPT_SAVE_CHOOSE;
        }
        break;
    case TOPT_SAVE_BACK:
    case TOPT_CONTROLLERS_BACK:
        /* back to the options page */
        if (TurnOptions_FadeOut(work))
        {
            *step = TOPT_OPTIONS;
        }
        break;

    /* 30-35: the controllers page */
    case TOPT_CONTROLLERS:
        work->shown = 1;
        work->result = OPTS_PAGE_CONTROLLERS;
        work->redraw = 0;
        work->player = 0;
        if (TurnOptions_FadeIn(work))
        {
            *step += 1;
        }
        break;
    case TOPT_CONTROLLERS_CHOOSE:
        if (pad->pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            work->redraw = 1;
            *step = TOPT_CONTROLLERS_BACK;
        }
        else if (pad->repeat & PADLleft)
        {
            work->player = (work->player == 0) ? g_numPlayers - 1 : work->player - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (pad->repeat & PADLright)
        {
            work->player = (work->player == g_numPlayers - 1) ? 0 : work->player + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if ((pad->pressed & (PADLup | PADLdown)) && g_players[work->player].out == 0)
        {
            /* up / down: a human player becomes a CPU player (but the one chosen first), a CPU
             * player a human one, who then picks a pad */
            pl = &g_players[work->player];
            if (pl->isCpu != 0)
            {
                pl->isCpu = 0;
                *step += 1;
            }
            else if (TurnOptions_FirstChosen() != work->player)
            {
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
                pl->isCpu = 1;
            }
            else
            {
                *step = TOPT_FIRST_NOT_CPU;
            }
        }
        break;
    case TOPT_CONTROLLER_SELECT:
        g_uiTaskWork[0][1] = work->player;
        Task_Start(g_uiTaskIds[0],ControllerSelect_Task);
        *step += 1;
        break;
    case TOPT_WAIT_CONTROLLER_SELECT:
    case TOPT_WAIT_FIRST_NOT_CPU:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = TOPT_CONTROLLERS_CHOOSE;
        }
        break;
    case TOPT_FIRST_NOT_CPU:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,"%CThe character chosen first%Kcannot be CPU.%E");
        *step += 1;
        break;

    /* 100-101: close */
    case TOPT_CLOSE:
        Card_DisableEvents();
        work->h -= 0x20;
        if (TurnOptions_FadeOut(work))
        {
            work->h = 0;
            *step += 1;
        }
        break;
    case TOPT_DONE:
        work->shown = 0;
        TurnOptions_FreeSlotTexts();
        return 1;

    /* 200-201: the memory card checks before the save page (Card_SaveTask) */
    case TOPT_CARD_CHECK:
        work->busy = 0;
        Task_Start(g_cardTaskId,Card_SaveTask);
        *step += 1;
        break;
    case TOPT_WAIT_CARD_CHECK:
        if (Task_IsDone(g_cardTaskId))
        {
            if (g_cardTaskState == CARDTASK_CANCELED)
            {
                work->redraw = 1;
                *step = TOPT_SAVE_BACK;
                work->busy = 0;
            }
            else if (g_cardTaskState == CARDTASK_REMOVED)
            {
                *step = TOPT_CARD_CHECK;
                work->busy = 0;
            }
            else
            {
                work->busy = 1;
                *step = TOPT_SAVE_PAGE;
            }
        }
        break;

    /* 210-212: the card was taken out */
    case TOPT_CARD_REMOVED:
        if (!Task_IsDone(g_uiTaskIds[0]) && g_tasks[(u16)g_uiTaskIds[0]].step < MSG_STEP_CLOSE)
        {
            g_tasks[(u16)g_uiTaskIds[0]].step = MSG_STEP_CLOSE;
        }
        *step += 1;
        break;
    case TOPT_CARD_REMOVED_MSG:
    case TOPT_CARD_REMOVED_MSG2:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,"%CMemory Card was removed.%KSave failed.%E");
            *step += 1;
        }
        break;
    case TOPT_WAIT_CARD_REMOVED_MSG:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->redraw = 1;
            *step = TOPT_SAVE_BACK;
            Save_FreeBuffer();
        }
        break;

    /* 213-217: saving */
    case TOPT_SAVING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CSaving\201c%KPlease do not remove Memory Card.%E");
        *step += 1;
        break;
    case TOPT_WAIT_SAVING_SHOWN:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state == TEXTWIN_SHOWN)
        {
            *step += 1;
        }
        break;
    case TOPT_SAVING_SHOWN:
        *step = TOPT_SAVE_WRITE;
        break;
    case TOPT_SAVING_CLOSE:
        Task_SetStep(g_uiTaskIds[0],MSG_STEP_CLOSE);
        *step += 1;
        break;
    case TOPT_WAIT_SAVING_CLOSE:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = TOPT_SAVE_RESULT;
        }
        break;
    case TOPT_WAIT_CARD_REMOVED_MSG2:         /* (not reached) */
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->redraw = 1;
            *step = TOPT_SAVE_BACK;
        }
        break;
    }

    if (work->busy != 0)
    {
        Card_EnableEvents();
        Card_Check();
        if (CARD_FAILED(g_cardStatus))
        {
            work->busy = 0;
            *step = TOPT_CARD_REMOVED;
        }
        Card_DisableEvents();
    }
    if (work->shown == 0)
    {
        return 0;
    }
    if (work->fade != 0)
    {
        Gfx_SetDrawArea(0,g_ot + g_otIndex,8);
        switch (work->result)
        {
        case OPTS_PAGE_OPTIONS:
            TurnOptions_DrawOptions(work);
            break;
        case OPTS_PAGE_SAVE:
            Save_DrawSlots(work->slot);
            break;
        case OPTS_PAGE_CONTROLLERS:
            TurnOptions_DrawControllers(work);
            break;
        }
        /* the page shows through a band fade lines high, centered on line 0x77 */
        g_drawPos[0] = 0;
        g_drawPos[2] = 0x13f;
        g_drawPos[1] = 0x77 - (short)work->fade / 2;
        g_drawPos[3] = g_drawPos[1] + work->fade - 1;
        Gfx_SetDrawArea(g_drawPos,g_ot + g_otIndex,8);
    }
    if (work->h != 0)
    {
        /* the darkened band */
        g_drawPos[0] = 0;
        g_drawPos[1] = 0x18;
        g_drawPos[2] = 0x140;
        g_boxFill.h = work->h;
        g_boxFill.attribute = GFX_SEMITRANS | 2 << GFX_ABR_SHIFT;
        g_boxFill.x = 0;
        g_boxFill.y = 0x18;
        g_boxFill.w = 0x140;
        g_boxFill.r = TurnOptions_TintColor(g_tintR);
        g_boxFill.g = TurnOptions_TintColor(g_tintG);
        g_boxFill.b = TurnOptions_TintColor(g_tintB);
        g_drawPos[3] = g_boxFill.h;
        GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,8);
    }
    return 0;
}

/* SavePromptWork.page */
#define SAVE_PAGE_BAND   1      /* the darkened band only (the card being checked) */
#define SAVE_PAGE_SLOTS  2      /* the slot list */

/* SavePrompt_Task steps */
enum SavePromptStep
{
    SPROMPT_INIT                   =   0,
    SPROMPT_ASK                    =   2,  /* "Save?" */
    SPROMPT_ANSWER                 =   3,  /* No: done */
    SPROMPT_OPEN                   =   5,  /* the band opens */
    SPROMPT_SAVE_PAGE              =  20,  /* the slot list */
    SPROMPT_SAVE_FADE_IN           =  21,
    SPROMPT_SAVE_CHOOSE            =  22,  /* a free slot is saved at once, a used one after "Overwrite?" */
    SPROMPT_OVERWRITE              =  23,  /* "Overwrite?" */
    SPROMPT_OVERWRITE_ANSWER       =  24,
    SPROMPT_SAVE_WRITE             =  25,  /* Save_Pack */
    SPROMPT_SAVE_RESULT            =  26,  /* "Save completed." / "Save failed." */
    SPROMPT_WAIT_SAVE_RESULT       =  27,
    SPROMPT_SAVE_BACK              =  29,  /* the list closes */
    SPROMPT_CLOSE                  = 100,  /* (not used here) */
    SPROMPT_DONE                   = 101,
    SPROMPT_CARD_CHECK             = 200,  /* Card_SaveTask */
    SPROMPT_WAIT_CARD_CHECK        = 201,
    SPROMPT_CARD_REMOVED           = 210,  /* the card was taken out */
    SPROMPT_CARD_REMOVED_MSG       = 211,  /* "Memory Card was removed. Save failed." */
    SPROMPT_WAIT_CARD_REMOVED_MSG  = 212,
    SPROMPT_SAVING                 = 213,  /* "Saving... Please do not remove Memory Card." */
    SPROMPT_WAIT_SAVING_SHOWN      = 214,
    SPROMPT_SAVING_SHOWN           = 215,
    SPROMPT_SAVING_CLOSE           = 216,
    SPROMPT_WAIT_SAVING_CLOSE      = 217,
    SPROMPT_CARD_REMOVED_MSG2      = 250,  /* (not reached) as SPROMPT_CARD_REMOVED_MSG */
    SPROMPT_WAIT_CARD_REMOVED_MSG2 = 251   /* (not reached) */
};

/* "Save?" Yes/No at the start of a turn in stage mode (playMode 1), then the save screen of
 * TurnOptions_Task's save page: the darkened band opens, the card is checked (Card_SaveTask), the
 * four slots are listed; a free slot is saved at once, a used one after "Overwrite?"; circle
 * closes the list and asks "Save?" again. While the card is watched (busy), taking it out ends
 * with "Memory Card was removed." (0x80064054) */
/* 0x80064054 */
int SavePrompt_Task(short *step, SavePromptWork *work)
{
    PadState *pad = &g_playerPads[g_curPlayer];
    MsgWork *msg;
    int i;

    FntPrint("save p : %d\n", (u16)*step);
    switch(*step)
    {
    case SPROMPT_INIT:
        work->shown = 0;
        work->page = 0;
        work->h = 0;
        work->fade = 0;
        work->slot = 0;
        work->busy = 0;
        for (i = 0; i < 4; i++)
        {
            g_menuRowItems[i] = -1;
        }
        *step = SPROMPT_ASK;
        break;
    case SPROMPT_ASK:
        Msg_AskYesNoCursor(0,0,1,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%CSave?%KYes%KNo%E");
        *step += 1;
        break;
    case SPROMPT_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            if (g_msgAnswer != 0)
            {
                return 1;               /* No */
            }
            *step = SPROMPT_OPEN;
        }
        break;
    case SPROMPT_OPEN:            /* the band opens */
        work->shown = 1;
        work->page = SAVE_PAGE_BAND;
        work->redraw = 0;
        work->fade += 0x20;
        work->h += 0x20;
        if ((short)work->fade >= 0xc0)
        {
            work->h = 0xc0;
            work->fade = 0xc0;
            *step = SPROMPT_CARD_CHECK;
        }
        break;

    /* 20-29: the slot list */
    case SPROMPT_SAVE_PAGE:
        work->busy = 1;
        work->shown = 1;
        work->page = SAVE_PAGE_SLOTS;
        work->redraw = 0;
        work->slot = 0;
        TurnOptions_FreeSlotTexts();
        Save_DescribeCurrent(work->slotFree);
        *step += 1;
        break;
    case SPROMPT_SAVE_FADE_IN:
        work->fade += 0x20;
        if ((short)work->fade >= 0xc0)
        {
            work->fade = 0xc0;
            *step += 1;
        }
        break;
    case SPROMPT_SAVE_CHOOSE:
        if (pad->pressed & PADRdown)
        {
            if (work->slotFree[work->slot] == 1)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = SPROMPT_SAVING;
            }
            else if (work->slotFree[work->slot] == 0)
            {
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step += 1;
            }
        }
        else if (pad->pressed & PADRright)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            work->redraw = 1;
            work->busy = 0;
            *step = SPROMPT_SAVE_BACK;
            Save_FreeBuffer();
        }
        else if (pad->repeat & PADLup)
        {
            work->slot = (work->slot == 0) ? 3 : work->slot - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (pad->repeat & PADLdown)
        {
            work->slot = (work->slot == 3) ? 0 : work->slot + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case SPROMPT_OVERWRITE:
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%COverwrite?%KYes%KNo%E");
        *step += 1;
        break;
    case SPROMPT_OVERWRITE_ANSWER:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = (g_msgAnswer == 0) ? SPROMPT_SAVING : SPROMPT_SAVE_CHOOSE;
        }
        break;
    case SPROMPT_SAVE_WRITE:
        work->busy = 0;
        work->saveFailed = Save_Pack(work->slot,g_curPlayer);
        *step = SPROMPT_SAVING_CLOSE;
        break;
    case SPROMPT_SAVE_RESULT:
        work->busy = 1;
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,
                 (work->saveFailed == 0) ? "%LSave completed.%E" : "%LSave failed.%E");
        *step += 1;
        break;
    case SPROMPT_WAIT_SAVE_RESULT:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->busy = 1;
            TurnOptions_FreeSlotTexts();
            Save_DescribeCurrent(work->slotFree);
            *step = SPROMPT_SAVE_CHOOSE;
        }
        break;
    case SPROMPT_SAVE_BACK:       /* the list closes: "Save?" again */
        work->fade -= 0x20;
        if ((short)work->fade < 1)
        {
            work->fade = 0;
            *step = SPROMPT_ASK;
        }
        break;

    /* 100-101: closing (not used here) */
    case SPROMPT_CLOSE:
        Card_DisableEvents();
        work->fade -= 0x20;
        work->h -= 0x20;
        if ((short)work->fade < 1)
        {
            work->h = 0;
            work->fade = 0;
            *step += 1;
        }
        break;
    case SPROMPT_DONE:
        work->shown = 0;
        TurnOptions_FreeSlotTexts();
        return 1;

    /* 200-201: checking the card */
    case SPROMPT_CARD_CHECK:
        work->busy = 0;
        Task_Start(g_cardTaskId,Card_SaveTask);
        *step += 1;
        break;
    case SPROMPT_WAIT_CARD_CHECK:
        if (Task_IsDone(g_cardTaskId))
        {
            if (g_cardTaskState == CARDTASK_CANCELED)
            {
                work->redraw = 1;
                *step = SPROMPT_SAVE_BACK;
                work->busy = 0;
            }
            else if (g_cardTaskState == CARDTASK_REMOVED)
            {
                *step = SPROMPT_CARD_CHECK;
                work->busy = 0;
            }
            else
            {
                work->busy = 1;
                *step = SPROMPT_SAVE_PAGE;
            }
        }
        break;

    /* 210-212: the card was taken out */
    case SPROMPT_CARD_REMOVED:
        if (!Task_IsDone(g_uiTaskIds[0]) && g_tasks[(u16)g_uiTaskIds[0]].step < MSG_STEP_CLOSE)
        {
            g_tasks[(u16)g_uiTaskIds[0]].step = MSG_STEP_CLOSE;  /* the open message closes */
        }
        *step += 1;
        break;
    case SPROMPT_CARD_REMOVED_MSG:
    case SPROMPT_CARD_REMOVED_MSG2:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,"%CMemory Card was removed.%KSave failed.%E");
            *step += 1;
        }
        break;
    case SPROMPT_WAIT_CARD_REMOVED_MSG:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->redraw = 1;
            *step = SPROMPT_SAVE_BACK;
            Save_FreeBuffer();
        }
        break;
    case SPROMPT_WAIT_CARD_REMOVED_MSG2:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            work->redraw = 1;
            *step = SPROMPT_SAVE_BACK;
        }
        break;

    /* 213-217: saving */
    case SPROMPT_SAVING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CSaving\201c%KPlease do not remove Memory Card.%E");
        *step += 1;
        break;
    case SPROMPT_WAIT_SAVING_SHOWN:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state == TEXTWIN_SHOWN)
        {
            *step += 1;               /* the message is up */
        }
        break;
    case SPROMPT_SAVING_SHOWN:
        *step = SPROMPT_SAVE_WRITE;
        break;
    case SPROMPT_SAVING_CLOSE:
        Task_SetStep(g_uiTaskIds[0],MSG_STEP_CLOSE);
        *step += 1;
        break;
    case SPROMPT_WAIT_SAVING_CLOSE:
        if (Task_IsDone(g_uiTaskIds[0]))
        {
            *step = SPROMPT_SAVE_RESULT;
        }
        break;
    }

    if (work->busy != 0)
    {
        Card_EnableEvents();
        Card_Check();
        if (CARD_FAILED(g_cardStatus))
        {
            work->busy = 0;
            *step = SPROMPT_CARD_REMOVED;
        }
        Card_DisableEvents();
    }
    if (work->shown == 0)
    {
        return 0;
    }
    if (work->fade != 0)
    {
        Gfx_SetDrawArea(0,g_ot + g_otIndex,8);
        if (work->page == SAVE_PAGE_SLOTS)
        {
            Save_DrawSlots(work->slot);
        }
        /* the list shows through a band fade lines high, centered on line 0x77 */
        g_drawPos[0] = 0;
        g_drawPos[2] = 0x13f;
        g_drawPos[1] = 0x77 - (short)work->fade / 2;
        g_drawPos[3] = g_drawPos[1] + work->fade - 1;
        Gfx_SetDrawArea(g_drawPos,g_ot + g_otIndex,8);
    }
    if (work->h != 0)
    {
        /* the darkened band */
        g_drawPos[0] = 0;
        g_drawPos[1] = 0x18;
        g_drawPos[2] = 0x140;
        g_boxFill.h = work->h;
        g_boxFill.attribute = GFX_SEMITRANS | 2 << GFX_ABR_SHIFT;
        g_boxFill.x = 0;
        g_boxFill.y = 0x18;
        g_boxFill.w = 0x140;
        g_boxFill.r = TurnOptions_TintColor(g_tintR);
        g_boxFill.g = TurnOptions_TintColor(g_tintG);
        g_boxFill.b = TurnOptions_TintColor(g_tintB);
        g_drawPos[3] = g_boxFill.h;
        GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,8);
    }
    return 0;
}
