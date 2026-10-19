/*
 * msg.h - message windows
 *
 * Msg_Show / Msg_AskYesNo and the tasks behind them.
 * The declarations of src/msg.c: its constants, the work areas of its tasks, its globals and its
 * functions.
 * See docs/text.md.
 */

#ifndef MSG_H
#define MSG_H

#include "common.h"

/* Msg_Task's steps (LoadGame_Task closes a message by setting MSG_STEP_CLOSE) */
enum MsgStep
{
    MSG_STEP_OPEN        =   0,  /* the window opens with the focus */
    MSG_STEP_WAIT_OPEN   =   1,
    MSG_STEP_HUMAN       =  10,
    MSG_STEP_WAIT_BUTTON =  11,  /* one of closeButtons, or autoClose frames */
    MSG_STEP_CPU         =  20,  /* a CPU player's: a quarter of a second */
    MSG_STEP_CPU_WAIT    =  21,
    MSG_STEP_CLOSE       = 100,
    MSG_STEP_WAIT_CLOSE  = 101
};

/* Msg_Show's defaults */
#define MSG_AUTO_POS   (-1000)
#define MSG_AUTO_SIZE  (-1)
#define MSG_ANY_BUTTON 0xF1FE    /* closeButtons: any button but L2 and start */
#define MSG_SHOW(text) Msg_Show(g_players[g_curPlayer].isCpu, MSG_AUTO_POS, MSG_AUTO_POS, \
                                MSG_AUTO_SIZE, MSG_AUTO_SIZE, 0x96, 0x60, (text))

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* The message window's parameters: the work area of g_uiTaskIds[0] (Msg_Task, MsgChoice_Task),
 * filled by Msg_Show / Msg_AskYesNo, or directly by dialogs before they start the task. */
typedef struct MsgWork
{
    int win;                     /* 0x00 text window */
    short x, y, w, h;            /* 0x04 MSG_AUTO_POS / MSG_AUTO_SIZE = automatic */
    unsigned short closeButtons; /* 0x0C pad buttons that close it */
    short pad0E;
    int pad10;
    char *text;                  /* 0x14 */
    int autoClose;               /* 0x18 frames until it closes by itself, 0 = wait for a button */
    int timer;                   /* 0x1C */
    unsigned char cpu;           /* 0x20 for a CPU player: closes without input */
    signed char cpuAnswer;       /* 0x21 MsgChoice_Task: the answer a CPU player picks */
    unsigned char pad22[66];
} MsgWork;                       /* size 0x64 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern int g_msgAnswer;     /* 0x800B26C8 */
extern char g_msgBuf[512];  /* 0x800CDE38 message text being built */

/* ---- Functions ---- */

/* src/msg.c */
int Msg_Task(short *step, MsgWork *work);                 /* 0x800565C8 */
int MsgChoice_Task(short *step, MsgWork *work);           /* 0x80056824 */
void Msg_Show(unsigned char cpu, short x, short y, short w, short h, int autoClose,
              short closeButtons, char *text);                /* 0x80064F94 */
void Msg_Close(void);                                     /* 0x80065094 */
int  Msg_IsDone(void);                                    /* 0x800650BC */
void Msg_AskYesNo(unsigned char cpu, unsigned char cpuAnswer, short x, short y,
                  short w, short h, char *text);              /* 0x800650E4 */
void Msg_AskYesNoCursor(u8 player, u8 cpuAnswer, char cursor, short x, short y, short w, short h, char *text); /* 0x80065208 */
void Msg_AskYesNoAt(u8 player, u8 cpuAnswer, char row, short x, short y, short w, short h, char *text); /* 0x8006533C */
void Msg_CloseChoice(void);                               /* 0x8006546C */
int  Msg_IsAnswered(void);                                /* 0x80065494 */

#endif /* MSG_H */
