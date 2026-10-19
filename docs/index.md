# Documentation index

The documentation of the Board Game - Top Shop decompilation: how the game works, as found while
decompiling it, and how the decompilation is built. Start with [build.md](build.md).

## Where things stand

- **All 657 game functions are C** in `src/` (53 files, about 44k lines including the data tables in
  `data.c` and `bss.c`). Only the PsyQ SDK (`asm/sdk.s`) and the executable's header
  (`asm/header.s`) stay assembly, by design.
- **Everything has a descriptive name**: every function, global, struct field and local. The code was
  rewritten by hand where it still read like decompiler output: over 785 function definitions (32.2k
  lines) there are no gotos, generic local names, raw byte-offset accesses, sign-extension or compiler
  idioms, decompiler-shaped loops, comma expressions in conditions or register leftovers. Task step
  numbers are decimal.
- **Named constants** for the controller bits, message layouts, palettes, alignments, task messages,
  board rules, window and menu states, task steps and the values of every switch on a code.
- **It builds and plays**: `make` builds a bootable `build/topshop.cue` / `.bin` whose game state
  matches the original's in 25 test scenarios played in an emulator ([build.md](build.md)).
- **The scripts** the game runs are decompiled into `scripts/`, and can be edited and built back into
  the disc's files ([script.md](script.md)).
- **A build option**: `make CPU_TALK=1` plays the CPU players' talk, which the retail game switched
  off, with the characters' voices, restored from the disc ([build.md](build.md),
  [script.md](script.md), [fileio.md](fileio.md)).

## The documents

| Document | What it covers |
|---|---|
| [build.md](build.md) | **start here**: building, the disc image, what the build is made of |
| [overview.md](overview.md) | the executable's header and section layout, `$gp`, the toolchain |
| [system.md](system.md) | boot, the main loop and the game-mode table, the end of a frame, the VSync callback, the controllers (multitap, 8 pads, 6 players) |
| [tasks.md](tasks.md) | the cooperative task scheduler (64 slots, mailboxes) |
| [fileio.md](fileio.md) | file indices, the CD queue and XA streaming, the sector heap, LZSS, image streams |
| [modes.md](modes.md) | the game modes and how they lead to each other (logo, title, new game, board, ending, stage clear, staff roll), fades, `GameConfig`, the stage money table, the characters' names, the options screens |
| [board.md](board.md) | the board game: the step map of `Board_Task`, the board, store and player data, the calendar (one round = one month), the rules, the board's tasks and ordering tables |
| [cpu.md](cpu.md) | the CPU players' decisions, routes, the event cards and the points exchange, store types and stores, the store dialogs |
| [script.md](script.md) | the event script interpreter, its opcodes and the scene it draws (view, layers, sprites, talk panel, portrait); the scripts on the disc decompiled into `scripts/` (`tools/tss.py`, the language in `scripts/README.md`) and built back |
| [unused-events.md](unused-events.md) | the 46 events the game never plays: what each would say and do, why most would do nothing, Pumpkin Head's broken quizzes, other unused script data |
| [text.md](text.md) | fonts, the glyph cache, texts, text windows, message windows |
| [graphics.md](graphics.md) | the frame and ordering tables, `VramImage`, drawing images and sprites, sprite groups, `.SPC` files, fades and tints, tile-map layers, tile sheets |
| [sound.md](sound.md) | music, the sound-effect slots, the XA voices |
| [card.md](card.md) | the memory card file, the save slots, the save and load tasks |
| [disc-formats.md](disc-formats.md) | the disc's files, the `.J` archive format and what each family holds |
| [filetable.txt](filetable.txt) | generated: file index → disc file (930 entries) |

## Source files (`src/`, organized by subsystem; behavior-equivalent, not byte-matching)
Edited by hand and organized by subsystem; the link order is `config/c_files.txt`.

**System**

| File | Covers |
|---|---|
| `main.c` | the entry point and the game-mode loop, the setup common to every mode |
| `sys.c` | boot initialization, the VSync callback, the end of every frame, fonts and the common sprites loaded at start-up |
| `pad.c` | controllers: set-up, decoding what the pads send, the per-player copies, assigning pads to players |
| `task.c` | the cooperative task scheduler (64 slots, messages) |
| `util.c` | small helpers: fixed-point math, angles, sine-wave tables, memory copies, packed tables, random permutations |
| `debug.c` | debug tools left in the game: the VRAM viewer, VRAM dumps to the PC, outlined boxes |

**Files and saves**

| File | Covers |
|---|---|
| `fileio.c` | the CD request queue, file reads, XA streaming, the sector heap |
| `lzss.c` | the LZSS decompressor |
| `card.c` | memory card access through the BIOS: files, events, checking the card |
| `save.c` | game saves on the memory card: the slots, packing and unpacking a game, the story progress, the card tasks of the save and load screens |

**Graphics and sound**

| File | Covers |
|---|---|
| `gfx.c` | drawing images and sprites (plain, aligned, scaled, rotated, tiled, numbers), loading sprite sets and TIM streams, the ordering-table helpers |
| `tilesheet.c` | tile sheets: pictures cut into 8x8 / 16x16 cells kept in VRAM and drawn cell by cell (portraits, scene images) |
| `fade.c` | screen fades, tints and the shape wipes between turns |
| `bgmap.c` | tile-map background layers (the board's two far planes) |
| `sound.c` | music (SEQ), sound effects, the sound set-up |

**Text and windows**

| File | Covers |
|---|---|
| `font.c` | the fonts: expanding glyphs from the font files, the glyph cache in VRAM |
| `text.c` | texts: building a text from a string with its control codes, drawing it; number and string formatting; Shift-JIS voiced marks |
| `textwin.c` | text windows: menus of texts in a frame, opening and closing, the cursor; the window frames themselves |
| `msg.c` | message windows: Msg_Show / Msg_AskYesNo and the tasks behind them |

**Front end**

| File | Covers |
|---|---|
| `game.c` | the default settings and board rules, the start-up loading screen |
| `logo.c` | game mode 0: the boot logos |
| `title.c` | game mode 2: the title screen, its animations, the intro and the attract demo |
| `menu.c` | pieces shared by the setup menus: the sliding entries, the bands, the hand cursor, the characters' animations |
| `newgame.c` | game mode 5: the new game set-up: the main menu, then the screens in turn; options; loading a saved game |
| `setup.c` | the free-play and story set-up screens: player count, the players, characters, names, stage, money rules, turn order |

**The board game**

| File | Covers |
|---|---|
| `board.c` | game mode 6: `Board_Task`, the turn in five phases |
| `boardsetup.c` | setting up the board: its tasks, data, players and stores, season and month graphics, pausing and resuming the display tasks |
| `boardmap.c` | the board's map: cells, squares, the ways on from a square, steps, the routes ahead and the bank on them |
| `boarddraw.c` | drawing the board: squares, elevators, stairs, pieces, the backgrounds of each board, the camera, the board's ordering tables |
| `shopopen.c` | the stores' pictures in VRAM and the shop-open animation after a move |
| `move.c` | moving a piece: the human player's choice of way, walking, stairs, "Stop in this square?" |
| `dice.c` | the dice roll (the rolling pencil) |
| `elevator.c` | riding the elevator |
| `player.c` | the players: money, assets, ranks, the asset history, backups of a move, player selectors |
| `stores.c` | the stores: opening and closing, chains, prices and goods, store types, the lists of stores the dialogs and the CPU players choose from |
| `storemenus.c` | the store dialogs: shopping, the vacant lot, deliveries, retail prices, selling, the store offer, their money windows |
| `bank.c` | the bank: payday |
| `hud.c` | the board's display tasks: the square name, steps left, calendar and goal, the players' panels, the minimap, the points window |
| `turnmenu.c` | the turn's command menu and its screens: map view, status, asset graph |
| `options.c` | the turn options (event scenes, CPU speed, controllers, save) and the stage-mode save prompt |
| `events.c` | event cards: drawing one at random or by choice, showing it, the points exchange at the bank, carrying a card out, choosing its target |
| `effects.c` | the event cards' effects on the stores and the players |
| `talk.c` | the characters' comments at the start and the end of a turn (which talk script) |
| `cpu.c` | the CPU players' decisions: routes, stores, prices, deliveries, the elevator, the points exchange and the targets of event cards |

**Event scripts**

| File | Covers |
|---|---|
| `script.c` | the event script interpreter: loading a script, the task that runs it, the opcode dispatcher, flow control, variables, arithmetic, sound and fade opcodes |
| `sceneops.c` | the script opcodes that drive the scene: view, layers, sprites, rectangles, the talk panel, the portrait |
| `gameops.c` | the script opcodes that act on the board game: money, stores, the store dialogs, moves, results |
| `scene.c` | the scene the scripts draw: view, layers, sprites, the talk panel and the portrait; the script variables refreshed every frame |

**After the board**

| File | Covers |
|---|---|
| `ending.c` | game mode 7: the ending, the final results screen |
| `stageclear.c` | game mode 8: between story stages |
| `staffroll.c` | game mode 9: the staff roll |

**Data**

| File | Covers |
|---|---|
| `data.c` | the game's tables and constant data (read-only data at its original place) |
| `bss.c` | the game's variables |

Headers: one per C file (`include/<name>.h` for `src/<name>.c`, each with its types and
constants, its tasks' work areas, its globals and its functions); `topshop.h` includes them all and
every C file includes it; `common.h`, `psyq.h` (SDK declarations), `fileid.h` (generated).

## Design (not open items)

- Behavior-equivalent, not byte-matching: `make` builds a relinked executable from the C (readable
  C was chosen over a matching decompilation at the start); the PsyQ SDK stays assembly.
- Unused by the game, kept as in the original: `RULE_*` 13, `g_sceneRects`, `StealItem_Task`,
  `Effect_BuyVacant`, the unreachable board steps, game mode 10, the options screen's last two rows,
  the characters' talk scripts (switched off; a `CPU_TALK` build plays them, see
  [script.md](script.md)), and 46 of the 100 events ([unused-events.md](unused-events.md)).
