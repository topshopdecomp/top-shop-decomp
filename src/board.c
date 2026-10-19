/*
 * board.c - game mode 6, the board game: Board_Task and its turn phases
 *
 * Board_Task is one large state machine on *step, split by step range into phase functions (see
 * include/board.h). Every frame Board_Task calls the phases in order; a phase returns 0 for
 * steps outside its range, 1 to end the mode, or 2 to restart the dispatch in the same frame.
 *
 * Turn flow (details in docs/board.md):
 *   setup (0-300) -> round start / calendar (2000-2200) -> turn start (2300-2500)
 *   -> "<name>'s turn." (3100) -> CPU talk script (3200) -> command menu (3310)
 *   -> dice (3800) -> [event (4000)] -> move (6000) -> square action (8000)
 *   -> settle money (8900) -> end of turn (9000) -> assets / win check (10000)
 *   -> next player (10400) -> 2000 ...  (all five phases are in this file)
 */
#include "common.h"
#include "topshop.h"

#define CUR (&g_players[g_curPlayer])

/* A CPU player's talk at the start and the end of its turn (steps 3200, 9100). The game plays it
 * only while g_optSkipCpuTalk is 0, but sets it to 1 at start-up and offers no option to change it,
 * so the retail game never shows the talk. A build with CPU_TALK defined (make CPU_TALK=1) plays it
 * whatever g_optSkipCpuTalk says (which stays as it is, so the saves are unchanged). The talk waits
 * for its XA voice to end, which needs the voices' real positions (Xa_Play, src/fileio.c); without
 * them, as in the retail game with g_optSkipCpuTalk at 0, the talk never ends. */
#ifdef CPU_TALK
#define CPU_TALK_ON  1
#else
#define CPU_TALK_ON  (!g_optSkipCpuTalk)
#endif

/* The sectors a talk script is unpacked into. The game takes the size of the packed script rounded
 * up to whole sectors, which the talk scripts on the disc already overrun by up to 416 bytes when
 * unpacked. A CPU_TALK build, which plays them, gives every one 2 sectors (4096 bytes; tools/tss.py
 * build stops a talk script that unpacks to more). */
#ifdef CPU_TALK
#define TALK_SCRIPT_SECTORS(packed)  2
#else
#define TALK_SCRIPT_SECTORS(packed)  (((packed) + 0x7FF) / 0x800)
#endif

/* RULE_AUTO_*: let the CPU logic answer this dialog for the current player */
#define AUTO_BEGIN(rule) \
    if (g_boardRules.enable[rule]) { w->savedIsCpu = CUR->isCpu; CUR->isCpu = 1; }
#define AUTO_END(rule) \
    if (g_boardRules.enable[rule]) CUR->isCpu = w->savedIsCpu

/* Index of a store owned by the current player whose squareId is the square the
 * player stands on, or numStores if there is none. */
static int FindOwnStoreHere(void)
{
    Store *s = g_stores;
    int i;

    for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++, s++)
    {
        if (s->owner == g_curPlayer &&
            s->squareId == Board_GetSquareId(g_players[s->owner].x,
                                             g_players[s->owner].floor))
            break;
    }
    return i;
}

/* Players still in the game that have reached the goal -> g_winners. */
static void CollectWinners(void)
{
    int i;

    g_numWinners = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (!g_players[i].out && g_gameConfig.goalMoney <= g_players[i].assets)
            g_winners[g_numWinners++] = i;
    }
}

/* Draw the full-screen event picture (VRAM 448,264, CLUT row 0x1EC). */
static void DrawEventPicture(void)
{
    VramImage img;
    short pos[2];

    img.mode = 1;
    img.x = 0x1C0;
    img.y = 0x108;
    img.w = 320;
    img.h = 240;
    img.clutY = 0x1EC;
    img.clutX = 0;
    pos[0] = pos[1] = 0;
    Gfx_DrawVramImage(&img, 0, pos, CUR_OT, 12);
}

/* Show or hide the board display tasks around full-screen actions. */
static void HideBoardHud(void)
{
    Task_SendMessage(g_minimapTaskId,DISP_HIDE);
    Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
    Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
    Task_SendMessage(g_calendarTaskId,DISP_HIDE);
}

static void ShowBoardHud(void)
{
    Task_SendMessage(g_minimapTaskId,DISP_SHOW);
    Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
    Task_SendMessage(g_squareInfoTaskId,DISP_SHOW);
    Task_SendMessage(g_calendarTaskId,DISP_SHOW);
}

/* File id of the calendar page for a month (0 = April): CAL_01..CAL_12. */
static int CalendarFile(int m)
{
    if (m + 3 >= 12)
        return m - 6;           /* = FILE_CAL_01_J + (m + 3 - 12) */
    if (m + 3 < 0)
        return m + 18;          /* unreachable for valid months */
    return m + 6;               /* = FILE_CAL_01_J + (m + 3) */
}

/* 0x800432F0 */
int Board_Task(short *step, void *work)
{
    int r;

    Gfx_QueueClear(0, 0, 0);
    FntPrint("~cfff");
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", (unsigned short)*step);
    FntPrint("com p use : %d, %d\n", g_debugComUseA, g_debugComUseB);
    g_debugPrint = 1;
    Board_FrameStub();
    g_gameConfig.demoTimer--;
    g_boardStepCopy = *step;

    for (;;)
    {
        r = Board_PhaseSetup((unsigned short *)step, work);
        if (r == 2) continue;
        if (r == 1) return 1;
        r = Board_PhaseTurn((unsigned short *)step, work);
        if (r == 2) continue;
        if (r == 1) return 1;
        r = Board_PhaseMove(step, work);
        if (r == 2) continue;
        if (r == 1) return 1;
        r = Board_PhaseSquare((unsigned short *)step, work);
        if (r == 2) continue;
        if (r == 1) return 1;
        r = Board_PhaseEndTurn((unsigned short *)step, work);
        if (r == 2) continue;
        break;
    }
    if (r == 1)
        return 1;

    /* attract demo: any of START/circle/cross/square ends it; draw the demo label */
    if (g_gameConfig.playMode == PLAY_DEMO && (unsigned short)*step > STEP_LOAD_BOARD)
    {
        if (g_playerPads[6].held & (PADstart | PADRright | PADRdown | PADRleft))
            *step = STEP_GAME_OVER;
        g_drawPos[0] = 0xB0;
        g_drawPos[1] = 0xC0;
        Sprite_DrawAligned(g_spriteGroups[2].base + 6, 0, PAL_DEFAULT, g_drawPos,
                           ALIGN_LEFT | ALIGN_TOP, CUR_OT, 1);
        g_drawPos[0] = g_sprites[g_spriteGroups[2].base + 6].w + 0xB0;
        Sprite_DrawAligned(g_spriteGroups[2].base + 7, 0, PAL_DEFAULT, g_drawPos,
                           ALIGN_LEFT | ALIGN_TOP, CUR_OT, 1);
    }
    if (g_showEventPicture == 1)
        DrawEventPicture();
    return 0;
}

/* 0x800435E4  steps 0..300: load the board and play the stage intro */
int Board_PhaseSetup(unsigned short *step, void *work)
{
    BoardWork *w = (BoardWork *)work;
    unsigned short next;

    switch (*step)
    {
    case STEP_SETUP:
        w->savePromptArmed = 0;
        g_showEventPicture = 0;
        g_frontOtOn = 0;
        g_frontOtUsed = 0;
        g_boardOtOn = 0;
        g_boardOtUsed = 0;
        g_boardUnused88 = 0;
        g_boardUnused8C = 0;
        Board_CreateTasks();
        Task_Start(g_auxTaskId, Game_LoadCommonTask);
        next = *step + 1;
        break;
    case STEP_SETUP_WAIT_COMMON:
        if (!Task_IsDone(g_auxTaskId))
            return 0;
        next = *step + 1;
        break;
    case STEP_SETUP_NEXT:
        next = STEP_LOAD_BOARD;
        break;
    case STEP_LOAD_BOARD:
        Board_InitGame();
        Board_LoadData();
        Gfx_EnableRender();
        next = *step + 1;
        break;
    case STEP_LOAD_SEASON:
        Board_LoadSeasonGraphics(SEASON(g_gameConfig.month));
        next = STEP_START_DISPLAY;
        break;
    case STEP_START_DISPLAY:
        Board_StartDisplayTasks();
        Task_Start(g_frameEndTaskId, Board_FrameEnd);
        Fade_SetBlack();
        *step = STEP_INTRO;
        return 0;

    case STEP_INTRO:                        /* stage intro only in stage mode, new game */
        next = STEP_BOARD_MUSIC;
        if (g_gameConfig.playMode == PLAY_STAGE && !g_resumedGame)
        {
            Board_SuspendDisplayTasks();
            next = *step + 1;
        }
        break;
    case STEP_INTRO_LOAD:
        Board_LoadBackground(g_boardStage);
        Fade_SetBlack();
        *g_loaderWork = FILE_CNT00_J + g_boardStage;
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        *step = STEP_INTRO_WAIT_LOAD;
        return 0;
    case STEP_INTRO_WAIT_LOAD:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_INTRO_SCRIPT:                 /* intro script; START skips */
        DrawEventPicture();
        if (g_playerPads[6].pressed & PADstart)
            Task_SendMessage(g_scriptTaskId, 0);
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        next = *step + 1;
        break;
    case STEP_INTRO_FADE_OUT:
        Fade_Out(8);
        next = *step + 1;
        break;
    case STEP_INTRO_WAIT_FADE:
        if (!Fade_IsDone())
            return 0;
        *step = STEP_INTRO_END;
        return 0;
    case STEP_INTRO_END:
        Board_LoadSeasonGraphics(SEASON(g_gameConfig.month));
        Board_ResumeDisplayTasks();
        Board_FocusCurrentPlayer();
        next = STEP_BOARD_MUSIC;
        break;

    case STEP_BOARD_MUSIC:                  /* board music, unless the calendar will start it */
        if (g_resumedGame || !g_optEventScenes || g_gameConfig.monthsElapsed != 0 ||
            g_gameConfig.month != 0)
            Bgm_Play(g_boardStage + 7, 0, 0x7F, 0x7F);
        next = STEP_TURN;
        break;

    default:
        return 0;
    }
    *step = next;
    return 0;
}

/* 0x80043A98  steps 2000..4999: start of round/turn, command menu, dice, events */
int Board_PhaseTurn(unsigned short *step, void *work)
{
    BoardWork *w = (BoardWork *)work;
    unsigned short next;
    int *src, *diceWork, r, n;

    switch (*step)
    {

    /* ---- start of a round (player 0) ---- */
    case STEP_TURN:
        Sys_ResetFonts();
        next = STEP_TURN_START;
        if (!g_resumedGame && g_curPlayer == 0)
        {
            next = STEP_CALENDAR;           /* month calendar scene */
            if (!g_optEventScenes)
            {
                Board_Unused(g_gameConfig.month);
                Board_LoadSeasonGraphics(SEASON(g_gameConfig.month));
                next = STEP_RECORD_ASSETS;
            }
        }
        break;
    case STEP_CALENDAR:
        if (g_gameConfig.monthsElapsed == 0 && g_gameConfig.month == 0)
            Bgm_Play(6, 0, 0x7F, 0x7F);
        Fade_SetBlack();
        Board_SuspendDisplayTasks();
        next = *step + 1;
        break;
    case STEP_CALENDAR_GRAPHICS:
        Board_LoadMonthGraphics(g_gameConfig.month);
        Fade_SetBlack();
        next = STEP_CALENDAR_LOAD;
        break;
    case STEP_CALENDAR_LOAD:
        *g_loaderWork = CalendarFile((signed char)g_gameConfig.month);
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        next = STEP_CALENDAR_WAIT_LOAD;
        break;
    case STEP_CALENDAR_WAIT_LOAD:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        Task_Start(g_scriptTaskId, EventScript_Task);
        (*step)++;
        g_showEventPicture = 1;
        return 0;
    case STEP_CALENDAR_SCRIPT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        g_showEventPicture = 0;
        next = STEP_CALENDAR_END;
        break;
    case STEP_CALENDAR_END:
        if (g_gameConfig.monthsElapsed == 0 && g_gameConfig.month == 0)
            Bgm_Play(g_boardStage + 7, 0, 0x7F, 0x7F);
        Board_LoadSeasonGraphics(SEASON(g_gameConfig.month));
        Board_ResumeDisplayTasks();
        Board_FocusCurrentPlayer();
        next = STEP_RECORD_ASSETS;
        break;
    case STEP_RECORD_ASSETS:
        for (n = 0; n < g_numPlayers; n++)
            Player_RecordAssets(n);
        next = STEP_TURN_START;
        break;

    /* ---- start of a player's turn ---- */
    case STEP_TURN_START:
        Board_ExpireEffects();
        if (CUR->out)
        {
            next = STEP_NEXT_PLAYER;        /* skip players who are out */
            break;
        }
        for (n = 18; n >= 0; n--)
            g_turnFlags[n] = 0;
        Board_LoadPieceSprites(CUR->charId);
        for (n = 0; n < g_numPlayers; n++)
        {
            g_players[n].pieceAnim = 0;
            g_players[n].pieceFlip = 0;
        }
        Board_FocusCurrentPlayer();
        next = STEP_TURN_WIPE;
        break;
    case STEP_TURN_WIPE:
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_NOW);
        Task_SendMessage(g_minimapTaskId,DISP_SHOW_NOW);
        Task_SendMessage(g_calendarTaskId,DISP_SHOW_NOW);
        Task_SendMessage(g_squareInfoTaskId,DISP_SHOW_NOW);
        Fade_SetClear();
        Task_Start(g_turnFxTaskId, Wipe_OpenTask);
        next = *step + 1;
        break;
    case STEP_TURN_WAIT_WIPE:
        if (!Task_IsDone(g_turnFxTaskId))
            return 0;
        next = (g_curPlayer == 0) ? STEP_SAVE_PROMPT : STEP_TURN_BEGIN;
        break;
    case STEP_SAVE_PROMPT:                  /* yearly save prompt in stage mode */
        if (g_gameConfig.playMode == PLAY_STAGE && g_gameConfig.month == 0 && w->savePromptArmed)
        {
            HideBoardHud();
            Task_Start(g_actionTaskIds[0], SavePrompt_Task);
            next = *step + 1;
        }
        else
        {
            w->savePromptArmed = 1;
            next = STEP_TURN_BEGIN;
        }
        break;
    case STEP_WAIT_SAVE_PROMPT:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        ShowBoardHud();
        *step = STEP_TURN_BEGIN;
        return 0;

    case STEP_TURN_BEGIN:
        next = (g_numPlayers == 0) ? STEP_ASSETS : STEP_ANNOUNCE;
        break;
    case STEP_ANNOUNCE:                     /* "<name>'s turn." */
        if (g_optCpuFast < 1)
        {
            strcpy(g_msgBuf, "%C");
            strcat(g_msgBuf, CUR->name);
            strcat(g_msgBuf, "'s turn.");
            MSG_SHOW(g_msgBuf);
            next = *step + 1;
        }
        else
        {
            next = STEP_START_TALK;
        }
        break;
    case STEP_WAIT_ANNOUNCE:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_START_TALK;
        return 0;
    case STEP_BANK_DISTANCE:                /* "There are N steps to the bank." (unreachable: nothing sets this step) */
        if (g_boardRules.enable[RULE_RESET_DIR])
        {
            CUR->dirX = 0;
            CUR->dirFloor = 0;
        }
        Player_FindNearestBank((signed char)g_curPlayer);
        strcpy(g_msgBuf, "%CThere are ");
        Str_FormatInt(g_bankDistance, 2, g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf, " steps to the bank.");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_WAIT_BANK_DISTANCE:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_START_TALK;
        return 0;

    case STEP_START_TALK:                   /* character talk at turn start */
        if (g_gameConfig.playMode == PLAY_DEMO)
            next = STEP_COMMANDS;
        else if (g_boardRules.enable[RULE_ALWAYS_TALK])
            next = *step + 1;
        else if (CUR->isCpu && CPU_TALK_ON)
            next = *step + 1;
        else
            next = STEP_COMMANDS;
        break;
    case STEP_START_TALK_SCRIPT:
    {
        unsigned char *base = g_secHeapBlocks[g_charScriptsBlock].addr;

        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
        /* .J entry charId of the character script archive, LZSS-compressed */
        src = (int *)(base + ((int *)base)[(signed char)CUR->charId + 1]);
        g_scriptBlock = SecHeap_Alloc(TALK_SCRIPT_SECTORS(src[0]));
        Lzss_Decompress(src, g_secHeapBlocks[g_scriptBlock].addr);
        g_scriptData = g_secHeapBlocks[g_scriptBlock].addr;
        g_scriptEntry = TurnStart_PickScript(g_curPlayer);
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    }
    case STEP_START_TALK_WAIT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        Task_SendMessage(g_minimapTaskId,DISP_SHOW);
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        SecHeap_Free(g_scriptBlock);
        next = *step + 1;
        break;
    case STEP_START_TALK_WAIT_XA:
        if (Cd_IsXaPlaying())
            return 0;
        *step = STEP_COMMANDS;
        return 0;

    /* ---- command menu ---- */
    case STEP_COMMANDS:
        if (g_optCpuFast >= 1 && CUR->isCpu)
        {
            next = STEP_ROLL;               /* fast CPU: roll straight away */
        }
        else
        {
            g_turnMenuChoice = TURNMENU_ROLL;
            next = STEP_COMMAND_MENU;
        }
        break;
    case STEP_COMMAND_MENU:
        Task_Start(g_actionTaskIds[0], TurnMenu_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_COMMAND_MENU:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        *step = STEP_COMMAND_CHOSEN;
        return 0;
    case STEP_COMMAND_CHOSEN:
        switch (g_turnMenuChoice)
        {
        case TURNMENU_ROLL:    w->rollCancelable = 1; next = STEP_ROLL; break;
        case TURNMENU_MAP:     next = STEP_MAP_VIEW; break;
        case TURNMENU_STATUS:  next = STEP_STATUS; break;
        case TURNMENU_GRAPH:   next = STEP_ASSET_GRAPH; break;
        case TURNMENU_OPTIONS: next = STEP_OPTIONS; break;
        default:               return 0;
        }
        break;
    case STEP_CLOSED_TODAY:                 /* (unreachable: nothing sets this step) */
        MSG_SHOW("You are closed today.");
        next = *step + 1;
        break;
    case STEP_WAIT_CLOSED_TODAY:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_END_TURN;
        return 0;
    case STEP_MAP_VIEW:                     /* menu option 1 */
        Task_SendMessage(g_minimapTaskId,DISP_MOVE_DOWN);
        Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_Start(g_actionTaskIds[0], Board_MapViewTask);
        next = *step + 1;
        break;
    case STEP_WAIT_MAP_VIEW:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        Task_SendMessage(g_minimapTaskId,DISP_MOVE_UP);
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        Task_SendMessage(g_squareInfoTaskId,DISP_SHOW);
        Task_SendMessage(g_calendarTaskId,DISP_SHOW);
        next = STEP_COMMAND_MENU;
        break;
    case STEP_STATUS:                       /* menu option 2 */
        HideBoardHud();
        Task_Start(g_uiTaskIds[7], Status_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_STATUS:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        ShowBoardHud();
        next = STEP_COMMAND_MENU;
        break;
    case STEP_ASSET_GRAPH:                  /* menu option 3 */
        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_Start(g_actionTaskIds[0], AssetGraph_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_ASSET_GRAPH:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        Task_SendMessage(g_minimapTaskId,DISP_SHOW);
        Task_SendMessage(g_squareInfoTaskId,DISP_SHOW);
        Task_SendMessage(g_calendarTaskId,DISP_SHOW);
        next = STEP_COMMAND_MENU;
        break;
    case STEP_OPTIONS:                      /* menu option 4: options / save */
        HideBoardHud();
        Task_Start(g_actionTaskIds[0], TurnOptions_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_OPTIONS:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        ShowBoardHud();
        next = STEP_COMMAND_MENU;
        break;

    /* ---- dice ---- */
    case STEP_ROLL:
        Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
        Task_SendMessage(g_minimapTaskId,DISP_MOVE_DOWN);
        Task_GetWork(g_diceTaskId, (void **)&diceWork);
        *diceWork = (signed char)w->rollCancelable;
        Task_Start(g_diceTaskId, Dice_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_ROLL:
        if (!Task_IsDone(g_diceTaskId))
            return 0;
        if (g_diceResult == DICE_RESULT_CANCELED)  /* canceled: back to the menu */
        {
            Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
            Task_SendMessage(g_minimapTaskId,DISP_MOVE_UP);
            next = STEP_COMMAND_MENU;
        }
        else if (g_diceResult == DICE_RESULT_EVENT)  /* special result: event */
        {
            next = STEP_EVENT;
        }
        else
        {
            CUR->stepsLeft = g_diceResult + 1;
            CUR->steps = g_diceResult + 1;
            next = STEP_MOVE;
        }
        break;

    /* ---- events (EVE_nnn.J scripts) ---- */
    case STEP_EVENT:
        next = STEP_SETTLE;
        if (g_boardRules.enable[RULE_EVENTS])
        {
            Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
            Task_SendMessage(g_calendarTaskId,DISP_HIDE);
            Task_SendMessage(g_minimapTaskId,DISP_HIDE);
            Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
            next = STEP_EVENT_PICK;
        }
        break;
    case STEP_EVENT_PICK:
        if (!g_boardRules.enable[RULE_PICK_EVENT])
        {
            w->eventId = Event_PickRandom(g_curPlayer);
            g_eventDrawn = 1;
            g_eventId = w->eventId;
            Task_Start(g_loaderTaskId, EventCard_LoadTask);
            next = STEP_EVENT_FLAGS;
        }
        else
        {
            Task_Start(g_uiTaskIds[7], EventPick_Task);
            next = *step + 1;
        }
        break;
    case STEP_EVENT_WAIT_PICK:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        w->eventId = Task_GetUserData(g_uiTaskIds[7]);
        g_eventDrawn = 1;
        g_eventId = w->eventId;
        Task_Start(g_loaderTaskId, EventCard_LoadTask);
        next = STEP_EVENT_FLAGS;
        break;
    case STEP_EVENT_FLAGS:                  /* event category flags */
        switch (w->eventId)
        {
        case EVE_SELL_150:
        case EVE_SELL_200:
        case EVE_SELL_250:
        case EVE_RIVALS_CLOSED:
        case EVE_ALL_CLOSED_2:
        case EVE_RIVALS_GIVE:
        case EVE_RIVALS_GIVE_1_5:
        case EVE_SANTA_X3:
        case EVE_SANTA_X5:
        case EVE_SANTA_X7:
        case EVE_STOP:
        case EVE_ROLL_AGAIN:
        case EVE_ANOTHER:
        case EVE_VACANT_LOT:
        case EVE_ALL_EMPTY_CLOSE:
        case EVE_RIVALS_EMPTY_CLOSE:
        case EVE_ALL_NO_ELEVATOR:
        case EVE_RIVALS_NO_ELEVATOR:
        case EVE_CHANCE_QUIZ:
        case EVE_SHOES:
        case EVE_SPEED_QUIZ:
        case EVE_PRICES_UP:
            g_turnFlags[TURN_EVENT] = 1;
            break;
        case EVE_MOVE_1:
        case EVE_MOVE_2:
        case EVE_MOVE_3:
        case EVE_MOVE_4:
        case EVE_MOVE_5:
        case EVE_MOVE_6:
        case EVE_MOVE_7:
        case EVE_MOVE_8:
            break;
        default:
            g_turnFlags[TURN_QUIET_EVENT] = 1;
            break;
        }
        next = STEP_EVENT_CARD;
        break;
    case STEP_EVENT_CARD:
        g_uiTaskWork[7][1] = w->eventId;
        Task_Start(g_uiTaskIds[7], EventCard_Task);
        next = *step + 1;
        break;
    case STEP_EVENT_WAIT_CARD:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        *step = STEP_EVENT_SCRIPT;
        return 0;
    case STEP_EVENT_SCRIPT:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_EVENT_WAIT_SCRIPT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        Bgm_Play(g_boardStage + 7, 0, 0x7F, 0x7F);
        r = Task_GetUserData(g_scriptTaskId);
        switch (r)
        {
        case EVENT_RESULT_NONE:    next = STEP_SETTLE; break;
        case EVENT_RESULT_ANOTHER: next = STEP_EVENT; break;
        case EVENT_RESULT_REROLL:  w->rollCancelable = 0; next = STEP_ROLL; break;
        case EVENT_RESULT_MOVE:    /* the steps set by the script */
            Task_SendMessage(g_squareInfoTaskId,DISP_SHOW);
            Task_SendMessage(g_calendarTaskId,DISP_SHOW);
            Task_SendMessage(g_minimapTaskId,DISP_SHOW_LOW);
            next = STEP_MOVE;
            break;
        default:
            return 0;
        }
        break;

    default:
        return 0;
    }
    *step = next;
    return 0;
}

/* 0x80044F24  steps 6000..6001: move the current player's piece */
int Board_PhaseMove(short *step, void *work)
{
    BoardWork *w = (BoardWork *)work;
    short next;

    if (*step == STEP_MOVE)
    {
        if (g_boardRules.enable[RULE_RESET_DIR])
        {
            CUR->dirX = 0;
            CUR->dirFloor = 0;
        }
        if (g_boardRules.enable[RULE_AUTO_MOVE])
        {
            w->savedIsCpu = CUR->isCpu;
            CUR->isCpu = 1;
        }
        Task_Start(g_moveTaskId, Move_Task);
        next = *step + 1;
    }
    else if (*step == STEP_WAIT_MOVE)
    {
        if (!Task_IsDone(g_moveTaskId))
            return 0;
        if (g_boardRules.enable[RULE_AUTO_MOVE])
            CUR->isCpu = w->savedIsCpu;
        next = (Task_GetUserData(g_moveTaskId) == 0) ? STEP_SQUARE : STEP_SETTLE;
    }
    else
    {
        return 0;
    }
    *step = next;
    return 0;
}

/* 0x800450B4  steps 8000..8999: act on the square the player landed on */
int Board_PhaseSquare(unsigned short *step, void *work)
{
    BoardWork *w = (BoardWork *)work;
    unsigned short next;
    unsigned char cpu, cursor;
    short cell;
    Store *st;
    int n;

    switch (*step)
    {
    case STEP_SQUARE:
        Task_SendMessage(g_squareInfoTaskId,DISP_HIDE);
        Task_SendMessage(g_calendarTaskId,DISP_HIDE);
        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        next = STEP_SQUARE_DISPATCH;
        break;
    case STEP_SQUARE_DISPATCH:              /* dispatch on the square type */
        if (!Task_IsDone(g_uiTaskIds[0]))
            return 0;
        cell = Board_GetCell(CUR->x, CUR->floor);
        switch (CELL_TYPE(cell))
        {
        case CELL_STORE:
            Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
            if (!g_boardRules.enable[RULE_STORES])
                next = STEP_SETTLE;
            else if (g_stores[cell].owner == -1)
                next = STEP_VACANT;
            else if (g_stores[cell].owner == g_curPlayer)
                next = STEP_OWN_STORE;
            else
                next = STEP_RIVAL_STORE;    /* someone else's */
            break;
        case CELL_ELEVATOR:
            if (g_boardRules.enable[RULE_ELEVATOR])
            {
                *step = STEP_ELEVATOR;
                return 0;
            }
            next = STEP_END_TURN;
            break;
        case CELL_BANK:
            if (g_boardRules.enable[RULE_BANK])
            {
                *step = STEP_BANK;
                return 0;
            }
            next = STEP_END_TURN;
            break;
        case CELL_UNUSED5:
            next = STEP_END_TURN;
            break;
        default:                            /* CELL_NONE and anything else */
            *step = STEP_SETTLE;
            return 0;
        }
        break;

    /* ---- elevator ---- */
    case STEP_ELEVATOR:
        if (CUR->noElevator)
        {
            *step = STEP_ELEVATOR_BROKEN;
            g_turnFlags[TURN_NO_ELEVATOR] = 1;
            return 0;
        }
        AUTO_BEGIN(RULE_AUTO_ELEVATOR);
        if (CUR->isCpu)
        {
            cursor = Cpu_ElevatorChoice(g_curPlayer, Board_GetCell(CUR->x, CUR->floor) & 0xFF);
            cpu = CUR->isCpu;
        }
        else
        {
            cursor = 0;
            cpu = 0;
        }
        Msg_AskYesNo(cpu, cursor, MSG_AUTO_POS, MSG_AUTO_POS, 0x1E, 3,
                     "%CUse the elevator?%KYes%KNo%E");
        next = *step + 1;
        break;
    case STEP_ELEVATOR_ANSWER:
        if (Msg_IsAnswered())
        {
            AUTO_END(RULE_AUTO_ELEVATOR);
            if (g_msgAnswer == 0)
            {
                *step = STEP_ELEVATOR_RIDE;
                g_turnFlags[TURN_ELEVATOR] = 1;
            }
            else
            {
                *step = STEP_SETTLE;
                g_turnFlags[TURN_NO_ELEVATOR] = 1;
            }
        }
        Board_FocusCurrentPlayer();
        return 0;
    case STEP_ELEVATOR_RIDE:
        Task_Start(g_actionTaskIds[0], Elevator_Task);
        next = *step + 1;
        break;
    case STEP_ELEVATOR_WAIT_RIDE:
        if (Task_IsDone(g_actionTaskIds[0]))
            (*step)++;
        Board_FocusCurrentPlayer();
        return 0;
    case STEP_ELEVATOR_FLOOR:               /* "<n>F." */
        CUR->screenY = (g_stageDefs[g_boardStage].originY + CUR->floor * 5) * 16 + 0x3A;
        Board_FocusCurrentPlayer();
        strcat(g_msgBuf, "%C");             /* sic: appends to the buffer's current contents */
        Str_FormatInt(g_stageDefs[g_boardStage].floors - CUR->floor, 1,
                      g_msgBuf + strlen(g_msgBuf));
        strcat(g_msgBuf, "F.%E");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_ELEVATOR_WAIT_FLOOR:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_SETTLE;
        return 0;
    case STEP_ELEVATOR_BROKEN:
        strcpy(g_msgBuf, "%CThe elevator is out of order.");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_ELEVATOR_WAIT_BROKEN:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_SETTLE;
        return 0;

    /* ---- the bank (CELL_BANK) ---- */
    case STEP_BANK:
        g_turnFlags[TURN_BANK] = 1;
        CUR->dirX = 0;
        CUR->dirFloor = 0;
        next = STEP_SETTLE;
        break;
    case STEP_SQUARE_UNUSED:                /* (unreachable: nothing sets this step) */
        next = STEP_SETTLE;
        break;

    /* ---- vacant store ---- */
    case STEP_VACANT:
        next = STEP_SETTLE;
        if (CUR->exempt == 0)
        {
            n = FindOwnStoreHere();
            if (CUR->vandalized)
                next = STEP_VACANT_VANDALIZED;
            else if (n == g_stageDefs[g_boardStage].numStores)
                next = STEP_VACANT_LOT;
            else
                next = STEP_VACANT_OFFER;
        }
        break;
    case STEP_VACANT_LOT:
        AUTO_BEGIN(RULE_AUTO_VACANT);
        g_uiTaskWork[7][1] = LOT_VACANT;
        Task_Start(g_uiTaskIds[7], VacantLot_Task);
        next = *step + 1;
        break;
    case STEP_VACANT_WAIT_LOT:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        AUTO_END(RULE_AUTO_VACANT);
        next = STEP_VACANT_SHOP_OPEN;
        break;
    case STEP_VACANT_OFFER:
        FindOwnStoreHere();                 /* sic: result unused in the original */
        AUTO_BEGIN(RULE_AUTO_STORE);
        g_uiTaskWork[7][1] = LOT_VACANT;
        Task_Start(g_uiTaskIds[7], StoreOffer_Task);
        next = *step + 1;
        break;
    case STEP_VACANT_WAIT_OFFER:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        AUTO_END(RULE_AUTO_STORE);
        next = STEP_VACANT_SHOP_OPEN;
        break;
    case STEP_VACANT_SHOP_OPEN:
        Task_Start(g_actionTaskIds[0], ShopOpen_Task);
        next = *step + 1;
        break;
    case STEP_VACANT_WAIT_SHOP_OPEN:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        *step = STEP_SETTLE;
        return 0;
    case STEP_VACANT_VANDALIZED:
        strcpy(g_msgBuf, "This store can't be opened%Kdue to vandalism.");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_VACANT_WAIT_VANDALIZED:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_SETTLE;
        return 0;

    /* ---- someone else's store ---- */
    case STEP_RIVAL_STORE:
        st = &g_stores[Board_GetCell(CUR->x, CUR->floor)];
        if (CUR->exempt == 0)
        {
            next = STEP_RIVAL_BUYOUT;
            if (st->numGoods != 0)
            {
                if (st->tempClosed)
                {
                    *step = STEP_RIVAL_TEMP_CLOSED;
                    g_turnFlags[TURN_STORE_CLOSED] = 1;
                    return 0;
                }
                *step = STEP_RIVAL_SHOP;
                if (st->priceChanged != 0 && st->pricePercent <= 99)
                    g_turnFlags[TURN_STORE_SALE] = 1;
                return 0;
            }
        }
        else if (st->numGoods == 0)
        {
            next = STEP_RIVAL_CLOSED;
        }
        else
        {
            next = st->tempClosed ? STEP_RIVAL_TEMP_CLOSED : STEP_RIVAL_STEAL;
        }
        break;
    case STEP_RIVAL_SHOP:
        AUTO_BEGIN(RULE_AUTO_OTHERS);
        Task_Start(g_uiTaskIds[7], Shopping_Task);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_SHOP:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        AUTO_END(RULE_AUTO_OTHERS);
        next = (g_stores[Board_GetCell(CUR->x, CUR->floor)].numGoods == 0) ? STEP_RIVAL_BUYOUT : STEP_RIVAL_DONE;
        break;
    case STEP_RIVAL_BUYOUT:
        n = FindOwnStoreHere();
        if (CUR->vandalized)
            next = STEP_RIVAL_VANDALIZED;
        else if (n == g_stageDefs[g_boardStage].numStores)
            next = STEP_RIVAL_BUYOUT_LOT;
        else
            next = STEP_RIVAL_BUYOUT_OFFER;
        break;
    case STEP_RIVAL_BUYOUT_LOT:
        AUTO_BEGIN(RULE_AUTO_VACANT);
        g_uiTaskWork[7][1] = LOT_BUYOUT;
        Task_Start(g_uiTaskIds[7], VacantLot_Task);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_BUYOUT_LOT:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        AUTO_END(RULE_AUTO_VACANT);
        next = STEP_RIVAL_SHOP_OPEN;
        break;
    case STEP_RIVAL_BUYOUT_OFFER:
        FindOwnStoreHere();                 /* sic: result unused in the original */
        AUTO_BEGIN(RULE_AUTO_STORE);
        g_uiTaskWork[7][1] = LOT_BUYOUT;
        Task_Start(g_uiTaskIds[7], StoreOffer_Task);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_BUYOUT_OFFER:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        AUTO_END(RULE_AUTO_STORE);
        next = STEP_RIVAL_SHOP_OPEN;
        break;
    case STEP_RIVAL_SHOP_OPEN:
        Task_Start(g_actionTaskIds[0], ShopOpen_Task);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_SHOP_OPEN:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        *step = STEP_RIVAL_DONE;
        return 0;
    case STEP_RIVAL_STEAL:
        Task_Start(g_uiTaskIds[7], StealItem_Task);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_STEAL:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        *step = STEP_RIVAL_DONE;
        return 0;
    case STEP_RIVAL_CLOSED:
        Msg_Show(CUR->isCpu, MSG_AUTO_POS, MSG_AUTO_POS, MSG_AUTO_SIZE, MSG_AUTO_SIZE, 0x96,
                 MSG_ANY_BUTTON, "%CClosed%E");
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_CLOSED:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_RIVAL_DONE;
        return 0;
    case STEP_RIVAL_VANDALIZED:
        strcpy(g_msgBuf, "This store can't be opened%Kdue to vandalism.");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_VANDALIZED:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_RIVAL_DONE;
        return 0;
    case STEP_RIVAL_TEMP_CLOSED:
        Msg_Show(CUR->isCpu, MSG_AUTO_POS, MSG_AUTO_POS, MSG_AUTO_SIZE, MSG_AUTO_SIZE, 0x96,
                 MSG_ANY_BUTTON, "%CTemporary Closed.%E");
        next = *step + 1;
        break;
    case STEP_RIVAL_WAIT_TEMP_CLOSED:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_RIVAL_DONE;
        return 0;
    case STEP_RIVAL_DONE:
        next = STEP_SETTLE;
        break;

    /* ---- own store ---- */
    case STEP_OWN_STORE:
        g_turnFlags[TURN_OWN_STORE] = 1;
        next = STEP_OWN_NO_DELIVERY;
        if (!CUR->noDelivery)
        {
            AUTO_BEGIN(RULE_AUTO_OWN);
            g_uiTaskWork[7][1] = g_curPlayer;
            Task_Start(g_uiTaskIds[7], DeliveryStore_Task);
            next = *step + 1;
        }
        break;
    case STEP_OWN_WAIT_STORE:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        *step = (Task_GetUserData(g_uiTaskIds[7]) < 0) ? STEP_OWN_DONE : STEP_OWN_DELIVERY;
        AUTO_END(RULE_AUTO_OWN);
        return 0;
    case STEP_OWN_DELIVERY:
        AUTO_BEGIN(RULE_AUTO_OWN);
        g_uiTaskWork[7][1] = Task_GetUserData(g_uiTaskIds[7]);
        g_uiTaskWork[7][2] = 0;
        Task_Start(g_uiTaskIds[7], Delivery_Task);
        next = *step + 1;
        break;
    case STEP_OWN_WAIT_DELIVERY:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        *step = STEP_OWN_DONE;
        AUTO_END(RULE_AUTO_OWN);
        return 0;
    case STEP_OWN_DONE:
        next = STEP_SETTLE;
        break;
    case STEP_OWN_NO_DELIVERY:
        strcpy(g_msgBuf, "Delivery is prohibited%Kby an unseen force.");
        MSG_SHOW(g_msgBuf);
        next = *step + 1;
        break;
    case STEP_OWN_WAIT_NO_DELIVERY:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_SETTLE;
        return 0;

    /* ---- settle ---- */
    case STEP_SETTLE:                       /* wait for every money/asset counter to catch up */
        for (n = 0; n < g_numPlayers; n++)
        {
            if (g_players[n].moneyShown != g_players[n].money ||
                g_players[n].assetsShown != g_players[n].assets)
                return 0;
        }
        next = *step + 1;
        break;
    case STEP_SETTLE_CHECK:
        next = STEP_END_TURN;
        if (CUR->exempt == 0 && CUR->money < 0)
            next = STEP_MINUS;
        break;
    case STEP_MINUS:
        MSG_SHOW("%CMoney went to minus.%E");
        next = *step + 1;
        break;
    case STEP_WAIT_MINUS:
        if (!Msg_IsDone())
            return 0;
        *step = STEP_MINUS_SELL;
        return 0;
    case STEP_MINUS_SELL:
        AUTO_BEGIN(RULE_AUTO_MINUS);
        Task_SendMessage(g_playerPanelTaskId,DISP_SHOW_LOW);
        g_uiTaskWork[7][1] = 0x4B;
        g_uiTaskWork[7][2] = 1;
        Task_Start(g_uiTaskIds[7], SellStore_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_MINUS_SELL:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        *step = STEP_END_TURN;
        AUTO_END(RULE_AUTO_MINUS);
        return 0;
    case STEP_SETTLE_UNUSED:                /* (unreachable: nothing sets this step) */
        Task_Start(g_actionTaskIds[0], ShopOpen_Task);
        next = *step + 1;
        break;
    case STEP_WAIT_SETTLE_UNUSED:
        if (!Task_IsDone(g_actionTaskIds[0]))
            return 0;
        next = STEP_END_TURN;
        break;

    default:
        return 0;
    }
    *step = next;
    return 0;
}

/* 0x80046E60  steps 9000..12105: end of turn, win check, next player, game end */
int Board_PhaseEndTurn(unsigned short *step, void *work)
{
    BoardWork *w = (BoardWork *)work;
    unsigned short next;
    unsigned char *base;
    PlayerSetup *ps;
    int *src;
    int i;

    switch (*step)
    {
    case STEP_END_TURN:                     /* optional event after the turn */
        next = STEP_END_TALK;
        if (g_boardRules.enable[RULE_POST_EVENT])
        {
            Task_Start(g_uiTaskIds[7],
                       g_boardRules.enable[RULE_PICK_EVENT] ? (void *)EventPick_Task : (void *)EventCard_Task);
            next = *step + 1;
        }
        break;
    case STEP_POST_EVENT_WAIT_PICK:
        if (!Task_IsDone(g_uiTaskIds[7]))
            return 0;
        next = (Task_GetUserData(g_uiTaskIds[7]) == -1) ? STEP_END_TALK : STEP_POST_EVENT_LOAD;
        break;
    case STEP_POST_EVENT_LOAD:
        *g_loaderWork = (short)Task_GetUserData(g_uiTaskIds[7]) + FILE_EVE_000_J;
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        next = *step + 1;
        break;
    case STEP_POST_EVENT_WAIT_LOAD:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        *step = STEP_POST_EVENT_SCRIPT;
        return 0;
    case STEP_POST_EVENT_SCRIPT:
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_POST_EVENT_WAIT_SCRIPT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        Bgm_Play(g_boardStage + 7, 0, 0x7F, 0x7F);
        switch (Task_GetUserData(g_scriptTaskId))
        {
        case EVENT_RESULT_NONE:
            *step = STEP_END_TALK;
            return 0;
        case EVENT_RESULT_ANOTHER:
            next = STEP_END_TURN;
            break;
        case EVENT_RESULT_REROLL:
            w->rollCancelable = 0;
            next = STEP_ROLL;
            break;
        case EVENT_RESULT_MOVE:
            Task_SendMessage(g_calendarTaskId,DISP_SHOW);
            Task_SendMessage(g_minimapTaskId,DISP_SHOW_LOW);
            next = STEP_MOVE;
            break;
        default:
            return 0;
        }
        break;

    case STEP_END_TALK:                     /* character talk at turn end */
        if (g_gameConfig.playMode == PLAY_DEMO)
            next = STEP_ASSETS;
        else if (g_boardRules.enable[RULE_ALWAYS_TALK])
            next = STEP_END_TALK_SCRIPT;
        else if (CUR->isCpu && CPU_TALK_ON)
            next = STEP_END_TALK_SCRIPT;
        else
            next = STEP_ASSETS;
        break;
    case STEP_END_TALK_SCRIPT:
        Task_SendMessage(g_minimapTaskId,DISP_HIDE);
        Task_SendMessage(g_playerPanelTaskId,DISP_COUNT_DROP);
        /* .J entry 16 + charId of the character script archive */
        base = g_secHeapBlocks[g_charScriptsBlock].addr;
        src = (int *)(base + ((int *)base)[(signed char)CUR->charId + 17]);
        g_scriptBlock = SecHeap_Alloc(TALK_SCRIPT_SECTORS(src[0]));
        Lzss_Decompress(src, g_secHeapBlocks[g_scriptBlock].addr);
        g_scriptData = g_secHeapBlocks[g_scriptBlock].addr;
        g_scriptEntry = TurnEnd_PickScript(g_curPlayer);
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_END_TALK_WAIT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        next = *step + 1;
        break;
    case STEP_END_TALK_WAIT_XA:
        if (Cd_IsXaPlaying())
            return 0;
        SecHeap_Free(g_scriptBlock);
        next = STEP_ASSETS;
        break;

    /* ---- assets and bankruptcy ---- */
    case STEP_ASSETS:
        g_resumedGame = 0;
        CUR->assets = Player_CalcAssets(g_curPlayer);
        if (CUR->money < 0)
        {
            next = STEP_BANKRUPT;
            if (g_gameConfig.playMode == PLAY_STAGE && g_curPlayer == 0)
                next = STEP_STAGE_LOST;     /* stage mode: the human player went bankrupt */
        }
        else
        {
            next = STEP_WIN_CHECK;
        }
        break;
    case STEP_BANKRUPT:                     /* (sic) nothing starts a fade here */
        next = *step + 1;
        break;
    case STEP_BANKRUPT_WAIT_FADE:
        if (!Fade_IsDone())
            return 0;
        next = *step + 1;
        break;
    case STEP_BANKRUPT_SKIP1:
    case STEP_BANKRUPT_SKIP2:
        next = *step + 1;
        break;
    case STEP_BANKRUPT_LOAD:                /* bankruptcy scene */
        *g_loaderWork = FILE_EVE_000_J;
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        next = *step + 1;
        break;
    case STEP_BANKRUPT_WAIT_LOAD:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        next = *step + 1;
        break;
    case STEP_BANKRUPT_SCRIPT:
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_BANKRUPT_WAIT_SCRIPT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        next = *step + 1;
        break;
    case STEP_BANKRUPT_OUT:
        g_numActivePlayers--;
        CUR->out = 1;
        CUR->rankKey = (g_curPlayer + (short)g_gameConfig.monthsElapsed * g_numPlayers);
        Bgm_Play(g_boardStage + 7, 0, 0x7F, 0x7F);
        next = STEP_WIN_CHECK;
        break;

    /* ---- win check ---- */
    case STEP_WIN_CHECK:
        switch (g_gameConfig.playMode)
        {
        case PLAY_NORMAL:                   /* normal game */
            CollectWinners();
            if (g_numWinners != 0 ||
                g_gameConfig.bankruptLimit <= g_numPlayers - g_numActivePlayers)
                next = STEP_GAME_OVER;
            else
                next = STEP_TURN_END_WIPE;
            break;
        case PLAY_STAGE:                    /* stage mode: player 0 is the human */
            CollectWinners();
            if (g_numWinners == 0)
            {
                next = (g_gameConfig.bankruptLimit <=
                        g_numPlayers - g_numActivePlayers) ? STEP_STAGE_WON : STEP_TURN_END_WIPE;
            }
            else
            {
                g_winner = g_winners[0];
                for (i = 1; i < g_numWinners; i++)
                {
                    if (g_players[g_winner].assets < g_players[g_winners[i]].assets)
                        g_winner = g_winners[i];
                }
                next = (g_winner == 0) ? STEP_STAGE_WON : STEP_STAGE_LOST;
            }
            break;
        case PLAY_DEMO:                     /* attract demo */
            next = (g_gameConfig.demoTimer < 0) ? STEP_GAME_OVER : STEP_TURN_END_WIPE;
            break;
        default:
            return 0;
        }
        break;

    /* ---- next player ---- */
    case STEP_TURN_END_WIPE:
        Task_Start(g_turnFxTaskId, Wipe_CloseTask);
        next = *step + 1;
        break;
    case STEP_TURN_END_WAIT_WIPE:
        if (!Task_IsDone(g_turnFxTaskId))
            return 0;
        *step = STEP_NEXT_PLAYER;
        return 0;
    case STEP_NEXT_PLAYER:
        Fade_SetBlack();
        Board_ResumeDisplayTasks();
        if (g_curPlayer == g_numPlayers - 1)
        {
            Calendar_Advance(1);            /* every round is one month */
            g_curPlayer = 0;
        }
        else
        {
            g_curPlayer++;
        }
        next = STEP_TURN;
        break;

    /* ---- end of the board game ---- */
    case STEP_GAME_OVER:
        Players_Rank();
        for (i = 0; i < g_numPlayers; i++)
        {
            ps = &g_gameConfig.players[(signed char)g_gameConfig.turnOrder[i]];
            ps->charId = g_players[i].charId;
            ps->finalRank = g_players[i].rank;
            ps->wasOut = g_players[i].out;
        }
        Snd_StopAll();
        Xa_RequestStop();
        Bgm_Stop();
        Fade_Out(30);
        next = *step + 1;
        break;
    case STEP_GAME_OVER_WAIT_FADE:
        if (!Fade_IsDone())
            return 0;
        next = *step + 1;
        break;
    case STEP_GAME_OVER_EXIT:               /* reset CD and heap, pick the next mode */
        Bgm_Stop();
        Task_Stop(g_loaderTaskId);
        CdFlush();
        CdReset(0);
        Cd_InitQueue();
        SecHeap_Init();
        g_fontWorkBlock = SecHeap_Alloc(0x20);
        TextWin_FreeAll();
        if (g_gameConfig.playMode == PLAY_NORMAL)
            g_gameMode = MODE_ENDING;
        else if (g_gameConfig.playMode != PLAY_DEMO && g_stageCleared == 1)
            g_gameMode = MODE_STAGE_CLEAR;
        else
            g_gameMode = MODE_TITLE;
        return 1;

    case STEP_STAGE_LOST:                   /* stage lost: EVE_001 scene */
        Fade_Out(90);
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_WAIT_FADE:
        if (!Fade_IsDone())
            return 0;
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_SUSPEND:
        Board_SuspendDisplayTasks();
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_BLACK:
        Fade_SetBlack();
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_LOAD:
        *g_loaderWork = FILE_EVE_001_J;
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_WAIT_LOAD:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_SCRIPT:
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_STAGE_LOST_WAIT_SCRIPT:
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        g_stageCleared = 0;
        next = STEP_GAME_OVER_WAIT_FADE;
        break;

    case STEP_STAGE_WON:                    /* stage won: CNT10 + stage scene */
        Fade_Out(30);
        next = *step + 1;
        break;
    case STEP_STAGE_WON_WAIT_FADE:
        if (!Fade_IsDone())
            return 0;
        next = *step + 1;
        break;
    case STEP_STAGE_WON_SUSPEND:
        Board_SuspendDisplayTasks();
        next = *step + 1;
        break;
    case STEP_STAGE_WON_LOAD:
        Board_LoadBackground(g_boardStage);
        Fade_SetBlack();
        *g_loaderWork = FILE_CNT10_J + g_boardStage;
        Task_Start(g_loaderTaskId, LoadJWithTim_Task);
        next = *step + 1;
        break;
    case STEP_STAGE_WON_SCRIPT:
        if (!Task_IsDone(g_loaderTaskId))
            return 0;
        Task_Start(g_scriptTaskId, EventScript_Task);
        next = *step + 1;
        break;
    case STEP_STAGE_WON_WAIT_SCRIPT:
        DrawEventPicture();
        if (!Task_IsDone(g_scriptTaskId))
            return 0;
        SecHeap_Free(g_scriptBlock);
        g_stageCleared = 1;
        next = STEP_GAME_OVER_WAIT_FADE;
        break;

    default:
        return 0;
    }
    *step = next;
    return 0;
}
