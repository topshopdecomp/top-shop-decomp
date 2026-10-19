/*
 * script.c - the event script interpreter
 *
 * The game's scenes (the events, the story mode's scenes, the calendar pages and the characters'
 * comments) are bytecode scripts; scripts/ holds them as text. This file loads a script file,
 * runs a script in a task (EventScript_Task, with Script_Run's dispatcher over the 121 opcodes),
 * and has the opcodes for flow (goto, switch, call, if), variables and arithmetic, waiting for
 * frames and buttons, music, sound effects, XA voice and fades.
 */
#include "common.h"
#include "topshop.h"

/* LoadJWithTim_Task steps */
enum LoadJWithTimStep
{
    LOADJ_READ       =  0,  /* start reading the .J file */
    LOADJ_UNPACK_TIM =  1,  /* read: unpack its TIM (entry 2), if it has one */
    LOADJ_UPLOAD_TIM =  2,  /* upload the TIM to VRAM */
    LOADJ_FREE_TIM   =  3,  /* free the unpacked TIM, drop it from the file's block */
    LOADJ_DONE       = 10   /* the script and the scene's images from entries 0 and 1 */
};

/* Task func: load a .J archive (file id in work->file) for the event scripts: entry 0 the script
 * (g_scriptData), entry 1 the scene images (g_sceneImages, g_sceneSheet.frames); an entry 2, if
 * there is one, is an LZSS-packed TIM, unpacked into a temporary 80-sector block, uploaded to
 * VRAM and then cut off the file's block (0x80042EC4) */
/* 0x80042EC4 */
int LoadJWithTim_Task(short *step, LoadJWithTimWork *work)
{
    u8 *file;

    switch(*step)
    {
    case LOADJ_READ:
        work->handle = File_ReadAsync(work->file,NULL,FILE_WHOLE,1);
        g_scriptBlock = g_fileHandles[work->handle].block;
        *step += 1;
        break;
    case LOADJ_UNPACK_TIM:
        if (File_IsDone(work->handle) != 1)
        {
            return 0;
        }
        file = g_secHeapBlocks[g_scriptBlock].addr;
        if (*(int *)file == 1)
        {
            /* no packed TIM */
            *step = LOADJ_DONE;
            return 0;
        }
        work->block = SecHeap_Alloc(80);
        Lzss_Decompress((int *)ARCHIVE_ENTRY(file,2),g_secHeapBlocks[work->block].addr);
        *step += 1;
        break;
    case LOADJ_UPLOAD_TIM:
        Tim_UploadStream(g_secHeapBlocks[work->block].addr,NULL);
        *step += 1;
        break;
    case LOADJ_FREE_TIM:
        SecHeap_Free(work->block);
        file = g_secHeapBlocks[g_scriptBlock].addr;
        SecHeap_Shrink(g_scriptBlock,(ARCHIVE_ENTRY(file,2) - file + 0x7ff) / 0x800);
        *step = LOADJ_DONE;
        /* fall through */
    case LOADJ_DONE:
        File_ReleaseHandle(work->handle);
        file = g_secHeapBlocks[g_scriptBlock].addr;
        g_scriptData = ARCHIVE_ENTRY(file,0);
        g_sceneImages = (VramImage *)ARCHIVE_ENTRY(file,1);
        g_sceneSheet.frames = (TileFrame *)ARCHIVE_ENTRY(file,1);
        return 1;
    }
    return 0;
}

/* Reset the event script interpreter: program counter to the start of the script data, variables
 * (g_scriptVars), work areas and flags cleared (0x8007C034) */
/* 0x8007C034 */
void Script_Reset(void)
{
    int i;

    g_scriptDepth = 0;
    g_scriptWait = 0;
    g_scriptRunning = 1;
    g_scriptPc = g_scriptData;
    for (i = 0; i < 256; i++)
    {
        g_scriptVars[i] = 0;
    }
    g_sceneView.visible = 0;
    g_sceneView.transition = VIEW_NO_TRANSITION;
    for (i = 0; i < 4; i++)
    {
        g_sceneLayers[i].active = 0;
    }
    for (i = 0; i < 64; i++)
    {
        g_sceneSprites[i].active = 0;
    }
    for (i = 0; i < 16; i++)
    {
        g_sceneRects[i].active = 0;
    }
    g_talk.state = TALK_HIDDEN;
    g_talk.cursorRow = -1;
    g_talk.hasText = 0;
    g_talk.speaker = 0;
    g_talk.frame = 6;               /* the host */
    Talk_Clear();
    g_portrait.shown = 0;
    g_scriptErrorShown = 0;
}

/* Run opcodes until one waits for the next frame; 1 once the script has ended. scratch: work area
 * for the opcodes that span several frames. An unknown opcode shows an error message (0x8007C124) */
/* 0x8007C124 */
u32 Script_Run(ScriptScratch *scratch)
{
    u32 opcode;
    u32 more;

    do
    {
        if (g_scriptRunning == 0)
        {
            return 1;
        }
        g_scriptOpOffset = g_scriptPc - g_scriptData;
        opcode = MAKE_U16(g_scriptPc[0],g_scriptPc[1]);
        g_scriptOpcode = opcode;
        switch (opcode)
        {
        case OP_END:
            more = Script_OpEnd();
            break;
        case OP_WAIT:
            more = Script_OpWait();
            break;
        case OP_WAIT_BUTTONS:
            more = Script_OpWaitButtons();
            break;
        case OP_WAIT_BUTTONS_FOR:
            more = Script_OpWaitButtonsFor();
            break;
        case OP_GOTO:
            more = Script_OpGoto();
            break;
        case OP_SWITCH:
            more = Script_OpSwitch();
            break;
        case OP_CALL:
            more = Script_OpCall();
            break;
        case OP_RETURN:
            more = Script_OpReturn();
            break;
        case OP_HALT:
            more = Script_OpHalt();
            break;
        case OP_IF:
            more = Script_OpIf();
            break;
        case OP_SET:
            more = Script_OpSet();
            break;
        case OP_ADD:
            more = Script_OpAdd();
            break;
        case OP_SUB:
            more = Script_OpSub();
            break;
        case OP_MUL:
            more = Script_OpMul();
            break;
        case OP_DIV:
            more = Script_OpDiv();
            break;
        case OP_MOD:
            more = Script_OpMod();
            break;
        case OP_AND:
            more = Script_OpAnd();
            break;
        case OP_OR:
            more = Script_OpOr();
            break;
        case OP_XOR:
            more = Script_OpXor();
            break;
        case OP_NEGATE:
            more = Script_OpNeg();
            break;
        case OP_RANDOM:
            more = Script_OpRandom();
            break;
        case OP_GET_INDEXED:
            more = Script_OpGetIndexed();
            break;
        case OP_SET_INDEXED:
            more = Script_OpSetIndexed();
            break;
        case OP_PLAY_BGM:
            more = Script_OpPlayBgm();
            break;
        case OP_STOP_BGM:
            more = Script_OpStopBgm();
            break;
        case OP_PLAY_SE:
            more = Script_OpPlaySe();
            break;
        case OP_NOP_33:
            more = Script_OpNop33();
            break;
        case OP_NOP_34:
            more = Script_OpNop34();
            break;
        case OP_NOP_35:
            more = Script_OpNop35();
            break;
        case OP_PLAY_XA:
            more = Script_OpPlayXa();
            break;
        case OP_STOP_XA:
            more = Script_OpStopXa();
            break;
        case OP_YIELD:
            more = Script_OpYield();
            break;
        case OP_IS_XA_PLAYING:
            more = Script_OpIsXaPlaying();
            break;
        case OP_FADE_IN:
            more = Script_OpFadeIn();
            break;
        case OP_FADE_OUT:
            more = Script_OpFadeOut();
            break;
        case OP_FADE_FROM_WHITE:
            more = Script_OpFadeFromWhite();
            break;
        case OP_FADE_TO_WHITE:
            more = Script_OpFadeToWhite();
            break;
        case OP_FADE_LEVEL_FOR:
            more = Script_OpFadeLevelFor();
            break;
        case OP_WAIT_FADE:
            more = Script_OpWaitFade();
            break;
        case OP_MARKER:
            more = Script_OpText();
            break;
        case OP_VIEW_SET:
            more = Script_OpSetView();
            break;
        case OP_VIEW_OPEN:
            more = Script_OpViewOpen();
            break;
        case OP_VIEW_CLOSE:
            more = Script_OpViewClose();
            break;
        case OP_VIEW_WAIT:
            more = Script_OpViewWait();
            break;
        case OP_LAYER_INIT:
            more = Script_OpLayerInit();
            break;
        case OP_LAYER_POS:
            more = Script_OpLayerSetPos();
            break;
        case OP_LAYER_SCROLL:
            more = Script_OpLayerScroll();
            break;
        case OP_LAYER_LINES:
            more = Script_OpLayerSetLines();
            break;
        case OP_LAYER_LINE_STEPS:
            more = Script_OpLayerSetLineSteps();
            break;
        case OP_LAYER_WAVE:
            more = Script_OpLayerWave();
            break;
        case OP_LAYER_WAVE_SCROLL:
            more = Script_OpLayerWaveScroll();
            break;
        case OP_SPRITE_INIT:
            more = Script_OpSpriteInit();
            break;
        case OP_SPRITE_HIDE:
            more = Script_OpSpriteHide();
            break;
        case OP_SPRITE_IMAGE:
            more = Script_OpSpriteSetImage();
            break;
        case OP_SPRITE_ATTR:
            more = Script_OpSpriteSetAttr();
            break;
        case OP_SPRITE_MOVE_BY:
            more = Script_OpSpriteMoveBy();
            break;
        case OP_SPRITE_POS:
            more = Script_OpSpriteSetPos();
            break;
        case OP_SPRITE_MOVE_TO:
            more = Script_OpSpriteMoveTo();
            break;
        case OP_SPRITE_SCALE_BY:
            more = Script_OpSpriteScaleBy();
            break;
        case OP_SPRITE_SCALE:
            more = Script_OpSpriteSetScale();
            break;
        case OP_NOP_89:
            more = Script_OpNop89();
            break;
        case OP_SPRITE_ROTATE:
            more = Script_OpSpriteRotate();
            break;
        case OP_SPRITE_ANGLE:
            more = Script_OpSpriteSetAngle();
            break;
        case OP_NOP_8F:
            more = Script_OpNop8F();
            break;
        case OP_SPRITE_ANIMATE:
            more = Script_OpSpriteAnimate();
            break;
        case OP_SPRITE_PATH:
            more = Script_OpSpritePath();
            break;
        case OP_SPRITE_GET_STATE:
            more = Script_OpSpriteGetState();
            break;
        case OP_SPRITE_GET_POS:
            more = Script_OpSpriteGetPos();
            break;
        case OP_SPRITE_GET_SCALE:
            more = Script_OpSpriteGetScale();
            break;
        case OP_SPRITE_GET_ANGLE:
            more = Script_OpSpriteGetAngle();
            break;
        case OP_GET_IMAGE_SIZE:
            more = Script_OpGetImageSize();
            break;
        case OP_SET_DIM:
            more = Script_OpSetDim();
            break;
        case OP_RECT_SET:
            more = Script_OpSetRect();
            break;
        case OP_RECT_HIDE:
            more = Script_OpHideRect();
            break;
        case OP_RECT_TO_VIEW:
            more = Script_OpSetRectToView();
            break;
        case OP_STEP_TOWARD:
            more = Script_OpStepToward();
            break;
        case OP_TALK_OPEN:
            more = Script_OpTalkOpen();
            break;
        case OP_TALK_CLOSE:
            more = Script_OpTalkClose();
            break;
        case OP_TALK_WAIT_OPEN:
            more = Script_OpTalkWaitOpen();
            break;
        case OP_TALK_WAIT_CLOSED:
            more = Script_OpTalkWaitClosed();
            break;
        case OP_TALK_CURSOR:
            more = Script_OpTalkCursor();
            break;
        case OP_TALK_NO_CURSOR:
            more = Script_OpTalkNoCursor();
            break;
        case OP_TALK_SPEAKER:
            more = Script_OpTalkSpeaker();
            break;
        case OP_TALK_CLEAR:
            more = Script_OpTalkClear();
            break;
        case OP_TALK_SHOW:
            more = Script_OpTalkShowText();
            break;
        case OP_TALK:
            more = Script_OpTalkPrint();
            break;
        case OP_TALK_NUMBER:
            more = Script_OpTalkNumber();
            break;
        case OP_TALK_PLAYER:
            more = Script_OpTalkPrintPlayer();
            break;
        case OP_TALK_STORE_TYPE:
            more = Script_OpTalkPrintStoreType();
            break;
        case OP_GET_TALK_Y:
            more = Script_OpGetTalkY();
            break;
        case OP_TALK_DATA:
            more = Script_OpTalkPrintData();
            break;
        case OP_NOP_D0:
            more = Script_OpNopD0();
            break;
        case OP_NOP_D1:
            more = Script_OpNopD1();
            break;
        case OP_NOP_F1:
            more = Script_OpNopF1();
            break;
        case OP_NOP_F2:
            more = Script_OpNopF2();
            break;
        case OP_PORTRAIT_SHOW:
            more = Script_OpPortraitShow(scratch);
            break;
        case OP_PORTRAIT_HIDE:
            more = Script_OpPortraitHide(scratch);
            break;
        case OP_PORTRAIT_FACE:
            more = Script_OpPortraitFace();
            break;
        case OP_COUNT_STORES:
            more = Script_OpCountStores();
            break;
        case OP_GET_STORE:
            more = Script_OpGetStore();
            break;
        case OP_EARN:
            more = Script_OpPlayerEarn();
            break;
        case OP_STORES_SET_GOODS:
            more = Script_OpStoresSetGoods();
            break;
        case OP_NOP_182:
            more = Script_OpNop182();
            break;
        case OP_NOP_183:
            more = Script_OpNop183();
            break;
        case OP_BUY_LOT:
            more = Script_OpBuyLot(scratch);
            break;
        case OP_SELL_STORE:
            more = Script_OpSellStore(scratch);
            break;
        case OP_NOP_186:
            more = Script_OpNop186();
            break;
        case OP_NOP_187:
            more = Script_OpNop187();
            break;
        case OP_RESULT_ANOTHER:
            more = Script_OpResult1();
            break;
        case OP_ADVANCE_MONTHS:
            more = Script_OpAdvanceMonths();
            break;
        case OP_MOVE_STEPS:
            more = Script_OpSetSteps();
            break;
        case OP_TURN_BACK:
            more = Script_OpPlayerTurnBack();
            break;
        case OP_NO_ELEVATOR:
            more = Script_OpNoElevator();
            break;
        case OP_STORES_SET_PRICE:
            more = Script_OpStoresSetPrice();
            break;
        case OP_MONEY_PERCENT:
            more = Script_OpMoneyPercent();
            break;
        case OP_COPY_MONEY:
            more = Script_OpCopyMoney();
            break;
        case OP_RESULT_REROLL:
            more = Script_OpResult2();
            break;
        case OP_STORES_CLOSE_FOR:
            more = Script_OpStoresCloseFor();
            break;
        case OP_STORES_CLOSE:
            more = Script_OpStoresClose(scratch);
            break;
        case OP_SET_SHOES:
            more = Script_OpSetShoes();
            break;
        case OP_STORES_GOODS_PRICIER:
            more = Script_OpStoresGoodsLevel();
            break;
        default:
            /* "Code: n / Address: offset" (in Japanese), once */
            more = 0;
            if (g_scriptErrorShown == 0)
            {
                g_scriptErrorShown = 1;
                strcpy(g_msgBuf,"%C\203R\201[\203h\201F");
                Str_FormatIntWide(opcode,0,(u8 *)g_msgBuf + strlen(g_msgBuf));
                strcat(g_msgBuf,"%K\203A\203h\203\214\203X\201F");
                Str_FormatIntWide(g_scriptPc - g_scriptData,0,(u8 *)g_msgBuf + strlen(g_msgBuf));
                Msg_Show(0,MSG_AUTO_POS,0x20,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADstart,g_msgBuf);
                g_sceneView.visible = 0;
            }
            break;
        }
    } while (more == 1);
    return 0;
}

/* The script's button bits for the pad buttons (several share one: cross and square, circle and
 * R1; L2 and start have none) */
static const struct
{
    u16 pad;
    u16 script;
} k_scriptButtons[] = {
    {PADLup, 0x1000}, {PADLright, 0x0001}, {PADLdown, 0x2000}, {PADLleft, 0x4000},
    {PADRup, 0x0040}, {PADRright, 0x0080}, {PADRdown, 0x0400}, {PADRleft, 0x0400},
    {PADL1, 0x0008}, {PADR1, 0x0080}, {PADR2, 0x0010}, {PADselect, 0x0020}
};

/* Pad buttons (PAD*) in the script's bits */
u16 Script_Buttons(u16 pad)
{
    u16 buttons = 0;
    int i;

    for (i = 0; i < (int)(sizeof(k_scriptButtons) / sizeof(k_scriptButtons[0])); i++)
    {
        if (pad & k_scriptButtons[i].pad)
        {
            buttons |= k_scriptButtons[i].script;
        }
    }
    return buttons;
}

/* Opcode 0x00: the script ends; free the talk panel glyphs (0x8007F3EC) */
/* 0x8007F3EC */
u32 Script_OpEnd(void)
{
    if (g_scriptWait == 0)
    {
        g_scriptRunning = 0;
        Talk_FreeGlyphs();
    }
    return 1;
}

/* The buttons of player 0's pad pressed this frame, in the script's bits */
static u16 Script_PressedButtons(void)
{
    return Script_Buttons(g_playerPads[0].pressed);
}

/* Opcode 0x02: wait until a button of the mask is pressed (pad 1); var 0xC0 = the buttons
 * (0x8007F520) */
/* 0x8007F520 */
u32 Script_OpWaitButtons(void)
{
    u16 buttons = Script_PressedButtons() & SCRIPT_ARG0;

    if (buttons != 0)
    {
        SCRIPT_RESULT = buttons;
        Script_Advance(4);
    }
    return 0;
}

/* Opcode 0x03: as 0x02 for at most n frames (var 0xC0 = 0 on time-out); with n = -1 it does
 * nothing and never ends (0x8007F600) */
/* 0x8007F600 */
u32 Script_OpWaitButtonsFor(void)
{
    u16 mask;
    short frames;
    u16 buttons;

    SCRIPT_READ_OPS(2);
    mask = SCRIPT_ARG(1);
    frames = SCRIPT_ARG(2);
    if (frames != -1)
    {
        g_scriptWait++;
        buttons = Script_PressedButtons() & mask;
        if (g_scriptWait == frames)
        {
            SCRIPT_RESULT = 0;
        }
        else
        {
            if (buttons == 0)
            {
                return 0;
            }
            SCRIPT_RESULT = buttons;
        }
        Script_Advance(6);
    }
    return 0;
}

/* Opcode 0x20: target, a, b, test: jump to target if the test of a and b holds: 0 a == b, 1 a != b,
 * 2 a > b, 3 a < b, 4 a >= b, 5 a <= b, 6 all bits of b set in a, 7 no common bits. Another test
 * holds or not as a register of Script_Run's happens to be; here it does not (0x8007F728) */
/* 0x8007F728 */
u32 Script_OpIf(void)
{
    short a;
    short b;
    int holds;

    SCRIPT_READ_OPS(4);
    a = SCRIPT_ARG(2);
    b = SCRIPT_ARG(3);
    switch (SCRIPT_ARG(4))
    {
    case TEST_EQ:
        holds = (a == b);
        break;
    case TEST_NE:
        holds = (a != b);
        break;
    case TEST_GT:
        holds = (a > b);
        break;
    case TEST_LT:
        holds = (a < b);
        break;
    case TEST_GE:
        holds = (a >= b);
        break;
    case TEST_LE:
        holds = (a <= b);
        break;
    case TEST_ALL_BITS:
        holds = ((a & b) == b);
        break;
    case TEST_NO_BITS:
        holds = ((a & b) == 0);
        break;
    default:
        holds = 0;
        break;
    }
    if (holds)
    {
        Script_Jump(g_scriptOps[1]);
    }
    else
    {
        Script_Advance(10);
    }
    return 1;
}

/* Opcode 0x2D: var = array[index]: a variable array, or a table of words at an offset in the
 * script data (0x8007F86C) */
/* 0x8007F86C */
u32 Script_OpGetIndexed(void)
{
    short index;
    u8 *word;

    SCRIPT_READ_OPS(3);
    index = SCRIPT_ARG(3);
    if (SCRIPT_IS_VAR(g_scriptOps[2]))
    {
        SCRIPT_RESULT = g_scriptVars[(g_scriptOps[2] & 0x7fff) + index];
    }
    else
    {
        word = g_scriptData + g_scriptOps[2] + index * 2;
        SCRIPT_RESULT = MAKE_U16(word[0],word[1]);
    }
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(8);
    return 1;
}

/* Opcode 0x44: set the fade level (r, g, b) for n frames, then restore the previous one. It
 * leaves g_scriptWait at 1 for the next opcode (0x8007F950) */
/* 0x8007F950 */
u32 Script_OpFadeLevelFor(void)
{
    short frames;
    short r;
    short g;
    short b;

    SCRIPT_READ_OPS(4);
    frames = SCRIPT_ARG(4);
    r = SCRIPT_ARG(1);
    g = SCRIPT_ARG(2);
    b = SCRIPT_ARG(3);
    if (g_scriptWait == 0)
    {
        Fade_GetLevel((u16 *)&g_scriptFadeSaved[0],(u16 *)&g_scriptFadeSaved[1],
                      (u16 *)&g_scriptFadeSaved[2]);
        Fade_SetLevel(r,g,b);
        g_scriptWait++;
    }
    if (g_scriptWait - 1 == frames)
    {
        Fade_SetLevel(g_scriptFadeSaved[0],g_scriptFadeSaved[1],g_scriptFadeSaved[2]);
        Script_Advance(10);
    }
    g_scriptWait++;
    return 0;
}

/* Opcode 0xB0: x, y, distance, angle: vars = the point x, y moved by distance in direction angle
 * (Math_Step) (0x80080B64) */
/* 0x80080B64 */
u32 Script_OpStepToward(void)
{
    short distance;
    short angle;
    int pos[2];

    SCRIPT_READ_OPS(6);
    pos[0] = SCRIPT_ARG(1) * 0x1000;
    pos[1] = SCRIPT_ARG(2) * 0x1000;
    distance = SCRIPT_ARG(3);
    angle = SCRIPT_ARG(4);
    Math_Step(angle >> 3,distance * 0x1000,pos);
    SCRIPT_VAR_SET(g_scriptOps[5],pos[0] / 0x1000);
    SCRIPT_VAR_SET(g_scriptOps[6],pos[1] / 0x1000);
    Script_Advance(14);
    return 1;
}

/* EventScript_Task steps */
enum EventScriptStep
{
    ESCRIPT_INIT    =   0,  /* reset the interpreter */
    ESCRIPT_START   =  10,
    ESCRIPT_RUN     =  50,  /* run the script a frame at a time until it ends */
    ESCRIPT_WAIT_XA = 100   /* ended: wait for the XA voice to finish */
};

/* Runs an event script loaded by the board (data pointer 0x800B269C / 0x800B2680). userData =
 * result (0..3) used by the turn logic. A message 0 from any task stops it: the voice is cut and
 * the task ends once the XA stream is over (0x80083B34) */
/* 0x80083B34 */
int EventScript_Task(short *step, EventScriptWork *work)
{
    switch (*step)
    {
    case ESCRIPT_INIT:
        work->running = 0;
        Task_SetUserData(-1,EVENT_RESULT_NONE);
        g_scriptRunning = 0;
        g_portraitSquash = 0;
        Script_Reset();
        *step = ESCRIPT_START;
        break;
    case ESCRIPT_START:
        *step = ESCRIPT_RUN;
        /* fall through */
    case ESCRIPT_RUN:
        work->running = 1;
        Scene_Update();
        if (Script_Run(&work->scratch) != 0)
        {
            *step = ESCRIPT_WAIT_XA;
        }
        break;
    case ESCRIPT_WAIT_XA:
        work->running = 0;
        if (Cd_IsXaPlaying() == 0)
        {
            return 1;
        }
        break;
    }
    if (work->running != 0 && Task_ReceiveMessageIf(TASK_ALL,0) == 0)
    {
        Xa_RequestStop();
        *step = ESCRIPT_WAIT_XA;
    }
    return 0;
}

/* Stop the event script (0x80083C38) */
/* 0x80083C38 */
void Script_Stop(void)
{
    g_scriptRunning = 0;
    g_portraitSquash = 0;
}

/* Move the script program counter past the current opcode (bytes long) (0x80083C48) */
/* 0x80083C48 */
void Script_Advance(u32 bytes)
{
    g_scriptWait = 0;
    g_scriptPc += (bytes & 0xffff);
}

/* Continue the script at offset target (a constant or a variable) (0x80083C64) */
/* 0x80083C64 */
void Script_Jump(short target)
{
    g_scriptWait = 0;
    g_scriptPc = g_scriptData + (short)Script_Value(target);
}

/* Continue at the start of the script (0x80083C9C) */
/* 0x80083C9C */
void Script_Restart(void)
{
    g_scriptWait = 0;
    g_scriptPc = g_scriptData;
}

/* Value of an operand: a constant, or g_scriptVars[operand & 0x7FFF] below -0x7F00 (0x80083CB0) */
/* 0x80083CB0 */
int Script_Value(u32 operand)
{
    if (!SCRIPT_IS_VAR(operand))
    {
        return (short)operand;
    }
    return g_scriptVars[operand & 0x7fff];
}

/* Opcode 0x01: wait n frames (-1 = forever); g_scriptWait counts them (0x80083CE8) */
/* 0x80083CE8 */
u32 Script_OpWait(void)
{
    short frames = SCRIPT_ARG0;

    if (frames != -1 && ++g_scriptWait == frames)
    {
        Script_Advance(4);
    }
    return 0;
}

/* Opcode 0x10: jump (0x80083D58) */
/* 0x80083D58 */
u32 Script_OpGoto(void)
{
    Script_Jump(SCRIPT_WORD(2));
    return 1;
}

/* Opcode 0x12: count, index, then count targets: jump to target index (past the list if out of
 * range) (0x80083D88) */
/* 0x80083D88 */
u32 Script_OpSwitch(void)
{
    short count;
    short index;

    SCRIPT_READ_OPS(2);
    index = SCRIPT_ARG(2);
    count = SCRIPT_ARG(1);
    if (index < 0 || count <= index)
    {
        Script_Advance(count * 2 + 6);
    }
    else
    {
        Script_Jump(SCRIPT_WORD(6 + index * 2));
    }
    return 1;
}

/* Opcode 0x13: call: push the return address and variables 0xF0-0xFF (Script_PushFrame, inline
 * in the original) (0x80083E40) */
/* 0x80083E40 */
u32 Script_OpCall(void)
{
    short target = SCRIPT_WORD(2);

    Script_PushFrame(g_scriptPc + 4);
    Script_Jump(target);
    return 1;
}

/* Opcode 0x14: return from a call, restoring variables 0xF0-0xFF (Script_PopFrame, inline in the
 * original) (0x80083EE4) */
/* 0x80083EE4 */
u32 Script_OpReturn(void)
{
    g_scriptWait = 0;
    g_scriptPc = Script_PopFrame();
    return 1;
}

/* Opcode 0x15: stay here (the script ends) (0x80083F6C) */
/* 0x80083F6C */
u32 Script_OpHalt(void)
{
    return 0;
}

/* Opcode 0x21: var = value (0x80083F7C) */
/* 0x80083F7C */
u32 Script_OpSet(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x2E: var[index] = value (0x80084000) */
/* 0x80084000 */
u32 Script_OpSetIndexed(void)
{
    short index;

    SCRIPT_READ_OPS(3);
    index = SCRIPT_ARG(2);
    SCRIPT_RESULT = SCRIPT_ARG(3);
    if (SCRIPT_IS_VAR(g_scriptOps[1]))
    {
        g_scriptVars[(g_scriptOps[1] & 0x7fff) + index] = SCRIPT_RESULT;
    }
    Script_Advance(8);
    return 1;
}

/* Opcode 0x23: var += value (0x800840A4) */
/* 0x800840A4 */
u32 Script_OpAdd(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) + SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x24: var -= value (0x80084148) */
/* 0x80084148 */
u32 Script_OpSub(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) - SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x25: var *= value (0x800841EC) */
/* 0x800841EC */
u32 Script_OpMul(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) * SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x26: var /= value (0x80084294) */
/* 0x80084294 */
u32 Script_OpDiv(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) / SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x27: var %= value (0x80084360) */
/* 0x80084360 */
u32 Script_OpMod(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) % SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x28: var &= value (0x8008442C) */
/* 0x8008442C */
u32 Script_OpAnd(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) & SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x29: var |= value (0x800844D0) */
/* 0x800844D0 */
u32 Script_OpOr(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) | SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x2A: var ^= value (0x80084574) */
/* 0x80084574 */
u32 Script_OpXor(void)
{
    SCRIPT_READ_OPS(2);
    SCRIPT_RESULT = SCRIPT_ARG(1) ^ SCRIPT_ARG(2);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x2B: var = -var (0x80084618) */
/* 0x80084618 */
u32 Script_OpNeg(void)
{
    SCRIPT_READ_OPS(1);
    SCRIPT_RESULT = -SCRIPT_ARG(1);
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x2C: var = random % value (a 17-bit random number from two rand() calls)
 * (0x800846A4) */
/* 0x800846A4 */
u32 Script_OpRandom(void)
{
    short range;
    int high;
    int low;

    SCRIPT_READ_OPS(2);
    range = SCRIPT_ARG(2);
    high = rand();
    low = rand();
    SCRIPT_RESULT = (high * 4 + (low & 3)) % range;
    SCRIPT_VAR_SET(g_scriptOps[1],SCRIPT_RESULT);
    Script_Advance(6);
    return 1;
}

/* Opcode 0x30: play a song (0x8008477C) */
/* 0x8008477C */
u32 Script_OpPlayBgm(void)
{
    Bgm_Play(SCRIPT_ARG0,0,0x7f,0x7f);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x31: stop the music (0x800847D4) */
/* 0x800847D4 */
u32 Script_OpStopBgm(void)
{
    Bgm_Stop();
    Script_Advance(2);
    return 1;
}

/* Opcode 0x32: play sound effect n (+0x17) (0x800847FC) */
/* 0x800847FC */
u32 Script_OpPlaySe(void)
{
    Se_Play((SCRIPT_ARG0 + 0x17),0x7f,0x7f,1);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x33: no effect (0x80084858) */
/* 0x80084858 */
u32 Script_OpNop33(void)
{
    Script_Advance(2);
    return 1;
}

/* Opcode 0x34: reads one operand, no effect (0x80084878) */
/* 0x80084878 */
u32 Script_OpNop34(void)
{
    Script_Value(SCRIPT_WORD(2));
    Script_Advance(4);
    return 1;
}

/* Opcode 0x35: no effect (0x800848B8) */
/* 0x800848B8 */
u32 Script_OpNop35(void)
{
    Script_Advance(2);
    return 1;
}

/* Opcode 0x38: continue in the next frame (0x800848D8) */
/* 0x800848D8 */
u32 Script_OpYield(void)
{
    Script_Advance(2);
    return 0;
}

/* Opcode 0x39: var = 1 while an XA voice stream plays (0x800848F8) */
/* 0x800848F8 */
u32 Script_OpIsXaPlaying(void)
{
    short var = SCRIPT_WORD(2);

    SCRIPT_RESULT = (Cd_IsXaPlaying() != 0);
    SCRIPT_VAR_SET(var,SCRIPT_RESULT);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x36: play XA voice (file, channel) (0x8008496C) */
/* 0x8008496C */
u32 Script_OpPlayXa(void)
{
    SCRIPT_READ_OPS(2);
    Xa_Play(1,SCRIPT_ARG(1) * 16 + SCRIPT_ARG(2));
    Script_Advance(6);
    return 1;
}

/* Opcode 0x37: stop the XA voice (0x800849E4) */
/* 0x800849E4 */
u32 Script_OpStopXa(void)
{
    Xa_RequestStop();
    Script_Advance(2);
    return 1;
}

/* Opcode 0x40: fade in over n frames (0x80084A0C) */
/* 0x80084A0C */
u32 Script_OpFadeIn(void)
{
    Fade_In((u16)SCRIPT_ARG0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x41: fade out over n frames (0x80084A54) */
/* 0x80084A54 */
u32 Script_OpFadeOut(void)
{
    Fade_Out((u16)SCRIPT_ARG0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x42: fade from white (0x80084A9C) */
/* 0x80084A9C */
u32 Script_OpFadeFromWhite(void)
{
    Fade_FromWhite((u16)SCRIPT_ARG0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x43: fade to white (0x80084AE4) */
/* 0x80084AE4 */
u32 Script_OpFadeToWhite(void)
{
    Fade_ToWhite((u16)SCRIPT_ARG0);
    Script_Advance(4);
    return 1;
}

/* Opcode 0x45: wait for the fade (0x80084B2C) */
/* 0x80084B2C */
u32 Script_OpWaitFade(void)
{
    if (Fade_IsDone() != 0)
    {
        Script_Advance(2);
    }
    return 0;
}

/* Push a call frame: the return address and script variables 0xF0-0xFF (0x80085BC4) */
/* 0x80085BC4 */
void Script_PushFrame(u8 *ret)
{
    ScriptFrame *frame = &g_scriptStack[g_scriptDepth];
    int i;

    frame->ret = ret;
    for (i = 0; i < 16; i++)
    {
        frame->vars[i] = g_scriptVars[0xf0 + i];
    }
    g_scriptDepth++;
}

/* Pop a call frame, restoring variables 0xF0-0xFF; returns the return address (0x80085C38) */
/* 0x80085C38 */
u8 * Script_PopFrame(void)
{
    ScriptFrame *frame;
    int i;

    g_scriptDepth--;
    frame = &g_scriptStack[g_scriptDepth];
    for (i = 0; i < 16; i++)
    {
        g_scriptVars[0xf0 + i] = frame->vars[i];
    }
    return frame->ret;
}
