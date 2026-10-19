/*
 * logo.c - game mode 0: the boot logos
 *
 * Shows A1.TIM, then cross-fades to E_KID.TIM. Both are 320x240 8bpp images uploaded once to
 * VRAM (448,256) and (608,256) and drawn with Gfx_DrawVramImage every frame; the cross-fade is
 * done with the global tint. START (player 0) skips to the fade-out at any point.
 */
#include "common.h"
#include "topshop.h"

#define LOGO_HOLD_FRAMES 210

/* Logo_Task steps */
enum LogoStep
{
    LOGO_LOAD          =   0,  /* the two logos */
    LOGO_FADE_IN       =  10,  /* fade in on logo 1 */
    LOGO_WAIT_FADE_IN  =  11,
    LOGO_HOLD1         =  20,  /* hold logo 1 */
    LOGO_WAIT_HOLD1    =  21,
    LOGO_CROSSFADE     =  30,  /* cross-fade to logo 2 */
    LOGO_HOLD2         =  40,  /* hold logo 2 */
    LOGO_WAIT_HOLD2    =  41,
    LOGO_FADE_OUT      =  50,  /* (START skips to here) */
    LOGO_WAIT_FADE_OUT =  51,
    LOGO_DONE          = 100   /* to the title */
};

/* 0x80041464 */
int Logo_Task(short *step, void *work)
{
    LogoWork *w = (LogoWork *)work;
    VramImage img;
    short pos[2];
    short level;

    switch (*step)
    {
    case LOGO_LOAD:
        g_vsyncInterval = 0;
        Mode_CreateSubTasks();          /* inlined in the original */
        Fade_SetBlack();                /* (called a second time here) */
        File_LoadTim(FILE_A1_TIM);
        File_LoadTim(FILE_E_KID_TIM);
        w->blend = 0;
        *step = LOGO_FADE_IN;
        break;

    case LOGO_FADE_IN:                  /* fade in on logo 1 */
        Fade_In(60);
        (*step)++;
        /* fall through */
    case LOGO_WAIT_FADE_IN:
        if (g_playerPads[0].pressed & PADstart)
            *step = LOGO_FADE_OUT;
        if (Fade_IsDone())
            *step = LOGO_HOLD1;
        break;

    case LOGO_HOLD1:                    /* hold logo 1 */
        w->timer = LOGO_HOLD_FRAMES;
        (*step)++;
        break;
    case LOGO_WAIT_HOLD1:
        if (g_playerPads[0].pressed & PADstart)
            *step = LOGO_FADE_OUT;
        if (--w->timer == 0)
            *step = LOGO_CROSSFADE;
        break;

    case LOGO_CROSSFADE:                /* cross-fade to logo 2 */
        w->blend += 0x8000;
        if (w->blend > 0x1EFFFF)
        {
            w->blend = 0x1F0000;
            *step = LOGO_HOLD2;
        }
        break;

    case LOGO_HOLD2:                    /* hold logo 2 */
        w->blend = 0x200000;
        w->timer = LOGO_HOLD_FRAMES;
        (*step)++;
        break;
    case LOGO_WAIT_HOLD2:
        if (g_playerPads[0].pressed & PADstart)
            *step = LOGO_FADE_OUT;
        if (--w->timer == 0)
            *step = LOGO_FADE_OUT;
        break;

    case LOGO_FADE_OUT:
        Fade_Out(15);
        (*step)++;
        break;
    case LOGO_WAIT_FADE_OUT:
        if (Fade_IsDone())
            *step = LOGO_DONE;
        break;

    case LOGO_DONE:
        g_gameMode = MODE_TITLE;
        Fade_SetBlack();
        Sys_LoadCommonSprites();
        Sys_LoadLoadingScreen();
        Progress_Reset();
        return 1;
    }

    /* logo 1 (A1.TIM), additive, dims as blend rises */
    img.mode = 1;
    img.clutX = 0;
    img.clutY = 0x1ED;
    img.x = 0x1C0;
    img.y = 0x100;
    img.w = 320;
    img.h = 240;
    pos[0] = pos[1] = 0;
    level = (w->blend >> 17);
    g_tintR = g_tintG = g_tintB = 0x80 - level * 8;
    Gfx_DrawVramImage(&img, GFX_SEMITRANS | (1 << GFX_ABR_SHIFT), pos, CUR_OT, 12);

    /* logo 2 (E_KID.TIM), brightens as blend rises */
    img.clutY = 0x1EE;
    img.x = 0x260;
    g_tintR = g_tintG = g_tintB = ((w->blend >> 17) << 3);
    Gfx_DrawVramImage(&img, 0, pos, CUR_OT, 12);

    g_tintR = g_tintG = g_tintB = 0x80;

    FntPrint("~cfff");
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("Exec Logo\n");
    g_debugPrint = 1;
    return 0;
}
