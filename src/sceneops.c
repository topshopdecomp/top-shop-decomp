/*
 * sceneops.c - the script opcodes that drive the scene
 *
 * The handlers of the event script interpreter's (src/script.c) opcodes 0x60-0x102: the view,
 * the background layers, the sprites (images, positions, scales, angles, animations, paths), the
 * rectangles, the talk panel's text and the portrait. scripts/README.md lists every opcode.
 */
#include "common.h"
#include "topshop.h"

/* Opcode 0x61: the view rectangle (x, y, w, h; x or y -1 = centered), closed (0x8007FA6C) */
/* 0x8007FA6C */
u32 Script_OpSetView(void)
{
    short x;
    short y;
    short w;
    short h;

    SCRIPT_READ_OPS(4);
    x = SCRIPT_ARG(1);
    y = SCRIPT_ARG(2);
    w = SCRIPT_ARG(3);
    h = SCRIPT_ARG(4);
    g_sceneView.visible = 0;
    g_sceneView.targetX = ((x == -1) ? (320 - w) / 2 : x) * 0x1000;
    g_sceneView.targetY = ((y == -1) ? (240 - h) / 2 : y) * 0x1000;
    g_sceneView.w = w * 0x1000;
    g_sceneView.h = h * 0x1000;
    g_sceneView.x = g_sceneView.targetX;
    g_sceneView.y = g_sceneView.targetY;
    g_sceneView.targetW = g_sceneView.w;
    g_sceneView.targetH = g_sceneView.h;
    Script_Advance(10);
    return 1;
}

/* Opcode 0x70: layer n shows image with priority and flags, at 0, 0, without line offsets
 * (0x8007FBC0) */
/* 0x8007FBC0 */
u32 Script_OpLayerInit(void)
{
    SceneLayer *layer;
    VramImage *img;
    short image;
    short pri;
    u16 flags;
    int i;

    SCRIPT_READ_OPS(4);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    image = SCRIPT_ARG(2);
    pri = SCRIPT_ARG(3);
    flags = SCRIPT_ARG(4);
    layer->image = image;
    layer->flags = flags;
    layer->active = 1;
    layer->scrolling = 0;
    layer->pri = pri + 32;
    layer->x = 0;
    layer->y = 0;
    img = &g_sceneImages[layer->image];
    layer->w = img->w;
    layer->h = img->h;
    layer->band = (layer->flags >> 12) + 1;
    layer->mode = img->mode;
    for (i = 0; i < 256; i++)
    {
        layer->lineX[i] = 0;
        layer->lineStep[i] = 0;
    }
    Script_Advance(10);
    return 1;
}

/* Opcode 0x72: layer n scrolls by vx, vy every frame (0x8007FD00) */
/* 0x8007FD00 */
u32 Script_OpLayerScroll(void)
{
    SceneLayer *layer;

    SCRIPT_READ_OPS(3);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    layer->scrolling = 1;
    layer->vx = SCRIPT_ARG(2);
    layer->vy = SCRIPT_ARG(3);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x73: lineX[first .. first + count] of layer n = value (and lineStep = 0); up to
 * index 256, one past the end (lineStep[0]) (0x8007FDA8) */
/* 0x8007FDA8 */
u32 Script_OpLayerSetLines(void)
{
    SceneLayer *layer;
    short value;
    int first;
    int last;
    int i;

    SCRIPT_READ_OPS(4);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    first = SCRIPT_ARG(2);
    if (first < 0)
    {
        first = 0;
    }
    last = first + SCRIPT_ARG(3);
    value = SCRIPT_ARG(4);
    for (i = first; i <= last && i <= 256; i++)
    {
        layer->lineX[i] = value;
        layer->lineStep[i] = 0;
    }
    Script_Advance(10);
    return 1;
}

/* Opcode 0x74: lineStep[first .. first + count] of layer n = value (up to index 256); lineX
 * moves by it every frame (0x8007FEB0) */
/* 0x8007FEB0 */
u32 Script_OpLayerSetLineSteps(void)
{
    SceneLayer *layer;
    short value;
    int first;
    int last;
    int i;

    SCRIPT_READ_OPS(4);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    first = SCRIPT_ARG(2);
    value = SCRIPT_ARG(4);
    layer->waveMode = 1;
    if (first < 0)
    {
        first = 0;
    }
    last = first + SCRIPT_ARG(3);
    for (i = first; i <= last && i <= 256; i++)
    {
        layer->lineStep[i] = value;
    }
    Script_Advance(10);
    return 1;
}

/* Opcode 0x75: lineX of layer n = a sine wave (length, amplitude, phase) (0x8007FFB8) */
/* 0x8007FFB8 */
u32 Script_OpLayerWave(void)
{
    SceneLayer *layer;
    short length;
    short amp;
    short phase;

    SCRIPT_READ_OPS(4);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    length = SCRIPT_ARG(2);
    amp = SCRIPT_ARG(3);
    phase = SCRIPT_ARG(4);
    layer->waveAmp = amp;
    layer->wavePhase = phase;
    layer->waveLength = length;
    layer->waveMode = 0;
    Wave_FillShort(layer->lineX,layer->band,0x100,length,amp,phase,0);
    Script_Advance(10);
    return 1;
}

/* Opcode 0x80: sprite n shows image (of the scene tile sheet) with attr at x, y with priority
 * (+32 with attr 0x100); scale 1, angle 0 (0x800800AC) */
/* 0x800800AC */
u32 Script_OpSpriteInit(void)
{
    SceneSprite *sprite;
    short image;
    u16 attr;
    short x;
    short y;
    char pri;

    SCRIPT_READ_OPS(6);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    image = SCRIPT_ARG(2);
    attr = SCRIPT_ARG(3);
    x = SCRIPT_ARG(4);
    y = SCRIPT_ARG(5);
    pri = SCRIPT_ARG(6);
    sprite->image = image;
    sprite->active = 1;
    sprite->attr = attr;
    sprite->w = g_sceneSheet.frames[image].w * 16;
    sprite->h = g_sceneSheet.frames[image].h * 16;
    if (attr & 0x100)
    {
        pri += 32;
    }
    sprite->pri = pri;
    sprite->x = x * 0x1000;
    sprite->y = y * 0x1000;
    sprite->state = SPRITE_STILL;
    sprite->scaleX = 0x1000;
    sprite->scaleY = 0x1000;
    sprite->angle = 0;
    Script_Advance(14);
    return 1;
}

/* Opcode 0x84: move sprite n by dx, dy (0x80080200) */
/* 0x80080200 */
u32 Script_OpSpriteMoveBy(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->x += SCRIPT_ARG(2) * 0x1000;
    sprite->y += SCRIPT_ARG(3) * 0x1000;
    Script_Advance(8);
    return 1;
}

/* Opcode 0x86: sprite n moves to x, y by speed per frame (speed 0: waits x frames; < 0: jumps)
 * (0x800802B8) */
/* 0x800802B8 */
u32 Script_OpSpriteMoveTo(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(4);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->targetX = SCRIPT_ARG(2);
    sprite->targetY = SCRIPT_ARG(3);
    sprite->speed = SCRIPT_ARG(4);
    sprite->state |= SPRITE_MOVE;
    Script_Advance(10);
    return 1;
}

/* Opcode 0x87: add to the scales of sprite n (1/256 units) (0x80080374) */
/* 0x80080374 */
u32 Script_OpSpriteScaleBy(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->scaleX += SCRIPT_ARG(2) * 16;
    sprite->scaleY += SCRIPT_ARG(3) * 16;
    Script_Advance(8);
    return 1;
}

/* Opcode 0x8D: add to the angle of sprite n (0x8008042C) */
/* 0x8008042C */
u32 Script_OpSpriteRotate(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(2);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->angle = (sprite->angle + (SCRIPT_ARG(2) & 0x7fff)) & 0x7fff;
    Script_Advance(6);
    return 1;
}

/* Opcode 0x90: sprite n plays the animation at an offset in the script data (0x800804BC) */
/* 0x800804BC */
u32 Script_OpSpriteAnimate(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(2);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->animTime = 0;
    sprite->animPos = SCRIPT_ARG(2);
    sprite->state |= SPRITE_ANIM;
    Script_Advance(6);
    return 1;
}

/* Opcode 0x91: sprite n follows the path at an offset in the script data, starting with its
 * first step (speed, x, y, animation, next) (0x8008054C) */
/* 0x8008054C */
u32 Script_OpSpritePath(void)
{
    SceneSprite *sprite;
    short offset;
    short step[5];

    SCRIPT_READ_OPS(2);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    offset = SCRIPT_ARG(2);
    sprite->pathPos = offset;
    sprite->state |= SPRITE_PATH;
    sprite->pathTime = 0;
    Mem_CopySwap16(step,g_scriptData + offset,10);
    sprite->state |= SPRITE_MOVE;
    sprite->targetX = step[1];
    sprite->targetY = step[2];
    sprite->speed = step[0];
    if (step[3] != 0)
    {
        sprite->animTime = 0;
        sprite->state |= SPRITE_ANIM;
        sprite->animPos = step[3];
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0x93: vars = position of sprite n (0x80080658) */
/* 0x80080658 */
u32 Script_OpSpriteGetPos(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    SCRIPT_VAR_SET(g_scriptOps[2],sprite->x / 0x1000);
    SCRIPT_VAR_SET(g_scriptOps[3],sprite->y / 0x1000);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x94: vars = scales of sprite n (1/256 units) (0x8008075C) */
/* 0x8008075C */
u32 Script_OpSpriteGetScale(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    SCRIPT_VAR_SET(g_scriptOps[2],sprite->scaleX / 16);
    SCRIPT_VAR_SET(g_scriptOps[3],sprite->scaleY / 16);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x96: vars = width and height of scene image n. An entry whose last word is 0 is a
 * TileFrame: its size in 16-pixel cells (0x80080860) */
/* 0x80080860 */
u32 Script_OpGetImageSize(void)
{
    VramImage *img;
    TileFrame *frame;
    short w;
    short h;

    SCRIPT_READ_OPS(3);
    img = &g_sceneImages[SCRIPT_ARG(1)];
    if (img->h == 0)
    {
        frame = (TileFrame *)img;
        w = frame->w * 16;
        h = frame->h * 16;
    }
    else
    {
        w = img->w;
        h = img->h;
    }
    SCRIPT_VAR_SET(g_scriptOps[2],w);
    SCRIPT_VAR_SET(g_scriptOps[3],h);
    Script_Advance(8);
    return 1;
}

/* Opcode 0xA0: rectangle n = x, y, w, h, value (g_sceneRects, not read anywhere) (0x80080998) */
/* 0x80080998 */
u32 Script_OpSetRect(void)
{
    SceneRect *rect;
    short x;
    short y;

    SCRIPT_READ_OPS(6);
    rect = &g_sceneRects[SCRIPT_ARG(1)];
    x = SCRIPT_ARG(2);
    y = SCRIPT_ARG(3);
    rect->x0 = x;
    rect->y0 = y;
    rect->active = 1;
    rect->x1 = x + SCRIPT_ARG(4) - 1;
    rect->y1 = y + SCRIPT_ARG(5) - 1;
    rect->value = SCRIPT_ARG(6);
    Script_Advance(14);
    return 1;
}

/* Opcode 0xA2: rectangle n = the size of the view, value (0x80080A90) */
/* 0x80080A90 */
u32 Script_OpSetRectToView(void)
{
    SceneRect *rect;

    SCRIPT_READ_OPS(2);
    rect = &g_sceneRects[SCRIPT_ARG(1)];
    rect->active = 1;
    rect->x0 = 0;
    rect->y0 = 0;
    rect->x1 = (g_sceneView.w / 0x1000) - 1;
    rect->y1 = (g_sceneView.h / 0x1000) - 1;
    rect->value = SCRIPT_ARG(2);
    Script_Advance(6);
    return 1;
}

/* Opcode 0xCA: print the text inline in the script in the talk panel (codes: Talk_Print)
 * (0x80080C90) */
/* 0x80080C90 */
u32 Script_OpTalkPrint(void)
{
    Script_Advance(Talk_Print(g_scriptPc + 2,1) + 3);
    return 1;
}

/* Opcode 0xCF: as 0xCA with the text at an offset in the script data (0x80080F2C) */
/* 0x80080F2C */
u32 Script_OpTalkPrintData(void)
{
    Talk_Print(g_scriptData + SCRIPT_ARG0,0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0xCC: print the name of player n (0x800811D8) */
/* 0x800811D8 */
u32 Script_OpTalkPrintPlayer(void)
{
    Talk_PutString(g_players[SCRIPT_ARG0].name);
    Script_Advance(4);
    return 1;
}

/* Opcode 0xCD: print the name of the type of store n (nothing for a vacant lot) (0x80081300) */
/* 0x80081300 */
u32 Script_OpTalkPrintStoreType(void)
{
    Store *st = &g_stores[SCRIPT_ARG0];

    if (st->owner != -1)
    {
        Talk_PutString(st->type->name);
    }
    Script_Advance(4);
    return 1;
}

/* Script_OpPortraitShow's steps (g_scriptWait) */
enum PortraitShowWait
{
    PSHOW_LOAD     = 0,  /* load the portrait with the face, place it */
    PSHOW_UNSQUASH = 1,  /* unsquash it over 8 frames */
    PSHOW_DONE     = 2   /* talking; the next opcode */
};

/* Opcode 0x100: (unused operand), face, position (0 middle, 1 left, 2 right): show the current
 * player's portrait with that face, unsquashing it over 8 frames (0x80081780) */
/* 0x80081780 */
u32 Script_OpPortraitShow(ScriptScratch *scratch)
{
    short face;
    short position;

    SCRIPT_READ_OPS(3);
    Script_Value(g_scriptOps[1]);
    face = SCRIPT_ARG(2);
    position = SCRIPT_ARG(3);
    switch (g_scriptWait)
    {
    case PSHOW_LOAD:
        g_portrait.talking = 0;
        g_portrait.mouth = 0;
        Portrait_Load(g_curPlayer,face);
        if (position == 1)
        {
            g_portrait.x = 0x30000;
        }
        else if (position == 0)
        {
            g_portrait.x = 0x60000;
        }
        else if (position == 2)
        {
            g_portrait.x = 0x90000;
        }
        g_portrait.y = 0x4a000;
        g_portrait.unused0C = 0x70000;
        g_portrait.unused10 = 0x60000;
        scratch->show.squashing = 0;
        g_scriptWait++;
        break;
    case PSHOW_UNSQUASH:
        g_portrait.shown = 1;
        if (scratch->show.squashing == 0)
        {
            g_portraitSquash = 1;
            g_portraitSquashSteps = 8;
            g_portraitSquashH = 0;
            scratch->show.squashing++;
        }
        else if (scratch->show.squashing == 1 && ++g_portraitSquashH == 8)
        {
            g_portraitSquash = 0;
            g_scriptWait = PSHOW_DONE;
        }
        break;
    case PSHOW_DONE:
        g_portrait.talking = 1;
        Script_Advance(8);
        break;
    }
    return 0;
}

/* Script_OpPortraitHide's steps (g_scriptWait) */
enum PortraitHideWait
{
    PHIDE_START  = 0,  /* stop talking */
    PHIDE_SQUASH = 1,  /* squash it away over 8 frames */
    PHIDE_DONE   = 2   /* hidden; the next opcode */
};

/* Opcode 0x101: squash the portrait away (the operand is not used) (0x8008197C) */
/* 0x8008197C */
u32 Script_OpPortraitHide(ScriptScratch *scratch)
{
    Script_Value(SCRIPT_WORD(2));
    switch (g_scriptWait)
    {
    case PHIDE_START:
        g_portrait.talking = 0;
        scratch->hide.squashing = 0;
        g_scriptWait = PHIDE_SQUASH;
        break;
    case PHIDE_SQUASH:
        if (scratch->hide.squashing == 0)
        {
            g_portraitSquashSteps = 8;
            g_portraitSquash = 1;
            g_portraitSquashH = 7;
            scratch->hide.squashing++;
        }
        else if (scratch->hide.squashing == 1 && --g_portraitSquashH < 0)
        {
            g_portrait.shown = 0;
            g_scriptWait = PHIDE_DONE;
        }
        break;
    case PHIDE_DONE:
        g_portrait.talking = 0;
        g_portrait.shown = 0;
        Script_Advance(4);
        break;
    }
    return 0;
}

/* Opcode 0x102: change the face of the portrait (0x80081A98) */
/* 0x80081A98 */
u32 Script_OpPortraitFace(void)
{
    short face = SCRIPT_ARG0;

    if (g_scriptWait == 0)
    {
        Portrait_LoadFace(g_curPlayer,face,0x300);
        g_scriptWait++;
    }
    else if (g_scriptWait == 1)
    {
        g_portrait.talking = 1;
        Script_Advance(4);
    }
    return 0;
}

/* Opcode 0x60: text inline in the script (NUL-terminated), skipped; its length is taken from a
 * 16-byte copy in g_scriptOps (longer texts run on into the memory after it) (0x80084B5C) */
/* 0x80084B5C */
u32 Script_OpText(void)
{
    Mem_Copy(g_scriptOps,g_scriptPc,0x10);
    if (g_scriptWait == 0)
    {
        Script_Advance(strlen((char *)(g_scriptOps + 1)) + 3);
    }
    return 0;
}

/* Opcode 0x62: open the view (style 0 at once, 1 the height grows, 2 slides in) over n frames
 * (0x80084BB8) */
/* 0x80084BB8 */
u32 Script_OpViewOpen(void)
{
    short style;
    short frames;

    SCRIPT_READ_OPS(2);
    style = SCRIPT_ARG(1);
    frames = SCRIPT_ARG(2);
    g_sceneView.visible = 1;
    g_sceneView.transition = VIEW_OPENING;
    g_sceneView.style = style;
    g_sceneView.step = VIEW_STEP_START;
    g_sceneView.frame = 0;
    g_sceneView.duration = frames;
    Script_Advance(6);
    return 1;
}

/* Opcode 0x63: close the view (styles as 0x62) over n frames (0x80084C38) */
/* 0x80084C38 */
u32 Script_OpViewClose(void)
{
    short style;
    short frames;

    SCRIPT_READ_OPS(2);
    style = SCRIPT_ARG(1);
    frames = SCRIPT_ARG(2);
    g_sceneView.visible = 1;
    g_sceneView.transition = VIEW_CLOSING;
    g_sceneView.style = style;
    g_sceneView.step = VIEW_STEP_START;
    g_sceneView.frame = 0;
    g_sceneView.duration = frames;
    Script_Advance(6);
    return 1;
}

/* Opcode 0x64: wait for the end of the view transition (0x80084CBC) */
/* 0x80084CBC */
u32 Script_OpViewWait(void)
{
    if (g_sceneView.transition == VIEW_NO_TRANSITION)
    {
        Script_Advance(2);
    }
    return 0;
}

/* Opcode 0x71: position of layer n (0x80084CE8) */
/* 0x80084CE8 */
u32 Script_OpLayerSetPos(void)
{
    SceneLayer *layer;

    SCRIPT_READ_OPS(3);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    layer->x = SCRIPT_ARG(2);
    layer->y = SCRIPT_ARG(3);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x76: the sine wave of layer n moves by step every frame (0x80084D88) */
/* 0x80084D88 */
u32 Script_OpLayerWaveScroll(void)
{
    SceneLayer *layer;

    SCRIPT_READ_OPS(2);
    layer = &g_sceneLayers[SCRIPT_ARG(1)];
    layer->waveMode = 2;
    layer->wavePhaseStep = SCRIPT_ARG(2);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x81: remove sprite n (0x80084E18) */
/* 0x80084E18 */
u32 Script_OpSpriteHide(void)
{
    g_sceneSprites[SCRIPT_ARG0].active = 0;
    Script_Advance(4);
    return 1;
}

/* Opcode 0x82: image and attr of sprite n (stops its animation and movement) (0x80084E7C) */
/* 0x80084E7C */
u32 Script_OpSpriteSetImage(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->image = SCRIPT_ARG(2);
    sprite->attr = SCRIPT_ARG(3);
    sprite->state = SPRITE_STILL;
    Script_Advance(8);
    return 1;
}

/* Opcode 0x83: attr of sprite n (0x80084F18) */
/* 0x80084F18 */
u32 Script_OpSpriteSetAttr(void)
{
    SCRIPT_READ_OPS(2);
    g_sceneSprites[SCRIPT_ARG(1)].attr = SCRIPT_ARG(2);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x85: position of sprite n (in pixels; kept as 20.12) (0x80084F98) */
/* 0x80084F98 */
u32 Script_OpSpriteSetPos(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->x = SCRIPT_ARG(2) * 0x1000;
    sprite->y = SCRIPT_ARG(3) * 0x1000;
    Script_Advance(8);
    return 1;
}

/* Opcode 0x88: scales of sprite n (1/256 units; kept as 1/4096) (0x80085040) */
/* 0x80085040 */
u32 Script_OpSpriteSetScale(void)
{
    SceneSprite *sprite;

    SCRIPT_READ_OPS(3);
    sprite = &g_sceneSprites[SCRIPT_ARG(1)];
    sprite->scaleX = SCRIPT_ARG(2) * 16;
    sprite->scaleY = SCRIPT_ARG(3) * 16;
    Script_Advance(8);
    return 1;
}

/* Opcode 0x89: three operands, no effect (0x800850E8) */
/* 0x800850E8 */
u32 Script_OpNop89(void)
{
    Script_Advance(8);
    return 1;
}

/* Opcode 0x8E: angle of sprite n (0x80085108) */
/* 0x80085108 */
u32 Script_OpSpriteSetAngle(void)
{
    SCRIPT_READ_OPS(2);
    g_sceneSprites[SCRIPT_ARG(1)].angle = SCRIPT_ARG(2) & 0x7fff;
    Script_Advance(6);
    return 1;
}

/* Opcode 0x8F: two operands, no effect (0x8008518C) */
/* 0x8008518C */
u32 Script_OpNop8F(void)
{
    Script_Advance(6);
    return 1;
}

/* Opcode 0x92: var = state of sprite n (SPRITE_*: still animating / moving) (0x800851AC) */
/* 0x800851AC */
u32 Script_OpSpriteGetState(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = (char)g_sceneSprites[SCRIPT_ARG(1)].state;
    SCRIPT_VAR_SET(g_scriptOps[2],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x95: var = angle of sprite n (0x80085258) */
/* 0x80085258 */
u32 Script_OpSpriteGetAngle(void)
{
    short sprite;

    SCRIPT_READ_OPS(2);
    sprite = SCRIPT_ARG(1);
    SCRIPT_VAR_SET(g_scriptOps[2],g_sceneSprites[sprite].angle);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x97: how much sprites with attr 0x200 are darkened (g_sceneDim) (0x800852F4) */
/* 0x800852F4 */
u32 Script_OpSetDim(void)
{
    g_sceneDim = SCRIPT_ARG0;
    Script_Advance(4);
    return 1;
}

/* Opcode 0xA1: rectangle n off (0x80085338) */
/* 0x80085338 */
u32 Script_OpHideRect(void)
{
    g_sceneRects[SCRIPT_ARG0].active = 0;
    Script_Advance(4);
    return 1;
}

/* Opcode 0xC6: who speaks in the talk panel (0 = the host, else the current player) (0x8008539C) */
/* 0x8008539C */
u32 Script_OpTalkSpeaker(void)
{
    g_talk.speaker = SCRIPT_ARG0;
    Script_Advance(4);
    return 1;
}

/* Opcode 0xC0: slide the talk panel in: wider for the host (frame style 6), else in the current
 * player's style (0x800853E4) */
/* 0x800853E4 */
u32 Script_OpTalkOpen(void)
{
    g_talk.state = TALK_SLIDING_IN;
    g_talk.w = 0x100;
    g_talk.h = 0x38;
    if (g_talk.speaker == 0)
    {
        g_talk.x = 0x10;
        g_talk.w = 0x120;
        g_talk.frame = 6;
    }
    else
    {
        g_talk.x = 0x20;
        g_talk.frame = g_curPlayer;
    }
    Script_Advance(2);
    return 1;
}

/* Opcode 0xC1: slide the talk panel out (0x8008545C) */
/* 0x8008545C */
u32 Script_OpTalkClose(void)
{
    g_talk.state = TALK_SLIDING_OUT;
    Script_Advance(2);
    return 1;
}

/* Opcode 0xC2: wait until the talk panel is in (0x80085488) */
/* 0x80085488 */
u32 Script_OpTalkWaitOpen(void)
{
    if (g_talk.state == TALK_SHOWN)
    {
        Script_Advance(2);
    }
    return 0;
}

/* Opcode 0xC3: wait until the talk panel is out (0x800854B8) */
/* 0x800854B8 */
u32 Script_OpTalkWaitClosed(void)
{
    if (g_talk.state == TALK_HIDDEN)
    {
        Script_Advance(2);
    }
    return 0;
}

/* Opcode 0xC4: highlight row n of the talk panel (0x800854E4) */
/* 0x800854E4 */
u32 Script_OpTalkCursor(void)
{
    g_talk.cursorRow = SCRIPT_ARG0;
    Script_Advance(4);
    return 1;
}

/* Opcode 0xC5: no highlighted row (0x8008552C) */
/* 0x8008552C */
u32 Script_OpTalkNoCursor(void)
{
    g_talk.cursorRow = -1;
    Script_Advance(2);
    return 1;
}

/* Opcode 0xC8: clear the talk panel text (0x80085558) */
/* 0x80085558 */
u32 Script_OpTalkClear(void)
{
    Talk_Clear();
    Script_Advance(2);
    return 1;
}

/* Opcode 0xC9: show the talk panel text (operand 0) (0x80085580) */
/* 0x80085580 */
u32 Script_OpTalkShowText(void)
{
    if (SCRIPT_ARG0 == 0)
    {
        g_talk.hasText = 1;
    }
    Script_Advance(4);
    return 1;
}

/* Opcode 0xCB: write a number (value, width) into the talk panel text (0x800855D4) */
/* 0x800855D4 */
u32 Script_OpTalkNumber(void)
{
    short value;
    short width;
    u8 *ch;

    SCRIPT_READ_OPS(2);
    value = SCRIPT_ARG(1);
    width = SCRIPT_ARG(2);
    Str_FormatInt(value,width,g_msgBuf);
    for (ch = (u8 *)g_msgBuf; *ch != 0; ch++)
    {
        Talk_PutChar(ch);
    }
    Script_Advance(6);
    return 1;
}

/* Opcode 0xCE: var = y position of the talk panel (0x80085684) */
/* 0x80085684 */
u32 Script_OpGetTalkY(void)
{
    SCRIPT_VAR_SET(SCRIPT_WORD(2),g_talk.y);
    Script_Advance(4);
    return 1;
}

/* Opcode 0xD0: no effect (0x800856E8) */
/* 0x800856E8 */
u32 Script_OpNopD0(void)
{
    Script_Advance(2);
    return 1;
}

/* Opcode 0xD1: no effect (0x80085708) */
/* 0x80085708 */
u32 Script_OpNopD1(void)
{
    Script_Advance(2);
    return 1;
}

/* Opcode 0xF1: three operands, no effect (0x80085728) */
/* 0x80085728 */
u32 Script_OpNopF1(void)
{
    Script_Advance(8);
    return 1;
}

/* Opcode 0xF2: three operands, no effect (0x80085748) */
/* 0x80085748 */
u32 Script_OpNopF2(void)
{
    Script_Advance(8);
    return 1;
}
