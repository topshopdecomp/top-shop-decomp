/*
 * boardsetup.c - setting up the board
 *
 * What the board game loads and creates before and between turns: the board's sector-heap blocks
 * and its persistent tasks, the stage's map planes and sprites, the players' pieces and faces
 * (CHR<char>P0.J, KAO<char>P.J), the players and stores of a new or resumed game, the season's
 * and the month's graphics, the calendar, the end of timed effects at the start of a turn, and
 * pausing and resuming the display tasks around full-screen scenes.
 */
#include "common.h"
#include "topshop.h"

/* Allocate board sector-heap blocks and create the board's persistent tasks (display, script,
 * loader, dice, move, 4 action slots, 8 UI slots whose work areas start with their slot number),
 * set up the scene OTs and the two tile sheets. g_vsyncInterval = 2 (30 fps). (0x80047DC0) */
/* 0x80047DC0 */
void Board_CreateTasks(void)
{
    int i;

    g_vsyncInterval = 2;
    g_faceBlock = SecHeap_Alloc(0x48);
    g_stageBlock = SecHeap_Alloc(0x10);
    g_seasonBlock = SecHeap_Alloc(0x10);
    g_boardExtraBlock = SecHeap_Alloc(0x10);
    g_mapViewTaskId = Task_Create(1,NULL);
    g_boardMiscTaskId = Task_Create(1,NULL);
    g_piecesTaskId = Task_Create(1,NULL);
    g_diceTaskId = Task_Create(1,NULL);
    g_minimapTaskId = Task_Create(1,NULL);
    g_playerPanelTaskId = Task_Create(1,&g_playerPanelWork);
    g_squareInfoTaskId = Task_Create(1,&g_squareInfoWork);
    g_calendarTaskId = Task_Create(1,NULL);
    g_moveTaskId = Task_Create(1,NULL);
    g_turnFxTaskId = Task_Create(1,NULL);
    g_stageBgTaskId = Task_Create(1,NULL);
    g_scriptTaskId = Task_Create(2,&g_scriptWork);
    g_farBgTaskId = Task_Create(3,NULL);
    g_cardTaskId = Task_Create(3,NULL);
    g_frameEndTaskId = Task_Create(3,NULL);
    g_loaderTaskId = Task_Create(3,&g_loaderWork);
    for (i = 0; i < 4; i++)
    {
        g_actionTaskIds[i] = Task_Create(1,&g_actionTaskWork[i]);
    }
    for (i = 0; i < 8; i++)
    {
        g_uiTaskIds[i] = Task_Create(1,&g_uiTaskWork[i]);
        g_uiTaskWork[i][0] = i;
    }
    g_storesTaskId = Task_Create(1,NULL);
    g_fadeR = -0xff000;
    g_fadeG = -0xff000;
    g_fadeB = -0xff000;
    Fade_Init();
    g_sceneOt[0].org = (GsOT_TAG *)g_sceneOtTags;
    g_sceneOt[1].org = (GsOT_TAG *)(g_sceneOtTags + 0x40);
    g_sceneOt[0].length = 6;
    g_sceneOt[1].length = 6;
    /* the portraits and the scenes share one tile sheet area in VRAM (0x280, 0) and its frames;
     * their images go through the second half of the text work block */
    g_portraitSheet.frames = g_sheetFrames;
    g_portraitFace = 0xffff;
    g_portraitSheet.vramX = 0x280;
    g_portraitSheet.vramY = 0;
    g_portraitSheet.full = 0;
    g_portraitSheet.nextCell = 0;
    g_portraitSheet.count = 0;
    g_sceneSheet.frames = g_sheetFrames;
    g_portraitSheet.stage = (u8 *)g_secHeapBlocks[g_fontWorkBlock].addr + 0x8000;
    g_sceneSheet.vramX = 0x280;
    g_sceneSheet.vramY = 0;
    g_sceneSheet.full = 0;
    g_sceneSheet.nextCell = 0;
    g_sceneSheet.count = 0;
    g_sceneSheet.stage = (u8 *)g_secHeapBlocks[g_fontWorkBlock].addr + 0x8000;
}

/* Set up background plane `plane` on the stage map archive (STG<stage>.J: entry 0 the map: +4
 * width, height in tiles, tile width, height, +8 the cells; entry 1 the tiles, +8) with its
 * ordering tables and buffers, and build it */
static void Board_InitPlane(BgPlane *plane, u8 *stage, GsOT_TAG *tags0, GsOT_TAG *tags1,
                            BgCell *visible, POLY_FT4 *prims0, POLY_FT4 *prims1)
{
    u8 *map = ARCHIVE_ENTRY(stage,0);
    BgMap *m = &plane->map;

    plane->ot[0].org = tags0;
    plane->ot[0].length = 1;
    plane->ot[1].org = tags1;
    plane->ot[1].length = 1;
    GsClearOt(0,0,&plane->ot[0]);
    GsClearOt(0,0,&plane->ot[1]);
    m->mapW = map[4];
    m->mapH = map[5];
    m->tileW = map[6];
    m->tileH = map[7];
    m->cells = (u16 *)(map + 8);
    m->ot[0] = &plane->ot[0];
    m->ot[1] = &plane->ot[1];
    m->scrollX = 0;
    m->scrollY = 0;
    m->tiles = (BgTile *)(ARCHIVE_ENTRY(stage,1) + 8);
    m->visible = visible;
    m->prims[0] = prims0;
    m->prims[1] = prims1;
    m->tpageBase = 0x80;
    /* the tiles covering the 320 x 240 screen, one more for a partly shown one when scrolled */
    m->cols = 320 / m->tileW + (320 % m->tileW == 0 ? 1 : 2);
    m->rows = 240 / m->tileH + (240 % m->tileH == 0 ? 1 : 2);
    m->col = 0;
    m->row = 0;
    BgMap_Build(m);
}

/* Board mode setup: load TALK.J (character scripts), the stage's sprites and map (STG<stage>.J),
 * the players' pieces (CHR<char>P0.J) and faces (KAO<char>P.J) (0x800480D4) */
/* 0x800480D4 */
void Board_LoadData(void)
{
    u8 *stage;
    u8 *chr;
    u8 *face;
    short block;
    int i;
    short vramOffsets[4];
    RECT rect;

    g_spriteGroups[2].next = g_spriteGroups[2].base;
    g_spriteGroups[3].next = g_spriteGroups[3].base;
    g_spriteGroups[1].next = g_spriteGroups[1].base;
    g_charScriptsBlock = g_fileHandles[File_ReadSync(FILE_TALK_J,NULL,FILE_WHOLE)].block;
    g_cameraX = 0;
    g_cameraY = 0;
    g_bgWaveX = 0;
    g_bgWaveY = 0;
    g_cameraTargetX = 0;
    g_cameraTargetY = 0;
    g_bgScrollX = 0;
    g_bgScrollY = 0;
    g_bg1ScrollX = 0;
    g_bg1ScrollY = 0;
    g_farBgScrollX = 0;
    g_farBgScrollY = 0;

    /* the stage's sprites (S<stage>_EXC1.SPC) and the far background of some stages */
    File_LoadPackedSprites(g_boardStage + FILE_S00_EXC1_SPC,2);
    switch (g_boardStage)
    {
    case 0:
        File_LoadPackedSprites(FILE_SF00_00_SPC,1);
        break;
    case 1:
        File_LoadPackedSprites(FILE_SF01_00_SPC,1);
        break;
    case 2:
        File_LoadPackedSprites(FILE_SF02_00_SPC,1);
        break;
    case 5:
        File_LoadPackedSprites(FILE_SF05_00_SPC,1);
        break;
    case 8:
    case 9:
        File_LoadTim(g_boardStage == 8 ? FILE_SF08_00_TIM : FILE_SF09_00_TIM);
        break;
    }

    /* the map, on both planes */
    File_ReadSync(g_boardStage * 5 + FILE_STG00_J,g_secHeapBlocks[g_stageBlock].addr,FILE_WHOLE);
    stage = g_secHeapBlocks[g_stageBlock].addr;
    Board_InitPlane(&g_bgPlanes[0],stage,g_bg0OtTags0,g_bg0OtTags1,g_bg0Visible,g_bg0Prims0,
                    g_bg0Prims1);
    Board_InitPlane(&g_bgPlanes[1],stage,g_bg1OtTags0,g_bg1OtTags1,g_bg1Visible,g_bg1Prims0,
                    g_bg1Prims1);
    g_bgOt0[0].org = g_bgOt0Tags0;
    g_bgOt0[1].org = g_bgOt0Tags1;
    g_bgOt0[0].length = 2;
    g_bgOt0[1].length = 2;
    g_bgOt1[0].org = g_bgOt1Tags0;
    g_bgOt1[1].org = g_bgOt1Tags1;
    g_bgOt1[0].length = 2;
    g_bgOt1[1].length = 2;
    g_boardOt[0].org = g_boardOtTags0;
    g_boardOt[0].length = 2;
    g_boardOt[1].org = g_boardOtTags1;
    g_boardOt[1].length = 2;
    GsClearOt(0,0,&g_boardOt[0]);
    GsClearOt(0,0,&g_boardOt[1]);
    g_boardOtOn = 1;
    DrawSync(0);
    g_frontOt[0].org = g_frontOtTags0;
    g_frontOt[0].length = 4;
    g_frontOt[1].org = g_frontOtTags1;
    g_frontOt[1].length = 4;
    g_frontOtOn = 1;

    /* the pieces: CHR<char>P0.J entry 0 has a 0x14-byte header, then a 16-color palette per
     * player slot (player i's goes to (0x140 + 16 * i, 0xF2)); entry 1 is the sprite set */
    rect.x = 0x140;
    rect.y = 0xf2;
    rect.w = 0x10;
    rect.h = 1;
    vramOffsets[3] = 0;
    vramOffsets[2] = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        block = g_fileHandles[File_ReadSync(g_players[i].charId + FILE_CHR0P0_J,NULL,FILE_WHOLE)].block;
        chr = g_secHeapBlocks[block].addr;
        LoadImage(&rect,(u_long *)(ARCHIVE_ENTRY(chr,0) + 0x14 + i * 0x20));
        vramOffsets[0] = i * 8 + 0x140;
        vramOffsets[1] = 0x100;
        Sprite_LoadSet((int *)ARCHIVE_ENTRY(chr,1),3,vramOffsets);
        DrawSync(0);
        rect.x += 0x10;
        SecHeap_Free(block);
    }

    /* the faces, one after the other in g_faceBlock */
    face = g_secHeapBlocks[g_faceBlock].addr;
    for (i = 0; i < g_numPlayers; i++)
    {
        g_faceData[i] = face;
        File_ReadSync(g_players[i].charId + FILE_KAO0P_J,face,FILE_WHOLE);
        face += File_GetSectors(g_players[i].charId + FILE_KAO0P_J) * 0x800;
    }
    g_eventDrawn = 0;
    if ((g_boardRules.enable[RULE_STORES_OPEN] != 0) || (g_resumedGame != 0))
    {
        Stores_LoadImages();
    }
}

/* Set up players and stores for a new board game from g_gameConfig and the stage definition (a
 * resumed game then takes the saved state, Save_Unpack) (0x80048C28) */
/* 0x80048C28 */
void Board_InitGame(void)
{
    PlayerSetup *setup;
    StageDef *stage;
    Player *pl;
    Store *st;
    short cell;
    short squareId;
    int run;
    int owner;
    int i;
    int j;

    g_gameConfig.demoTimer = 0x708;
    g_gameConfig.year = 0;
    g_gameConfig.month = 0;
    g_gameConfig.monthsElapsed = 0;
    for (i = NUM_STORE_TYPES - 1; i >= 0; i--)
    {
        g_storeTypes[i].available = 1;
    }
    g_boardStage = g_gameConfig.stage;
    for (i = 0; i < MAX_STORES; i++)
    {
        st = &g_stores[i];
        st->owner = -1;
        st->chainLen = 0;
        st->index = i;
        st->chainPos = 0;
        st->squareFlag = 0;
        st->tempClosed = 0;
        st->closedUntil = 0;
        st->numGoods = g_gameConfig.startGoods;
        st->priceChanged = 0;
        st->pricePercent = 100;
        st->priceUntil = 0;
        st->maxGoods = g_kindMaxGoods[st->chainLen];
        for (j = 0; j < 8; j++)
        {
            st->goods[j].active = 0;
            st->goods[j].level = 0;
        }
        st->type = NULL;
        g_storeImageState[i] = STORE_IMAGE_NONE;
    }

    /* the players, in the turn order */
    g_curPlayer = 0;
    g_numPlayers = g_gameConfig.numPlayers;
    g_numActivePlayers = g_gameConfig.numPlayers;
    for (i = 0; i < 6; i++)
    {
        pl = &g_players[i];
        setup = &g_gameConfig.players[(char)g_gameConfig.turnOrder[i]];
        stage = &g_stageDefs[g_boardStage];
        strncpy(pl->name,setup->name,13);
        strncpy(pl->title,setup->name,13);
        pl->index = i;
        pl->charId = setup->charId;
        pl->isCpu = setup->isCpu;
        pl->out = 0;
        pl->pieceShown = 1;
        pl->choosing = 0;
        pl->color = i;
        pl->pad = setup->pad;
        pl->x = stage->startX;
        pl->floor = stage->startFloor;
        pl->dirX = 0;
        pl->dirFloor = 0;
        pl->panel = i;
        pl->screenX = (stage->originX + pl->x * 5) * 16 + 0x28;
        pl->screenY = (stage->originY + pl->floor * 5) * 16 + 0x3a;
        pl->pieceState = 0xd2;
        pl->stepsSincePayday = 0;
        pl->totalSteps = 0;
        pl->money = g_gameConfig.startMoney;
        pl->moneyShown = g_gameConfig.startMoney;
        pl->historyLen = 0;
        pl->noTargeting = 0;
        pl->paydayEffect = 0;
        pl->diceEffect = 0;
        pl->diceEffectUntil = 0;
        pl->noElevator = 0;
        pl->noElevatorUntil = 0;
        pl->noDelivery = 0;
        pl->noDeliveryUntil = 0;
        pl->vandalized = 0;
        pl->vandalizedUntil = 0;
        pl->exempt = 0;
        pl->shoes = 0;
        pl->shopHistory = 0;
        pl->passedHalfGoal = 0;
        pl->inElevator = 0;
        pl->paydays = 0;
        pl->customers = 0;
        pl->purchases = 0;
        pl->assets = g_gameConfig.startMoney;
        pl->assetsShown = g_gameConfig.startMoney;
        pl->points = (g_boardRules.startPoints == 0xff) ? 99 : g_boardRules.startPoints;
    }
    if (g_resumedGame != 0)
    {
        Save_Unpack();
        Board_Unused();
    }

    /* each store's square */
    stage = &g_stageDefs[g_boardStage];
    for (i = 0; i < stage->floors; i++)
    {
        for (j = 0; j < g_stageDefs[g_boardStage].width; j++)
        {
            cell = Board_GetCell(j,i);
            if (CELL_TYPE((u16)cell) == CELL_STORE)
            {
                g_stores[cell].x = j;
                g_stores[cell].floor = i;
                g_stores[cell].squareId = Board_GetSquareId(j,i);
                g_stores[cell].squareFlag = ((char)Board_GetSquareFlag(j,i) == 2) ? 1 : 0;
            }
        }
        stage = &g_stageDefs[g_boardStage];
    }

    /* debug rule: every store open, the stores of a square shared out among the players */
    if (g_boardRules.enable[RULE_STORES_OPEN] != 0)
    {
        squareId = -1;
        run = 0;
        owner = 0;                              /* (unset in the original before its first use) */
        for (i = 0; i < g_stageDefs[g_boardStage].numStores; i++)
        {
            run++;
            if (squareId != g_stores[i].squareId)
            {
                run = 0;                            /* the first store of a square */
            }
            squareId = g_stores[i].squareId;
            switch (g_numPlayers)
            {
            case 1:
                owner = 0;
                break;
            case 2:
                owner = (short)run / 2;
                break;
            case 3:
                if ((short)run == 2)
                {
                    owner = 1;
                }
                else if ((short)run == 3)
                {
                    owner = 2;
                }
                else if (run >= 0 && run < 2)
                {
                    owner = 0;
                }
                break;
            case 4:
            case 5:
            case 6:
                owner = run;
                break;
            }
            Store_Open(i,(owner + squareId * 4),owner);
        }
    }
    Stores_UpdateChains();
    Stores_UpdateOwned();
    Players_Rank();
    Stores_TakeSnapshot();
    for (i = 0; i < g_numPlayers; i++)
    {
        g_players[i].moneyShown = g_players[i].money;
        g_players[i].assetsShown = g_players[i].assets;
    }
    /* the controllers: by the setup slots, then again by the players' order */
    for (i = 0; i < 6; i++)
    {
        Pad_Assign(i,g_gameConfig.players[i].pad);
    }
    Board_FocusCurrentPlayer();
    for (i = 0; i < g_numPlayers; i++)
    {
        Pad_Assign(i,g_players[i].pad);
    }
}

/* Start the board display tasks (0x800B26A8, 26D2, 26E0, 26E4, 26E8, 26F0, 270E, 2710).
 * (0x80049404) */
/* 0x80049404 */
void Board_StartDisplayTasks(void)
{
    void *func;

    Task_Start(g_piecesTaskId,Board_DrawPiecesTask);
    Task_Start(g_storesTaskId,Board_DrawStoresTask);
    Task_Start(g_minimapTaskId,Minimap_Task);
    Task_Start(g_playerPanelTaskId,PlayerPanel_Task);
    Task_Start(g_squareInfoTaskId,SquareInfo_Task);
    Task_Start(g_calendarTaskId,Hud_CalendarTask);
    Task_Start(g_farBgTaskId,Board_FarBackgroundTask);
    if (g_boardStage == 8)
    {
        func = Bg_Stage8Task;
    }
    else
    {
        if (g_boardStage != 9)
        {
            return;
        }
        func = Bg_Stage9Task;
    }
    Task_Start(g_stageBgTaskId,func);
}

/* Load SB<stage>_0<season>.SPC to VRAM and STG<stage>_0<season>.J
 * into g_seasonBlock for plane 1. season = ((month + 1) / 3) % 4. (0x800494F4) */
/* 0x800494F4 */
void Board_LoadSeasonGraphics(unsigned int season)
{
    u8 *stage;

    season &= 0xff;
    File_LoadPackedImages(g_boardStage * 4 + season + FILE_SB00_00_SPC);
    File_ReadSync(g_boardStage * 5 + season + FILE_STG00_00_J,g_secHeapBlocks[g_seasonBlock].addr,
                  FILE_WHOLE);
    /* the season's map and tiles on plane 1 (archive layout: Board_InitPlane) */
    stage = g_secHeapBlocks[g_seasonBlock].addr;
    g_bgPlanes[1].map.cells = (u16 *)(ARCHIVE_ENTRY(stage,0) + 8);
    g_bgPlanes[1].map.tiles = (BgTile *)(ARCHIVE_ENTRY(stage,1) + 8);
    g_bgPlanes[1].map.mapW = ARCHIVE_ENTRY(stage,0)[4];
    g_bgPlanes[1].map.mapH = ARCHIVE_ENTRY(stage,0)[5];
    BgMap_Build(&g_bgPlanes[1].map);
}

/* Upload BGCM<n>.SPC for a calendar month (month 0 = April -> file 3) (0x800496AC) */
/* 0x800496AC */
void Board_LoadMonthGraphics(u32 month)
{
    month &= 0xff;
    /* month 0 = April: BGCM04.SPC */
    File_LoadPackedImages(FILE_BGCM01_SPC + (month + 3 < 12 ? month + 3 : month - 9));
}

/* Load a character's piece sprites (g_pieceSpriteFiles) into sprite group 3 after its first 30
 * (0x800497BC) */
/* 0x800497BC */
void Board_LoadPieceSprites(u32 charId)
{
    g_spriteGroups[3].next = g_spriteGroups[3].base + 0x1e;
    File_LoadPackedSprites(g_pieceSpriteFiles[charId & 0xff],3);
}

/* Start of a turn: end store closures and player penalties whose turn count has passed, update the
 * stores, rank the players (0x800498C8) */
/* 0x800498C8 */
void Board_ExpireEffects(void)
{
    Store *st;
    Player *pl;
    int i;
    u32 now;

    now = TURN_NUMBER(g_gameConfig.monthsElapsed,g_curPlayer);   /* this turn */
    for (i = 0; i < NUM_STORES; i++)
    {
        st = &g_stores[i];
        if ((st->priceChanged != 0) && (st->priceUntil < now))
        {
            st->priceChanged = 0;
            st->priceUntil = 0;
            st->pricePercent = 100;
        }
        if ((st->tempClosed != 0) && (st->closedUntil < now))
        {
            st->tempClosed = 0;
            st->closedUntil = 0;
        }
    }
    for (i = 0; i < g_numPlayers; i++)
    {
        pl = &g_players[i];
        if (pl->out == 0)
        {
            Player_FindNearestBank(i);
            pl->bankDistance = g_bankDistance;
            if ((pl->noElevator != 0) && (pl->noElevatorUntil < now))
            {
                pl->noElevator = 0;
                pl->noElevatorUntil = 0;
            }
            if ((pl->diceEffect != 0) && (pl->diceEffectUntil < now))
            {
                pl->diceEffect = 0;
                pl->diceEffectUntil = 0;
            }
            if ((pl->noDelivery != 0) && (pl->noDeliveryUntil < now))
            {
                pl->noDelivery = 0;
                pl->noDeliveryUntil = 0;
            }
            if ((pl->vandalized != 0) && (pl->vandalizedUntil < now))
            {
                pl->vandalized = 0;
                pl->vandalizedUntil = 0;
            }
            pl->prevRankOrder = pl->rankOrder;
            pl->prevRank = pl->rank;
        }
    }
    /* the current player's CPU plan is forgotten, its shopping history moves on a turn */
    g_players[g_curPlayer].cpuPlan = 0;
    g_players[g_curPlayer].cpuPlanArg = 0;
    g_players[g_curPlayer].shopHistory <<= 1;
    Stores_UpdateOwned();
    Players_Rank();
}

/* Free the character script block (0x80049B38) */
/* 0x80049B38 */
void Board_FreeScripts(void)
{
    SecHeap_Free(g_scriptBlock);
}

/* Disable the board overlay ordering tables (see Board_FrameStub) (0x80049B5C) */
/* 0x80049B5C */
void Board_ResetOverlays(void)
{
    g_frontOtOn = 0;
    g_frontOtUsed = 0;
    g_boardOtOn = 0;
    g_boardOtUsed = 0;
}

/* Task_Suspend the 8 board display tasks. (0x80049B84) */
/* 0x80049B84 */
void Board_SuspendDisplayTasks(void)
{
    Task_Suspend(g_piecesTaskId);
    Task_Suspend(g_storesTaskId);
    Task_Suspend(g_minimapTaskId);
    Task_Suspend(g_playerPanelTaskId);
    Task_Suspend(g_squareInfoTaskId);
    Task_Suspend(g_calendarTaskId);
    Task_Suspend(g_stageBgTaskId);
    Task_Suspend(g_farBgTaskId);
}

/* Board_SuspendDisplayTasks with task in place of the first display task (0x80049B8C) */
/* 0x80049B8C */
void Board_SuspendDisplayTasksWith(short task)
{
    Task_Suspend(task);
    Task_Suspend(g_storesTaskId);
    Task_Suspend(g_minimapTaskId);
    Task_Suspend(g_playerPanelTaskId);
    Task_Suspend(g_squareInfoTaskId);
    Task_Suspend(g_calendarTaskId);
    Task_Suspend(g_stageBgTaskId);
    Task_Suspend(g_farBgTaskId);
}

/* Task_Resume the 8 board display tasks. (0x80049C1C) */
/* 0x80049C1C */
void Board_ResumeDisplayTasks(void)
{
    Task_Resume(g_piecesTaskId);
    Task_Resume(g_storesTaskId);
    Task_Resume(g_minimapTaskId);
    Task_Resume(g_playerPanelTaskId);
    Task_Resume(g_squareInfoTaskId);
    Task_Resume(g_calendarTaskId);
    Task_Resume(g_stageBgTaskId);
    Task_Resume(g_farBgTaskId);
}

/* Board_ResumeDisplayTasks with task in place of the first display task (0x80049C24) */
/* 0x80049C24 */
void Board_ResumeDisplayTasksWith(short task)
{
    Task_Resume(task);
    Task_Resume(g_storesTaskId);
    Task_Resume(g_minimapTaskId);
    Task_Resume(g_playerPanelTaskId);
    Task_Resume(g_squareInfoTaskId);
    Task_Resume(g_calendarTaskId);
    Task_Resume(g_stageBgTaskId);
    Task_Resume(g_farBgTaskId);
}

/* Empty (0x80049F00) */
/* 0x80049F00 */
void Board_Unused(void)
{
}

/* Load C_BG<stage>.SPC (LZSS image stream) and upload it to VRAM. (0x80049F08) */
/* 0x80049F08 */
void Board_LoadBackground(unsigned int stage)
{
    File_LoadPackedImages((stage & 0xff) + FILE_C_BG00_SPC);
}

/* Advance the game calendar: month = (month + n) % 12, year += carry, monthsElapsed += n. Called
 * with 1 when the last player of a round finishes (one round = one month). (0x8007B2A8) */
/* 0x8007B2A8 */
void Calendar_Advance(int months)
{
    signed char total;

    total = g_gameConfig.month + months;
    g_gameConfig.monthsElapsed += months;
    g_gameConfig.year += total / 12;
    g_gameConfig.month = total % 12;
}
