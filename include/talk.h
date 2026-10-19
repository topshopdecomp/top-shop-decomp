/*
 * talk.h - which comment a character makes
 *
 * The characters' comments at the start and the end of a turn (which talk script).
 * The declarations of src/talk.c: its functions.
 * See docs/board.md.
 */

#ifndef TALK_H
#define TALK_H

#include "common.h"

/* ---- Functions ---- */

/* src/talk.c */
u32 TurnStart_PickScript(u16 player);  /* 0x8008BCA8 */
u32 TurnEnd_PickScript(u32 player);    /* 0x8008C95C */

#endif /* TALK_H */
