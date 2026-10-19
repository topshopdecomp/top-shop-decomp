# The memory card and the saves

The code:
- `src/card.c`: the memory card through the BIOS (the PsyQ memory card sample's approach);
- `src/save.c`: the save file: its slots (`Save_*`), the card tasks of the save and load screens, the
  story progress; the buffers `g_cardBuf` and `g_saveSlots`;
- the save screens themselves: `src/options.c` (the turn options, the save prompt) and
  `src/newgame.c` (loading a game).

## The save file

- **One file per card**, `BASLUS-01333TOPSHOP`, 2 blocks, created by `Save_CreateFile`.
- It holds a header (the Shift-JIS title at +4), the **system block** and the **4 game slots**:
  - the system block (`CardSystem`, file offset 0x200): which slots are used, the options, the story
    progress (`g_progress`, a `Progress` of 18 bytes);
  - the game slots, at the file offsets `k_saveSlotOffsets` = {0x280, 0xC00, 0x1580, 0x1F00}.
    In `Save_SelectSlot`, slot index 4 means the system block.
- Each block has a checksum (`Save_Checksum`: the byte sum with every even-numbered byte inverted,
  then inverted).
- The file header uses `k_saveMagic` "SC", the icon `k_saveIconClut` with three 16×16 frames, and the
  title `k_saveTitle` ("ＴＯＰ　ＳＨＯＰ").

### The game slot (`SaveImage`, `include/save.h`)

`Save_Pack` builds it in `g_saveSlots[slot]`; `Save_SelectSlot` copies a slot into `g_saveImage`
(`0x800FE1A8`, one struct assignment of 0x978 bytes); `Save_Unpack` restores the game from it. The
layout, confirmed from both functions (offsets in the slot):

| Offset | Field | What it holds |
|---|---|---|
| 0x000 | `title[13]` | the saving player's `Player.title` |
| 0x00D | `boardStage` | |
| 0x00E | `boardPlayers` | `g_numPlayers` |
| 0x00F | `numActivePlayers` | |
| 0x010 | `curPlayer` | |
| 0x014 | the `GameConfig` fields | `playMode` … `goalMoney`, with `turnOrder[6]` at 0x02C |
| 0x034 | `SavePlayer players[6]` | 0x88 each: the `Player` fields that persist, including the 13-entry `assetHistory` |
| 0x364 | `SaveStore stores[40]` | 0x1E each: the owner, the store type, closures and price changes, 8 × {active, level} |
| 0x814 | `SaveStoreType storeTypes[44]` | the `available` flag, and a *pointer*, `StoreType.store` (the store of that type), saved and restored as it is |
| 0x974 | `checksum` | `Save_Checksum` of the 0x974 bytes before it; 0x978 in all |

## The card tasks

- **`Card_LoadSystemTask`** (in the new-game set-up): reads the progress and the options; "System data
  load complete." / "Memory Card was removed. Load failed."
- **`Card_SaveTask`**: writes the packed game, with the card checks and the format and
  not-enough-space prompts. `Save_Pack` packs the configuration, the players, the stores and the store
  types into a slot; `Save_WriteSlot` writes the buffer.
- **`Card_ReadSavesTask`** with `LoadGame_Task` (the set-up's "Load"): reads, checks, lets the player
  choose a slot; `Save_DescribeSlot` ("No data." / the stage and month).
- **Story mode** saves at every stage clear (`StageClear_Task`, mode 8, runs `Card_SaveProgressTask`).
  Played in an emulator (a stage cleared and saved, the story continued from that card, the last
  stage), the rebuilt game writes card files byte for byte the same as the original's.
- When `Card_SaveTask` or `Card_ReadSavesTask` ends, `g_cardTaskState` is `CARDTASK_CANCELED` 0 (no
  card and "Quit", not formatted, no save), `CARDTASK_READY` 1, or `CARDTASK_REMOVED` 2 (the card was
  taken out: start again).

### The save screen

The save screen (four slots; `Save_DescribeCurrent` 1 = free, 0 = used; "Overwrite?", "Saving… Please
do not remove Memory Card.", "Save completed." / "Save failed.") exists twice: in `TurnOptions_Task`
(the turn menu's options, in free play) and in `SavePrompt_Task` (`0x80064054`, "Save?" at the start of
a year in stage mode). Both draw it with `Save_DrawSlots`. While the card is watched (`busy`),
`Card_Check` runs every frame, and a failure ends with "Memory Card was removed. Save failed.".

## Card access (`src/card.c`)

Card I/O goes through BIOS events (`Card_OpenEvents`, `Card_WaitSwEvent`, `Card_WaitHwEvent`).

- **The result of a card event** (`g_cardStatus`, `include/card.h`): `CARD_OK` 0, `CARD_ERROR` 1 (no
  card), `CARD_TIMEOUT` 2, `CARD_NEW` 3 (a card was put in); `CARD_FAILED(s)` is true for 1 and 2.
- **`Card_Check`** (`0x8001B71C`): `Card_Info`; if that failed, its status. If it is OK and the
  directory was already read (`g_cardInfoValid`), OK. A new card is cleared first (`Card_Clear`). Then
  `Card_Load`: a new (unformatted) card has no files; otherwise the directory is read
  (`Card_ListFiles`, and `g_cardInfoValid = 1`).
- **`Card_CheckRetry`** (`0x8001B8FC`): the same with retries. Status checks (`_card_info`) up to 16 in
  all (that count is never reset), failed loads and failed clears counted apart; it gives up when
  either count passes 15.
- **`Card_CreateFile`** (`0x8001BD9C`): not enough free blocks → 2. 15 tries to create the file (→ 1),
  then 15 to open it to write the header (the title and 1–3 icon frames, by the icon count byte):
  the write failed → 3, the open failed → 4; 0 when done (also in `g_cardCreateResult`).
