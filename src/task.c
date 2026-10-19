/*
 * task.c - the cooperative task scheduler
 *
 * 64 task slots (g_tasks). Each frame, main calls Task_RunAll, which calls the func of every
 * running task, priority pass 0 first through pass 3. A func gets &task->step (a state counter
 * it advances itself) and task->work (100 bytes of scratch) and returns 1 when it is finished.
 *
 * Each task has a one-entry mailbox: Task_SendMessage overwrites it, Task_ReceiveMessage
 * consumes it.
 */
#include "psyq.h"
#include "task.h"

#define TASK_MAX 64

/* g_tasks, g_taskInScheduler, g_currentTaskId, g_taskCount: see task.h
 * (defined by the generated assembly until data is migrated to C). */

/* Inside the scheduler, TASK_SELF (-1) means the task currently executing. */
#define RESOLVE_SELF(id) \
    if ((id) == TASK_SELF && g_taskInScheduler == 1) (id) = g_currentTaskId

/* 0x80023F64 */
int Task_Create(int priority, void *outWork)
{
    Task *t = g_tasks;
    int id;

    for (id = 0; id < TASK_MAX; id++, t++)
    {
        if (!(t->state & TASK_ALLOCATED))
        {
            t->parent = TASK_NONE;
            t->state = TASK_ALLOCATED;
            t->step = 0;
            t->priority = priority & 3;
            t->unused08 = 1;
            t->msgPending = 0;
            t->msgSender = TASK_NONE;
            t->msgValue = 0;
            t->userData = 0;
            if (outWork != NULL)
                *(void **)outWork = t->work;
            g_taskCount++;
            return id;
        }
    }
    return -1;
}

/* 0x80023FFC */
int Task_Start(short id, void *func)
{
    Task *t;

    RESOLVE_SELF(id);
    t = &g_tasks[id];
    if (!(t->state & TASK_ALLOCATED))
        return -1;

    t->parent = g_taskInScheduler ? g_currentTaskId : TASK_NONE;
    t->state = TASK_ALLOCATED | TASK_RUNNING;
    t->step = 0;
    t->unused0A = 0;
    t->func = (TaskFunc)func;
    t->userData = 0;
    return 0;
}

/* 0x800240A8 */
int Task_SendMessage(short target, int value)
{
    short self = g_taskInScheduler ? g_currentTaskId : TASK_NONE;
    short i;

    switch (target)
    {
    case TASK_CHILDREN:
        for (i = 0; i < TASK_MAX; i++)
        {
            if ((g_tasks[i].state & TASK_RUNNING) && g_tasks[i].parent == self)
                Task_PostMessage(i, g_tasks[i].parent, value);
        }
        return 0;
    case TASK_ALL:
        for (i = 0; i < TASK_MAX; i++)
        {
            if (g_tasks[i].state & TASK_RUNNING)
                Task_PostMessage(i, self, value);
        }
        return 0;
    case TASK_PARENT:
        target = g_tasks[self].parent;
        break;
    case TASK_SELF:
        target = g_currentTaskId;
        break;
    }
    return Task_PostMessage(target, self, value);
}

/* 0x80024250 */
int Task_ReceiveMessage(short from, int *outValue)
{
    short self = g_taskInScheduler ? g_currentTaskId : TASK_NONE;
    short i;

    switch (from)
    {
    case TASK_CHILDREN:
        /* first pending message sent by any running child */
        for (i = 0; i < TASK_MAX; i++)
        {
            if ((g_tasks[i].state & TASK_RUNNING) && g_tasks[i].parent == self &&
                Task_TakeMessageFrom(g_tasks[i].parent, i, outValue) == 0)
                return 0;
        }
        return -1;
    case TASK_ALL:
        return Task_TakeAnyMessage(self, outValue);
    case TASK_PARENT:
        return Task_TakeMessageFrom(self, g_tasks[self].parent, outValue);
    case TASK_SELF:
        return Task_TakeMessageFrom(self, self, outValue);
    }
    return Task_TakeMessageFrom(self, from, outValue);
}

/* 0x800243C0  Like Task_ReceiveMessage, but only consumes a message equal to value. */
int Task_ReceiveMessageIf(short from, int value)
{
    short self = g_taskInScheduler ? g_currentTaskId : TASK_NONE;
    short i;

    switch (from)
    {
    case TASK_CHILDREN:
        for (i = 0; i < TASK_MAX; i++)
        {
            if ((g_tasks[i].state & TASK_RUNNING) && g_tasks[i].parent == self &&
                Task_TakeMessageFromIf(g_tasks[i].parent, i, value) == 0)
                return 0;
        }
        return -1;
    case TASK_ALL:
        return Task_TakeAnyMessageIf(self, value);
    case TASK_PARENT:
        return Task_TakeMessageFromIf(self, g_tasks[self].parent, value);
    case TASK_SELF:
        return Task_TakeMessageFromIf(self, self, value);
    }
    return Task_TakeMessageFromIf(self, from, value);
}

/* 0x80024530 */
void Task_RunAll(void)
{
    Task *t;
    int pass, id, ret;

    g_taskInScheduler = 1;
    for (pass = 0; pass < 4; pass++)
    {
        t = g_tasks;
        for (id = 0; id < TASK_MAX; id++, t++)
        {
            g_currentTaskId = id;
            if (t->state == (TASK_ALLOCATED | TASK_RUNNING) && t->priority == pass)
            {
                ret = t->func(&t->step, t->work);
                /* func may have stopped/suspended itself; only finish if still running */
                if (t->state == (TASK_ALLOCATED | TASK_RUNNING) && ret == 1)
                    t->state = TASK_ALLOCATED;
            }
        }
    }
    g_taskInScheduler = 0;
}

/* 0x80024620 */
int Task_TakeMessageFromIf(short self, short sender, int value)
{
    Task *t;

    if (self < 0)
        return -1;
    t = &g_tasks[self];
    if (!(t->state & TASK_RUNNING) || t->msgPending != 1 || t->msgSender != sender)
        return -1;
    if (value != t->msgValue)
        return -1;
    t->msgPending = 0;
    t->msgSender = TASK_NONE;
    t->msgValue = 0;
    return 0;
}

/* 0x800246A4 */
void Task_InitAll(void)
{
    int i;

    g_taskInScheduler = 0;
    g_taskCount = 0;
    memset(g_tasks, 0, sizeof(g_tasks));
    for (i = TASK_MAX - 1; i >= 0; i--)
        g_tasks[i].state = 0;
}

/* 0x800246F8 */
int Task_SetUnused08(short id, short value)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    g_tasks[id].unused08 = value;
    return 0;
}

/* 0x80024758 */
int Task_Free(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    g_tasks[id].state = 0;
    g_taskCount--;
    return 0;
}

/* 0x800247CC */
int Task_GetWork(short id, void *outWork)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    *(void **)outWork = g_tasks[id].work;
    return 0;
}

/* 0x80024834  Give the task a new func and leave it idle (allocated, not running) until it is
 * started again. Nothing in the game calls it. */
int Task_SetFuncIdle(short id, void *func)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    g_tasks[id].state = TASK_ALLOCATED;
    g_tasks[id].unused0A = 0;
    g_tasks[id].func = (TaskFunc)func;
    return 0;
}

/* 0x800248A0  Stop a running task; the slot stays allocated. */
int Task_Stop(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_RUNNING))
        return -1;
    g_tasks[id].state = TASK_ALLOCATED;
    return 0;
}

/* 0x80024904 */
int Task_SetStep(short id, short step)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_RUNNING))
        return -1;
    g_tasks[id].step = step;
    return 0;
}

/* 0x80024964 */
int Task_GetStep(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_RUNNING))
        return -1;
    return g_tasks[id].step;
}

/* 0x800249C8 */
int Task_SetUserData(short id, int value)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    g_tasks[id].userData = value;
    return 0;
}

/* 0x80024A28 */
int Task_GetUserData(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_ALLOCATED))
        return -1;
    return g_tasks[id].userData;
}

/* 0x80024A8C */
int Task_Suspend(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_RUNNING))
        return -1;
    g_tasks[id].state = TASK_ALLOCATED | TASK_RUNNING | TASK_SUSPENDED;
    return 0;
}

/* 0x80024AF0 */
int Task_Resume(short id)
{
    RESOLVE_SELF(id);
    if (!(g_tasks[id].state & TASK_SUSPENDED))
        return -1;
    g_tasks[id].state = TASK_ALLOCATED | TASK_RUNNING;
    return 0;
}

/* 0x80024B54 */
int Task_GetState(short id)
{
    RESOLVE_SELF(id);
    return (signed char)g_tasks[id].state;
}

/* 0x80024B9C */
int Task_IsDone(short id)
{
    RESOLVE_SELF(id);
    return !(g_tasks[id].state & TASK_RUNNING);
}

/* 0x80024BF0 */
int Task_CountAllocated(void)
{
    int i, n = 0;

    for (i = 0; i < TASK_MAX; i++)
    {
        if (g_tasks[i].state & TASK_ALLOCATED)
            n++;
    }
    return n;
}

/* 0x80024C30 */
int Task_PostMessage(short target, short sender, int value)
{
    if (target < 0 || !(g_tasks[target].state & TASK_RUNNING))
        return -1;
    g_tasks[target].msgPending = 1;
    g_tasks[target].msgSender = sender;
    g_tasks[target].msgValue = value;
    return 0;
}

/* 0x80024C84 */
int Task_TakeMessageFrom(short self, short sender, int *outValue)
{
    Task *t;

    if (self < 0)
        return -1;
    t = &g_tasks[self];
    if (!(t->state & TASK_RUNNING) || t->msgPending != 1 || t->msgSender != sender)
        return -1;
    *outValue = t->msgValue;
    t->msgPending = 0;
    t->msgSender = TASK_NONE;
    t->msgValue = 0;
    return 0;
}

/* 0x80024CFC */
int Task_TakeAnyMessage(short self, int *outValue)
{
    Task *t;

    if (self < 0)
        return -1;
    t = &g_tasks[self];
    if (!(t->state & TASK_RUNNING) || t->msgPending != 1)
        return -1;
    *outValue = t->msgValue;
    t->msgPending = 0;
    t->msgSender = TASK_NONE;
    t->msgValue = 0;
    return 0;
}

/* 0x80024D6C */
int Task_TakeAnyMessageIf(short self, int value)
{
    Task *t;

    if (self < 0)
        return -1;
    t = &g_tasks[self];
    if (!(t->state & TASK_RUNNING) || t->msgPending != 1)
        return -1;
    if (value != t->msgValue)
        return -1;
    t->msgPending = 0;
    t->msgSender = TASK_NONE;
    t->msgValue = 0;
    return 0;
}
