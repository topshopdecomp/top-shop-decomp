# The events the game never plays

The disc holds 100 event scripts, `EVE_000.J`–`EVE_099.J`, decompiled into
`scripts/events/eve_000.tss`–`eve_099.tss`. The game plays 54 of them; the other 46 can't come up in
any game. This page lists those 46, what they would say and do, and why several of them would not work
even if they were played. The script language and the interpreter are in [script.md](script.md); the
events that are played are described in [cpu.md](cpu.md) and [board.md](board.md).

## Which events are played

- **At random**: `Event_PickRandom` (`0x8008CC4C`) draws from `g_eventWeights` (`0x800ADC64`), 49 rows
  of an event and its cumulative weights for 8 situations. The rows name only events 24–37, 40–54,
  58–72, 75 and 76; the last row, 99, means "the month's event".
- **The month's event**: `g_monthEvents` (`0x800ADE20`), one per month, events 80–88 (October's, for
  example, is 82: Pumpkin Head's Halloween sale).
- **Started by the board itself**: event 0 (a player goes bankrupt) and event 1 (a story stage is
  lost).
- **The debug rules**: `RULE_PICK_EVENT` would let a player pick any event (`EventPick_Task`,
  `0x8006CECC`) and `RULE_POST_EVENT` adds an event after every turn ([board.md](board.md)), but
  nothing turns them on.

So events **2–23, 38, 39, 55–57, 73, 74, 77–79 and 89–99** are never played.

## What they are

The English is from the comments in the scripts (`tools/tss_english.json`); `<player>` is the
current player.

| Events | Who speaks | What it says | What its code does |
|---|---|---|---|
| 2–7 | the Young Lady, "an announcement from the information counter" | the prices of the goods in `<player>`'s store go up 3% or 4%, or down 1%–4% | `nop_183`: nothing |
| 8–15 | the Owner, "a message from me" | "I'll raise / lower the prices of everyone's goods" by 1%, 4%, 3%, 4% / 1%–4% | `nop_183`: nothing |
| 16–23 | the Young Lady | the prices in every store but `<player>`'s go up or down 1%–4% | `nop_183`: nothing |
| 38, 39 | — | picks a player at random; with exactly one store (the script tests `== 1`), "『that store's type』's price went up 5% ♭" (38) or "down 5% ≦" (39); otherwise "Seems it doesn't mean anything ≦" | `nop_183`: nothing |
| 55–57 | the Young Lady | the building's maintenance crew is stuck in a traffic jam (55), the building is renovated (56), it gets a makeover (57): "it will take about a year / 4 months / a month, so time moves forward"; closed stores and broken elevators come back sooner | `advance_months` 12, 4 or 1: the calendar moves on |
| 73 | Pumpkin Head | the chance quiz (below) | money for a right answer |
| 74 | the Owner | "You can buy every vacant store ≧ (but not in an area where you have a store)" | `buy_lot`: the current player may open one store on a vacant lot along its route (the usual dialog) |
| 77 | Pumpkin Head | the speed-up quiz (below) | money for each right answer |
| 78, 79 | — | "A TV station came to report ☆ Everyone's store reputation went up 20%"; "Terrorists are holed up inside ≦ Everyone's store reputation went down 30%" | `nop_187`: nothing |
| 89–94 | — | the reputation of `<player>`'s store (89, 90), of everyone's (91, 92), of every store but `<player>`'s (93, 94) went up or down 10% | `nop_187`: nothing |
| 95–99 | the Young Lady | all five: the prices of the goods in `<player>`'s store go up 1% | a test version of the event card scene (below); `nop_183`: nothing |

Two texts disagree with their own operands: event 9 says the prices go up 4%, but its `nop_183` is
given 2 (the series 8–11 would be 1, 2, 3, 4); event 92 says everyone's reputation went down 10%, but
its `nop_187` is given +10. Neither matters, since the opcodes do nothing.

## Why most of them would do nothing

- **Prices and reputation**: the opcodes these events end with, `nop_183` (prices) and `nop_187`
  (reputation), skip their two operands and return (`Script_OpNop183` `0x800858D8`,
  `Script_OpNop187` `0x80085918`; `0x182` and `0x186` are the same). Played, events 2–23, 38, 39,
  78, 79 and 89–99 would show their card and text and change nothing.
- **The ones that would work**: 55–57 (`advance_months`, `Script_OpAdvanceMonths` `0x80085990`) and
  74 (`buy_lot`, `Script_OpBuyLot` `0x80082B94`), although 74 offers one vacant lot, not "every
  vacant store". The quizzes would pay out if they could be answered (below).
- **Events 95–99** are a test version of the event card scene: the corner effects are typed in as
  numbers, and each entry is confirmed with script button bit `0x0200`, which no button gives
  ([script.md](script.md)), so a human player could not get past them.

## Pumpkin Head's quizzes

Pumpkin Head hosts both quizzes. In normal play he appears only in October's event (82): "Hee!?
Halloween sale. All items are 30% off for one turn."

### Event 73: the chance quiz

"Here's a chance quiz. Pick the correct answer from the three choices about the store items, and a
BONUS will be awarded."

- **The question**: one of 44 store types, each with 6 goods listed from the most expensive to the
  cheapest (a table in the script), and one of 6 ranks: the most expensive, the 2nd or 3rd most
  expensive, the 3rd or 2nd cheapest, the cheapest. "At *store*, which is the *rank* item?", with
  three of that store's goods as the answers, in a random order. The store types are the Japanese
  release's: the first is Matsumoto Kiyoshi, a real drugstore chain (Health food, Hair care,
  Toiletries, Cosmetics, Medicine, Daily goods).
- **The prize**: `money_by_years` × 5, where `money_by_years` is 100 × (months played / 24 + 1), an
  amount that grows every two years. A wrong answer shows the right one.
- **It stops at once**: its first text, an empty `talk ""` at offset `0x00CB`, lacks its terminating
  NUL, so the interpreter goes on at `0x00CE`, inside the next instruction, reads the word `0x2C80`
  (no opcode) and stops the script with the "unknown code" message, "コード：n / アドレス：offset"
  (`Script_Run`, `0x8007C124`). Seen in an emulator with the event forced by the debug rules, on the
  original disc too.
- **A human could not answer**: the answer menu moves its cursor with up and down, but confirming
  needs button bit `0x0200`, which no button gives. Only a CPU player gets through: it picks the right
  answer 70% of the time (`quiz_choose` in `eve_073.tss`).
- **Half translated**: the introduction and the results are in English ("Congratulations! As a prize,
  I will give you…", "Wrong answer. The correct answer is…"), the question itself is still Japanese
  ("…で …い商品は次のどれですか？", "at …, which is the …-est item?").

### Event 77: the speed-up quiz

"This is a speed-up quiz. The quiz is two-choice questions about store goods; a wrong answer or
running out of time ends it. 'Number right × *money*' is yours. Start!"

- **The questions**: "*store*: which of these does it sell?" or "which of these does it not sell?",
  two answers each, against a timer, one after another until a wrong answer or "Time." / "I'm sorry,
  but the time is up."
- **The prize**: the number right × `money_by_years`; none right, "Since you didn't get the answers
  right, you didn't win anything."
- **A human could not answer**: the same unobtainable button bit `0x0200` confirms an answer.
- **Partly translated**: "Start!", "Time." and the results are in English, the rules and the questions
  in Japanese.

## The language

The US release left these 46 events in Japanese, apart from the English lines of events 73 and 77
above. The game can draw the Japanese (its font has the characters), but never does.

## Other script data the game never uses

- `CAL_13.J`, a copy of January's calendar page: `CalendarFile` (`src/board.c`) gives `CAL_01.J`–
  `CAL_12.J` only.
- `TALK.J` entries 14, 15, 30 and 31: the start- and end-of-turn talk of characters 14 and 15, which
  can't be chosen (the setup offers 14 characters; their lines are variants of Colin's and Squosh's).
- The characters' talk as a whole is never shown in the retail game; see [script.md](script.md).
