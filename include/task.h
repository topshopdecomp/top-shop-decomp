/*
 * task.h - the cooperative task scheduler
 *
 * The cooperative task scheduler (64 slots, messages).
 * The declarations of src/task.c: its types and constants, its globals and its functions.
 * See docs/tasks.md.
 */

#ifndef TASK_H
#define TASK_H

#include "common.h"

/* Task callback. Called once per frame by Task_RunAll while the task is running.
 * step: per-task state counter (Task.step), work: per-task scratch (Task.work).
 * Return 1 to finish (task goes back to TASK_ALLOCATED), 0 to keep running. */
typedef int (*TaskFunc)(short *step, void *work);

/* Task.state bits */
#define TASK_ALLOCATED 0x01
#define TASK_RUNNING   0x02
#define TASK_SUSPENDED 0x04

/* Special task ids accepted by Task_SendMessage / Task_ReceiveMessage */
#define TASK_SELF      (-1)
#define TASK_NONE      (-2)
#define TASK_ALL       (-3)
#define TASK_CHILDREN  (-4)
#define TASK_PARENT    (-5)

typedef struct Task
{
    unsigned char state;      /* 0x00 TASK_* bits: 0 free, 1 idle, 3 running, 7 suspended */
    unsigned char priority;   /* 0x01 scheduler pass 0..3 (lower runs first) */
    short unused02;           /* 0x02 not used by the game code */
    short step;               /* 0x04 callback state counter, reset by Task_Start */
    short parent;             /* 0x06 id of the task that started this one, TASK_NONE if none */
    short unused08;           /* 0x08 1 at create, else set by Task_SetUnused08 (which nothing calls); never read */
    short unused0A;            /* 0x0A cleared by Task_Start / Task_SetFuncIdle; never read */
    short msgPending;         /* 0x0C 1 when msgValue holds an unread message */
    short msgSender;          /* 0x0E sender id of pending message, TASK_NONE if none */
    int msgValue;             /* 0x10 */
    int userData;             /* 0x14 Task_SetUserData / Task_GetUserData */
    TaskFunc func;            /* 0x18 */
    unsigned char work[0x64]; /* 0x1C per-task scratch, passed to func */
} Task;                       /* size 0x80 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char g_taskInScheduler;  /* 0x800B24D0 */
extern short g_currentTaskId;   /* 0x800B24D2 */
extern int g_taskCount;         /* 0x800B24D4 */
extern Task g_tasks[64];        /* 0x800CE9F8 */

/* ---- Functions ---- */

/* src/task.c */
/* Task_Start / Task_SetFuncIdle take any task function (int or u32 f(short or u16 *step,
 * T *work), see TaskFunc); Task_Create / Task_GetWork store the work pointer through outWork,
 * the address of the caller's pointer variable. */
int  Task_Create(int priority, void *outWork);            /* 0x80023F64 */
int  Task_Start(short id, void *func);                    /* 0x80023FFC */
int  Task_SendMessage(short target, int value);           /* 0x800240A8 */
int  Task_ReceiveMessage(short from, int *outValue);      /* 0x80024250 */
int  Task_ReceiveMessageIf(short from, int value);        /* 0x800243C0 */
void Task_RunAll(void);                                   /* 0x80024530 */
int  Task_TakeMessageFromIf(short self, short sender, int value); /* 0x80024620 */
void Task_InitAll(void);                                  /* 0x800246A4 */
int  Task_SetUnused08(short id, short value);             /* 0x800246F8 */
int  Task_Free(short id);                                 /* 0x80024758 */
int  Task_GetWork(short id, void *outWork);               /* 0x800247CC */
int  Task_SetFuncIdle(short id, void *func);              /* 0x80024834 */
int  Task_Stop(short id);                                 /* 0x800248A0 */
int  Task_SetStep(short id, short step);                  /* 0x80024904 */
int  Task_GetStep(short id);                              /* 0x80024964 */
int  Task_SetUserData(short id, int value);               /* 0x800249C8 */
int  Task_GetUserData(short id);                          /* 0x80024A28 */
int  Task_Suspend(short id);                              /* 0x80024A8C */
int  Task_Resume(short id);                               /* 0x80024AF0 */
int  Task_GetState(short id);                             /* 0x80024B54 */
int  Task_IsDone(short id);                               /* 0x80024B9C */
int  Task_CountAllocated(void);                           /* 0x80024BF0 */
int  Task_PostMessage(short target, short sender, int value); /* 0x80024C30 */
int  Task_TakeMessageFrom(short self, short sender, int *outValue); /* 0x80024C84 */
int  Task_TakeAnyMessage(short self, int *outValue);      /* 0x80024CFC */
int  Task_TakeAnyMessageIf(short self, int value);        /* 0x80024D6C */

#endif /* TASK_H */
