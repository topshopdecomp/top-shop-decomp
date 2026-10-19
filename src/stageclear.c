/*
 * stageclear.c - game mode 8: between story stages
 *
 * After a story stage is won: the clear is counted and the progress can be saved
 * (Card_SaveProgressTask in src/save.c); then the next stage, the title, or after the last stage
 * the staff roll.
 */
#include "common.h"
#include "topshop.h"

/* StageClear_Task steps */
enum StageClearStep
{
    CLEAR_INIT             =   0,  /* the set-up graphics, fade in */
    CLEAR_WAIT_FADE_IN     =   1,
    CLEAR_SAVE             =  20,  /* Card_SaveProgressTask counts the clear, offers to save */
    CLEAR_WAIT_SAVE        =  21,  /* after the last stage: the staff roll */
    CLEAR_MENU             =  30,  /* StageClearMenu_Task: go on? */
    CLEAR_WAIT_MENU        =  31,
    CLEAR_NEXT_STAGE       =  40,  /* the next stage's set-up: StageSelect_Task */
    CLEAR_WAIT_NEXT_STAGE  =  41,
    CLEAR_MONEY_RULES      =  50,  /* MoneyRules_Task */
    CLEAR_WAIT_MONEY_RULES =  51,
    CLEAR_FADE_OUT         = 100,
    CLEAR_WAIT_FADE_OUT    = 101,
    CLEAR_DONE             = 102   /* to the game mode chosen */
};

/* StageClearMenu_Task's rows: its user data */
#define CLRMENU_GO_ON  0           /* on to the next stage */
#define CLRMENU_QUIT   1           /* back to the title */

/* Game mode 8: between story stages. Card_SaveProgressTask counts the clear and offers to save
 * the progress; then after the last stage g_gameMode 9 (the staff roll), else StageClearMenu_Task
 * asks whether to go on: yes -> the next stage's set-up -> mode 6, no -> mode 2 (0x8007496C) */
/* 0x8007496C */
int StageClear_Task(short *step, void *work)
{
    int faded;
    int done;
    int answer;

    Gfx_QueueClear(0,0,0);
    FntPrint("~c000");
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", (u16)*step);
    g_debugPrint = 1;
    switch(*step)
    {
    case CLEAR_INIT:
        *(u8 *)work = MODE_BOARD;
        Mode_CreateSubTasks();
        NewGame_LoadGraphics();
        NewGame_LoadCharGraphics();
        Fade_SetBlack();
        Fade_In(0xf);
        Bgm_Play(2,0,0x7f,0x7f);
        *step += 1;
        break;
    case CLEAR_WAIT_FADE_IN:
        faded = Fade_IsDone();
        if (faded != 0)
        {
            *step = CLEAR_SAVE;
        }
    default:
        break;
    case CLEAR_SAVE:
        g_clearedStage = (char)g_gameConfig.stage;
        Task_Start(g_subTaskIds[0],Card_SaveProgressTask);
        *step += 1;
        break;
    case CLEAR_WAIT_SAVE:
        done = Task_IsDone(g_subTaskIds[0]);
        if (done != 0)
        {
            if (g_clearedStage == 9)
            {
                *(u8 *)work = MODE_STAFF_ROLL;
                *step = CLEAR_FADE_OUT;
            }
            else
            {
                g_clearedStage++;
                *step = CLEAR_MENU;
            }
        }
        break;
    case CLEAR_MENU:
        Task_Start(g_subTaskIds[0],StageClearMenu_Task);
        *step += 1;
        break;
    case CLEAR_WAIT_MENU:
        done = Task_IsDone(g_subTaskIds[0]);
        if (done == 0) break;
        answer = Task_GetUserData(g_subTaskIds[0]);
        if (answer == CLRMENU_GO_ON)
        {
            *(u8 *)work = MODE_BOARD;
            *step = CLEAR_NEXT_STAGE;
            break;
        }
        *(u8 *)work = MODE_TITLE;
        *step = CLEAR_FADE_OUT;
        break;
    case CLEAR_NEXT_STAGE:
        Story_SetupPlayers();
        *(u32 *)g_subTaskWork[0] = 2;
        Task_Start(g_subTaskIds[0],StageSelect_Task);
        *step += 1;
        break;
    case CLEAR_WAIT_NEXT_STAGE:
        done = Task_IsDone(g_subTaskIds[0]);
        if (done != 0)
        {
            *step = CLEAR_MONEY_RULES;
        }
        break;
    case CLEAR_MONEY_RULES:
        *(u32 *)g_subTaskWork[0] = 2;
        Task_Start(g_subTaskIds[0],MoneyRules_Task);
        *step += 1;
        break;
    case CLEAR_WAIT_MONEY_RULES:
        done = Task_IsDone(g_subTaskIds[0]);
        if (done == 0) break;
        *step = CLEAR_FADE_OUT;
        break;
    case CLEAR_FADE_OUT:
        Fade_Out(0xf);
        *step += 1;
        break;
    case CLEAR_WAIT_FADE_OUT:
        faded = Fade_IsDone();
        if (faded == 0) break;
        *step += 1;
        break;
    case CLEAR_DONE:
        Bgm_Stop();
        g_gameMode = *(char *)work;
        return 1;
    }
    Gfx_DrawBackground();
    TextWin_DrawAll();
    Font_FlushUploads();
    g_glyphUploadPtr = g_glyphUploadBuf;
    return 0;
}

/* StageClearMenu_Task steps */
enum StageClearMenuStep
{
    CLRMENU_INIT           =   0,
    CLRMENU_ITEM_IN        =   1,  /* the rows slide in, 2 frames apart */
    CLRMENU_ITEM_DELAY     =   2,
    CLRMENU_WAIT_ITEMS     =   3,
    CLRMENU_FADE_IN        =  10,
    CLRMENU_CHOOSE         =  20,
    CLRMENU_FADE_OUT       =  30,
    CLRMENU_ITEM_OUT       =  31,  /* the rows slide out */
    CLRMENU_ITEM_OUT_DELAY =  32,
    CLRMENU_DONE           = 100   /* the row chosen: the user data */
};

/* The menu after a cleared stage: two rows (sprites 0x86 and 0x87 of group 5) slide in one after
 * the other, their band fades in, up / down and cross on pad 0, the rows slide out; the row
 * chosen is the task's user data (0x80074C7C) */
/* 0x80074C7C */
u32 StageClearMenu_Task(short *step, StageClearMenuWork *work)
{
    PadState *pad = &g_playerPads[6];
    MenuItem *item;
    int i;

    switch(*step)
    {
    case CLRMENU_INIT:
        work->shown = 1;
        work->count = 2;
        work->moving = 0;
        work->item = 0;
        work->fade = 0x100;
        for (i = 0; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->state = MENUITEM_STILL;
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->offsetY = 0;
            item->sprite = 0x86 + i;
            /* centered on x 0xa0, rows 0x20 apart from line 0x58 */
            item->x = 0xa0 - g_sprites[GROUP_SPRITE(5,item->sprite)].w / 2;
            item->y = 0x58 + i * 0x20 - g_sprites[GROUP_SPRITE(5,item->sprite)].h / 2;
        }
        work->animItem = 0;
        *step += 1;
        break;
    case CLRMENU_ITEM_IN:         /* the rows slide in, 2 frames apart */
        g_menuItems[work->animItem].state = MENUITEM_SLIDE_IN;
        if (++work->animItem == work->count)
        {
            *step = CLRMENU_WAIT_ITEMS;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case CLRMENU_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = CLRMENU_ITEM_IN;
        }
        break;
    case CLRMENU_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = CLRMENU_FADE_IN;
        }
        break;
    case CLRMENU_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->fade = 0;
            *step = CLRMENU_CHOOSE;
        }
        break;
    case CLRMENU_CHOOSE:
        if (pad->pressed & PADRdown)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = CLRMENU_FADE_OUT;
        }
        else if (pad->repeat & (PADLup | PADLdown))
        {
            work->item = (work->item == 0) ? 1 : 0;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case CLRMENU_FADE_OUT:
        work->fade += 0x40;
        if (work->fade > 0xff)
        {
            work->fade = 0x100;
            *step += 1;
        }
        work->animItem = 0;
        break;
    case CLRMENU_ITEM_OUT:        /* the rows slide out */
        g_menuItems[work->animItem].state = MENUITEM_FOLD;
        if (++work->animItem == work->count)
        {
            *step = CLRMENU_DONE;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case CLRMENU_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = CLRMENU_ITEM_OUT;
        }
        break;
    case CLRMENU_DONE:
        Task_SetUserData(-1,work->item);
        work->shown = 0;
        return 1;
    }
    if (work->shown != 0)
    {
        Menu_DrawBand(g_menuItems[0].y,0x10,work->fade);
        Menu_DrawBand(g_menuItems[1].y,0x10,work->fade);
        if (work->fade == 0)
        {
            g_drawPos[0] = 0x50;
            g_drawPos[1] = g_menuItems[work->item].y;
            Sprite_DrawAligned(MENU_HAND(g_frameVsync),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        }
        MenuItems_Draw(work->count);
    }
    return 0;
}
