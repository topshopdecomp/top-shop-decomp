/*
 * sceneops.h - the script opcodes that drive the scene
 *
 * View, layers, sprites, rectangles, the talk panel, the portrait.
 * The declarations of src/sceneops.c: its functions.
 * See docs/script.md.
 */

#ifndef SCENEOPS_H
#define SCENEOPS_H

#include "common.h"
#include "script.h"

/* ---- Functions ---- */

/* src/sceneops.c */
u32 Script_OpSetView(void);                         /* 0x8007FA6C */
u32 Script_OpLayerInit(void);                       /* 0x8007FBC0 */
u32 Script_OpLayerScroll(void);                     /* 0x8007FD00 */
u32 Script_OpLayerSetLines(void);                   /* 0x8007FDA8 */
u32 Script_OpLayerSetLineSteps(void);               /* 0x8007FEB0 */
u32 Script_OpLayerWave(void);                       /* 0x8007FFB8 */
u32 Script_OpSpriteInit(void);                      /* 0x800800AC */
u32 Script_OpSpriteMoveBy(void);                    /* 0x80080200 */
u32 Script_OpSpriteMoveTo(void);                    /* 0x800802B8 */
u32 Script_OpSpriteScaleBy(void);                   /* 0x80080374 */
u32 Script_OpSpriteRotate(void);                    /* 0x8008042C */
u32 Script_OpSpriteAnimate(void);                   /* 0x800804BC */
u32 Script_OpSpritePath(void);                      /* 0x8008054C */
u32 Script_OpSpriteGetPos(void);                    /* 0x80080658 */
u32 Script_OpSpriteGetScale(void);                  /* 0x8008075C */
u32 Script_OpGetImageSize(void);                    /* 0x80080860 */
u32 Script_OpSetRect(void);                         /* 0x80080998 */
u32 Script_OpSetRectToView(void);                   /* 0x80080A90 */
u32 Script_OpTalkPrint(void);                       /* 0x80080C90 */
u32 Script_OpTalkPrintData(void);                   /* 0x80080F2C */
u32 Script_OpTalkPrintPlayer(void);                 /* 0x800811D8 */
u32 Script_OpTalkPrintStoreType(void);              /* 0x80081300 */
u32 Script_OpPortraitShow(ScriptScratch *scratch);  /* 0x80081780 */
u32 Script_OpPortraitHide(ScriptScratch *scratch);  /* 0x8008197C */
u32 Script_OpPortraitFace(void);                    /* 0x80081A98 */
u32 Script_OpText(void);                            /* 0x80084B5C */
u32 Script_OpViewOpen(void);                        /* 0x80084BB8 */
u32 Script_OpViewClose(void);                       /* 0x80084C38 */
u32 Script_OpViewWait(void);                        /* 0x80084CBC */
u32 Script_OpLayerSetPos(void);                     /* 0x80084CE8 */
u32 Script_OpLayerWaveScroll(void);                 /* 0x80084D88 */
u32 Script_OpSpriteHide(void);                      /* 0x80084E18 */
u32 Script_OpSpriteSetImage(void);                  /* 0x80084E7C */
u32 Script_OpSpriteSetAttr(void);                   /* 0x80084F18 */
u32 Script_OpSpriteSetPos(void);                    /* 0x80084F98 */
u32 Script_OpSpriteSetScale(void);                  /* 0x80085040 */
u32 Script_OpNop89(void);                           /* 0x800850E8 */
u32 Script_OpSpriteSetAngle(void);                  /* 0x80085108 */
u32 Script_OpNop8F(void);                           /* 0x8008518C */
u32 Script_OpSpriteGetState(void);                  /* 0x800851AC */
u32 Script_OpSpriteGetAngle(void);                  /* 0x80085258 */
u32 Script_OpSetDim(void);                          /* 0x800852F4 */
u32 Script_OpHideRect(void);                        /* 0x80085338 */
u32 Script_OpTalkSpeaker(void);                     /* 0x8008539C */
u32 Script_OpTalkOpen(void);                        /* 0x800853E4 */
u32 Script_OpTalkClose(void);                       /* 0x8008545C */
u32 Script_OpTalkWaitOpen(void);                    /* 0x80085488 */
u32 Script_OpTalkWaitClosed(void);                  /* 0x800854B8 */
u32 Script_OpTalkCursor(void);                      /* 0x800854E4 */
u32 Script_OpTalkNoCursor(void);                    /* 0x8008552C */
u32 Script_OpTalkClear(void);                       /* 0x80085558 */
u32 Script_OpTalkShowText(void);                    /* 0x80085580 */
u32 Script_OpTalkNumber(void);                      /* 0x800855D4 */
u32 Script_OpGetTalkY(void);                        /* 0x80085684 */
u32 Script_OpNopD0(void);                           /* 0x800856E8 */
u32 Script_OpNopD1(void);                           /* 0x80085708 */
u32 Script_OpNopF1(void);                           /* 0x80085728 */
u32 Script_OpNopF2(void);                           /* 0x80085748 */

#endif /* SCENEOPS_H */
