/*
 * scene.c - the scene the scripts draw
 *
 * The state the event scripts drive and its drawing every frame: the view rectangle with its
 * outline and its transitions, the four background layers, the 64 sprites with their animations
 * and paths, the talk panel (three lines of text) and the speaking character's portrait; and the
 * script variables the interpreter refreshes every frame (the players, the buttons, the
 * calendar).
 */
#include "common.h"
#include "topshop.h"

/* The script variables for the pads, players and game (SVAR_*) */
static void Scene_UpdateVars(void)
{
    u16 held;
    u16 pressed;
    int i;

    g_scriptVars[SVAR_FRAME] = (g_scriptVars[SVAR_FRAME] + 1) & 0x7fff;
    g_scriptVars[SVAR_ACTIVE_PLAYERS] = g_numActivePlayers;
    g_scriptVars[SVAR_NUM_PLAYERS] = g_numPlayers;
    g_scriptVars[SVAR_CUR_PLAYER] = g_curPlayer;
    g_scriptVars[SVAR_PAD_HELD] = Script_Buttons(g_playerPads[6].held);
    g_scriptVars[SVAR_PAD_PRESSED] = Script_Buttons(g_playerPads[6].pressed);
    g_scriptVars[SVAR_ANY_HELD] = g_scriptVars[SVAR_PAD_HELD];
    g_scriptVars[SVAR_ANY_PRESSED] = g_scriptVars[SVAR_PAD_PRESSED];
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].isCpu == 0)
        {
            held = Script_Buttons(g_playerPads[i].held);
            pressed = Script_Buttons(g_playerPads[i].pressed);
            g_scriptVars[SVAR_HELD + i] = held;
            g_scriptVars[SVAR_PRESSED + i] = pressed;
            g_scriptVars[SVAR_ANY_HELD] |= held;
            g_scriptVars[SVAR_ANY_PRESSED] |= pressed;
        }
        else
        {
            g_scriptVars[SVAR_HELD + i] = 0;
            g_scriptVars[SVAR_PRESSED + i] = 0;
        }
        g_scriptVars[SVAR_IS_CPU + i] = (g_players[i].isCpu != 0) ? 0x10 : 0;
    }
    g_scriptVars[SVAR_CUR_HELD] = g_scriptVars[SVAR_HELD + g_curPlayer];
    g_scriptVars[SVAR_CUR_PRESSED] = g_scriptVars[SVAR_PRESSED + g_curPlayer];
    g_scriptVars[SVAR_CUR_IS_CPU] = g_scriptVars[SVAR_IS_CPU + g_curPlayer];
    g_scriptVars[SVAR_ENTRY] = g_scriptEntry;
    g_scriptVars[SVAR_CHAR] = (char)g_players[g_curPlayer].charId;
    g_scriptVars[SVAR_MONTHS] = g_gameConfig.monthsElapsed;
    g_scriptVars[SVAR_START_MONEY] = g_gameConfig.startMoney;
    g_scriptVars[SVAR_GOAL] = g_gameConfig.goalMoney;
    g_scriptVars[SVAR_FLOORS] = g_stageDefs[g_boardStage].floors;
}

/* The view's rectangle once open */
static void SceneView_SetOpen(SceneView *view)
{
    view->x = view->targetX;
    view->y = view->targetY;
    view->w = view->targetW;
    view->h = view->targetH;
}

/* One frame of the view opening (transition 1) or closing (2): style 0 at once, 1 the height
 * grows / shrinks, 2 it slides from / to the left over duration frames. The slide moves from -w
 * to targetX - 2w, then jumps to targetX (as in the original) */
static void SceneView_Transition(SceneView *view)
{
    short frame = view->frame;

    if (view->transition == VIEW_OPENING)
    {
        switch (view->style)
        {
        case VIEW_AT_ONCE:
            if (view->step == VIEW_STEP_START)
            {
                view->visible = 1;
                view->transition = VIEW_NO_TRANSITION;
                SceneView_SetOpen(view);
            }
            break;
        case VIEW_GROW:
            if (view->step == VIEW_STEP_START)
            {
                view->visible = 1;
                view->h = 0;
                frame = 0;
                view->x = view->targetX;
                view->y = view->targetY;
                view->w = view->targetW;
                view->step = VIEW_STEP_RUNNING;
            }
            else if (view->step == VIEW_STEP_RUNNING)
            {
                view->h = view->targetH * frame / view->duration;
                if (frame == view->duration)
                {
                    view->h = view->targetH;
                    view->transition = VIEW_NO_TRANSITION;
                }
                else
                {
                    frame++;
                }
            }
            break;
        case VIEW_SLIDE:
            if (view->step == VIEW_STEP_START)
            {
                view->visible = 1;
                frame = 0;
                SceneView_SetOpen(view);
                view->x = -view->targetW;
                view->step = VIEW_STEP_RUNNING;
            }
            else if (view->step == VIEW_STEP_RUNNING)
            {
                view->x = (view->targetX - view->targetW) * frame / view->duration - view->targetW;
                if (frame == view->duration)
                {
                    view->x = view->targetX;
                    view->transition = VIEW_NO_TRANSITION;
                }
                frame++;
            }
            break;
        }
    }
    else if (view->transition == VIEW_CLOSING)
    {
        if (view->step == VIEW_STEP_START && (view->style == VIEW_GROW || view->style == VIEW_SLIDE))
        {
            view->visible = 1;
            view->step++;
            SceneView_SetOpen(view);
            frame = view->duration;
        }
        else if (view->style == VIEW_AT_ONCE)
        {
            if (view->step == VIEW_STEP_START)
            {
                view->visible = 0;
                view->transition = VIEW_NO_TRANSITION;
            }
        }
        else if (view->style == VIEW_GROW)
        {
            if (view->step == VIEW_STEP_RUNNING)
            {
                view->h = view->targetH * frame / view->duration;
                if (frame == 0)
                {
                    view->h = 0;
                    view->visible = 0;
                    view->transition = VIEW_NO_TRANSITION;
                }
                else
                {
                    frame--;
                }
            }
        }
        else if (view->style == VIEW_SLIDE)
        {
            if (view->step == VIEW_STEP_RUNNING)
            {
                view->x = (view->targetX - view->targetW) * frame / view->duration - view->targetW;
                if (frame == 0)
                {
                    view->visible = 0;
                    view->transition = VIEW_NO_TRANSITION;
                }
                frame--;
            }
        }
    }
    view->frame = frame;
}

/* A tint component (0x80 = normal) scaled to a color, kept in 0-255 */
static u8 Scene_TintColor(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 0xff)
    {
        return 0xff;
    }
    return value;
}

/* One line of the view's outline */
static void SceneView_SortLine(short x0, short y0, short x1, short y1)
{
    g_line.x0 = x0;
    g_line.y0 = y0;
    g_line.x1 = x1;
    g_line.y1 = y1;
    GsSortLine(&g_line,g_sceneOt + g_otIndex,0x1f);
}

/* The portrait: its mouth moves while it talks; drawn squashed while it (un)folds */
static void Portrait_Draw(void)
{
    if (g_portrait.shown == 0)
    {
        return;
    }
    if (g_portrait.talking != 0)
    {
        g_portrait.mouth = (g_portrait.mouth + 1) & 0xf;
    }
    g_drawPos[0] = g_portrait.x / 0x1000;
    g_drawPos[1] = g_portrait.y / 0x1000;
    if (g_portraitSquash == 0)
    {
        TileSheet_DrawAligned(&g_portraitSheet,g_portrait.mouth / 8,0,g_drawPos,ALIGN_TOP | ALIGN_LEFT,
                              g_sceneOt + g_otIndex,0);
    }
    else if (g_portraitSquash >= 0 && g_portraitSquash < 4)
    {
        TileSheet_DrawSquashed(&g_portraitSheet,g_portrait.mouth / 8,0,g_drawPos,ALIGN_TOP | ALIGN_LEFT,
                               g_portraitSquashH,g_sceneOt + g_otIndex,0);
    }
}

/* The talk panel: it slides in / out 16 lines a frame; its text, the highlighted row and its
 * frame */
static void Talk_Draw(void)
{
    u8 kind;
    int row;
    int col;

    if (g_talk.state == TALK_SLIDING_IN)
    {
        g_talk.y -= 0x10;
        if (g_talk.y <= 0xa8)
        {
            g_talk.y = 0xa8;
            g_talk.state = TALK_SHOWN;
        }
    }
    else if (g_talk.state == TALK_SLIDING_OUT)
    {
        g_talk.y += 0x10;
        if (g_talk.y >= 0xf0)
        {
            g_talk.y = 0xf0;
            g_talk.state = TALK_HIDDEN;
        }
    }
    if (g_talk.hasText == 0)
    {
        return;
    }
    g_drawPos[1] = g_talk.y + 4;
    for (row = 0; row < TALK_ROWS; row++)
    {
        g_drawPos[0] = g_talk.x + 8;
        for (col = 0; col < TALK_COLS; col++)
        {
            kind = g_talk.kind[row][col];
            if (kind != TALK_NONE)
            {
                Font_DrawCell(g_talkGlyphs[row][col],g_drawPos[0],g_drawPos[1],0,g_ot + g_otIndex,2);
            }
            if (kind == TALK_NONE || kind == TALK_FULL)
            {
                g_drawPos[0] += 14;
            }
            else if (kind == TALK_HALF)
            {
                g_drawPos[0] += 7;
            }
        }
        g_drawPos[1] += 0x10;
    }
    if (g_talk.state == TALK_SHOWN && g_talk.cursorRow != -1)
    {
        g_drawPos[3] = 0x10;
        g_boxFill.attribute = GFX_SEMITRANS;
        g_boxFill.h = 0x10;
        g_boxFill.x = g_talk.x + 8;
        g_boxFill.w = g_talk.w - 0x10;
        g_boxFill.y = g_talk.y + g_talk.cursorRow * 0x10 + 4;
        g_boxFill.r = Scene_TintColor((g_tintR - 0x80) * 2 + 0x80);
        g_boxFill.g = Scene_TintColor((g_tintG - 0x80) * 2 + 0x80);
        g_boxFill.b = Scene_TintColor((g_tintB - 0x80) * 2 + 0x80);
        g_drawPos[0] = g_boxFill.x;
        g_drawPos[1] = g_boxFill.y;
        g_drawPos[2] = g_boxFill.w;
        GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,2);
    }
    g_rect[0] = g_talk.x;
    g_rect[1] = g_talk.y;
    g_rect[2] = g_talk.w;
    g_rect[3] = g_talk.h;
    Frame_DrawTalk(g_rect,g_talk.frame,g_ot + g_otIndex,2);
}

/* Every frame: refresh the script variables for the pads and players (SVAR_*), run the view
 * transition and draw the view outline, layers, sprites, portrait and talk panel (0x8007CA48) */
/* 0x8007CA48 */
void Scene_Update(void)
{
    short x;
    short y;
    short w;
    short h;
    int i;

    Scene_UpdateVars();
    GsClearOt(0,4,g_sceneOt + g_otIndex);
    if (g_sceneView.visible != 0)
    {
        SceneView_Transition(&g_sceneView);
        Scene_DrawLayers();
        g_line.r = Scene_TintColor(g_tintR * 2 - 1);
        g_line.g = Scene_TintColor(g_tintG * 2 - 1);
        g_line.b = Scene_TintColor(g_tintB * 2 - 1);
        x = g_sceneView.x / 0x1000;
        y = g_sceneView.y / 0x1000;
        w = g_sceneView.w / 0x1000;
        h = g_sceneView.h / 0x1000;
        SceneView_SortLine(x,y,x + w,y);
        SceneView_SortLine(x,y + h,x + w,y + h);
        SceneView_SortLine(x,y,x,y + h);
        SceneView_SortLine(x + w,y,x + w,y + h);
        g_drawPos[0] = x;
        g_drawPos[1] = y;
    }
    for (i = 0; i < 64; i++)
    {
        if (g_sceneSprites[i].active != 0)
        {
            SceneSprite_Update(&g_sceneSprites[i]);
        }
    }
    Portrait_Draw();
    Talk_Draw();
    /* the scene's ordering table is drawn clipped to the view */
    g_drawPos[0] = g_sceneView.x / 0x1000;
    g_drawPos[1] = g_sceneView.y / 0x1000;
    g_drawPos[2] = g_sceneView.w / 0x1000;
    g_drawPos[3] = g_sceneView.h / 0x1000;
    Gfx_SetDrawArea(g_drawPos,g_sceneOt + g_otIndex,0x3f);
    Gfx_SetDrawArea(0,g_sceneOt + g_otIndex,0x20);
    GsSortOt(g_sceneOt + g_otIndex,g_ot + g_otIndex);
}

/* Sort a strip of a layer's image, from its line `line` on, at g_drawPos[0], [1], g_drawPos[2]
 * x g_drawPos[3] pixels */
static void SceneLayer_SortStrip(SceneLayer *layer, u32 attr, int line)
{
    VramImage *img = &g_sceneImages[layer->image];

    g_sprite.x = g_drawPos[0];
    g_sprite.y = g_drawPos[1];
    g_sprite.w = g_drawPos[2];
    g_sprite.h = g_drawPos[3];
    g_sprite.attribute = attr | img->mode << 24;
    g_sprite.tpage = img->x / 64 + (img->y + line) / 256 * 16;
    g_sprite.u = img->x % 64;
    g_sprite.v = img->y + line;
    g_sprite.cx = img->clutX;
    g_sprite.cy = img->clutY;
    g_sprite.r = g_tintR;
    g_sprite.g = g_tintG;
    g_sprite.b = g_tintB;
    GsSortFastSprite(&g_sprite,g_sceneOt + g_otIndex,layer->pri);
}

/* A row of a layer's strips from g_drawPos[0] to the right edge of the view */
static void SceneLayer_SortRow(SceneLayer *layer, u32 attr, int line, short viewRight)
{
    while (g_drawPos[0] < viewRight)
    {
        SceneLayer_SortStrip(layer,attr,line);
        g_drawPos[0] += layer->w;
    }
}

/* Draw the four background layers in the view: the image once, tiled over the view, line by line
 * with the lineX offsets (moving by lineStep, or a sine wave), or tiled in bands of lines. The
 * tiled bands mix up the view's x and y (as in the original) (0x8007DCC4) */
/* 0x8007DCC4 */
void Scene_DrawLayers(void)
{
    SceneLayer *layer;
    VramImage *img;
    short viewX;
    short viewY;
    short viewW;
    short viewH;
    u32 attr;
    u32 line;
    short x;
    short y;
    int band;
    int i;
    int k;

    viewX = g_sceneView.x / 0x1000;
    viewY = g_sceneView.y / 0x1000;
    viewW = g_sceneView.w / 0x1000;
    viewH = g_sceneView.h / 0x1000;
    for (i = 0; i < 4; i++)
    {
        layer = &g_sceneLayers[i];
        if (layer->active == 0)
        {
            continue;
        }
        attr = (layer->flags & LAYER_SEMITRANS) ? GFX_SEMITRANS : 0;
        img = &g_sceneImages[layer->image];
        if (layer->scrolling != 0)
        {
            layer->x += layer->vx;
            layer->y += layer->vy;
        }
        if (!(layer->flags & LAYER_LINES))
        {
            if (!(layer->flags & LAYER_TILED))
            {
                g_drawPos[0] = layer->x + viewX;
                g_drawPos[1] = layer->y + viewY;
                Gfx_DrawImage(img,attr,PAL_DEFAULT,g_drawPos,g_sceneOt + g_otIndex,layer->pri);
                continue;
            }
            y = viewY + layer->y % layer->h;
            g_drawPos[1] = (viewY < y) ? y - layer->h : y;
            while (g_drawPos[1] < viewY + viewH)
            {
                x = viewX + layer->x % layer->w;
                g_drawPos[0] = (viewX < x) ? x - layer->w : x;
                while (g_drawPos[0] < viewX + viewW)
                {
                    Gfx_DrawImage(img,attr,PAL_DEFAULT,g_drawPos,g_sceneOt + g_otIndex,layer->pri);
                    g_drawPos[0] += layer->w;
                }
                g_drawPos[1] += layer->h;
            }
            continue;
        }
        if (layer->waveMode == 1)
        {
            for (k = 0; k < 256; k++)
            {
                layer->lineX[k] += layer->lineStep[k];
            }
        }
        else if (layer->waveMode == 2)
        {
            Wave_FillShort(layer->lineX,viewH,layer->band,layer->waveLength,layer->waveAmp,
                           layer->wavePhase,0);
            layer->wavePhase += layer->wavePhaseStep;
        }
        if (!(layer->flags & LAYER_TILED))
        {
            /* one line a strip */
            for (k = 0; k < layer->h; k++)
            {
                g_drawPos[0] = layer->lineX[k] + layer->x + viewX;
                g_drawPos[1] = layer->y + viewY + k;
                g_drawPos[2] = layer->w;
                g_drawPos[3] = 1;
                SceneLayer_SortStrip(layer,attr,k);
            }
            continue;
        }
        /* bands of `band` lines down the view, each shifted by its lineX and tiled across; a band that
         * runs past the image's last line goes on with its first ones */
        g_drawPos[2] = layer->w;
        for (k = 0; band = layer->band, k < (viewH + band - 1) / band; k++)
        {
            line = (u32)(k * band - layer->y) % layer->h;
            g_drawPos[1] = viewX + k * band;
            x = viewY + (layer->x + layer->lineX[k]) % layer->w;
            g_drawPos[0] = (x > 0) ? x - layer->w : x;
            g_drawPos[3] = layer->band + viewY;
            if (layer->h < (int)(line + layer->band))
            {
                g_drawPos[3] = layer->h + viewY - line;
                SceneLayer_SortRow(layer,attr,line,viewX + viewW);
                g_drawPos[1] += g_drawPos[3];
                g_drawPos[3] = layer->band - g_drawPos[3];
                x = viewX + (layer->x + layer->lineX[k]) % layer->w;
                g_drawPos[0] = (viewX < x) ? x - layer->w : x;
                SceneLayer_SortRow(layer,attr,0,viewX + viewW);
            }
            else
            {
                SceneLayer_SortRow(layer,attr,line,viewX + viewW);
            }
        }
    }
}

/* Advance a script sprite along its path (steps of speed, x, y, animation, next), its animation
 * (frames of image, frames shown, flip bits, next; next -1 ends, -2 is the following entry) and
 * movement, then draw it (0x8007EAFC) */
/* 0x8007EAFC */
void SceneSprite_Update(SceneSprite *s)
{
    short step[5];
    short frame[4];
    int pos[2];
    int target[2];
    int dist[2];
    int angle;
    int moved;

    if ((s->attr & SCENE_OVER) && g_sceneView.visible != 0)
    {
        return;
    }
    if ((s->state & SPRITE_PATH) && !(s->state & SPRITE_MOVE))
    {
        /* the step's target reached: the next step */
        Mem_CopySwap16(step,g_scriptData + s->pathPos,10);
        if (step[4] == -1)
        {
            s->state &= ~(SPRITE_PATH | SPRITE_MOVE | SPRITE_ANIM);
        }
        else
        {
            s->pathPos = (step[4] == -2) ? s->pathPos + 10 : step[4];
            Mem_CopySwap16(step,g_scriptData + s->pathPos,10);
            s->state |= SPRITE_MOVE;
            s->targetX = step[1];
            s->targetY = step[2];
            s->animTime = 0;
            s->state |= SPRITE_ANIM;
            s->speed = step[0];
            s->animPos = step[3];
        }
    }
    if (s->state & SPRITE_ANIM)
    {
        Mem_CopySwap16(frame,g_scriptData + s->animPos,8);
        s->image = frame[0];
        s->attr = (s->attr & ~(SCENE_FLIP_X | SCENE_FLIP_Y)) | frame[2];
        if (++s->animTime == frame[1])
        {
            s->animTime = 0;
            if (frame[3] == -2)
            {
                s->animPos += 8;
            }
            else if (frame[3] == -1)
            {
                s->state &= ~SPRITE_ANIM;
            }
            else
            {
                s->animPos = frame[3];
            }
        }
    }
    if (s->state & SPRITE_MOVE)
    {
        if (s->speed == 0)
        {
            /* a pause of targetX frames */
            if (s->targetX < 1)
            {
                s->state &= ~SPRITE_MOVE;
            }
            else
            {
                s->targetX--;
            }
        }
        else
        {
            /* speed pixels a frame toward the target; a negative speed jumps there */
            moved = 0;
            if (s->speed > 0)
            {
                pos[0] = s->x;
                pos[1] = s->y;
                target[0] = s->targetX * 0x1000;
                target[1] = s->targetY * 0x1000;
                angle = Math_AngleDist(pos,target,dist);
                if (s->speed * 0x1000 <= dist[0])
                {
                    Math_Step(angle,s->speed * 0x1000,pos);
                    s->x = pos[0];
                    s->y = pos[1];
                    moved = 1;
                }
            }
            if (!moved)
            {
                s->x = s->targetX * 0x1000;
                s->state &= ~SPRITE_MOVE;
                s->y = s->targetY * 0x1000;
            }
        }
    }
    if (s->active != 0)
    {
        Scene_DrawImage(s->image,s->attr,s->x / 0x1000,s->y / 0x1000,s->scaleX,s->scaleY,s->angle,
                         s->pri);
    }
}

/* Alignment (ALIGN_*) for bits 4-7 of a script image's attr: row * 4 + column */
static const u8 k_sceneAlign[16] = {
    ALIGN_TOP | ALIGN_LEFT, ALIGN_TOP | ALIGN_HCENTER, ALIGN_TOP | ALIGN_RIGHT, 0,
    ALIGN_VCENTER | ALIGN_LEFT, ALIGN_VCENTER | ALIGN_HCENTER, ALIGN_VCENTER | ALIGN_RIGHT, 0,
    ALIGN_BOTTOM | ALIGN_LEFT, ALIGN_BOTTOM | ALIGN_HCENTER, ALIGN_BOTTOM | ALIGN_RIGHT, 0,
    0, 0, 0, 0
  };
#define SCENE_ALIGN(attr)  k_sceneAlign[((attr) >> 4) & 0xf]

/* Draw frame index of a tile sheet scaled and rotated (attr: SCENE_*). A negative priority draws
 * in the main ordering table (slot 5) rather than the scene's (0x8007EE2C) */
/* 0x8007EE2C */
void Scene_DrawSheetImage(TileSheet *sheet, u16 index, u32 attr, short x, short y, u16 scaleX, u16 scaleY, u16 angle, short pri)
{
    u32 flags;
    GsOT *ot;
    short dim;

    flags = (attr & SCENE_FLIP_X) ? GFX_FLIP_X : 0;
    if (attr & SCENE_FLIP_Y)
    {
        flags |= GFX_FLIP_Y;
    }
    if (attr & SCENE_OVER)
    {
        pri += 32;
    }
    if (attr & (SCENE_SEMITRANS | SCENE_SEMITRANS2))
    {
        flags |= GFX_SEMITRANS;
    }
    g_drawPos[0] = x;
    g_drawPos[1] = y;
    g_drawPos[2] = scaleX;
    g_drawPos[3] = scaleY;
    if (pri < 0)
    {
        pri = 5;
        ot = g_ot;
    }
    else
    {
        ot = g_sceneOt;
    }
    if (!(attr & SCENE_DIM))
    {
        TileSheet_DrawRotated(sheet,index,flags,g_drawPos,SCENE_ALIGN(attr),(short)angle >> 3,
                              ot + g_otIndex,pri);
        return;
    }
    /* darkened: tinted, semi-transparent mode 1 (added to the flags, as in the original) */
    dim = (g_sceneDim + 1) / 2;
    g_tintR = 0x80 - dim;
    g_tintB = g_tintR;
    g_tintG = g_tintR;
    TileSheet_DrawRotated(sheet,index,flags + (GFX_SEMITRANS | 1 << GFX_ABR_SHIFT),g_drawPos,
                          SCENE_ALIGN(attr),(short)angle >> 3,ot + g_otIndex,pri);
    g_tintR = 0x80;
    g_tintG = 0x80;
    g_tintB = 0x80;
}

/* Draw a sprite of the loaded sprite groups scaled and rotated (Sprite_DrawScaled; attr:
 * SCENE_OVER and the alignment) (0x8007F13C) */
/* 0x8007F13C */
void Scene_DrawGroupImage(u32 sprite, u32 attr, short x, short y, u16 scaleX, u16 scaleY, short angle, short pri)
{
    if (attr & SCENE_OVER)
    {
        pri += 32;
    }
    g_drawPos[2] = scaleX;
    g_drawPos[3] = scaleY;
    g_drawPos[0] = x;
    g_drawPos[1] = y;
    Sprite_DrawScaled(sprite & 0xffff,0,PAL_DEFAULT,g_drawPos,SCENE_ALIGN(attr),angle >> 3,
                      g_ot + g_otIndex,(pri < 0) ? 5 : pri);
}

/* Give back the glyph cells of the talk panel's characters and empty its cells */
void Talk_FreeGlyphs(void)
{
    short row;
    short col;

    for (row = 0; row < TALK_ROWS; row++)
    {
        for (col = 0; col < TALK_COLS; col++)
        {
            if (g_talk.kind[row][col] != TALK_NONE)
            {
                GLYPH_CELL_FREE(g_talkGlyphs[row][col]);
                g_talk.kind[row][col] = TALK_NONE;
            }
        }
    }
}

/* Empty the talk panel: free the glyph cells and start again at row 0 (0x8007F2F4) */
/* 0x8007F2F4 */
void Talk_Clear(void)
{
    g_talk.hasText = 0;
    g_talk.row = 0;
    g_talk.col = 0;
    Talk_FreeGlyphs();
    g_talk.cursorRow = -1;
}

/* A string (a name) into the talk panel */
void Talk_PutString(char *s)
{
    int i;

    for (i = 0; s[i] != 0; i++)
    {
        Talk_PutChar((u8 *)s + i);
        if (TALK_LAST_KIND == TALK_FULL)
        {
            i++;
        }
    }
}

/* Two decimal digits */
#define TWO_DIGITS(p)  (((char)(p)[0] - '0') * 10 + ((char)(p)[1] - '0'))

/* Text with codes into the talk panel; returns the bytes read (without the NUL). Codes: %Nnn /
 * %NM? the name of player nn / the current player, %K next row (the third at most), %Snn nn
 * spaces; another % is skipped. The script's inline text takes two bytes for any character but a
 * half-width one; text in the script data only for a full-width one, so the full-width space is
 * read as two half-width characters there */
int Talk_Print(u8 *text, int inlineText)
{
    u8 *ch = text;
    int player;
    int n;

    while (*ch != 0)
    {
        if (*ch != '%')
        {
            Talk_PutChar(ch);
            if (inlineText ? TALK_LAST_KIND != TALK_HALF : TALK_LAST_KIND == TALK_FULL)
            {
                ch++;
            }
            ch++;
            continue;
        }
        switch (ch[1])
        {
        case 'N':
            player = ((char)ch[2] == 'M') ? g_curPlayer : TWO_DIGITS(ch + 2);
            Talk_PutString(g_players[player].name);
            ch += 4;
            break;
        case 'K':
            if (g_talk.row < 2)
            {
                g_talk.row++;
            }
            g_talk.col = 0;
            ch += 2;
            break;
        case 'S':
            for (n = TWO_DIGITS(ch + 2); n > 0; n--)
            {
                Talk_PutChar((u8 *)g_strSpace);
            }
            ch += 4;
            break;
        default:
            ch++;
            break;
        }
    }
    return ch - text;
}

/* Add one Shift-JIS character (two bytes, or one half-width byte) to the talk panel; wraps after 38
 * columns, ignored after the third row (wrapping off the third row writes past chars, into kind)
 * (0x80081450) */
/* 0x80081450 */
void Talk_PutChar(u8 *ch)
{
    u16 code;
    u16 lead;

    code = ch[0] | ch[1] << 8;
    if (g_talk.col >= TALK_COLS)
    {
        if (g_talk.row > 2)
        {
            return;
        }
        g_talk.col = 0;
        g_talk.row++;
    }
    g_talk.chars[g_talk.row][g_talk.col] = code;
    if (g_talk.kind[g_talk.row][g_talk.col] != TALK_NONE)
    {
        GLYPH_CELL_FREE(g_talkGlyphs[g_talk.row][g_talk.col]);
    }
    lead = code & 0xf0;
    if (code == 0x4081)                       /* the full-width space (81 40): nothing drawn */
    {
        g_talk.kind[g_talk.row][g_talk.col] = TALK_NONE;
    }
    else if (lead == 0x80 || lead == 0x90 || lead == 0xe0 || lead == 0xf0)
    {
        /* a Shift-JIS lead byte: a full-width character */
        g_talkGlyphs[g_talk.row][g_talk.col] = Font_CacheGlyph((u8 *)&g_talk.chars[g_talk.row][g_talk.col]);
        g_talk.kind[g_talk.row][g_talk.col] = TALK_FULL;
    }
    else
    {
        g_talk.chars[g_talk.row][g_talk.col] = (u8)g_talk.chars[g_talk.row][g_talk.col];
        g_talkGlyphs[g_talk.row][g_talk.col] = Font_CacheGlyph((u8 *)&g_talk.chars[g_talk.row][g_talk.col]);
        g_talk.kind[g_talk.row][g_talk.col] = TALK_HALF;
    }
    g_talk.col++;
}

/* Load face `face` of player's character into the portrait tile sheet, its cells at vramX */
void Portrait_LoadFace(short player, short face, short vramX)
{
    void *faceData = g_faceData[player];
    short block;
    u32 *tim;
    short offset[4];

    g_portraitSheet.frames = g_sheetFrames;
    g_portraitSheet.vramX = vramX;
    g_portraitSheet.vramY = 0;
    g_portraitSheet.full = 0;
    g_portraitSheet.nextCell = 0;
    g_portraitSheet.count = 0;
    g_portraitSheet.stage = (u8 *)g_secHeapBlocks[g_fontWorkBlock].addr + 0x8000;
    LoadImage(&g_portraitClut,CHARFILE_CLUT(faceData,player));
    block = SecHeap_Alloc(8);
    tim = g_secHeapBlocks[block].addr;
    Lzss_Decompress(CHARFILE_IMAGE(faceData,face),(u8 *)tim);
    offset[0] = 0;
    offset[1] = 0;
    offset[2] = g_portraitClut.x;
    offset[3] = g_portraitClut.y;
    TileSheet_LoadTims(&g_portraitSheet,tim,1,offset);
    SecHeap_Free(block);
    TileSheet_Flush(&g_portraitSheet);
}

/* Load face number face of player's character (KAO?P.J) into the portrait tile sheet (0x800839E8) */
/* 0x800839E8 */
void Portrait_Load(short player, int face)
{
    Portrait_LoadFace(player,face,0x280);
}

/* Draw a script image: bits 12-14 of image pick the image group (0 scene tile sheet, 1 sprite
 * group 3, 2 the same from its image 30 on, 3 portrait sheet), the rest the image (0x80085A50) */
/* 0x80085A50 */
void Scene_DrawImage(u32 image, short attr, short x, short y, u32 scaleX, u32 scaleY, short angle, short pri)
{
    switch (image & IMG_GROUP_MASK)
    {
    case IMG_SCENE:
        Scene_DrawSheetImage(&g_sceneSheet,image & 0xfff,attr,x,y,scaleX,scaleY,angle,pri);
        break;
    case IMG_BOARD:
        Scene_DrawGroupImage((short)(g_spriteGroups[3].base + (image & 0xfff)),attr,x,y,scaleX,scaleY,
                             angle,pri);
        break;
    case IMG_BOARD30:
        Scene_DrawGroupImage((short)(g_spriteGroups[3].base + (image & 0xfff) + 30),attr,x,y,scaleX,
                             scaleY,angle,pri);
        break;
    case IMG_FACE:
        Scene_DrawSheetImage(&g_portraitSheet,image & 0xfff,attr,x,y,scaleX,scaleY,angle,pri);
        break;
    }
}
