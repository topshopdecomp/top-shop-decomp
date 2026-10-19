/*
 * sys.c - boot initialization and the end of every frame
 *
 * Sys_Init sets the system up at boot; the VSync callback; Sys_EndFrame finishes every frame
 * (drawing, swapping the buffers, 30 / 60 frames a second); and loading the fonts, the loading
 * screen and the common sprites at start-up.
 */
#include "common.h"
#include "topshop.h"

/* VSync callback (installed by Sys_Init). Counts vblanks, pumps Cd_Update, decodes the two InitTAP
 * buffers (digital pads 0x41 / multitap 0x80) into g_pads[].connected/raw, then calls rand().
 * (0x800196E8) */
/* 0x800196E8 */
void Sys_VSyncCallback(void)
{
    g_vsyncCount++;
    Cd_Update();
    Pad_DecodePort(&g_pads[0],g_padBuf0);
    Pad_DecodePort(&g_pads[4],g_padBuf1);
    rand();
}

/* Boot init: GPU reset/clear, Pad_Init, SsInit, CdInit, Cd_InitQueue, SecHeap_Init,
 * GsInitGraph(320x240), OTs, sound setup, the blank CLUT row (1, 0x1FE: 255 entries of 0x8000),
 * debug font, VSyncCallback(Sys_VSyncCallback), srand(123). (0x80019AF4) */
/* 0x80019AF4 */
void Sys_Init(void)
{
    RECT rect;
    short block;
    int i;

    ResetCallback();
    VSync(0);
    SetDispMask(0);
    ResetGraph(0);
    rect.w = 0x280;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0x1ff;
    ClearImage(&rect,0,0,0);
    DrawSync(0);
    Pad_Init();
    SsInit();
    VSync(0);
    SetDispMask(1);
    CdInit();
    Cd_InitQueue();
    g_fileIoDisabled = 0;
    SecHeap_Init();
    g_skipFrameClear = 0;
    g_skipFrameClearLast = 0;
    g_fadeUnused = 0;
    g_renderEnabled = 0;
    g_renderEnabledLast = 0;
    g_debugPrint = 0;
    GsInitGraph(0x140,0xf0,4,1,0);
    GsDefDispBuff(0,0,0,0xf0);
    g_bufferOrigin[0] = 0;
    g_bufferOrigin[3] = 0xf0;
    g_bufferOrigin[1] = 0;
    g_bufferOrigin[2] = 0;
    g_ot[0].length = 5;
    g_ot[0].org = g_otTags0;
    g_clearQueued = 0;
    g_ot[1].length = 5;
    g_ot[1].org = g_otTags1;
    g_otIndex = g_otIndexInit;
    GsClearOt(0,0,g_ot + (char)g_otIndexInit);
    GsSetWorkBase(g_packetArea[g_otIndex]);
    g_tintR = 0x80;
    g_tintB = 0x80;
    g_tintG = 0x80;
    g_tintSlotR[0] = 0x80;
    g_tintSlotB[0] = 0x80;
    g_tintSlotG[0] = 0x80;
    g_tintSlotR[1] = 0x80;
    g_tintSlotB[1] = 0x80;
    g_tintSlotG[1] = 0x80;
    g_vsyncInterval = 0;
    Task_InitAll();
    g_modeTaskId = Task_Create(1,NULL);
    Snd_Init(0xdd,FILE_SEQ_J);
    g_rect[2] = 0x400;
    g_rect[0] = 0;
    g_rect[1] = 0;
    g_rect[3] = 0x200;
    ClearImage((RECT *)g_rect,0,0,0);
    DrawSync(0);
    block = SecHeap_Alloc(1);
    for (i = 0; i < 0xff; i++)
    {
        ((u16 *)g_secHeapBlocks[block].addr)[i] = 0x8000;
    }
    g_rect[1] = 0x1fe;
    g_rect[0] = 1;
    g_rect[3] = 1;
    g_rect[2] = 0xff;
    LoadImage((RECT *)g_rect,g_secHeapBlocks[block].addr);
    DrawSync(0);
    SecHeap_Free(block);
    FntLoad(0x3c0,0x100);
    SetDumpFnt(FntOpen(0x10,0x10,0x100,200,0,0x200));
    g_vsyncCount = 0;
    g_frameVsync = 0;
    VSyncCallback(Sys_VSyncCallback);
    srand(123);
}

/* Per-frame end: DrawSync, VSync(g_vsyncInterval), swap display buffer when a frame is ready,
 * draw/clear the double-buffered OT, then update g_pads held/pressed/repeat/changed from each
 * pad's 16-frame history (g_padHistory[pad * 16 + frame]). (0x80019DEC) */
/* 0x80019DEC */
void Sys_EndFrame(void)
{
    PadState *pad;
    u16 *history;
    u32 pos;
    int otIndex;
    short padNo;
    short i;

    DrawSync(0);
    VSync(g_vsyncInterval);
    if (g_renderEnabledLast == 1)
    {
        GsSwapDispBuff();
        if (g_skipFrameClearLast == 0)
        {
            GsClearOt(0,0,g_ot + g_otIndex);
            /* frame start packet: the base DRAWENV with clip/offset y of this buffer */
            g_drawEnv = g_drawEnvBase;
            otIndex = g_otIndex;
            g_drawEnv.clip.y = g_bufferOrigin[otIndex * 2 + 1];
            g_drawEnv.ofs[1] = g_bufferOrigin[otIndex * 2 + 1];
            g_drawEnv.isbg = 1;
            g_drawEnv.r0 = 0;
            g_drawEnv.g0 = 0;
            g_drawEnv.b0 = 0;
            SetDrawEnv(&g_drEnvPackets[otIndex],&g_drawEnv);
            AddPrim(g_ot[g_otIndex].org,&g_drEnvPackets[g_otIndex]);
        }
        g_renderEnabledLast = 0;
    }
    g_skipFrameClearLast = g_skipFrameClear;
    if (g_renderEnabled == 1)
    {
        GsDrawOt(g_ot + g_otIndex);
        g_otIndex = (g_otIndex + 1) & 1;
        GsSetWorkBase(g_packetArea[g_otIndex]);
        GsClearOt(0,0,g_ot + g_otIndex);
        g_clearQueued = 0;
        g_renderEnabledLast = 1;    /* swapped at the next frame end */
        if (g_debugPrint == 1 && (g_playerPads[0].held & (PADL1 | PADL2)) != (PADL1 | PADL2))
        {
            g_debugPrint = 0;
        }
    }
    /* the pads: held, and pressed / changed since the last frame; repeat: pressed, or held for
     * the last 9 frames (the bit is then taken out of the frame 4 back, so that it fires again
     * 5 frames later) */
    pos = (g_padHistoryPos + 1) & 0xf;
    for (padNo = 0; padNo < 8; padNo++)
    {
        pad = &g_pads[padNo];
        history = &g_padHistory[padNo * 16];
        pad->held = pad->raw;
        pad->changed = history[g_padHistoryPos] ^ pad->held;
        pad->pressed = pad->changed & pad->held;
        history[pos] = pad->held;
        pad->repeat = history[pos];
        for (i = 1; i <= 8; i++)
        {
            pad->repeat &= history[(pos - i) & 0xf];
        }
        history[(pos - 4) & 0xf] ^= pad->repeat;
        pad->raw = 0;
        pad->repeat |= pad->pressed;
    }
    g_padHistoryPos = pos;
}

/* Read LOAD_256.SPC (still LZSS-compressed) to 0x80150000 and keep it there: the loading screen
 * Game_LoadCommonTask unpacks and shows (0x80036A98) */
/* 0x80036A98 */
void Sys_LoadLoadingScreen(void)
{
    File_ReadSync(FILE_LOAD_256_SPC,&g_loadScreenBuf,FILE_WHOLE);
}

/* Load FONT0.FNT (g_fontFull) and FONT0H.FNT (g_fontHalf), allocate a 32-sector work block for
 * the text (g_fontWorkBlock), set up the text windows and the cached glyphs, and the seven sprite
 * groups (0x80036BC0) */
/* 0x80036BC0 */
void Sys_LoadFonts(void)
{
    File_ReadSync(FILE_FONT0_FNT,&g_fontFull,FILE_WHOLE);
    File_ReadSync(FILE_FONT0H_FNT,&g_fontHalf,FILE_WHOLE);
    g_fontWorkBlock = SecHeap_Alloc(0x20);
    Font_Init(&g_fontFull,&g_fontHalf,g_secHeapBlocks[g_fontWorkBlock].addr);
    TextWin_InitAll();
    Font_CacheDigits();
    Font_CacheKana();
    g_spriteGroupTable[0].size = 0x113;
    g_spriteGroupTable[1].size = 0x46;
    g_spriteGroupTable[2].size = 10;
    g_spriteGroupTable[3].size = 100;
    g_spriteGroupTable[4].size = 0xf0;
    g_spriteGroupTable[5].size = 0xbe;
    g_spriteGroupTable[6].size = 0x78;
    Sprite_InitGroups(g_spriteGroupTable,7);
    g_frontOtOn = 0;
    g_frontOtUsed = 0;
    g_boardOtOn = 0;
    g_boardOtUsed = 0;
}

/* Reset sprite group 0, load ETC2.SPC, LZSS-decompress into a temp block (sized from the compressed
 * size word), Sprite_LoadSet into group 0, free. Called when the logo sequence ends. (0x80036CB8) */
/* 0x80036CB8 */
void Sys_LoadCommonSprites(void)
{

    g_spriteGroups->next = g_spriteGroups->base;
    File_LoadPackedSprites(FILE_ETC2_SPC,0);
}

/* Re-run the text setup from Sys_LoadFonts without reloading files. Called by main each mode
 * change. (0x80036EC0) */
/* 0x80036EC0 */
void Sys_ResetFonts(void)
{
    Font_Init(&g_fontFull,&g_fontHalf,g_secHeapBlocks[g_fontWorkBlock].addr);
    TextWin_InitAll();
    Font_CacheDigits();
    Font_CacheKana();
}

/* One frame every 2 vblanks (0x8004A078) */
/* 0x8004A078 */
void Sys_Set30Fps(void)
{
    g_vsyncInterval = 2;
}
