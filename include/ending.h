/*
 * ending.h - game mode 7: the ending
 *
 * The final results screen.
 * The declarations of src/ending.c: its types and constants, the work areas of its tasks, its
 * globals and its functions.
 * See docs/modes.md.
 */

#ifndef ENDING_H
#define ENDING_H

#include "common.h"

/* A player on the results screen (g_resultsRows, Ending_ResultsTask). */
typedef struct ResultsRow
{
    signed char state;           /* 0x00 RESULTS_* */
    signed char rank;            /* 0x01 PlayerSetup.finalRank */
    signed char charId;          /* 0x02 */
    signed char frame;           /* 0x03 CharAnimFrame index */
    signed char timer;           /* 0x04 */
    unsigned char pad05;
    short y;                     /* 0x06 in 1/64 pixels */
    short unused08;              /* 0x08 set to 0, not read */
} ResultsRow;                    /* size 10 */
#define RESULTS_RISING   0
#define RESULTS_PLACED   1   /* in place, its rank still turning */
#define RESULTS_SHOWN    2   /* its rank shown */
#define RESULTS_FALLING  3   /* the last place falls off the screen */

/* Confetti of the ending (g_sparkles, Ending_SparkleTask). */
typedef struct Sparkle
{
    signed char active;          /* 0x00 */
    signed char kind;            /* 0x01 color and shape, 0-23 */
    signed char dir;             /* 0x02 +1 / -1: which way it sways */
    signed char phase;           /* 0x03 animation phase */
    short delay;                 /* 0x04 frames before it starts falling */
    short dx;                    /* 0x06 */
    short x, y;                  /* 0x08 in 1/64 pixels */
    short sway;                  /* 0x0C */
    short dy;                    /* 0x0E */
} Sparkle;                       /* size 0x10 */

/* Circling stars of the ending (g_endingCircles, Ending_CircleTask). */
typedef struct EndingCircle
{
    signed char active;          /* 0x00 */
    signed char mirrored;        /* 0x01 turns the other way */
    short angle;                 /* 0x02 */
    short radius;                /* 0x04 in 1/64 pixels */
    short delay;                 /* 0x06 frames before it starts */
    short cx, cy;                /* 0x08 center, in 1/64 pixels */
    short unused0C;              /* 0x0C 16 for the first, -16 for the others; not read */
    short unused0E;              /* 0x0E set to 0, not read */
} EndingCircle;                  /* size 0x10 */

/* ---- The tasks' work areas (Task.work, 0x64 bytes) ---- */

/* work area of Ending_ResultsTask */
typedef struct EndingResultsWork
{
    signed char shown;   /* 0x00 the rows are drawn */
    signed char busy;   /* 0x01 the title zooms in */
    signed char slot;   /* 0x02 sound slot of the drum roll */
    signed char players;   /* 0x03 */
    signed char lastRank;   /* 0x04 the highest rank: the last place */
    signed char showWinner;   /* 0x05 the sign over the winner */
    signed char winners;   /* 0x06 players ranked first */
    signed char signFrame;   /* 0x07 the sign's animation (sprite + signFrame / 16) */
    signed char signState;   /* 0x08 0, 1 or 2: the picture under the sign (none, 0x32, 0x33) */
    unsigned char pad09[1];
    short scroll;   /* 0x0A counts 16 a frame, 0-1023 (not read) */
    unsigned short wave;   /* 0x0C counts 32 a frame, 0-767 (not read) */
    short barSpeed;   /* 0x0E the last place falling: speed and y (1/64 pixels) of its column */
    short faceSpeed;   /* 0x10 ... of its face */
    short barY;   /* 0x12 */
    short faceY;   /* 0x14 */
    short winnerX;   /* 0x16 the sign's x and y (1/64 pixels) */
    short signY;   /* 0x18 */
    short signAngle;   /* 0x1A the sign's two halves opening */
    unsigned short column;   /* 0x1C width of a player's column */
    short rowStep;   /* 0x1E height between two ranks */
    int zoom;   /* 0x20 the title's (4.12) */
    int timer;   /* 0x24 */
    unsigned char pad28[60];
} EndingResultsWork;

/* work area of Ending_SparkleTask */
typedef struct EndingSparkleWork
{
    short base;   /* 0x00 */
    unsigned char pad02[2];
    unsigned char active;   /* 0x04 */
    unsigned char pad05[95];
} EndingSparkleWork;

/* work area of Ending_CircleTask */
typedef struct EndingCircleWork
{
    short base;   /* 0x00 */
    unsigned char pad02[2];
    unsigned char active;   /* 0x04 */
    unsigned char pad05[95];
} EndingCircleWork;

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern u8 k_resultsRowLayout[5][2];       /* 0x8001759C */
extern u8 k_resultsFaceLayout[5][2];      /* 0x800175A8 */
extern u8 k_endingCircleOrder[4];         /* 0x800B2038 */
extern ResultsRow g_resultsRows[6];       /* 0x80111FA8 */
extern Sparkle g_sparkles[128];           /* 0x80111FE8 */
extern EndingCircle g_endingCircles[16];  /* 0x801127E8 */

/* ---- Functions ---- */

/* src/ending.c */
void Ending_LoadTims(void);                               /* 0x80042AE0 */
void Ending_LoadGraphics(void);                           /* 0x80072A94 */
u32 Ending_ResultsTask(short *step, EndingResultsWork *work); /* 0x80072D18 */
u32 Ending_SparkleTask(short *step, EndingSparkleWork *work); /* 0x800741C4 */
u32 Ending_CircleTask(u16 *step, EndingCircleWork *work); /* 0x800745B0 */
int Ending_Task(short *step, void *work);                 /* 0x80076068 */

#endif /* ENDING_H */
