# CPU players, event cards and the store dialogs

The code: `src/cpu.c` (the decisions), `src/storemenus.c` (the store dialogs), `src/stores.c` (the
stores' state and the lists of stores), `src/boardmap.c` (the routes), `src/events.c` (event cards,
the points exchange), `src/effects.c` (the cards' effects), `src/talk.c` (the comments at the start
and end of a turn).

## How a CPU player decides

CPU players run the same dialog tasks as humans. Where a human would move the cursor, the task asks a
`Cpu_*` function for the answer:

| Dialog | Decision |
|---|---|
| a junction while moving (`Move_Walk`, `move.c`) | `Cpu_ChooseRoute`: scores each route of `g_routes` by its distance to the bank, the rent it would pay (other players' stores, `goods[numGoods - 1].price`) and its money |
| `VacantLot_Task` (`0x800598DC`) | `Cpu_ChooseStoreToOpen` |
| `StoreOffer_Task` (open, buy or expand here) | `Cpu_RefuseStoreOffer` |
| `Shopping_Task` (someone else's store) | `Cpu_ChooseItemToBuy` |
| `DeliveryStore_Task`, `Delivery_Task` | `Cpu_ChooseStoreToDeliver`, `Cpu_ChooseDelivery` |
| `SellStore_Task` (a negative balance) | `Cpu_ChooseStoreToSell` |
| the elevator | `Cpu_ElevatorChoice` |
| the points exchange (`PointsExchange_Task`) | `Cpu_ChooseExchange` → `Cpu_ExchangeForAssets` / `Cpu_ExchangeForMoney` / `Cpu_EventTarget` |

**Personality.** Tables indexed by the character (`Player.charId`), from `0x800AD9C8` on:

| Table | Address | What it holds |
|---|---|---|
| `k_cpuBankDistance[16]` | `0x800AD9C8` | how many squares ahead the character looks for the bank and the money it may need on the way |
| `k_cpuOpenChance[14][4][3]` | `0x800AD9D8` | the % chance to open a store, by the money left (4 bands) and the bank's distance (3 bands) (`Cpu_ChooseStoreToOpen`, `0x80086934`) |
| `k_cpuPriceRange[14][2]` | `0x800ADA80` | the % of the money left it may spend beyond the cheapest choice, minimum and maximum |
| `k_cpuOfferAccept[14][4]` | `0x800ADA9C` | the share of its money it pays for a store, by how many of its own stores are next to it (`Cpu_RefuseStoreOffer`, `0x800874A0`) |
| `k_cpuDeliveryRatio[14][3]` | `0x800ADAD4` | the % of its money spent on goods, by the bank's distance (near, far, farther) |
| `k_cpuDeliveryChance[14][4]` | `0x800ADB00` | the cumulative % chances of the 4 strategies of `Cpu_ChooseDelivery` |
| `k_cpuExchange[14]` | `0x800ADBBC` | `CpuExchangePref`: the chance of a money card, the money wanted at hand, the character's card list ended by 0xFF |

The decisions also use `rand()`. Some rules
(`RULE_AUTO_*`) set `isCpu` for a while, so that a human player's dialog is answered automatically.

## Routes

`Player_BuildRoutes` (`boardmap.c`) fills `g_routes` (`0x80114168`, `Routes`): up to 4 branches of
`maxSteps` squares from the player's square, each step {x, floor, dx, dfloor}. `Player_FindNearestBank`
sets `g_bankRoute`, `g_bankDistance` and `g_routeBankDist[4]`. `Player.dirX` / `dirFloor` are the
current direction: cleared at the bank and by `RULE_RESET_DIR`; the script opcode 0x18B (`turn_back`)
reverses or stops it.

## Stores (`include/stores.h`)

- **`g_storeTypes[44]`** (`0x800ACEF4`, `StoreType`, 0x3C bytes): `available` (cleared once one is
  opened), the base `value`, the `name` ("Pharmacy"…), 6 `items` {cost, markup (200%), name} from the
  most expensive down, and `store`, the store of that type on the board.
- **`g_stores[40]`** (`0x80114A40`, `Store`, 0x48 bytes):
  - `owner`, its square (`squareId`, `x`, `floor`), `storeType` and `type`;
  - `tempClosed` until `closedUntil`;
  - `numGoods` / `maxGoods`, and 8 `goods` {active, level, basePrice, price};
  - `chainLen` (0–3: the length − 1 of the chain of neighboring stores of one owner, set by
    `Stores_UpdateChains`; it indexes the tables `g_kindMaxGoods` and `k_chainLevels`), `chainTypes`,
    `chainPos`;
  - the price changes: `priceChanged`, `pricePercent`, `priceUntil`.
- **Dates of timed effects** (`closedUntil`, `priceUntil`, `Player.noElevatorUntil`…) are turn
  numbers: monthsElapsed × numPlayers + player (`TURN_NUMBER`), checked every turn
  (`Board_ExpireEffects`, `boardsetup.c`).
- **Paydays**: `Player_PaydayAmount` (`bank.c`):
  `(max(0, assets / 10) + (paydays + 1) × 100) × payScale / payScale of board 0 × (squares walked
  since the last payday × 75 / payScale + 25) / 100`, ×1.5 with the bonus. The event effects
  (`Player.paydayEffect`, +0x33) can stop it (1) or double it (2).

## The store dialogs (`src/storemenus.c`; the board's windows in `src/hud.c`, payday in `src/bank.c`)

`SquareInfo_Task` (the square's name), `StepsLeft_Task`, `MoneyWin_Task`, `Payday_Task`,
`Shopping_Task` ("task_shoping_win"), `VacantLot_Task`, `ItemList_Task`, `Delivery_Task` ("buyingin
win"), `RetailPrices_Task`, `RetailPricesInput_Task`, `SellStore_Task`, `StoreOffer_Task` with
`StoreOffer_Question`, `DeliveryStore_Task` ("shopview win"), `CostWin_Task` ("money − cost = rest").

## Events and event cards

Two different things:

- **The board's events** (board steps 4000–4051): a roll of E draws one at random, and plays its
  script, `EVE_<id>.J` (the ids `EVE_*` in `include/events.h`; [script.md](script.md)).
  `Event_PickRandom` (`0x8008CC4C`) uses a weighted table, `g_eventWeights` (`0x800ADC64`): 49 rows
  of 9 bytes, each an event and its cumulative weight for each of 8 situations (the first year,
  leading, behind the leader by up to 1500 / 3000 / 6000 / 10000 / more, at half the goal). 99 means
  the month's event, `g_monthEvents[month]`. The events that can come up are 24–37, 40–54, 58–72,
  75 and 76, and the months' 80–88; the others never do ([unused-events.md](unused-events.md)).
  Before the script, `EventCard_Task` (`0x8006BC80`) shows the event as a card: it gives the player
  1–3 event points at once (at random; none for event 76; at most 99 in all), three cards are dealt
  face down, the player picks one, and it turns over to the event's picture, `C_<id>_.TIM`
  (`EventCard_LoadTask`, `0x8006D2E4`).
- **The event cards** (`g_eventCards[53]`, `0x800ABF9C`), bought with those points in the points
  exchange: {points, name}, from 0 "Earn 1 Point." to 52 "Claim Victory!" (99 points). They have
  names in `include/events.h` (`EV_EARN_POINT` 0 … `EV_CLAIM_VICTORY` 52, in `g_eventCards` order).
  Their effects are C, not scripts (below). `Player.points` (+0x3A) is a player's points balance.

**Targets**: `g_eventTargets[event]` (`0x80111F70`) is a bit mask of the players a card can target,
filled by `Event_ListTargets`. A CPU player's target per card comes from `Cpu_EventTarget`
(`0x80089B84`), which dispatches to the `CpuTarget_*` helpers (named after the cards they serve). Four
helpers are never called (`*Unused`).

**In play** (`src/events.c`, `src/effects.c`):
- A card is bought in the points exchange at the bank: `PointsExchange_Task` (`0x8006D4F4`; 99 points
  at most; "Is this event OK?"). A card used on a rival opens `PlayerSelect_Task` (`0x8006FA50`), which
  offers the targets from `Event_ListTargets` (`0x80070060`).
- It is carried out by `Event_ApplyTask` (`0x8006E3EC`). These effects have their own task steps:
  - `Effect_CloseChain` (`0x80070974`);
  - `Effect_OpenStoreNewSquare` (`0x80070C88`; "Open a store for free" and "Open new store with rival
    money": a random vacant lot on a square where the player has no store);
  - `Effect_TakeOverChain`, `Effect_StealChain`, `Effect_Delivery`, `Effect_RebuildClosed`,
    `Effect_TakeOverClosed`, `Effect_DemolishClosed`;
  - `Effect_ShuffleMoney` ("Players money exchange");
  - `Effect_SwitchPlaces` (`0x800726FC`, "Switch places with rival").

  `Effect_BuyVacant` (`0x800729A4`) is never called.
- The dice cards set `Player.diceEffect` (`DICE_*`; see [board.md](board.md)). "Equip "Chrip-Chrip"
  shoes" (`EV_SHOES`, spelled so on the card) sets `Player.shoes` (+0x3B): the footsteps squeak (sound
  0x36).
- Every card but the money cards and Claim Victory was bought once in an emulator, in the original
  and the rebuilt game, and the players and the stores compared after each: the same.
