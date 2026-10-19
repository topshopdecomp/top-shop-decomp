/*
 * scene.h - the scene the scripts draw
 *
 * View, layers, sprites, the talk panel and the portrait; the script variables refreshed every
 * frame.
 * The declarations of src/scene.c: its types and constants, its globals and its functions.
 * See docs/script.md.
 */

#ifndef SCENE_H
#define SCENE_H

#include "common.h"
#include "psyq.h"
#include "gfx.h"
#include "tilesheet.h"

/* Script image attr bits (SceneSprite.attr, Scene_DrawImage) */
#define SCENE_FLIP_X        0x0002
#define SCENE_FLIP_Y        0x0004
#define SCENE_SEMITRANS     0x0008   /* (0x0400 too) */
/* bits 4-7: alignment, row * 4 + column (top / middle / bottom, left / center / right) */
#define SCENE_OVER          0x0100   /* 32 ordering table slots further; a sprite is hidden while the view is visible */
#define SCENE_DIM           0x0200   /* darkened by g_sceneDim */
#define SCENE_SEMITRANS2    0x0400

/* SceneView.step: of the transition */
#define VIEW_STEP_START     0    /* not started yet */
#define VIEW_STEP_RUNNING   1

/* SceneView.transition */
#define VIEW_NO_TRANSITION  0
#define VIEW_OPENING        1
#define VIEW_CLOSING        2
/* SceneView.style: how it opens and closes (AT_ONCE, GROW, SLIDE in the script language) */
#define VIEW_AT_ONCE        0
#define VIEW_GROW           1    /* the height grows */
#define VIEW_SLIDE          2    /* slides in from the left */

/* A script image number: bits 12-14 the group (the script language's IMG_* names), the rest the
 * image in it (Scene_DrawImage) */
#define IMG_GROUP_MASK  0x7000
#define IMG_SCENE       0x0000   /* the scene's tile sheet (g_sceneSheet) */
#define IMG_BOARD       0x1000   /* sprite group 3 */
#define IMG_BOARD30     0x2000   /* the same, from its image 30 on */
#define IMG_FACE        0x3000   /* the portrait sheet (g_portraitSheet) */

/* The view (g_sceneView, 0x80115A18): the screen rectangle the layers and sprites are drawn
 * in, with an outline, opened and closed by opcodes 0x61-0x64. Coordinates are 20.12. */
typedef struct SceneView
{
    unsigned char visible;       /* 0x00 */
    unsigned char transition;    /* 0x01 VIEW_NO_TRANSITION, VIEW_OPENING, VIEW_CLOSING */
    unsigned char style;         /* 0x02 VIEW_AT_ONCE, VIEW_GROW, VIEW_SLIDE */
    unsigned char pad03;
    short step;                  /* 0x04 VIEW_STEP_START, VIEW_STEP_RUNNING */
    short frame;                 /* 0x06 */
    short duration;              /* 0x08 frames */
    short pad0A;
    int x, y, w, h;              /* 0x0C current rectangle */
    int targetX, targetY;        /* 0x1C rectangle once open */
    int targetW, targetH;        /* 0x24 */
    int pad2C;
} SceneView;                     /* size 0x30 */

/* Layer flags */
#define LAYER_TILED      0x0001  /* repeat the image over the view */
#define LAYER_LINES      0x0002  /* draw line by line, each band shifted by lineX[] */
#define LAYER_SEMITRANS  0x0004

/* bits 12-15: band height - 1 (lines per lineX entry) */

/* A background layer (g_sceneLayers[4], 0x80115A48), opcodes 0x70-0x76. */
typedef struct SceneLayer
{
    unsigned char active;        /* 0x000 */
    unsigned char scrolling;     /* 0x001 x += vx, y += vy every frame */
    signed char pri;             /* 0x002 ordering table slot */
    unsigned char pad003;
    short image;                 /* 0x004 entry of g_sceneImages */
    unsigned short flags;        /* 0x006 LAYER_* */
    unsigned short mode;         /* 0x008 copied from the image, not used */
    unsigned short pad00A;
    short x, y;                  /* 0x00C position in the view */
    short w, h;                  /* 0x010 image size (the tiling step) */
    short vx, vy;                /* 0x014 scroll speed */
    int pad018;
    unsigned char waveMode;      /* 0x01C 0 fixed lineX, 1 lineX += lineStep every frame, 2 sine wave */
    signed char band;            /* 0x01D lines per lineX entry */
    short waveLength;            /* 0x01E sine wave (see Wave_FillShort) */
    short waveAmp;               /* 0x020 */
    short wavePhase;             /* 0x022 */
    short wavePhaseStep;         /* 0x024 added to wavePhase every frame */
    short lineX[256];            /* 0x026 x offset of each band */
    short lineStep[256];         /* 0x226 */
    short pad426;
} SceneLayer;                    /* size 0x428 */

/* Sprite states */
#define SPRITE_STILL     0x00    /* not animated, not moving */
#define SPRITE_ANIM      0x01    /* animPos: frames of 4 words (image, frames, flip bits, next) */
#define SPRITE_MOVE      0x02    /* moving to targetX, targetY */
#define SPRITE_PATH      0x10    /* pathPos: steps of 5 words (speed, x, y, animation, next) */

/* A script sprite (g_sceneSprites[64], 0x80116AE8), opcodes 0x80-0x96. Drawn with
 * Scene_DrawImage; x, y and the scales are 20.12. */
typedef struct SceneSprite
{
    unsigned char active;        /* 0x00 */
    signed char pri;             /* 0x01 ordering table slot */
    unsigned char state;         /* 0x02 SPRITE_* */
    unsigned char pad03;
    short image;                 /* 0x04 bits 12-14 pick the image group (see Scene_DrawImage) */
    unsigned short attr;         /* 0x06 bits 4-7 alignment, 0x100 hidden while the view is open... */
    short w, h;                  /* 0x08 image size in pixels */
    short speed;                 /* 0x0C movement per frame; 0 = wait targetX frames */
    short targetX, targetY;      /* 0x0E */
    short pad12;
    int x, y;                    /* 0x14 */
    int scaleX, scaleY;          /* 0x1C 0x1000 = 1 */
    short angle;                 /* 0x24 0x1000 = 360 degrees, 15 bits */
    short animPos;               /* 0x26 offset of the current animation frame in the script data */
    short animTime;              /* 0x28 frames shown so far */
    short pathPos;               /* 0x2A offset of the current path step */
    short pathTime;              /* 0x2C */
    short pad2E;
} SceneSprite;                   /* size 0x30 */

/* A rectangle set by opcodes 0xA0-0xA2 (g_sceneRects[16], 0x801176E8). Nothing in the
 * executable reads them: they have no effect. */
typedef struct SceneRect
{
    unsigned char active;        /* 0x00 */
    unsigned char value;         /* 0x01 */
    short x0, y0, x1, y1;        /* 0x02 inclusive */
} SceneRect;                     /* size 0x0A */

#define TALK_ROWS  3
#define TALK_COLS  38

/* TalkPanel.kind of a cell */
#define TALK_NONE   0    /* empty, or the full-width space */
#define TALK_FULL   1    /* a full-width (two-byte) character */
#define TALK_HALF   2    /* a half-width character */
/* The kind of the character Talk_PutChar put last */
#define TALK_LAST_KIND  (g_talk.kind[0][g_talk.row * TALK_COLS + g_talk.col - 1])

/* TalkPanel.state */
#define TALK_HIDDEN       0
#define TALK_SHOWN        1
#define TALK_SLIDING_IN   2
#define TALK_SLIDING_OUT  3

/* The talk panel at the bottom of the screen (g_talk, 0x80117788), opcodes 0xC0-0xCF. */
typedef struct TalkPanel
{
    unsigned char state;         /* 0x000 TALK_* */
    unsigned char hasText;       /* 0x001 */
    signed char cursorRow;       /* 0x002 highlighted row, -1 none */
    signed char speaker;         /* 0x003 0 = the host, else the current player */
    signed char frame;           /* 0x004 frame style for Frame_DrawTalk (6 = host, else a player) */
    unsigned char pad005;
    short x, y, w, h;            /* 0x006 */
    short row, col;              /* 0x00E where the next character goes */
    unsigned short chars[3][38]; /* 0x012 Shift-JIS (a half-width character in the low byte) */
    unsigned char kind[3][38];   /* 0x0F6 TALK_* */
    unsigned char pad16A[6];
} TalkPanel;                     /* size 0x170 */

/* The speaking character's portrait (g_portrait, 0x801178F8), opcodes 0x100-0x102. */
typedef struct Portrait
{
    unsigned char shown;         /* 0x00 */
    unsigned char talking;       /* 0x01 mouth animation on */
    short mouth;                 /* 0x02 0-15; frame = mouth / 8 */
    int x, y;                    /* 0x04 20.12 */
    int unused0C, unused10;      /* 0x0C set to 0x70000, 0x60000; not read */
} Portrait;                      /* size 0x14 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern RECT g_portraitClut;             /* 0x800B2388 */
extern char g_strSpace[4];              /* 0x800B2390 */
extern short g_sceneDim;                /* 0x800B274C */
extern short g_portraitSquash;          /* 0x800B2750 */
extern short g_portraitSquashH;         /* 0x800B2752 */
extern u16 g_portraitSquashSteps;       /* 0x800B2754 */
extern VramImage *g_sceneImages;        /* 0x800B2760 */
extern SceneView g_sceneView;           /* 0x80115A18 */
extern SceneLayer g_sceneLayers[4];     /* 0x80115A48 */
extern SceneSprite g_sceneSprites[64];  /* 0x80116AE8 */
extern SceneRect g_sceneRects[16];      /* 0x801176E8 */
extern TalkPanel g_talk;                /* 0x80117788 */
extern Portrait g_portrait;             /* 0x801178F8 */
extern GsOT g_sceneOt[2];               /* 0x80117B20 */
extern short g_talkGlyphs[3][38];       /* 0x80117B48 glyph cache cell of each talk panel character */
extern TileSheet g_portraitSheet;       /* 0x80117C78 */

/* ---- Functions ---- */

/* src/scene.c */
void Scene_Update(void);                                  /* 0x8007CA48 */
void Scene_DrawLayers(void);                              /* 0x8007DCC4 */
void SceneSprite_Update(SceneSprite *s);                  /* 0x8007EAFC */
void Scene_DrawSheetImage(TileSheet *sheet, u16 index, u32 attr, short x, short y, u16 scaleX, u16 scaleY, u16 angle, short pri); /* 0x8007EE2C */
void Scene_DrawGroupImage(u32 sprite, u32 attr, short x, short y, u16 scaleX, u16 scaleY, short angle, short pri); /* 0x8007F13C */
void Talk_FreeGlyphs(void);                               /* C-only helper, no original address */
void Talk_Clear(void);                                    /* 0x8007F2F4 */
void Talk_PutString(char *s);                             /* C-only helper, no original address */
int Talk_Print(u8 *text, int inlineText);                 /* C-only helper, no original address */
void Talk_PutChar(u8 *ch);                                /* 0x80081450 */
void Portrait_LoadFace(short player, short face, short vramX); /* C-only helper, no original address */
void Portrait_Load(short player, int face);               /* 0x800839E8 */
void Scene_DrawImage(u32 image, short attr, short x, short y, u32 scaleX, u32 scaleY, short angle, short pri); /* 0x80085A50 */

#endif /* SCENE_H */
