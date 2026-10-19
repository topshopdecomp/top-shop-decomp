/*
 * boarddraw.c - drawing the board
 *
 * The display tasks that draw the board every frame: the store squares with their pictures,
 * signs and prices, the elevators and the stairs, the players' pieces with their shadows and the
 * arrows of the ways on; each stage's far background and its animation; the camera that follows
 * the current player; and the board's ordering tables, cleared at the start of a frame and sent
 * at its end.
 */
#include "common.h"
#include "topshop.h"

/* End of a board frame: text windows, glyph uploads, then the board ordering tables that were drawn
 * into (g_frontOt, then g_bgOt0, g_bgOt1, g_boardOt) (0x800430F4) */
/* 0x800430F4 */
u32 Board_FrameEnd(void)
{
    TextWin_DrawAll();
    Font_FlushUploads();
    g_glyphUploadPtr = g_glyphUploadBuf;
    if (g_frontOtUsed != 0)
    {
        GsSortOt(g_frontOt + g_otIndex,g_ot + g_otIndex);
    }
    if (g_boardOtUsed != 0)
    {
        GsSortOt(g_bgOt0 + g_otIndex,g_ot + g_otIndex);
        GsSortOt(g_bgOt1 + g_otIndex,g_ot + g_otIndex);
        GsSortOt(g_boardOt + g_otIndex,g_ot + g_otIndex);
    }
    return 0;
}

/* Start of every Board_Task frame: clears g_frontOt when g_frontOtOn, and g_bgOt0, g_bgOt1,
 * g_boardOt when g_boardOtOn; resets g_frontOtUsed / g_boardOtUsed (0x800431FC) */
/* 0x800431FC */
void Board_FrameStub(void)
{
    if (g_frontOtOn != 0)
    {
        g_frontOtUsed = 0;
        GsClearOt(0,0xe,g_frontOt + g_otIndex);
    }
    if (g_boardOtOn != 0)
    {
        g_boardOtUsed = 0;
        GsClearOt(0,0xf,g_bgOt0 + g_otIndex);
        GsClearOt(0,0x14,g_bgOt1 + g_otIndex);
        GsClearOt(0,0xd,g_boardOt + g_otIndex);
    }
}

/* Draw the full-screen event picture (VRAM 0x1C0, 0x108, CLUT row 0x1EC) (0x80049FEC) */
/* 0x80049FEC */
void Board_DrawEventPicture(void)
{
    VramImage img;
    short pos [4];

    img.mode = 1;
    img.x = 0x1c0;
    img.y = 0x108;
    img.w = 0x140;
    img.h = 0xf0;
    img.clutY = 0x1ec;
    img.clutX = 0;
    pos[0] = 0;
    pos[1] = 0;
    Gfx_DrawVramImage(&img,0,pos,g_ot + g_otIndex,0xc);
}

/* An elevator square at (sx, sy): its doors, open by work->slide on the square work->x, floor */
static void Board_DrawElevator(BoardDrawStoresWork *work, int x, int floor, short sx, short sy)
{
    short doors = g_spriteGroups[2].base;

    Gfx_SetDrawArea(NULL,g_ot + g_otIndex,0x10);
    g_drawPos[1] = sy;
    if (work->highlight == 0 || (char)work->x != x || (char)work->floor != floor)
    {
        g_drawPos[0] = sx + 0x28;
        Sprite_DrawAligned(doors,0,PAL_DEFAULT,g_drawPos,ALIGN_TOP | ALIGN_RIGHT,g_ot + g_otIndex,0x10);
        Sprite_DrawAligned(doors,0x400000,PAL_DEFAULT,g_drawPos,ALIGN_TOP | ALIGN_LEFT,g_ot + g_otIndex,
                           0x10);
    }
    else
    {
        g_drawPos[0] = sx - (work->slide - 0x28);
        Sprite_DrawAligned(doors,0,PAL_DEFAULT,g_drawPos,ALIGN_TOP | ALIGN_RIGHT,g_ot + g_otIndex,0x10);
        g_drawPos[0] = sx + work->slide + 0x28;
        Sprite_DrawAligned(doors,0x400000,PAL_DEFAULT,g_drawPos,ALIGN_TOP | ALIGN_LEFT,g_ot + g_otIndex,
                           0x10);
        g_drawPos[0] = sx + 0x28;
        g_drawPos[1] = sy;
        Sprite_DrawAligned(doors + 5,0,PAL_DEFAULT,g_drawPos,ALIGN_TOP,g_ot + g_otIndex,0x13);
    }
    g_drawPos[2] = 0x50;
    g_drawPos[3] = 0x30;
    g_drawPos[0] = sx;
    g_drawPos[1] = sy;
    Gfx_SetDrawArea(g_drawPos,g_ot + g_otIndex,0x10);
}

/* A store square at (sx, sy): its picture (or the empty square), and when owned the sold-out /
 * closed sign or the price of its cheapest goods and a mark per goods slot, and the owner's mark */
static void Board_DrawStore(short store, int x, int floor, short sx, short sy)
{
    Store *st = &g_stores[store];
    VramImage img;
    short id;
    int i;

    g_drawPos[0] = sx;
    g_drawPos[1] = sy;
    if (g_storeImageState[store] == STORE_IMAGE_READY)
    {
        StoreImage_VramPos(store,&img.x);
        StoreImage_ClutPos(store,(u16 *)&img.clutX);
        img.mode = 1;
        img.w = 0x50;
        img.h = 0x30;
        img.palette = 0;
        Gfx_DrawImage(&img,0x8000000,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,0x11);
    }
    else
    {
        Sprite_Draw(g_spriteGroups[2].base +
                    (char)k_stageCellSprites[g_boardStage][floor * g_stageDefs[g_boardStage].width + x] + 3,
                    0x8000000,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,0x11);
    }
    if (st->owner == -1)
    {
        return;
    }
    g_drawPos[0] = sx;
    g_drawPos[1] = sy + 0x3a;
    if (st->numGoods == 0)
    {
        Sprite_Draw(g_spriteGroups[0].base + 0x8c,0x8000000,PAL_DEFAULT,g_drawPos,g_boardOt + g_otIndex,0);
    }
    else if (st->tempClosed == 0)
    {
        Gfx_DrawNumber(st->goods[st->numGoods - 1].price,sx,g_drawPos[1],8,8,g_spriteGroups[0].base + 0x40,0,
                       g_boardOt + g_otIndex,0);
        for (i = 0; i < g_kindMaxGoods[st->chainLen]; i++)
        {
            g_drawPos[0] = sx + (short)(i / 2) * 6 + 8;
            g_drawPos[1] = sy + (short)(i % 2) * 6 + 0x3e;
            if (st->goods[i].active == 0)
            {
                id = g_spriteGroups[0].base + 0x72;   /* empty slot */
            }
            else if (st->goods[i].level < 2)
            {
                id = g_spriteGroups[0].base + 0x74;   /* the two most expensive levels */
            }
            else
            {
                id = g_spriteGroups[0].base + 0x73;
            }
            Sprite_Draw(id,0x8000000,PAL_DEFAULT,g_drawPos,g_boardOt + g_otIndex,0);
        }
    }
    else
    {
        Sprite_Draw(g_spriteGroups[0].base,0x8000000,PAL_DEFAULT,g_drawPos,g_boardOt + g_otIndex,0);
    }
    /* the owner's mark, in the owner's palette */
    g_drawPos[0] = sx + 8;
    g_drawPos[1] = sy + 0x3c;
    img = g_sprites[g_spriteGroups[0].base + 0xaa];
    img.clutX = 0x140;
    img.clutY = 0xfb;
    Gfx_DrawImage(&img,0x8000000,st->owner,g_drawPos,g_boardOt + g_otIndex,0);
}

/* A connector square (cell): the stairs, by the stage, the connector's height and shape */
static void Board_DrawConnector(u16 cell, short sx, short sy)
{
    short stairs = g_spriteGroups[2].base + 1;

    g_drawPos[0] = sx - 0x20;
    g_drawPos[1] = sy + 0x5f;
    switch (g_boardStage)
    {
    case 4:
    case 5:
    case 8:
    case 9:
        g_drawPos[1] = sy + 0x47;
        break;
    case 6:
        g_drawPos[1] = sy + 0x4f;
        break;
    }
    switch (cell & 0xf)
    {
    case 0:
        g_drawPos[0] = sx - 0x28;
        break;
    case 2:
        g_drawPos[0] = sx - 0x18;
        break;
    }
    switch (cell & 0xfff0)
    {
    case CONNECTOR_STAIRS:
        Sprite_DrawAligned(stairs,0x8000000,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM | ALIGN_LEFT,
                           g_boardOt + g_otIndex,3);
        break;
    case CONNECTOR_STAIRS_FLIPPED:
        Sprite_DrawAligned(stairs,0x400000,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM | ALIGN_LEFT,
                           g_boardOt + g_otIndex,3);
        break;
    case CONNECTOR_STAIRS_CROSSED:
        Sprite_DrawAligned(stairs,0x400000,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM | ALIGN_LEFT,
                           g_boardOt + g_otIndex,3);
        Sprite_DrawAligned(stairs,0x8000000,PAL_DEFAULT,g_drawPos,ALIGN_BOTTOM | ALIGN_LEFT,
                           g_boardOt + g_otIndex,1);
        break;
    }
}

/* Board_DrawStoresTask steps */
enum DrawStoresStep
{
    STORES_INIT          =  0,
    STORES_IDLE          = 10,  /* draw the stores */
    STORES_DOORS_OPEN    = 20,  /* ELEVATOR_DOORS_OPEN: on the current player's square */
    STORES_DOORS_OPENING = 21,  /* then ELEVATOR_DOORS_DONE */
    STORES_DOORS_CLOSE   = 30,  /* ELEVATOR_DOORS_CLOSE */
    STORES_DOORS_CLOSING = 31   /* then ELEVATOR_DOORS_DONE */
};

/* Display task: draw the store pictures, signs and owner marks, the elevators and connectors of
 * the visible part of the board. Messages: 0xE open the elevator doors on the current player's
 * square, 0xF close them (then message 5 to g_actionTaskIds[0]) (0x8004A08C) */
/* 0x8004A08C */
u32 Board_DrawStoresTask(short *step, BoardDrawStoresWork *work)
{
    StageDef *stage;
    u16 cell;
    short sx;
    short sy;
    int floor;
    int x;
    int msg[2];

    /* (the original copies the unused offset table at 0x8001249C to the stack here) */
    if (Task_ReceiveMessage(-3,msg) == 0)
    {
        if (msg[0] == ELEVATOR_DOORS_OPEN)
        {
            *step = STORES_DOORS_OPEN;
        }
        else if (msg[0] == ELEVATOR_DOORS_CLOSE)
        {
            *step = STORES_DOORS_CLOSE;
        }
    }
    switch (*step)
    {
    case STORES_INIT:
        work->active = 1;
        work->highlight = 0;
        work->unused01 = 0;
        *step = STORES_IDLE;
        break;
    case STORES_DOORS_OPEN:                 /* the doors open */
        work->highlight = 1;
        work->x = g_players[g_curPlayer].x;
        work->floor = g_players[g_curPlayer].floor;
        work->slide = 0;
        *step += 1;
        break;
    case STORES_DOORS_OPENING:
        work->slide += 4;
        if (work->slide >= 0x18)
        {
            work->slide = 0x18;
            Task_SendMessage(g_actionTaskIds[0],ELEVATOR_DOORS_DONE);
            *step = STORES_IDLE;
        }
        break;
    case STORES_DOORS_CLOSE:                /* they close */
        work->x = g_players[g_curPlayer].x;
        work->floor = g_players[g_curPlayer].floor;
        *step += 1;
        break;
    case STORES_DOORS_CLOSING:
        work->slide -= 4;
        if (work->slide <= 0)
        {
            work->slide = 0;
            work->highlight = 0;
            Task_SendMessage(g_actionTaskIds[0],ELEVATOR_DOORS_DONE);
            *step = STORES_IDLE;
        }
        break;
    }
    /* every square, from the last floor and column back */
    for (floor = g_stageDefs[g_boardStage].floors - 1; floor >= 0; floor--)
    {
        for (x = g_stageDefs[g_boardStage].width - 1; x >= 0; x--)
        {
            stage = &g_stageDefs[g_boardStage];
            sx = (g_cameraX / 4096) + (stage->originX + (short)x * 5) * 16;
            sy = (g_cameraY / 4096) + (stage->originY + (short)floor * 5) * 16;
            cell = Board_GetCell(x,floor);
            switch (CELL_TYPE(cell))
            {
            case CELL_ELEVATOR:
                if ((u16)(sx + 0x4f) < 399 && sy > -0x50 && sy < 0xf0)
                {
                    Board_DrawElevator(work,x,floor,sx,sy);
                }
                break;
            case CELL_STORE:
                if ((u16)(sx + 0x4f) < 399 && sy > -0x50 && sy < 0xf0)
                {
                    Board_DrawStore(cell,x,floor,sx,sy);
                }
                break;
            case CELL_CONNECTOR:
                Board_DrawConnector(cell,sx,sy);
                break;
            }
        }
    }
    return 0;
}

/* Board_FarBackgroundTask steps */
enum FarBackgroundStep
{
    FARBG_INIT =  0,
    FARBG_DRAW = 10   /* every frame */
};

/* Display task: the far background: the two map planes (plane 1 scrolling at 1/8 across and 1/16
 * up and down), the stage's own far layer, and after drawing, the camera position (0x8004AF8C) */
/* 0x8004AF8C */
u32 Board_FarBackgroundTask(short *step, BoardFarBackgroundWork *work)
{
    SPRT *line;
    DR_MODE *mode;
    int bg1ScrollX;
    int bg1ScrollY;
    int i;

    if (*step == FARBG_INIT)
    {
        work->active = 1;
        *step = FARBG_DRAW;
    }
    BgMap_Scroll(&g_bgPlanes[0].map,g_bgScrollX - g_cameraX,g_bgScrollY - g_cameraY);
    BgMap_Draw(&g_bgPlanes[0].map,g_bgOt0 + g_otIndex,3);
    bg1ScrollX = (g_bgScrollX + 0x360000) / 8 - 0x360000;
    bg1ScrollY = (g_bgScrollY + 0x2c8000) / 16 - 0x2c8000;
    BgMap_Scroll(&g_bgPlanes[1].map,g_bg1ScrollX - bg1ScrollX,g_bg1ScrollY - bg1ScrollY);
    BgMap_Draw(&g_bgPlanes[1].map,g_bgOt1 + g_otIndex,3);
    g_bg1ScrollX = bg1ScrollX;
    g_bg1ScrollY = bg1ScrollY;
    switch(g_boardStage)
    {
    case 0:
        Bg_DrawStage0(g_cameraX / 4096,g_cameraY / 4096);
        break;
    case 1:
        g_parallaxX = Math_MulFixed12(g_cameraX,0x14cc);
        Bg_DrawStage1(g_parallaxX / 4096,g_cameraY / 4096);
        break;
    case 2:
        Bg_DrawStage2(g_cameraX / 4096,g_cameraY / 4096);
        break;
    case 5:
        Bg_DrawStage5(g_cameraX / 4096,g_cameraY / 4096);
        break;
    case 8:
    case 9:
        /* a picture in VRAM (0x3C0, 0) drawn line by line, each shifted by g_bgWave */
        for (i = 0; i < 0xf0; i++)
        {
            line = g_primPtr;
            SetSprt(line);
            line->r0 = g_tintR;
            line->g0 = g_tintG;
            line->b0 = g_tintB;
            line->x0 = 0;
            line->y0 = i;
            line->u0 = ((-g_bgWaveX - g_bgScrollX) + g_bgWave[i] + 0x800) / 4096;
            line->v0 = i + ((-g_bgWaveY - g_bgScrollY + 0x800) / 4096);
            line->w = 0x140;
            line->h = 1;
            line->clut = GetClut(0x140,0xfd);
            SetSemiTrans(line,1);
            AddPrim(g_ot[g_otIndex].org + 0xc,line);
            g_primPtr = line + 1;
        }
        mode = g_primPtr;
        SetDrawMode(mode,0,0,GetTPage(0,0,0x3c0,0),NULL);
        AddPrim(g_ot[g_otIndex].org + 0xc,mode);
        g_primPtr = mode + 1;
        break;
    }
    g_boardOtUsed = 1;
    g_farBgScrollX = g_bgScrollX;
    g_farBgScrollY = g_bgScrollY;
    g_bgScrollX = g_cameraX;
    g_bgScrollY = g_cameraY;
    g_cameraX = g_cameraTargetX;
    g_cameraY = g_cameraTargetY;
    return 0;
}

/* One channel of the background tint (g_tintR/G/B): (tint - 0x80) * 2, kept within 0-255 */
static u8 Bg_TintChannel(short tint)
{
    int v = (tint - 0x80) * 2;

    if (v < 0)
    {
        return 0;
    }
    return (v < 0x100) ? v : 0xff;
}

/* A plain box of the background tint at (x, y) */
static void Bg_DrawTintBox(short x, short y, short size)
{
    g_boxFill.attribute = 0x40000000;
    g_boxFill.w = size;
    g_boxFill.h = size;
    g_boxFill.r = Bg_TintChannel(g_tintR);
    g_boxFill.g = Bg_TintChannel(g_tintG);
    g_boxFill.b = Bg_TintChannel(g_tintB);
    g_boxFill.x = x;
    g_boxFill.y = y;
    GsSortBoxFill(&g_boxFill,g_ot + g_otIndex,0xc);
}

/* A 40 x 40 quarter of a stage 5 tile: a sprite of group 1, or 0: a tinted box */
static void Bg_DrawStage5Quarter(char sprite, short *pos)
{
    if (sprite == 0)
    {
        Bg_DrawTintBox(pos[0],pos[1],0x28);
    }
    else
    {
        Sprite_Draw(g_spriteGroups[1].base + sprite,0x40000000,PAL_DEFAULT,pos,g_ot + g_otIndex,0xc);
    }
}

/* Far background of stage 5: 80 x 80 tiles, 26 a row (k_bgStage5Frames): frame 0 a tinted box,
 * else four 40 x 40 quarters (k_bgStage5Sprites, 4 per frame) (0x8004D1F0) */
/* 0x8004D1F0 */
void Bg_DrawStage5(short scrollX, short scrollY)
{
    const u8 *quarters;
    short pos[2];
    short tile;
    short row;
    short x;
    short y;
    int frame;

    row = -(scrollY / 0x50) * 0x1a - scrollX / 0x50;     /* the tile at the top left */
    for (y = scrollY % 0x50; y <= 0xef; y += 0x50, row += 0x1a)
    {
        tile = row;
        for (x = (scrollX % 0x50) - 0x10; x < 0x140; x += 0x50, tile++)
        {
            frame = k_bgStage5Frames[tile];
            if (frame == 0)
            {
                Bg_DrawTintBox(x,y,0x50);
                continue;
            }
            quarters = &k_bgStage5Sprites[frame * 4];
            pos[0] = x;
            pos[1] = y;
            Bg_DrawStage5Quarter(quarters[0],pos);
            pos[0] += 0x28;
            Bg_DrawStage5Quarter(quarters[1],pos);
            pos[1] += 0x28;
            Bg_DrawStage5Quarter(quarters[3],pos);
            pos[0] = x;
            Bg_DrawStage5Quarter(quarters[2],pos);
        }
    }
}

/* A far background of size x size pixel tiles, `stride` to a row of the table, scrolled by
 * (scrollX, scrollY): for each tile with a sprite (values <= blank but 0 or more: none) the sprite
 * of group 1 */
static void Bg_DrawTiles(const char *tiles, int size, int stride, int blank, short scrollX, short scrollY)
{
    short pos[2];
    int row;
    int tile;
    int x;
    int y;

    row = (short)(scrollY / size) * -stride - scrollX / size;
    for (y = scrollY % size; (short)y < 0xf0; y += size, row += stride)
    {
        tile = row;
        for (x = scrollX % size; (short)x < 0x140; x += size, tile++)
        {
            if (tiles[(short)tile] > blank || tiles[(short)tile] < 0)
            {
                pos[0] = x;
                pos[1] = y;
                Sprite_Draw(g_spriteGroups[1].base + tiles[(short)tile],0,PAL_DEFAULT,pos,
                            g_ot + g_otIndex,0xc);
            }
        }
    }
}

/* Far background of stage 0: 80 x 80 tiles, 26 a row (k_bgStage0Sprites) (0x8004D958) */
/* 0x8004D958 */
void Bg_DrawStage0(short scrollX, short scrollY)
{
    Bg_DrawTiles(k_bgStage0Sprites,0x50,0x1a,0,scrollX,scrollY);
}

/* Far background of stage 1: 64 x 64 tiles, 32 a row (k_bgStage1Frames; 0 and 1 are blank)
 * (0x8004DB04) */
/* 0x8004DB04 */
void Bg_DrawStage1(short scrollX, short scrollY)
{
    Bg_DrawTiles(k_bgStage1Frames,0x40,0x20,1,scrollX,scrollY);
}

/* Far background of stage 2: 48 x 48 tiles, 43 a row (k_bgStage2Sprites) (0x8004DC8C) */
/* 0x8004DC8C */
void Bg_DrawStage2(short scrollX, short scrollY)
{
    Bg_DrawTiles(k_bgStage2Sprites,0x30,0x2b,0,scrollX,scrollY);
}

/* Move the camera target a sixth of the way (at least 1.0) toward: mode 1 the current player, mode
 * 2 (x, y) (20.12 fixed point from pixels). Returns 1 when it is there (mode 0: at once)
 * (0x8004DED0) */
/* 0x8004DED0 */
u32 Camera_MoveTo(int mode, int x, int y)
{
    int pos[2];
    int target[2];
    int dist;
    int angle;
    int stepLen;

    pos[0] = g_cameraTargetX;
    pos[1] = g_cameraTargetY;
    if (mode == 1)
    {
        Board_FocusCurrentPlayer();
        target[0] = g_cameraTargetX;
        target[1] = g_cameraTargetY;
    }
    else if (mode == 0)
    {
        return 1;
    }
    else if (mode == 2)
    {
        target[0] = (x << 16) >> 4;
        target[1] = (y << 16) >> 4;
    }
    /* (other modes: target is not set, as in the original) */
    angle = Math_AngleDist(pos,target,&dist);
    if (dist <= 0x1000)
    {
        g_cameraTargetX = target[0];
        g_cameraTargetY = target[1];
        return 1;
    }
    stepLen = dist / 6;
    if (stepLen < 0x1000)
    {
        stepLen = 0x1000;
    }
    Math_Step(angle,stepLen,pos);
    g_cameraTargetX = pos[0];
    g_cameraTargetY = pos[1];
    return 0;
}

/* Bg_Stage8Task steps */
enum BgStage8Step
{
    STAGE8BG_INIT =  0,
    STAGE8BG_WAVE = 10   /* the wave moves on every frame */
};

/* Display task of stage 8: scroll the background and animate its wave (g_bgWave) (0x8004E0C8) */
/* 0x8004E0C8 */
u32 Bg_Stage8Task(short *step, BgStage8Work *work)
{
    if (*step == STAGE8BG_INIT)
    {
        work->bgWaveX = 0;
        work->bgWaveY = 0;
        work->phaseA = 0;
        work->phaseB = 0;
        work->angle = 0;
        *step = STAGE8BG_WAVE;
    }
    else if (*step == STAGE8BG_WAVE)
    {
        work->bgWaveX -= 0x1000;
        work->phaseA += 0x2000;
        work->bgWaveY += 0x1000;
        work->angle += 10;
        work->phaseB += 0x2000;
    }
    /* the wave's height swings with the angle (32 at most) */
    work->wave = rsin(work->angle) * 0x20;
    Wave_FillInt(g_bgWave,0x100,1,0x80,(short)(work->wave / 0x1000),0,0,0);
    g_bgWaveX = work->bgWaveX;
    g_bgWaveY = work->bgWaveY;
    return 0;
}

/* Bg_Stage9Task steps */
enum BgStage9Step
{
    STAGE9BG_INIT =  0,
    STAGE9BG_WAVE = 10   /* the wave moves on every frame */
};

/* Display task of stage 9: scroll the background and animate its wave (g_bgWave) (0x8004E1D0) */
/* 0x8004E1D0 */
u32 Bg_Stage9Task(short *step, BgStage9Work *work)
{
    if (*step == STAGE9BG_INIT)
    {
        work->bgWaveX = 0;
        work->bgWaveY = 0;
        work->phaseA = 0;
        work->phaseB = 0;
        work->angle = 0;
        *step = STAGE9BG_WAVE;
    }
    else if (*step == STAGE9BG_WAVE)
    {
        work->bgWaveX -= 0x2000;
        work->phaseA += 0x2000;
        work->bgWaveY -= 0x666;
        work->angle += 10;
        work->phaseB += 0x800;
    }
    /* the wave's height swings with the angle (16 at most), its phase runs on */
    work->wave = rsin(work->angle) * 0x10;
    Wave_FillInt(g_bgWave,0x100,1,0x20,(short)(work->wave / 0x1000),(short)(work->phaseB / 0x1000),0,0);
    g_bgWaveX = work->bgWaveX;
    g_bgWaveY = work->bgWaveY;
    return 0;
}

/* The zoom of a piece (0x1000 = 1.0): smaller the higher it is above its floor while the dice is
 * shown (it jumps) */
static short Board_PieceZoom(Player *pl, int y)
{
    if (pl->inElevator == 0)
    {
        return 0x1000;
    }
    return 0x1000 - ((((g_stageDefs[g_boardStage].originY + pl->floor * 5) * 16 - ((short)y - 0x3a)) *
                            0x1000) / 100);
}

/* An arrow of the current player's choice of ways (g_players[].exits), animated every 8 frames */
static void Board_DrawExitArrow(Player *pl, int dx, int dy, int sprite, u32 attr, u32 align)
{
    g_drawPos[0] = pl->screenX + (g_cameraX / 4096) + dx;
    g_drawPos[1] = pl->screenY + (g_cameraY / 4096) + dy;
    Sprite_DrawAligned(g_frameVsync / 8 % 4 + g_spriteGroups[0].base + sprite,attr,PAL_DEFAULT,g_drawPos,
                       align,g_ot + g_otIndex,1);
}

/* Board_DrawPiecesTask steps */
enum DrawPiecesStep
{
    PIECES_INIT = 0,  /* the pieces' animations reset */
    PIECES_DRAW = 1   /* every frame */
};

/* Display task: draw the players' pieces on the board, from the current player on (pieces on
 * one square side by side), with their shadows, and the arrows of the ways the current player
 * may choose (0x8004E2F4) */
/* 0x8004E2F4 */
u32 Board_DrawPiecesTask(short *step)
{
    signed char squares[6][2];                /* where the pieces drawn so far stand */
    CharAnimFrame *anim;
    Player *pl;
    GsOT *ot;
    u16 sprite;
    u16 base = 0;                             /* (k_pieceAnims only has kinds 0x00, 0x10, 0x40, 0x60) */
    u16 id;
    u32 flip;
    short dx;
    short lift = 0;
    int shown;
    int same;
    int player;
    int pri;
    int y;
    int i;
    int n;
    int w;
    int h;

    g_frontOtUsed = 1;
    if (*step == PIECES_INIT)
    {
        for (i = 0; i < g_numPlayers; i++)
        {
            g_players[i].pieceAnim = 0;
            g_players[i].animShown = 0;
            g_players[i].animFrame = 0;
            g_players[i].animTimer = 0;
        }
        *step += 1;
    }
    else if (*step != PIECES_DRAW)
    {
        g_frontOtUsed = 1;
        return 0;
    }
    shown = 0;
    for (n = 0; n < g_numPlayers; n++)
    {
        player = (g_curPlayer + n) % g_numPlayers;
        pl = &g_players[player];
        pl->panel = n;
        if (pl->pieceShown == 0 || pl->out != 0)
        {
            continue;
        }
        squares[shown][0] = pl->x;
        squares[shown][1] = pl->floor;
        shown++;
        /* the animation */
        if (pl->animShown != pl->pieceAnim)
        {
            pl->animFrame = 0;
            pl->animTimer = 0;
            pl->animShown = pl->pieceAnim;
        }
        anim = &k_pieceAnims[(char)pl->charId * 7 + (u16)pl->pieceAnim][(u16)pl->animFrame];
        sprite = anim->sprite;
        pl->pieceSprite = sprite;
        pl->animTimer++;
        flip = (u16)pl->pieceFlip ^ (char)anim->flip;
        if ((u16)pl->animTimer == (char)anim->frames)
        {
            if (anim->next == -2)
            {
                pl->animFrame++;
            }
            else
            {
                pl->animFrame = anim->next;
            }
            pl->animTimer = 0;
        }
        /* pieces on one square: alternately left and right of it, and a little higher */
        same = 0;
        for (i = 0; i < shown - 1; i++)
        {
            if (squares[i][0] == squares[shown - 1][0] && squares[i][1] == squares[shown - 1][1])
            {
                same++;
            }
        }
        if (same == 0 || same == 5)
        {
            dx = 0;
        }
        else if (((same - 1) & 1) == 0)
        {
            dx = ~((same - 1) / 2) << 4;
        }
        else
        {
            dx = ((same - 1) / 2 + 1) << 4;
        }
        switch (same)
        {
        case 0:
            lift = 0;
            break;
        case 1:
        case 2:
            lift = 2;
            break;
        case 3:
        case 4:
            lift = 4;
            break;
        case 5:
            lift = 6;
            break;
        }
        y = (u16)pl->screenY - (lift + 2);
        /* the piece: sprite kinds by bits 4-7 (0: the player's own pictures) */
        switch (sprite & 0xf0)
        {
        case 0x00:
            base = g_spriteGroups[3].base + (short)player * 5 + (sprite & 0xf);
            break;
        case 0x10:
            base = g_spriteGroups[3].base + 0x1e + (sprite & 0xf);
            break;
        case 0x40:
            base = g_spriteGroups[3].base + 0x28 + (sprite & 0xf);
            break;
        case 0x60:
            base = g_spriteGroups[3].base + 0x2d + (sprite & 0xf);
            break;
        default:
            base += sprite & 0xf;
            break;
        }
        id = base;
        if ((sprite & 0xf0) == 0x60 && (flip & 0xffff) == 0x10)
        {
            id = base + 5;                        /* the other side instead of flipped */
            flip = 0;
        }
        g_drawPos[0] = dx + pl->screenX + (g_cameraX / 4096);
        g_drawPos[1] = (pl->screenY + (g_cameraY / 4096)) - lift;
        g_drawPos[2] = Board_PieceZoom(pl,y);
        g_drawPos[3] = g_drawPos[2];
        if (pl->pieceState == 0xde)
        {
            ot = &g_ot[g_otIndex];
            pri = 0x12;
        }
        else if (pl->pieceState == 0xbe)
        {
            ot = &g_boardOt[g_otIndex];
            pri = 2;
        }
        else
        {
            ot = &g_frontOt[g_otIndex];
            pri = n;
        }
        Sprite_DrawZoomed(id,flip << 0x12,player,g_drawPos,ALIGN_BOTTOM,ot,(short)pri);
        /* its shadow */
        g_drawPos[0] = dx + pl->screenX + (g_cameraX / 4096);
        g_drawPos[1] = (pl->screenY + (g_cameraY / 4096)) - (lift + 2);
        g_drawPos[2] = Board_PieceZoom(pl,y);
        g_drawPos[3] = g_drawPos[2];
        if (pl->pieceState == 0xde)
        {
            ot = &g_ot[g_otIndex];
            pri = 0x12;
        }
        else
        {
            ot = &g_frontOt[g_otIndex];
            pri = (char)pl->panel + 9;
        }
        Sprite_DrawZoomed(g_spriteGroups[0].base + 0x71 & 0xffff,0x40000000,PAL_BLANK,g_drawPos,0,ot,pri);
    }

    /* the arrows of the ways the current player may choose */
    pl = &g_players[g_curPlayer];
    if (pl->choosing != 0)
    {
        w = g_sprites[g_spriteGroups[3].base].w / 2;
        h = g_sprites[g_spriteGroups[3].base].h / 2;
        if (pl->exits & EXIT_RIGHT)
        {
            Board_DrawExitArrow(pl,w,-h,0x96,0x400000,ALIGN_LEFT);
        }
        if (pl->exits & EXIT_RIGHT_FLOOR_MINUS)
        {
            Board_DrawExitArrow(pl,w,-g_sprites[g_spriteGroups[3].base].h,0x9a,0x400000,ALIGN_BOTTOM | ALIGN_LEFT);
        }
        if (pl->exits & EXIT_RIGHT_FLOOR_PLUS)
        {
            Board_DrawExitArrow(pl,w,0,0x9a,0xc00000,ALIGN_TOP | ALIGN_LEFT);
        }
        if (pl->exits & EXIT_LEFT)
        {
            Board_DrawExitArrow(pl,-w,-h,0x96,0,ALIGN_RIGHT);
        }
        if (pl->exits & EXIT_LEFT_FLOOR_MINUS)
        {
            Board_DrawExitArrow(pl,-w,-g_sprites[g_spriteGroups[3].base].h,0x9a,0,ALIGN_BOTTOM | ALIGN_RIGHT);
        }
        if (pl->exits & EXIT_LEFT_FLOOR_PLUS)
        {
            Board_DrawExitArrow(pl,-w,0,0x9a,0x800000,ALIGN_TOP | ALIGN_RIGHT);
        }
    }
    return 0;
}

/* Sprite id of a player's piece pose: the high nibble picks the sprites (0 the player's own
 * five, 1, 4 and 6 shared ones of group 3, else from base), the low nibble the frame
 * (0x80055960) */
/* 0x80055960 */
int Piece_GetSprite(short player, u16 pose, short base)
{
    short first;

    switch (pose & 0xf0)
    {
    case 0x00:
        first = GROUP_SPRITE(3,player * 5);
        break;
    case 0x10:
        first = GROUP_SPRITE(3,0x1e);
        break;
    case 0x40:
        first = GROUP_SPRITE(3,0x28);
        break;
    case 0x60:
        first = GROUP_SPRITE(3,0x2d);
        break;
    default:
        first = base;
        break;
    }
    return (short)(first + (pose & 0xf));
}

/* Set the board camera target (0x800B2688/268C, 20.12 fixed) to center the current player's piece.
 * (0x8007B654) */
/* 0x8007B654 */
void Board_FocusCurrentPlayer(void)
{
    g_cameraTargetX = (0xa0 - g_players[g_curPlayer].screenX) * 0x1000;
    g_cameraTargetY = (0x78 - g_players[g_curPlayer].screenY) * 0x1000;
}
