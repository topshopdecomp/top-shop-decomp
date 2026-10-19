/*
 * pad.c - controllers
 *
 * Setting up the controllers (multitaps included: up to 8 pads), decoding the buttons each pad
 * sends, the per-player copies the game reads every frame (held, pressed, repeating), and
 * assigning pads to players.
 */
#include "common.h"
#include "topshop.h"

#define PAD_ID_DIGITAL  0x41
#define PAD_ID_MULTITAP 0x80

/* One digital pad's reply in an InitTAP buffer (status 0, id 0x41, then the buttons, active low):
 * pad marked connected and the buttons OR-ed into raw, else marked not connected */
static void Pad_Decode(PadState *pad, u8 *reply)
{
    if (reply[0] == 0 && reply[1] == PAD_ID_DIGITAL)
    {
        pad->connected = 1;
        pad->raw |= ~MAKE_U16(reply[2],reply[3]);
    }
    else
    {
        pad->connected = 0;
    }
}

/* A controller port's InitTAP buffer into its four g_pads: a pad straight in the port is the
 * first of them; a multitap gives four 8-byte replies from byte 2 on */
void Pad_DecodePort(PadState *pads, u8 *buf)
{
    int i;

    if (buf[0] == 0 && buf[1] == PAD_ID_DIGITAL)
    {
        pads[0].connected = 1;
        pads[1].connected = 0;
        pads[2].connected = 0;
        pads[3].connected = 0;
        pads[0].raw |= ~MAKE_U16(buf[2],buf[3]);
    }
    else if (buf[0] == 0 && buf[1] == PAD_ID_MULTITAP)
    {
        for (i = 0; i < 4; i++)
        {
            Pad_Decode(&pads[i],buf + 2 + i * 8);
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            pads[i].connected = 0;
        }
    }
}

/* 0x8001A9F0 */
void Pad_Init(void)
{
    int pad, i;

    ResetCallback();
    InitTAP((char *)g_padBuf0, sizeof(g_padBuf0), (char *)g_padBuf1, sizeof(g_padBuf1));
    InitCARD(0);
    StartCARD();
    _bu_init();
    StartTAP();
    ChangeClearPAD(0);
    Card_OpenEvents();
    for (pad = 0; pad < 8; pad++)
        for (i = 0; i < 16; i++)
            g_padHistory[pad * 16 + i] = 0;
    for (i = 5; i >= 0; i--)
        g_playerPadMap[i] = 0;
}

/* empty (0x8001AAAC) */
void Pad_Unused1(void)
{
}

/* empty (0x8001AAB4) */
void Pad_Unused2(void)
{
}

/* g_playerPads[i] = g_pads[g_playerPadMap[i]] for the 6 players; g_playerPads[6] is
 * always pad 0. Called first in main's frame loop. (0x8001AC4C) */
void Pad_UpdatePlayers(void)
{
    char i;

    g_playerPads[6] = g_pads[0];
    for (i = 0; i < 6; i++)
    {
        g_playerPads[i] = g_pads[g_playerPadMap[i]];
    }
}

/* Give player the first connected controller on which buttons are pressed; -1 if none (0x800654BC) */
/* 0x800654BC */
int Pad_AssignPressed(u32 player, u16 buttons)
{
    short pad;

    for (pad = 0; pad < 8; pad++)
    {
        if ((g_pads[pad].connected != 0) && ((g_pads[pad].pressed & buttons) != 0))
        {
            g_playerPadMap[player & 0xff] = pad;
            return pad;
        }
    }
    return -1;
}

/* Give player controller pad; if it is not connected, controller 0, or for pads 4-7 (second
 * multitap) controller 4 when that one is connected (0x80065540) */
/* 0x80065540 */
void Pad_Assign(u32 player, u8 pad)
{
    if (g_pads[pad].connected == 0)
    {
        if (pad < 4)
        {
            pad = 0;
        }
        else
        {
            pad = (g_pads[4].connected == 1) ? 4 : 0;
        }
    }
    g_playerPadMap[player & 0xff] = pad;
}
