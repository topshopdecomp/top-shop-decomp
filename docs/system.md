# System: boot, main loop, frame, VSync, controllers

The code is in `src/main.c` (`main` and the game-mode loop), `src/sys.c` (start-up, the VSync
callback, the end of every frame) and `src/pad.c` (the controllers). Types: `PadState` in
`include/pad.h`. Everything here is confirmed from the code.

## Boot

`main` (`0x8001A170`) calls `__main`, `SetMem(2)` and `ResetCallback`, then:

1. **`Sys_Init`** (`0x80019AF4`):
   - resets the GPU and clears VRAM;
   - `Pad_Init`, `SsInit`, `CdInit`, `Cd_InitQueue`, `SecHeap_Init`;
   - `GsInitGraph(320, 240, …)` with a double-buffered ordering table: `g_ot` (`0x800CBCF0`,
     2 × `GsOT`) with 32 tags each (`g_otTags0` / `g_otTags1`, `0x800B3A88` / `0x800B3B08`), and
     the packet areas `g_packetArea` (`0x800B3CF0`, 2 × 0xC000 bytes);
   - the sound set-up (`Snd_Init(0xDD, FILE_SEQ_J)`, `FILE_SEQ_J` being 0xA9);
   - uploads a 255-entry CLUT row, all `0x8000`, at VRAM y = 0x1FE;
   - the debug font (`FntLoad`, `FntOpen`, `SetDumpFnt`);
   - `VSyncCallback(Sys_VSyncCallback)` and `srand(123)`.
2. **`Game_InitDefaults`** (`0x80036ACC`): the options, a one-player game with 2000 to start and
   10000 to win, empty player slots, and `BoardRules_Init`.
3. **`Sys_LoadFonts`** (`0x80036BC0`): loads `FONT0.FNT` to `0x80130000` (`g_fontFull`) and
   `FONT0H.FNT` to `0x8012F000` (`g_fontHalf`), and sets up the glyph cache (`Font_Init`).

Then `main` turns rendering on and starts the game at mode 0, the boot logos.

## The main loop

For each game mode, `main`:
- ends a frame, frees every task (`Task_InitAll`) and creates two task slots: `g_modeTaskId` for
  the mode and `g_auxTaskId` (only `Board_Task` uses it: it runs `Game_LoadCommonTask`, the loading
  screen, on it while it sets the board up);
- turns file reading back on (`g_fileIoDisabled = 0`) and resets the fonts (`Sys_ResetFonts`);
- starts the mode's task, chosen by `g_gameMode` (`0x800B23F8`, `MODE_*` in `include/main.h`).

Then, every frame until that task is done: `Pad_UpdatePlayers`, `Task_RunAll`, `Sys_EndFrame`,
and `g_frameVsync = g_vsyncCount`.

| Mode | `MODE_*` | Task | What it is |
|---|---|---|---|
| 0 | `MODE_LOGO` | `Logo_Task` (`0x80041464`) | the boot logos; the mode the game starts in |
| 2 | `MODE_TITLE` | `Title_Task` (`0x80040EB0`) | the title screen, the intro and the attract demo |
| 3 | `MODE_TITLE_INTRO` | — | a marker while `Title_Task` plays its intro; `main` starts no task for it |
| 5 | `MODE_NEW_GAME` | `NewGame_Task` (`0x80036F1C`) | the main menu and the set-up screens |
| 6 | `MODE_BOARD` | `Board_Task` (`0x800432F0`) | the board game |
| 7 | `MODE_ENDING` | `Ending_Task` (`0x80076068`) | the ending and the final results |
| 8 | `MODE_STAGE_CLEAR` | `StageClear_Task` (`0x8007496C`) | between story stages |
| 9 | `MODE_STAFF_ROLL` | `StaffRoll_Task` (`0x8007618C`) | the staff roll |
| 10 | `MODE_UNUSED10` | `Mode10_Stub` (`0x8001C950`) | an empty stub |
| 0xB | `MODE_CONTINUE` | — | keeps the frame loop running after the mode task ends |

See [modes.md](modes.md) for how the modes lead to each other.

## The end of a frame: `Sys_EndFrame` (`0x80019DEC`)

1. `DrawSync(0)`, then `VSync(g_vsyncInterval)`. The game runs at 60 frames a second
   (`g_vsyncInterval` 0, set by `Mode_CreateSubTasks`, `0x80042A3C`, in every mode's first step),
   except on the board, at 30 (2, set by `Board_CreateTasks`, `0x80047DC0`). `Sys_Set30Fps`
   (`0x8004A078`) would set 2 too; nothing calls it.
2. If the last frame was drawn (`g_renderEnabledLast`): `GsSwapDispBuff`, and, unless
   `g_skipFrameClearLast` is set, a fresh ordering table that starts with a draw-environment packet
   clearing the screen to black.
3. If rendering is on (`g_renderEnabled`): `GsDrawOt` on this frame's ordering table, switch to the
   other buffer (`g_otIndex`), `GsSetWorkBase`, and clear the new ordering table. (With L1 and L2
   not both held, the debug print `g_debugPrint` is switched off here.)
4. The controllers' per-frame update for all 8 pads (below).

## The VSync callback: `Sys_VSyncCallback` (`0x800196E8`)

In `src/sys.c`, with the pad decoding in `src/pad.c`:

- `g_vsyncCount++`, then `Cd_Update()` (the CD request queue).
- Decodes both `InitTAP` receive buffers (`g_padBuf0` `0x800B3B88`, `g_padBuf1` `0x800B3BAA`, 34
  bytes each). Status 0 with type `0x41` (a digital pad) goes to slot A; type `0x80` (a multitap)
  gives 4 pads. It sets `g_pads[i].connected` and ORs the button bits, inverted, into
  `g_pads[i].raw`.
- Ends with a `rand()` call, which stirs the random generator every vertical blank.

## Controllers

- **`g_pads[8]`** (`0x800B3A28`, `PadState`, 12 bytes each): port 1's taps A–D, then port 2's.
- **Every frame** (in `Sys_EndFrame`): `held = raw`, `changed = previous ^ held`,
  `pressed = changed & held`; then `raw = 0`.
- **Auto-repeat**: `repeat` is `pressed`, or a button held for the last 9 frames. The history is a
  16-frame ring, `g_padHistory` (`0x800B3BD0`, `[pad * 16 + frame]`). When a repeat fires, the bit is
  taken out of the entry 4 frames back, so it fires again 5 frames later.
- **The decoding** is `Pad_DecodePort` / `Pad_Decode` in the C (`src/pad.c`): helpers of the C
  version, inline code in the original. The button bits use the PsyQ names (`PADRdown` cross,
  `PADRright` circle, `PADLup`…; see `include/psyq.h`).
- **`Pad_UpdatePlayers`** (`0x8001AC4C`): `g_playerPads[i] = g_pads[g_playerPadMap[i]]` for the 6
  players (`g_playerPads` at `0x800B39D0`, the map at `0x800B2400`), and `g_playerPads[6] =
  g_pads[0]`.
- **`Pad_Init`** (`0x8001A9F0`) also initializes the memory card (`InitCARD`, `StartCARD`,
  `_bu_init`).

## `$gp`

`$gp = 0x800B1E74`, derived from `0x65C(gp)` = `0x800B24D0` (`g_taskInScheduler`). The small
globals from `0x800B23B8` to `0x800B2560` are reached gp-relative.

## Answered questions

- `0x800B23FD`, `0x800B23FE` and `0x800B23FC` are `g_skipFrameClear`, `g_skipFrameClearLast` and
  `g_renderEnabledLast`; `0x800FE0B0` is `g_gameConfig` (start and goal money, etc.).
- `g_auxTaskId`: `Board_Task` runs the loading screen (`Game_LoadCommonTask`) on it while it sets
  the board up (`STEP_SETUP`); nothing else uses it.
