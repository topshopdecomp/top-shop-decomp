/*
 * setup.c - the free-play and story set-up screens
 *
 * The screens a new game goes through: the number of players, the player list, the character
 * wheel, name entry, the stage, the money rules (starting money, goal, bankruptcies) and the
 * turn order; and the players of a story stage.
 */
#include "common.h"
#include "topshop.h"

/* PlayerCount_Task steps */
enum PlayerCountStep
{
    PCOUNT_INIT           =   0,
    PCOUNT_ITEM_IN        =   1,  /* the items come in two frames apart */
    PCOUNT_ITEM_DELAY     =   2,
    PCOUNT_WAIT_ITEMS     =   3,
    PCOUNT_FADE_IN        =  10,
    PCOUNT_HUMANS         =  20,  /* the human count */
    PCOUNT_CPU_START      =  30,
    PCOUNT_CPU_FADE_IN    =  31,
    PCOUNT_CPUS           =  40,  /* the CPU count (none to choose with six humans) */
    PCOUNT_CPU_BACK       =  50,
    PCOUNT_CPU_FADE_OUT   =  51,
    PCOUNT_FADE_OUT       = 100,  /* the characters run off for good, the bands fade */
    PCOUNT_ITEM_OUT       = 101,
    PCOUNT_ITEM_OUT_DELAY = 102,
    PCOUNT_WAIT_ITEMS_OUT = 103,
    PCOUNT_WAIT_RUN_OFF   = 104,
    PCOUNT_DONE           = 105 
};

/* Number of human players (1-6) and of CPU players (to make 2-6 players in all; 4 by default):
 * GameConfig.numHumans / numCpus / numPlayers, the players' isCpu. A character for each count
 * runs off when it changes and comes back holding up the new number. g_menuResult: 0 done, 1
 * back (0x800392CC) */
/* 0x800392CC */
u32 PlayerCount_Task(short *step, PlayerCountWork *work)
{
    MenuItem *item;
    int arrived;
    int i;
    int k;

    switch (*step)
    {
    case PCOUNT_INIT:
        work->shown = 1;
        work->pos[0].y = 96;
        work->pos[1].y = 0xa0;
        work->fade[0] = 0x100;
        work->fade[1] = 0x100;
        work->running[0] = 0;
        work->pos[0].x = -0x28;
        work->running[1] = 0;
        work->pos[1].x = -0x28;
        work->moving = 0;
        work->value[0] = 0;
        work->value[1] = 0;
        work->count = 13;
        for (i = 0; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->state = MENUITEM_STILL;
            item->offsetY = 0;
            if (i == 0)
            {
                item->sprite = 0x46;
                item->x = 0x10;
                item->y = 0x10;
            }
            else if (i < 7)
            {
                /* the human counts 1-6 */
                item->sprite = i + 0x32;
                item->x = 0x30;
                item->y = 0x2c + i * 12;
            }
            else if (i < 13)
            {
                /* the CPU counts 0-5 */
                item->sprite = i + 0x26;
                item->x = 200;
                item->y = 0x32 + i * 12;
            }
        }
        work->item = 0;
        *step += 1;
        break;
    case PCOUNT_ITEM_IN:
        /* the items come in two frames apart */
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = PCOUNT_WAIT_ITEMS;
        break;
    case PCOUNT_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = PCOUNT_ITEM_IN;
        }
        break;
    case PCOUNT_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = PCOUNT_FADE_IN;
        }
        break;
    case PCOUNT_FADE_IN:
        work->fade[0] -= 0x40;
        if (work->fade[0] < 1)
        {
            work->running[0] = 1;
            work->timer[0] = 1;
            work->fade[0] = 0;
            *step = PCOUNT_HUMANS;
        }
        break;
    case PCOUNT_HUMANS:
        /* the human count */
        work->timer[1] = 40;
        if (g_playerPads[6].pressed & PADRdown)
        {
            g_gameConfig.numHumans = work->value[0] + 1;
            *step = PCOUNT_CPU_START;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].pressed & PADRright)
        {
            g_menuResult = MENU_RESULT_BACK;
            *step = PCOUNT_FADE_OUT;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            work->value[0] = (work->value[0] == 0) ? 5 : work->value[0] - 1;
            work->timer[0] = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            work->value[0] = (work->value[0] == 5) ? 0 : work->value[0] + 1;
            work->timer[0] = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case PCOUNT_CPU_START:
        work->moving = 1;
        work->minCpus = 2 - g_gameConfig.numHumans;
        if ((char)(2 - g_gameConfig.numHumans) < 0)
        {
            work->minCpus = 0;
        }
        work->maxCpus = 6 - g_gameConfig.numHumans;
        work->value[1] = (4 - (char)g_gameConfig.numHumans < 0) ? work->minCpus : 4 - g_gameConfig.numHumans;
        *step += 1;
        break;
    case PCOUNT_CPU_FADE_IN:
        work->fade[1] -= 0x40;
        if (work->fade[1] < 1)
        {
            work->running[1] = 1;
            work->timer[1] = 1;
            work->fade[1] = 0;
            *step = PCOUNT_CPUS;
        }
        break;
    case PCOUNT_CPUS:
        /* the CPU count (none to choose with six humans) */
        if ((g_playerPads[6].pressed & PADRdown) || g_gameConfig.numHumans == 6)
        {
            g_gameConfig.numCpus = work->value[1];
            g_gameConfig.numPlayers = g_gameConfig.numHumans + g_gameConfig.numCpus;
            for (i = 0; i < g_gameConfig.numPlayers; i++)
            {
                g_gameConfig.players[i].isCpu = (i >= (char)g_gameConfig.numHumans);
            }
            g_menuResult = MENU_RESULT_NEXT;
            *step = PCOUNT_FADE_OUT;
            Se_Play(SE_OK,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].pressed & PADRright)
        {
            *step = PCOUNT_CPU_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            work->value[1] = (work->value[1] == work->minCpus) ? work->maxCpus : work->value[1] - 1;
            work->timer[1] = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            work->value[1] = (work->value[1] == work->maxCpus) ? work->minCpus : work->value[1] + 1;
            work->timer[1] = 40;
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case PCOUNT_CPU_BACK:
        work->moving = 0;
        *step += 1;
        break;
    case PCOUNT_CPU_FADE_OUT:
        work->fade[1] += 0x40;
        if (work->fade[1] > 0xff)
        {
            work->fade[1] = 0x100;
            *step = PCOUNT_HUMANS;
        }
        break;
    case PCOUNT_FADE_OUT:
        /* the characters run off for good, the bands fade */
        work->timer[0] = 0x7fffffff;
        work->timer[1] = 0x7fffffff;
        arrived = 1;
        for (k = 0; k < 2; k++)
        {
            work->fade[k] += 0x40;
            if (work->fade[k] < 0x100)
            {
                arrived = 0;
            }
            else
            {
                work->fade[k] = 0x100;
            }
        }
        if (arrived)
        {
            *step += 1;
        }
        work->item = 0;
        break;
    case PCOUNT_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        work->item++;
        if (work->item != work->count)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = PCOUNT_WAIT_ITEMS_OUT;
        break;
    case PCOUNT_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = PCOUNT_ITEM_OUT;
        }
        break;
    case PCOUNT_WAIT_ITEMS_OUT:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step += 1;
        }
        break;
    case PCOUNT_WAIT_RUN_OFF:
        if (work->pos[1].x < -0x27 && work->pos[0].x < -0x27)
        {
            *step += 1;
        }
        break;
    case PCOUNT_DONE:
        work->shown = 0;
        return 1;
    }

    if (work->shown != 0)
    {
        Menu_DrawBand(work->value[0] * 12 + 0x38,12,work->fade[0]);
        Menu_DrawBand(work->value[1] * 12 + 0x86,12,work->fade[1]);
        if (work->moving == 0 ? work->fade[0] == 0 : work->fade[1] == 0)
        {
            g_drawPos[0] = 0x18;
            g_drawPos[1] = (work->moving == 0) ? work->value[0] * 12 + 0x36 : work->value[1] * 12 + 0x84;
            Sprite_Draw(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
        }
        MenuItems_Draw(work->count);
    }
    for (k = 0; k < 2; k++)
    {
        if (work->running[k] == 0)
        {
            continue;
        }
        if (work->timer[k] == 0)
        {
            /* in: it stops at x 0xf0 (humans) / 0x50 (CPUs) */
            work->pos[k].x -= 8;
            if (k == 0)
            {
                if (work->pos[0].x < 0xf1)
                {
                    work->pos[0].x = 0xf0;
                }
            }
            else if (work->pos[k].x < 0x51)
            {
                work->pos[k].x = 0x50;
            }
        }
        else if (--work->timer[k] == 0)
        {
            work->pos[k].x = 0x168;
            work->shownValue[k] = work->value[k];
        }
        else
        {
            work->pos[k].x -= 8;
            if (work->pos[k].x < -0x27)
            {
                work->pos[k].x = -0x28;
            }
        }
        g_drawPos[0] = work->pos[k].x;
        g_drawPos[1] = work->pos[k].y;
        Sprite_DrawAligned(GROUP_SPRITE(5,(k == 0) ? work->shownValue[0] + 0xaa : work->shownValue[1] + 0xa4),
                           0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
    }
    return 0;
}

/* A random character for a CPU player: one story mode has opened that nobody has taken */
static int PlayerMenu_RandomChar(void)
{
    int count;
    int pick;
    int seen;
    int c;

    count = 0;
    for (c = 0; c < 14; c++)
    {
        if (g_charTaken[c] == 0 && g_progress.charOpen[c] != 0)
        {
            count++;
        }
    }
    pick = rand() % count;
    seen = 0;
    for (c = 0; c < 14; c++)
    {
        if (g_charTaken[c] == 0 && g_progress.charOpen[c] != 0)
        {
            if (seen == pick)
            {
                break;
            }
            seen++;
        }
    }
    return c;
}

/* The y of a row of the player list: the slots from 0x3c down, "OK" (row `players`) at 0xcc */
#define PLAYERMENU_ROW_Y(work, row)  ((row) == (work)->players ? 0xcc : 0x3c + (row) * 0x18)

/* PlayerMenu_Task steps */
enum PlayerMenuStep
{
    PMENU_INIT            =   0,
    PMENU_ITEM_IN         =   1,  /* the items come in two frames apart */
    PMENU_ITEM_DELAY      =   2,
    PMENU_WAIT_ITEMS      =   3,
    PMENU_WALK_IN_START   =   5,  /* the players' characters */
    PMENU_WALK_IN         =   6,  /* they walk in and stand */
    PMENU_FADE_IN         =  10,
    PMENU_CHOOSE          =  20,  /* a player: its pad (cross), or the character */
    PMENU_FADE_OUT        = 100,
    PMENU_LEAVE           = 101,  /* (goes on) */
    PMENU_WALK_OUT        = 102,  /* they walk out to the left */
    PMENU_ITEMS_OUT_START = 103,
    PMENU_ITEM_OUT        = 104,
    PMENU_ITEM_OUT_DELAY  = 105,
    PMENU_WAIT_ITEMS_OUT  = 106,
    PMENU_DONE            = 107 
};

/* The player list of the setup screens (debug name task_player_menu): a row per player slot
 * filled so far (its character walks in, with its pad: animated for a human), then "OK" once all
 * are. When every human has a character the CPU players get random ones (g_cpuNames). A human
 * slot's row is taken with cross on the pad that will play it (Pad_AssignPressed). g_menuResult:
 * 0 OK, 1 back (circle), 2 that slot (g_setupSlot) to set up (0x80039CA4) */
/* 0x80039CA4 */
u32 PlayerMenu_Task(short *step, PlayerMenuWork *work)
{
    MenuChar *ch;
    MenuItem *item;
    PlayerSetup *setup;
    u16 sprite;
    int arrived;
    int pad;
    int c;
    int i;

    FntPrint("exec task_player_menu\n");
    FntPrint("phaes : %d\n", (u16)*step);
    switch (*step)
    {
    case PMENU_INIT:
        for (i = 5; i >= 0; i--)
        {
            g_menuRowItems[i] = -1;
        }
        work->shown = 1;
        work->players = g_gameConfig.numPlayers;
        for (i = 0; i < g_gameConfig.numPlayers && g_gameConfig.players[i].charId != 0xff; i++)
        {
        }
        g_setupLastSlot = i;
        if (g_setupLastSlot >= (char)g_gameConfig.numHumans)
        {
            /* the humans are set: the CPU players get random characters */
            for (i = g_setupLastSlot; i < g_gameConfig.numPlayers; i++)
            {
                setup = &g_gameConfig.players[i];
                if ((char)setup->charId == -1)
                {
                    c = PlayerMenu_RandomChar();
                    setup->charId = c;
                    strcpy(setup->name,g_cpuNames[(char)c]);
                    g_charTaken[(char)setup->charId] = 1;
                    g_slotFilled[i] = 1;
                }
            }
            g_setupLastSlot = g_gameConfig.numPlayers;
        }
        work->fade = 0x100;
        work->items = 2;
        work->cursor = g_setupLastSlot;
        for (i = 0; i < work->items; i++)
        {
            item = &g_menuItems[i];
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->state = MENUITEM_STILL;
            item->offsetX = 0x140;
            item->offsetY = 0;
            if (i == 0)
            {
                /* the title: two sprites side by side */
                g_menuItems[0].sprite = -1;
                g_menuItems[0].x = 0x10;
                g_menuItems[0].y = 0x10;
                g_menuSpriteParts[0].sprite = GROUP_SPRITE(5,0x47);
                g_menuSpriteParts[2].sprite = -1;
                g_menuSpriteParts[0].x = 0;
                g_menuSpriteParts[1].sprite = GROUP_SPRITE(5,0x48);
                g_menuSpriteParts[1].x = g_sprites[GROUP_SPRITE(5,0x47)].w;
                g_menuSpriteParts[0].y = -(g_sprites[GROUP_SPRITE(5,0x47)].h / 2);
                g_menuSpriteParts[0].flip = 0;
                g_menuSpriteParts[1].flip = 0;
                g_menuSpriteParts[1].y = -(g_sprites[GROUP_SPRITE(5,0x48)].h / 2);
                SpriteList_GetBounds(&g_menuSpriteList,g_menuSpriteParts);
            }
            else if (i == 1)
            {
                g_menuItems[1].sprite = 0x7d;
                g_menuItems[1].x = 0xf0;
                g_menuItems[1].y = 0xcc;
            }
        }
        for (i = 0; i < g_gameConfig.numPlayers; i++)
        {
            if (g_gameConfig.players[i].charId != 0xff)
            {
                g_menuRowItems[i] = Text_Create(g_gameConfig.players[i].name);
            }
        }
        for (i = 0; i < work->players; i++)
        {
            g_playerMenuChars[i].shown = 0;
        }
        work->item = 0;
        *step += 1;
        break;
    case PMENU_ITEM_IN:
        /* the menu items come in two frames apart */
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item == work->items)
        {
            *step = PMENU_WAIT_ITEMS;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case PMENU_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = PMENU_ITEM_IN;
        }
        break;
    case PMENU_WAIT_ITEMS:
        if (g_menuItems[work->items - 1].state == MENUITEM_STILL)
        {
            *step = PMENU_WALK_IN_START;
        }
        break;
    case PMENU_WALK_IN_START:
        for (i = 0; i < work->players; i++)
        {
            ch = &g_playerMenuChars[i];
            ch->y = 0x24 + i * 0x18;
            ch->x = 0x168 + i * 100;
            ch->anim = 1;
            ch->shown = 0;
            ch->frame = 0;
            ch->timer = 0;
            ch->unused0A = 0;
            ch->shown = 1;
            ch->lastAnim = 1;
        }
        *step += 1;
        break;
    case PMENU_WALK_IN:
        /* they walk in to x 0x28, 0x4c, ... and stand */
        arrived = 1;
        for (i = 0; i < work->players; i++)
        {
            ch = &g_playerMenuChars[i];
            ch->x -= 0x10;
            if (ch->x > 0x28 + i * 0x24)
            {
                arrived = 0;
            }
            else
            {
                ch->x = 0x28 + i * 0x24;
                ch->anim = 0;
            }
        }
        if (arrived)
        {
            *step = PMENU_FADE_IN;
        }
        break;
    case PMENU_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->fade = 0;
            *step = PMENU_CHOOSE;
        }
        break;
    case PMENU_CHOOSE:
        if ((int)work->cursor < (char)g_gameConfig.numHumans)
        {
            pad = Pad_AssignPressed(work->cursor,PADRdown);
            if (pad != -1)
            {
                g_gameConfig.players[work->cursor].pad = pad;
                g_menuResult = (work->cursor != work->players) ? MENU_RESULT_CHARACTER : MENU_RESULT_NEXT;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = PMENU_FADE_OUT;
                break;
            }
        }
        else if (g_playerPads[6].pressed & PADRdown)
        {
            g_gameConfig.players[work->cursor].pad = 0;
            g_menuResult = (work->cursor == work->players) ? MENU_RESULT_NEXT : MENU_RESULT_CHARACTER;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = PMENU_FADE_OUT;
            break;
        }
        if (g_playerPads[6].pressed & PADRright)
        {
            g_menuResult = MENU_RESULT_BACK;
            *step = PMENU_FADE_OUT;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
        }
        else if (g_playerPads[6].repeat & PADLup)
        {
            work->cursor = (work->cursor == 0) ? g_setupLastSlot : work->cursor - 1;
            if (g_setupLastSlot != 0)
            {
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
        }
        else if (g_playerPads[6].repeat & PADLdown)
        {
            work->cursor = (work->cursor == g_setupLastSlot) ? 0 : work->cursor + 1;
            if (g_setupLastSlot != 0)
            {
                Se_Play(SE_CURSOR,0x7f,0x7f,1);
            }
        }
        break;
    case PMENU_FADE_OUT:
        work->fade += 0x40;
        if (work->fade >= 0x100)
        {
            work->fade = 0x100;
            *step += 1;
        }
        break;
    case PMENU_LEAVE:
        *step += 1;
        break;
    case PMENU_WALK_OUT:
        /* they walk out to the left */
        arrived = 1;
        for (i = 0; i < work->players; i++)
        {
            ch = &g_playerMenuChars[i];
            ch->x -= 0x10;
            if (ch->x < -0x1f)
            {
                ch->x = -0x20;
            }
            else
            {
                arrived = 0;
            }
        }
        if (arrived)
        {
            *step += 1;
        }
        break;
    case PMENU_ITEMS_OUT_START:
        work->item = 0;
        *step += 1;
        break;
    case PMENU_ITEM_OUT:
        /* the menu items leave two frames apart */
        g_menuItems[work->item].state = MENUITEM_FOLD;
        work->item++;
        if (work->item == work->items)
        {
            *step = PMENU_WAIT_ITEMS_OUT;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case PMENU_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = PMENU_ITEM_OUT;
        }
        break;
    case PMENU_WAIT_ITEMS_OUT:
        if (g_menuItems[work->items - 1].state == MENUITEM_STILL)
        {
            *step += 1;
        }
        break;
    case PMENU_DONE:
        g_setupSlot = work->cursor;
        work->shown = 0;
        for (i = 0; i < 6; i++)
        {
            Text_Free(g_menuRowItems[i]);
        }
        return 1;
    }

    if (work->shown != 0)
    {
        for (i = 0; i <= g_setupLastSlot; i++)
        {
            Menu_DrawBand(PLAYERMENU_ROW_Y(work,i),0x10,work->fade);
        }
        MenuItems_Draw(work->items);
        if (work->fade == 0)
        {
            /* the hand at the row */
            g_drawPos[0] = 0x18;
            g_drawPos[1] = (work->cursor == work->players) ? 0xcc : (short)work->cursor * 0x18 + 0x3c;
            Sprite_DrawAligned(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,ALIGN_TOP,
                               g_ot + g_otIndex,8);
        }
        for (i = 0; i < work->players; i++)
        {
            ch = &g_playerMenuChars[i];
            setup = &g_gameConfig.players[i];
            if (ch->shown != 0)
            {
                g_drawPos[0] = ch->x - 8;
                g_drawPos[1] = ch->y + 0x20;
                Sprite_Draw((i < (char)g_gameConfig.numHumans) ? GROUP_SPRITE(5,0x66) + g_frameVsync / 8 % 3
                                                               : GROUP_SPRITE(5,0x69),
                            0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
            }
            if (setup->charId == 0xff)
            {
                /* no character yet: a blank figure */
                if (ch->shown != 0)
                {
                    g_drawPos[0] = ch->x;
                    g_drawPos[1] = ch->y;
                    Sprite_DrawAligned(GROUP_SPRITE(4,0),0,PAL_BLANK,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
                }
                continue;
            }
            sprite = MenuChar_Animate(ch,setup->charId);
            if (ch->shown != 0)
            {
                g_drawPos[0] = ch->x;
                g_drawPos[1] = ch->y;
                Sprite_DrawAligned(GROUP_SPRITE(4,sprite) + (char)setup->charId * 5,0,
                                   k_charPalettes[(char)setup->charId],g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
                g_drawPos[0] = ch->x + 0x10;
                g_drawPos[1] = ch->y + 0x2c;
                Sprite_DrawAligned(GROUP_SPRITE(0,0x71),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
            }
            if (work->fade == 0)
            {
                /* the name */
                g_drawPos[0] = (i < 3) ? i * 0x24 + 0x78 : (i - 3) * 0x24 + 0x28;
                g_drawPos[1] = i * 0x18 + 0x3e;
                FntPrint("%d-%x ", i, g_menuRowItems[i]);
                Text_Draw(g_menuRowItems[i],g_drawPos[0],g_drawPos[1],g_ot + g_otIndex,8);
            }
        }
    }
    FntPrint("\n");
    return 0;
}

/* CharSelect_Task steps */
enum CharSelectStep
{
    CHARSEL_INIT              =   0,
    CHARSEL_ITEM_IN           =   1,  /* the title slides in */
    CHARSEL_ITEM_DELAY        =   2,
    CHARSEL_WAIT_ITEMS        =   3,
    CHARSEL_WHEEL             =  10,  /* the open characters around the wheel */
    CHARSEL_FIRST_FREE        =  20,  /* start on the first character nobody took */
    CHARSEL_WHEEL_IN          =  21,  /* the wheel turns in */
    CHARSEL_CHOOSE            =  40,
    CHARSEL_SPIN              =  41,
    CHARSEL_CHOSEN            =  90,
    CHARSEL_LEAVE             = 100,  /* the wheel turns on, the characters go out behind */
    CHARSEL_WHEEL_OUT         = 110,
    CHARSEL_WHEEL_TURNING_OUT = 111,
    CHARSEL_ITEMS_OUT_START   = 130,  /* then the title */
    CHARSEL_ITEM_OUT          = 131,
    CHARSEL_ITEM_OUT_DELAY    = 132,
    CHARSEL_WAIT_ITEMS_OUT    = 133,
    CHARSEL_DONE              = 134 
};

/* Choosing a player's character on a turning wheel: the characters story mode has opened, the ones
 * other players took grayed out and skipped; left / right turn the wheel, cross picks, circle
 * goes back (0x8003AC38) */
/* 0x8003AC38 */
u32 CharSelect_Task(short *step, CharSelectWork *work)
{
    CharSlot *slot;
    CharAnimFrame *anim;
    MenuItem *item;
    u16 sprite;
    u16 palette;
    short charId;
    short screenX;
    short screenY;
    int angle;
    int depth;
    int scale;
    int side;
    int pri;
    int done;
    int tries;
    int pad;
    int i;

    for (;;)
    {
        work->changed = 0;
        switch (*step)
        {
        case CHARSEL_INIT:
            /* the title, the wheel empty; this player's character is free again */
            if (g_slotFilled[work->slot] != 0)
            {
                g_charTaken[(char)g_gameConfig.players[work->slot].charId] = 0;
            }
            work->done = 1;
            work->changed = 0;
            work->zoom = 0x100;
            work->count = 1;
            for (i = 0, item = g_menuItems; i < (char)work->count; i++, item++)
            {
                item->scale = 0x1000;
                item->scaleY = 0x333;
                item->state = MENUITEM_STILL;
                item->offsetX = 0x140;
                item->offsetY = 0;
                if (i == 0)
                {
                    g_menuItems[0].sprite = -1;
                    g_menuItems[0].x = 16;
                    g_menuItems[0].y = 16;
                    g_menuSpriteParts[0].sprite = g_spriteGroups[5].base + 0x49;
                    g_menuSpriteParts[2].sprite = -1;
                    g_menuSpriteParts[0].x = 0;
                    g_menuSpriteParts[1].sprite = g_spriteGroups[5].base + 0x4a;
                    g_menuSpriteParts[1].x = g_sprites[g_spriteGroups[5].base + 0x49].w;
                    g_menuSpriteParts[0].y = -(g_sprites[g_spriteGroups[5].base + 0x49].h / 2);
                    g_menuSpriteParts[0].flip = 0;
                    g_menuSpriteParts[1].flip = 0;
                    g_menuSpriteParts[1].y = -(g_sprites[g_spriteGroups[5].base + 0x4a].h / 2);
                    SpriteList_GetBounds(&g_menuSpriteList,g_menuSpriteParts);
                }
            }
            work->chars = 14;
            for (i = 0; i < work->chars; i++)
            {
                g_charSelectSlots[i].shown = 0;
            }
            work->item = 0;
            *step += 1;
            break;

        /* the title slides in */
        case CHARSEL_ITEM_IN:
            g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
            work->item++;
            if (work->item != work->count)
            {
                work->delay = 2;
                *step += 1;
            }
            else
            {
                *step = CHARSEL_WAIT_ITEMS;
            }
            break;
        case CHARSEL_ITEM_DELAY:
            work->delay--;
            if (work->delay == 0)
            {
                *step = CHARSEL_ITEM_IN;
            }
            break;
        case CHARSEL_WAIT_ITEMS:
            if (g_menuItems[(char)work->count - 1].state == MENUITEM_STILL)
            {
                *step = CHARSEL_WHEEL;
            }
            break;

        /* the open characters around the wheel */
        case CHARSEL_WHEEL:
            work->moving = 0;
            work->pick = 0;
            work->chars = 0;
            for (i = 0; i < 14; i++)
            {
                if (g_progress.charOpen[i] != 0)
                {
                    g_charSelectSlots[work->chars].charId = i;
                    work->chars++;
                }
            }
            work->spin = -0x400;
            work->target = -0x400;
            work->delay = 0;
            work->charId = g_charSelectSlots[work->pick].charId;
            for (i = 0; i < work->chars; i++)
            {
                slot = &g_charSelectSlots[i];
                slot->anim = 1;
                slot->shown = 0;
                slot->frame = 0;
                slot->timer = 0;
                slot->radius = 0x800;
                slot->lastAnim = 1;
                slot->unused1A = 0x78;
                slot->unused0C = 0;
                slot->angle = -((i << 12) / work->chars);
            }
            *step = CHARSEL_FIRST_FREE;
            break;
        case CHARSEL_FIRST_FREE:
            /* start on the first character nobody took */
            for (tries = 0; (0 < work->chars) && (g_charTaken[work->charId] != 0); )
            {
                work->pick = work->pick == work->chars - 1 ? 0 : work->pick + 1;
                tries++;
                work->charId = g_charSelectSlots[work->pick].charId;
                if (work->chars <= tries)
                {
                    break;
                }
            }
            work->base = 0x1000 - g_charSelectSlots[work->pick].angle;
            *step += 1;
            break;
        case CHARSEL_WHEEL_IN:
            /* the wheel turns in (faster with a button), the characters coming in from behind */
            if ((g_playerPads[work->slot].pressed & (PADRright | PADRdown)) != 0)
            {
                work->moving = 1;
            }
            work->done = 1;
            work->confirming = 0;
            work->spin += work->moving == 0 ? 0x16 : 0x5b;
            for (i = 0; i < work->chars; i++)
            {
                slot = &g_charSelectSlots[i];
                if (-0x401 < work->spin + slot->angle)
                {
                    slot->shown = 1;
                    depth = (work->spin + slot->angle) * 0x90 / 1024;
                    slot->radius = -depth < 0 ? 0x90 : 0x90 - depth;
                }
            }
            if ((work->target < work->base) && (work->base <= work->spin))
            {
                work->spin = work->base & 0xffff;
                *step = CHARSEL_CHOOSE;
            }
            break;

        /* choosing */
        case CHARSEL_CHOOSE:
            work->changed = 1;
            work->spin &= 0xfff;
            g_charSelectSlots[work->pick].anim = 0;
            pad = work->slot;
            if ((g_playerPads[pad].pressed & PADRdown) != 0)
            {
                g_slotFilled[pad] = 1;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = CHARSEL_CHOSEN;
                break;
            }
            if ((g_playerPads[pad].pressed & PADRright) != 0)
            {
                if (g_slotFilled[pad] == 0)
                {
                    if (g_gameConfig.players[pad].charId != 0xff)
                    {
                        g_gameConfig.players[pad].charId = 0xff;
                    }
                }
                else
                {
                    g_charTaken[(char)g_gameConfig.players[pad].charId] = 1;
                }
                g_menuResult = MENU_RESULT_BACK;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
                *step = CHARSEL_LEAVE;
                break;
            }
            if ((g_playerPads[pad].held & (PADLright | PADLleft)) == 0)
            {
                break;
            }
            /* the next character nobody took, to the left or the right; the wheel turns to it */
            tries = 1;
            if ((g_playerPads[pad].held & PADLleft) != 0)
            {
                do
                {
                    if (work->chars <= tries)
                    {
                        break;
                    }
                    work->pick = (work->pick == 0 ? work->chars : work->pick) - 1;
                    work->charId = g_charSelectSlots[work->pick].charId;
                    tries++;
                } while (g_charTaken[work->charId] != 0);
                work->spinStep = -0x16;
                work->base = -g_charSelectSlots[work->pick].angle;
                if (work->spin < work->base)
                {
                    work->base -= 0x1000;
                }
                work->confirming = 1;
            }
            else
            {
                do
                {
                    if (work->chars <= tries)
                    {
                        break;
                    }
                    work->pick = work->pick == work->chars - 1 ? 0 : work->pick + 1;
                    work->charId = g_charSelectSlots[work->pick].charId;
                    tries++;
                } while (g_charTaken[work->charId] != 0);
                work->spinStep = 0x16;
                work->base = -g_charSelectSlots[work->pick].angle;
                if (work->base < work->spin)
                {
                    work->base += 0x1000;
                }
                work->confirming = 0;
            }
            for (i = 0; i < work->chars; i++)
            {
                g_charSelectSlots[i].anim = 1;
            }
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
            *step += 1;
            continue;                       /* turn at once */
        case CHARSEL_SPIN:
            work->spin += (short)work->spinStep;
            if ((short)work->spinStep < 1)
            {
                if (work->target <= work->base)
                {
                    break;
                }
                done = !(work->base < work->spin);
            }
            else
            {
                if (work->base <= work->target)
                {
                    break;
                }
                done = !(work->spin < work->base);
            }
            if (done)
            {
                work->spin = work->base;
                *step = CHARSEL_CHOOSE;
            }
            break;

        case CHARSEL_CHOSEN:
            g_gameConfig.players[work->slot].charId = work->charId;
            strcpy(g_gameConfig.players[work->slot].name,g_cpuNames[work->charId]);
            g_charTaken[(char)g_gameConfig.players[work->slot].charId] = 1;
            g_menuResult = MENU_RESULT_NEXT;
            *step = CHARSEL_LEAVE;
            break;

        /* leave: the wheel turns on and the characters go out behind, then the title */
        case CHARSEL_LEAVE:
            work->moving = 0;
            work->delay = 0;
            for (i = 0; i < work->chars; i++)
            {
                g_charSelectSlots[i].anim = 1;
            }
            work->confirming = 0;
            *step = CHARSEL_WHEEL_OUT;
            break;
        case CHARSEL_WHEEL_OUT:
            for (i = 0; i < work->chars; i++)
            {
                if (0 < work->spin + g_charSelectSlots[i].angle)
                {
                    g_charSelectSlots[i].angle -= 0x1000;
                }
            }
            *step += 1;
            break;
        case CHARSEL_WHEEL_TURNING_OUT:
            if ((g_playerPads[work->slot].pressed & (PADRright | PADRdown)) != 0)
            {
                work->moving = 1;
            }
            work->spin += work->moving == 0 ? 0x16 : 0x5b;
            for (i = 0; i < work->chars; i++)
            {
                slot = &g_charSelectSlots[i];
                angle = work->spin + slot->angle;
                if (0 <= angle)
                {
                    depth = angle * 0x90 / 1024;
                    slot->radius = depth < 0 ? 0x90 : depth + 0x90;
                }
                if (0x3ff < work->spin + slot->angle)
                {
                    slot->shown = 0;
                }
            }
            done = true;
            for (i = 0; i < work->chars; i++)
            {
                if (g_charSelectSlots[i].shown != 0)
                {
                    done = false;
                }
            }
            if (done)
            {
                *step = CHARSEL_ITEMS_OUT_START;
            }
            break;
        case CHARSEL_ITEMS_OUT_START:
            work->item = 0;
            *step += 1;
            break;
        case CHARSEL_ITEM_OUT:
            g_menuItems[work->item].state = MENUITEM_FOLD;
            work->item++;
            if (work->item != work->count)
            {
                work->delay = 2;
                *step += 1;
            }
            else
            {
                *step = CHARSEL_WAIT_ITEMS_OUT;
            }
            break;
        case CHARSEL_ITEM_OUT_DELAY:
            work->delay--;
            if (work->delay == 0)
            {
                *step = CHARSEL_ITEM_OUT;
            }
            break;
        case CHARSEL_WAIT_ITEMS_OUT:
            if (g_menuItems[(char)work->count - 1].state == MENUITEM_STILL)
            {
                *step += 1;
            }
            break;
        case CHARSEL_DONE:
            return 1;
        }
        break;
    }

    work->target = work->spin;
    if (work->done == 0)
    {
        return 0;
    }
    MenuItems_Draw((char)work->count);
    if (work->changed != 0)
    {
        /* the arrows either side of the front */
        g_drawPos[0] = 0x80;
        g_drawPos[1] = 0x78;
        Sprite_DrawAligned(g_spriteGroups[5].base + g_frameVsync / 8 % 4 + 0x6e,0,PAL_DEFAULT,g_drawPos,0,
                           g_ot + g_otIndex,8);
        g_drawPos[0] = 0xc0;
        g_drawPos[1] = 0x78;
        Sprite_DrawAligned(g_spriteGroups[5].base + g_frameVsync / 8 % 4 + 0x6e,0x400000,PAL_DEFAULT,g_drawPos,0,
                           g_ot + g_otIndex,8);
    }
    for (i = 0; i < work->chars; i++)
    {
        slot = &g_charSelectSlots[i];
        if (slot->shown == 0)
        {
            continue;
        }
        /* the animation */
        if (slot->lastAnim != slot->anim)
        {
            slot->frame = 0;
            slot->timer = 0;
            slot->lastAnim = slot->anim;
        }
        charId = slot->charId;
        if (charId == 0)
        {
            anim = k_charAnimsChar0[slot->anim];
        }
        else if (charId == 4)
        {
            anim = k_charAnimsChar4[slot->anim];
        }
        else
        {
            anim = k_charAnims[slot->anim];
        }
        sprite = anim[slot->frame].sprite;
        slot->timer++;
        if (slot->timer == (char)anim[slot->frame].frames)
        {
            if (anim[slot->frame].next == -2)
            {
                slot->frame++;
            }
            else
            {
                slot->frame = anim[slot->frame].next;
            }
            slot->timer = 0;
        }
        /* on the wheel: x across, depth from the front (0) to the back (twice the radius), smaller
         * and higher up further back */
        angle = (short)(work->spin + slot->angle);
        side = slot->radius * -rsin(angle) / 4096;
        depth = (short)(slot->radius + slot->radius * -rcos(angle) / 4096);
        screenY = (-(depth * 40) / (slot->radius * 2));
        scale = 0x1000 - depth * 0x666 / (slot->radius * 2);
        screenX = (scale * (short)side / 4096);
        pri = depth * 8 / slot->radius + 8;
        g_drawPos[0] = screenX + 0xa0;
        g_drawPos[1] = screenY + 0x78;
        g_drawPos[2] = (scale * 0x24 / 4096);
        g_drawPos[3] = (scale * 0x36 / 4096);
        /* poses from 0x50 have a back view, for the far half of the wheel */
        if (0x4f < (short)sprite)
        {
            if (work->confirming == 0)
            {
                if (slot->radius <= depth)
                {
                    sprite += 0x50;
                }
            }
            else if (depth < slot->radius)
            {
                sprite += 0x50;
            }
        }
        palette = g_charTaken[charId] == 0 ? k_charPalettes[i] : -101;
        Sprite_DrawStretched(g_spriteGroups[4].base + sprite + charId * 5,0,palette,g_drawPos,0,
                             g_ot + g_otIndex,(short)pri);
        /* its shadow */
        g_drawPos[0] = screenX + 0xa0;
        g_drawPos[1] = screenY + (short)(scale * 0x36 / 4096) / 2 + 0x78;
        g_drawPos[2] = (scale * 0x24 / 4096);
        g_drawPos[3] = (scale * 9 / 4096);
        Sprite_DrawStretched(g_spriteGroups[0].base + 0x71,0x40000000,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,
                             (short)pri);
    }
    return 0;
}

/* Name entry grid keys: k_nameEntryChars cells read as little-endian u16 (Shift-JIS bytes
 * swapped). Every other cell is a character to add. */
#define NE_KEY_NONE        0x9481  /* "＃" (0x8194): no key, the cursor skips it */
#define NE_KEY_HIRAGANA    0xBD95  /* 平 (0x95BD): to page 0 */
#define NE_KEY_KATAKANA    0xD095  /* 片 (0x95D0): to page 1 */
#define NE_KEY_ALPHABET    0x7089  /* 英 (0x8970): to page 2 */
#define NE_KEY_DAKUTEN     0x6881  /* ” (0x8168): voice the last kana (Sjis_RemapA) */
#define NE_KEY_HANDAKUTEN  0x4281  /* 。 (0x8142): half-voice the last kana (Sjis_RemapB) */
#define NE_KEY_RESET       0x898F  /* 初 (0x8F89): back to the old name */
#define NE_KEY_DELETE      0xDF96  /* 戻 (0x96DF): delete the last character */
#define NE_KEY_END         0x498F  /* 終 (0x8F49): done */

#define NE_MAX_LEN   12

/* NameEntryWork.page: the grid of characters shown */
#define NE_PAGE_HIRAGANA  0
#define NE_PAGE_KATAKANA  1
#define NE_PAGE_ALPHABET  2
#define NE_END_COL   12            /* where 終 is */
#define NE_END_ROW   5

/* The key under the name entry cursor (NE_KEY_* or a character) */
static int NameEntry_Key(NameEntryWork *work)
{
    u8 *cell = k_nameEntryChars[work->page][work->row][work->col];

    return cell[0] | cell[1] << 8;
}

/* Rebuild the text of the name after a change */
static void NameEntry_SetText(NameEntryWork *work)
{
    Text_Free(work->id);
    work->id = Text_Create((char *)work->name);
}

/* NameEntry_Task steps */
enum NameEntryStep
{
    NAME_INIT            =   0,
    NAME_ITEM_IN         =   1,  /* the title flies in */
    NAME_ITEM_DELAY      =   2,
    NAME_WAIT_ITEMS      =   3,
    NAME_WALK_IN_START   =   5,  /* the character walks in from the right */
    NAME_WALK_IN         =   6,
    NAME_FADE_IN         =  10,  /* the name field fades in */
    NAME_GRID_UP         =  15,  /* the grid slides up */
    NAME_ENTER           =  20,  /* the keys (START: to the end key) */
    NAME_GRID_DOWN       = 100,  /* everything leaves */
    NAME_FADE_OUT        = 101,
    NAME_WALK_OUT_START  = 102,
    NAME_WALK_OUT        = 103,
    NAME_ITEMS_OUT_START = 104,
    NAME_ITEM_OUT        = 105,
    NAME_ITEM_OUT_DELAY  = 106,
    NAME_WAIT_ITEMS_OUT  = 107,
    NAME_DONE            = 108 
};

/* Cross on the key under the cursor (NameEntry_Task step 0x14) */
static void NameEntry_PressKey(short *step, NameEntryWork *work)
{
    int key = NameEntry_Key(work);
    int charId = g_gameConfig.players[work->slot].charId;
    u32 changed;

    switch (key)
    {
    case NE_KEY_ALPHABET:
        work->page = NE_PAGE_ALPHABET;
        break;
    case NE_KEY_HIRAGANA:
        work->page = NE_PAGE_HIRAGANA;
        break;
    case NE_KEY_KATAKANA:
        work->page = NE_PAGE_KATAKANA;
        break;
    case NE_KEY_END:
        /* an empty name becomes the character's own one */
        if (work->len == 0)
        {
            strcpy((char *)work->name,g_cpuNames[charId]);
        }
        strcpy(g_gameConfig.players[work->slot].name,(char *)work->name);
        g_menuResult = MENU_RESULT_NEXT;
        *step = NAME_GRID_DOWN;
        break;
    case NE_KEY_RESET:
        strcpy((char *)work->name,g_gameConfig.players[work->slot].name);
        NameEntry_SetText(work);
        work->pos = work->len = Text_GetWidth(work->id);
        work->col = NE_END_COL;
        work->row = NE_END_ROW;
        break;
    case NE_KEY_DELETE:
        if (work->len <= 0)
        {
            /* nothing left to delete: back to the character choice */
            g_charTaken[charId] = 0;
            g_menuResult = MENU_RESULT_BACK;
            *step = NAME_GRID_DOWN;
        }
        else
        {
            work->pos--;
            work->len--;
            work->name[work->pos] = 0;
            NameEntry_SetText(work);
        }
        break;
    case NE_KEY_DAKUTEN:
    case NE_KEY_HANDAKUTEN:
        /* Japanese pages only (unreachable in this version: page 2 has no page keys). Works on
         * two-byte characters, so on name[len * 2 - 2], unlike the one-byte adding below */
        if (work->len <= 0)
        {
            return;
        }
        if (key == NE_KEY_DAKUTEN)
        {
            changed = Sjis_RemapA((char *)&work->name[work->len * 2 - 2]);
        }
        else
        {
            changed = Sjis_RemapB((char *)&work->name[work->len * 2 - 2]);
        }
        if (changed == 0)
        {
            return;
        }
        NameEntry_SetText(work);
        if (work->len == NE_MAX_LEN)
        {
            work->col = NE_END_COL;
            work->row = NE_END_ROW;
        }
        break;
    default:
        /* add the character (its first byte: one-byte characters) */
        if (work->pos >= NE_MAX_LEN)
        {
            return;
        }
        work->name[work->len] = k_nameEntryChars[work->page][work->row][work->col][0];
        work->pos++;
        work->len++;
        work->name[work->pos] = 0;
        NameEntry_SetText(work);
        if (work->len == NE_MAX_LEN)
        {
            /* full: to 終 */
            work->col = NE_END_COL;
            work->row = NE_END_ROW;
        }
        break;
    }
    Se_Play(SE_OK,0x7f,0x7f,1);
}

/* Move the name entry cursor with the d-pad (repeating while held), wrapping around and skipping
 * the cells without a key. Going up or down in a column with no other key, the cursor tries the
 * column to the left. */
static void NameEntry_MoveCursor(NameEntryWork *work, u16 buttons)
{
    signed char startRow;

    if (buttons & PADLup)
    {
        startRow = work->row;
        for (;;)
        {
            do
            {
                if (--work->row < 0)
                {
                    work->row = 5;
                }
            } while (NameEntry_Key(work) == NE_KEY_NONE);
            if (work->row != startRow)
            {
                break;
            }
            work->col--;
        }
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
    }
    if (buttons & PADLdown)
    {
        startRow = work->row;
        for (;;)
        {
            do
            {
                if (++work->row > 5)
                {
                    work->row = 0;
                }
            } while (NameEntry_Key(work) == NE_KEY_NONE);
            if (work->row != startRow)
            {
                break;
            }
            work->col--;
        }
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
    }
    if (buttons & PADLleft)
    {
        do
        {
            if (--work->col < 0)
            {
                work->col = 13;
            }
        } while (NameEntry_Key(work) == NE_KEY_NONE);
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
    }
    if (buttons & PADLright)
    {
        do
        {
            if (++work->col > 13)
            {
                work->col = 0;
            }
        } while (NameEntry_Key(work) == NE_KEY_NONE);
        Se_Play(SE_CURSOR,0x7f,0x7f,1);
    }
}

/* Entering a player's name: a 14 x 6 grid of characters (page 2, the alphabet; the Japanese
 * kana pages 0 and 1 are left over and cannot be reached), with the player's character beside
 * it. Cross adds the key under the cursor, circle deletes, start jumps to 終 (end). Ends with
 * g_menuResult 0 (name stored in the player's setup) or 1 (back to the character choice)
 * (0x8003BFC8) */
/* 0x8003BFC8 */
u32 NameEntry_Task(short *step, NameEntryWork *work)
{
    PadState *pad;
    MenuItem *item;
    int charId;
    int palette;
    int i;
    u16 sprite;

    FntPrint("exec name entry\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch(*step)
    {
    case NAME_INIT:
        work->done = 1;
        work->col = NE_END_COL;
        work->row = NE_END_ROW;
        work->page = NE_PAGE_ALPHABET;
        work->len = 0;
        work->pos = 0;
        work->fade = 0x100;
        strcpy((char *)work->name,g_gameConfig.players[work->slot].name);
        work->id = Text_Create((char *)work->name);
        work->pos = work->len = Text_GetWidth(work->id);
        work->gridY = 0xf0;
        work->gridX = 0;
        work->items = 1;
        for (i = 0; i < work->items; i++)
        {
            item = &g_menuItems[i];
            item->state = MENUITEM_STILL;
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->offsetY = 0;
            if (i == 0)
            {
                g_menuItems[0].sprite = 0x4b;
                g_menuItems[0].x = 0x10;
                g_menuItems[0].y = 0x10;
            }
        }
        g_nameEntryChar[0].shown = 0;
        work->item = 0;
        *step += 1;
        break;
    /* 1-3: the title flies in */
    case NAME_ITEM_IN:
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        if (++work->item != work->items)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = NAME_WAIT_ITEMS;
        break;
    case NAME_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = NAME_ITEM_IN;
        }
        break;
    case NAME_WAIT_ITEMS:
        if (g_menuItems[work->items - 1].state == MENUITEM_STILL)
        {
            *step = NAME_WALK_IN_START;
        }
        break;
    /* 5, 6: the character walks in from the right */
    case NAME_WALK_IN_START:
        g_nameEntryChar[0].shown = 1;
        g_nameEntryChar[0].anim = 1;
        g_nameEntryChar[0].frame = 0;
        g_nameEntryChar[0].timer = 0;
        /* the original reads g_nameEntryChar[i].anim with i left over in a register from the
         * caller; frame and timer are 0 already, so any value has the same effect */
        g_nameEntryChar[0].lastAnim = g_nameEntryChar[0].anim;
        g_nameEntryChar[0].x = 0x140;
        g_nameEntryChar[0].unused0A = 0;
        g_nameEntryChar[0].y = 0x2c;
        *step += 1;
        break;
    case NAME_WALK_IN:
        g_nameEntryChar[0].x -= 0x10;
        if (g_nameEntryChar[0].x <= 0x30)
        {
            g_nameEntryChar[0].x = 0x30;
            g_nameEntryChar[0].anim = 0;
            *step = NAME_FADE_IN;
        }
        break;
    /* 10, 0xf: the name field fades in, the grid slides up */
    case NAME_FADE_IN:
        work->fade -= 0x40;
        if (work->fade <= 0)
        {
            work->fade = 0;
            *step = NAME_GRID_UP;
        }
        break;
    case NAME_GRID_UP:
        work->gridY -= 0x10;
        if (work->gridY <= 0x70)
        {
            work->gridY = 0x70;
            *step = NAME_ENTER;
        }
        break;
    case NAME_ENTER:
        pad = &g_playerPads[work->slot];
        if (pad->pressed & PADstart)
        {
            work->col = NE_END_COL;
            work->row = NE_END_ROW;
        }
        else if (pad->pressed & PADRdown)
        {
            NameEntry_PressKey(step,work);
            break;
        }
        else if (pad->pressed & PADRright)
        {
            if (work->len == 0)
            {
                /* back to the character choice */
                g_charTaken[g_gameConfig.players[work->slot].charId] = 0;
                g_menuResult = MENU_RESULT_BACK;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
                *step = NAME_GRID_DOWN;
            }
            else
            {
                work->pos--;
                work->len--;
                work->name[work->pos] = 0;
                NameEntry_SetText(work);
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
            }
        }
        NameEntry_MoveCursor(work,pad->repeat);
        break;
    /* 100-0x6c: everything leaves */
    case NAME_GRID_DOWN:
        work->gridY += 0x10;
        if (work->gridY >= 0xf0)
        {
            work->gridY = 0xf0;
            *step += 1;
        }
        break;
    case NAME_FADE_OUT:
        work->fade += 0x40;
        if (work->fade >= 0x100)
        {
            work->fade = 0x100;
            *step += 1;
        }
        break;
    case NAME_WALK_OUT_START:
        g_nameEntryChar[0].anim = 1;
        *step += 1;
        break;
    case NAME_WALK_OUT:
        g_nameEntryChar[0].x -= 0x10;
        if (g_nameEntryChar[0].x <= -0x20)
        {
            g_nameEntryChar[0].x = -0x20;
            g_nameEntryChar[0].shown = 0;
            *step += 1;
        }
        break;
    case NAME_ITEMS_OUT_START:
        work->item = 0;
        *step += 1;
        break;
    case NAME_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        if (++work->item != work->items)
        {
            work->delay = 2;
            *step += 1;
            break;
        }
        *step = NAME_WAIT_ITEMS_OUT;
        break;
    case NAME_ITEM_OUT_DELAY:
        if (--work->delay == 0)
        {
            *step = NAME_ITEM_OUT;
        }
        break;
    case NAME_WAIT_ITEMS_OUT:
        if (g_menuItems[work->items - 1].state == MENUITEM_STILL)
        {
            *step += 1;
        }
        break;
    case NAME_DONE:
        work->done = 0;
        Text_Free(work->id);
        return 1;
    }

    if (work->done == 0)
    {
        return 0;
    }
    /* the blinking palette (0-14, every 4 frames) of the cursors */
    if (g_nameEntryBlink++ == 59)
    {
        g_nameEntryBlink = 0;
    }
    palette = (g_nameEntryBlink / 4) % 15;

    /* the name field, its text cursor and the name */
    Menu_DrawBand(0x40,0x14,work->fade);
    if (work->fade == 0)
    {
        g_drawPos[0] = 0x60;
        g_drawPos[1] = 0x50;
        Sprite_DrawAligned(g_spriteGroups[5].base + 0x7f,0,palette,g_drawPos,ALIGN_LEFT | ALIGN_TOP,
                           g_ot + g_otIndex,8);
        if (work->len != NE_MAX_LEN)
        {
            g_drawPos[0] = work->len * 8 + 0x58;
            g_drawPos[1] = 0x36;
            Sprite_DrawAligned(g_spriteGroups[5].base + g_frameVsync / 8 % 4 + 0x78,0,PAL_DEFAULT,g_drawPos,
                               ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        }
        Text_Draw(work->id,0x60,0x42,g_ot + g_otIndex,8);
    }

    /* the character and its shadow */
    if (g_nameEntryChar[0].shown != 0)
    {
        charId = g_gameConfig.players[work->slot].charId;
        sprite = MenuChar_Animate(&g_nameEntryChar[0],charId);
        g_drawPos[0] = g_nameEntryChar[0].x;
        g_drawPos[1] = g_nameEntryChar[0].y;
        Sprite_DrawAligned(sprite + g_spriteGroups[4].base + charId * 5,0,k_charPalettes[charId],
                           g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
        g_drawPos[0] = g_nameEntryChar[0].x + 0x10;
        g_drawPos[1] = g_nameEntryChar[0].y + 0x2c;
        Sprite_DrawAligned(g_spriteGroups[0].base + 0x71,0,0,g_drawPos,0,g_ot + g_otIndex,8);
    }

    /* the grid: its cursor (wide on the word keys), the title, the page */
    Menu_DrawBand(work->gridY,0x70,0);
    g_drawPos[0] = work->gridX + work->col * 0x10 + 0x20;
    g_drawPos[1] = work->gridY + work->row * 0x10 + 8;
    switch (NameEntry_Key(work))
    {
    case NE_KEY_RESET:
    case NE_KEY_END:
    case NE_KEY_ALPHABET:
    case NE_KEY_HIRAGANA:
    case NE_KEY_KATAKANA:
    case NE_KEY_DELETE:
        sprite = g_spriteGroups[5].base + 0x7e;
        break;
    default:
        sprite = g_spriteGroups[5].base + 0x80;
        break;
    }
    Sprite_DrawAligned(sprite,0,palette,g_drawPos,ALIGN_LEFT | ALIGN_TOP,g_ot + g_otIndex,8);
    MenuItems_Draw(work->items);
    g_drawPos[0] = work->gridX + 0x20;
    g_drawPos[1] = work->gridY + 8;
    switch (work->page)
    {
    case NE_PAGE_HIRAGANA:
        sprite = g_spriteGroups[5].base + 0x50;
        break;
    case NE_PAGE_KATAKANA:
        sprite = g_spriteGroups[5].base + 0x51;
        break;
    case NE_PAGE_ALPHABET:
        sprite = g_spriteGroups[5].base + 0x4f;
        break;
    default:
        return 0;
    }
    Sprite_Draw(sprite,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
    return 0;
}

/* StageSelect_Task steps */
enum StageSelectStep
{
    STAGESEL_INIT           =   0,
    STAGESEL_ITEM_IN        =   1,  /* the items come in two frames apart */
    STAGESEL_ITEM_DELAY     =   2,
    STAGESEL_WAIT_ITEMS     =   3,
    STAGESEL_ZOOM_IN        =  10,  /* the stage picture zooms and spins in */
    STAGESEL_CHOOSE         =  20,  /* left / right scroll (free play only) */
    STAGESEL_SCROLL_PREV    =  21,
    STAGESEL_SCROLL_NEXT    =  22,
    STAGESEL_ZOOM_OUT       = 100,
    STAGESEL_FADE_OUT       = 101,
    STAGESEL_ITEM_OUT       = 102,
    STAGESEL_ITEM_OUT_DELAY = 103,
    STAGESEL_WAIT_ITEMS_OUT = 104,
    STAGESEL_DONE           = 105 
};

/* Choosing the stage (in story mode, work[0] = 1: the next stage is shown) (0x8003D3AC) */
/* 0x8003D3AC */
u32 StageSelect_Task(short *step, StageSelectWork *work)
{
    MenuItem *item;
    short scrollX;
    u16 next;
    u16 fade;
    int scaleDone;
    int angleDone;
    int sprite;
    int i;
    int pos;

    for (;;)
    {
        work->picked = 0;
        switch (*step)
        {
        case STAGESEL_INIT:
            work->shown = 1;
            work->moving = 0;
            work->stage = g_gameConfig.stage;
            work->y = 0x80;
            work->scrollX = 0;
            work->scaleX = 0;
            work->scaleY = 0;
            work->angle = 0;
            work->progress = g_progress.freeStages;
            memcpy(&work->savedX, &work->scrollX, 4);  /* savedX, savedY = scrollX, y */
            work->fade = 0x100;
            work->count = 1;
            item = g_menuItems;
            for (i = 0; i < (short)work->count; i++, item++)
            {
                item->state = MENUITEM_STILL;
                item->scale = 0x1000;
                item->scaleY = 0x333;
                item->offsetX = 0x140;
                item->offsetY = 0;
                if (i == 0)
                {
                    if (work->story == 0)
                    {
                        g_menuItems[0].sprite = 0x4c;
                    }
                    else
                    {
                        g_menuItems[0].sprite = (char)g_gameConfig.stage + 0x8b;
                    }
                    item->x = 0x10;
                    item->y = 0x10;
                }
            }
            work->item = 0;
            *step += 1;
            break;
        case STAGESEL_ITEM_IN:      /* the menu items come in one by one */
            g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
            next = work->item + 1;
            work->item = next;
            if (next != work->count)
            {
                work->delay = 2;
                *step += 1;
            }
            else
            {
                *step = STAGESEL_WAIT_ITEMS;
            }
            break;
        case STAGESEL_ITEM_DELAY:
            work->delay--;
            if (work->delay == 0)
            {
                *step = STAGESEL_ITEM_IN;
            }
            break;
        case STAGESEL_WAIT_ITEMS:
            if (g_menuItems[(short)work->count - 1].state == MENUITEM_STILL)
            {
                *step = STAGESEL_ZOOM_IN;
            }
            break;
        case STAGESEL_ZOOM_IN:      /* the stage picture zooms and spins in */
            work->moving = 1;
            work->scaleX += 0x200;
            work->scaleY += 0x200;
            scaleDone = 0xfff < work->scaleX;
            work->angle += 0x200;
            if (scaleDone)
            {
                work->scaleX = 0x1000;
                work->scaleY = 0x1000;
            }
            angleDone = 0xfff < work->angle;
            if (angleDone)
            {
                work->angle = 0;
            }
            if (angleDone && scaleDone)
            {
                *step = STAGESEL_CHOOSE;
            }
            break;
        case STAGESEL_CHOOSE:     /* choose: left / right scroll (free play only) */
            work->picked = 1;
            work->scrollX = (work->scrollX % (work->progress * 0xa0));
            if ((g_playerPads[6].pressed & PADRdown) != 0)
            {
                g_menuResult = MENU_RESULT_NEXT;
                Se_Play(SE_OK,0x7f,0x7f,1);
                *step = STAGESEL_ZOOM_OUT;
            }
            else if (work->story <= 1 && (g_playerPads[6].pressed & PADRright) != 0)
            {
                g_menuResult = MENU_RESULT_BACK;
                Se_Play(SE_CANCEL,0x7f,0x7f,1);
                *step = STAGESEL_ZOOM_OUT;
            }
            else if (work->story == 0)
            {
                if ((g_playerPads[6].held & PADLright) != 0)
                {
                    work->scrollSpeed = 8;
                    *step = STAGESEL_SCROLL_NEXT;
                    Se_Play(SE_CURSOR,0x7f,0x7f,1);
                }
                else if ((g_playerPads[6].held & PADLleft) != 0)
                {
                    work->scrollSpeed = 8;
                    *step = STAGESEL_SCROLL_PREV;
                    Se_Play(SE_CURSOR,0x7f,0x7f,1);
                }
            }
            break;
        case STAGESEL_SCROLL_PREV:  /* scroll to the previous stage */
            scrollX = work->scrollX + work->scrollSpeed;
            work->scrollX = scrollX;
            if (scrollX < 0xa0)
            {
                break;
            }
            if (work->stage == 0)
            {
                work->stage = work->progress - 1;
            }
            else
            {
                work->stage--;
            }
            work->scrollX = 0;
            *step = STAGESEL_CHOOSE;
            continue;                 /* and take input in the same frame */
        case STAGESEL_SCROLL_NEXT:  /* scroll to the next stage */
            scrollX = work->scrollX - work->scrollSpeed;
            work->scrollX = scrollX;
            if (-0xa0 < scrollX)
            {
                break;
            }
            if (work->stage != work->progress - 1)
            {
                work->stage++;
            }
            else
            {
                work->stage = 0;
            }
            work->scrollX = 0;
            *step = STAGESEL_CHOOSE;
            continue;
        case STAGESEL_ZOOM_OUT:
            work->scaleX -= 0x200;
            work->scaleY -= 0x200;
            work->angle -= 0x200;
            if (work->scaleX <= 0)
            {
                work->moving = 0;
                *step += 1;
            }
            break;
        case STAGESEL_FADE_OUT:
            fade = work->fade + 0x40;
            work->fade = fade;
            if (0xff < (short)fade)
            {
                work->fade = 0x100;
                *step += 1;
            }
            work->item = 0;
            break;
        case STAGESEL_ITEM_OUT:    /* the menu items leave one by one */
            g_menuItems[work->item].state = MENUITEM_FOLD;
            next = work->item + 1;
            work->item = next;
            if (next != work->count)
            {
                work->delay = 2;
                *step += 1;
            }
            else
            {
                *step = STAGESEL_WAIT_ITEMS_OUT;
            }
            break;
        case STAGESEL_ITEM_OUT_DELAY:
            work->delay--;
            if (work->delay == 0)
            {
                *step = STAGESEL_ITEM_OUT;
            }
            break;
        case STAGESEL_WAIT_ITEMS_OUT:
            if (g_menuItems[(short)work->count - 1].state == MENUITEM_STILL)
            {
                *step += 1;
            }
            break;
        case STAGESEL_DONE:
            g_gameConfig.stage = work->stage;
            work->shown = 0;
            return 1;
        }
        break;
    }
    if (work->shown != 0)
    {
        memcpy(&work->savedX, &work->scrollX, 4);  /* savedX, savedY = scrollX, y */
        if (work->story == 0 && work->picked != 0)
        {
            /* blinking arrows on both sides */
            sprite = g_spriteGroups[5].base + (short)(g_frameVsync / 8 % 4) + 0x6e;
            g_drawPos[0] = 0x60;
            g_drawPos[1] = 0x80;
            Sprite_DrawAligned(sprite,0,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
            g_drawPos[0] = 0xe0;
            g_drawPos[1] = 0x80;
            Sprite_DrawAligned(sprite,0x400000,PAL_DEFAULT,g_drawPos,0,g_ot + g_otIndex,8);
        }
        if (work->moving != 0)
        {
            if (work->story == 0)
            {
                /* free play: the chosen stage and two on each side */
                pos = -0x1e0;
                for (i = -2; i < 3; i++)
                {
                    g_drawPos[0] = work->scrollX - pos;
                    g_drawPos[1] = work->y;
                    g_drawPos[2] = work->scaleX;
                    g_drawPos[3] = work->scaleY;
                    pos += 0xa0;
                    Sprite_DrawScaled((u16)GROUP_SPRITE(5,(work->stage - i + work->progress) % work->progress + 0x9a),
                                      0,PAL_DEFAULT,g_drawPos,0,work->angle,g_ot + g_otIndex,8);
                }
            }
            else
            {
                g_drawPos[0] = work->scrollX + 0xa0;
                g_drawPos[1] = work->y;
                g_drawPos[2] = work->scaleX;
                g_drawPos[3] = work->scaleY;
                Sprite_DrawScaled((u16)GROUP_SPRITE(5,work->stage + 0x9a),0,PAL_DEFAULT,g_drawPos,0,
                                  work->angle,g_ot + g_otIndex,8);
            }
        }
        MenuItems_Draw(work->count);
    }
    return 0;
}

/* MoneyRules_Task's rows (work->row) */
enum MoneyRow
{
    MONEY_ROW_OK,                /* cross here goes on */
    MONEY_ROW_GOAL,              /* the goal money */
    MONEY_ROW_START,             /* the start money */
    MONEY_ROW_BANKRUPT           /* the bankruptcies that end the game */
};

/* MoneyRules_Task's menu items: the title, then the rows' labels */
enum MoneyItem
{
    MONEYITEM_TITLE,
    MONEYITEM_OK,
    MONEYITEM_GOAL,
    MONEYITEM_START,
    MONEYITEM_BANKRUPT
};

/* A change of a money rule with left / right (row 1 the goal, 10000 to 99900 and above the start
 * money + 100; row 2 the start money, 1000 to 20000 and below the goal - 100; row 3 the
 * bankruptcies that end the game, 1 to the players - 1), by 100 / 1; 0 at the limit */
static int MoneyRules_Change(int row, int up)
{
    GameConfig *cfg = &g_gameConfig;

    switch (row)
    {
    case MONEY_ROW_GOAL:
        if (up)
        {
            if (cfg->goalMoney > 99899)
            {
                return 0;
            }
            cfg->goalMoney += 100;
        }
        else
        {
            if (cfg->goalMoney < 10001 || cfg->goalMoney <= cfg->startMoney + 100)
            {
                return 0;
            }
            cfg->goalMoney -= 100;
        }
        return 1;
    case MONEY_ROW_START:
        if (up)
        {
            if (cfg->startMoney > 19999 || cfg->goalMoney - 100 <= cfg->startMoney)
            {
                return 0;
            }
            cfg->startMoney += 100;
        }
        else
        {
            if (cfg->startMoney < 1001)
            {
                return 0;
            }
            cfg->startMoney -= 100;
        }
        return 1;
    case MONEY_ROW_BANKRUPT:
        if (up)
        {
            if (cfg->numPlayers - 1 <= cfg->bankruptLimit)
            {
                return 0;
            }
            cfg->bankruptLimit++;
        }
        else
        {
            if (cfg->bankruptLimit < 2)
            {
                return 0;
            }
            cfg->bankruptLimit--;
        }
        return 1;
    }
    return 0;
}

/* MoneyRules_Task steps */
enum MoneyRulesStep
{
    MONEY_INIT           =   0,
    MONEY_ITEM_IN        =   1,  /* the items come in two frames apart */
    MONEY_ITEM_DELAY     =   2,
    MONEY_WAIT_ITEMS     =   3,
    MONEY_FADE_IN        =  10,
    MONEY_CHOOSE         =  20,  /* up / down the row, left / right the value */
    MONEY_FADE_OUT       = 100,
    MONEY_ITEM_OUT       = 101,
    MONEY_ITEM_OUT_DELAY = 102,
    MONEY_WAIT_ITEMS_OUT = 103,
    MONEY_DONE           = 104 
};

/* Start money, goal money and how many bankruptcies end the game (0x8003DC78) */
/* 0x8003DC78 */
u32 MoneyRules_Task(short *step, MoneyRulesWork *work)
{
    MenuItem *menuItem;
    int i;

    switch(*step)
    {
    case MONEY_INIT:
        work->shown = 1;
        work->fade = 0x100;
        work->unused06 = 0;
        work->row = MONEY_ROW_OK;
        work->count = 5;
        for (i = 0; i < (short)work->count; i++)
        {
            menuItem = &g_menuItems[i];
            menuItem->scale = 0x1000;
            menuItem->scaleY = 0x333;
            menuItem->offsetX = 0x140;
            menuItem->state = MENUITEM_STILL;
            menuItem->offsetY = 0;
            switch (i)
            {
            case MONEYITEM_TITLE:
                menuItem->sprite = (work->story == 0) ? 0x4d : 0xb8;
                menuItem->x = 0x10;
                menuItem->y = 0x10;
                break;
            case MONEYITEM_OK:       menuItem->sprite = 0x7c; menuItem->x = 0x30; menuItem->y = 0x28; break;
            case MONEYITEM_GOAL:     menuItem->sprite = 0x61; menuItem->x = 0x30; menuItem->y = 0x40; break;
            case MONEYITEM_START:    menuItem->sprite = 0x60; menuItem->x = 0x30; menuItem->y = 0x58; break;
            case MONEYITEM_BANKRUPT: menuItem->sprite = 0x5f; menuItem->x = 0x30; menuItem->y = 0x70; break;
            }
        }
        work->item = 0;
        *step += 1;
        break;
    case MONEY_ITEM_IN:
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        if (++work->item == work->count)
        {
            *step = MONEY_WAIT_ITEMS;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case MONEY_ITEM_DELAY:
        work->delay--;
        if (work->delay == 0)
        {
            *step = MONEY_ITEM_IN;
        }
        break;
    case MONEY_WAIT_ITEMS:
        if (g_menuItems[(short)work->count - 1].state == MENUITEM_STILL)
        {
            *step = MONEY_FADE_IN;
        }
        break;
    case MONEY_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->fade = 0;
            *step = MONEY_CHOOSE;
        }
        break;
    case MONEY_CHOOSE:
        if (((g_playerPads[6].pressed & PADRdown) != 0) && (work->row == MONEY_ROW_OK))
        {
            g_menuResult = MENU_RESULT_NEXT;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = MONEY_FADE_OUT;
        }
        if ((work->story < 2) && ((g_playerPads[6].pressed & PADRright) != 0))
        {
            g_menuResult = MENU_RESULT_BACK;
            Se_Play(SE_CANCEL,0x7f,0x7f,1);
            *step = MONEY_FADE_OUT;
        }
        else if (work->story == 0)
        {
            if (g_playerPads[6].repeat & PADLup)
            {
                work->row = (work->row == MONEY_ROW_OK) ? MONEY_ROW_BANKRUPT : work->row - 1;
            }
            else if (g_playerPads[6].repeat & PADLdown)
            {
                work->row = (work->row == MONEY_ROW_BANKRUPT) ? MONEY_ROW_OK : work->row + 1;
            }
            else if (g_playerPads[6].repeat & PADLright)
            {
                if (!MoneyRules_Change(work->row,1))
                {
                    break;
                }
            }
            else if (g_playerPads[6].repeat & PADLleft)
            {
                if (!MoneyRules_Change(work->row,0))
                {
                    break;
                }
            }
            else
            {
                break;
            }
            Se_Play(SE_CURSOR,0x7f,0x7f,1);
        }
        break;
    case MONEY_FADE_OUT:
        work->fade += 0x40;
        if (work->fade > 0xff)
        {
            work->fade = 0x100;
            *step += 1;
        }
        work->item = 0;
        break;
    case MONEY_ITEM_OUT:
        g_menuItems[work->item].state = MENUITEM_FOLD;
        if (++work->item == work->count)
        {
            *step = MONEY_WAIT_ITEMS_OUT;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case MONEY_ITEM_OUT_DELAY:
        work->delay--;
        if (work->delay == 0)
        {
            *step = MONEY_ITEM_OUT;
        }
        break;
    case MONEY_WAIT_ITEMS_OUT:
        if (g_menuItems[(short)work->count - 1].state == MENUITEM_STILL)
        {
            *step += 1;
        }
        break;
    case MONEY_DONE:
        work->shown = 0;
        return 1;
    }
    if (work->shown != 0)
    {
        g_drawPos[1] = 0x28 + work->row * 0x18;      /* the band on the row */
        Menu_DrawBand(g_drawPos[1],0x10,work->fade);
        if (work->fade == 0)
        {
            g_drawPos[0] = 0x18;
            Sprite_Draw(GROUP_SPRITE(5,0x78) + g_frameVsync / 8 % 4,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
        }
        MenuItems_Draw(work->count);
        if (work->fade != 0)
        {
            return 0;
        }
        g_drawPos[0] = 0xc0;
        g_drawPos[1] = 0x40;
        Gfx_DrawNumber(g_gameConfig.goalMoney,0xc0,0x40,6,0x10,g_spriteGroups[5].base + 0x52,0,
                       g_ot + g_otIndex,8);
        g_drawPos[0] = 0xc0;
        g_drawPos[1] = 0x58;
        Gfx_DrawNumber(g_gameConfig.startMoney,0xc0,0x58,6,0x10,g_spriteGroups[5].base + 0x52,0,
                       g_ot + g_otIndex,8);
        g_drawPos[0] = 0xc0;
        g_drawPos[1] = 0x70;
        Gfx_DrawNumber(g_gameConfig.bankruptLimit,0xc0,0x70,6,0x10,
                       g_spriteGroups[5].base + 0x52,0,g_ot + g_otIndex,8);
    }
    return 0;
}

/* Move v 8 pixels toward target; 1 once it is there */
static int TurnOrder_StepToward(short *v, short target)
{
    if (*v == target)
    {
        return 1;
    }
    if (target < *v)
    {
        *v -= 8;
        if (target < *v)
        {
            return 0;
        }
    }
    else
    {
        *v += 8;
        if (*v < target)
        {
            return 0;
        }
    }
    *v = target;
    return 1;
}

/* The character animation of a turn order slot (k_charAnims*, its anim) for character charId */
static CharAnimFrame *TurnOrder_Anim(TurnOrderSlot *slot, u8 charId)
{
    if (charId == 0)
    {
        return k_charAnimsChar0[slot->anim];
    }
    if (charId == 4)
    {
        return k_charAnimsChar4[slot->anim];
    }
    return k_charAnims[slot->anim];
}

/* A character on the turning wheel, seen from the side: smaller and higher at the back (where the
 * back view sprites, +0x50, are used), with its shadow */
static void TurnOrder_DrawOnWheel(TurnOrderWork *work, TurnOrderSlot *slot, u16 sprite, int charId)
{
    short radius;
    short angle;
    short across;
    short depth;
    short pri;
    int scale;

    radius = slot->radius / 0x1000;
    angle = work->spin + slot->angle;
    across = radius * -rsin(angle) / 0x1000;
    depth = radius * -rcos(angle) / 0x1000;   /* < 0 at the front */
    scale = 0x1000 - depth * 0x666 / 0xc0;
    across = scale * across / 0x1000;
    pri = (radius == 0) ? 8 : (depth + radius) * 8 / radius + 8;
    g_drawPos[0] = across + 0xa0;
    g_drawPos[1] = 0xa8 - (depth * 0x28 / 0xc0);
    g_drawPos[2] = scale / 0x80;
    g_drawPos[3] = scale * 0x30 / 0x1000;
    if ((short)sprite > 0x4f && depth >= 0)
    {
        sprite += 0x50;
    }
    Sprite_DrawStretched((u16)(GROUP_SPRITE(4,sprite) + charId * 5),0,k_charPalettes[charId],g_drawPos,
                         0x20,g_ot + g_otIndex,pri);
    g_drawPos[0] = across + 0xa0;
    g_drawPos[1] = 0xa6 - (depth * 0x28 / 0xc0);
    g_drawPos[2] = scale * 0x24 / 0x1000;
    g_drawPos[3] = scale * 9 / 0x1000;
    Sprite_DrawStretched((u16)GROUP_SPRITE(0,0x71),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,
                         g_ot + g_otIndex,pri);
}

/* TurnOrderWork.state */
#define TORDER_STATE_OFF    0    /* nothing drawn */
#define TORDER_STATE_ON     1    /* the characters and the list's bands */
#define TORDER_STATE_NAMES  2    /* the same, the names' texts made */

/* TurnOrder_Task steps */
enum TurnOrderStep
{
    TORDER_INIT        =   0,
    TORDER_ITEM_IN     =   1,  /* the items come in two frames apart */
    TORDER_ITEM_DELAY  =   2,
    TORDER_WAIT_ITEMS  =   3,
    TORDER_WHEEL       =  20,  /* the characters spread around the wheel, far out */
    TORDER_WHEEL_IN    =  21,  /* each appears past a quarter turn, then comes in */
    TORDER_PAUSE       =  22,  /* 60 frames */
    TORDER_WAIT_PAUSE  =  23,
    TORDER_TO_MIDDLE   =  24,
    TORDER_DRAW        =  30,  /* the order drawn */
    TORDER_WALK        =  31,  /* the next in the order walks to its place */
    TORDER_NEXT        =  32,
    TORDER_LIST        =  40,  /* the names */
    TORDER_FADE_IN     =  50,
    TORDER_WAIT_BUTTON =  60,
    TORDER_DONE        = 100 
};

/* Shuffling the characters to decide the turn order (debug name task_char_shuffle): the title
 * comes in; the characters walk onto a turning wheel from the edge, gather in the middle, then go
 * one by one, in the order Util_RandomPermutation drew, to their place in the list; cross goes
 * on (GameConfig.turnOrder) (0x8003E51C) */
/* 0x8003E51C */
u32 TurnOrder_Task(short *step, TurnOrderWork *work)
{
    TurnOrderSlot *slot;
    CharAnimFrame *anim;
    MenuItem *item;
    u16 frame;
    u16 sprite;
    u16 turned;
    int charId;
    int arrived;
    int place;
    int i;

    FntPrint("exec task_char_shuffle\n");
    FntPrint("phase : %d\n", (u16)*step);
    switch (*step)
    {
    case TORDER_INIT:
        work->state = TORDER_STATE_ON;
        work->fade = 0x100;
        work->unused01 = 0;
        work->count = 1;
        for (i = 0; i < work->count; i++)
        {
            item = &g_menuItems[i];
            item->state = MENUITEM_STILL;
            item->scale = 0x1000;
            item->scaleY = 0x333;
            item->offsetX = 0x140;
            item->offsetY = 0;
            if (i == 0)
            {
                g_menuItems[0].sprite = 0x4e;
                g_menuItems[0].x = 0x10;
                g_menuItems[0].y = 0x10;
            }
        }
        Util_RandomPermutation(g_gameConfig.numPlayers,work->order);
        work->item = 0;
        for (i = 5; i >= 0; i--)
        {
            g_turnOrderSlots[i].shown = 0;
        }
        *step += 1;
        break;
    case TORDER_ITEM_IN:
        /* the menu items come in two frames apart */
        g_menuItems[work->item].state = MENUITEM_SLIDE_IN;
        work->item++;
        if (work->item == work->count)
        {
            *step = TORDER_WAIT_ITEMS;
            break;
        }
        work->delay = 2;
        *step += 1;
        break;
    case TORDER_ITEM_DELAY:
        if (--work->delay == 0)
        {
            *step = TORDER_ITEM_IN;
        }
        break;
    case TORDER_WAIT_ITEMS:
        if (g_menuItems[work->count - 1].state == MENUITEM_STILL)
        {
            *step = TORDER_WHEEL;
        }
        break;
    case TORDER_WHEEL:
        /* the wheel: the characters spread around it, far out */
        work->players = g_gameConfig.numPlayers;
        work->spin = 0;
        work->spinSpeed = 0x5b;
        work->spinStep = 0x1000 / work->players;
        for (i = 0; i < work->players; i++)
        {
            slot = &g_turnOrderSlots[i];
            slot->shown = 0;
            slot->placed = 0;
            slot->anim = 1;
            slot->lastAnim = 1;
            slot->frame = 0;
            slot->timer = 0;
            slot->radius = 0xc0000;
            slot->started = 0;
            slot->angle = -((i << 12) / work->players);
        }
        *step += 1;
        break;
    case TORDER_WHEEL_IN:
        /* each one appears when it turns past a quarter turn, then comes in to radius 16 */
        arrived = 1;
        for (i = 0; i < work->players; i++)
        {
            slot = &g_turnOrderSlots[i];
            turned = work->spin + slot->angle;
            if ((u16)(turned - 0x400) < 0x5b || (u16)(turned + 0x400) < 0x5b)
            {
                if (slot->started != 0)
                {
                    slot->shown = 1;
                }
            }
            else
            {
                slot->started = 1;
            }
            if (slot->shown == 0)
            {
                arrived = 0;
            }
            else
            {
                slot->radius -= 0x4000;
                if (slot->radius > 0x10000)
                {
                    arrived = 0;
                }
                else
                {
                    slot->radius = 0x10000;
                }
            }
        }
        if (arrived)
        {
            *step += 1;
        }
        break;
    case TORDER_PAUSE:
        work->delay = 60;
        *step += 1;
        break;
    case TORDER_WAIT_PAUSE:
        if (--work->delay == 0)
        {
            *step += 1;
        }
        break;
    case TORDER_TO_MIDDLE:
        /* into the middle */
        for (i = 0; i < work->players; i++)
        {
            g_turnOrderSlots[i].radius -= 0x4000;
        }
        if (g_turnOrderSlots[0].radius < 1)
        {
            for (i = 0; i < work->players; i++)
            {
                g_turnOrderSlots[i].radius = 0;
            }
            *step = TORDER_DRAW;
        }
        break;
    case TORDER_DRAW:
        work->spinSpeed = 0;
        work->rowH = 0x90 / work->players;
        for (i = 0; i < work->players; i++)
        {
            slot = &g_turnOrderSlots[i];
            slot->placed = 1;
            slot->x = 0xa0;
            slot->y = 0xa8;
        }
        work->pick = 0;
        *step += 1;
        break;
    case TORDER_WALK:
        /* the next in the drawn order walks to its place */
        slot = &g_turnOrderSlots[work->order[work->pick]];
        arrived = TurnOrder_StepToward(&slot->x,work->pick * 0x24 + 0x38);
        if (!TurnOrder_StepToward(&slot->y,work->pick * 0x18 + 0x53))
        {
            arrived = 0;
        }
        if (arrived)
        {
            slot->anim = 0;
            *step = TORDER_NEXT;
        }
        break;
    case TORDER_NEXT:
        work->pick++;
        *step = (work->pick == work->players) ? TORDER_LIST : TORDER_WALK;
        break;
    case TORDER_LIST:
        work->state = TORDER_STATE_NAMES;
        for (i = 0; i < work->players; i++)
        {
            g_menuRowItems[i] = Text_Create(g_gameConfig.players[work->order[i]].name);
        }
        *step = TORDER_FADE_IN;
        break;
    case TORDER_FADE_IN:
        work->fade -= 0x40;
        if (work->fade < 1)
        {
            work->fade = 0;
            *step = TORDER_WAIT_BUTTON;
        }
        break;
    case TORDER_WAIT_BUTTON:
        if (g_playerPads[6].pressed & PADRdown)
        {
            g_menuResult = MENU_RESULT_NEXT;
            Se_Play(SE_OK,0x7f,0x7f,1);
            *step = TORDER_DONE;
        }
        break;
    case TORDER_DONE:
        for (i = 0; i < g_gameConfig.numPlayers; i++)
        {
            g_gameConfig.turnOrder[i] = work->order[i];
            Text_Free(g_menuRowItems[i]);
        }
        work->state = TORDER_STATE_OFF;
        return 1;
    }

    if (work->state != TORDER_STATE_OFF)
    {
        /* the list: a band per turn with its number and (once faded in) the name */
        for (i = 0; i < g_gameConfig.numPlayers; i++)
        {
            Menu_DrawBand(0x3c + i * 0x18,0x10,work->fade);
            if (work->fade == 0)
            {
                g_drawPos[0] = (i < 3) ? 0x78 + i * 0x24 : -0x44 + i * 0x24;
                g_drawPos[1] = 0x3e + i * 0x18;
                Text_Draw(g_menuRowItems[i],g_drawPos[0],0x3e + i * 0x18,g_ot + g_otIndex,8);
                g_drawPos[0] = 0x10;
                g_drawPos[1] = 0x3c + i * 0x18;
                Sprite_Draw(GROUP_SPRITE(5,0x14) + i,0,PAL_DEFAULT,g_drawPos,g_ot + g_otIndex,8);
            }
        }
        MenuItems_Draw(work->count);
    }
    work->spin += work->spinSpeed;
    if (work->state <= TORDER_STATE_OFF)
    {
        return 0;
    }
    for (i = 0; i < work->players; i++)
    {
        slot = &g_turnOrderSlots[i];
        charId = (char)g_gameConfig.players[i].charId;
        if (slot->lastAnim != slot->anim)
        {
            slot->frame = 0;
            slot->timer = 0;
            slot->lastAnim = slot->anim;
        }
        anim = TurnOrder_Anim(slot,g_gameConfig.players[i].charId);
        frame = slot->frame;
        sprite = anim[frame].sprite;
        if (++slot->timer == (char)anim[frame].frames)
        {
            slot->frame = (anim[frame].next == -2) ? frame + 1 : anim[frame].next;
            slot->timer = 0;
        }
        if (slot->shown == 0)
        {
            continue;
        }
        if (slot->placed == 0)
        {
            TurnOrder_DrawOnWheel(work,slot,sprite,charId);
        }
        else if (slot->placed == 1)
        {
            /* in its place, in the color of its turn */
            for (place = 0; place < 6 && work->order[place] != i; place++)
            {
            }
            g_drawPos[0] = slot->x;
            g_drawPos[1] = slot->y;
            Sprite_DrawAligned(GROUP_SPRITE(4,sprite) + charId * 5,0,place,g_drawPos,ALIGN_BOTTOM,g_ot + g_otIndex,8);
            g_drawPos[0] = slot->x;
            g_drawPos[1] = slot->y - 2;
            Sprite_DrawAligned(GROUP_SPRITE(0,0x71),GFX_SEMITRANS,PAL_BLANK,g_drawPos,0,g_ot + g_otIndex,8);
        }
    }
    return 0;
}

/* Story Mode stage setup: player 1 human, the stage's CPU opponents (k_storyOpponents), its
 * money to start and to win (g_stageMoneyTable) (0x80042900) */
/* 0x80042900 */
void Story_SetupPlayers(void)
{
    PlayerSetup *ps;
    int i;

    g_gameConfig.playMode = PLAY_STAGE;
    g_resumedGame = 0;
    g_gameConfig.numHumans = 1;
    g_gameConfig.numPlayers = 4;
    g_gameConfig.numCpus = 3;
    g_gameConfig.bankruptLimit = 3;
    g_gameConfig.startMoney = g_stageMoneyTable[(char)g_gameConfig.stage].startMoney;
    g_gameConfig.goalMoney = g_stageMoneyTable[(char)g_gameConfig.stage].goalMoney;
    g_gameConfig.maxBuy = 2;
    for (i = 0; i < g_gameConfig.numPlayers; i++)
    {
        ps = &g_gameConfig.players[i];
        ps->isCpu = (i < (char)g_gameConfig.numHumans) ? 0 : 1;
        ps->pad = 0;
        ps->charId = k_storyOpponents[(char)g_gameConfig.stage * 4 + i];
        strcpy(ps->name,g_cpuNames[(char)ps->charId]);
        g_gameConfig.turnOrder[i] = i;
    }
}
