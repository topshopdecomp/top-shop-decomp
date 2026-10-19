/*
 * staffroll.h - game mode 9: the staff roll
 *
 * The declarations of src/staffroll.c: its types, the work areas of its tasks, its globals and its
 * functions.
 * See docs/modes.md.
 */

#ifndef STAFFROLL_H
#define STAFFROLL_H

#include "common.h"

/* A page of the staff roll (k_staffRoll, 0x800AC218). */
typedef struct StaffRollSection
{
    unsigned short wait;         /* 0x00 frames before the next page */
    unsigned char lines;         /* 0x02 sprites on the page, 0 = end */
    unsigned char sprite[7];     /* 0x03 */
} StaffRollSection;              /* size 10 */

/* a text line of a staff roll page (StaffRollRunWork.rows): it slides in from the side
 * (x, a 8.8-ish offset from the center, / 64 = pixels) and out again */
typedef struct RollRow
{
    short x;   /* 0x00 */
    short dx;   /* 0x02 */
} RollRow;

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of StaffRoll_RunTask */
typedef struct StaffRollRunWork
{
    signed char mode;   /* 0x00 */
    signed char fading;   /* 0x01 */
    signed char started;   /* 0x02 */
    signed char section;   /* 0x03 */
    signed char lines;   /* 0x04 */
    unsigned char pad05[1];
    short blink;   /* 0x06 */
    short y;   /* 0x08 */
    RollRow rows[7];   /* 0x0A the page's text lines */
    signed char phase;   /* 0x26 WALKERS_* */
    signed char walker;   /* 0x27 the next character to walk across (StaffRoll_WalkerTask) */
    short wait;   /* 0x28 */
    unsigned char pad2A[2];
    int frame;   /* 0x2C */
    unsigned char pad30[52];
} StaffRollRunWork;

/* work area of StaffRoll_WalkerTask */
typedef struct StaffRollWalkerWork
{
    unsigned int charIndex;   /* 0x00 set by StaffRoll_Task: the character (0-13) */
    unsigned char started;   /* 0x04 */
    signed char leftward;   /* 0x05 walking right to left */
    signed char lower;   /* 0x06 on the lower line (y 160, else 32) */
    unsigned char pad07[1];
    short walk;   /* 0x08 8.8: walking animation frame 0-5 */
    unsigned char unused0A;   /* 0x0A */
    unsigned char unused0B;   /* 0x0B */
    int x;   /* 0x0C 20.12 */
    int y;   /* 0x10 20.12 */
    unsigned char pad14[80];
} StaffRollWalkerWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern StaffRollSection k_staffRoll[12];  /* 0x800AC218 */
extern u32 k_staffRollInit;               /* 0x800B2044 */
extern char g_staffRollState;             /* 0x800B2652 */

/* ---- Functions ---- */

/* src/staffroll.c */
void StaffRoll_LoadGraphics(void);                        /* 0x80075140 */
u32 StaffRoll_RunTask(u16 *step, StaffRollRunWork *work); /* 0x800753FC */
u32 StaffRoll_WalkerTask(u16 *step, StaffRollWalkerWork *work); /* 0x80075DD0 */
int StaffRoll_Task(short *step, void *work);              /* 0x8007618C */

#endif /* STAFFROLL_H */
