/*
 * title.c - game mode 2: the title screen
 *
 * The title screen, its animations and menu effects, the intro, and the attract demo (a game of
 * CPU players) that starts when nobody presses a button.
 */
#include "common.h"
#include "topshop.h"

/* Load the title screen graphics: the sprites of OPEN2-4.SPC (group 6), the character picture
 * OPEN5.J (its CLUT and sprite set, group 3), OPEN5.SPC after it, and CORNER.TIM (0x80040AB8) */
/* 0x80040AB8 */
void Title_LoadGraphics(void)
{
    short block;
    void *data;
    RECT clut;
    short vramPos[4];

    g_spriteGroups[6].next = g_spriteGroups[6].base;
    File_LoadPackedSprites(FILE_OPEN2_SPC,6);
    File_LoadPackedSprites(FILE_OPEN3_SPC,6);
    File_LoadPackedSprites(FILE_OPEN4_SPC,6);
    clut.x = 0x140;
    clut.y = 0xf2;
    clut.w = 0x10;
    clut.h = 1;
    vramPos[3] = 0;
    vramPos[2] = 0;
    block = g_fileHandles[File_ReadSync(FILE_OPEN5_J,NULL,FILE_WHOLE)].block;
    data = g_secHeapBlocks[block].addr;
    LoadImage(&clut,CHARFILE_CLUT(data,0));
    vramPos[0] = 0x140;
    vramPos[1] = 0x100;
    Sprite_LoadSet(CHARFILE_IMAGE(data,0),3,vramPos);
    DrawSync(0);
    SecHeap_Free(block);
    g_spriteGroups[3].next = g_spriteGroups[3].base + 0x1e;
    File_LoadPackedSprites(FILE_OPEN5_SPC,3);
    File_LoadTim(FILE_CORNER_TIM);
}

/* The attract demo's game: four CPU players (all six setups filled with the characters in order
 * and their names, g_cpuNames), a random board of the first eight, 3000 to start and 10000 to win
 * (GameConfig.playMode 2) */
static void Title_SetupDemo(void)
{
    int i;

    g_gameConfig.numCpus = 4;
    g_gameConfig.numHumans = 0;
    g_gameConfig.numPlayers = 4;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
    {
        g_gameConfig.turnOrder[i] = i;
        g_gameConfig.players[i].isCpu = 1;
        g_gameConfig.players[i].charId = i;
        strcpy(g_gameConfig.players[i].name,g_cpuNames[i]);
    }
    g_gameConfig.stage = rand() % 8;
    g_gameConfig.goalMoney = 10000;
    g_gameConfig.startMoney = 3000;
    g_gameConfig.playMode = PLAY_DEMO;
    g_gameMode = MODE_BOARD;
    g_resumedGame = 0;
}

/* Title_Task steps */
enum TitleStep
{
    TITLE_LOAD       =   0,
    TITLE_MENU       =  10,  /* TitleMenu_Task */
    TITLE_WAIT_MENU  =  11,  /* on its TITLE_RESULT_* */
    TITLE_INTRO      =  20,  /* Title_SplashTask */
    TITLE_WAIT_INTRO =  21,  /* not skipped: the attract demo */
    TITLE_DEMO       =  30,  /* Title_SetupDemo */
    TITLE_DEMO_2     =  40,  /* (as TITLE_DEMO; nothing sets this step) */
    TITLE_DONE       = 100 
};

/* TitleMenu_Task's user data: how the title screen ended */
#define TITLE_RESULT_IDLE    0   /* nobody pressed START for 30 seconds: the intro */
#define TITLE_RESULT_START   1   /* START: a new game */
#define TITLE_RESULT_UNUSED  2   /* (never set) game mode 10 */

/* Title_SplashTask's user data */
#define SPLASH_SHOWN    0
#define SPLASH_SKIPPED  1        /* a button cut it short */

/* Game mode 2: title screen. Runs TitleMenu_Task; START -> g_gameMode 5 (new game setup). 30 s
 * idle -> the intro (g_gameMode 3, Title_SplashTask) then, if it was not skipped, a CPU-only
 * attract game (Title_SetupDemo) -> g_gameMode 6. Menu result 2 -> mode 10 (unused).
 * (0x80040EB0) */
/* 0x80040EB0 */
int Title_Task(short *step, void *work)
{
    void *func;

    Gfx_QueueClear(0,0,0);
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", (u16)*step);
    g_debugPrint = 1;
    switch ((u16)*step)
    {
    case TITLE_LOAD:
        g_newGameStarted = 0;
        Mode_CreateSubTasks();      /* inline in the original */
        Title_LoadGraphics();
        *step = TITLE_MENU;
        return 0;
    case TITLE_MENU:
        func = TitleMenu_Task;
        *(u32 *)work = 1800;        /* frames before the intro */
        g_gameMode = MODE_TITLE;
        break;
    case TITLE_WAIT_MENU:
        if (!Task_IsDone(g_subTaskIds[0]))
        {
            return 0;
        }
        switch (Task_GetUserData(g_subTaskIds[0]))
        {
        case TITLE_RESULT_IDLE:
            *step = TITLE_INTRO;
            break;
        case TITLE_RESULT_START:
            g_gameMode = MODE_NEW_GAME;
            *step = TITLE_DONE;
            break;
        case TITLE_RESULT_UNUSED:
            g_gameMode = MODE_UNUSED10;
            *step = TITLE_DONE;
            break;
        }
        return 0;
    case TITLE_INTRO:
        func = Title_SplashTask;
        g_gameMode = MODE_TITLE_INTRO;
        break;
    case TITLE_WAIT_INTRO:
        if (!Task_IsDone(g_subTaskIds[0]))
        {
            return 0;
        }
        *step = (Task_GetUserData(g_subTaskIds[0]) == SPLASH_SHOWN) ? TITLE_DEMO : TITLE_MENU;
        return 0;
    case TITLE_DEMO:
    case TITLE_DEMO_2:
        Title_SetupDemo();
        *step = TITLE_DONE;
        return 0;
    case TITLE_DONE:
        Bgm_Stop();
        return 1;
    default:
        g_debugPrint = 1;
        return 0;
    }
    Task_Start(g_subTaskIds[0],func);
    *step += 1;
    return 0;
}

/* Title_SplashTask steps */
enum TitleSplashStep
{
    SPLASH_INIT          =   0,
    SPLASH_FADE_IN       =  10,
    SPLASH_WAIT_FADE_IN  =  11,
    SPLASH_SHOW          =  20,  /* 180 frames */
    SPLASH_WAIT          =  21,  /* a button skips it */
    SPLASH_FADE_OUT      = 100,
    SPLASH_WAIT_FADE_OUT = 101 
};

/* Task: show a picture (sprite set 6) between fades for a while; a button skips it (user data 1)
 * (0x8004126C) */
/* 0x8004126C */
u32 Title_SplashTask(u16 *step, TitleSplashWork *work)
{
    int faded;

    switch (*step)
    {
    case SPLASH_INIT:
        Task_SetUserData(-1,SPLASH_SHOWN);
        work->started = 1;
        Fade_SetBlack();
        *step = SPLASH_FADE_IN;
        break;
    case SPLASH_FADE_IN:
        Fade_In(0xf);
        *step += 1;
        faded = Fade_IsDone();
        if (faded != 0)
        {
            *step = SPLASH_SHOW;
        }
        break;
    case SPLASH_WAIT_FADE_IN:
        faded = Fade_IsDone();
        if (faded != 0)
        {
            *step = SPLASH_SHOW;
        }
        break;
    case SPLASH_SHOW:
        work->timer = 0xb4;
        *step += 1;
        break;
    case SPLASH_WAIT:
        work->timer--;
        if (work->timer == 0)
        {
            *step = SPLASH_FADE_OUT;
        }
        else if ((g_playerPads[6].pressed & (PADRright | PADRdown | PADRleft | PADstart)) != 0)
        {
            Task_SetUserData(-1,SPLASH_SKIPPED);
            *step = SPLASH_FADE_OUT;
        }
        break;
    case SPLASH_FADE_OUT:
        Fade_Out(0xf);
        *step += 1;
        faded = Fade_IsDone();
        if (faded != 0)
        {
            return 1;
        }
        break;
    case SPLASH_WAIT_FADE_OUT:
        faded = Fade_IsDone();
        if (faded != 0)
        {
            return 1;
        }
        break;
    default:
    }
    if (work->started != 0)
    {
        g_drawPos[0] = 0xa0;
        g_drawPos[1] = 0x78;
        Sprite_DrawAligned(g_spriteGroups[6].base,0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
    }
    return 0;
}

/* START on the first pad skips the title's opening */
#define TITLE_SKIPPED()  (g_playerPads[0].pressed & PADstart)

/* The title's background picture: 320x240, 8-bit, at (0x360, 0) in VRAM */
static void TitleMenu_DrawBackground(void)
{
    VramImage img;
    short pos[4];

    img.mode = 1;
    img.x = 0x360;
    img.w = 0x140;
    img.h = 0xf0;
    img.clutY = 0x1e7;
    img.y = 0;
    img.clutX = 0;
    pos[0] = 0;
    pos[1] = 0;
    Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,0xf);
}

/* A picture of two sprites side by side (group 6, first and first + 1) as a sprite list */
static void TitleMenu_SetupPair(SpriteList *list, SpritePart *parts, int first)
{
    parts[0].sprite = GROUP_SPRITE(6,first);
    parts[2].sprite = -1;
    parts[0].x = 0;
    parts[1].sprite = GROUP_SPRITE(6,first + 1);
    parts[1].x = g_sprites[GROUP_SPRITE(6,first)].w;
    parts[0].y = 0;
    parts[1].y = 0;
    parts[0].flip = 0;
    parts[1].flip = 0;
    SpriteList_GetBounds(list,parts);
}

/* TitleMenu_Task steps */
enum TitleMenuStep
{
    TITLEMENU_INIT          =   0,  /* the title music */
    TITLEMENU_FADE_IN       =  10,
    TITLEMENU_FRAME_1       =  11,  /* (a frame) */
    TITLEMENU_FRAME_2       =  12,  /* (a frame) */
    TITLEMENU_WAIT_FADE_IN  =  13,
    TITLEMENU_FRAME_3       =  20,  /* (a frame) */
    TITLEMENU_FRAME_4       =  21,  /* (a frame) */
    TITLEMENU_LOGO          =  30,  /* the logo and the title */
    TITLEMENU_LOGO_IN       =  31,  /* the logo slides in from the right */
    TITLEMENU_TITLE_DROP    =  32,  /* the title drops in */
    TITLEMENU_LOGO_BOUNCE   =  33,  /* (not reached) a bounce */
    TITLEMENU_LOGO_PLACED   =  34,
    TITLEMENU_LOGO_DONE     =  35,
    TITLEMENU_SIGN          =  40,  /* the sign */
    TITLEMENU_SIGN_IN       =  41,  /* slides in from the left */
    TITLEMENU_SIGN_OPEN     =  42,  /* widens */
    TITLEMENU_SIGN_BOUNCE   =  43,  /* (not reached) a bounce */
    TITLEMENU_SIGN_DONE     =  44,
    TITLEMENU_JINGLE        =  50,
    TITLEMENU_WAIT_JINGLE   =  51,  /* until the jingle ends */
    TITLEMENU_SKIPPED       =  55,  /* START: everything in place */
    TITLEMENU_SKIPPED_DONE  =  56,
    TITLEMENU_WHEEL         =  60,  /* the wheel spins */
    TITLEMENU_WHEEL_SPIN    =  61,
    TITLEMENU_IDLE_START    =  62,
    TITLEMENU_IDLE          =  63,  /* 1800 frames for START, else TITLE_RESULT_IDLE */
    TITLEMENU_LEAVE         = 100,
    TITLEMENU_LEAVE_FRAME   = 101,  /* (a frame) */
    TITLEMENU_WAIT_FADE_OUT = 102,
    TITLEMENU_DONE          = 103 
};

/* Title screen: fade in, the opening (the logo slides in from the right and the title drops into
 * place, both only as values, nothing draws them; the sign slides in from the left and stretches,
 * until the jingle (XA) ends; START skips it), then the wheel spins up and slows, and the idle
 * countdown of 1800 frames runs. START (any pad) -> user data 1; time out -> user data 0. Every
 * 0.5-2.5 s a title animation (TitleMenu_EffectATask 70%, TitleMenu_EffectBTask 30%)
 * (0x8004191C) */
/* 0x8004191C */
int TitleMenu_Task(short *step, TitleMenuWork *work)
{
    switch(*step)
    {
    case TITLEMENU_INIT:
        Bgm_Play(1,0,0x7f,0x7f);
        Fade_SetBlack();
        work->logoPhase = 0;
        work->signShown = 0;
        work->wheelShown = 0;
        work->effectState = 0;
        work->active = 0;
        TitleMenu_SetupPair(&g_menuSpriteList,g_menuSpriteParts,0xf);
        TitleMenu_SetupPair(&g_titleSpriteList,g_titleSpriteParts,0x11);
        *step = TITLEMENU_FADE_IN;
        break;
    case TITLEMENU_FADE_IN:
        Fade_In(60);
        /* fall through */
    case TITLEMENU_FRAME_1:
    case TITLEMENU_FRAME_2:
    case TITLEMENU_FRAME_3:
    case TITLEMENU_JINGLE:
    case TITLEMENU_LEAVE_FRAME:
        *step += 1;
        break;
    case TITLEMENU_WAIT_FADE_IN:
        if (Fade_IsDone())
        {
            *step = TITLEMENU_FRAME_3;
        }
        break;
    case TITLEMENU_FRAME_4:
        *step = TITLEMENU_LOGO;
        break;

    /* 30-35: the logo and the title */
    case TITLEMENU_LOGO:
        work->logoPhase = 1;
        work->logoX = 0x140000;
        work->logoY = 0x20000;
        work->speed = -0x8000;
        work->titleY = 0x110000;
        work->logoShown = 0;
        work->speedY = 0;
        work->titleX = 0x80000;
        *step += 1;
        break;
    case TITLEMENU_LOGO_IN:
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed -= 0x1000;
        work->logoX += work->speed;
        if (work->logoX <= 0x18000)
        {
            work->logoX = 0x18000;
            *step += 1;
        }
        break;
    case TITLEMENU_TITLE_DROP:
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed += 0x1800;
        work->titleY += work->speed;
        if (work->titleY >= 0x110000)
        {
            work->titleY = 0x110000;
            *step = TITLEMENU_LOGO_PLACED;
        }
        break;
    case TITLEMENU_LOGO_BOUNCE:   /* (not reached) a bounce */
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed -= 0x4000;
        work->titleY += work->speed;
        if (work->titleY <= 0x110000)
        {
            work->titleY = 0x110000;
            *step += 1;
        }
        break;
    case TITLEMENU_LOGO_PLACED:
        work->logoPhase = 2;
        work->logoShown = 1;
        work->unused1A = 0;
        *step += 1;
        break;
    case TITLEMENU_LOGO_DONE:
        *step = TITLE_SKIPPED() ? TITLEMENU_SKIPPED : TITLEMENU_SIGN;
        break;

    /* 40-44: the sign */
    case TITLEMENU_SIGN:
        work->signShown = 1;
        work->signX = -0x74000;
        work->signY = 0xbc000;
        work->speed = 0;
        work->speedY = 0;
        work->signW = 0x74000;
        work->signH = 0x10000;
        *step += 1;
        break;
    case TITLEMENU_SIGN_IN:
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed += 0x1000;
        work->signX += work->speed;
        if (work->signX >= 0x66000)
        {
            work->signX = 0x66000;
            *step += 1;
        }
        break;
    case TITLEMENU_SIGN_OPEN:
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed -= 0x3000;
        work->signW -= work->speed;
        if (work->signW >= 0x74000)
        {
            work->signW = 0x74000;
            *step = TITLEMENU_SIGN_DONE;
        }
        break;
    case TITLEMENU_SIGN_BOUNCE:   /* (not reached) a bounce */
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        work->speed += 0x6000;
        work->signW -= work->speed;
        if (work->signW <= 0x74000)
        {
            work->signW = 0x74000;
            *step += 1;
        }
        break;
    case TITLEMENU_SIGN_DONE:
        *step = TITLE_SKIPPED() ? TITLEMENU_SKIPPED : TITLEMENU_JINGLE;
        break;
    case TITLEMENU_WAIT_JINGLE:   /* until the jingle ends */
        if (TITLE_SKIPPED())
        {
            *step = TITLEMENU_SKIPPED;
            break;
        }
        if (!Cd_IsXaPlaying())
        {
            *step = TITLEMENU_WHEEL;
        }
        break;

    /* 55-56: skipped: everything in place */
    case TITLEMENU_SKIPPED:
        work->logoPhase = 2;
        work->logoY = 0x20000;
        work->titleY = 0x110000;
        work->titleX = 0x80000;
        work->logoShown = 1;
        work->logoX = 0x18000;
        work->signShown = 1;
        work->signX = 0x66000;
        work->signY = 0xbc000;
        work->signW = 0x74000;
        work->signH = 0x10000;
        *step += 1;
        break;
    case TITLEMENU_SKIPPED_DONE:
        *step = TITLEMENU_WHEEL;
        break;

    /* 60-63: the wheel spins, and the wait */
    case TITLEMENU_WHEEL:
        work->angle = 0x400;
        work->spin = 0x2000;
        work->active = 1;
        work->wheelShown = 1;
        work->wheelPalette = 0;
        work->effectState = 1;
        work->effectDelay = 60;
        *step += 1;
        break;
    case TITLEMENU_WHEEL_SPIN:
        work->spin -= 0x20;
        if ((short)work->spin <= 0x20)
        {
            work->spin = 0x20;
        }
        work->angle += (short)work->spin >> 4;
        if (work->angle >= 0xa000)
        {
            work->angle = 0;
            *step += 1;
        }
        break;
    case TITLEMENU_IDLE_START:
        work->idleTimer = 1800;
        *step += 1;
        break;
    case TITLEMENU_IDLE:
        if (--work->idleTimer == 0)
        {
            Task_SetUserData(-1,TITLE_RESULT_IDLE);
            *step = TITLEMENU_LEAVE;
        }
        break;

    /* 100-103: leaving */
    case TITLEMENU_LEAVE:
        work->active = 0;
        Bgm_Stop();
        Fade_Out(15);
        *step += 1;
        break;
    case TITLEMENU_WAIT_FADE_OUT:
        if (Fade_IsDone())
        {
            *step += 1;
        }
        break;
    case TITLEMENU_DONE:
        work->effectState = 0;
        Task_Stop(g_subTaskIds[1]);
        return 1;
    }

    if (work->active != 0 && (g_playerPads[6].pressed & PADstart))
    {
        work->angle = 0;
        Task_SetUserData(-1,TITLE_RESULT_START);
        Se_Play(SE_OK,0x7f,0x7f,1);
        *step = TITLEMENU_LEAVE;
    }
    if (work->signShown != 0)
    {
        g_drawPos[2] = work->signW / 0x1000;
        g_drawPos[3] = work->signH / 0x1000;
        g_drawPos[0] = work->signX / 0x1000 + 0x74;
        g_drawPos[1] = work->signY / 0x1000;
        Sprite_DrawAligned(GROUP_SPRITE(6,0xc),0,PAL_DEFAULT,g_drawPos,ALIGN_RIGHT | ALIGN_TOP,g_ot + g_otIndex,9);
    }
    if (work->wheelShown != 0)
    {
        /* the wheel's lights: 14 palettes, a new one every 16 frames */
        work->wheelPalette = (short)(work->wheelPalette + 0x10) % 0xe00;
        g_drawPos[0] = 0xa0;
        g_drawPos[1] = 0x98;
        Sprite_DrawEx(GROUP_SPRITE(6,1),0,(char)(work->wheelPalette >> 8),g_drawPos,0,work->angle,0x800,g_ot + g_otIndex,8);
    }
    if (work->effectState == 1)
    {
        if (work->effectDelay == 0)
        {
            Task_Start(g_subTaskIds[1],(rand() % 10 < 7) ? (void *)TitleMenu_EffectATask : (void *)TitleMenu_EffectBTask);
            work->effectState = 2;
        }
        else
        {
            work->effectDelay--;
        }
    }
    else if (work->effectState == 2 && Task_IsDone(g_subTaskIds[1]))
    {
        work->effectState = 1;
        work->effectDelay = rand() % 120 + 30;
    }
    TitleMenu_DrawBackground();
    return 0;
}

/* TitleMenu_EffectATask steps */
enum TitleEffectAStep
{
    PEEK_START      =   0,  /* one of the six places at random */
    PEEK_OUT        =  10,  /* coming out */
    PEEK_STAY_START =  11,
    PEEK_STAY       =  12,  /* a second */
    PEEK_BACK       =  13,  /* going back */
    PEEK_DONE       = 100 
};

/* TitleMenuEffectAWork.kind: where the character peeks in */
enum PeekKind
{
    PEEK_TOP_LEFT,               /* hanging from the top */
    PEEK_TOP_RIGHT,
    PEEK_BOTTOM_LEFT,            /* up from the bottom */
    PEEK_BOTTOM_RIGHT,
    PEEK_LEFT,                   /* sideways from the edge */
    PEEK_RIGHT
};

/* Task: title-screen animation A (picked 70% of the time): a character (sprite group 6, sprites
 * 2-5, more of it showing each) peeks in at one of six places (hanging from the top left or
 * right, up from the bottom left or right, sideways from the left or right edge), stays a second
 * and goes back (0x800422E0) */
/* 0x800422E0 */
u32 TitleMenu_EffectATask(u16 *step, TitleMenuEffectAWork *work)
{
    u16 id;

    switch (*step)
    {
    case PEEK_START:
        work->started = 1;
        work->reveal = 0;
        work->kind = rand() % 6;
        *step = PEEK_OUT;
        break;
    case PEEK_OUT:                /* coming out */
        work->reveal += 0x20;
        if ((short)work->reveal >= 0x300)
        {
            work->reveal = 0x300;
            *step += 1;
        }
        break;
    case PEEK_STAY_START:
        work->timer = 60;
        *step += 1;
        break;
    case PEEK_STAY:               /* staying */
        if (--work->timer == 0)
        {
            *step += 1;
        }
        break;
    case PEEK_BACK:               /* going back */
        work->reveal -= 0x20;
        if ((short)work->reveal < 1)
        {
            work->reveal = 0;
            *step = PEEK_DONE;
        }
        break;
    case PEEK_DONE:
        return 1;
    }
    if (work->started == 0)
    {
        return 0;
    }
    id = g_spriteGroups[6].base + 2 + ((short)work->reveal >> 8);
    switch (work->kind)
    {
    case PEEK_TOP_LEFT:           /* hanging from the top */
    case PEEK_TOP_RIGHT:
        g_drawPos[0] = (work->kind == PEEK_TOP_LEFT) ? 0x48 : 0xf8;
        g_drawPos[1] = 0;
        Sprite_DrawAligned(id,GFX_FLIP_Y,PAL_DEFAULT,g_drawPos,ALIGN_TOP,g_ot + g_otIndex,7);
        break;
    case PEEK_BOTTOM_LEFT:        /* up from the bottom */
    case PEEK_BOTTOM_RIGHT:
        g_drawPos[0] = (work->kind == PEEK_BOTTOM_LEFT) ? 0x48 : 0xf8;
        g_drawPos[1] = 0xf0;
        g_drawPos[2] = 0x50;
        g_drawPos[3] = 0x40;
        Sprite_DrawAligned(id,0,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM,g_ot + g_otIndex,7);
        break;
    case PEEK_LEFT:               /* sideways from the left / right edge */
    case PEEK_RIGHT:
        g_drawPos[0] = (work->kind == PEEK_LEFT) ? 0x1e : 0x122;
        g_drawPos[1] = 0x78;
        g_drawPos[2] = 0x50;
        g_drawPos[3] = 0x40;
        Sprite_DrawStretchedRotated(id,0,PAL_DEFAULT,g_drawPos,0,(work->kind == PEEK_LEFT) ? 0xc00 : 0x400,g_ot + g_otIndex,7);
        break;
    }
    return 0;
}

/* TitleMenu_EffectBTask steps */
enum TitleEffectBStep
{
    STROLL_START =   0,  /* a random side and height */
    STROLL_WALK  =  10,  /* across the screen */
    STROLL_DONE  = 100 
};

/* Task: title-screen animation B (picked 30% of the time): a character (sprite group 3, sprites
 * 50-54) walks across the screen at a random height, left to right or (mirrored) right to left
 * (0x80042680) */
/* 0x80042680 */
u32 TitleMenu_EffectBTask(u16 *step, TitleMenuEffectBWork *work)
{
    short sprite;

    switch (*step)
    {
    case STROLL_START:
        work->started = 1;
        work->leftward = rand() % 2;
        work->x = work->leftward ? 320 << 12 : -32 << 12;
        work->y = (rand() % 128 + 32) << 12;
        work->walk = 0;
        *step = STROLL_WALK;
        break;
    case STROLL_WALK:             /* walking: 6 frames of 0x100, 4 pixels a frame */
        work->walk = (short)(work->walk + 0x40) % 0x600;
        if (work->leftward == 0)
        {
            work->x += 0x4000;
            if (work->x >= 320 << 12)
            {
                work->x = 320 << 12;
                *step = STROLL_DONE;
            }
        }
        else
        {
            work->x -= 0x4000;
            if (work->x < -0x1ffff)
            {
                work->x = -32 << 12;
                *step = STROLL_DONE;
            }
        }
        break;
    case STROLL_DONE:
        return 1;
    }
    if (work->started == 0)
    {
        return 0;
    }
    switch ((short)work->walk >> 8)
    {
    default:
    case 0:
    case 3: sprite = 50; break;
    case 1: sprite = 51; break;
    case 2: sprite = 52; break;
    case 4: sprite = 53; break;
    case 5: sprite = 54; break;
    }
    g_drawPos[0] = work->x / 0x1000;
    g_drawPos[1] = work->y / 0x1000;
    Sprite_DrawAligned(g_spriteGroups[3].base + sprite,work->leftward ? GFX_FLIP_X : 0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,0xb);
    return 0;
}
