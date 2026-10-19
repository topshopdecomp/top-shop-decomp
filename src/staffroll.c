/*
 * staffroll.c - game mode 9: the staff roll
 *
 * After the last story stage: the credits roll by while the characters walk along.
 */
#include "common.h"
#include "topshop.h"

/* Load the staff roll's graphics: STAFF1-3.SPC (sprite group 4), E_CHR0-1.SPC (group 5),
 * A1.TIM and END_BG.TIM (0x80075140) */
/* 0x80075140 */
void StaffRoll_LoadGraphics(void)
{
    int i;

    g_spriteGroups[5].next = g_spriteGroups[5].base;
    g_spriteGroups[4].next = g_spriteGroups[4].base;
    for (i = 0; i < 3; i++)
    {
        File_LoadPackedSprites(FILE_STAFF1_SPC + i,4);
    }
    for (i = 0; i < 2; i++)
    {
        File_LoadPackedSprites(FILE_E_CHR0_SPC + i,5);
    }
    File_LoadTim(FILE_A1_TIM);
    File_LoadTim(FILE_END_BG_TIM);
}

/* StaffRollWork.phase: the characters walking across */
#define WALKERS_OFF      0
#define WALKERS_WAIT     1       /* the next one after work->wait frames */
#define WALKERS_WALKING  2       /* until StaffRoll_WalkerTask is done */

/* g_staffRollState: what the staff roll shows */
#define STAFFROLL_PAGES    0     /* the pages of names */
#define STAFFROLL_PICTURE  1     /* the closing picture */
#define STAFFROLL_END      3     /* "End" */

/* StaffRoll_RunTask steps */
enum StaffRollStep
{
    ROLL_INIT             =   0,
    ROLL_FADE_IN          =  10,
    ROLL_WAIT_FADE_IN     =  11,
    ROLL_START            = 100,  /* the pages */
    ROLL_NEXT_PAGE        = 110,  /* the next page, or the end */
    ROLL_LAYOUT           = 120,  /* lay the page out off screen */
    ROLL_SLIDE_IN         = 121,  /* the lines slide in, one every 4 frames */
    ROLL_SHOW             = 130,
    ROLL_WAIT_SHOW        = 131,  /* the page's time */
    ROLL_SLIDE_OUT_START  = 140,
    ROLL_SLIDE_OUT        = 141,  /* the lines slide out, accelerating */
    ROLL_GAP              = 150,  /* 30 frames */
    ROLL_WAIT_GAP         = 151,
    ROLL_WAIT_STARS       = 280,  /* the last page done: wait for the stars */
    ROLL_PICTURE          = 300,  /* the closing picture */
    ROLL_WAIT_PICTURE_IN  = 301,
    ROLL_PICTURE_SHOW     = 310,  /* 120 frames */
    ROLL_WAIT_PICTURE     = 311,
    ROLL_PICTURE_FADE_OUT = 320,
    ROLL_WAIT_PICTURE_OUT = 321,
    ROLL_END              = 400,  /* "End" */
    ROLL_WAIT_END_IN      = 401,
    ROLL_END_SHOW         = 410,
    ROLL_WAIT_START       = 411,  /* wait for START */
    ROLL_FADE_OUT         = 420,
    ROLL_WAIT_FADE_OUT    = 421,
    ROLL_DONE             = 999 
};

/* The staff roll: scrolling names and pictures, then back to the title (0x800753FC) */
/* 0x800753FC */
u32 StaffRoll_RunTask(u16 *step, StaffRollRunWork *work)
{
    RollRow *row;
    short x;
    int done;
    int i;
    short wait;
    VramImage img;
    short pos[2];

    Gfx_QueueClear(0,0,0);
    FntPrint("~cfff");
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", *step);
    g_debugPrint = 1;
    switch (*step)
    {
    case ROLL_INIT:
        g_staffRollState = STAFFROLL_PAGES;
        work->mode = 0;
        work->section = 0;
        work->fading = 0;
        work->blink = 0;
        work->phase = WALKERS_OFF;
        Fade_SetBlack();
        Bgm_Play(4,0,0x7f,0x7f);
        *step = ROLL_FADE_IN;
        break;
    case ROLL_FADE_IN:
        Fade_In(0xf);
        *step += 1;
        break;
    case ROLL_WAIT_FADE_IN:
        if (Fade_IsDone() != 0)
        {
            work->wait = 0x78;
            work->phase = WALKERS_WAIT;
            work->walker = 0;
            *step = ROLL_START;
        }
        break;
    case ROLL_START:
        work->fading = 1;
        work->started = 0;
        work->section = 0;
        *step = ROLL_NEXT_PAGE;
        break;
    case ROLL_NEXT_PAGE:         /* next page, or the end */
        if (k_staffRoll[work->section].lines == 0)
        {
            *step = ROLL_WAIT_STARS;
        }
        else
        {
            *step = ROLL_LAYOUT;
        }
        break;
    case ROLL_LAYOUT:            /* lay the page out off screen */
        work->mode = 1;
        work->lines = k_staffRoll[work->section].lines;
        work->y = (0x110 - work->lines * 0x20 >> 1);
        for (i = 0; i < 7; i++)
        {
            work->rows[i].x = 0x5000;
            work->rows[i].dx = -0x400;
        }
        work->frame = 0;
        *step += 1;
        break;
    case ROLL_SLIDE_IN:          /* slide the lines in, one every 4 frames */
        done = 1;
        work->frame++;
        for (i = 0; i < work->lines; i++)
        {
            row = &work->rows[i];
            if (work->frame / 4 < i)
            {
                done = 0;
                continue;
            }
            row->x += row->dx;
            if (0 < row->x)
            {
                done = 0;
            }
            else
            {
                row->x = 0;
                row->dx = 0;
            }
        }
        if (done)
        {
            *step = ROLL_SHOW;
        }
        break;
    case ROLL_SHOW:
        work->frame = k_staffRoll[work->section].wait;
        *step += 1;
        break;
    case ROLL_WAIT_SHOW:         /* show the page */
        work->frame--;
        if (work->frame == 0)
        {
            *step = ROLL_SLIDE_OUT_START;
        }
        break;
    case ROLL_SLIDE_OUT_START:
        for (i = 6; i >= 0; i--)
        {
            work->rows[i].x = 0;
        }
        work->frame = 0;
        *step += 1;
        break;
    case ROLL_SLIDE_OUT:         /* slide the lines out, accelerating */
        done = 1;
        work->frame++;
        for (i = 0; i < work->lines; i++)
        {
            row = &work->rows[i];
            if (work->frame / 4 < i)
            {
                done = 0;
                continue;
            }
            row->dx -= 0x80;
            x = row->x + row->dx;
            row->x = x;
            if (-0x5000 < x)
            {
                done = 0;
            }
            else
            {
                row->x = -0x5000;
                row->dx = 0;
            }
        }
        if (done)
        {
            *step = ROLL_GAP;
        }
        break;
    case ROLL_GAP:
        work->mode = 0;
        work->frame = 0x1e;
        *step += 1;
        break;
    case ROLL_WAIT_GAP:
        work->frame--;
        if (work->frame == 0)
        {
            work->section++;
            work->blink++;
            *step = ROLL_NEXT_PAGE;
        }
        break;
    case ROLL_WAIT_STARS:       /* last page done: wait for the stars */
        if (Task_IsDone(g_subTaskIds[1]) != 0)
        {
            Fade_SetBlack();
            *step = ROLL_PICTURE;
        }
        break;
    case ROLL_PICTURE:            /* the closing picture */
        work->phase = WALKERS_OFF;
        g_staffRollState = STAFFROLL_PICTURE;
        Fade_SetBlack();
        Fade_In(0x3c);
        *step += 1;
        break;
    case ROLL_WAIT_PICTURE_IN:
        if (Fade_IsDone() != 0)
        {
            *step = ROLL_PICTURE_SHOW;
        }
        break;
    case ROLL_PICTURE_SHOW:
        work->frame = 0x78;
        *step += 1;
        break;
    case ROLL_WAIT_PICTURE:
        work->frame--;
        if (work->frame == 0)
        {
            *step = ROLL_PICTURE_FADE_OUT;
        }
        break;
    case ROLL_PICTURE_FADE_OUT:
        Fade_Out(0x3c);
        *step += 1;
        break;
    case ROLL_WAIT_PICTURE_OUT:
        if (Fade_IsDone() != 0)
        {
            *step = ROLL_END;
        }
        break;
    case ROLL_END:
        Bgm_Stop();
        g_staffRollState = STAFFROLL_END;
        Fade_In(0x3c);
        *step += 1;
        break;
    case ROLL_WAIT_END_IN:
        if (Fade_IsDone() != 0)
        {
            *step = ROLL_END_SHOW;
        }
        break;
    case ROLL_END_SHOW:
        work->frame = 600;
        *step += 1;
        break;
    case ROLL_WAIT_START:       /* wait for Start */
        if ((g_playerPads[6].pressed & PADstart) != 0)
        {
            *step = ROLL_FADE_OUT;
        }
        break;
    case ROLL_FADE_OUT:
        Fade_Out(0x3c);
        *step += 1;
        break;
    case ROLL_WAIT_FADE_OUT:
        if (Fade_IsDone() != 0)
        {
            *step = ROLL_DONE;
        }
        break;
    case ROLL_DONE:
        Bgm_Stop();
        g_gameMode = MODE_STAFF_ROLL;
        return 1;
    }
    /* the page's lines, alternately from the left and from the right */
    if (work->fading != 0 && work->mode == 1)
    {
        for (i = 0; i < work->lines; i++)
        {
            x = (work->rows[i].x / 64);
            g_drawPos[0] = (((work->blink + i) & 1) == 0 ? -x : x) + 0xa0;
            g_drawPos[1] = work->y + (short)i * 0x20;
            if (work->started == 0)
            {
                Sprite_DrawAligned(g_spriteGroups[4].base + k_staffRoll[work->section].sprite[i],0,PAL_DEFAULT,
                                   g_drawPos,0,g_ot + g_otIndex,8);
            }
        }
    }
    /* a character walks across every 120 frames, each in turn */
    if (work->phase == WALKERS_WAIT)
    {
        wait = work->wait;
        work->wait = wait - 1;
        if (wait == 1)
        {
            ((StaffRollWalkerWork *)g_subTaskWork[1])->charIndex = work->walker;
            Task_Start(g_subTaskIds[1],StaffRoll_WalkerTask);
            work->phase = WALKERS_WALKING;
            work->walker = (work->walker + 1) % 14;
        }
    }
    else if (work->phase == WALKERS_WALKING && Task_IsDone(g_subTaskIds[1]) != 0)
    {
        work->wait = 0x78;
        work->phase = WALKERS_WAIT;
    }
    /* the closing picture and the "End" screen */
    if (g_staffRollState == STAFFROLL_PICTURE)
    {
        img.x = 0x1c0;
        img.y = 0x100;
        img.h = 0xf0;
        img.clutY = 0x1ed;
        pos[1] = 0;
    }
    else if (g_staffRollState == STAFFROLL_END)
    {
        img.x = 0x340;
        img.y = 0x120;
        img.h = 0xe0;
        img.clutY = 0x1eb;
        pos[1] = 8;
    }
    else
    {
        return 0;
    }
    pos[0] = 0;
    img.w = 0x140;
    img.clutX = 0;
    img.mode = 1;
    Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,0xc);
    return 0;
}

/* StaffRoll_WalkerTask steps */
enum StaffRollWalkerStep
{
    WALKER_START =   0,  /* a random side and height */
    WALKER_WALK  =  10,  /* across the screen */
    WALKER_DONE  = 100 
};

/* Task: during the staff roll, one of the 14 characters (work->charIndex: five sprites of group 5
 * each, 0x50 further on the ones facing right) walks across the screen on the upper or lower
 * line, left to right or right to left (0x80075DD0) */
/* 0x80075DD0 */
u32 StaffRoll_WalkerTask(u16 *step, StaffRollWalkerWork *work)
{
    u16 sprite;

    switch (*step)
    {
    case WALKER_START:
        work->started = 1;
        work->leftward = rand() % 2;
        work->lower = rand() % 2;
        work->x = (work->leftward == 0) ? -32 << 12 : 320 << 12;
        work->y = (work->lower == 0) ? 32 << 12 : 160 << 12;
        work->walk = 0;
        *step = WALKER_WALK;
        break;
    case WALKER_WALK:             /* 6 frames of 0x100, 4 pixels a frame */
        work->walk = (short)(work->walk + 0x40) % 0x600;
        if (work->leftward == 0)
        {
            work->x += 0x4000;
            if (work->x >= 320 << 12)
            {
                work->x = 320 << 12;
                *step = WALKER_DONE;
            }
        }
        else
        {
            work->x -= 0x4000;
            if (work->x < -0x1ffff)
            {
                work->x = -32 << 12;
                *step = WALKER_DONE;
            }
        }
        break;
    case WALKER_DONE:
        return 1;
    }
    if (work->started == 0)
    {
        return 0;
    }
    sprite = (short)work->charIndex * 5;
    if (work->leftward == 0)
    {
        sprite += 0x50;
    }
    switch (work->walk >> 8)
    {
    case 1: sprite += 1; break;
    case 2: sprite += 2; break;
    case 4: sprite += 3; break;
    case 5: sprite += 4; break;
    }
    g_drawPos[0] = work->x / 0x1000;
    g_drawPos[1] = work->y / 0x1000;
    Sprite_DrawAligned(GROUP_SPRITE(5,sprite & 0xff),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    return 0;
}

/* StaffRoll_Task steps */
enum StaffRollModeStep
{
    STAFF_LOAD     =   0,  /* the sub-tasks, the graphics */
    STAFF_RUN      =  10,  /* StaffRoll_RunTask */
    STAFF_WAIT_RUN =  11,
    STAFF_DONE     = 100   /* to the title */
};

/* Game mode 9: staff roll. StaffRoll_LoadGraphics loads STAFF*.SPC, E_CHR*.SPC, A1.TIM, END_BG.TIM; runs
 * StaffRoll_RunTask. Then g_gameMode = 2. (0x8007618C) */
/* 0x8007618C */
int StaffRoll_Task(short *step, void *work)
{
    u16 phase;
    int next;
    int done;

    phase = *step;
    if (phase == STAFF_DONE)
    {
        g_gameMode = MODE_TITLE;
        next = 1;
    }
    else
    {
        if (phase <= STAFF_DONE)
        {
            if (phase == STAFF_LOAD)
            {
                Mode_CreateSubTasks();
                StaffRoll_LoadGraphics();
                *step = STAFF_RUN;
            }
            else
            {
                if (phase != STAFF_RUN)
                {
                    if (phase != STAFF_WAIT_RUN)
                    {
                        return 0;
                    }
                    done = Task_IsDone(g_subTaskIds[0]);
                    if (done == 0)
                    {
                        return 0;
                    }
                    *step = STAFF_DONE;
                    return 0;
                }
                Task_Start(g_subTaskIds[0],StaffRoll_RunTask);
                *step += 1;
            }
        }
        next = 0;
    }
    return next;
}
