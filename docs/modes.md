# Game modes

`main` (`0x8001A170`) runs one task per game mode, chosen by `g_gameMode` (`0x800B23F8`); see
[system.md](system.md). Declarations: `include/main.h` (`MODE_*`, `g_gameMode`), one header per mode's
file (`include/logo.h`, `title.h`, `newgame.h`, `setup.h`, `menu.h`, `ending.h`, `stageclear.h`,
`staffroll.h`), and the settings in `include/game.h`.

## How the modes lead to each other

Confirmed from every write of `g_gameMode`:

```
0 Logo ─────────────→ 2 Title ──START──→ 5 NewGame ──→ 6 Board ──→ 7 Ending ─→ 2
                        │                                 │
                        └─30 s idle→ intro (3) → attract  ├──→ 8 StageClear ─→ 6 next stage
                                     demo: 6 with         │                  ├→ 9 StaffRoll → 2   (after stage 9)
                                     g_gameConfig.playMode = 2              └→ 2 (declined to continue)
                                                          └──→ 2 Title (stage lost, or demo over)
```

How the board ends: the attract demo goes back to 2; a normal game (`playMode` 0) goes to 7; stage
mode (`playMode` 1) goes to 8 if the stage was won (`g_stageCleared`), else to 2. See
[board.md](board.md), steps 11000–12105.

| Mode | Task | Address | What it is | Evidence |
|---|---|---|---|---|
| 0 | `Logo_Task` | `0x80041464` | the publisher and developer logos | loads `A1.TIM` and `E_KID.TIM`; the debug text "Exec Logo"; `src/logo.c` |
| 2 | `Title_Task` | `0x80040EB0` | the title screen | runs `TitleMenu_Task` (`0x8004191C`): START → 5; after 1800 frames idle, the intro, then the attract demo |
| 3 | — | — | a marker while `Title_Task` runs its intro task, `Title_SplashTask` | `main` has no case for 3 |
| 5 | `NewGame_Task` | `0x80036F1C` | the new-game set-up | the memory card's "System data load", "exec task_player_menu", "exec name entry", "exec task_char_shuffle"; sets stage 0 |
| 6 | `Board_Task` | `0x800432F0` | the board game | the turn-phase functions `0x800435E4`, `0x80043A98`, `0x80044F24`, `0x800450B4`, `0x80046E60`; the strings "'s turn.", "steps to the bank" |
| 7 | `Ending_Task` | `0x80076068` | the ending: the final results | loads `ENDING.SPC`, `HYO_BG.TIM` and the players' `CHR*P0.J` |
| 8 | `StageClear_Task` | `0x8007496C` | between story stages | counts the clear and offers to save (`Card_SaveProgressTask`, with the format prompts); after stage 9 → mode 9; else it asks whether to continue |
| 9 | `StaffRoll_Task` | `0x8007618C` | the staff roll | loads `STAFF*.SPC`, `E_CHR*.SPC`, `END_BG.TIM` |
| 10 | `Mode10_Stub` | `0x8001C950` | unused | an empty function, never reached: `TitleMenu_Task` never returns the result (2) that leads here, and `NewGame_Task` would only go here if `g_newGameStarted` were set, which it never is (only ever 0) |

## What every mode starts with

Every mode task's first step calls `Mode_CreateSubTasks` (`0x80042A3C`). It creates:
- 16 sub-tasks, `g_subTaskIds` (`0x80117E50`) with the work pointers `g_subTaskWork` (`0x80117E70`);
- two more: `g_cardTaskId` and `g_uiTaskIds[0]` (the message windows' task);

then calls `Fade_Init` and `Fade_SetBlack`. It also sets 60 frames a second (`g_vsyncInterval` 0;
the board sets 30, see [system.md](system.md)). The screens run as sub-tasks, and the mode task polls
`Task_IsDone` / `Task_GetUserData` for their result.

## Fades (`0x80056124`–`0x800564C4`)

Two fade tasks. The level per channel is 20.12 fixed point: 0 normal, `0xFFF01000` (−255.0) black.
`Fade_SetBlack` / `Fade_SetClear` set it at once; `Fade_In(frames)` / `Fade_Out(frames)` animate it
through `Fade_AnimTask` (`0x80055A2C`); `Fade_IsDone` polls. More in [graphics.md](graphics.md).

## The game settings: `g_gameConfig` (`0x800FE0B0`)

`GameConfig` in `include/game.h`; every field is named:
- `playMode`: 0 a normal game, 1 stage mode, 2 the attract demo (`PLAY_*`);
- `stage` (0–9);
- `startMoney` / `goalMoney`, from `g_stageMoneyTable` (`0x800A8AE0`): 3000 / 10000 for stage 0,
  rising to 5000 / 30000 for stage 9;
- `turnOrder[6]`;
- `players[6]` (`PlayerSetup`: `charId`, `isCpu`, `pad`, `name`, `wasOut`, `finalRank`);
- `numPlayers`, `numHumans`, `numCpus`, `startGoods`, `maxBuy`, `month`, `year`, `monthsElapsed`,
  `bankruptLimit`, `demoTimer` (see [board.md](board.md));
- `unused01` (10 by default; saved, never read) and `unused0B` (not used by the game code).

The characters (`g_cpuNames`, `0x800A8AA8`, 14): Meower, Maiko, Dona, Gummi, Merlin, Cindy, Rob,
Patty, Chat, Oilow, Bro, Squosh, Santa, Colin.

## The title screen and the other screens

- **`TitleMenu_Task`** (`0x8004191C`):
  1. fades in;
  2. the sign (sprite 0xC of group 6) slides in from the left and stretches while the jingle (XA)
     plays (START skips to the end); the logo and title values (`logoX`, `titleY`…) are animated, but
     nothing reads them;
  3. the wheel (sprite 1 of group 6, 14 palettes cycling) spins up and slows down, and the
     1800-frame idle countdown runs.

  Every 0.5–2.5 s a title animation runs on `g_subTaskIds[1]`: `TitleMenu_EffectATask` (`0x800422E0`,
  70% of the time: a character peeks in at one of six places) or `TitleMenu_EffectBTask`
  (`0x80042680`, 30%: a character walks across).
- **The attract demo** (`Title_SetupDemo`, inline in `Title_Task` in the original, `0x80040EB0`): 4
  CPU players (all six set-ups filled with characters 0–5 and their `g_cpuNames`), a random board of
  the first eight, 3000 to start, 10000 to win, `playMode` 2.
- **A story stage's set-up**, `Story_SetupPlayers` (`0x80042900`): player 1 human, the stage's three
  opponents (`k_storyOpponents[stage * 4 + i]`), the money from `g_stageMoneyTable`.
- **The staff roll**: one of the 14 characters walks across the screen every 120 frames, each in
  turn (`StaffRoll_WalkerTask`, `0x80075DD0`).

## The options screens

- **The main menu's options** (`Options_Task`, `0x8003F424`, `src/newgame.c`): up and down choose a
  row, left and right change it. The rows: event scenes on or off (`g_optEventScenes`), the CPU
  players' speed (`g_optCpuFast`), stereo or mono (`g_optMono`), a song test (cross plays song 0–22),
  a sound-effect test (cross plays sound effect 0–35, the entry 23 + n of `SEQ.J`), and Exit. The
  cursor wraps between the first row and Exit, so two more rows the code handles can't be reached:
  `OPTION_SKIP_CPU_TALK`, the switch for the CPU players' talk (`g_optSkipCpuTalk`, which the game
  sets to 1 and never changes; [script.md](script.md)), and `OPTION_XA_TEST`, an XA voice test
  (cross plays track 0–223 of `g_xaSets[1]`, the characters' voices; [fileio.md](fileio.md)).
- **The turn options** (`TurnOptions_Task`, `0x80062458`, `src/options.c`; the turn menu's options,
  board step 3700): event scenes and the CPU players' speed, the controllers, and saving the game
  (the save screen, [card.md](card.md)). A comment of the original's code left there mentions a row
  for the talk switch; there is none.

## Answered questions

- How the board ends (above).
- `0x800FE068` is `g_boardRules` (`BoardRules`); see [board.md](board.md).
