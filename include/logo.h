/*
 * logo.h - game mode 0: the boot logos
 *
 * The declarations of src/logo.c: the work areas of its tasks and its functions.
 * See docs/modes.md.
 */

#ifndef LOGO_H
#define LOGO_H

#include "common.h"

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Logo_Task */
typedef struct LogoWork
{
    int blend;      /* 0x00 cross-fade 0 .. 0x200000; brightness step = blend >> 17 (0..16) */
    int timer;      /* 0x04 hold countdown */
} LogoWork;

/* ---- Functions ---- */

/* src/logo.c */
int Logo_Task(short *step, void *work);  /* 0x80041464 */

#endif /* LOGO_H */
