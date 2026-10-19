# The task system (a cooperative scheduler)

Most of the game runs as **tasks**: functions called once a frame, each with its own step counter
and work area, that pass small messages to each other. The code is at `0x80023F64`–`0x80024DDF`;
the C is `src/task.c` and `include/task.h`. Everything here is confirmed from the code.

## Data

| Global | Address | Type | What it is |
|---|---|---|---|
| `g_tasks` | `0x800CE9F8` | `Task[64]` | the task slots, 0x80 bytes each (0x2000 in all) |
| `g_taskInScheduler` | `0x800B24D0` (`0x65C(gp)`) | `char` | 1 while `Task_RunAll` goes through the tasks |
| `g_currentTaskId` | `0x800B24D2` (`0x65E(gp)`) | `short` | the slot being run |
| `g_taskCount` | `0x800B24D4` (`0x660(gp)`) | `int` | the slots allocated |

### The `Task` slot (0x80 bytes)

| Offset | Field | What it holds |
|---|---|---|
| 0x00 | `state` | bits `TASK_ALLOCATED` 1, `TASK_RUNNING` 2, `TASK_SUSPENDED` 4; the values seen are 0 (free), 1 (idle), 3 (running), 7 (suspended) |
| 0x01 | `priority` | the scheduler pass it runs in, 0–3 |
| 0x02 | `unused02` | never accessed |
| 0x04 | `step` | the task's state counter, reset by `Task_Start` |
| 0x06 | `parent` | the task that started it |
| 0x08 | `unused08` | 1 when created, else set by `Task_SetUnused08`; never read |
| 0x0A | `unused0A` | cleared when started; never read |
| 0x0C | `msgPending` | the mailbox: 1 when `msgValue` holds an unread message |
| 0x0E | `msgSender` | who sent it |
| 0x10 | `msgValue` | the message |
| 0x14 | `userData` | `Task_SetUserData` / `Task_GetUserData` |
| 0x18 | `func` | the task's function |
| 0x1C | `work` | 100 bytes of scratch for the function |

## How it works

- **Creating and starting.** `Task_Create(priority, &work)` allocates a slot, idle.
  `Task_Start(id, func)` sets it running with `step = 0` and records the current task as its
  `parent`.
- **Running.** `Task_RunAll` (`0x80024530`) runs once per frame from `main`. For each priority pass,
  0 to 3, it calls `func(&task->step, task->work)` for every slot whose state is exactly 3 (running,
  not suspended) and whose priority is that pass. A function returns 1 when it is finished: the
  state goes from 3 back to 1, and the slot stays allocated.
- **Task functions** are state machines written by hand: `switch (*step) { case 0: ...; (*step)++;
  }`. An example is `LoadJWithTim_Task`, the file loader ([fileio.md](fileio.md)).
- **Ids.** `TASK_SELF` (-1) means the current task, only while inside the scheduler; a message sent
  from outside the scheduler has the sender `TASK_NONE` (-2).

### Messages

Each task has a one-message mailbox: a new message overwrites an unread one.

- `Task_SendMessage(target, value)` sends to a task id, to `TASK_SELF` (-1), to `TASK_ALL` (-3:
  every running task), to `TASK_CHILDREN` (-4: every running task this one started) or to
  `TASK_PARENT` (-5).
- `Task_ReceiveMessage(from, &value)` takes the pending message if it came from `from`: a task id,
  `TASK_SELF`, `TASK_ALL` (any sender), `TASK_CHILDREN` (any running child) or `TASK_PARENT`.

## Answered questions

- `unused08` (set to 1 on create, written by `Task_SetUnused08`) and `unused0A` (cleared on start)
  are never read; `unused02` is never accessed.
- `Task_SetFuncIdle` (`0x80024834`) gives a task a new `func` and leaves it idle (state 1) until it
  is started again; nothing in the game calls it.
