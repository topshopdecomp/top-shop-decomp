/*
 * boarddraw.h - drawing the board
 *
 * Squares, elevators, stairs, pieces, the backgrounds of each board, the camera, the board's
 * ordering tables.
 * The declarations of src/boarddraw.c: its constants, the work areas of its tasks, its globals and
 * its functions.
 * See docs/board.md.
 */

#ifndef BOARDDRAW_H
#define BOARDDRAW_H

#include "common.h"
#include "psyq.h"
#include "boardmap.h"
#include "gfx.h"

/* Camera_MoveTo mode 2 position that shows board cell (x, floor) at screen position (sx, sy):
 * cells are 5 x 5 units of 16 pixels from the stage's origin */
#define CAMERA_CELL_X(x, sx)      ((short)((g_stageDefs[g_boardStage].originX + (x) * 5) * -16 + (sx)))
#define CAMERA_CELL_Y(floor, sy)  ((short)((g_stageDefs[g_boardStage].originY + (floor) * 5) * -16 + (sy)))

/* Board_DrawStoresTask messages: open / close the elevator doors on the current player's square;
 * it answers ELEVATOR_DOORS_DONE to g_actionTaskIds[0] when they have moved */
#define ELEVATOR_DOORS_OPEN   0xe
#define ELEVATOR_DOORS_CLOSE  0xf
#define ELEVATOR_DOORS_DONE   5

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Board_DrawStoresTask */
typedef struct BoardDrawStoresWork
{
    unsigned char active;   /* 0x00 */
    unsigned char unused01;   /* 0x01 */
    unsigned char highlight;   /* 0x02 */
    unsigned char x;   /* 0x03 */
    unsigned char floor;   /* 0x04 */
    unsigned char pad05[1];
    short slide;   /* 0x06 */
    unsigned char pad08[92];
} BoardDrawStoresWork;

/* work area of Board_FarBackgroundTask */
typedef struct BoardFarBackgroundWork
{
    unsigned char active;   /* 0x00 */
    unsigned char pad01[99];
} BoardFarBackgroundWork;

/* work area of Bg_Stage8Task */
typedef struct BgStage8Work
{
    int bgWaveX;   /* 0x00 */
    int bgWaveY;   /* 0x04 */
    int phaseA;   /* 0x08 */
    int phaseB;   /* 0x0C */
    int wave;   /* 0x10 */
    short angle;   /* 0x14 */
    unsigned char unused16;   /* 0x16 */
    unsigned char unused17;   /* 0x17 */
    unsigned char pad18[76];
} BgStage8Work;

/* work area of Bg_Stage9Task */
typedef struct BgStage9Work
{
    int bgWaveX;   /* 0x00 */
    int bgWaveY;   /* 0x04 */
    int phaseA;   /* 0x08 */
    int phaseB;   /* 0x0C */
    int wave;   /* 0x10 */
    short angle;   /* 0x14 */
    unsigned char unused16;   /* 0x16 */
    unsigned char unused17;   /* 0x17 */
    unsigned char pad18[76];
} BgStage9Work;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 *k_stageCellSprites[GAME_NUM_STAGES];  /* 0x800AAA84 per stage: sprite of each board square (index y * width + x) */
extern char k_bgStage5Frames[340];               /* 0x800AAAAC stage 5 far background: frame (k_bgStage5Sprites) per 80x80 tile, 26 per row */
extern u8 k_bgStage5Sprites[148];                /* 0x800AAC00 */
extern char k_bgStage0Sprites[340];              /* 0x800AAC94 stage 0 far background: sprite per 80x80 tile (0 = none), 26 per row */
extern char k_bgStage1Frames[512];               /* 0x800AADE8 stage 1 far background: sprite per 64x64 tile (0, 1 = none), 32 per row */
extern char k_bgStage2Sprites[948];              /* 0x800AAFE8 stage 2 far background: sprite per 48x48 tile (0 = none), 43 per row */
extern CharAnimFrame *k_pieceAnims[14 * 7];      /* 0x800AB510 board piece animations: [charId * 7 + Player.pieceAnim] */
extern char g_frontOtUsed;                       /* 0x800B2530 */
extern char g_boardOtUsed;                       /* 0x800B253E */
extern int g_bgWaveX;                            /* 0x800B2660 */
extern int g_bgWaveY;                            /* 0x800B2664 */
extern int g_cameraX;                            /* 0x800B2668 */
extern int g_cameraY;                            /* 0x800B266C */
extern int g_cameraTargetX;                      /* 0x800B2688 */
extern int g_cameraTargetY;                      /* 0x800B268C */
extern int g_bgScrollX;                          /* 0x800B2690 */
extern int g_bgScrollY;                          /* 0x800B2694 */
extern u32 g_farBgScrollX;                       /* 0x800B26A0 */
extern u32 g_farBgScrollY;                       /* 0x800B26A4 */
extern int g_bg1ScrollX;                         /* 0x800B26B0 */
extern int g_bg1ScrollY;                         /* 0x800B26B4 */
extern int g_parallaxX;                          /* 0x800B26B8 */
extern int g_bgWave[256];                        /* 0x80111960 */
extern GsOT g_boardOt[2];                        /* 0x80111E20 */
extern GsOT g_frontOt[2];                        /* 0x80111E48 */

/* ---- Functions ---- */

/* src/boarddraw.c */
u32 Board_FrameEnd(void);                                 /* 0x800430F4 */
void Board_FrameStub(void);                               /* clears the board overlay OTs when flagged (0x800431FC) */
void Board_DrawEventPicture(void);                        /* 0x80049FEC */
u32 Board_DrawStoresTask(short *step, BoardDrawStoresWork *work); /* 0x8004A08C */
u32 Board_FarBackgroundTask(short *step, BoardFarBackgroundWork *work); /* 0x8004AF8C */
void Bg_DrawStage5(short scrollX, short scrollY);         /* 0x8004D1F0 */
void Bg_DrawStage0(short scrollX, short scrollY);         /* 0x8004D958 */
void Bg_DrawStage1(short scrollX, short scrollY);         /* 0x8004DB04 */
void Bg_DrawStage2(short scrollX, short scrollY);         /* 0x8004DC8C */
u32 Camera_MoveTo(int mode, int x, int y);                /* 0x8004DED0 */
u32 Bg_Stage8Task(short *step, BgStage8Work *work);       /* 0x8004E0C8 */
u32 Bg_Stage9Task(short *step, BgStage9Work *work);       /* 0x8004E1D0 */
u32 Board_DrawPiecesTask(short *step);                    /* 0x8004E2F4 */
int Piece_GetSprite(short player, u16 pose, short base);  /* 0x80055960 */
void Board_FocusCurrentPlayer(void);                      /* 0x8007B654 */

#endif /* BOARDDRAW_H */
