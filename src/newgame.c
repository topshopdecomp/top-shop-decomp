/*
 * newgame.c - game mode 5: the new-game set-up
 *
 * The main menu (free play, story mode from the start or a stage it has reached, load, options)
 * and the set-up screens that follow it in turn (src/setup.c), the options screen and loading a
 * saved game; then the board.
 */
#include "common.h"
#include "topshop.h"

/* NewGame_Task steps */
enum NewGameStep
{
    NEWGAME_LOAD             =   0,  /* the sub-tasks, the set-up graphics */
    NEWGAME_SYSTEM_DATA      =   1,  /* Card_LoadSystemTask */
    NEWGAME_WAIT_SYSTEM_DATA =   2,
    NEWGAME_SETUP            =  10,
    NEWGAME_MENU             =  20,  /* NewGame_MenuTask */
    NEWGAME_WAIT_MENU        =  21,
    NEWGAME_DONE             = 100   /* to the board */
};

/* Game mode 5: new game setup. Memory card system-data load (Card_LoadSystemTask), then NewGame_MenuTask
 * (player menu / name entry / character shuffle tasks), GameConfig.stage = 0, then g_gameMode
 * = 6 (or 10 if g_newGameStarted is set). (0x80036F1C) */
/* 0x80036F1C */
int NewGame_Task(short *step, void *work)
{
    Gfx_QueueClear(0,0,0);
    FntPrint("~c000DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", (u16)*step);
    g_debugPrint = 1;
    Game_Unused1();
    switch (*step)
    {
    case NEWGAME_LOAD:
        Mode_CreateSubTasks();
        NewGame_LoadGraphics();
        Fade_SetClear();
        *step += 1;
        break;
    case NEWGAME_SYSTEM_DATA:
        Task_Start(g_subTaskIds[0],Card_LoadSystemTask);
        *step += 1;
        break;
    case NEWGAME_WAIT_SYSTEM_DATA:
        if (Task_IsDone(g_subTaskIds[0]))
        {
            Fade_SetBlack();
            *step = NEWGAME_SETUP;
        }
        break;
    case NEWGAME_SETUP:
        g_newGameStarted = 0;
        NewGame_LoadCharGraphics();
        BoardRules_Init();
        g_gameConfig.stage = 0;
        *step = NEWGAME_MENU;
        break;
    case NEWGAME_MENU:
        Task_Start(g_subTaskIds[0],NewGame_MenuTask);
        *step += 1;
        break;
    case NEWGAME_WAIT_MENU:
        if (Task_IsDone(g_subTaskIds[0]))
        {
            *step = NEWGAME_DONE;
        }
        break;
    case NEWGAME_DONE:
        g_gameMode = (g_newGameStarted != 0) ? MODE_UNUSED10 : MODE_BOARD;
        return 1;
    }
    TextWin_DrawAll();
    Font_FlushUploads();
    g_glyphUploadPtr = g_glyphUploadBuf;
    return 0;
}

/* Load the setup screens' sprites (SET1.SPC, SET2.SPC, M256_BG.SPC) (0x80037128) */
/* 0x80037128 */
void NewGame_LoadGraphics(void)
{
    short base;
    SpriteGroup *grp;

    grp = g_spriteGroups;
    base = g_spriteGroups[5].base;
    g_spriteGroups[4].next = g_spriteGroups[4].base;
    grp[5].next = base;
    File_LoadPackedSprites(FILE_SET1_SPC,5);
    File_LoadPackedSprites(FILE_SET2_SPC,5);
    File_LoadPackedImages(FILE_M256_BG_SPC);
}

/* Load SET3.SPC and the character sprites SET0CHR0-2.SPC (0x80037370) */
/* 0x80037370 */
void NewGame_LoadCharGraphics(void)
{

    File_LoadPackedSprites(FILE_SET3_SPC,5);
    File_LoadPackedSprites(FILE_SET0CHR0_SPC,4);
    File_LoadPackedSprites(FILE_SET0CHR1_SPC,4);
    File_LoadPackedSprites(FILE_SET0CHR2_SPC,4);
}

/* NewGame_MenuTask steps */
enum NewGameMenuStep
{
    NGMENU_FADE_IN           =    0,
    NGMENU_WAIT_FADE_IN      =    1,
    NGMENU_MAIN              =  100,  /* MainMenu_Task */
    NGMENU_WAIT_MAIN         =  101,  /* on by g_mainMenuChoice */
    NGMENU_PLAYER_COUNT      = 1000,  /* free play: PlayerCount_Task */
    NGMENU_WAIT_PLAYER_COUNT = 1001,
    NGMENU_PLAYER_MENU       = 1040,  /* PlayerMenu_Task */
    NGMENU_WAIT_PLAYER_MENU  = 1041,
    NGMENU_CHAR_SELECT       = 1060,  /* CharSelect_Task */
    NGMENU_WAIT_CHAR_SELECT  = 1061,
    NGMENU_NAME_ENTRY        = 1080,  /* NameEntry_Task */
    NGMENU_WAIT_NAME_ENTRY   = 1081,
    NGMENU_STAGE             = 1100,  /* StageSelect_Task */
    NGMENU_WAIT_STAGE        = 1101,
    NGMENU_MONEY_RULES       = 1120,  /* MoneyRules_Task */
    NGMENU_WAIT_MONEY_RULES  = 1121,
    NGMENU_TURN_ORDER        = 1140,  /* TurnOrder_Task */
    NGMENU_WAIT_TURN_ORDER   = 1141,
    NGMENU_STORY_STAGE       = 2000,  /* story mode: StageSelect_Task */
    NGMENU_WAIT_STORY_STAGE  = 2001,
    NGMENU_STORY_MONEY       = 2100,  /* MoneyRules_Task */
    NGMENU_WAIT_STORY_MONEY  = 2101,
    NGMENU_LOAD              = 3000,  /* LoadGame_Task */
    NGMENU_WAIT_LOAD         = 3001,
    NGMENU_OPTIONS           = 4000,  /* Options_Task */
    NGMENU_WAIT_OPTIONS      = 4001,
    NGMENU_START             = 5000,  /* on to the game */
    NGMENU_WAIT_FADE_OUT     = 5001,
    NGMENU_DONE              = 5002 
};

/* Runs the setup screens on g_subTaskIds[1], each reporting back in g_menuResult (0 on, else
 * back): the main menu (MainMenu_Task, g_mainMenuChoice), then for free play the player count,
 * the player menu (its 2 opens the character select, then the name entry), the stage, the money
 * rules and the turn order; story mode: stage and money rules; load; options. Draws the
 * background picture (0x80037648) */
/* 0x80037648 */
u32 NewGame_MenuTask(u16 *step)
{
    VramImage img;
    short pos[4];
    int i;

    switch (*step)
    {
    case NGMENU_FADE_IN:
        g_mainMenuChoice = MAINMENU_FREE_PLAY;
        Bgm_Play(2,0,0x7f,0x7f);
        Fade_SetBlack();
        Fade_In(15);
        *step += 1;
        break;
    case NGMENU_WAIT_FADE_IN:
        if (Fade_IsDone())
        {
            *step = NGMENU_MAIN;
        }
        break;

    /* the main menu */
    case NGMENU_MAIN:
        Task_Start(g_subTaskIds[1],MainMenu_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_MAIN:
        if (!Task_IsDone(g_subTaskIds[1]))
        {
            break;
        }
        switch (g_mainMenuChoice)
        {
        case MAINMENU_FREE_PLAY: *step = NGMENU_PLAYER_COUNT; break;
        case MAINMENU_STORY:     *step = NGMENU_STORY_STAGE; break;   /* story mode */
        case MAINMENU_LOAD:      *step = NGMENU_LOAD; break;
        case MAINMENU_OPTIONS:   *step = NGMENU_OPTIONS; break;
        }
        break;

    /* 1000-1141: free play */
    case NGMENU_PLAYER_COUNT:
        g_setupSlot = 0;
        g_setupLastSlot = 0;
        g_gameConfig.bankruptLimit = 1;
        g_gameConfig.maxBuy = 2;
        g_gameConfig.startMoney = 2000;
        g_gameConfig.goalMoney = 10000;
        for (i = 0; i < 14; i++)
        {
            g_charTaken[i] = 0;
        }
        for (i = 0; i < 6; i++)
        {
            g_gameConfig.players[i].charId = 0xff;
            g_gameConfig.players[i].pad = 0;
        }
        for (i = 0; i < 6; i++)
        {
            g_slotFilled[i] = 0;
        }
        Task_Start(g_subTaskIds[1],PlayerCount_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_PLAYER_COUNT:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult != MENU_RESULT_NEXT) ? NGMENU_MAIN : NGMENU_PLAYER_MENU;
        }
        break;
    case NGMENU_PLAYER_MENU:
        *(int *)g_subTaskWork[1] = g_setupSlot;
        Task_Start(g_subTaskIds[1],PlayerMenu_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_PLAYER_MENU:
        if (!Task_IsDone(g_subTaskIds[1]))
        {
            break;
        }
        switch (g_menuResult)
        {
        case MENU_RESULT_NEXT:      *step = NGMENU_STAGE; break;         /* all set: the stage */
        case MENU_RESULT_BACK:      *step = NGMENU_PLAYER_COUNT; break;  /* back to the player count */
        case MENU_RESULT_CHARACTER: *step = NGMENU_CHAR_SELECT; break;   /* a player's character */
        }
        break;
    case NGMENU_CHAR_SELECT:
        *(int *)g_subTaskWork[1] = g_setupSlot;
        ((int *)g_subTaskWork[1])[1] = 0;
        Task_Start(g_subTaskIds[1],CharSelect_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_CHAR_SELECT:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult == MENU_RESULT_NEXT) ? NGMENU_NAME_ENTRY : NGMENU_PLAYER_MENU;
        }
        break;
    case NGMENU_NAME_ENTRY:
        *(int *)g_subTaskWork[1] = g_setupSlot;
        Task_Start(g_subTaskIds[1],NameEntry_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_NAME_ENTRY:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult != MENU_RESULT_NEXT) ? NGMENU_CHAR_SELECT : NGMENU_PLAYER_MENU;
        }
        break;
    case NGMENU_STAGE:
        *(u32 *)g_subTaskWork[1] = 0;
        Task_Start(g_subTaskIds[1],StageSelect_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_STAGE:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult == MENU_RESULT_NEXT) ? NGMENU_MONEY_RULES : NGMENU_PLAYER_MENU;
        }
        break;
    case NGMENU_MONEY_RULES:
        g_gameConfig.startMoney = g_stageMoneyTable[(char)g_gameConfig.stage].startMoney;
        g_gameConfig.goalMoney = g_stageMoneyTable[(char)g_gameConfig.stage].goalMoney;
        *(u32 *)g_subTaskWork[1] = 0;
        Task_Start(g_subTaskIds[1],MoneyRules_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_MONEY_RULES:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult == MENU_RESULT_NEXT) ? NGMENU_TURN_ORDER : NGMENU_STAGE;
        }
        break;
    case NGMENU_TURN_ORDER:
        Task_Start(g_subTaskIds[1],TurnOrder_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_TURN_ORDER:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult != MENU_RESULT_NEXT) ? NGMENU_MONEY_RULES : NGMENU_START;
        }
        break;

    /* 2000-2101: story mode */
    case NGMENU_STORY_STAGE:
        Story_SetupPlayers();
        *(u32 *)g_subTaskWork[1] = 1;
        Task_Start(g_subTaskIds[1],StageSelect_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_STORY_STAGE:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult == MENU_RESULT_NEXT) ? NGMENU_STORY_MONEY : NGMENU_MAIN;
        }
        break;
    case NGMENU_STORY_MONEY:
        *(u32 *)g_subTaskWork[1] = 1;
        Task_Start(g_subTaskIds[1],MoneyRules_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_STORY_MONEY:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult != MENU_RESULT_NEXT) ? NGMENU_STORY_STAGE : NGMENU_START;
        }
        break;

    /* 3000: load, 4000: options */
    case NGMENU_LOAD:
        Task_Start(g_subTaskIds[1],LoadGame_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_LOAD:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = (g_menuResult == MENU_RESULT_NEXT) ? NGMENU_START : NGMENU_MAIN;
        }
        break;
    case NGMENU_OPTIONS:
        Task_Start(g_subTaskIds[1],Options_Task);
        *step += 1;
        break;
    case NGMENU_WAIT_OPTIONS:
        if (Task_IsDone(g_subTaskIds[1]))
        {
            *step = NGMENU_MAIN;
        }
        break;

    /* 5000-5002: on to the game */
    case NGMENU_START:
        Bgm_Stop();
        Fade_Out(15);
        *step += 1;
        break;
    case NGMENU_WAIT_FADE_OUT:
        if (Fade_IsDone())
        {
            *step += 1;
        }
        break;
    case NGMENU_DONE:
        return 1;
    }
    img.mode = IMG_MODE_8BIT;
    img.x = 0x2c0;
    img.w = 0x140;
    img.h = 0xf0;
    img.clutY = 0x1e9;
    img.y = 0;
    img.clutX = 0;
    pos[0] = 0;
    pos[1] = 0;
    Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,0x1e);
    return 0;
}

/* MainMenu_Task steps */
enum MainMenuStep
{
    MAIN_INIT                 =   0,
    MAIN_ITEM_IN              =   1,  /* the items come in two frames apart */
    MAIN_ITEM_DELAY           =   2,
    MAIN_WAIT_ITEMS           =   3,
    MAIN_FADE_IN              =  10,
    MAIN_CHOOSE               =  20,
    MAIN_STORY                =  50,  /* story mode: from the start, or from a stage */
    MAIN_STORY_ITEM_IN        =  51,
    MAIN_STORY_ITEM_DELAY     =  52,
    MAIN_STORY_WAIT_ITEMS     =  53,
    MAIN_STORY_FADE_IN        =  60,
    MAIN_STORY_CHOOSE         =  70,
    MAIN_STORY_BACK           =  80,  /* back: the story rows leave */
    MAIN_STORY_ITEM_OUT       =  81,
    MAIN_STORY_ITEM_OUT_DELAY =  82,
    MAIN_LEAVE                = 100,  /* everything leaves (the character runs off) */
    MAIN_FADE_OUT             = 101,
    MAIN_ITEM_OUT             = 102,
    MAIN_ITEM_OUT_DELAY       = 103,
    MAIN_WAIT_ITEMS_OUT       = 104,
    MAIN_WAIT_RUN_OFF         = 105,
    MAIN_DONE                 = 106 
};

/* Main menu: free play, story mode (from the start, or a stage it has reached: left / right),
 * load, options. A character runs off when the choice changes and comes back with it
 * (g_mainMenuChoice; GameConfig.playMode and stage) (0x800385A8) */
/* 0x800385A8 */
u32 MainMenu_Task(short *step, MainMenuWork *work)
{
    MenuItem *item;
    short bob;
    int i;

    switch (*step)
    {
    case MAIN_INIT:
        work->shown = 1;
        work->inStory = 0;
        work->storyRow = 0;
        work->stage = 0;
        work->mainMenuChoice = g_mainMenuChoice;
        work->progress = (g_progress.storyStage < 10) ? g_progress.storyStage : 9;
        work->fade = 0x100;
        work->storyFade = 0x100;
        work->item = 0;
        work->count = 5;
        for (i = 0; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->state = MENUITEM_STILL;
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->offsetY = 0;
            if (i == 0)
            {
                g_menuItems[0].sprite = 0x45;
                g_menuItems[0].x = 0x10;
                g_menuItems[0].y = 0x10;
            }
            else
            {
                item->sprite = i + 0x61;
                item->x = 0x30;
                item->y = 0x10 + i * 0x18;
            }
        }
        work->x = -0x2a;
        work->started = 0;
        work->y = 0xb0;
        *step += 1;
        break;
    case MAIN_ITEM_IN:
        /* the items come in two frames apart */
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = MAIN_WAIT_ITEMS;
        break;
    case MAIN_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = MAIN_ITEM_IN;
        }
        break;
    case MAIN_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = MAIN_FADE_IN;
        }
        break;
    case MAIN_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->started = 1;
            work->timer = 1;
            work->fade = 0;
            *step = MAIN_CHOOSE;
        }
        break;
    case MAIN_CHOOSE:
        work->inStory = 0;
        if (g_playerPads[6].pressed & PADRdown)
        {
            if (work->mainMenuChoice == MAINMENU_STORY)
            {
                if (work->progress != 0)
                {
                    Se_Play(SE_OK,0x7f,0x7f,1);
                    *step = MAIN_STORY;
                    break;
                }
                work->stage = 0;
            }
            else
            {
                g_menuResult = MENU_RESULT_NEXT;
            }
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = MAIN_LEAVE;
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            work->mainMenuChoice = (work->mainMenuChoice == MAINMENU_FREE_PLAY) ? MAINMENU_OPTIONS : work->mainMenuChoice - 1;
            work->timer = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            work->mainMenuChoice = (work->mainMenuChoice == MAINMENU_OPTIONS) ? MAINMENU_FREE_PLAY : work->mainMenuChoice + 1;
            work->timer = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;

    /* 50-82: story mode: from the start, or from a stage */
    case MAIN_STORY:
        work->inStory = 1;
        work->storyRow = 1;
        work->storyFade = 0x100;
        work->count = 7;
        work->item = 5;
        work->stage = work->progress;
        for (i = 5; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->state = MENUITEM_STILL;
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->offsetY = 0;
            item->x = 0xb0;
            item->y = 0x98 + (i - 5) * 0x18;
            if (i == 5)
            {
                g_menuItems[5].sprite = 0x5e;
            }
            else if (i == 6)
            {
                g_menuItems[6].sprite = work->stage + 0x19;
            }
        }
        *step += 1;
        break;
    case MAIN_STORY_ITEM_IN:
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = MAIN_STORY_WAIT_ITEMS;
        break;
    case MAIN_STORY_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = MAIN_STORY_ITEM_IN;
        }
        break;
    case MAIN_STORY_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = MAIN_STORY_FADE_IN;
        }
        break;
    case MAIN_STORY_FADE_IN:
        work->storyFade -= 0x40;
        if (work->storyFade < 1)
        {
            work->storyFade = 0;
            *step = MAIN_STORY_CHOOSE;
        }
        break;
    case MAIN_STORY_CHOOSE:
        if (g_playerPads[6].pressed & PADRdown)
        {
            g_menuResult = MENU_RESULT_NEXT;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = MAIN_LEAVE;
        }
        else if (g_playerPads[6].pressed & PADRright)
        {
            work->timer = 40;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = MAIN_STORY_BACK;
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            work->storyRow = (work->storyRow == 0) ? 1 : work->storyRow - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            work->storyRow = (work->storyRow == 1) ? 0 : work->storyRow + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLleft)
        {
            if (work->storyRow == 1 && work->stage > 1)
            {
                work->stage--;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
        }
        else if (g_playerPads[6].repeat & PADLright)
        {
            if (work->storyRow == 1 && work->stage < work->progress)
            {
                work->stage++;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
        }
        break;
    case MAIN_STORY_BACK:
        /* back: the story rows leave */
        work->storyFade += 0x40;
        if (work->storyFade > 0xff)
        {
            work->storyFade = 0x100;
            *step += 1;
        }
        work->item = 5;
        break;
    case MAIN_STORY_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = MAIN_CHOOSE;
        break;
    case MAIN_STORY_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = MAIN_STORY_ITEM_OUT;
        }
        break;

    /* 100-106: everything leaves (the character runs off) */
    case MAIN_LEAVE:
        work->timer = 0x7fffffff;
        *step += 1;
        break;
    case MAIN_FADE_OUT:
        work->storyFade += 0x40;
        work->fade += 0x40;
        if (work->storyFade > 0xff)
        {
            work->storyFade = 0x100;
        }
        if (work->fade > 0xff)
        {
            work->fade = 0x100;
            *step += 1;
        }
        work->item = 0;
        break;
    case MAIN_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = MAIN_WAIT_ITEMS_OUT;
        break;
    case MAIN_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = MAIN_ITEM_OUT;
        }
        break;
    case MAIN_WAIT_ITEMS_OUT:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step += 1;
        }
        break;
    case MAIN_WAIT_RUN_OFF:
        if (work->x < -0x29)
        {
            *step += 1;
        }
        break;
    case MAIN_DONE:
        g_mainMenuChoice = work->mainMenuChoice;
        g_gameConfig.playMode = (work->mainMenuChoice != MAINMENU_FREE_PLAY);
        g_gameConfig.stage = (work->storyRow == 0) ? 0 : work->stage;
        g_resumedGame = 0;
        work->shown = 0;
        return 1;
    }

    if (work->shown != 0)
    {
        Menu_DrawBand(g_menuItems[work->mainMenuChoice + 1].y,0x10,work->fade);
        Menu_DrawBand(g_menuItems[work->storyRow + 5].y,0x10,work->storyFade);
        if (work->inStory == 0)
        {
            if (work->fade == 0)
            {
                Menu_DrawHand(g_menuItems[work->mainMenuChoice + 1].y);
            }
        }
        else if (work->storyFade == 0)
        {
            Menu_DrawHand(g_menuItems[work->storyRow + 5].y);
        }
        g_menuItems[6].sprite = work->stage + 0x19;
        MenuItems_Draw(work->count);
        if (work->count == 7 && g_menuItems[6].offsetX == 0)
        {
            /* the arrows either side of the stage */
            if (work->stage > 1)
            {
                g_drawPos[0] = 0xa0;
                g_drawPos[1] = g_menuItems[6].y;
                Sprite_DrawAligned(GROUP_SPRITE(5,0x6d),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
            }
            if (work->stage < work->progress)
            {
                g_drawPos[0] = 0x110;
                g_drawPos[1] = g_menuItems[6].y;
                Sprite_DrawAligned(GROUP_SPRITE(5,0x6d),GFX_FLIP_X,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                                   g_ot + g_otIndex,8);
            }
        }
    }
    if (work->started != 0)
    {
        /* the running character: 40 frames after a change it comes in from the right with the new
         * choice and stops at x 0xa0 (0x60 with the story rows); in between it runs off left */
        if (work->timer == 0)
        {
            work->x -= 8;
            if (work->inStory == 0)
            {
                if (work->x < 0xa1)
                {
                    work->x = 0xa0;
                }
            }
            else if (work->x < 0x61)
            {
                work->x = 0x60;
            }
        }
        else if (--work->timer == 0)
        {
            work->x = 0x168;
            work->shownChoice = work->mainMenuChoice;
        }
        else
        {
            work->x -= 8;
            if (work->x < -0x29)
            {
                work->x = -0x2a;
            }
        }
        /* it hops as it runs */
        bob = rsin((work->x - 0xa0) * 0x20) / 128;
        g_drawPos[0] = work->x;
        g_drawPos[1] = work->y + ((bob < 0) ? bob : -bob);
        Sprite_DrawAligned(GROUP_SPRITE(5,0xb0) + work->shownChoice * 2 + g_frameVsync / 16 % 2,0,PAL_DEFAULT,
                           g_drawPos,0,g_ot + g_otIndex,8);
    }
    return 0;
}

/* Options_Task steps */
enum OptionsStep
{
    OPTSCR_INIT           =   0,
    OPTSCR_ITEM_IN        =   1,  /* the items come in two frames apart */
    OPTSCR_ITEM_DELAY     =   2,
    OPTSCR_WAIT_ITEMS     =   3,
    OPTSCR_FADE_IN        =  10,
    OPTSCR_CHOOSE         =  20,  /* up / down the row, left / right change it */
    OPTSCR_FADE_OUT       = 100,
    OPTSCR_ITEM_OUT       = 101,
    OPTSCR_ITEM_OUT_DELAY = 102,
    OPTSCR_WAIT_ITEMS_OUT = 103,
    OPTSCR_DONE           = 104 
};

/* Options_Task's rows (work->row) */
enum OptionRow
{
    OPTION_EVENT_SCENES,         /* event scenes on / off */
    OPTION_CPU_SPEED,            /* CPU players normal / fast */
    OPTION_MONO,                 /* stereo / mono */
    OPTION_SONG_TEST,            /* cross plays song n */
    OPTION_SE_TEST,              /* cross plays sound effect n */
    OPTION_EXIT,
    OPTION_SKIP_CPU_TALK,        /* (unreachable: the rows stop at OPTION_EXIT) */
    OPTION_XA_TEST               /* (unreachable) cross plays XA voice track n */
};

/* Options_Task's menu items: the title, then the rows' labels */
enum OptionItem
{
    OPTITEM_TITLE,
    OPTITEM_EVENT_SCENES,
    OPTITEM_CPU_SPEED,
    OPTITEM_MONO,
    OPTITEM_SONG_TEST,
    OPTITEM_SE_TEST,
    OPTITEM_EXIT
};

/* Options: CPU talk, CPU speed, event scenes, mono sound, sound and voice tests (0x8003F424) */
/* 0x8003F424 */
u32 Options_Task(short *step, OptionsWork *work)
{
    int changed;
    char optCpuFast;
    short y;
    short sprite;
    u16 fade;
    u16 id;
    MenuItem *item;
    int frameVsync;
    u32 i;

    switch(*step)
    {
    case OPTSCR_INIT:
        i = 0;
        work->shown = 1;
        work->moving = 0;
        work->row = OPTION_EVENT_SCENES;
        work->optEventScenes = g_optEventScenes;
        work->optMono = g_optMono;
        item = g_menuItems;
        work->optSkipCpuTalk = g_optSkipCpuTalk;
        optCpuFast = g_optCpuFast;
        work->slot = -1;
        work->fade = 0x100;
        work->seTest = 0;
        work->track = 0;
        work->playing = 0;
        work->song = 0;
        work->count = 7;
        work->optCpuFast = optCpuFast;
        do
        {
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->state = MENUITEM_STILL;
            item->offsetY = 0;
            switch (i)
            {
            case OPTITEM_TITLE:        item->sprite = 0x65; item->x = 0x10; item->y = 0x10; break;
            case OPTITEM_EVENT_SCENES: item->sprite = 0x83; item->x = 0x30; item->y = 0x28; break;
            case OPTITEM_CPU_SPEED:    item->sprite = 0x96; item->x = 0x30; item->y = 0x3c; break;
            case OPTITEM_MONO:         item->sprite = 0x39; item->x = 0x30; item->y = 0x50; break;
            case OPTITEM_SONG_TEST:    item->sprite = 0x3c; item->x = 0x30; item->y = 0x64; break;
            case OPTITEM_SE_TEST:      item->sprite = 0x3a; item->x = 0x30; item->y = 0x78; break;
            case OPTITEM_EXIT:         item->sprite = 0x3f; item->x = 0x30; item->y = 0x8c; break;
            }
            item++;
            i++;
        } while ((int)i < work->count);
        work->item = 0;
        *step += 1;
        break;
    case OPTSCR_ITEM_IN:
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        y = work->item + 1;
        work->item = y;
        if (y != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = OPTSCR_WAIT_ITEMS;
        break;
    case OPTSCR_ITEM_DELAY:
        work->delay--;
        if (work->delay == 0)
        {
            *step = OPTSCR_ITEM_IN;
        }
        break;
    case OPTSCR_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = OPTSCR_FADE_IN;
        }
        break;
    case OPTSCR_FADE_IN:
        fade = work->fade - 0x40;
        work->fade = fade;
        if ((short)fade < 1)
        {
            work->fade = 0;
            *step = OPTSCR_CHOOSE;
        }
        break;
    case OPTSCR_CHOOSE:         /* the option rows: up / down choose, left / right change */
        work->moving = 0;
        if ((g_playerPads[6].pressed & PADRdown) != 0)
        {
            switch (work->row)
            {
            case OPTION_SONG_TEST:      /* play the song */
                work->playing = 1;
                Bgm_Play(work->song,0,0x7f,0x7f);
                break;
            case OPTION_SE_TEST:        /* play the sound effect */
                if (work->slot != -1)
                {
                    Se_StopSlot(work->slot);
                    work->slot = -1;
                }
                work->slot = Se_Play(work->seTest + 0x17,0x7f,0x7f,1);
                break;
            case OPTION_EXIT:
                g_menuResult = MENU_RESULT_BACK;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = OPTSCR_FADE_OUT;
                break;
            case OPTION_XA_TEST:        /* (unreachable: rows stop at 5) XA voice test */
                Xa_Play(1,(u8)work->track);
                break;
            }
            break;
        }
        if ((g_playerPads[6].pressed & PADRright) != 0)
        {
            g_menuResult = MENU_RESULT_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = OPTSCR_FADE_OUT;
            break;
        }
        if ((g_playerPads[6].repeat & (PADLup | PADLdown)) != 0)
        {
            if ((g_playerPads[6].repeat & PADLup) != 0)
            {
                work->row = work->row == OPTION_EVENT_SCENES ? OPTION_EXIT : work->row - 1;
            }
            else
            {
                work->row = work->row == OPTION_EXIT ? OPTION_EVENT_SCENES : work->row + 1;
            }
            if (work->slot != -1)
            {
                Se_StopSlot(work->slot);
                work->slot = -1;
            }
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            break;
        }
        changed = 1;
        if ((g_playerPads[6].repeat & PADLleft) != 0)
        {
            /* left */
            switch (work->row)
            {
            case OPTION_EVENT_SCENES:
                work->optEventScenes = work->optEventScenes == 0 ? 1 : 0;
                g_optEventScenes = work->optEventScenes;
                break;
            case OPTION_CPU_SPEED:
                work->optCpuFast = work->optCpuFast == 0 ? 1 : work->optCpuFast - 1;
                g_optCpuFast = work->optCpuFast;
                break;
            case OPTION_MONO:
                work->optMono = work->optMono == 0 ? 1 : 0;
                g_optMono = work->optMono;
                break;
            case OPTION_SONG_TEST:
                work->song = work->song == 0 ? 22 : work->song - 1;
                break;
            case OPTION_SE_TEST:
                work->seTest = work->seTest == 0 ? 35 : work->seTest - 1;
                break;
            case OPTION_SKIP_CPU_TALK:  /* (unreachable) */
                work->optSkipCpuTalk = work->optSkipCpuTalk == 0 ? 1 : work->optSkipCpuTalk - 1;
                g_optSkipCpuTalk = work->optSkipCpuTalk;
                break;
            case OPTION_XA_TEST:        /* (unreachable) */
                work->track = work->track == 0 ? -0x21 : work->track - 1;
                break;
            default:
                changed = 0;
                break;
            }
            if (changed)
            {
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            break;
        }
        if ((g_playerPads[6].repeat & PADLright) != 0)
        {
            /* right */
            switch (work->row)
            {
            case OPTION_EVENT_SCENES:
                work->optEventScenes = work->optEventScenes == 1 ? 0 : 1;
                g_optEventScenes = work->optEventScenes;
                break;
            case OPTION_CPU_SPEED:
                work->optCpuFast = work->optCpuFast == 1 ? 0 : work->optCpuFast + 1;
                g_optCpuFast = work->optCpuFast;
                break;
            case OPTION_MONO:
                work->optMono = work->optMono == 1 ? 0 : 1;
                g_optMono = work->optMono;
                break;
            case OPTION_SONG_TEST:
                work->song = work->song == 22 ? 0 : work->song + 1;
                break;
            case OPTION_SE_TEST:
                work->seTest = work->seTest == 35 ? 0 : work->seTest + 1;
                break;
            case OPTION_SKIP_CPU_TALK:  /* (unreachable) */
                work->optSkipCpuTalk = work->optSkipCpuTalk == 1 ? 0 : work->optSkipCpuTalk + 1;
                g_optSkipCpuTalk = work->optSkipCpuTalk;
                break;
            case OPTION_XA_TEST:        /* (unreachable) */
                work->track = work->track == -0x21 ? 0 : work->track + 1;
                break;
            default:
                changed = 0;
                break;
            }
            if (changed)
            {
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            break;
        }
        break;
    case OPTSCR_FADE_OUT:
        y = work->fade + 0x40;
        work->fade = y;
        if (0xff < y)
        {
            work->fade = 0x100;
            *step += 1;
        }
        work->item = 0;
        break;
    case OPTSCR_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        y = work->item + 1;
        work->item = y;
        if (y != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = OPTSCR_WAIT_ITEMS_OUT;
        break;
    case OPTSCR_ITEM_OUT_DELAY:
        work->delay--;
        if (work->delay == 0)
        {
            *step = OPTSCR_ITEM_OUT;
        }
        break;
    case OPTSCR_WAIT_ITEMS_OUT:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step += 1;
            break;
        }
        break;
    case OPTSCR_DONE:
        Xa_RequestStop();
        if (work->slot != -1)
        {
            Se_StopSlot(work->slot);
            work->slot = -1;
        }
        if (work->playing == 1)
        {
            Bgm_Play(2,0,0x7f,0x7f);
        }
        work->shown = 0;
        return 1;
    }
    if (work->shown == 0)
    {
        return 0;
    }
    switch(work->row)
    {
    case OPTION_EVENT_SCENES:
        y = 0x28;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    case OPTION_CPU_SPEED:
        y = 0x3c;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    case OPTION_MONO:
        y = 0x50;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    case OPTION_SONG_TEST:
        y = 100;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    case OPTION_SE_TEST:
        y = 0x78;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    case OPTION_EXIT:
        y = 0x8c;
        Menu_DrawBand(y,0x10,work->fade);
        break;
    default:
    }
    if ((work->moving == 0) && (work->fade == 0))
    {
        switch(work->row)
        {
        case OPTION_EVENT_SCENES:
            g_drawPos[1] = 0x28;
            break;
        case OPTION_CPU_SPEED:
            g_drawPos[1] = 0x3c;
            break;
        case OPTION_MONO:
            g_drawPos[1] = 0x50;
            break;
        case OPTION_SONG_TEST:
            g_drawPos[1] = 100;
            break;
        case OPTION_SE_TEST:
            g_drawPos[1] = 0x78;
            break;
        case OPTION_EXIT:
            g_drawPos[1] = 0x8c;
            break;
        case OPTION_SKIP_CPU_TALK:
            g_drawPos[1] = 0xa0;
        }
        g_drawPos[0] = 0x18;
        Sprite_Draw(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    }
    MenuItems_Draw(work->count);
    if (work->fade == 0)
    {
        g_drawPos[0] = 0xb0;
        g_drawPos[1] = 0x28;
        sprite = 0x85;
        if (work->optEventScenes != 0)
        {
            sprite = 0x84;
        }
        Sprite_Draw(g_spriteGroups[5].base + sprite,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
        g_drawPos[0] = 0xb0;
        g_drawPos[1] = 0x3c;
        sprite = 0x98;
        if (work->optCpuFast == 0)
        {
            sprite = 0x97;
        }
        Sprite_Draw(g_spriteGroups[5].base + sprite,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
        g_drawPos[0] = 0xb0;
        g_drawPos[1] = 0x50;
        if (work->optMono == 0)
        {
            id = g_spriteGroups[5].base + 0x3d;
        }
        else
        {
            id = g_spriteGroups[5].base + 0x3b;
        }
        Sprite_Draw(id,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
        g_drawPos[0] = 0xb0;
        g_drawPos[1] = 100;
        Gfx_DrawNumber(work->song,0xb0,100,3,0xc,g_spriteGroups[5].base + 0x52,0,
                       g_ot + g_otIndex,8);
        g_drawPos[0] = 0xb0;
        g_drawPos[1] = 0x78;
        Gfx_DrawNumber(work->seTest,0xb0,0x78,3,0xc,g_spriteGroups[5].base + 0x52,0,
                       g_ot + g_otIndex,8);
    }
    return 0;
}

/* Forget the texts of the list rows (g_menuRowItems) */
static void LoadGame_FreeRowTexts(void)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        Text_Free(g_menuRowItems[i]);
        g_menuRowItems[i] = -1;
    }
}

/* The list: the system data row, the four slots (a used one's text further left), the hand */
static void LoadGame_Draw(LoadGameWork *work)
{
    int i;

    Menu_DrawBand(0x28,0x10,work->fade);
    for (i = 0; i < 4; i++)
    {
        Menu_DrawBand(0x46 + i * 0x28,0x20,work->fade);
        if (work->fade == 0)
        {
            g_drawPos[0] = (work->slotUsed[i] != 0) ? 0x30 : 0x60;
            g_drawPos[1] = 0x56 + i * 0x28;
            Text_Draw(g_menuRowItems[i],g_drawPos[0],0x56 + i * 0x28,g_ot + g_otIndex,8);
        }
    }
    MenuItems_Draw(work->count);
    if (work->fade != 0 || work->saves == 0)
    {
        return;
    }
    g_drawPos[0] = 0x20;
    g_drawPos[1] = (work->slot == 4) ? 0x28 : work->slot * 0x28 + 0x46;
    Sprite_Draw(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
}

/* LoadGame_Task steps */
enum LoadGameStep
{
    LOADGAME_INIT               =   0,
    LOADGAME_ITEM_IN            =   1,  /* the items come in two frames apart */
    LOADGAME_ITEM_DELAY         =   2,
    LOADGAME_WAIT_ITEMS         =   3,
    LOADGAME_FADE_IN            =  10,
    LOADGAME_CHOOSE             =  20,
    LOADGAME_NOTHING            =  30,  /* nothing to load */
    LOADGAME_SYSTEM_LOADED      =  90,  /* "System data load complete." */
    LOADGAME_WAIT_SYSTEM_LOADED =  91,
    LOADGAME_FADE_OUT           = 100,
    LOADGAME_ITEM_OUT           = 101,  /* the items leave two frames apart */
    LOADGAME_ITEM_OUT_DELAY     = 102,
    LOADGAME_WAIT_ITEMS_OUT     = 103,
    LOADGAME_DONE               = 104,
    LOADGAME_READ_CARD          = 200,  /* Card_ReadSavesTask */
    LOADGAME_WAIT_READ          = 201,
    LOADGAME_LIST               = 202,  /* the list of the saves */
    LOADGAME_REMOVED            = 210,  /* the card was taken out */
    LOADGAME_REMOVED_MSG        = 211,  /* "Memory Card was removed. Load failed." */
    LOADGAME_WAIT_REMOVED_MSG   = 212 
};

/* Loading a saved game from the memory card (debug name task data): the four slots and the system
 * data alone (only the unlocked characters and stages: "System data load complete."). Choosing a
 * save resumes it (g_resumedGame). g_menuResult: 0 a save loaded, 1 back (0x8004006C) */
/* 0x8004006C */
u32 LoadGame_Task(u16 *step, LoadGameWork *work)
{
    MenuItem *item;
    int i;

    FntPrint("exec task data\n");
    FntPrint("phase : %d\n", *step);
    switch (*step)
    {
    case LOADGAME_INIT:
        work->checkCard = 0;
        work->shown = 0;
        *step = LOADGAME_READ_CARD;
        for (i = 3; i >= 0; i--)
        {
            g_menuRowItems[i] = -1;
        }
        break;
    case LOADGAME_ITEM_IN:
        /* the items come in two frames apart */
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item == work->count)
        {
            *step = LOADGAME_WAIT_ITEMS;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case LOADGAME_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = LOADGAME_ITEM_IN;
        }
        break;
    case LOADGAME_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = LOADGAME_FADE_IN;
        }
        break;
    case LOADGAME_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->fade = 0;
            work->checkCard = 1;
            *step = (work->saves == 0) ? LOADGAME_NOTHING : LOADGAME_CHOOSE;
        }
        break;
    case LOADGAME_CHOOSE:
        if (g_playerPads[6].pressed & PADRdown)
        {
            Save_SelectSlot(work->slot);
            if (work->slot == 4)
            {
                g_menuResult = MENU_RESULT_BACK;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = LOADGAME_SYSTEM_LOADED;
                break;
            }
            g_gameConfig.playMode = PLAY_NORMAL;
            g_resumedGame = 1;
            g_menuResult = MENU_RESULT_NEXT;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = LOADGAME_FADE_OUT;
        }
        else if (g_playerPads[6].pressed & PADRright)
        {
            g_menuResult = MENU_RESULT_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = LOADGAME_FADE_OUT;
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            if (work->saves < 2)
            {
                break;
            }
            do
            {
                work->slot = (work->slot == 0) ? 4 : work->slot - 1;
            } while (work->slotUsed[work->slot] == 0);
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            if (work->saves < 2)
            {
                break;
            }
            do
            {
                work->slot = (work->slot == 4) ? 0 : work->slot + 1;
            } while (work->slotUsed[work->slot] == 0);
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case LOADGAME_NOTHING:
        /* nothing to load */
        if (g_playerPads[6].pressed & (PADRright | PADRdown))
        {
            g_menuResult = MENU_RESULT_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = LOADGAME_FADE_OUT;
        }
        break;
    case LOADGAME_SYSTEM_LOADED:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,
                 "%CSystem data load complete.%E");
        *step += 1;
        break;
    case LOADGAME_WAIT_SYSTEM_LOADED:
        if (Msg_IsDone())
        {
            *step = LOADGAME_FADE_OUT;
        }
        break;
    case LOADGAME_FADE_OUT:
        work->checkCard = 0;
        work->fade += 0x40;
        if (work->fade > 0xff)
        {
            work->fade = 0x100;
            *step += 1;
        }
        work->item = 0;
        break;
    case LOADGAME_ITEM_OUT:
        /* the items leave two frames apart */
        g_menuItems[work->item].state = MENUITEM_FOLD;
        work->item++;
        if (work->item == work->count)
        {
            *step = LOADGAME_WAIT_ITEMS_OUT;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case LOADGAME_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = LOADGAME_ITEM_OUT;
        }
        break;
    case LOADGAME_WAIT_ITEMS_OUT:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = LOADGAME_DONE;
        }
        break;
    case LOADGAME_DONE:
        work->shown = 0;
        Save_FreeBuffer();
        LoadGame_FreeRowTexts();
        return 1;

    /* 200-202: read the card */
    case LOADGAME_READ_CARD:
        Task_Start(g_cardTaskId,Card_ReadSavesTask);
        *step += 1;
        break;
    case LOADGAME_WAIT_READ:
        if (!Task_IsDone(g_cardTaskId))
        {
            break;
        }
        if (g_cardTaskState == CARDTASK_CANCELED)
        {
            g_menuResult = MENU_RESULT_BACK;
            LoadGame_FreeRowTexts();
            return 1;
        }
        *step += 1;
        break;
    case LOADGAME_LIST:
        work->saves = 0;
        LoadGame_FreeRowTexts();
        Save_DescribeSlot(work->slotUsed,&work->saves);
        work->slotUsed[4] = 1;
        work->shown = 1;
        work->slot = 0;
        work->saves++;
        while (work->slotUsed[work->slot] == 0)
        {
            work->slot = (work->slot == 4) ? 0 : work->slot + 1;
        }
        work->fade = 0x100;
        work->unused02 = 0;
        work->unused04 = 0;
        work->unused03 = 0;
        work->count = 6;
        for (i = 0; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->state = MENUITEM_STILL;
            item->offsetX = 0x140;
            item->offsetY = 0;
            if (i == 0)
            {
                g_menuItems[0].sprite = 100;
                g_menuItems[0].x = 0x10;
                g_menuItems[0].y = 0x10;
            }
            else if (i == 1)
            {
                g_menuItems[1].sprite = 0x40;
                g_menuItems[1].x = 0x30;
                g_menuItems[1].y = 0x28;
            }
            else
            {
                item->sprite = i + 0x3f;
                item->x = 0x30;
                item->y = -10 + i * 0x28;
            }
        }
        work->item = 0;
        *step = LOADGAME_ITEM_IN;
        break;

    /* 210-212: the card was taken out */
    case LOADGAME_REMOVED:
        if (!Msg_IsDone() && g_tasks[(u16)g_uiTaskIds[0]].step >= MSG_STEP_CLOSE)
        {
            g_tasks[(u16)g_uiTaskIds[0]].step = MSG_STEP_CLOSE;
        }
        *step += 1;
        break;
    case LOADGAME_REMOVED_MSG:
        if (Msg_IsDone())
        {
            Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,
                     "%CMemory Card was removed.%KLoad failed.%E");
        }
        *step += 1;
        break;
    case LOADGAME_WAIT_REMOVED_MSG:
        if (Msg_IsDone())
        {
            g_menuResult = MENU_RESULT_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = LOADGAME_FADE_OUT;
        }
        break;
    }
    if (work->checkCard != 0)
    {
        Card_EnableEvents();
        Card_Check();
        if (g_cardStatus > 0 && g_cardStatus < 3)
        {
            work->checkCard = 0;
            *step = LOADGAME_REMOVED;
        }
        Card_DisableEvents();
    }
    if (work->shown != 0)
    {
        LoadGame_Draw(work);
    }
    return 0;
}
