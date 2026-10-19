/*
 * ending.c - game mode 7: the ending
 *
 * After a free-play game: the final results screen, where each player's column rises to the
 * height of its rank, the sign comes down over the winner, with confetti and circling stars;
 * then back to the title.
 */
#include "common.h"
#include "topshop.h"

/* Upload A1.TIM and E_KID.TIM to VRAM (0x80042AE0) */
/* 0x80042AE0 */
void Ending_LoadTims(void)
{
    File_LoadTim(FILE_A1_TIM);
    File_LoadTim(FILE_E_KID_TIM);
}

/* Load the results screen's graphics: its sprites (ENDING.SPC, group 6), the background
 * (HYO_BG.TIM) and each player's character picture (CHR?P0.J: sprites of group 3, with its CLUT
 * in the player's color) (0x80072A94) */
/* 0x80072A94 */
void Ending_LoadGraphics(void)
{
    short block;
    void *data;
    RECT clut;
    short vramPos[4];
    short i;

    g_spriteGroups[6].next = g_spriteGroups[6].base;
    g_spriteGroups[3].next = g_spriteGroups[3].base;
    File_LoadPackedSprites(FILE_ENDING_SPC,6);
    File_LoadTim(FILE_HYO_BG_TIM);
    clut.x = 0x140;
    clut.y = 0xf2;
    clut.w = 0x10;
    clut.h = 1;
    vramPos[3] = 0;
    vramPos[2] = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        block = g_fileHandles[File_ReadSync((char)g_players[i].charId + FILE_CHR0P0_J,NULL,FILE_WHOLE)].block;
        data = g_secHeapBlocks[block].addr;
        LoadImage(&clut,CHARFILE_CLUT(data,i));
        vramPos[0] = i * 8 + 0x140;
        vramPos[1] = 0x100;
        Sprite_LoadSet(CHARFILE_IMAGE(data,0),3,vramPos);
        DrawSync(0);
        clut.x += 0x10;
        SecHeap_Free(block);
    }
}

/* Sprites of the results screen, in sprite group 6 */
#define RESULTS_SPRITE(n)  GROUP_SPRITE(6,n)

/* The x of the middle of player k's column */
#define RESULTS_COLUMN_X(work, k)  ((short)(k) * (work)->column + (short)(work)->column / 2 + 0x20)

/* The columns of the results screen: a cap, the player's face (its character's animation, in its
 * color), its column and its rank on it (turning while it rises) */
static void Results_DrawRows(EndingResultsWork *work)
{
    ResultsRow *row;
    CharAnimFrame *anim;
    char sprite;
    char frame;
    short y;
    short h;
    int k;

    for (k = 0; k < work->players; k++)
    {
        row = &g_resultsRows[k];
        /* the cap */
        g_drawPos[0] = (short)work->column / 2 + (short)k * work->column + 0x1c;
        if (row->state == RESULTS_FALLING)
        {
            g_drawPos[1] = 0x48 - g_sprites[RESULTS_SPRITE(4)].h;
        }
        else
        {
            g_drawPos[1] = (row->y >> 6) - (g_sprites[RESULTS_SPRITE(4)].h - 8);
        }
        Sprite_DrawAligned(RESULTS_SPRITE(4),0,PAL_DEFAULT,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,7);
        Gfx_SetDrawArea(0,g_ot + g_otIndex,6);
        /* the face */
        if (row->charId == 0)
        {
            anim = k_charAnimsChar0[0];
        }
        else if (row->charId == 4)
        {
            anim = k_charAnimsChar4[0];
        }
        else
        {
            anim = k_charAnims[0];
        }
        sprite = (row->state == RESULTS_FALLING) ? 0 : anim[row->frame].sprite;
        frame = row->frame;
        if ((u8)++row->timer == anim[frame].frames)
        {
            if (anim[frame].next == -2)
            {
                row->frame++;
            }
            else
            {
                row->frame = anim[frame].next;
            }
            row->timer = 0;
        }
        g_drawPos[2] = (char)k_resultsFaceLayout[work->players - 2][0];
        g_drawPos[3] = (char)k_resultsFaceLayout[work->players - 2][1];
        g_drawPos[0] = RESULTS_COLUMN_X(work,k);
        y = (row->state == RESULTS_FALLING) ? work->faceY : row->y;
        g_drawPos[1] = (y >> 6) - (char)k_resultsFaceLayout[work->players - 2][1] * 2 / 3;
        Sprite_DrawStretched((u16)(sprite + g_spriteGroups[3].base + k * 5),0,k,g_drawPos,0x10,
                             g_ot + g_otIndex,6);
        g_drawPos[0] = 0;
        g_drawPos[2] = 0x140;
        g_drawPos[1] = 0;
        g_drawPos[3] = 0xd7;
        Gfx_SetDrawArea(g_drawPos,g_ot + g_otIndex,6);
        /* the column */
        g_drawPos[2] = (char)k_resultsRowLayout[work->players - 2][0];
        g_drawPos[3] = (char)k_resultsRowLayout[work->players - 2][1];
        g_drawPos[0] = RESULTS_COLUMN_X(work,k);
        y = (row->state == RESULTS_FALLING) ? work->barY : row->y;
        g_drawPos[1] = (y >> 6) + (char)k_resultsRowLayout[work->players - 2][1] / 2;
        Sprite_DrawStretched((u16)(RESULTS_SPRITE(5) + k),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,5);
        /* the rank on it */
        g_drawPos[0] = RESULTS_COLUMN_X(work,k);
        h = g_drawPos[3];
        g_drawPos[2] = (char)k_resultsRowLayout[work->players - 2][0];
        if (row->state == RESULTS_FALLING)
        {
            y = work->barY;
        }
        else
        {
            y = row->y;
            g_drawPos[3] = (char)k_resultsRowLayout[work->players - 2][1];
        }
        g_drawPos[1] = (y >> 6) + h / 2;
        if (row->state < RESULTS_SHOWN)
        {
            Sprite_DrawStretched((u16)(RESULTS_SPRITE(0x25) + (g_frameVsync / 4) % 6),0,PAL_DEFAULT,g_drawPos,0,
                                 g_ot + g_otIndex,4);
        }
        else if (row->rank == work->lastRank && row->rank != 0)
        {
            Sprite_DrawStretched((u16)RESULTS_SPRITE(0x2a),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,4);
        }
        else
        {
            Sprite_DrawStretched((u16)(RESULTS_SPRITE(0x25) + row->rank),0,PAL_DEFAULT,g_drawPos,0,
                                 g_ot + g_otIndex,4);
        }
    }
}

/* The sign over the winner (or the middle with several winners): it comes down, its two halves
 * open (signAngle) on a picture (signState) */
static void Results_DrawSign(EndingResultsWork *work)
{
    g_drawPos[0] = work->winnerX / 64;
    g_drawPos[1] = work->signY / 64 + 8;
    Sprite_DrawAligned(((u8)work->signFrame >> 4) + RESULTS_SPRITE(0xd),0,PAL_DEFAULT,g_drawPos,ALIGN_TOP,
                       g_ot + g_otIndex,3);
    g_drawPos[0] = work->winnerX / 64;
    g_drawPos[1] = work->signY / 64 + 8;
    g_drawPos[2] = 0x1000;
    g_drawPos[3] = 0x1000;
    Sprite_DrawScaled((u16)RESULTS_SPRITE(0xb),0,PAL_DEFAULT,g_drawPos,0x11,-work->signAngle,
                      g_ot + g_otIndex,2);
    Sprite_DrawScaled((u16)RESULTS_SPRITE(0xc),0,PAL_DEFAULT,g_drawPos,0x12,work->signAngle,
                      g_ot + g_otIndex,2);
    if (work->signState == 1 || work->signState == 2)
    {
        g_drawPos[0] = work->winnerX / 64;
        g_drawPos[1] = work->signY / 64 + 8;
        Sprite_DrawAligned((work->signState == 1) ? RESULTS_SPRITE(0x32) : RESULTS_SPRITE(0x33),0,PAL_DEFAULT,
                           g_drawPos,ALIGN_TOP,g_ot + g_otIndex,1);
    }
}

/* Ending_ResultsTask steps */
enum EndingResultsStep
{
    ENDRES_INIT           =   0,
    ENDRES_ROWS           =   1,  /* a row per player, at the bottom */
    ENDRES_FADE_IN        =   3,
    ENDRES_WAIT_FADE_IN   =   4,
    ENDRES_PAUSE          =   5,  /* 300 frames */
    ENDRES_WAIT_PAUSE     =   6,
    ENDRES_TITLE_ZOOM     =   7,  /* the title zooms in */
    ENDRES_TITLE_SHOWN    =   8,  /* 60 frames */
    ENDRES_WAIT_TITLE     =   9,
    ENDRES_RISE_SOUND     =  15,
    ENDRES_RISE           =  20,  /* the columns rise 1/2 pixel a frame to their rank */
    ENDRES_RISEN          =  30,  /* 120 frames */
    ENDRES_WAIT_RISEN     =  31,
    ENDRES_ALL_SHOWN      =  32,  /* 60 frames; no last place: the winner */
    ENDRES_WAIT_LAST      =  33,
    ENDRES_LAST_FALLS     =  34,  /* the last place falls */
    ENDRES_FALLING        =  35,
    ENDRES_SIGN           =  40,  /* the sign comes down over the winner */
    ENDRES_SIGN_DOWN      =  41,
    ENDRES_SIGN_PAUSE     =  42,  /* 60 frames */
    ENDRES_WAIT_SIGN      =  43,
    ENDRES_SIGN_TURN      =  44,
    ENDRES_SIGN_FLIP      =  45,
    ENDRES_CELEBRATE      =  46,  /* Ending_CircleTask, Ending_SparkleTask */
    ENDRES_WAIT_CELEBRATE =  47,
    ENDRES_END            = 100,
    ENDRES_WAIT_BUTTON    = 101,  /* a button ends it */
    ENDRES_FADE_OUT       = 105,
    ENDRES_WAIT_FADE_OUT  = 106 
};

/* Task: the final results screen. The title zooms in, then each player's column rises to the
 * height of its rank (in turn order; the last place to the top) with a drum roll; the last place
 * then falls off, and a sign comes down over the winner (the middle with a tie) and opens. A
 * button (start, square, cross or circle) after that leaves (0x80072D18) */
/* 0x80072D18 */
u32 Ending_ResultsTask(short *step, EndingResultsWork *work)
{
    ResultsRow *row;
    VramImage img;
    PlayerSetup *setup;
    int allPlaced;
    int target;
    int limit;
    int i;

    switch (*step)
    {
    case ENDRES_INIT:
        work->shown = 0;
        work->players = g_gameConfig.numPlayers;
        Fade_SetBlack();
        *step += 1;
        /* fall through */
    case ENDRES_ROWS:
        work->column = 0x100 / work->players;
        work->rowStep = (0xa0 - (char)k_resultsRowLayout[work->players - 2][1]) / (work->players - 1);
        for (i = 0; i < work->players; i++)
        {
            row = &g_resultsRows[i];
            setup = &g_gameConfig.players[(char)g_gameConfig.turnOrder[i]];
            row->state = RESULTS_RISING;
            row->rank = setup->finalRank;
            row->frame = 0;
            row->timer = 0;
            row->charId = setup->charId;
            row->unused08 = 0;
            row->y = ((work->players - 1) * work->rowStep + 0x40) * 0x40;
        }
        work->shown = 1;
        work->showWinner = 0;
        work->winners = 0;
        work->lastRank = 0;
        for (i = 0; i < work->players; i++)
        {
            if (work->lastRank < g_resultsRows[i].rank)
            {
                work->lastRank = g_resultsRows[i].rank;
            }
            if (g_resultsRows[i].rank == 0)
            {
                work->winners++;
            }
        }
        work->busy = 1;
        work->zoom = 0x1000;
        work->scroll = 0;
        work->wave = 0;
        *step = ENDRES_FADE_IN;
        break;
    case ENDRES_FADE_IN:
        Fade_In(15);
        *step += 1;
        break;
    case ENDRES_WAIT_FADE_IN:
        if (Fade_IsDone())
        {
            *step += 1;
        }
        break;
    case ENDRES_PAUSE:
        work->timer = 300;
        *step += 1;
        break;
    case ENDRES_WAIT_PAUSE:
    case ENDRES_WAIT_RISEN:
    case ENDRES_WAIT_LAST:
    case ENDRES_WAIT_SIGN:
        if (--work->timer == 0)
        {
            *step += 1;
        }
        break;
    case ENDRES_TITLE_ZOOM:
        /* the title zooms in */
        work->zoom += 0x266;
        if (work->zoom > 0x7fff)
        {
            work->zoom = 0x8000;
            *step += 1;
        }
        break;
    case ENDRES_TITLE_SHOWN:
        work->busy = 0;
        work->timer = 60;
        *step += 1;
        break;
    case ENDRES_WAIT_TITLE:
        if (--work->timer == 0)
        {
            *step = ENDRES_RISE_SOUND;
        }
        break;
    case ENDRES_RISE_SOUND:
        work->slot = Se_Play(0x20,0x7f,0x7f,1);
        *step = ENDRES_RISE;
        break;
    case ENDRES_RISE:
        /* the columns rise 1/2 pixel a frame to their rank */
        allPlaced = 1;
        for (i = 0; i < work->players; i++)
        {
            row = &g_resultsRows[i];
            if (row->state != RESULTS_RISING)
            {
                continue;
            }
            if (row->rank == work->lastRank && row->rank != 0)
            {
                target = 0x40;
            }
            else
            {
                target = row->rank * (u16)work->rowStep + 0x40;
            }
            row->y -= 0x20;
            if (row->y <= target << 6)
            {
                row->y = target << 6;
                row->state = (row->rank != work->lastRank && row->rank != 0) ? RESULTS_SHOWN : RESULTS_PLACED;
            }
            if (row->state == RESULTS_RISING)
            {
                allPlaced = 0;
            }
        }
        if (allPlaced)
        {
            *step = ENDRES_RISEN;
        }
        break;
    case ENDRES_RISEN:
        work->timer = 120;
        *step += 1;
        break;
    case ENDRES_ALL_SHOWN:
        Se_StopSlot(work->slot);
        for (i = 0; i < work->players; i++)
        {
            g_resultsRows[i].state = RESULTS_SHOWN;
        }
        work->timer = 60;
        *step = (work->lastRank != 0) ? *step + 1 : ENDRES_SIGN;
        break;
    case ENDRES_LAST_FALLS:
        /* the last place falls */
        Se_Play(0x39,0x7f,0x7f,1);
        for (i = 0; i < work->players; i++)
        {
            row = &g_resultsRows[i];
            if (row->rank == work->lastRank && row->rank != 0)
            {
                row->state = RESULTS_FALLING;
            }
        }
        work->barY = 0x1000;
        work->faceY = 0x1000;
        work->barSpeed = 0;
        work->faceSpeed = 0;
        work->timer = 8;
        *step += 1;
        break;
    case ENDRES_FALLING:
        work->barSpeed += 0x20;
        work->barY += work->barSpeed;
        limit = (work->players - 1) * work->rowStep + 0x40;
        if (work->barY >= limit * 0x40)
        {
            work->barY = (short)limit * 0x40;
            Se_Play(0x3a,0x7f,0x7f,1);
        }
        if (work->timer != 0)
        {
            work->timer--;
            break;
        }
        work->faceSpeed += 0x20;
        work->faceY += work->faceSpeed;
        limit = (work->players - 1) * work->rowStep + 0x50;
        if (work->faceY >= limit * 0x40)
        {
            work->faceY = (short)limit * 0x40;
            *step = ENDRES_SIGN;
        }
        break;
    case ENDRES_SIGN:
        /* the sign comes down over the winner */
        Bgm_Play(5,0,0x7f,0x7f);
        work->showWinner = 1;
        if (work->winners < 2)
        {
            for (i = 0; i < work->players && g_resultsRows[i].rank != 0; i++)
            {
            }
            work->winnerX = RESULTS_COLUMN_X(work,i) * 0x40;
        }
        else
        {
            work->winnerX = 0x2800;
        }
        work->signAngle = 0;
        work->signFrame = 0;
        work->signState = 0;
        work->signY = -(g_sprites[RESULTS_SPRITE(0xb)].h * 0x40);  /* above the screen */
        *step += 1;
        break;
    case ENDRES_SIGN_DOWN:
        work->signY += 0x40;
        if (work->signY >= 0)
        {
            work->signY = 0;
            *step += 1;
        }
        break;
    case ENDRES_SIGN_PAUSE:
        work->timer = 60;
        *step += 1;
        break;
    case ENDRES_SIGN_TURN:
        Se_Play(0x21,0x7f,0x7f,1);
        work->signState = 1;
        work->signAngle += 0x20;
        if (work->signAngle > 0x154)
        {
            work->signAngle = 0x155;
            *step += 1;
        }
        break;
    case ENDRES_SIGN_FLIP:
        work->signState = 2;
        work->signFrame += 4;
        if ((u8)work->signFrame > 0x6f)
        {
            *step += 1;
        }
        break;
    case ENDRES_CELEBRATE:
        work->signState = 0;
        *(int *)g_subTaskWork[1] = work->winnerX;
        Task_Start(g_subTaskIds[1],Ending_CircleTask);
        *(int *)g_subTaskWork[2] = work->winnerX;
        Task_Start(g_subTaskIds[2],Ending_SparkleTask);
        *step += 1;
        break;
    case ENDRES_WAIT_CELEBRATE:
        if (Task_IsDone(g_subTaskIds[1]) && Task_IsDone(g_subTaskIds[2]))
        {
            *step = ENDRES_END;
        }
        break;
    case ENDRES_END:
        work->timer = 600;
        *step += 1;
        break;
    case ENDRES_WAIT_BUTTON:
        work->timer--;
        if (g_playerPads[6].pressed & (PADstart | PADRleft | PADRdown | PADRright))
        {
            *step = ENDRES_FADE_OUT;
        }
        break;
    case ENDRES_FADE_OUT:
        Bgm_Stop();
        Fade_Out(15);
        *step += 1;
        break;
    case ENDRES_WAIT_FADE_OUT:
        if (Fade_IsDone())
        {
            return 1;
        }
        break;
    }
    if (work->busy != 0)
    {
        /* the title */
        g_drawPos[0] = 0xa0;
        g_drawPos[1] = 0x78;
        g_drawPos[2] = g_sprites[RESULTS_SPRITE(0x31)].w * work->zoom / 0x1000;
        g_drawPos[3] = g_sprites[RESULTS_SPRITE(0x31)].h * work->zoom / 0x1000;
        Sprite_DrawStretched((u16)RESULTS_SPRITE(0x31),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,3);
    }
    if (work->shown != 0)
    {
        work->scroll = (short)(work->scroll + 0x10) % 0x400;
        work->wave = (short)(work->wave + 0x20) % 0x300;
        Results_DrawRows(work);
        if (work->showWinner > 0)
        {
            Results_DrawSign(work);
        }
    }
    /* the background picture */
    img.mode = 1;
    img.x = 0x280;
    img.y = 0;
    img.w = 0x140;
    img.h = 0xf0;
    img.clutX = 0;
    img.clutY = 0x1e9;
    g_drawPos[0] = 0;
    g_drawPos[1] = 0;
    Gfx_DrawVramImage(&img,0,g_drawPos,g_ot + g_otIndex,0xf);
    return 0;
}

/* Ending_SparkleTask steps */
enum EndingSparkleStep
{
    SPARKLE_INIT = 0,  /* the 128 pieces */
    SPARKLE_FALL = 1   /* until every piece has left at the bottom */
};

/* Task: the confetti of the results screen: 128 pieces (work->base: the x they fall around, 1/64
 * pixels) start one after the other from the top, sway from side to side as they fall and leave
 * at the bottom; the task ends when they all have (0x800741C4) */
/* 0x800741C4 */
u32 Ending_SparkleTask(short *step, EndingSparkleWork *work)
{
    Sparkle *s;
    char dir;
    int frame;
    int falling;
    int i;

    if (*step == SPARKLE_INIT)
    {
        work->active = 1;
        for (i = 0; i < 128; i++)
        {
            s = &g_sparkles[i];
            s->active = 1;
            s->kind = rand() % 24;
            s->dir = 1;
            s->phase = rand() % 3;
            s->delay = rand() % 512;
            s->y = 0x200;
            s->x = work->base + (rand() % 16 - 0x18) * 0x40;
            s->dx = rand() % 16 - 8;
            s->sway = 8;
            s->dy = 0x30;
        }
        *step += 1;
    }
    else if (*step == SPARKLE_FALL)
    {
        falling = 0;
        for (s = g_sparkles; s < &g_sparkles[128]; s++)
        {
            if (s->active == 0)
            {
                continue;
            }
            if (s->delay == 0)
            {
                /* the fall slows; then it sways the other way at full speed again */
                s->dy -= 2;
                if (s->dy < 1)
                {
                    dir = s->dir;
                    s->dy = 0x30;
                    s->dir = -dir;
                    s->sway = (s->dir * 0x100) >> 5;
                }
                s->x += s->dx;
                s->x += s->sway;
                s->y += s->dy;
                if (s->y > 0x3bff)
                {
                    s->active = 0;
                }
            }
            else
            {
                s->delay--;
            }
            if (s->active != 0)
            {
                falling = 1;
            }
        }
        if (!falling)
        {
            return 1;
        }
    }
    if (work->active != 0)
    {
        /* kind: the shape (kind / 8), the palette ((kind / 2) % 4), mirrored when even */
        for (i = 0; i < 128; i++)
        {
            s = &g_sparkles[i];
            if (s->delay != 0 || s->active == 0)
            {
                continue;
            }
            g_drawPos[0] = (s->x >> 6) + 0x10;
            g_drawPos[1] = s->y >> 6;
            frame = g_frameVsync / 8 + s->phase;
            Sprite_DrawAligned(GROUP_SPRITE(6,0x15) + (s->kind / 8) * 4 + (frame % 3),
                               (s->kind & 1) ? 0 : GFX_FLIP_X,(s->kind / 2 % 4),g_drawPos,0,
                               g_ot + g_otIndex,3);
        }
    }
    return 0;
}

/* Ending_CircleTask steps */
enum EndingCircleStep
{
    CIRCLES_INIT  = 0,
    CIRCLES_START = 1,  /* 16 circles, each after a random delay */
    CIRCLES_TURN  = 2   /* until they are all done */
};

/* Task: the stars of the results screen: 16 of them (work->base: the x of the circles' edge, 1/64
 * pixels) start one after the other and turn in growing circles one way or the other; the task
 * ends when they all have turned far enough (0x800745B0) */
/* 0x800745B0 */
u32 Ending_CircleTask(u16 *step, EndingCircleWork *work)
{
    EndingCircle *c;
    short angle;
    short size;
    int turning;

    switch (*step)
    {
    case CIRCLES_INIT:
        work->active = 0;
        *step += 1;
        break;
    case CIRCLES_START:
        work->active = 1;
        for (c = g_endingCircles; c < &g_endingCircles[16]; c++)
        {
            c->active = 1;
            c->mirrored = rand() % 2;
            c->angle = 0;
            c->delay = rand() % 480;
            size = rand() % 32 + 0x40;
            c->radius = size * 0x40;
            c->cx = (c->mirrored == 0) ? work->base - size * 0x40 : work->base + c->radius;
            c->cy = 0;
            c->unused0C = (c == g_endingCircles) ? 0x10 : -0x10;
            c->unused0E = 0;
        }
        *step += 1;
        break;
    case CIRCLES_TURN:
        turning = 0;
        for (c = g_endingCircles; c < &g_endingCircles[16]; c++)
        {
            if (c->active == 0)
            {
                continue;
            }
            if (c->delay == 0)
            {
                c->angle += 0x10;
                c->radius += 0x10;
                if (c->angle > 0x8e2)
                {
                    c->active = 0;
                }
            }
            else
            {
                c->delay--;
            }
            if (c->active != 0)
            {
                turning = 1;
            }
        }
        if (!turning)
        {
            return 1;
        }
        break;
    }
    if (work->active != 0)
    {
        for (c = g_endingCircles; c < &g_endingCircles[16]; c++)
        {
            if (c->active == 0)
            {
                continue;
            }
            angle = (c->mirrored == 0) ? c->angle : 0x800 - c->angle;
            g_drawPos[0] = ((rcos(angle) * c->radius * 16 >> 16) + c->cx) >> 6;
            g_drawPos[1] = ((rsin(angle) * c->radius * 16 >> 16) + c->cy) >> 6;
            Sprite_DrawAligned(k_endingCircleOrder[g_frameVsync / 8 % 4] + GROUP_SPRITE(6,0x21),
                               (c->mirrored != 0) ? GFX_FLIP_X : 0,PAL_DEFAULT,g_drawPos,ALIGN_TOP,
                               g_ot + g_otIndex,3);
        }
    }
    return 0;
}

/* Ending_Task steps */
enum EndingStep
{
    ENDING_LOAD         =   0,  /* the sub-tasks, the graphics */
    ENDING_RESULTS      =  10,  /* Ending_ResultsTask */
    ENDING_WAIT_RESULTS =  11,
    ENDING_DONE         = 100   /* to the title */
};

/* Game mode 7: the ending: the results screen (Ending_LoadGraphics, Ending_ResultsTask), then
 * g_gameMode = 2 (the title) (0x80076068) */
/* 0x80076068 */
int Ending_Task(short *step, void *work)
{
    Gfx_QueueClear(0,0,0);
    FntPrint("DrawBufId  : %d\n", g_otIndex);
    FntPrint("main phase : %d\n", (u16)*step);
    g_debugPrint = 1;
    switch ((u16)*step)
    {
    case ENDING_LOAD:
        Mode_CreateSubTasks();
        Ending_LoadGraphics();
        *step = ENDING_RESULTS;
        break;
    case ENDING_RESULTS:
        Task_Start(g_subTaskIds[0],Ending_ResultsTask);
        *step += 1;
        break;
    case ENDING_WAIT_RESULTS:
        if (Task_IsDone(g_subTaskIds[0]))
        {
            *step = ENDING_DONE;
        }
        break;
    case ENDING_DONE:
        g_gameMode = MODE_TITLE;
        return 1;
    }
    return 0;
}
