/*
 * msg.c - message windows
 *
 * The game's message boxes: Msg_Show shows a text in a window for a time or until a button is
 * pressed, Msg_AskYesNo asks a yes / no question; the tasks behind them, and the queries whether
 * a message is done or answered.
 */
#include "common.h"
#include "topshop.h"

/* Task func behind Msg_Show: the window opens with the focus, then closes on one of
 * closeButtons (cancel sound), after autoClose frames if set, or for a CPU player by itself
 * after a quarter of a second (0x800565C8) */
/* 0x800565C8 */
int Msg_Task(short *step, MsgWork *work)
{
    switch ((u16)*step)
    {
    case MSG_STEP_OPEN:
        TextWin_Open(work->win);
        TextWin_SetFocus(work->win,1);
        *step += 1;
        /* fall through */
    case MSG_STEP_WAIT_OPEN:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        *step = (work->cpu != 0) ? MSG_STEP_CPU : MSG_STEP_HUMAN;
        return 0;
    case MSG_STEP_HUMAN:
        work->timer = 0;
        *step += 1;
        return 0;
    case MSG_STEP_WAIT_BUTTON:    /* waiting for a button or the time */
        if (g_playerPads[g_curPlayer].pressed & work->closeButtons)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = MSG_STEP_CLOSE;
        }
        if (work->autoClose != 0 && work->autoClose == work->timer)
        {
            *step = MSG_STEP_CLOSE;
        }
        work->timer++;
        return 0;
    case MSG_STEP_CPU:            /* a CPU player's */
        work->timer = 15;
        *step += 1;
        return 0;
    case MSG_STEP_CPU_WAIT:
        if (--work->timer == 0)
        {
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = MSG_STEP_CLOSE;
        }
        return 0;
    case MSG_STEP_CLOSE:
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        return 0;
    case MSG_STEP_WAIT_CLOSE:
        return g_textWins[work->win].state == TEXTWIN_FREEING;
    }
    return 0;
}

/* MsgChoice_Task steps */
enum MsgChoiceStep
{
    CHOICE_FOCUS      =   0,
    CHOICE_WAIT_OPEN  =   1,
    CHOICE_HUMAN      =  10,  /* cross takes the row, circle answers No */
    CHOICE_CPU        =  20,  /* a CPU player's cursor moves to its answer */
    CHOICE_CPU_MOVE   =  21,
    CHOICE_CPU_WAIT   =  22,  /* a quarter of a second */
    CHOICE_CLOSE      = 100,  /* the answer into g_msgAnswer */
    CHOICE_WAIT_CLOSE = 101 
};

/* Task func behind Msg_AskYesNo: a two-row menu (Yes / No); cross takes the row at the cursor,
 * circle answers No; a CPU player's cursor moves to its answer (cpuAnswer) and is taken a
 * quarter of a second later. The answer (the row) goes to g_msgAnswer (0x80056824) */
/* 0x80056824 */
int MsgChoice_Task(short *step, MsgWork *work)
{
    switch ((u16)*step)
    {
    case CHOICE_FOCUS:
        TextWin_SetFocus(work->win,1);
        *step += 1;
        /* fall through */
    case CHOICE_WAIT_OPEN:
        if (g_textWins[work->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        *step = (work->cpu != 0) ? CHOICE_CPU : CHOICE_HUMAN;
        return 0;
    case CHOICE_HUMAN:            /* the human player's choice */
        if (g_playerPads[g_curPlayer].pressed & PADRdown)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = CHOICE_CLOSE;
        }
        else if (g_playerPads[g_curPlayer].pressed & PADRright)
        {
            g_textWins[work->win].menuCursor = 1;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = CHOICE_CLOSE;
        }
        else if (g_playerPads[g_curPlayer].repeat & PADLup)
        {
            TextWin_CursorUp(work->win);
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[g_curPlayer].repeat & PADLdown)
        {
            TextWin_CursorDown(work->win);
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        return 0;
    case CHOICE_CPU:              /* a CPU player's */
        work->timer = 2;
        *step += 1;
        return 0;
    case CHOICE_CPU_MOVE:
        if (--work->timer != 0)
        {
            return 0;
        }
        if (work->cpuAnswer != TextWin_GetCursor(work->win))
        {
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        TextWin_SetMenu(work->win,1,2,work->cpuAnswer);
        work->timer = 15;
        *step += 1;
        return 0;
    case CHOICE_CPU_WAIT:
        if (--work->timer == 0)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = CHOICE_CLOSE;
        }
        return 0;
    case CHOICE_CLOSE:
        g_msgAnswer = TextWin_GetCursor(work->win);
        TextWin_SetFocus(work->win,0);
        TextWin_Close(work->win);
        *step += 1;
        return 0;
    case CHOICE_WAIT_CLOSE:
        return g_textWins[work->win].state == TEXTWIN_FREEING;
    }
    return 0;
}

/* Show a message in the message window (Msg_Task, g_uiTaskIds[0]); MSG_AUTO_POS / MSG_AUTO_SIZE
 * place and size it automatically; it closes after autoClose frames or on closeButtons (0x80064F94) */
/* 0x80064F94 */
void Msg_Show(unsigned char cpu, short x, short y, short w, short h, int autoClose, short closeButtons, char *text)
{
    short win;
    MsgWork *msg;

    Task_GetWork(g_uiTaskIds[0],&msg);
    msg->cpu = cpu;
    msg->x = x;
    msg->y = y;
    msg->w = w;
    msg->h = h;
    msg->autoClose = autoClose;
    msg->closeButtons = closeButtons;
    msg->text = text;
    win = TextWin_Create(msg->x,msg->y,msg->w,msg->h);
    msg->win = win;
    TextWin_AddText(msg->win,msg->text);
    Task_Start(g_uiTaskIds[0],Msg_Task);
}

/* Close the message window (message task step 100) (0x80065094) */
/* 0x80065094 */
void Msg_Close(void)
{
    Task_SetStep(g_uiTaskIds[0],MSG_STEP_CLOSE);
}

/* Task_IsDone(g_uiTaskIds[0]). (0x800650BC) */
/* 0x800650BC */
int Msg_IsDone(void)
{
    return Task_IsDone(g_uiTaskIds[0]);
}

/* Show a choice window ("%C...%KYes%KNo%E") via MsgChoice_Task (a CPU player picks cpuAnswer);
 * answer index in g_msgAnswer (0 =
 * first). Poll Msg_IsAnswered. (0x800650E4) */
/* 0x800650E4 */
void Msg_AskYesNo(unsigned char player, unsigned char cpuAnswer, short x, short y, short w, short h, char *text)
{
    short win;
    MsgWork *msg;

    Task_GetWork(g_uiTaskIds[0],&msg);
    msg->cpu = player;
    msg->cpuAnswer = cpuAnswer;
    msg->x = x;
    msg->y = y;
    msg->w = w;
    msg->h = h;
    msg->text = text;
    win = TextWin_Create(msg->x,msg->y,msg->w,msg->h);
    msg->win = win;
    TextWin_AddText(msg->win,msg->text);
    TextWin_SetMenu(msg->win,1,2,0);
    TextWin_Open(msg->win);
    Task_Start(g_uiTaskIds[0],MsgChoice_Task);
}

/* Msg_AskYesNo with the cursor starting on item cursor (0x80065208) */
/* 0x80065208 */
void Msg_AskYesNoCursor(u8 player, u8 cpuAnswer, char cursor, short x, short y, short w, short h, char *text)
{
    short win;
    MsgWork *msg;

    Task_GetWork(g_uiTaskIds[0],&msg);
    msg->cpu = player;
    msg->cpuAnswer = cpuAnswer;
    msg->x = x;
    msg->y = y;
    msg->w = w;
    msg->h = h;
    msg->text = text;
    win = TextWin_Create(msg->x,msg->y,msg->w,msg->h);
    msg->win = win;
    TextWin_AddText(msg->win,msg->text);
    TextWin_SetMenu(msg->win,1,2,cursor);
    TextWin_Open(msg->win);
    Task_Start(g_uiTaskIds[0],MsgChoice_Task);
}

/* Msg_AskYesNo with the two answers on rows row, row + 1 of the text (0x8006533C) */
/* 0x8006533C */
void Msg_AskYesNoAt(u8 player, u8 cpuAnswer, char row, short x, short y, short w, short h, char *text)
{
    short win;
    MsgWork *msg;

    Task_GetWork(g_uiTaskIds[0],&msg);
    msg->cpu = player;
    msg->cpuAnswer = cpuAnswer;
    msg->x = x;
    msg->y = y;
    msg->w = w;
    msg->h = h;
    msg->text = text;
    win = TextWin_Create(msg->x,msg->y,msg->w,msg->h);
    msg->win = win;
    TextWin_AddText(msg->win,msg->text);
    TextWin_SetMenu(msg->win,row,2,0);
    TextWin_Open(msg->win);
    Task_Start(g_uiTaskIds[0],MsgChoice_Task);
}

/* Close the question window (0x8006546C) */
/* 0x8006546C */
void Msg_CloseChoice(void)
{
    Task_SetStep(g_uiTaskIds[0],CHOICE_CLOSE);
}

/* Task_IsDone(g_uiTaskIds[0]). (0x80065494) */
/* 0x80065494 */
int Msg_IsAnswered(void)
{
    return Task_IsDone(g_uiTaskIds[0]);
}
