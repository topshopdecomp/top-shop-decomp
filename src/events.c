/*
 * events.c - event cards
 *
 * Everything about the event cards but their effects: the event of a turn, drawn at random
 * (Event_PickRandom) or chosen with a debug rule, and its card scene where three cards are dealt
 * and the player picks one; loading the event's picture; the points exchange at the bank, where
 * cards are bought with points; carrying a card out (Event_ApplyTask); choosing the target
 * player; and which players each card can be used on now.
 */
#include "common.h"
#include "topshop.h"

/* The event card's map of the board: Minimap_Draw(16, 144, 288, 80, a, b) (0x8006BC3C) */
/* 0x8006BC3C */
void EventCard_DrawFrame(short a, short b)
{
    Minimap_Draw(0x10,0x90,0x120,0x50,a,b);
}

/* What EventCard_Task draws (EventCardWork.disp) */
#define CARDS_HIDDEN   0
#define CARDS_DEALT    1   /* the three cards come in face down */
#define CARDS_CHOOSE   2   /* the chosen one raised, the hand over it */
#define CARDS_PICKED   3   /* the others shrink away */
#define CARDS_MOVING   4   /* the chosen one goes to the middle */
#define CARDS_TURNING  5
#define CARDS_SHOWN    6   /* face up */
#define CARDS_LEAVING  7   /* it flies off, turning and growing */

/* Sprites of the cards, in sprite group 0 */
#define CARD_HAND      0x83
#define CARD_BACK      0xC6
#define CARD_FRAME     0xC7
#define CARD_PICTURE   0xC8   /* the event's picture (EventCard_LoadTask) */

/* A card face down at (x, y), with its shadow */
static void EventCard_DrawBack(short x, short y)
{
    g_drawPos[0] = x;
    g_drawPos[1] = y;
    Sprite_DrawAligned(GROUP_SPRITE(0,CARD_BACK),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
    g_drawPos[0] = x + 4;
    g_drawPos[1] = y + 4;
    Sprite_DrawAligned(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
}

/* EventCard_Task steps */
enum EventCardStep
{
    ECARD_DEAL          =   0,  /* the event's points; three cards dealt face down */
    ECARD_DEAL_IN       =   1,  /* they come in from the right */
    ECARD_SPREAD        =   2,  /* they spread out */
    ECARD_CHOOSE        =  10,  /* a human player picks one (left / right, cross) */
    ECARD_CPU           =  20,  /* a CPU player moves to a random card */
    ECARD_CPU_MOVE      =  21,
    ECARD_CPU_MOVE_WAIT =  22,
    ECARD_CPU_PICK      =  23,  /* 15 frames */
    ECARD_CPU_PICK_WAIT =  24,
    ECARD_PICKED        =  30,  /* once the event is loaded */
    ECARD_OTHERS_SHRINK =  31,
    ECARD_TO_MIDDLE     =  35,
    ECARD_MOVE_RIGHT    =  36,  /* the left card to the middle */
    ECARD_MOVE_LEFT     =  37,  /* the right card to the middle */
    ECARD_TURN          =  40,  /* it turns over */
    ECARD_TURNING       =  41,  /* two half turns */
    ECARD_SHOWN         =  50,
    ECARD_SHOW_HUMAN    =  51,  /* 150 frames or a button */
    ECARD_WAIT_HUMAN    =  52,
    ECARD_SHOW_CPU      =  53,  /* 60 frames */
    ECARD_WAIT_CPU      =  54,
    ECARD_LEAVE         =  60,  /* it flies off */
    ECARD_LEAVING       =  61,
    ECARD_DONE          = 100 
};

/* Task: the event drawn this turn (g_eventId): it gives its points at once; three cards are dealt
 * face down, the player picks one (left / right, cross; a CPU player at random), the others shrink
 * away, the chosen one goes to the middle, turns over (the event's picture), then flies off. User
 * data: g_eventId (0x8006BC80) */
/* 0x8006BC80 */
u32 EventCard_Task(short *step, EventCardWork *work)
{
    Player *pl = &g_players[g_curPlayer];
    ScreenPos *chosen;
    char points;
    int eventId;
    int cosine;
    int dealt;
    int i;

    FntPrint("exec task event card\n");
    FntPrint("phase : %d\n", (u16)*step);
    FntPrint("disp : %d\n", work->disp);
    points = g_eventCardPoints;
    eventId = g_eventId;
    switch (*step)
    {
    case ECARD_DEAL:
        work->points = g_eventCardPoints;
        work->eventId = eventId;
        pl->points = (pl->points + points > 99) ? 99 : pl->points + points;
        work->loaded = 0;
        work->choice = 0;
        work->disp = CARDS_DEALT;
        for (i = 0; i < 3; i++)
        {
            work->cards[i].x = 0x160;
            work->cards[i].y = 0x78;
        }
        work->slot = Se_Play(0x38,0x7f,0x7f,1);
        *step += 1;
        break;
    case ECARD_DEAL_IN:
        for (i = 0; i < 3; i++)
        {
            work->cards[i].x -= 0x18;
        }
        if (work->cards[0].x <= 0x50)
        {
            for (i = 2; i >= 0; i--)
            {
                work->cards[i].x = 0x50;
            }
            *step += 1;
        }
        break;
    case ECARD_SPREAD:
        /* they spread out to x 0x50, 0xa0, 0xf0 */
        dealt = 1;
        for (i = 0; i < 3; i++)
        {
            work->cards[i].x += 0x18;
            if (work->cards[i].x < 0x50 + i * 0x50)
            {
                dealt = 0;
            }
            else
            {
                work->cards[i].x = 0x50 + i * 0x50;
            }
        }
        if (dealt)
        {
            Se_StopSlot(work->slot);
            work->loaded = 1;
            *step = (pl->isCpu == 0) ? ECARD_CHOOSE : ECARD_CPU;
            work->disp = CARDS_CHOOSE;
        }
        break;
    case ECARD_CHOOSE:
        if (g_playerPads[g_curPlayer].pressed & PADRdown)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = ECARD_PICKED;
        }
        else if (g_playerPads[g_curPlayer].repeat & PADLleft)
        {
            work->choice = (work->choice == 0) ? 2 : work->choice - 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[g_curPlayer].repeat & PADLright)
        {
            work->choice = (work->choice == 2) ? 0 : work->choice + 1;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    /* 20-24: a CPU player moves to a random card */
    case ECARD_CPU:
        work->cpuChoice = rand() % 3;
        *step += 1;
        break;
    case ECARD_CPU_MOVE:
        if (work->choice != work->cpuChoice)
        {
            work->choice++;
            work->timer = 4;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            *step += 1;
            break;
        }
        *step = ECARD_CPU_PICK;
        break;
    case ECARD_CPU_MOVE_WAIT:
        if (--work->timer == 0)
        {
            *step = ECARD_CPU_MOVE;
        }
        break;
    case ECARD_CPU_PICK:
        work->timer = 15;
        *step += 1;
        break;
    case ECARD_CPU_PICK_WAIT:
        if (--work->timer == 0)
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = ECARD_PICKED;
        }
        break;
    case ECARD_PICKED:
        if (g_eventDrawn == 0)
        {
            work->disp = CARDS_PICKED;
            work->scaleX = 0x1000;
            work->scaleY = 0x1000;
            *step += 1;
        }
        break;
    case ECARD_OTHERS_SHRINK:
        work->scaleX -= 0x198;
        work->scaleY -= 0x198;
        if (work->scaleX < 1)
        {
            work->scaleX = 0;
            work->scaleY = 0;
            *step = ECARD_TO_MIDDLE;
        }
        break;
    case ECARD_TO_MIDDLE:
        work->disp = CARDS_MOVING;
        if (work->choice == 0)
        {
            *step = ECARD_MOVE_RIGHT;
        }
        else if (work->choice == 1)
        {
            *step = ECARD_TURN;
        }
        else if (work->choice == 2)
        {
            *step = ECARD_MOVE_LEFT;
        }
        break;
    case ECARD_MOVE_RIGHT:
        chosen = &work->cards[work->choice];
        chosen->x += 0x18;
        if (chosen->x > 0x9f)
        {
            chosen->x = 0xa0;
            *step = ECARD_TURN;
        }
        break;
    case ECARD_MOVE_LEFT:
        chosen = &work->cards[work->choice];
        chosen->x -= 0x18;
        if (chosen->x <= 0xa0)
        {
            chosen->x = 0xa0;
            *step = ECARD_TURN;
        }
        break;
    case ECARD_TURN:
        work->slot = Se_Play(0x38,0x7f,0x7f,1);
        work->disp = CARDS_TURNING;
        work->angle = 0;
        work->timer = 2;
        *step += 1;
        break;
    case ECARD_TURNING:
        /* two half turns */
        work->angle = (work->angle + 0x100) & 0xfff;
        if (work->angle == 0x800 && --work->timer == 0)
        {
            Se_StopSlot(work->slot);
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = ECARD_SHOWN;
        }
        break;
    case ECARD_SHOWN:
        work->disp = CARDS_SHOWN;
        *step = (pl->isCpu == 0) ? ECARD_SHOW_HUMAN : ECARD_SHOW_CPU;
        break;
    case ECARD_SHOW_HUMAN:
        work->timer = 150;
        *step += 1;
        break;
    case ECARD_WAIT_HUMAN:
        if (--work->timer == 0 || (g_playerPads[g_curPlayer].pressed & MSG_ANY_BUTTON))
        {
            *step = ECARD_LEAVE;
        }
        break;
    case ECARD_SHOW_CPU:
        work->timer = 60;
        *step += 1;
        break;
    case ECARD_WAIT_CPU:
        if (--work->timer == 0)
        {
            *step = ECARD_LEAVE;
        }
        break;
    case ECARD_LEAVE:
        work->slot = Se_Play(0x38,0x7f,0x7f,1);
        work->disp = CARDS_LEAVING;
        work->x = work->cards[work->choice].x;
        work->y = work->cards[work->choice].y;
        work->scaleX = 0x1000;
        work->scaleY = 0x1000;
        work->angle = 0;
        *step += 1;
        break;
    case ECARD_LEAVING:
        chosen = &work->cards[work->choice];
        chosen->x += 8;
        chosen->y -= 8;
        work->x += 4;
        work->y -= 4;
        work->scaleX += 0xcc;
        work->scaleY += 0xcc;
        work->angle -= 0xb6;
        if (work->x > 0x16f)
        {
            Se_StopSlot(work->slot);
            work->disp = CARDS_HIDDEN;
            *step = ECARD_DONE;
        }
        break;
    case ECARD_DONE:
        Task_SetUserData(-1,g_eventId);
        return 1;
    }

    chosen = &work->cards[work->choice];
    switch (work->disp)
    {
    case CARDS_DEALT:
        for (i = 0; i < 3; i++)
        {
            EventCard_DrawBack(work->cards[i].x,work->cards[i].y);
        }
        break;
    case CARDS_CHOOSE:
        for (i = 0; i < 3; i++)
        {
            g_drawPos[0] = work->cards[i].x;
            g_drawPos[1] = work->cards[i].y;
            if (work->choice == i)
            {
                g_drawPos[0] -= 4;
                g_drawPos[1] -= 4;
            }
            Sprite_DrawAligned(GROUP_SPRITE(0,CARD_BACK),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
            g_drawPos[0] = work->cards[i].x + 4;
            g_drawPos[1] = work->cards[i].y + 4;
            Sprite_DrawAligned(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
            if (work->choice == i)
            {
                g_drawPos[0] = work->cards[i].x;
                g_drawPos[1] = work->cards[i].y - 0x40;
                Sprite_DrawAligned(GROUP_SPRITE(0,CARD_HAND),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
            }
        }
        break;
    case CARDS_PICKED:
        EventCard_DrawBack(chosen->x,chosen->y);
        for (i = 0; i < 3; i++)
        {
            if (work->choice == i)
            {
                continue;
            }
            g_drawPos[0] = work->cards[i].x;
            g_drawPos[1] = work->cards[i].y;
            g_drawPos[2] = work->scaleX + 0xcc;
            g_drawPos[3] = work->scaleY + 0xcc;
            Sprite_DrawZoomed(GROUP_SPRITE(0,CARD_BACK),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
            g_drawPos[0] = work->cards[i].x + 4;
            g_drawPos[1] = work->cards[i].y + 4;
            Sprite_DrawZoomed(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
        }
        break;
    case CARDS_MOVING:
        EventCard_DrawBack(chosen->x,chosen->y);
        break;
    case CARDS_TURNING:
        /* narrowed by the turn: the back, then (past a quarter turn) the picture in its frame; the
         * shadow narrows with it */
        g_drawPos[2] = 0x40;
        cosine = rcos(work->angle);
        if (cosine < 0)
        {
            cosine = -cosine;
        }
        g_drawPos[3] = (u32)(cosine * 5) >> 8;
        g_drawPos[0] = chosen->x;
        g_drawPos[1] = chosen->y;
        if (rcos(work->angle) < 0)
        {
            Sprite_DrawRotScaled(GROUP_SPRITE(0,CARD_PICTURE),GFX_FLIP_Y,PAL_DEFAULT,g_drawPos,0,
                                 work->angle,0x147,g_ot + g_otIndex,8);
            Sprite_DrawRotScaled(GROUP_SPRITE(0,CARD_FRAME),GFX_FLIP_Y,PAL_DEFAULT,g_drawPos,0,
                                 work->angle,0x147,g_ot + g_otIndex,8);
        }
        else
        {
            Sprite_DrawRotScaled(GROUP_SPRITE(0,CARD_BACK),GFX_FLIP_Y,PAL_DEFAULT,g_drawPos,0,
                                 work->angle,0x147,g_ot + g_otIndex,8);
        }
        g_drawPos[0] = chosen->x + 4;
        g_drawPos[1] = chosen->y + 4;
        Sprite_DrawStretched(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,
                             g_ot + g_otIndex,8);
        break;
    case CARDS_SHOWN:
        g_drawPos[0] = chosen->x;
        g_drawPos[1] = chosen->y;
        Sprite_DrawAligned(GROUP_SPRITE(0,CARD_PICTURE),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
        Sprite_DrawAligned(GROUP_SPRITE(0,CARD_FRAME),0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
        g_drawPos[0] = chosen->x + 4;
        g_drawPos[1] = chosen->y + 4;
        Sprite_DrawAligned(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
        break;
    case CARDS_LEAVING:
        g_drawPos[0] = chosen->x;
        g_drawPos[1] = chosen->y;
        g_drawPos[2] = work->scaleX;
        g_drawPos[3] = work->scaleY;
        Sprite_DrawScaled(GROUP_SPRITE(0,CARD_PICTURE),0,PAL_DEFAULT,g_drawPos,0,work->angle,
                          g_ot + g_otIndex,8);
        Sprite_DrawScaled(GROUP_SPRITE(0,CARD_FRAME),0,PAL_DEFAULT,g_drawPos,0,work->angle,
                          g_ot + g_otIndex,8);
        g_drawPos[0] = work->x + 4;
        g_drawPos[2] = 0x1000;
        g_drawPos[3] = 0x1000;
        g_drawPos[1] = work->y + 4;
        Sprite_DrawScaled(GROUP_SPRITE(0,CARD_BACK),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,work->angle,
                          g_ot + g_otIndex,8);
        break;
    }
    return 0;
}

/* EventPick_Task steps */
enum EventPickStep
{
    EVPICK_OPEN       =   0,
    EVPICK_WAIT_OPEN  =   1,
    EVPICK_CHOOSE     =  10,  /* left / right the event, cross picks, circle cancels */
    EVPICK_CLOSE      = 100,
    EVPICK_WAIT_CLOSE = 101 
};

/* Task (rule RULE_PICK_EVENT): the player chooses the event from a list (0x8006CECC) */
/* 0x8006CECC */
u32 EventPick_Task(u16 *step, EventPickWork *work)
{
    int enable;
    short win;
    int len;
    int choice;

    switch (*step)
    {
    case EVPICK_OPEN:
        work->done = 1;
        memcpy(g_msgBuf, "%CEvents   :", 13);
        len = strlen(g_msgBuf);
        Str_FormatInt((char)g_boardRules.enable[RULE_PICKED_EVENT],2,g_msgBuf + len);
        win = TextWin_Create(TEXTWIN_AUTO,TEXTWIN_AUTO,-1,-1);
        work->win = win;
        TextWin_AddText(work->win,g_msgBuf);
        TextWin_Open(work->win);
        win = TextWin_Create(TEXTWIN_AUTO,0x8c,0x2e,-1);
        work->id = win;
        TextWin_AddText(work->id,k_eventPickTexts[(char)g_boardRules.enable[RULE_PICKED_EVENT]]);
        TextWin_Open(work->id);
        *step += 1;
        break;
    case EVPICK_WAIT_OPEN:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            *step = EVPICK_CHOOSE;
            return 0;
        }
        return 0;
    case EVPICK_CHOOSE:
        if ((g_playerPads[g_curPlayer].pressed & PADRdown) == 0)
        {
            if ((g_playerPads[g_curPlayer].pressed & PADRright) == 0)
            {
                if ((g_playerPads[g_curPlayer].repeat & PADLleft) == 0)
                {
                    if ((g_playerPads[g_curPlayer].repeat & PADLright) == 0)
                    {
                        return 0;
                    }
                    enable = g_boardRules.enable[RULE_PICKED_EVENT] == 99;
                    g_boardRules.enable[RULE_PICKED_EVENT]++;
                    if (enable)
                    {
                        g_boardRules.enable[RULE_PICKED_EVENT] = 0;
                    }
                    memcpy(g_msgBuf, "%CEvent:", 9);
                }
                else
                {
                    enable = g_boardRules.enable[RULE_PICKED_EVENT] == 0;
                    g_boardRules.enable[RULE_PICKED_EVENT] += 0xff;
                    if (enable)
                    {
                        g_boardRules.enable[RULE_PICKED_EVENT] = 99;
                    }
                    memcpy(g_msgBuf, "%CEvents   :", 13);
                }
                len = strlen(g_msgBuf);
                Str_FormatInt((char)g_boardRules.enable[RULE_PICKED_EVENT],2,g_msgBuf + len);
                TextWin_ClearText(work->win);
                TextWin_AddText(work->win,g_msgBuf);
                TextWin_ClearText(work->id);
                TextWin_AddText(work->id,k_eventPickTexts[(char)g_boardRules.enable[RULE_PICKED_EVENT]]);
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
                return 0;
            }
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            choice = -1;
        }
        else
        {
            Se_Play(SE_OK,0x7f,0x7f,1);
            choice = (char)g_boardRules.enable[RULE_PICKED_EVENT];
        }
        Task_SetUserData(-1,choice);
        *step = EVPICK_CLOSE;
        break;
    case EVPICK_CLOSE:
        TextWin_Close(work->win);
        TextWin_Close(work->id);
        *step += 1;
        break;
    case EVPICK_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING)
        {
            return 0;
        }
        return 1;
    default:
        return 0;
    }
    return 0;
}

/* EventCard_LoadTask steps */
enum EventCardLoadStep
{
    CARDLOAD_START        = 0,  /* read the picture; the card's frame for its points */
    CARDLOAD_FREE_FRAME   = 1,
    CARDLOAD_WAIT_PICTURE = 2,
    CARDLOAD_SCRIPT       = 3   /* then LoadJWithTim_Task for EVE_<id>.J */
};

/* Task: load the picture of the event drawn (C_<n>_.TIM) and the card's frame for its points
 * (g_eventCardPoints: 1-3 at random, 0 for event 0x4C; k_eventCardArchive), then become
 * LoadJWithTim_Task for the event's script (EVE_<n>.J). g_eventDrawn while it loads (0x8006D2E4) */
/* 0x8006D2E4 */
u32 EventCard_LoadTask(u16 *step, EventCardLoadWork *work)
{
    switch (*step)
    {
    case CARDLOAD_START:
        g_eventDrawn = 1;
        work->handle = File_ReadAsync(g_eventId + FILE_C_000__TIM,NULL,FILE_WHOLE,1);
        work->block = SecHeap_Alloc(2);
        if (g_eventId == EVE_MISS)
        {
            g_eventCardPoints = 0;
        }
        else
        {
            g_eventCardPoints = rand() % 3 + 1;
        }
        Lzss_Decompress((int *)ARCHIVE_ENTRY(&k_eventCardArchive,g_eventCardPoints),
                        g_secHeapBlocks[work->block].addr);
        Tim_UploadStream(g_secHeapBlocks[work->block].addr,NULL);
        *step += 1;
        return 0;
    case CARDLOAD_FREE_FRAME:
        SecHeap_Free(work->block);
        *step += 1;
        /* fall through */
    case CARDLOAD_WAIT_PICTURE:
        if (File_IsDone(work->handle) != 1)
        {
            return 0;
        }
        Tim_UploadStream(g_secHeapBlocks[g_fileHandles[work->handle].block].addr,NULL);
        *step += 1;
        return 0;
    case CARDLOAD_SCRIPT:
        SecHeap_Free(g_fileHandles[work->handle].block);
        File_ReleaseHandle(work->handle);
        g_eventDrawn = 0;
        ((LoadJWithTimWork *)work)->file = FILE_EVE_000_J + g_eventId;
        Task_Start(TASK_SELF,LoadJWithTim_Task);
        return 0;
    }
    return 0;
}

/* The cards that are played on a rival, chosen with PlayerSelect_Task */
static int Event_TargetsRival(int card)
{
    switch (card)
    {
    case EV_STOP_DELIVERY:
    case EV_STOP_NEW_STORES:
    case EV_DELIVER_RIVAL:
    case EV_REVERSE:
    case EV_CLOSE_STORE:
    case EV_SLOW_DICE:
    case EV_DICE_E:
    case EV_CAPTURE_STORE:
    case EV_MONEY_3_4:
    case EV_OUT_OF_STOCK_1:
    case EV_SWITCH_PLACES:
    case EV_STOP_RIVAL_MONEY:
    case EV_STORE_RIVAL_MONEY:
    case EV_CLOSE_STORE_LV2:
    case EV_HALVE_POINTS:
    case EV_MONEY_HALF:
    case EV_CLOSE_STORE_LV3:
    case EV_MONEY_TO_1:
    case EV_TAKE_HALF_MONEY:
    case EV_CLOSE_STORE_LV4:
    case EV_TAKE_STORE_LV4:
    case EV_RIVALS_MONEY:
        return 1;
    }
    return 0;
}

/* The card on row `row` of the list scrolled by scroll (the list wraps after the last card) */
static int PointsExchange_Card(int scroll, int row)
{
    int card = scroll + row;

    return (card > EVENT_COUNT - 1) ? card - EVENT_COUNT : card;
}

/* The six rows of cards from work->scroll: dark (palette 2) when the player lacks the points, 1
 * when the card has no use now */
static void PointsExchange_List(PointsExchangeWork *work)
{
    char *palette;
    int card;
    int row;

    g_debugMsgCnt = 0;
    strcpy(g_msgBuf,"%L");
    for (row = 0; row < 6; row++)
    {
        card = PointsExchange_Card(work->scroll,row);
        work->unusable[1 + row] = (g_eventTargets[card] == 0);
        if (g_players[g_curPlayer].points < (char)g_eventCards[card].points)
        {
            palette = "%P02";
        }
        else if (work->unusable[1 + row] == 0)
        {
            palette = "%P00";
        }
        else
        {
            palette = "%P01";
        }
        strcat(g_msgBuf,palette);
        strcat(g_msgBuf,g_eventCards[card].name);
        strcat(g_msgBuf,"%K");
    }
    strcat(g_msgBuf,"%E");
    TextWin_ClearText(work->win);
    TextWin_AddText(work->win,g_msgBuf);
    TextWin_SetArrowUp(work->win,1);
    TextWin_SetArrowDown(work->win,1);
    if (work->scroll == 0)
    {
        TextWin_SetArrowUp(work->win,0);
    }
    if (work->scroll == 47)
    {
        TextWin_SetArrowDown(work->win,0);
    }
}

/* PointsExchange_Task steps */
enum PointsExchangeStep
{
    EXCH_TITLE           =   0,  /* "Points exchange menu." */
    EXCH_WAIT_TITLE      =   1,
    EXCH_OPEN_LIST       =  10,
    EXCH_WAIT_LIST       =  11,
    EXCH_CHOOSE          =  20,
    EXCH_HUMAN           =  30,  /* a human player chooses */
    EXCH_HUMAN_CHOOSE    =  31,
    EXCH_CPU             =  40,  /* a CPU player scrolls to its card */
    EXCH_CPU_SCROLL      =  41,
    EXCH_CPU_SCROLL_WAIT =  42,
    EXCH_CPU_PICK        =  43,  /* 15 frames */
    EXCH_CPU_PICK_WAIT   =  44,
    EXCH_PICKED          =  50,  /* a card played on a rival: EXCH_RIVAL */
    EXCH_CONFIRM         =  51,  /* "Is this event OK?" */
    EXCH_CONFIRM_ANSWER  =  52,
    EXCH_CANCEL          =  60,  /* "Cancel point exchange?" (not reached) */
    EXCH_CANCEL_ANSWER   =  61,
    EXCH_NOT_ENOUGH      =  70,  /* "You don't have enough points." */
    EXCH_WAIT_NOT_ENOUGH =  71,
    EXCH_RIVAL           =  80,  /* PlayerSelect_Task */
    EXCH_WAIT_RIVAL      =  81,
    EXCH_PAY             =  90,  /* pay the points */
    EXCH_CLOSE           = 100,
    EXCH_WAIT_POINTS     = 101,
    EXCH_WAIT_CLOSE      = 102,
    EXCH_WAIT_CAMERA     = 103 
};

/* Task: the points exchange at the bank: a scrolling list of the event cards to buy with points.
 * A human player picks one (cross; circle back to the top), a CPU player scrolls to the one
 * Cpu_ChooseExchange chose; a card played on a rival asks for the player (PlayerSelect_Task),
 * others for confirmation. User data: the card, with the target player in the upper half, or -1
 * (0x8006D4F4) */
/* 0x8006D4F4 */
u32 PointsExchange_Task(u16 *step, PointsExchangeWork *work)
{
    Player *pl = &g_players[g_curPlayer];
    PadState *pad;
    TextWin *win;
    u8 target[8];
    int choice;

    g_debugStep = *step;
    switch (*step)
    {
    case EXCH_TITLE:
        work->shown = 0;
        work->mode = 0;
        work->arrived = 1;
        work->refresh = 0;
        Msg_Show(pl->isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0x5a,MSG_ANY_BUTTON,
                 "%CPoints exchange menu.");
        *step += 1;
        break;
    case EXCH_WAIT_TITLE:
        if (Msg_IsDone())
        {
            *step = EXCH_OPEN_LIST;
        }
        break;
    case EXCH_OPEN_LIST:
        work->scroll = 0;
        work->refresh = 1;
        work->x = pl->x;
        work->mode = 0;
        work->floor = pl->floor;
        work->win = TextWin_Create(0x14,0x20,0x26,6);
        TextWin_Open(work->win);
        TextWin_SetMenu(work->win,0,6,0);
        work->card = work->scroll + g_textWins[work->win].menuCursor;
        Event_ListTargets();
        *step += 1;
        break;
    case EXCH_WAIT_LIST:
        if (g_textWins[work->win].state == TEXTWIN_SHOWN)
        {
            work->shown = 1;
            g_uiTaskWork[6][1] = 0xae;
            g_uiTaskWork[6][2] = 0x14;
            Task_Start(g_uiTaskIds[6],Points_DisplayTask);
            *step = EXCH_CHOOSE;
        }
        break;
    case EXCH_CHOOSE:
        *step = (pl->isCpu != 0) ? EXCH_CPU : EXCH_HUMAN;
        break;

    /* 30-31: a human player chooses */
    case EXCH_HUMAN:
        if (!Msg_IsDone())
        {
            break;
        }
        TextWin_SetFocus(work->win,1);
        work->timeout = 300;
        *step += 1;
        break;
    case EXCH_HUMAN_CHOOSE:
        pad = &g_playerPads[g_curPlayer];
        win = &g_textWins[work->win];
        if (pad->pressed & PADRdown)
        {
            if (pl->points < (char)g_eventCards[work->card].points)
            {
                *step = EXCH_NOT_ENOUGH;
            }
            else if (work->unusable[TextWin_GetCursor(work->win) + 1] == 0)
            {
                work->target = 0;
                if (g_boardRules.enable[RULE_AUTO_EXCHANGE] == 0 && g_boardRules.enable[RULE_AUTO_EVENT] != 0)
                {
                    work->target = Cpu_EventTarget(g_curPlayer,work->card);
                }
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = EXCH_PICKED;
            }
        }
        else if (pad->pressed & PADRright)
        {
            /* back to the top */
            if (work->scroll != 0 || win->menuCursor != 0)
            {
                work->scroll = 0;
                win->menuCursor = 0;
                work->refresh = 1;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        else if (pad->repeat & PADLup)
        {
            if (win->menuCursor >= 1)
            {
                win->menuCursor--;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            else if (work->scroll > 0)
            {
                work->scroll--;
                work->refresh = 1;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            work->timeout = 300;
        }
        else if (pad->repeat & PADLdown)
        {
            if (win->menuCursor <= 4)
            {
                win->menuCursor++;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            else if (work->scroll < 47)
            {
                work->scroll++;
                work->refresh = 1;
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
            work->timeout = 300;
        }
        work->card = PointsExchange_Card(work->scroll,g_textWins[work->win].menuCursor);
        if (work->timeout != 0)
        {
            work->timeout--;
            break;
        }
        /* a while without input: the title again */
        TextWin_SetFocus(work->win,0);
        Msg_Show(pl->isCpu,MSG_AUTO_POS,0x40,0x20,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CPoints exchange menu.");
        *step = EXCH_HUMAN;
        break;

    /* 40-44: a CPU player moves the cursor down to its card, then waits a moment */
    case EXCH_CPU:
        choice = Cpu_ChooseExchange(g_curPlayer,target);
        work->cpuCard = choice;
        g_debugComUseB = target[0];
        g_debugComUseA = (char)choice;
        work->target = target[0];
        *step += 1;
        break;
    case EXCH_CPU_SCROLL:
        if (work->card == work->cpuCard)
        {
            *step = EXCH_CPU_PICK;
            break;
        }
        win = &g_textWins[work->win];
        if (win->menuCursor < 5)
        {
            win->menuCursor++;
        }
        else
        {
            work->scroll++;
            if (work->scroll > EVENT_COUNT - 1)
            {
                work->scroll -= EVENT_COUNT;
            }
            work->refresh = 1;
        }
        work->card = PointsExchange_Card(work->scroll,g_textWins[work->win].menuCursor);
        work->delay = 2;
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case EXCH_CPU_SCROLL_WAIT:
        if (--work->delay == 0)
        {
            *step = EXCH_CPU_SCROLL;
        }
        break;
    case EXCH_CPU_PICK:
        work->delay = 15;
        *step += 1;
        break;
    case EXCH_CPU_PICK_WAIT:
        if (--work->delay == 0)
        {
            *step = EXCH_PICKED;
        }
        break;

    /* 50-52: confirm a card without a rival target */
    case EXCH_PICKED:
        if (Event_TargetsRival(work->card))
        {
            *step = EXCH_RIVAL;
            break;
        }
        *step += 1;
        break;
    case EXCH_CONFIRM:
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            work->isCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(pl->isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                     "%CIs this event OK?%KYes%KNo%E");
        *step += 1;
        break;
    case EXCH_CONFIRM_ANSWER:
        if (!Msg_IsAnswered())
        {
            break;
        }
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            pl->isCpu = work->isCpu;
        }
        TextWin_SetFocus(work->win,1);
        if (g_msgAnswer == 0)
        {
            Task_SetUserData(-1,work->card);
            *step = EXCH_PAY;
            break;
        }
        *step = EXCH_CHOOSE;
        break;

    /* 60-61: cancel the exchange (not reached) */
    case EXCH_CANCEL:
        TextWin_SetFocus(work->win,0);
        Msg_AskYesNo(pl->isCpu,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                     "%CCancel point exchange?%KYes%KNo%E");
        *step += 1;
        break;
    case EXCH_CANCEL_ANSWER:
        if (Msg_IsAnswered())
        {
            TextWin_SetFocus(work->win,1);
            *step = (g_msgAnswer == 0) ? EXCH_CLOSE : EXCH_CHOOSE;
            Task_SetUserData(-1,-1);
        }
        break;

    case EXCH_NOT_ENOUGH:
        TextWin_SetFocus(work->win,0);
        Msg_Show(pl->isCpu,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x96,MSG_ANY_BUTTON,
                 "%CYou don't have enough points.%E");
        *step += 1;
        break;
    case EXCH_WAIT_NOT_ENOUGH:
        if (!Msg_IsDone())
        {
            break;
        }
        TextWin_SetFocus(work->win,1);
        *step = EXCH_CHOOSE;
        break;

    /* 80-81: choose the rival */
    case EXCH_RIVAL:
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            work->isCpu = pl->isCpu;
            pl->isCpu = 1;
        }
        TextWin_SetFocus(work->win,0);
        g_uiTaskWork[5][1] = work->card;
        g_uiTaskWork[5][2] = work->target;
        Task_Start(g_uiTaskIds[5],PlayerSelect_Task);
        *step += 1;
        break;
    case EXCH_WAIT_RIVAL:
        if (!Task_IsDone(g_uiTaskIds[5]))
        {
            break;
        }
        if (g_boardRules.enable[RULE_AUTO_EVENT] != 0)
        {
            pl->isCpu = work->isCpu;
        }
        TextWin_SetFocus(work->win,1);
        choice = Task_GetUserData(g_uiTaskIds[5]);
        if (choice != -1)
        {
            Task_SetUserData(-1,choice << 16 | work->card);
            *step = EXCH_PAY;
            break;
        }
        *step = EXCH_CHOOSE;
        break;

    /* 90-103: pay the points and close */
    case EXCH_PAY:
        if (g_boardRules.startPoints != 0xff)
        {
            pl->points -= g_eventCards[work->card].points;
        }
        *step = EXCH_CLOSE;
        break;
    case EXCH_CLOSE:
        work->mode = 0;
        Task_SendMessage(g_uiTaskIds[6],0);
        *step += 1;
        break;
    case EXCH_WAIT_POINTS:
        if (!Task_IsDone(g_uiTaskIds[6]))
        {
            break;
        }
        TextWin_Close(work->win);
        *step += 1;
        break;
    case EXCH_WAIT_CLOSE:
        if (g_textWins[work->win].state != TEXTWIN_FREEING)
        {
            break;
        }
        work->shown = 0;
        *step += 1;
        break;
    case EXCH_WAIT_CAMERA:
        if (work->arrived != 0)
        {
            return 1;
        }
        break;
    }
    if (work->refresh != 0)
    {
        PointsExchange_List(work);
        work->refresh = 0;
    }
    FntPrint("MsgCnt : %d\n", g_debugMsgCnt);
    work->arrived = Camera_MoveTo((char)work->mode,CAMERA_CELL_X(work->x,8),CAMERA_CELL_Y(work->floor,0x46));
    return 0;
}

/* Event_ApplyTask steps */
enum EventApplyStep
{
    APPLY_INIT           =   0,
    APPLY_EFFECT         =  10,  /* the card's effect: at once, or an Effect_* over several frames */
    APPLY_SHOP_OPEN      =  20,
    APPLY_WAIT_SHOP_OPEN =  21,
    APPLY_DONE           = 100 
};

/* Task: carry out an event card's effect (work->event, on work->player): at once, or through an
 * Effect_* function over several frames. User data: work->result. The third argument is only
 * read for an unknown card: the original takes whatever the scheduler left in that register
 * (0x8006E3EC) */
/* 0x8006E3EC */
u32 Event_ApplyTask(u16 *step, EventApplyWork *work, int arg)
{
    Player *cur = &g_players[g_curPlayer];
    Player *target = &g_players[work->player];
    short until;
    char picked[8];
    int count;
    int left;
    int seen;
    int done;
    int r;
    int i;
    int j;
    int k;

    /* (the original also copies the player color names k_eventApplyTable into an unused local) */
    switch (*step)
    {
    case APPLY_INIT:
        work->showShops = 0;
        work->result = 0;
        work->effectStep = 0;
        *step = APPLY_EFFECT;
        return 0;
    case APPLY_EFFECT:
        break;
    case APPLY_SHOP_OPEN:
        Task_Start(g_actionTaskIds[1],ShopOpen_Task);
        *step += 1;
        return 0;
    case APPLY_WAIT_SHOP_OPEN:
        if (!Task_IsDone(g_actionTaskIds[1]))
        {
            return 0;
        }
        *step = APPLY_DONE;
        return 0;
    case APPLY_DONE:
        Task_SetUserData(-1,work->result != 0);
        return 1;
    default:
        return 0;
    }

    /* timed effects last until the same player's turn two months on (four for the prices) */
    until = TURN_NUMBER((short)g_gameConfig.monthsElapsed + 2,g_curPlayer);
    done = 1;
    switch ((u16)work->event)
    {
    case EV_EARN_POINT:
        cur->points++;
        if (cur->points > 98)
        {
            cur->points = 99;
        }
        break;
    case EV_MONEY_200:
        Player_Earn(g_curPlayer,200);
        break;
    case EV_STOP_DELIVERY:
        target->noDelivery = 1;
        target->noDeliveryUntil = until;
        break;
    case EV_STOP_NEW_STORES:
        target->vandalized = 1;
        target->vandalizedUntil = until;
        break;
    case EV_MONEY_400:
        Player_Earn(g_curPlayer,400);
        break;
    case EV_DELIVER_OWN:
    case EV_DELIVER_ALL_OWN:
    case EV_DELIVER_FREE:
        done = Effect_Delivery(&work->effectStep,(EffectDeliveryWork *)work->effectWork,work->event,0);
        break;
    case EV_DELIVER_RIVAL:
        done = Effect_Delivery(&work->effectStep,(EffectDeliveryWork *)work->effectWork,work->event,
                               work->player);
        break;
    case EV_REVERSE:
        target->dirX = -target->dirX;
        target->dirFloor = -target->dirFloor;
        break;
    case EV_CLOSE_STORE:
        done = Effect_CloseChain(&work->effectStep,(EffectCloseChainWork *)work->effectWork,
                                 work->player,0);
        break;
    case EV_MONEY_700:
        Player_Earn(g_curPlayer,700);
        break;
    case EV_FREE_STORE:
        done = Effect_OpenStoreNewSquare(&work->effectStep,(EffectOpenStoreNewSquareWork *)work->effectWork,-1);
        break;
    case EV_SLOW_DICE:
        target->diceEffect = 1;
        target->diceEffectUntil = until;
        break;
    case EV_DICE_E:
        target->diceEffect = 2;
        target->diceEffectUntil = until;
        break;
    case EV_MONEY_STORES_150:
        Stores_ListByOwner(g_curPlayer);
        Player_Earn(g_curPlayer,g_storeListCount * 150);
        break;
    case EV_CAPTURE_STORE:
        done = Effect_RebuildClosed(&work->effectStep,(EffectRebuildClosedWork *)work->effectWork,
                                    work->player);
        break;
    case EV_LOWER_PENCIL:
        cur->diceEffect = 3;
        cur->diceEffectUntil = until;
        break;
    case EV_MONEY_3_4:
        target->money = target->money * 3 / 4;
        break;
    case EV_OUT_OF_STOCK_1:
        /* one of the rival's stores with goods, at random, without them */
        Stores_ListOperating(work->player);
        work->store = g_storeList[rand() % g_storeListCount];
        Store_ClearGoods(work->store);
        break;
    case EV_MONEY_1000:
        Player_Earn(g_curPlayer,1000);
        break;
    case EV_SWITCH_PLACES:
        done = Effect_SwitchPlaces(&work->effectStep,(u32)work->effectWork,work->player);
        work->result = 1;
        if (done)
        {
            *step = (work->showShops == 0) ? APPLY_DONE : APPLY_SHOP_OPEN;
        }
        return 0;
    case EV_ALL_REVERSE:
        for (i = 0; i < g_numPlayers; i++)
        {
            if (g_players[i].out == 0)
            {
                g_players[i].dirX = -g_players[i].dirX;
                g_players[i].dirFloor = -g_players[i].dirFloor;
            }
        }
        break;
    case EV_MONEY_DOUBLE:
        cur->paydayEffect = 2;
        break;
    case EV_STOP_RIVAL_MONEY:
        target->paydayEffect = 1;
        break;
    case EV_SHOES:
        cur->shoes = 1;
        break;
    case EV_MONEY_1300:
        Player_Earn(g_curPlayer,1300);
        break;
    case EV_STORE_RIVAL_MONEY:
        done = Effect_OpenStoreNewSquare(&work->effectStep,(EffectOpenStoreNewSquareWork *)work->effectWork,
                                   work->player);
        break;
    case EV_CUT_BY_STORES_150:
        for (k = 0; k < g_numPlayers; k++)
        {
            if (k != g_curPlayer && g_players[k].out == 0)
            {
                Stores_ListByOwner(k);
                Player_Pay(k,g_storeListCount * 150);
            }
        }
        break;
    case EV_CLOSE_OUT_OF_STOCK:
        done = Effect_DemolishClosed(&work->effectStep,(EffectDemolishClosedWork *)work->effectWork);
        break;
    case EV_CLOSE_STORE_LV2:
        done = Effect_CloseChain(&work->effectStep,(EffectCloseChainWork *)work->effectWork,
                                 work->player,1);
        break;
    case EV_MONEY_1600:
        Player_Earn(g_curPlayer,1600);
        break;
    case EV_ITEMS_DOUBLE:
        /* the current player's stores sell at 200% for four months */
        Stores_ListByOwner(WHO_CURRENT);
        for (j = 0; j < g_storeListCount; j++)
        {
            work->store = g_storeList[j];
            g_stores[work->store].priceChanged = 1;
            g_stores[work->store].pricePercent = 200;
            g_stores[work->store].priceUntil = TURN_NUMBER((short)g_gameConfig.monthsElapsed + 4,
                                                           g_curPlayer);
        }
        break;
    case EV_OUT_OF_STOCK_4:
        /* up to four of the rivals' stores with goods, at random, without them */
        Stores_ListOperating(WHO_OTHERS);
        count = (g_storeListCount < 4) ? g_storeListCount : 4;
        for (j = 0; j < count; j++)
        {
            r = rand();
            left = g_storeListCount - j;
            seen = 0;
            for (k = 0; k < g_storeListCount; k++)
            {
                for (i = 0; i < j && picked[i] != k; i++)
                {
                }
                if (i < j)
                {
                    continue;                         /* already picked */
                }
                if ((short)seen != (short)(r % left))
                {
                    seen++;
                    continue;
                }
                picked[j] = k;
                work->store = g_storeList[k];
                Store_ClearGoods(work->store);
                break;
            }
        }
        break;
    case EV_MONEY_STORES_300:
        Stores_ListByOwner(g_curPlayer);
        Player_Earn(g_curPlayer,g_storeListCount * 300);
        break;
    case EV_DELIVER_CHEAP:
        /* every free slot of the player's stores gets goods of level 5 */
        Stores_ListByOwner(g_curPlayer);
        for (j = 0; j < g_storeListCount; j++)
        {
            work->store = g_storeList[j];
            for (i = 0; i < g_stores[work->store].maxGoods; i++)
            {
                if (g_stores[work->store].goods[i].active == 0)
                {
                    g_stores[work->store].goods[i].active = 1;
                    g_stores[work->store].goods[i].level = 5;
                }
            }
            Store_SortGoods(work->store);
        }
        break;
    case EV_DELIVER_EXPENSIVE:
        /* the first free slot of each of the player's stores with room gets the most expensive goods
         * its chain allows */
        Stores_ListNotFull(g_curPlayer);
        for (j = 0; j < g_storeListCount; j++)
        {
            work->store = g_storeList[j];
            for (i = 0; i < g_stores[work->store].maxGoods; i++)
            {
                if (g_stores[work->store].goods[i].active == 0)
                {
                    g_stores[work->store].goods[i].active = 1;
                    g_stores[work->store].goods[i].level =
                         6 - k_chainLevels[g_stores[work->store].chainLen];
                    break;
                }
            }
            Store_SortGoods(work->store);
        }
        break;
    case EV_HALVE_POINTS:
        target->points >>= 1;
        break;
    case EV_MONEY_HALF:
        target->money /= 2;
        break;
    case EV_MONEY_SHUFFLE:
        done = Effect_ShuffleMoney();
        break;
    case EV_ALL_OUT_OF_STOCK:
        done = Effect_TakeOverClosed(&work->effectStep,(EffectTakeOverClosedWork *)work->effectWork);
        break;
    case EV_CLOSE_STORE_LV3:
        done = Effect_CloseChain(&work->effectStep,(EffectCloseChainWork *)work->effectWork,
                                 work->player,2);
        break;
    case EV_HALVE_ALL:
        for (i = 0; i < g_numPlayers; i++)
        {
            if (i != g_curPlayer && g_players[i].out == 0)
            {
                g_players[i].money /= 2;
            }
        }
        break;
    case EV_CUT_BY_STORES_300:
        for (k = 0; k < g_numPlayers; k++)
        {
            if (k != g_curPlayer && g_players[k].out == 0)
            {
                Stores_ListByOwner(k);
                Player_Pay(k,g_storeListCount * 300);
            }
        }
        break;
    case EV_MONEY_TO_1:
        target->money = 1;
        break;
    case EV_TAKE_HALF_MONEY:
        Player_Earn(g_curPlayer,target->money / 2);
        Player_Pay(work->player,target->money / 2);
        break;
    case EV_CLOSE_STORE_LV4:
        done = Effect_CloseChain(&work->effectStep,(EffectCloseChainWork *)work->effectWork,
                                 work->player,3);
        break;
    case EV_UPGRADE_STORE:
        done = Effect_TakeOverChain(&work->effectStep,(EffectTakeOverChainWork *)work->effectWork);
        break;
    case EV_DELIVER_COSTLY:
        /* every free slot of the player's stores gets the most expensive goods its chain allows */
        Stores_ListByOwner(g_curPlayer);
        for (j = 0; j < g_storeListCount; j++)
        {
            work->store = g_storeList[j];
            for (i = 0; i < g_stores[work->store].maxGoods; i++)
            {
                if (g_stores[work->store].goods[i].active == 0)
                {
                    g_stores[work->store].goods[i].active = 1;
                    g_stores[work->store].goods[i].level =
                         6 - k_chainLevels[g_stores[work->store].chainLen];
                }
            }
            Store_SortGoods(work->store);
        }
        break;
    case EV_ALL_MONEY_TO_1:
        for (i = 0; i < g_numPlayers; i++)
        {
            if (i != g_curPlayer && g_players[i].out == 0)
            {
                g_players[i].money = 1;
            }
        }
        break;
    case EV_TAKE_STORE_LV4:
        done = Effect_StealChain(&work->effectStep,(EffectStealChainWork *)work->effectWork,
                                 work->player);
        break;
    case EV_RIVALS_MONEY:
        Player_Earn(g_curPlayer,target->money);
        break;
    case EV_CLAIM_VICTORY:
        cur->money += g_gameConfig.goalMoney - cur->assets;
        work->result = 1;
        break;
    default:
        done = arg;
        break;
    }
    if (done)
    {
        *step = (work->showShops == 0) ? APPLY_DONE : APPLY_SHOP_OPEN;
    }
    return 0;
}

/* PlayerSelect_Task steps */
enum PlayerSelectStep
{
    PSEL_OPEN          =   0,  /* the list of the rivals (the ones the card can't target dimmed) */
    PSEL_WAIT_OPEN     =   1,
    PSEL_HUMAN         =  10,  /* up / down, cross picks, circle cancels */
    PSEL_CPU           =  20,  /* a CPU player moves to its choice */
    PSEL_CPU_MOVE      =  21,
    PSEL_CPU_MOVE_WAIT =  22,
    PSEL_CPU_PICK      =  23,  /* 15 frames */
    PSEL_CPU_PICK_WAIT =  24,
    PSEL_CHOSEN        =  50,
    PSEL_CLOSE         = 100,
    PSEL_WAIT_CLOSE    = 101 
};

/* Task: menu of the players' names to pick a target player (CPU players pick by themselves)
 * (0x8006FA50) */
/* 0x8006FA50 */
u32 PlayerSelect_Task(short *step, PlayerSelectWork *work)
{
    int unusable;
    int *playerPanelWork;
    short win;
    int cursor;
    int id;
    u32 i;
    Player *pl;

    playerPanelWork = g_playerPanelWork;
    switch(*step)
    {
    case PSEL_OPEN:
        work->shown = 1;
        work->skipSelf = 1;
        work->count = 0;
        memcpy(g_msgBuf, "%C", 3);
        for (i = 0; i < g_numPlayers; i++)
        {
            pl = &g_players[i];
            if ((pl->out == 0) && ((work->skipSelf != 1 || (i != g_curPlayer))))
            {
                unusable = ((g_eventTargets[work->event] >> i) & 1) == 0;
                if (work->count != 0)
                {
                    strcat(g_msgBuf,"%K");
                }
                strcat(g_msgBuf,unusable ? "%P01" : "%P00");
                strcat(g_msgBuf,g_players[i].name);
                /* (from choices[-1]: read back as choices[cursor - 1]) */
                work->choices[work->count - 1] = i;
                if (unusable)
                {
                    work->choices[work->count - 1] |= 0x80;
                }
                work->count++;
            }
        }
        strcat(g_msgBuf,"%E");
        win = TextWin_Create(-1000,-1000,0xc,work->count);
        work->id = win;
        TextWin_AddText(work->id,g_msgBuf);
        TextWin_Open(work->id);
        TextWin_SetMenu(work->id,0,work->count,0);
        *step += 1;
        break;
    case PSEL_WAIT_OPEN:
        if (g_textWins[work->id].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        *g_playerPanelWork = 1;
        playerPanelWork[1] = work->panelMode;
        TextWin_SetFocus(work->id,1);
        if (g_players[g_curPlayer].isCpu == 0)
        {
            *step = PSEL_HUMAN;
        }
        else
        {
            *step = PSEL_CPU;
        }
        break;
    case PSEL_HUMAN:
        if ((g_playerPads[g_curPlayer].pressed & PADRdown) == 0)
        {
            if ((g_playerPads[g_curPlayer].pressed & PADRright) == 0)
            {
                if ((g_playerPads[g_curPlayer].repeat & PADLup) == 0)
                {
                    if ((g_playerPads[g_curPlayer].repeat & PADLdown) != 0)
                    {
                        TextWin_CursorDown(work->id);
                    }
                }
                else
                {
                    TextWin_CursorUp(work->id);
                }
            }
            else
            {
                Task_SetUserData(-1,-1);
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
                *step = PSEL_CLOSE;
            }
        }
        else
        {
            cursor = TextWin_GetCursor(work->id);
            if ((work->choices[cursor - 1] & 0x80) == 0)
            {
                cursor = TextWin_GetCursor(work->id);
                Task_SetUserData(-1,work->choices[cursor - 1]);
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = PSEL_CHOSEN;
            }
        }
        cursor = TextWin_GetCursor(work->id);
        g_playerPanelWork[1] = work->choices[cursor - 1];
        break;
    case PSEL_CPU:
        if (work->exclude == -1)
        {
            Task_SetUserData(-1,-1);
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = PSEL_CLOSE;
            break;
        }
        *step += 1;
        break;
    case PSEL_CPU_MOVE:
        id = work->id;
        if (work->choices[g_textWins[id].menuCursor - 1] == work->exclude)
        {
            *step = PSEL_CPU_PICK;
            return 0;
        }
        g_textWins[id].menuCursor++;
        work->delay = 4;
        cursor = TextWin_GetCursor(work->id);
        g_playerPanelWork[1] = work->choices[cursor - 1];
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
        *step += 1;
        break;
    case PSEL_CPU_MOVE_WAIT:
        work->delay--;
        if (work->delay == 0)
        {
            *step = PSEL_CPU_MOVE;
        }
        break;
    case PSEL_CPU_PICK:
        work->delay = 0xf;
        *step += 1;
        break;
    case PSEL_CPU_PICK_WAIT:
        work->delay--;
        if (work->delay == 0)
        {
            work->result = work->exclude;
            *step = PSEL_CHOSEN;
            Se_Play(SE_OK,0x7f,0x7f,1);
            return 0;
        }
        break;
    case PSEL_CHOSEN:
        cursor = TextWin_GetCursor(work->id);
        Task_SetUserData(-1,work->choices[cursor - 1]);
        *step = PSEL_CLOSE;
        break;
    case PSEL_CLOSE:
        *g_playerPanelWork = 0;
        playerPanelWork[1] = work->panelMode;
        TextWin_SetFocus(work->id,0);
        TextWin_Close(work->id);
        *step += 1;
        break;
    case PSEL_WAIT_CLOSE:
        if (g_textWins[work->id].state == TEXTWIN_FREEING)
        {
            work->shown = 0;
            return 1;
        }
        return 0;
    }
    return 0;
}

/* What a rival (another player still in) needs to be a card's target (Event_RivalTargets) */
#define RIVAL_ANY       0
#define RIVAL_DELIVERS  1   /* its deliveries not stopped */
#define RIVAL_BUILDS    2   /* its new stores not stopped */
#define RIVAL_HAS_ROOM  3   /* a store with free goods slots */
#define RIVAL_MOVING    4   /* a direction of movement */
#define RIVAL_CHAINS    5   /* the head of a chain of chainLen `arg` */
#define RIVAL_CLOSED    6   /* a store without goods */
#define RIVAL_SELLING   7   /* a store with goods */
#define RIVAL_PAID      8   /* its paydays not stopped */

/* The rivals that have what `need` says, as a bit mask */
static u8 Event_RivalTargets(int need, int arg)
{
    Player *pl;
    u32 owned;
    u8 targets;
    int ok;
    int i;

    targets = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        pl = &g_players[i];
        if (i == g_curPlayer || pl->out != 0)
        {
            continue;
        }
        switch (need)
        {
        case RIVAL_DELIVERS:
            ok = (pl->noDelivery == 0);
            break;
        case RIVAL_BUILDS:
            ok = (pl->vandalized == 0);
            break;
        case RIVAL_HAS_ROOM:
            ok = (Stores_ListNotFull(i) != 0);
            break;
        case RIVAL_MOVING:
            ok = (pl->dirX != 0);
            break;
        case RIVAL_CHAINS:
            ok = (Stores_ListChainHeads(i,arg) != 0);
            break;
        case RIVAL_CLOSED:
            ok = (Stores_ListClosed(i) != 0);
            break;
        case RIVAL_SELLING:
            owned = Stores_ListByOwner(i);
            ok = (owned != Stores_ListClosed(i));
            break;
        case RIVAL_PAID:
            ok = (pl->paydayEffect != 1);
            break;
        default:
            ok = 1;
            break;
        }
        if (ok)
        {
            targets |= 1 << i;
        }
    }
    return targets;
}

/* Every player (a card usable without a target) */
#define TARGETS_ALL  0x3f

/* For every event card, the players it can be used on now (g_eventTargets, bit masks): none when
 * the current player lacks the points; for the cards played on a rival, the rivals it would do
 * something to; for the others, everyone when the card would have an effect (0x80070060) */
/* 0x80070060 */
void Event_ListTargets(void)
{
    u8 *targets;
    u32 owned;
    int event;

    for (event = 0; event < EVENT_COUNT; event++)
    {
        targets = &g_eventTargets[event];
        *targets = 0;
        if (g_players[g_curPlayer].points < (char)g_eventCards[event].points)
        {
            continue;
        }
        switch (event)
        {
        case EV_STOP_DELIVERY:
            *targets = Event_RivalTargets(RIVAL_DELIVERS,0);
            break;
        case EV_STOP_NEW_STORES:
            *targets = Event_RivalTargets(RIVAL_BUILDS,0);
            break;
        case EV_DELIVER_OWN:
            if (Stores_ListNotFull(g_curPlayer) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_DELIVER_RIVAL:
            *targets = Event_RivalTargets(RIVAL_HAS_ROOM,0);
            break;
        case EV_REVERSE:
            *targets = Event_RivalTargets(RIVAL_MOVING,0);
            break;
        case EV_CLOSE_STORE:
            *targets = Event_RivalTargets(RIVAL_CHAINS,0);
            break;
        case EV_FREE_STORE:
            if (Stores_ListVacantNewSquares(g_curPlayer) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_SLOW_DICE:
        case EV_DICE_E:
        case EV_MONEY_3_4:
        case EV_SWITCH_PLACES:
        case EV_HALVE_POINTS:
        case EV_MONEY_HALF:
        case EV_MONEY_TO_1:
        case EV_TAKE_HALF_MONEY:
        case EV_RIVALS_MONEY:
            *targets = Event_RivalTargets(RIVAL_ANY,0);
            break;
        case EV_MONEY_STORES_150:
        case EV_ITEMS_DOUBLE:
        case EV_MONEY_STORES_300:
            if (Stores_ListByOwner(WHO_CURRENT) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_CAPTURE_STORE:
            *targets = Event_RivalTargets(RIVAL_CLOSED,0);
            break;
        case EV_OUT_OF_STOCK_1:
            *targets = Event_RivalTargets(RIVAL_SELLING,0);
            break;
        case EV_STOP_RIVAL_MONEY:
            *targets = Event_RivalTargets(RIVAL_PAID,0);
            break;
        case EV_SHOES:
            if (g_players[g_curPlayer].shoes == 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_STORE_RIVAL_MONEY:
            Stores_ListVacantNewSquares(g_curPlayer);
            if (g_storeListCount != 0)
            {
                *targets = Event_RivalTargets(RIVAL_ANY,0);
            }
            break;
        case EV_DELIVER_ALL_OWN:
            if ((int)Stores_ListNotFull(WHO_CURRENT) > 1)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_CUT_BY_STORES_150:
        case EV_CUT_BY_STORES_300:
            if (Stores_ListByOwner(WHO_OTHERS) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_CLOSE_OUT_OF_STOCK:
        case EV_ALL_OUT_OF_STOCK:
            if (Stores_ListClosed(WHO_OTHERS) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_CLOSE_STORE_LV2:
            *targets = Event_RivalTargets(RIVAL_CHAINS,1);
            break;
        case EV_OUT_OF_STOCK_4:
            owned = Stores_ListByOwner(WHO_OTHERS);
            if ((int)(owned - Stores_ListClosed(WHO_OTHERS)) > 2)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_DELIVER_FREE:
        case EV_DELIVER_CHEAP:
        case EV_DELIVER_EXPENSIVE:
        case EV_DELIVER_COSTLY:
            if (Stores_ListNotFull(WHO_CURRENT) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        case EV_CLOSE_STORE_LV3:
            *targets = Event_RivalTargets(RIVAL_CHAINS,2);
            break;
        case EV_CLOSE_STORE_LV4:
        case EV_TAKE_STORE_LV4:
            *targets = Event_RivalTargets(RIVAL_CHAINS,3);
            break;
        case EV_UPGRADE_STORE:
            if (Stores_ListExtendable(WHO_CURRENT) != 0)
            {
                *targets = TARGETS_ALL;
            }
            break;
        default:
            *targets = TARGETS_ALL;
            break;
        }
    }
}

/* The random event of the player's turn: the first row of g_eventWeights whose threshold for the
 * player's bracket (see EventWeights) is above a random number 0-199; 99 is the month's event
 * (g_monthEvents) (0x8008CC4C) */
/* 0x8008CC4C */
u32 Event_PickRandom(u32 player)
{
    Player *pl = &g_players[player & 0xffff];
    int leader;
    int behind;
    int bracket;
    short r;
    u32 event;
    int i;

    leader = 0;
    for (i = 0; i < g_numPlayers; i++)
    {
        if (g_players[i].out == 0 && leader < g_players[i].assets)
        {
            leader = g_players[i].assets;
        }
    }
    behind = leader - pl->assets;
    if (pl->assets >= g_gameConfig.goalMoney / 2)
    {
        bracket = 7;
    }
    else if (g_gameConfig.monthsElapsed < 12)
    {
        bracket = 0;
    }
    else if (behind <= 0)
    {
        bracket = 1;
    }
    else if (behind <= 1500)
    {
        bracket = 2;
    }
    else if (behind <= 3000)
    {
        bracket = 3;
    }
    else if (behind <= 6000)
    {
        bracket = 4;
    }
    else if (behind <= 10000)
    {
        bracket = 5;
    }
    else
    {
        bracket = 6;
    }
    r = rand() % 200;
    event = EVE_NOT_DRAWN;
    for (i = 0; i < 49; i++)
    {
        if (r < g_eventWeights[i].below[bracket])
        {
            event = g_eventWeights[i].event;
            break;
        }
    }
    if (event == EVE_NOT_DRAWN)
    {
        event = g_monthEvents[(char)g_gameConfig.month];
    }
    return event;
}
