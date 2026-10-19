/*
 * textwin.h - text windows
 *
 * Menus of texts in a frame, opening and closing, the cursor; the window frames themselves.
 * The declarations of src/textwin.c: its types and constants, its globals and its functions.
 * See docs/text.md.
 */

#ifndef TEXTWIN_H
#define TEXTWIN_H

#include "common.h"
#include "psyq.h"

/* 16 windows (g_textWins). A window holds lines (TextLine chain in g_textLines), each
 * line a text id. TextWin_Draw lays the window out the first time it is drawn: x/y
 * == TEXTWIN_AUTO center it, cols/rows == -1 size it to the text. */
#define TEXT_WINS        16
#define TEXT_LINES       320
#define TEXTWIN_AUTO     (-1000)

/* TextWin.state: open animation, shown, close animation, freed */
#define TEXTWIN_IDLE     0
#define TEXTWIN_OPEN     1   /* TextWin_Open: lay out, then grow the frame */
#define TEXTWIN_GROWING  2
#define TEXTWIN_SHOWN1   3   /* 3, 4: the two frames after opening */
#define TEXTWIN_SHOWN2   4
#define TEXTWIN_SHOWN    5   /* drawn every frame until TextWin_Close */
#define TEXTWIN_SHRINKING 6
#define TEXTWIN_CLOSED   7   /* 7, 8: wait 15 frames, then free the window */
#define TEXTWIN_FREEING  8

typedef struct TextWin
{
    short x;              /* 0x00 frame position (TEXTWIN_AUTO = centered) */
    short y;              /* 0x02 */
    short cols;           /* 0x04 inner width in 7-pixel columns (-1 = fit the text) */
    short rows;           /* 0x06 inner height in 16-pixel rows (-1 = fit the text) */
    short align;          /* 0x08 text state between TextWin_AddText calls: alignment */
    short pal;            /* 0x0A and palette */
    short numLines;       /* 0x0C lines added */
    short firstLine;      /* 0x0E first TextLine, -1 = none */
    short lastLine;       /* 0x10 */
    short anim;           /* 0x12 open/close animation counter */
    short state;          /* 0x14 TEXTWIN_* */
    unsigned char laidOut;    /* 0x16 TextWin_Layout done */
    unsigned char closing;    /* 0x17 set by TextWin_Close */
    signed char depth;        /* 0x18 drawing order (0 = drawn first) */
    unsigned char menuOn;     /* 0x19 show the menu cursor bar */
    signed char menuRow;      /* 0x1A row of the first menu item */
    signed char menuCount;    /* 0x1B number of menu items */
    signed char menuCursor;   /* 0x1C selected item */
    unsigned char pad1D;
    short scroll;         /* 0x1E first visible row * 256 */
    unsigned char rowsPlaced; /* 0x20 TextWin_PlaceRows done (cleared to re-place after scrolling) */
    unsigned char arrowUp;    /* 0x21 show the "more above" arrow */
    unsigned char arrowDown;  /* 0x22 show the "more below" arrow */
    unsigned char pad23;
} TextWin;                /* size 0x24 */

/* One line of a text window. */
typedef struct TextLine
{
    short next;           /* 0x00 next line; self = last; -1 = free */
    short text;           /* 0x02 text id (GlyphNode head) */
    short row;            /* 0x04 row in the window */
} TextLine;               /* size 6 */

/* ---- Globals (src/data.c, src/bss.c) ---- */
extern short g_textFocusWin;       /* 0x800B24C0 */
extern short g_textFocusOn;        /* 0x800B24C2 */
extern short g_textLineNext;       /* 0x800B24C4 */
extern u8 g_textWinUsed[2];        /* 0x800B24C6 */
extern char g_textWinCount;        /* 0x800B24C8 */
extern TextLine g_textLines[320];  /* 0x800CE038 */
extern TextWin g_textWins[16];     /* 0x800CE7B8 */

/* ---- Functions ---- */

/* src/textwin.c */
void TextWin_InitAll(void);                               /* 0x8001F470 */
short TextWin_Create(short x, short y, short cols, short rows); /* 0x8001F558 */
void TextWin_Draw(short id, GsOT *ot, u16 pri);           /* 0x8001F6A0 */
void TextWin_DrawFrame(short *rect, GsOT *ot, short pri); /* 0x8001FC80 */
void TextWin_Layout(short id);                            /* 0x800209A0 */
void TextWin_PlaceRows(short id);                         /* 0x80020C5C */
int TextWin_AddText(short id, char *text);                /* 0x80020D7C */
void TextWin_ClearText(short id);                         /* 0x800210E8 */
void TextWin_AnimOpen(short id);                          /* 0x800211E4 */
void TextWin_AnimClose(short id);                         /* 0x8002140C */
void TextWin_DrawAll(void);                               /* 0x80021634 */
void TextWin_FreeAll(void);                               /* 0x80021998 */
void TextWin_Free(short id);                              /* 0x80021AAC */
void TextWin_Open(short id);                              /* 0x80021B5C */
void TextWin_Close(short id);                             /* 0x80021B9C */
void TextWin_RemoveDepth(char depth);                     /* 0x80021BC8 */
void TextWin_SetMenu(short id, char row, char count, char cursor); /* 0x80021C44 */
int TextWin_GetCursor(short id);                          /* 0x80021CE0 */
void TextWin_CursorUp(short id);                          /* 0x80021D1C */
void TextWin_CursorDown(short id);                        /* 0x80021D64 */
void TextWin_SetFocus(short id, u16 on);                  /* 0x80021DA8 */
void TextWin_SetArrowUp(short id, u8 on);                 /* 0x80021DB8 */
void TextWin_SetArrowDown(short id, u8 on);               /* 0x80021DE0 */
void Frame_DrawPanel(short *rect, short palette, GsOT *ot, short pri); /* 0x8002CD60 */
void Frame_DrawTalk(short *rect, short palette, GsOT *ot, short pri); /* 0x8002E3EC */
void Frame_DrawPlain(short *rect, GsOT *ot, short pri);   /* 0x8002F19C */

#endif /* TEXTWIN_H */
