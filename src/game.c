/*
 * game.c - the default settings and board rules, the start-up loading screen
 *
 * Game_InitDefaults sets the options and game settings a session starts with; BoardRules_Init
 * fills the board's rules (g_boardRules) from the game settings when a game starts;
 * Game_LoadCommonTask is the loading screen at start-up.
 */
#include "common.h"
#include "topshop.h"

/* Empty (0x80036848) */
/* 0x80036848 */
void Game_Unused1(void)
{
}

/* Game_LoadCommonTask steps */
enum LoadCommonStep
{
    LOADSCREEN_UPLOAD     = 0,  /* the loading screen's picture to VRAM */
    LOADSCREEN_DRAW       = 1,  /* shown two frames */
    LOADSCREEN_DRAW_AGAIN = 2,
    LOADSCREEN_BLANK_CLUT = 3   /* a blank CLUT row */
};

/* Task: the loading screen while the game starts: the picture packed in the executable
 * (g_loadScreenBuf, its first word the unpacked size) unpacked and put in VRAM at (0x140, 0),
 * shown for two frames, then the blank CLUT row at (1, 0x1FE): 255 entries of 0x8000 (black,
 * not transparent) (0x80036850) */
/* 0x80036850 */
u32 Game_LoadCommonTask(u16 *step)
{
    RECT rect;
    VramImage img;
    short pos[4];
    short block;
    short i;

    switch (*step)
    {
    case LOADSCREEN_UPLOAD:
        Fade_SetClear();
        rect.x = 0x140;
        rect.y = 0;
        rect.w = 0;
        rect.h = 0;
        block = SecHeap_Alloc((int)(g_loadScreenBuf + 0x7ff) / 0x800);
        Lzss_Decompress(&g_loadScreenBuf,g_secHeapBlocks[block].addr);
        Tim_UploadStream(g_secHeapBlocks[block].addr,&rect.x);
        DrawSync(0);
        SecHeap_Free(block);
        *step += 1;
        return 0;
    case LOADSCREEN_DRAW:
    case LOADSCREEN_DRAW_AGAIN:
        img.x = 0x140;
        img.w = 0x140;
        img.h = 0xf0;
        img.mode = IMG_MODE_8BIT;
        img.y = 0;
        img.clutX = 0;
        img.clutY = 500;
        pos[0] = 0;
        pos[1] = 0;
        Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,1);
        *step += 1;
        return 0;
    case LOADSCREEN_BLANK_CLUT:
        g_renderEnabled = 0;
        block = SecHeap_Alloc(1);
        for (i = 0; i < 0xff; i++)
        {
            ((u16 *)g_secHeapBlocks[block].addr)[i] = 0x8000;
        }
        rect.y = 0x1fe;
        rect.x = 1;
        rect.h = 1;
        rect.w = 0xff;
        LoadImage(&rect,g_secHeapBlocks[block].addr);
        DrawSync(0);
        SecHeap_Free(block);
        return 1;
    }
    return 0;
}

/* Empty (0x80036AC4) */
/* 0x80036AC4 */
void Game_Unused2(void)
{
}

/* Default settings, once at start-up (main, after Sys_Init): the options, a one-player game of
 * 2000 to start and 10000 to win, empty player slots, the board rules (0x80036ACC) */
/* 0x80036ACC */
void Game_InitDefaults(void)
{
    int k;
    int i;

    g_optEventScenes = 1;
    g_optSkipCpuTalk = 1;
    g_optCpuFast = 0;
    g_optMono = 0;
    g_optRule38 = 0;
    g_optUnused = 0;
    g_resumedGame = 0;
    g_gameConfig.playMode = PLAY_NORMAL;
    g_gameConfig.unused01 = 10;
    g_gameConfig.numPlayers = 1;
    g_gameConfig.bankruptLimit = 1;
    g_gameConfig.startMoney = 2000;
    g_gameConfig.goalMoney = 10000;
    g_gameConfig.startGoods = 3;
    g_gameConfig.numHumans = 1;
    g_gameConfig.numCpus = 0;
    g_gameConfig.stage = 0;
    g_gameConfig.maxBuy = 2;
    for (k = 0; k < GAME_MAX_PLAYERS; k++)
    {
        g_gameConfig.players[k].charId = 0xff;
        g_gameConfig.players[k].pad = 0;
        for (i = 0; i < 6; i++)
        {
            g_gameConfig.players[k].name[i] = 0;
        }
    }
    BoardRules_Init();
}

/* Fill g_boardRules from g_gameConfig and the defaults (rules 0-3 on, the others off) (0x80036DAC) */
/* 0x80036DAC */
void BoardRules_Init(void)
{
    int i;

    g_boardRules.unused0A = 0;
    g_boardRules.unused0B = 1;
    for (i = 0; i < 6; i++)
    {
        g_boardRules.unused0E[i].index = i;
        g_boardRules.unused0E[i].flag = 1;
    }
    g_boardRules.unused0D = 2;
    g_boardRules.unused0E[0].flag = 0;
    /* the squares on, the rest off (the memcpy also clears 4) */
    memcpy(g_boardRules.enable,"\x01\x01\x01\x01",5);
    g_boardRules.enable[RULE_STORES_OPEN] = 0;
    g_boardRules.enable[RULE_POST_EVENT] = 0;
    g_boardRules.enable[RULE_PICK_EVENT] = 0;
    g_boardRules.enable[RULE_SLOW_DICE] = 0;
    g_boardRules.enable[RULE_RESET_DIR] = 0;
    g_boardRules.enable[RULE_DEBUG_POINTS] = 0;
    g_boardRules.enable[RULE_ALWAYS_TALK] = 0;
    g_boardRules.enable[RULE_AUTO_VACANT] = 0;
    g_boardRules.enable[RULE_AUTO_STORE] = 0;
    g_boardRules.enable[RULE_AUTO_MINUS] = 0;
    g_boardRules.enable[RULE_AUTO_OWN] = 0;
    g_boardRules.enable[RULE_AUTO_OTHERS] = 0;
    g_boardRules.enable[RULE_AUTO_EXCHANGE] = 0;
    g_boardRules.enable[RULE_AUTO_EVENT] = 0;
    g_boardRules.enable[RULE_AUTO_MOVE] = 0;
    g_boardRules.enable[RULE_AUTO_ELEVATOR] = 0;
    g_boardRules.startMoney = g_gameConfig.startMoney;
    g_boardRules.goalMoney = g_gameConfig.goalMoney;
    g_boardRules.bankruptLimit = g_gameConfig.bankruptLimit;
    g_boardRules.startGoods = g_gameConfig.startGoods;
    g_boardRules.unused38 = g_optRule38;
    g_boardRules.unused39[0] = 0;
    g_boardRules.unused39[1] = 0;
    g_boardRules.unused39[2] = 0;
    g_boardRules.unused39[3] = 0;
    g_boardRules.unused39[6] = 0;
    g_boardRules.unused39[7] = 0;
    g_boardRules.unused39[4] = 0;
    g_boardRules.unused39[5] = 0;
    g_boardStage = 0;
    g_numPlayers = 1;
    g_players[0].charId = 0;
}
