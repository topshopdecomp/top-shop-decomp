# Event scripts and the scene

The game's scenes (the events, the story mode's scenes, the calendar pages, the characters' talk)
are bytecode run by an interpreter. The code:
- `src/script.c`: the interpreter (loading a script, the task that runs it, the dispatcher
  `Script_Run`, flow control, variables, arithmetic, the sound and fade opcodes);
- `src/sceneops.c`: the opcodes that drive the scene;
- `src/gameops.c`: the opcodes that act on the board game;
- `src/scene.c`: the scene's state and its drawing, the talk panel, the portrait.

Declarations: `include/script.h` (the interpreter, its opcodes), `include/sceneops.h`,
`include/gameops.h` and `include/scene.h` (the scene).

## Where scripts come from

The board loads a script file for each occasion:
- an event, a story scene or a calendar page: the `.J` file is loaded by `LoadJWithTim_Task`
  ([fileio.md](fileio.md)) into the sector heap block `g_scriptBlock`. The archive's header gives the
  offsets: +4 → entry 0, the script's code (`g_scriptData`); +8 → entry 1, the image table
  (`g_sceneImages`, also the frame table of `g_sceneSheet`). See `LoadJWithTim_Task` in
  `src/script.c` and `Board_PhaseTurn` / `Board_PhaseEndTurn` in `src/board.c`;
- a character's talk at the start or the end of a turn: entry `charId` (or `16 + charId`) of `TALK.J`,
  LZSS-unpacked into a new block. Its entry point is chosen by `TurnStart_PickScript` (`0x8008BCA8`) /
  `TurnEnd_PickScript` (`0x8008C95C`) into `g_scriptEntry` (variable 0xD2).

Then `EventScript_Task` (`0x80083B34`) runs it on `g_scriptTaskId`.

## The interpreter

- **Code** is big-endian 16-bit words: an opcode, then its operands. `Script_Run` (`0x8007C124`)
  executes opcodes until one returns 0 (wait for the next frame). An unknown opcode shows "コード：n"
  with its offset, once (`g_scriptErrorShown`).
- **Operands** below −0x7F00 name a variable: `g_scriptVars[op & 0x7FFF]` (256 shorts at
  `0x801155D8`); `Script_Value` (`0x80083CB0`) decodes them. The operands are copied to `g_scriptOps`
  (`0x80117B10`) by `Mem_CopySwap16`.
- **Results** go to variable 0xC0, and to the destination operand when it names a variable.
- **Calls** (0x13 / 0x14) push the return address and the variables 0xF0–0xFF on `g_scriptStack`
  (16 frames; `Script_PushFrame` / `Script_PopFrame`).
- **Multi-frame opcodes**: `g_scriptWait` counts frames or sub-steps inside one; the opcodes that run
  dialogs keep their state in a scratch area of the task's work area (`Script_Run(scratch)`).
- **Variables refreshed every frame** by `Scene_Update` (`0x8007CA48`):

  | Variable | Holds |
  |---|---|
  | 0xC2… / 0xC9… | each player's pad, held / pressed |
  | 0xCF | the number of players |
  | 0xD0 | the current player |
  | 0xD1 | a frame counter |
  | 0xD2 | the entry |
  | 0xD3 / 0xD4… | the CPU flags |
  | 0xDA | the floors |
  | 0xDB | the months elapsed |
  | 0xDC / 0xDD | the start and goal money |
  | 0xDE / 0xDF | pad 1, held / pressed (bits remapped) |
  | 0xE0 / 0xE1 | all the human players' pads |
  | 0xE2 | the character |
  | 0xE3 | the players still in |

## Opcodes

They are named `OP_<mnemonic>` in the C (`enum ScriptOp`, `include/script.h`), the tests `TEST_*`.
There are 121: the original `Script_Run` (`0x8007C124`) checks `opcode < 0x195` and jumps through a
table at `0x800186D0`; every entry not listed here goes to the "unknown code" message.

| Opcodes | Where | What |
|---|---|---|
| 0x00 | `script.c` | end |
| 0x01 | | wait |
| 0x02, 0x03 | | wait for buttons |
| 0x10, 0x12, 0x13, 0x14, 0x15 | | goto, switch, call, return, halt |
| 0x20 | | if (the tests: 0 ==, 1 !=, 2 >, 3 <, 4 >=, 5 <=, 6 all bits, 7 no common bits) |
| 0x21, 0x23–0x2C | | set; arithmetic, bit operations, random |
| 0x2D, 0x2E | | read a table, set by index |
| 0x30–0x32 | | music and sound effects |
| 0x36, 0x37, 0x39 | | the XA voice |
| 0x38 | | yield |
| 0x40–0x45 | | fades |
| 0x60 | `sceneops.c` / `script.c` | a marker (a name in the script, skipped) |
| 0x61; 0x62, 0x63, 0x64 | | the view rectangle; open, close, wait |
| 0x70–0x76 | | the layers |
| 0x80–0x96 | | the sprites |
| 0x97 | | dim |
| 0xA0–0xA2 | | the rectangles |
| 0xB0 | | step toward an angle |
| 0xC0–0xCF | | the talk panel: open, close, waits, the cursor row, the speaker, clear, print inline or data text, a player's name, a store type's name |
| 0x100–0x102 | | the portrait |
| 0x140, 0x141 | `gameops.c` | store counts |
| 0x180–0x194 | | the game's effects: money for player selectors, the stores' goods, prices and closings, the buy-a-lot and store dialogs, no elevator, turning back |

**Selectors.** Players: n, −1 everybody playing, −2 the current player, −3 everybody else. Stores:
−4−p the stores of player p, −3 / −2 / −1 as for players, anything else a store's index.

## The scene (`include/scene.h`)

- **`g_sceneView`** (`0x80115A18`): the rectangle everything is drawn in (20.12 fixed point), with an
  outline (`g_line`), opened and closed over n frames in three styles.
- **`g_sceneLayers[4]`** (`0x80115A48`, 0x428 bytes each): tiled background images with a scroll
  speed and per-band x offsets (`lineX`): still, moving, or a sine wave.
- **`g_sceneSprites[64]`** (`0x80116AE8`, 0x30 bytes each): an image, attributes, a 20.12 position,
  scale and angle. A sprite can move to a target, play an animation (frames of 4 words in the script's
  data: image, frames, flip bits, next) or follow a path (steps of 5 words). They are drawn by
  `Scene_DrawImage`: bits 12–14 of the image number select the scene's tile sheet, the sprite groups
  or the portrait sheet.
- **`g_talk`** (`0x80117788`): the talk panel at the bottom: 3 rows of 38 Shift-JIS characters, cached
  as glyphs (`g_talkGlyphs`), a frame per speaker (6 = the host), a highlighted row. `Talk_PutChar`
  reads a character **byte by byte** (the text is often at odd addresses, e.g. the names inside
  `Player`); it wraps a row after 38 characters, and wrapping off the third row writes past the
  panel's characters, into their kinds (`Talk_PutChar`, `0x80081450`). The panel is 0x120 wide at x
  0x10 for the host, 0x100 wide at x 0x20 for a player (`Script_OpTalkOpen`, `0x800853E4`), at y 0xA8
  when shown; the text starts 8 pixels in, a half-width character is drawn 7 pixels wide and a
  full-width one 14, the rows 16 pixels apart. So a player's panel holds about 240 pixels a row.
- **`g_portrait`** (`0x801178F8`): the speaking character's face, from `KAO?P.J` (`Portrait_Load`),
  squashed in and out (`g_portraitSquash*`), with a mouth animation.
- The ordering tables `g_sceneOt[2]` (`0x80117B20`), with their tags at `g_sceneOtTags`.
- **`g_sceneRects[16]`** (`0x801176E8`): set by 0xA0–0xA2 but never read, so they have no effect.

## Names in the C

- `include/script.h`: `SCRIPT_WORD(at)`, `SCRIPT_ARG(n)`, `SCRIPT_READ_OPS(n)` and `SCRIPT_VAR_SET`
  read the big-endian operands (`MAKE_U16`); `SCRIPT_IS_VAR(word)` tells a variable operand;
  `SCRIPT_RESULT` is variable 0xC0; the per-frame variables are `SVAR_*`. `ScriptScratch` is the union
  of what each multi-frame opcode keeps between frames (`EventScriptWork.scratch`).
- `include/gameops.h`: the player selectors `SCRIPT_ALL_PLAYERS` (−1), `SCRIPT_CUR_PLAYER` (−2),
  `SCRIPT_OTHER_PLAYERS` (−3).
- `Script_OpSellStore` (`0x80082D50`, opcode 0x185): a sell-a-store dialog (`SellStore_Task`), for a
  % of the store's value. `Script_OpSetShoes` (opcode 0x193): the "Chrip-Chrip" shoes,
  `Player.shoes`, the squeaky footsteps of `Move_Task`.
- `Script_Reset` (`0x8007C034`) clears the 256 variables, the 4 layers, the 64 sprites and the 16
  rectangles, and resets the talk panel to the host's frame (6).
- The store dialogs a script opens (0x184 `Script_OpBuyLot` and 0x185 `Script_OpSellStore`, both
  through `Script_StoreDialog`) play for the current player; under `RULE_AUTO_SCRIPT` the game answers
  them.

## The scripts on the disc, decompiled

`scripts/` holds all 165 scripts as text:

| Files | From |
|---|---|
| `events/eve_000.tss`–`eve_099.tss` | entry 0 of `EVE_NNN.J` |
| `stages/cnt00.tss`–`cnt19.tss` | `CNTNN.J` |
| `calendar/cal_01.tss`–`cal_13.tss` | `CAL_NN.J` |
| `talk/start_NN.tss`, `talk/end_NN.tss` | entries NN and 16 + NN of `TALK.J` (LZSS) |

`scripts/README.md` describes the language: how a script runs (which statements end a frame, the
start, the end, an event's result, calls), how statements are written and stored, the variables and
their values, every opcode, the constants, the data, the texts, and the event card scene's shared
routines.
The tool is `tools/tss.py` (`extract`, `build`, `asm`, `dis`), with `tools/tss_names.py` (the names of
the routines and variables, the opcode descriptions, the calendar pictures) and `tools/tss_english.json`
(the English for the 1998 Japanese texts).

### Building edited scripts

The scripts can be edited and built back into the disc's files: `make scripts` (`tools/tss.py build
[DIR] [-o OUT]`) assembles every file and writes `EVE_*.J`, `CNT*.J`, `CAL_*.J` and `TALK.J` into
`scripts/build/`.
- A file with an edited script gets the new bytecode (a talk script is LZSS-packed again by
  `lzss_compress`, which `Lzss_Decompress` unpacks); its other entries come from `disc/A`, padded to 4
  bytes as on the disc.
- The executable reads each file at a fixed place for a fixed number of sectors (`g_fileLocTable`,
  `g_fileSectorTable`), so a file may not grow past its last sector (an error), and a file that got
  shorter is padded to its size on the disc.
- Every address in the scripts is a label, so code and data can move. Rebuilt files play in the
  emulator: a moved picture on a calendar page, new talk texts.

### How the disassembler finds things

1. It follows the code from offset 0: goto, if, switch and call targets (no script jumps through a
   variable).
2. Then the data the code names: animations, paths, tables, texts.
3. Then code that nothing reaches, from the start of each gap that follows code (shown as
   `unused_XXXX`).
4. Then paths, animations and text tables that nothing names, by their shape.
5. The rest is `words` / `bytes`.

The routines the scripts share are named by their code (`tss_names.ROUTINES`, from their copies in
`EVE_002.J`) or by small patterns.

## Facts found on the way

All checked in the code or the emulator:

- **Opcode 0x194** (`stores_goods_pricier`) goes to `Script_OpStoresGoodsLevel` (`0x800824E8`), as in
  the original's table. Event 49 ("Due to an error in ordering, all items in X's store have changed to
  cheaper items", drawn at random) uses it: the goods levels go 3, 4, 5 → 4, 5, 5 (the same in the
  original and the rebuilt game, played in an emulator).
- **Measuring the talk's text** (`Script_OpTalkPrint`, `0x80080C90`, through `Talk_Print`, for inline
  text; `Script_OpTalkPrintData` for data text): a character is two bytes when its lead byte's high
  nibble is 8, 9, E or F; inline text also takes the full-width space (81 40) as two bytes, data text
  as one (then `@`). The `%N` / `%S` codes are 4 bytes, `%K` 2, another `%` 1. Opcode 0x60 measures its
  text in a 14-byte copy.
- **Event 73** (a quiz) writes `talk ""` without its NUL: the game goes on at 0x00CE, inside the next
  instruction (the word 0x2C80, no opcode), and stops with the "unknown code" message, in the original
  too (`boardevents`; `g_scriptErrorShown` = 1 in both). Its answer menu, and event 77's, wait for the
  script button bit 0x0200, which no button sets.
- **Events 95–99** hold a test version of the card-scene framework (the effects typed in as numbers,
  confirmed with the same unobtainable bit). Events 2–23, 38, 39 and 95–99 announce price changes made
  with opcodes 0x182 / 0x183, which do nothing.
- **What is played.** Only events 24–37, 40–54, 58–72, 75 and 76 (at random, `g_eventWeights`) and
  80–88 (the months' events, `g_monthEvents`) are played, plus 0 (bankruptcy) and 1 (a story stage
  lost); nothing turns `RULE_PICK_EVENT` on. `CAL_13.J` (a copy of January) is never loaded. The
  events never played are described in [unused-events.md](unused-events.md).
- **The never-played events and the talk scripts are in Japanese, and the talk is never shown
  either.** `Board_Task` plays it on a CPU player's turn only while `g_optSkipCpuTalk` is 0 (or under
  `RULE_ALWAYS_TALK`, which nothing sets), and the game sets it to 1 (`Game_InitDefaults`) with no
  reachable option to change it (`OPTION_SKIP_CPU_TALK` is past the options menu's last row; the save
  keeps it). Seen in the emulator: a whole round of CPU turns without a talk script. With the variable
  set to 0, the talk shows (the portrait and the line), and then the turn stops at step 3202, on the
  original disc too: after `end`, `EventScript_Task` waits for the XA voice to end, and the voice
  streams from no real position on the disc, so it never does ([fileio.md](fileio.md)). The talk
  scripts never call `stop_xa`; events 0 and 1, which play a voice too, stop it themselves. (A guess:
  it was switched off because it was not translated, or because it hung on the voice.)
- **Playing the talk**: `make CPU_TALK=1` ([build.md](build.md)) builds a game that plays the talk on
  every CPU player's turn, whatever `g_optSkipCpuTalk` says, and gives `Xa_Play` the XA files' real
  positions, so the voice ends and the turn goes on. Seen in the emulator: three rounds of four CPU
  players, every turn with its talk at the start and at the end. Its disc image has the voices too:
  the disc stores their files as plain data, which the drive does not play, and the build writes
  them back as XA ([fileio.md](fileio.md)). Seen in the emulator: the sound recorded during each
  talk matches the voice it asks for (Meower's voice 12 at the start of a first-month turn) and no
  other. It also unpacks every talk script into 2 sectors (4096 bytes): the game's buffer, the packed
  size rounded up to whole sectors, is overrun by the disc's talk scripts by up to 416 bytes.
- **A talk script's layout**: row `$entry * 3 + random 3` of a table of (text offset, face, XA
  channel).
- **The script's button bits** (`Scene_Update`, through `Script_Buttons`): right 0x0001, L1 0x0008, R2
  0x0010, select 0x0020, triangle 0x0040, circle and R1 0x0080, cross and square 0x0400, up 0x1000,
  down 0x2000, left 0x4000.
- **The scene's images**: entry 1 of a script file is the `TileFrame` table of `g_sceneSheet` (VRAM
  0x280, 0), entry 2 the LZSS-packed TIM stream (the palettes on lines 489 / 490, the cells from line
  8).
