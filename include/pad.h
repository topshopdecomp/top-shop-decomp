/*
 * pad.h - controllers
 *
 * Set-up, decoding what the pads send, the per-player copies, assigning pads to players.
 * The declarations of src/pad.c: its types, its globals and its functions.
 * See docs/system.md.
 */

#ifndef PAD_H
#define PAD_H

#include "common.h"

/* Per-controller state (g_pads[8]: port 1 taps A-D, port 2 taps A-D),
 * and per-player copies (g_playerPads[7]). Button bits are active-high. */
typedef struct PadState
{
    short connected;          /* 0x00 1 if a digital pad (id 0x41) is present */
    unsigned short raw;       /* 0x02 buttons OR-ed in by the VSync callback, cleared each frame */
    unsigned short held;      /* 0x04 buttons down this frame */
    unsigned short pressed;   /* 0x06 newly pressed this frame */
    unsigned short repeat;    /* 0x08 pressed, or auto-repeat while held (fires once held 9 frames, then periodically) */
    unsigned short changed;   /* 0x0A held XOR previous frame */
} PadState;                   /* size 12 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern char g_playerPadMap[6];    /* 0x800B2400 player -> g_pads index */
extern PadState g_playerPads[7];  /* 0x800B39D0 per player (via g_playerPadMap); [6] = pad 0 */
extern PadState g_pads[8];        /* 0x800B3A28 decoded per physical pad (multitap: 4 per port) */
extern u8 g_padBuf0[34];          /* 0x800B3B88 InitTAP receive buffer, port 1 */
extern u8 g_padBuf1[34];          /* 0x800B3BAA port 2 */
extern u16 g_padHistory[128];     /* 0x800B3BD0 16 frames x 8 pads */

/* ---- Functions ---- */

/* src/pad.c */
void Pad_DecodePort(PadState *pads, u8 *buf);    /* C-only helper, no original address */
void Pad_Init(void);                             /* 0x8001A9F0 */
void Pad_Unused1(void);                          /* 0x8001AAAC */
void Pad_Unused2(void);                          /* 0x8001AAB4 */
void Pad_UpdatePlayers(void);                    /* 0x8001AC4C */
int Pad_AssignPressed(u32 player, u16 buttons);  /* 0x800654BC */
void Pad_Assign(u32 player, u8 pad);             /* 0x80065540 */

#endif /* PAD_H */
