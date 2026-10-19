/*
 * main.c - the entry point and the game-mode loop
 *
 * main initializes the system and then runs the game modes (logo, title, new game, board,
 * ending, stage clear, staff roll) one after another: each mode is a task that runs until it
 * sets the next mode. Mode_CreateSubTasks is the set-up every mode starts with.
 */
#include "common.h"
#include "topshop.h"

void __main(void);                       /* 0x8008DBFC PsyQ C runtime init */

/* 0x8001A170 */
int main(void)
{
    TaskFunc entry;

    __main();
    SetMem(2);
    ResetCallback();
    Sys_Init();
    Game_InitDefaults();
    Sys_LoadFonts();
    g_renderEnabled = 1;
    g_gameMode = MODE_LOGO;

    for (;;)
    {
        g_skipFrameClear = 0;
        Sys_EndFrame();
        Task_InitAll();
        g_modeTaskId = Task_Create(0, NULL);
        g_auxTaskId = Task_Create(0, NULL);
        g_fileIoDisabled = 0;
        Sys_ResetFonts();

        switch (g_gameMode)
        {
        case MODE_LOGO:        entry = Logo_Task;       break;
        case MODE_TITLE:       entry = Title_Task;      break;
        case MODE_NEW_GAME:    entry = NewGame_Task;    break;
        case MODE_BOARD:       entry = Board_Task;      break;
        case MODE_ENDING:      entry = Ending_Task;     break;
        case MODE_STAGE_CLEAR: entry = StageClear_Task; break;
        case MODE_STAFF_ROLL:  entry = StaffRoll_Task;  break;
        case MODE_UNUSED10:    entry = Mode10_Stub;     break;
        default: entry = NULL;        break;   /* no task started */
        }
        if (entry != NULL)
            Task_Start(g_modeTaskId, entry);

        do
        {
            do
            {
                Pad_UpdatePlayers();
                Task_RunAll();
                Sys_EndFrame();
                g_frameVsync = g_vsyncCount;
            } while (!Task_IsDone(g_modeTaskId));
        } while (g_gameMode == MODE_CONTINUE);
    }
}

/* Game mode 10 task: empty, never finishes. Only reachable through results nothing
 * produces (see docs/modes.md); probably a removed debug mode. (0x8001C950) */
int Mode10_Stub(short *step, void *work)
{
}

/* Common mode setup: create the 16 sub-tasks (g_subTaskIds, work pointers in g_subTaskWork), the
 * memory-card task (priority 3, g_cardTaskId) and the UI task (g_uiTaskIds[0], work
 * g_uiTaskWork), then Fade_Init + Fade_SetBlack. Inlined copies exist in Logo_Task and
 * Title_Task. (0x80042A3C) */
/* 0x80042A3C */
void Mode_CreateSubTasks(void)
{
    int i;

    g_vsyncInterval = 0;
    for (i = 0; i < 16; i++)
    {
        g_subTaskIds[i] = Task_Create(1,&g_subTaskWork[i]);
    }
    g_cardTaskId = Task_Create(3,NULL);
    g_uiTaskIds[0] = Task_Create(1,g_uiTaskWork);
    Fade_Init();
    Fade_SetBlack();
}
