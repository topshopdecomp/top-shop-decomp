# The board game (game mode 6)

The board game is `Board_Task` (`0x800432F0`): one state machine on `*step`, split by step range
into five phase functions (the [step map](#the-step-map) below). It is in
`src/board.c` (the dispatcher and the five phases); the rest of the board is in the other board files
(`boardsetup.c`, `boardmap.c`, `boarddraw.c`, `shopopen.c`, `move.c`, `dice.c`, `elevator.c`,
`player.c`, `stores.c`, `storemenus.c`, `bank.c`, `hud.c`, `turnmenu.c`, `options.c`, `events.c`,
`effects.c`, `talk.c`, `cpu.c`; [index.md](index.md) says what each covers), each with its header in
`include/`. The callees are named; see also [cpu.md](cpu.md) and [script.md](script.md).

## The board

**A stage is a building of several floors.** `g_stageDefs[stage]` (`0x800ACD14`, `StageDef`): 24–40
stores, 11 or 16 squares wide, 3–5 floors, a cell map and a square-id map.

**Cells.** `Board_GetCell(x, floor)` gives the type in the high byte (`CELL_*` in
`include/boardmap.h`; counted from the ten maps in `src/data.c`):

| Type | What it is |
|---|---|
| `0x000` | a store (the low byte is the store's index) |
| `0x100` | nothing, or out of range |
| `0x200` | the elevator |
| `0x300` | stairs (a connector between floors) |
| `0x400` | the bank ("A1 Bank") |
| `0x500` | on no board (`Board_PhaseSquare` would end the turn on it) |
| `0x600` | a landing beside the middle stairs (boards 1, 4, 5): `Board_StepAlong` walks over it without counting a step; minimap sprite 0xE0 |

**Square flags.** `StageDef.squareFlags` is 1 or 2 on store squares. When a lot with 1 becomes vacant,
the shutter goes up over it as over an opened store (`ShopOpen_Task`); 2 (boards 5 and 8) skips that.

**Squares and chains.** A *square* (`Store.squareId`, 0–10) is a group of neighboring lots. A
player's stores next to each other on one square form a chain (`chainLen`, `chainPos`); a chain can
grow while not all the square's lots are in it (`Stores_ListExtendable`, `0x800798B4`). "Open a store
for free" and the vacant-lot pick only offer lots on squares where the player has no store yet
(`Stores_ListVacantNewSquares`, `0x8007ABA0`).

**Stores and store types.** `g_stores[40]` (`0x80114A40`, `Store`): the owner (−1: vacant), the square,
the store type, the chain, the goods slots with their prices, the timed closings and price changes.
`g_storeTypes[44]` (`0x800ACEF4`): the kinds of store and what they sell. Both layouts are in
[cpu.md](cpu.md).

**Players.** `g_players[6]` (`0x80113CE8`, `Player`, 0xC0 bytes each) holds, among others:
- the position (`x`, `floor`) and direction (`dirX`, `dirFloor`), the steps;
- the event points (+0x3A), the distance to the bank (+0x3F), the money (+0x68), the assets (+0x70);
- the timed effects with their end dates (+0x58 to +0x5E: the dice effect, no elevator, no delivery,
  vandalized);
- `paydays` (+0x54, the bank visits so far) and `stepsSincePayday` (+0x56, the squares walked since
  the last payday; counted and reset in `Move_Start` (`0x8004F7E4`) and `Move_Walk`).

**Routes.** `g_routes` (`0x80114168`): the branches ahead of the player ([cpu.md](cpu.md)).

### Store and player selectors

The `Stores_List*` functions (`stores.c`, `0x800789C8`–`0x8007A800`) all take `who`:
`WHO_CURRENT` (−2) the current player's stores, `WHO_OTHERS` (−3) the other players', `WHO_ANY` (−1)
anyone's, anything else player `who`'s (`Store_OwnedBy`).
- A quirk: `Stores_ListChainHeadsUpTo` (`0x8007946C`) compares `chainLen >= length` for `WHO_ANY`
  but `== length` otherwise.
- `Player_Resolve` (`0x8007AF1C`) takes the same codes. Its `WHO_ANY` was meant to pick a random
  active player, but the random number is not stepped in its loop, so it returns the first active
  player one time in `g_numActivePlayers` and player 0 otherwise (kept).

### Payday

`Player_PaydayAmount` (`0x80076270`, `bank.c`): `(max(assets / 10, 0) + (paydays + 1) * 100)`,
scaled by this board's `StageDef.payScale` against board 0's, times
`(stepsSincePayday * 75 / payScale + 25)` %; ×1.5 when the move stops on the bank; `paydayEffect` 1
makes it 0, 2 doubles it. `Player_PaydayAfter` (`0x8007640C`) gives the same as if the player had
walked n more squares.

### The dice (the pencil)

`Dice_Task` (`0x800545E4`) rolls a hexagonal pencil: a reel of faces, 3 cells of 0x1000 a face. By
`Player.diceEffect`:
- `DICE_NORMAL`: 9 faces (27 cells); faces 0–7 are steps − 1, face 8 is E (an event);
- `DICE_1_2` (`EV_SLOW_DICE`): 1 and 2 only;
- `DICE_E` (`EV_DICE_E`): always E;
- `DICE_SLOW` (`EV_LOWER_PENCIL`): rolls slowly from a fixed start, so that it can be aimed (as with
  `RULE_SLOW_DICE`).

Cross rolls (a CPU player rolls after half a second); circle goes back if that is allowed.

### The elevator

`Elevator_Task` (`0x800552A4`). `Board_DrawStoresTask` opens and closes the doors on the messages
`ELEVATOR_DOORS_OPEN` (0xE) and `ELEVATOR_DOORS_CLOSE` (0xF), and answers `ELEVATOR_DOORS_DONE` (5).
The car goes to a random one of the elevator's stops (`StageDef.elevators[e]`: a count, then the
floors) to a looped hum. Afterwards the piece faces down from the top floor and up from the bottom
one.

## Time

One round (every player takes a turn) is one month. `Calendar_Advance(1)` (`0x8007B2A8`) runs when
the last player finishes. `GameConfig.month` 0 is April; then `year` and `monthsElapsed`. The season,
`((month + 1) / 3) % 4`, picks `SB<stage>_0<season>.SPC` and `STG<stage>_0<season>.J`. At the start of
each round (player 0) the month's calendar page `CAL_xx.J` plays as a script, if `g_optEventScenes`
is on.

## Play modes (`GameConfig.playMode`)

| Value | Mode | Ends in |
|---|---|---|
| 0 | a normal game | the ending (mode 7) |
| 1 | stage mode: 10 stages, memory card saves, a save prompt every year | stage clear (8) or the title (2) |
| 2 | the attract demo: CPU players only, ends on a button or when `demoTimer` < 0 | the title |

## The rules (`g_boardRules`, `0x800FE068`, `BoardRules`)

Copied from the settings when a game starts (`BoardRules_Init`, `0x80036DAC`). The `enable[]` flags
switch features on (`RULE_*` in `include/game.h`; every index the code reads is named):

| Index | Rule |
|---|---|
| 0 | stores |
| 1 | the elevator |
| 2 | events on a roll of E |
| 3 | the bank |
| 4 | `RULE_FREE_BUYING`: lots and deliveries without the money check |
| 5 | every store open from the start |
| 6 | an event after every turn (`RULE_POST_EVENT`) |
| 7 | pick the event (`RULE_PICK_EVENT`) |
| 8 | (not a flag) the event picked |
| 9 | `RULE_SLOW_DICE` |
| 10 | clear the direction before moving |
| 11 | `RULE_DEBUG_POINTS`: L1 / R1 on the player panels change the points |
| 12 | always talk (`RULE_ALWAYS_TALK`) |
| 13 | never read |
| 14–23 | `RULE_AUTO_*`: `isCpu = 1` while that dialog plays |

Defaults: 0–3 on, the rest off; nothing in the retail game turns the others on.

## The step map

The steps have names (`enum BoardStep`, `STEP_*` in `include/board.h`).

### `Board_PhaseSetup` (`0x800435E4`), steps 0–300

| Step | What happens |
|---|---|
| 0–2 | `Board_CreateTasks`; the aux task runs `Game_LoadCommonTask`; wait |
| 3 | `Board_InitGame`, `Board_LoadData` (loads the board), `Gfx_EnableRender` |
| 4 | `Board_LoadSeasonGraphics(season)` |
| 100 | `Board_StartDisplayTasks`, the task `Board_FrameEnd` |
| 200–213 | stage mode, new game: the background `C_BG<stage>`, the intro script `CNT<stage>.J` (START skips it) |
| 220 | the season graphics, resume the display, focus the camera |
| 300 | the board's music (`Bgm_Play(stage + 7)`) → 2000 |

### `Board_PhaseTurn` (`0x80043A98`), steps 2000–4999

| Step | What happens |
|---|---|
| 2000 | the round starts: player 0 and not a resumed game → 2100, the calendar (2200 if event scenes are off); else 2300 |
| 2100–2120 | the calendar page `CAL_xx.J` as a script, with the event picture; music 6 on the very first month |
| 2200 | `Player_RecordAssets(i)` for every player |
| 2300 | the turn starts: players who are out are skipped (→ 10400); clear `g_turnFlags`; focus the camera |
| 2400–2401 | the turn-start wipe (`Wipe_OpenTask`) |
| 2500–2501 | stage mode, month 0, not the first round: `SavePrompt_Task` |
| 3000 | no players left → 10000 |
| 3100–3101 | "%C<name>'s turn.", unless `g_optCpuFast` |
| 3110–3111 | "There are N steps to the bank." (unreachable) |
| 3200–3203 | a CPU player's talk: entry `charId` of `TALK.J` (`g_charScriptsBlock`) on `EventScript_Task`; only while `g_optSkipCpuTalk` is 0, which the game never sets, or in a `CPU_TALK` build ([script.md](script.md)). The script is unpacked into a block of its packed size rounded up to whole sectors, which the talk scripts on the disc overrun by up to 416 bytes; a `CPU_TALK` build gives every one 2 sectors ([script.md](script.md)) |
| 3300 | a fast CPU player rolls at once; else 3310 |
| 3310–3320 | `TurnMenu_Task`: 0 roll; 1 → 3400 (the map), 2 → 3500 (status), 3 → 3600 (the asset graph), 4 → 3700 (options and saving); all come back to 3310 |
| 3350–3351 | "You are closed today." → 9000 (unreachable) |
| 3800–3801 | `Dice_Task`: −1 cancel → 3310; 8 → 4000; else `stepsLeft = steps = n + 1` → 6000 |
| 4000–4051 | an event: at random (`Event_PickRandom`) or picked (`EventPick_Task`), shown (`EventCard_Task`), then the `EVE_<id>.J` script. Its result: 0 → 8900, 1 → 4000 (another event), 2 → roll again (3800), 3 → move (6000) |

### `Board_PhaseMove` (`0x80044F24`), steps 6000–6001

`Move_Task`; result 0 → 8000, else 8900.

### `Board_PhaseSquare` (`0x800450B4`), steps 8000–8999

| Step | What happens |
|---|---|
| 8000 | hide the square name, the calendar and the minimap |
| 8200 | wait for the message window, then go by the cell (`Board_GetCell`): a store → 8600 (vacant), 8800 (own) or 8700 (someone else's), or 8900 with the stores rule off; the elevator → 8300 and the bank → 8400 (each → 9000 with its rule off); type 0x500 → 9000; anything else → 8900 |
| 8300–8351 | the elevator: "Use the elevator?" (if `noElevator`: "The elevator is out of order."); the ride (`Elevator_Task`), "%C<n>F.%E" |
| 8400 | the bank (`CELL_BANK`), when the move ends on it: `g_turnFlags[TURN_BANK] = 1`, clear `dirX` / `dirFloor` → 8900 |
| 8600–8661 | a vacant store (owner −1): if vandalized, "This store can't be opened due to vandalism."; else the store dialogs (`VacantLot_Task` / `StoreOffer_Task`), then `ShopOpen_Task` |
| 8700–8790 | someone else's store: closed / "Temporary Closed." / shopping (`Shopping_Task`) / a buy-out; with `Player.exempt` (never set), `StealItem_Task` (`0x80061658`, an untranslated Japanese "steal an item" roulette, unreachable) |
| 8800–8851 | the player's own store (owner == `g_curPlayer`): if `noDelivery`, "Delivery is prohibited by an unseen force."; else the delivery dialogs (`DeliveryStore_Task`, `Delivery_Task`) |
| 8900 | wait until every player's `moneyShown` / `assetsShown` has caught up |
| 8901–8921 | money < 0: "Money went to minus.", then the forced dialog `SellStore_Task(0x4B, 1)` |
| then | → 9000 |

### `Board_PhaseEndTurn` (`0x80046E60`), steps 9000–12105

| Step | What happens |
|---|---|
| 9000–9021 | `RULE_POST_EVENT`: an event after the turn (as in the 4000s) |
| 9100 | a CPU player's talk at the end of the turn? → 9200 (only while `g_optSkipCpuTalk` is 0, never in play, or in a `CPU_TALK` build); else 10000 |
| 9200–9202 | entry `16 + charId` of `TALK.J` (`g_charScriptsBlock`) |
| 10000 | `assets = Player_CalcAssets(cur)`; money < 0 → 10100 (stage mode: 12000); else 10200 |
| 10100–10108 | bankruptcy: the `EVE_000.J` script; the player is out, `numActivePlayers--`, `rankKey` |
| 10200 | the win check, among the players still in with `assets >= goalMoney`. A normal game: a winner, or `bankruptLimit` reached → 11000. Stage mode: the richest winner; player 0 → 12100, anyone else → 12000. The demo: `demoTimer < 0` → 11000. Else 10300 |
| 10300–10301 | the turn-end wipe (`Wipe_CloseTask`) |
| 10400 | the next player (after the last: `Calendar_Advance(1)`) → 2000 |
| 11000–11002 | the game is over: the results go back into `GameConfig.players`, the CD and the heap are shut down, `g_gameMode` is set (the demo → 2, a normal game → 7, stage mode → 8 if `g_stageCleared`, else 2), return 1 |
| 12000–12007 | the stage is lost: the `EVE_001.J` script, `g_stageCleared = 0` |
| 12100–12105 | the stage is won: the `CNT10 + stage` script, `g_stageCleared = 1` |

## The tasks the board uses (created by `Board_CreateTasks`)

| Task id | Runs |
|---|---|
| `g_scriptTaskId` (`0x800B26E6`) | `EventScript_Task` |
| `g_loaderTaskId` (`0x800B26C2`) | `LoadJWithTim_Task`, `EventCard_LoadTask` (the loaders) |
| `g_actionTaskIds[0]` (`0x800B26D8`) | `TurnMenu_Task`, its screens, `SavePrompt_Task`, `TurnOptions_Task`, `ShopOpen_Task` |
| `g_diceTaskId`, `g_moveTaskId` | `Dice_Task`, `Move_Task` |
| `g_turnFxTaskId` (`0x800B26D0`) | `Wipe_OpenTask` (at the start of a turn), `Wipe_CloseTask` (at its end) |
| `g_uiTaskIds[8]` (`0x801155A8`) | [0] the message windows, [1] the steps-left window, [7] the store and event dialogs |
| `g_cardTaskId`, `g_mapViewTaskId`, `g_frameEndTaskId` | `Card_SaveTask`, `Board_MapViewTask`, `Board_FrameEnd` |

**The display tasks**, started by `Board_StartDisplayTasks`: `g_piecesTaskId` (`Board_DrawPiecesTask`),
`g_storesTaskId` (`Board_DrawStoresTask`), `g_minimapTaskId` (`Minimap_Task`), `g_playerPanelTaskId`
(`PlayerPanel_Task`), `g_squareInfoTaskId` (`SquareInfo_Task`), `g_calendarTaskId`
(`Hud_CalendarTask`), `g_farBgTaskId` (`Board_FarBackgroundTask`), `g_stageBgTaskId` (the backgrounds
of stages 8 and 9). Their messages are `DISP_*` (`include/hud.h`):

| Message | Meaning |
|---|---|
| 0 | end |
| 1 | slide in |
| 2 | slide out |
| 3 | refresh (the square name, the calendar) |
| 6 | the minimap slides in lower down; the panels rise |
| 7 | the panels count the money and drop |
| 8, 9 | the minimap moves down, up |
| 0xC, 0xD | the minimap shows or hides the ends of the routes |
| 0x10 | in place at once |

## The board's ordering tables

`Board_FrameStub` clears and `Board_FrameEnd` sorts the board's own ordering tables: `g_bgOt0` /
`g_bgOt1` (the two tile-map layers, `g_bgPlanes`), `g_boardOt` (the pieces and stores) and `g_frontOt`
(over everything), each a pair, one per frame buffer. `g_boardOtOn` / `g_frontOtOn` switch them on;
the `*Used` flags mark that something was drawn this frame.

## Answered questions and leftovers

- The command menu's options: 1 `Board_MapViewTask`, 2 `Status_Task` (the stores per player), 3
  `AssetGraph_Task` (the assets per month).
- `Player.exempt` (+0x38; when set, `Board_PhaseSquare` skips the vacant-store and minus-balance steps
  and offers `StealItem_Task`) is never set by any code.
- Steps 3110 ("There are N steps to the bank."), 3350 ("You are closed today."), 8500 and 8930 are
  unreachable: no code sets or steps into them (the scripts only return a result).
- The scripts: see [script.md](script.md).
- `isCpu` really means "controlled by the game" (consistent with all its uses).
- Quirks of the original kept in `board.c`:
  - step 8312 `strcat`s the floor label onto what `g_msgBuf` held before;
  - steps 8620 / 8740 search for an owned store and ignore the result;
  - steps 10100–10103 wait for a fade that nothing started;
  - stage mode treats player 0 as the human (bankrupt means the stage is lost);
  - `Player_FindNearestBank` (`0x80077D80`) leaves `g_routeBankDist[route]` of a route without the
    bank at its old value (+1).
