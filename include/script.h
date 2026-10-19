/*
 * script.h - the event script interpreter
 *
 * Loading a script, the task that runs it, the opcode dispatcher, flow control, variables,
 * arithmetic, sound and fade opcodes.
 * The declarations of src/script.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/script.md.
 */

#ifndef SCRIPT_H
#define SCRIPT_H

#include "common.h"
#include "psyq.h"

/* Script call stack (g_scriptStack, 0x801157D8): opcode 0x13 pushes the return address and
 * variables 0xF0-0xFF, opcode 0x14 restores them. */
typedef struct ScriptFrame
{
    unsigned char *ret;          /* 0x00 return address in the script */
    short vars[16];              /* 0x04 saved g_scriptVars[0xF0..0xFF] */
} ScriptFrame;                   /* size 0x24 */

/* Script variables Scene_Update sets every frame (g_scriptVars[SVAR_*]; buttons in the script's
 * bits, see Script_Buttons) */
#define SVAR_RESULT         0xC0   /* SCRIPT_RESULT */
#define SVAR_CUR_HELD       0xC1   /* the current player's buttons held */
#define SVAR_HELD           0xC2   /* + player: buttons held (0 for a CPU player) */
#define SVAR_CUR_PRESSED    0xC8
#define SVAR_PRESSED        0xC9   /* + player */
#define SVAR_NUM_PLAYERS    0xCF
#define SVAR_CUR_PLAYER     0xD0
#define SVAR_FRAME          0xD1   /* counts frames (15 bits) */
#define SVAR_ENTRY          0xD2   /* g_scriptEntry: the entry the script was started at */
#define SVAR_CUR_IS_CPU     0xD3
#define SVAR_IS_CPU         0xD4   /* + player: 0x10 for a CPU player */
#define SVAR_FLOORS         0xDA
#define SVAR_MONTHS         0xDB   /* g_gameConfig.monthsElapsed */
#define SVAR_START_MONEY    0xDC
#define SVAR_GOAL           0xDD
#define SVAR_PAD_HELD       0xDE   /* pad 1 (g_playerPads[6]) */
#define SVAR_PAD_PRESSED    0xDF
#define SVAR_ANY_HELD       0xE0   /* pad 1 and every human player's */
#define SVAR_ANY_PRESSED    0xE1
#define SVAR_CHAR           0xE2   /* the current player's character */
#define SVAR_ACTIVE_PLAYERS 0xE3

/* Work area of the opcodes that wait over several frames (EventScriptWork.scratch, given to
 * Script_Run); each uses its own bytes */
typedef union ScriptScratch
{
    unsigned char savedIsCpu;    /* 0x184, 0x185: the player's isCpu while the dialog plays for it */
    struct
    {
        signed char next;        /* index in g_storeList of the store to close next */
        signed char store;
        signed char squareId;    /* (not read) */
        unsigned char pad03;
        signed char x, floor;    /* its square: the camera goes there */
    } close;                     /* 0x192 */
    struct
    {
        int pad00;
        int squashing;           /* 0 the squash not started yet, 1 running */
    } hide;                      /* 0x101 */
    struct
    {
        int pad00[5];
        int squashing;
    } show;                      /* 0x100 */
    unsigned char bytes[80];
} ScriptScratch;

/* Script opcodes: the first word of each instruction (Script_Run). The names are the script
 * language's (tools/tss.py, scripts/README.md). */
enum ScriptOp
{
    OP_END                   = 0x000,  /* end of the script (frees the talk panel's glyphs) */
    OP_WAIT                  = 0x001,  /* wait n frames (FOREVER: never goes on) */
    OP_WAIT_BUTTONS          = 0x002,  /* wait until a button of the mask is pressed (pad 1); $result
                                          = the buttons */
    OP_WAIT_BUTTONS_FOR      = 0x003,  /* the same for at most n frames ($result = 0 on time-out);
                                          FOREVER: stays here */
    OP_GOTO                  = 0x010,  /* jump */
    OP_SWITCH                = 0x012,  /* jump to the label in the list picked by the value (0 the
                                          first); past the list if out of range */
    OP_CALL                  = 0x013,  /* call a routine: push the return point and $l0-$l15 */
    OP_RETURN                = 0x014,  /* return from a call, restoring $l0-$l15 */
    OP_HALT                  = 0x015,  /* stay here for good */
    OP_IF                    = 0x020,  /* jump if the test holds: == != > < >= <=, & (all bits of b
                                          set in a), !& (no bit in common) */
    OP_SET                   = 0x021,  /* set a variable (every computing statement also sets
                                          $result) */
    OP_ADD                   = 0x023,  /* add */
    OP_SUB                   = 0x024,  /* subtract */
    OP_MUL                   = 0x025,  /* multiply */
    OP_DIV                   = 0x026,  /* divide (rounds toward 0) */
    OP_MOD                   = 0x027,  /* remainder */
    OP_AND                   = 0x028,  /* bitwise and */
    OP_OR                    = 0x029,  /* bitwise or */
    OP_XOR                   = 0x02A,  /* bitwise exclusive or */
    OP_NEGATE                = 0x02B,  /* $x = -$x */
    OP_RANDOM                = 0x02C,  /* a random number 0 .. n-1 */
    OP_GET_INDEXED           = 0x02D,  /* read a table: word [index] of a table in the script, or
                                          variable $base+index */
    OP_SET_INDEXED           = 0x02E,  /* write variable $base+index */
    OP_PLAY_BGM              = 0x030,  /* play song n */
    OP_STOP_BGM              = 0x031,  /* stop the music */
    OP_PLAY_SE               = 0x032,  /* play sound effect n (game sound n+0x17) */
    OP_NOP_33                = 0x033,  /* no effect */
    OP_NOP_34                = 0x034,  /* no effect (reads its operand) */
    OP_NOP_35                = 0x035,  /* no effect */
    OP_PLAY_XA               = 0x036,  /* play voice track file*16+channel of XA set 1 */
    OP_STOP_XA               = 0x037,  /* stop the voice */
    OP_YIELD                 = 0x038,  /* go on next frame */
    OP_IS_XA_PLAYING         = 0x039,  /* $x = 1 while a voice plays */
    OP_FADE_IN               = 0x040,  /* fade in over n frames */
    OP_FADE_OUT              = 0x041,  /* fade out */
    OP_FADE_FROM_WHITE       = 0x042,  /* fade in from white */
    OP_FADE_TO_WHITE         = 0x043,  /* fade out to white */
    OP_FADE_LEVEL_FOR        = 0x044,  /* set the fade level (r, g, b) for n frames, then put it back */
    OP_WAIT_FADE             = 0x045,  /* wait until the fade is done */
    OP_MARKER                = 0x060,  /* a name in the script, skipped (costs a frame; 13 characters
                                          at most) */
    OP_VIEW_SET              = 0x061,  /* the view rectangle (x or y -1: centered); closed */
    OP_VIEW_OPEN             = 0x062,  /* open the view over n frames */
    OP_VIEW_CLOSE            = 0x063,  /* close the view */
    OP_VIEW_WAIT             = 0x064,  /* wait for the view to open or close */
    OP_LAYER_INIT            = 0x070,  /* background layer n shows scene image (priority, LAYER
                                          flags, BANDn: lines per band) */
    OP_LAYER_POS             = 0x071,  /* layer position */
    OP_LAYER_SCROLL          = 0x072,  /* layer scrolls by vx, vy a frame */
    OP_LAYER_LINES           = 0x073,  /* x offset of bands first..first+count */
    OP_LAYER_LINE_STEPS      = 0x074,  /* x offset steps of bands (each frame) */
    OP_LAYER_WAVE            = 0x075,  /* bands follow a sine wave */
    OP_LAYER_WAVE_SCROLL     = 0x076,  /* the wave moves by step a frame */
    OP_SPRITE_INIT           = 0x080,  /* sprite n shows an image at x, y (scale 1, angle 0) */
    OP_SPRITE_HIDE           = 0x081,  /* remove sprite n */
    OP_SPRITE_IMAGE          = 0x082,  /* image and attr of sprite n (stops its animation and
                                          movement) */
    OP_SPRITE_ATTR           = 0x083,  /* attr of sprite n */
    OP_SPRITE_MOVE_BY        = 0x084,  /* move sprite n by dx, dy */
    OP_SPRITE_POS            = 0x085,  /* position of sprite n */
    OP_SPRITE_MOVE_TO        = 0x086,  /* sprite n moves to x, y at speed pixels a frame (0: waits x
                                          frames, -1: jumps there) */
    OP_SPRITE_SCALE_BY       = 0x087,  /* add to the scales (256 = 1) */
    OP_SPRITE_SCALE          = 0x088,  /* set the scales (256 = 1) */
    OP_NOP_89                = 0x089,  /* no effect */
    OP_SPRITE_ROTATE         = 0x08D,  /* turn sprite n (32768 = a full turn) */
    OP_SPRITE_ANGLE          = 0x08E,  /* set the angle */
    OP_NOP_8F                = 0x08F,  /* no effect */
    OP_SPRITE_ANIMATE        = 0x090,  /* sprite n plays an animation (frames) */
    OP_SPRITE_PATH           = 0x091,  /* sprite n follows a path (steps) */
    OP_SPRITE_GET_STATE      = 0x092,  /* $x = sprite n's state (1 animating, 2 moving, 0x10 on a
                                          path) */
    OP_SPRITE_GET_POS        = 0x093,  /* position of sprite n */
    OP_SPRITE_GET_SCALE      = 0x094,  /* scales of sprite n */
    OP_SPRITE_GET_ANGLE      = 0x095,  /* angle of sprite n */
    OP_GET_IMAGE_SIZE        = 0x096,  /* width and height of scene image n */
    OP_SET_DIM               = 0x097,  /* how dark DIM sprites are (0-255) */
    OP_RECT_SET              = 0x0A0,  /* rectangle n (never used for anything) */
    OP_RECT_HIDE             = 0x0A1,  /* rectangle n off */
    OP_RECT_TO_VIEW          = 0x0A2,  /* rectangle n = the view */
    OP_STEP_TOWARD           = 0x0B0,  /* $to_x, $to_y = x, y moved distance at angle */
    OP_TALK_OPEN             = 0x0C0,  /* slide the talk panel in */
    OP_TALK_CLOSE            = 0x0C1,  /* slide it out */
    OP_TALK_WAIT_OPEN        = 0x0C2,  /* wait until it is in */
    OP_TALK_WAIT_CLOSED      = 0x0C3,  /* wait until it is out */
    OP_TALK_CURSOR           = 0x0C4,  /* highlight row n */
    OP_TALK_NO_CURSOR        = 0x0C5,  /* no highlighted row */
    OP_TALK_SPEAKER          = 0x0C6,  /* who speaks: HOST (wide panel) or PLAYER (the current
                                          player's frame) */
    OP_TALK_CLEAR            = 0x0C8,  /* clear the text */
    OP_TALK_SHOW             = 0x0C9,  /* show the text (operand 0; another value does nothing) */
    OP_TALK                  = 0x0CA,  /* add a text to the panel (codes: %NMY, %Nnn, %K, %Snn) */
    OP_TALK_NUMBER           = 0x0CB,  /* add a number (width digits) */
    OP_TALK_PLAYER           = 0x0CC,  /* add player n's name */
    OP_TALK_STORE_TYPE       = 0x0CD,  /* add the store type name of store n */
    OP_GET_TALK_Y            = 0x0CE,  /* $x = the talk panel's y */
    OP_TALK_DATA             = 0x0CF,  /* add the text at a label (or an offset in a variable) */
    OP_NOP_D0                = 0x0D0,  /* no effect */
    OP_NOP_D1                = 0x0D1,  /* no effect */
    OP_NOP_F1                = 0x0F1,  /* no effect */
    OP_NOP_F2                = 0x0F2,  /* no effect */
    OP_PORTRAIT_SHOW         = 0x100,  /* show the current player's portrait with a face, unsquashing
                                          it */
    OP_PORTRAIT_HIDE         = 0x101,  /* squash the portrait away */
    OP_PORTRAIT_FACE         = 0x102,  /* change the portrait's face */
    OP_COUNT_STORES          = 0x140,  /* $x = how many stores the player(s) own */
    OP_GET_STORE             = 0x141,  /* $x = their n-th store (out of range: 0 here, a leftover
                                          value in the original) */
    OP_EARN                  = 0x180,  /* the player(s) earn an amount (negative: pay) */
    OP_STORES_SET_GOODS      = 0x181,  /* the stores get count goods slots at a level (0 the most
                                          expensive) */
    OP_NOP_182               = 0x182,  /* no effect */
    OP_NOP_183               = 0x183,  /* no effect */
    OP_BUY_LOT               = 0x184,  /* the current player may open a store on a vacant lot along
                                          the route */
    OP_SELL_STORE            = 0x185,  /* the current player sells a store for a percentage of its
                                          value (SELL_*) */
    OP_NOP_186               = 0x186,  /* no effect */
    OP_NOP_187               = 0x187,  /* no effect */
    OP_RESULT_ANOTHER        = 0x188,  /* result 1: another event follows */
    OP_ADVANCE_MONTHS        = 0x189,  /* the calendar moves on n months */
    OP_MOVE_STEPS            = 0x18A,  /* the current player moves n squares (result 3) */
    OP_TURN_BACK             = 0x18B,  /* the current player stops (STOP) or turns around (REVERSE) */
    OP_NO_ELEVATOR           = 0x18C,  /* the player(s) can't take the elevator for n months */
    OP_STORES_SET_PRICE      = 0x18D,  /* the stores sell at a percentage for n months */
    OP_MONEY_PERCENT         = 0x18E,  /* the player(s)' money * percent / 100 */
    OP_COPY_MONEY            = 0x18F,  /* the player(s)' money = player n's */
    OP_RESULT_REROLL         = 0x190,  /* result 2: the player rolls again */
    OP_STORES_CLOSE_FOR      = 0x191,  /* the stores close for n months */
    OP_STORES_CLOSE          = 0x192,  /* the stores close (only_empty ON: those without goods); the
                                          owner gets a percentage */
    OP_SET_SHOES             = 0x193,  /* the player(s) put on (ON) or take off the Chirp-Chirp shoes */
    OP_STORES_GOODS_PRICIER  = 0x194,  /* the stores' goods n levels more expensive (negative:
                                          cheaper) */
};

/* OP_IF's test (its fourth operand; ==, !=, >, <, >=, <=, & and !& in the script language) */
enum ScriptTest
{
    TEST_EQ,                     /* a == b */
    TEST_NE,                     /* a != b */
    TEST_GT,                     /* a > b */
    TEST_LT,                     /* a < b */
    TEST_GE,                     /* a >= b */
    TEST_LE,                     /* a <= b */
    TEST_ALL_BITS,               /* (a & b) == b: all bits of b set in a */
    TEST_NO_BITS                 /* (a & b) == 0: no common bits */
};

/* The event script's outcome: EventScript_Task's user data, set by opcodes, read by
 * Board_PhaseEndTurn */
#define EVENT_RESULT_NONE     0  /* nothing more: the turn goes on */
#define EVENT_RESULT_ANOTHER  1  /* OP_RESULT_ANOTHER: another event follows */
#define EVENT_RESULT_REROLL   2  /* OP_RESULT_REROLL: the player rolls again */
#define EVENT_RESULT_MOVE     3  /* OP_MOVE_STEPS: the player moves n squares */

/* Script operands are the big-endian words after the opcode word. A word 0x8000-0x80FF names the
 * variable g_scriptVars[word & 0x7FFF]; any other word is a constant (Script_Value). */
#define SCRIPT_IS_VAR(word)  ((short)(word) < -0x7F00)
#define SCRIPT_RESULT        g_scriptVars[SVAR_RESULT]   /* the value the last computing opcode produced */
/* The word at byte offset `at` of the current opcode */
#define SCRIPT_WORD(at)      ((short)MAKE_U16(g_scriptPc[at], g_scriptPc[(at) + 1]))
/* The value of the opcode's only operand */
#define SCRIPT_ARG0          ((short)Script_Value(SCRIPT_WORD(2)))
/* Read the opcode's first n operand words into g_scriptOps[1..n] */
#define SCRIPT_READ_OPS(n)   Mem_CopySwap16((u8 *)(g_scriptOps + 1), g_scriptPc + 2, (n) * 2)
/* The value of operand i (after SCRIPT_READ_OPS) */
#define SCRIPT_ARG(i)        ((short)Script_Value(g_scriptOps[i]))
/* Store value into the variable the word names (nothing for a constant) */
#define SCRIPT_VAR_SET(word, value) \
    do { if (SCRIPT_IS_VAR(word)) g_scriptVars[(word) & 0x7FFF] = (value); } while (0)

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of LoadJWithTim_Task */
typedef struct LoadJWithTimWork
{
    int file;   /* 0x00 FileId */
    short handle;   /* 0x04 */
    short block;   /* 0x06 */
    unsigned char pad08[92];
} LoadJWithTimWork;

/* work area of EventScript_Task */
typedef struct EventScriptWork
{
    unsigned char pad00[12];
    unsigned char running;   /* 0x0C */
    unsigned char pad0D[7];
    ScriptScratch scratch;   /* 0x14 */
} EventScriptWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 *g_scriptData;               /* 0x800B269C */
extern short g_scriptFadeSaved[3];     /* 0x800B2728 */
extern u8 *g_scriptPc;                 /* 0x800B2730 */
extern u8 g_scriptRunning;             /* 0x800B2734 */
extern int g_scriptWait;               /* 0x800B2738 */
extern int g_scriptDepth;              /* 0x800B2740 */
extern char g_scriptErrorShown;        /* 0x800B275C */
extern u32 g_scriptOpcode;             /* 0x800B2810 */
extern u32 g_scriptOpOffset;           /* 0x800B2814 */
extern short g_scriptVars[256];        /* 0x801155D8 */
extern ScriptFrame g_scriptStack[16];  /* 0x801157D8 */
extern short g_scriptOps[8];           /* 0x80117B10 */

/* ---- Functions ---- */

/* src/script.c */
/* tasks run by the board */
int LoadJWithTim_Task(short *step, LoadJWithTimWork *work); /* 0x80042EC4 */
void Script_Reset(void);                                  /* 0x8007C034 */
u32 Script_Run(ScriptScratch *scratch);                   /* 0x8007C124 */
u16 Script_Buttons(u16 pad);                              /* C-only helper, no original address */
u32 Script_OpEnd(void);                                   /* 0x8007F3EC */
u32 Script_OpWaitButtons(void);                           /* 0x8007F520 */
u32 Script_OpWaitButtonsFor(void);                        /* 0x8007F600 */
u32 Script_OpIf(void);                                    /* 0x8007F728 */
u32 Script_OpGetIndexed(void);                            /* 0x8007F86C */
u32 Script_OpFadeLevelFor(void);                          /* 0x8007F950 */
u32 Script_OpStepToward(void);                            /* 0x80080B64 */
int EventScript_Task(short *step, EventScriptWork *work); /* 0x80083B34 */
void Script_Stop(void);                                   /* 0x80083C38 */
void Script_Advance(u32 bytes);                           /* 0x80083C48 */
void Script_Jump(short target);                           /* 0x80083C64 */
void Script_Restart(void);                                /* 0x80083C9C */
int Script_Value(u32 operand);                            /* 0x80083CB0 */
u32 Script_OpWait(void);                                  /* 0x80083CE8 */
u32 Script_OpGoto(void);                                  /* 0x80083D58 */
u32 Script_OpSwitch(void);                                /* 0x80083D88 */
u32 Script_OpCall(void);                                  /* 0x80083E40 */
u32 Script_OpReturn(void);                                /* 0x80083EE4 */
u32 Script_OpHalt(void);                                  /* 0x80083F6C */
u32 Script_OpSet(void);                                   /* 0x80083F7C */
u32 Script_OpSetIndexed(void);                            /* 0x80084000 */
u32 Script_OpAdd(void);                                   /* 0x800840A4 */
u32 Script_OpSub(void);                                   /* 0x80084148 */
u32 Script_OpMul(void);                                   /* 0x800841EC */
u32 Script_OpDiv(void);                                   /* 0x80084294 */
u32 Script_OpMod(void);                                   /* 0x80084360 */
u32 Script_OpAnd(void);                                   /* 0x8008442C */
u32 Script_OpOr(void);                                    /* 0x800844D0 */
u32 Script_OpXor(void);                                   /* 0x80084574 */
u32 Script_OpNeg(void);                                   /* 0x80084618 */
u32 Script_OpRandom(void);                                /* 0x800846A4 */
u32 Script_OpPlayBgm(void);                               /* 0x8008477C */
u32 Script_OpStopBgm(void);                               /* 0x800847D4 */
u32 Script_OpPlaySe(void);                                /* 0x800847FC */
u32 Script_OpNop33(void);                                 /* 0x80084858 */
u32 Script_OpNop34(void);                                 /* 0x80084878 */
u32 Script_OpNop35(void);                                 /* 0x800848B8 */
u32 Script_OpYield(void);                                 /* 0x800848D8 */
u32 Script_OpIsXaPlaying(void);                           /* 0x800848F8 */
u32 Script_OpPlayXa(void);                                /* 0x8008496C */
u32 Script_OpStopXa(void);                                /* 0x800849E4 */
u32 Script_OpFadeIn(void);                                /* 0x80084A0C */
u32 Script_OpFadeOut(void);                               /* 0x80084A54 */
u32 Script_OpFadeFromWhite(void);                         /* 0x80084A9C */
u32 Script_OpFadeToWhite(void);                           /* 0x80084AE4 */
u32 Script_OpWaitFade(void);                              /* 0x80084B2C */
void Script_PushFrame(u8 *ret);                           /* 0x80085BC4 */
u8 * Script_PopFrame(void);                               /* 0x80085C38 */

#endif /* SCRIPT_H */
