# Top Shop's event scripts

The scenes of Board Game - Top Shop (the events, the story-mode scenes, the calendar pages and
the characters' comments during a turn) are bytecode run by the interpreter in `src/script.c`
(opcode handlers in `src/script.c`, `src/sceneops.c`, `src/gameops.c`; the scene they draw in
`src/scene.c`). This directory holds every script on the disc, disassembled into a readable text
form by `tools/tss.py`, with names for everything the interpreter defines, the routines the
scripts share, comments, and an English rendering of each text.

    make scripts                              # assemble every file here into the disc's files,
                                              # in scripts/build/ (tools/tss.py build)
    python3 -I tools/tss.py build [DIR] [-o OUT]   # the same for the files in DIR, into OUT
    python3 -I tools/tss.py extract           # write this directory again from the disc,
                                              # but not over the edited scripts (--force)
    python3 -I tools/tss.py extract DIR       # every script as on the disc, into DIR
    python3 -I tools/tss.py asm FILE.tss OUT  # one file to raw bytecode
    python3 -I tools/tss.py dis disc/A/EVE_049.J      # disassemble a script file to stdout
    python3 -I tools/tss.py dis disc/A/TALK.J 3       # (TALK.J: the entry number)

The files are UTF-8. As they are, they assemble back to exactly the bytes on the disc, except
the talk scripts, which are edited: their texts are in English and stay up three times as long (see
"Texts" below). `tools/tss.py extract` keeps a file that holds an edited script, unless given
`--force`; into a new directory it writes every script as it is on the disc.

## Editing the scripts

Edit any of the files, then `make scripts` (or `tools/tss.py build`). It writes every disc file
that holds scripts into `scripts/build/`: `EVE_NNN.J`, `CNTNN.J`, `CAL_NN.J` and `TALK.J`. In a
file whose script was edited, the script is assembled again and the rest of the file (the
pictures) is taken from the disc (`disc/A`); an edited talk script is LZSS-packed again. Files
with no edited script are the disc's own. The build lists the files with edited scripts.

A script may grow or shrink: every address in it (jumps, calls, tables, texts, animations,
paths) is written as a label, so instructions and data can be added, removed or moved. Limits:

- The executable knows where each file is on the disc and how many 2048-byte sectors it reads
  (`g_fileSectorTable`), so a file can't grow past the end of its last sector. The build reports
  an error saying how many bytes too long such a file is, and exits with status 1 (it still
  writes the file). A file that got shorter is padded with zeros to its size on the disc, so
  that every file stays where the executable looks for it (`g_fileLocTable`).
- The game unpacks a talk script into a buffer the size of its packed form rounded up to 2048
  bytes, which some talk scripts on the disc already overrun (by up to 416 bytes); it never plays
  them. A build with `CPU_TALK` (`make CPU_TALK=1`), which plays them, unpacks every one into 4096
  bytes, and the build stops a talk script that unpacks to more (an error, as above).
- A text of an edited talk script must fit the talk panel (see "Texts" below); the build reports
  one that does not as an error.

The disc image the build makes (`make`) builds the scripts first and takes these files from
`scripts/build/` in place of the disc's, so edited scripts play.

## The files

| Directory | Source on the disc | What |
|---|---|---|
| `events/eve_NNN.tss` | `EVE_NNN.J` entry 0 | the scene of event NNN (0: bankruptcy, 1: story stage lost) |
| `stages/cntNN.tss` | `CNTNN.J` entry 0 | story mode: 00-09 the introduction of stage NN+1, 10-19 stage NN-9 won |
| `calendar/cal_NN.tss` | `CAL_NN.J` entry 0 | the animated calendar page of month NN (13 is never loaded) |
| `talk/start_NN.tss`, `talk/end_NN.tss` | `TALK.J` entries NN and 16+NN (LZSS) | what character NN says at the start / the end of its turn |

Characters: 0 Meower, 1 Maiko, 2 Dona, 3 Gummi, 4 Merlin, 5 Cindy, 6 Rob, 7 Patty, 8 Chat,
9 Oilow, 10 Bro, 11 Squosh, 12 Santa, 13 Colin (`g_cpuNames`). Talk entries 14 and 15 are for
characters 14 and 15, which can't be chosen (the setup offers 14); their lines are variants of
Colin's and Squosh's.

Each file starts with a comment block: where the script comes from, when the game plays it, what
its code does to the game ("What it does") and what it says ("What it says").

## The language

### How a script runs

The board loads a script and runs it in a task (`EventScript_Task`, `0x80083B34`): an event, a
story scene, a calendar page or a character's talk. Every frame, `Script_Run` (`0x8007C124`)
carries out statements one after the other until one of them ends the frame. On the board a frame
is 1/30 second, two video frames (`g_vsyncInterval` 2).

- **Statements that end the frame**: `wait`, `wait_buttons`, `wait_buttons_for`, `yield`, `halt`,
  `marker`, `fade_level_for`, `wait_fade`, `view_wait`, `talk_wait_open`, `talk_wait_closed`,
  `portrait_show`, `portrait_hide`, `portrait_face` and `stores_close`; `buy_lot` and `sell_store`
  while their dialog is open. `wait n` takes n frames, the others at least one. Every other
  statement takes no time, so a loop must go through one of these, or the game freezes in it.
- **The start**: the first byte of the script, with all 256 variables at 0, the view closed, the 4
  layers, 64 sprites and 16 rectangles off, the talk panel hidden and empty (`Script_Reset`,
  `0x8007C034`).
- **The end**: `end` ends the script and frees the talk panel's characters; the task is done once
  an XA voice has finished. `halt` stays where it is for good (no script uses it). A message 0
  from another task stops the script and cuts the voice.
- **An event's result**, what the board does next: the turn goes on (0, the default); another
  event (`result_another`, 1); the player rolls again (`result_reroll`, 2); the player moves n
  squares (`move_steps n`, 3). The last one carried out counts.
- **Calls**: `call` pushes the return point and `$l0`-`$l15` on a stack of 16 calls
  (`g_scriptStack`, `0x801157D8`), `return` pops them. A routine sees its caller's `$l0`-`$l15`
  and may change them, since the caller gets its own back; so a routine gives its answer in
  another variable (`money_by_years` uses `$v00`). Nothing checks the stack: 17 calls deep, or a
  `return` without a call, goes outside it.
- **A trap**: `fade_level_for` leaves the interpreter's frame counter at 1 for the next statement.
  A `wait` or `wait_buttons_for` right after it counts one frame short (`wait 1` never ends), and
  an `end` right after it does not end: the game freezes. Put any other statement in between
  (`yield`, `$v00 = 0`). No script on the disc uses `fade_level_for`.

### Writing statements

- One statement per line; `label:` on a line of its own; `;` starts a comment (not inside quotes).
  Names are case sensitive: statements are lower case, constants upper case.
- **Numbers** are decimal or `0x` hexadecimal. A constant expression joins numbers, constant names
  and labels with `+`, `-` and `|`, worked out from left to right (no precedence, no parentheses);
  `-` before a value negates it (`IMG_FACE+1`, `MIDDLE_CENTER|SEMITRANS2`, `-100`). A variable
  stands alone, never inside an expression.
- **Variables** are written `$name` (below). Any operand may be a variable, read when the statement
  runs, except one the statement writes to (and the table of `$x = table[index]`, where a variable
  is an array, below). So `goto $x` jumps to the offset in `$x`, and a talk script reads a text's
  offset from a table and shows it with `talk_data $l0`.
- **Labels**: letters, digits and `_`, not starting with a digit, each defined once; a label is
  the offset of what follows it, in bytes from the start of the script. A label with the name of a
  constant reads as the constant.
- **`key=value` operands**: a few statements name their operands (the table below). The key may be
  left out; when it is given it must be the one of that place. The keys are only there to be read.
- **Plain forms**: every statement can also be written as its name and its operands, in the
  order of the table: `add $x, 1` is `$x += 1`. The names of the statements written with `=` are
  `set`, `add`, `sub`, `mul`, `div`, `mod`, `and`, `or`, `xor`, `random`, `get_indexed`, `set_indexed`, `is_xa_playing`, `get_talk_y`. The disassembler writes the plain form for a statement whose destination is a constant
  instead of a variable (the scripts have a few, e.g. `set 0, 0`): it only sets `$result`.
- **Tests**: `if a OP b goto label`, OP one of `==` `!=` `>` `<` `>=` `<=` (signed), `&` (every
  bit of b is set in a), `!&` (a and b have no bit in common). `?n` writes the test by its number
  (0-7, in that order); another number never holds in this build, and in the original holds or
  not as a leftover register happens to be.
- **`switch value: label, label, ...`** jumps to the label the value picks (0 the first), and goes
  on after the list when the value is out of range.
- **`var $name = $vNN`** names a variable for the rest of the file (not with a name of the
  interpreter's). The event scripts do this for the card scene's variables (below).
- The assembler stops at the first error, with its line number.

### Values and variables

256 variables, `$v00`-`$vFF`, hold signed 16-bit numbers (-32768 to 32767). Statements compute in
32 bits and keep the low 16 (32767 + 1 is -32768). `/` and `%` round toward 0. `random n` is 0 to
n - 1 (a 17-bit random number, modulo n). Dividing by 0 (`/= 0`, `%= 0`, `random 0`) runs the
compiler's divide-by-zero trap (`break 7`), in the original too: never let it happen.

`$x = table[index]` reads the word at `table` + 2 × index (`table` a label, or an offset);
`$x = $base[index]` reads variable `$base` + index, an array of variables; `$base[index] = value`
writes one (with a constant base, it only sets `$result`). Nothing checks the index.

Some variables have names:

| Variable | Index | Holds |
|---|---|---|
| `$result` | 0xC0 | the value the last computing statement produced; the buttons `wait_buttons` and `wait_buttons_for` saw (0 on a time-out) |
| `$cur_held` | 0xC1 | the current player's buttons held (0 for a CPU player) |
| `$held0`-`$held5` | 0xC2-0xC7 | each player's buttons held |
| `$cur_pressed` | 0xC8 | the current player's buttons pressed this frame |
| `$pressed0`-`$pressed5` | 0xC9-0xCE | each player's buttons pressed |
| `$num_players` | 0xCF | players in the game |
| `$cur_player` | 0xD0 | whose turn it is |
| `$frame` | 0xD1 | counts frames (0-32767) |
| `$entry` | 0xD2 | the situation a talk script was started for |
| `$cur_is_cpu` | 0xD3 | `CPU` (0x10) if the current player is a CPU player, else 0 |
| `$is_cpu0`-`$is_cpu5` | 0xD4-0xD9 | the same for each player |
| `$floors` | 0xDA | floors of the board |
| `$months` | 0xDB | months played |
| `$start_money` | 0xDC | the money players start with |
| `$goal` | 0xDD | the goal (assets to win) |
| `$pad_held` | 0xDE | pad 1's buttons held |
| `$pad_pressed` | 0xDF | pad 1's buttons pressed |
| `$any_held` | 0xE0 | the buttons held on pad 1 and every human player's pad |
| `$any_pressed` | 0xE1 | the same, pressed |
| `$char` | 0xE2 | the current player's character (0 Meower ... 13 Colin) |
| `$active_players` | 0xE3 | players still in |
| `$l0`-`$l15` | 0xF0-0xFF | saved by `call` and restored by `return` (local variables) |

0xC1-0xE3 are set by the interpreter every frame (`Scene_Update`); buttons are the script's
button bits (below). The others are the script's own; the event card scene's are below.

### Statements

| Statement | Opcode | What it does |
|---|---|---|
| `end` | 0x00 | end of the script (frees the talk panel's glyphs) |
| `wait frames` | 0x01 | wait n frames (FOREVER: never goes on) |
| `wait_buttons buttons` | 0x02 | wait until a button of the mask is pressed (pad 1); $result = the buttons |
| `wait_buttons_for buttons, frames` | 0x03 | the same for at most n frames ($result = 0 on time-out); FOREVER: stays here |
| `goto label` | 0x10 | jump |
| `switch value: label, label, ...` | 0x12 | jump to the label in the list picked by the value (0 the first); past the list if out of range |
| `call label` | 0x13 | call a routine: push the return point and $l0-$l15 |
| `return` | 0x14 | return from a call, restoring $l0-$l15 |
| `halt` | 0x15 | stay here for good |
| `if a OP b goto label` | 0x20 | jump if the test holds: == != > < >= <=, & (all bits of b set in a), !& (no bit in common) |
| `$x = value` | 0x21 | set a variable (every computing statement also sets $result) |
| `$x += value` | 0x23 | add |
| `$x -= value` | 0x24 | subtract |
| `$x *= value` | 0x25 | multiply |
| `$x /= value` | 0x26 | divide (rounds toward 0) |
| `$x %= value` | 0x27 | remainder |
| `$x &= value` | 0x28 | bitwise and |
| `$x \|= value` | 0x29 | bitwise or |
| `$x ^= value` | 0x2A | bitwise exclusive or |
| `negate $x` | 0x2B | $x = -$x |
| `$x = random n` | 0x2C | a random number 0 .. n-1 |
| `$x = table[index]` | 0x2D | read a table: word [index] of a table in the script, or variable $base+index |
| `$base[index] = value` | 0x2E | write variable $base+index |
| `play_bgm song` | 0x30 | play song n |
| `stop_bgm` | 0x31 | stop the music |
| `play_se sound` | 0x32 | play sound effect n (game sound n+0x17) |
| `nop_33` | 0x33 | no effect |
| `nop_34 value` | 0x34 | no effect (reads its operand) |
| `nop_35` | 0x35 | no effect |
| `play_xa file, channel` | 0x36 | play voice track file*16+channel of XA set 1 |
| `stop_xa` | 0x37 | stop the voice |
| `yield` | 0x38 | go on next frame |
| `$x = xa_playing` | 0x39 | $x = 1 while a voice plays |
| `fade_in frames` | 0x40 | fade in over n frames |
| `fade_out frames` | 0x41 | fade out |
| `fade_from_white frames` | 0x42 | fade in from white |
| `fade_to_white frames` | 0x43 | fade out to white |
| `fade_level_for r=n, g=n, b=n, frames=n` | 0x44 | set the fade level (r, g, b) for n frames, then put it back |
| `wait_fade` | 0x45 | wait until the fade is done |
| `marker "name"` | 0x60 | a name in the script, skipped (costs a frame; 13 characters at most) |
| `view_set x=n, y=n, w=n, h=n` | 0x61 | the view rectangle (x or y -1: centered); closed |
| `view_open style, frames` | 0x62 | open the view over n frames |
| `view_close style, frames` | 0x63 | close the view |
| `view_wait` | 0x64 | wait for the view to open or close |
| `layer_init n, image=n, pri=n, flags=flags` | 0x70 | background layer n shows scene image (priority, LAYER flags, BANDn: lines per band) |
| `layer_pos n, x, y` | 0x71 | layer position |
| `layer_scroll n, vx, vy` | 0x72 | layer scrolls by vx, vy a frame |
| `layer_lines n, first=n, count=n, x=n` | 0x73 | x offset of bands first..first+count |
| `layer_line_steps n, first=n, count=n, step=n` | 0x74 | x offset steps of bands (each frame) |
| `layer_wave n, length=n, amp=n, phase=n` | 0x75 | bands follow a sine wave |
| `layer_wave_scroll n, step` | 0x76 | the wave moves by step a frame |
| `sprite_init n, image=image, attr=attr, x=n, y=n, pri=n` | 0x80 | sprite n shows an image at x, y (scale 1, angle 0) |
| `sprite_hide n` | 0x81 | remove sprite n |
| `sprite_image n, image=image, attr=attr` | 0x82 | image and attr of sprite n (stops its animation and movement) |
| `sprite_attr n, attr` | 0x83 | attr of sprite n |
| `sprite_move_by n, dx, dy` | 0x84 | move sprite n by dx, dy |
| `sprite_pos n, x, y` | 0x85 | position of sprite n |
| `sprite_move_to n, x=n, y=n, speed=n` | 0x86 | sprite n moves to x, y at speed pixels a frame (0: waits x frames, -1: jumps there) |
| `sprite_scale_by n, sx, sy` | 0x87 | add to the scales (256 = 1) |
| `sprite_scale n, sx, sy` | 0x88 | set the scales (256 = 1) |
| `nop_89 a, b, c` | 0x89 | no effect |
| `sprite_rotate n, angle` | 0x8D | turn sprite n (32768 = a full turn) |
| `sprite_angle n, angle` | 0x8E | set the angle |
| `nop_8F a, b` | 0x8F | no effect |
| `sprite_animate n, anim` | 0x90 | sprite n plays an animation (frames) |
| `sprite_path n, path` | 0x91 | sprite n follows a path (steps) |
| `sprite_get_state n, $x` | 0x92 | $x = sprite n's state (1 animating, 2 moving, 0x10 on a path) |
| `sprite_get_pos n, $x, $y` | 0x93 | position of sprite n |
| `sprite_get_scale n, $sx, $sy` | 0x94 | scales of sprite n |
| `sprite_get_angle n, $x` | 0x95 | angle of sprite n |
| `get_image_size image, $w, $h` | 0x96 | width and height of scene image n |
| `set_dim level` | 0x97 | how dark DIM sprites are (0-255) |
| `rect_set n, x=n, y=n, w=n, h=n, value=n` | 0xA0 | rectangle n (never used for anything) |
| `rect_hide n` | 0xA1 | rectangle n off |
| `rect_to_view n, value` | 0xA2 | rectangle n = the view |
| `step_toward x=n, y=n, distance=n, angle=n, to_x=$x, to_y=$x` | 0xB0 | $to_x, $to_y = x, y moved distance at angle |
| `talk_open` | 0xC0 | slide the talk panel in |
| `talk_close` | 0xC1 | slide it out |
| `talk_wait_open` | 0xC2 | wait until it is in |
| `talk_wait_closed` | 0xC3 | wait until it is out |
| `talk_cursor row` | 0xC4 | highlight row n |
| `talk_no_cursor` | 0xC5 | no highlighted row |
| `talk_speaker HOST/PLAYER` | 0xC6 | who speaks: HOST (wide panel) or PLAYER (the current player's frame) |
| `talk_clear` | 0xC8 | clear the text |
| `talk_show 0` | 0xC9 | show the text (operand 0; another value does nothing) |
| `talk "text"` | 0xCA | add a text to the panel (codes: %NMY, %Nnn, %K, %Snn) |
| `talk_number value, width` | 0xCB | add a number (width digits) |
| `talk_player player` | 0xCC | add player n's name |
| `talk_store_type store` | 0xCD | add the store type name of store n |
| `$x = talk_y` | 0xCE | $x = the talk panel's y |
| `talk_data text` | 0xCF | add the text at a label (or an offset in a variable) |
| `nop_D0` | 0xD0 | no effect |
| `nop_D1` | 0xD1 | no effect |
| `nop_F1 a, b, c` | 0xF1 | no effect |
| `nop_F2 a, b, c` | 0xF2 | no effect |
| `portrait_show unused=n, face=n, pos=pos` | 0x100 | show the current player's portrait with a face, unsquashing it |
| `portrait_hide unused` | 0x101 | squash the portrait away |
| `portrait_face face` | 0x102 | change the portrait's face |
| `count_stores who, $x` | 0x140 | $x = how many stores the player(s) own |
| `get_store who, n, $x` | 0x141 | $x = their n-th store (out of range: 0 here, a leftover value in the original) |
| `earn who, amount` | 0x180 | the player(s) earn an amount (negative: pay) |
| `stores_set_goods stores, count=n, level=n` | 0x181 | the stores get count goods slots at a level (0 the most expensive) |
| `nop_182 a, b` | 0x182 | no effect |
| `nop_183 a, b` | 0x183 | no effect |
| `buy_lot unused` | 0x184 | the current player may open a store on a vacant lot along the route |
| `sell_store percent=n, kind=kind` | 0x185 | the current player sells a store for a percentage of its value (SELL_*) |
| `nop_186 a, b` | 0x186 | no effect |
| `nop_187 a, b` | 0x187 | no effect |
| `result_another` | 0x188 | result 1: another event follows |
| `advance_months months` | 0x189 | the calendar moves on n months |
| `move_steps squares` | 0x18A | the current player moves n squares (result 3) |
| `turn_back STOP/REVERSE` | 0x18B | the current player stops (STOP) or turns around (REVERSE) |
| `no_elevator who, months=n` | 0x18C | the player(s) can't take the elevator for n months |
| `stores_set_price stores, percent=n, months=n` | 0x18D | the stores sell at a percentage for n months |
| `money_percent who, percent` | 0x18E | the player(s)' money * percent / 100 |
| `copy_money who, from_player=n` | 0x18F | the player(s)' money = player n's |
| `result_reroll` | 0x190 | result 2: the player rolls again |
| `stores_close_for stores, months=n` | 0x191 | the stores close for n months |
| `stores_close stores, percent=n, only_empty=ON/OFF` | 0x192 | the stores close (only_empty ON: those without goods); the owner gets a percentage |
| `set_shoes who, ON/OFF` | 0x193 | the player(s) put on (ON) or take off the Chirp-Chirp shoes |
| `stores_goods_pricier stores, levels` | 0x194 | the stores' goods n levels more expensive (negative: cheaper) |

The operands: `who` is a player and `stores` a set of stores (the constants below); `buttons` a
set of the script's button bits; `image` a scene image number, or an image group + n; `attr` an
image's flip, transparency and anchor bits; `frames` a number of frames (`FOREVER`: never); `$x`
a variable the statement writes; `label`, `anim`, `path`, `table` and `text` labels of the
script's code and data.

### Constants

| Name | Value | What it is for |
|---|---|---|
| `FOREVER` | -1 | frames: `wait` and `wait_buttons_for` never go on |
| `ALL_PLAYERS`, `CUR_PLAYER`, `OTHER_PLAYERS` | -1, -2, -3 | `who`: every player still in, the current player, every other player still in (or a player number, 0-5) |
| `ALL_STORES`, `CUR_STORES`, `OTHER_STORES` | -1, -2, -3 | `stores`: every player's stores, the current player's, the other players' (or a store number) |
| `STORES_OF_P0`-`STORES_OF_P5` | -4 to -9 | `stores`: player n's stores |
| `P0_STORES_ON_FLOOR0`-`P0_STORES_ON_FLOOR5` | -10 to -15 | `stores_close_for` only: player 0's stores on floor n from the top (no script uses them) |
| `AT_ONCE`, `GROW`, `SLIDE` | 0, 1, 2 | the view's styles (`view_open`, `view_close`) |
| `POS_MIDDLE`, `POS_LEFT`, `POS_RIGHT` | 0, 1, 2 | the portrait's positions (`portrait_show`) |
| `HOST`, `PLAYER` | 0, 1 | `talk_speaker`: the host's wide panel, the current player's frame |
| `SELL_CHOICE` | 0 | `sell_store`: sell one of the stores, or not |
| `SELL_FORCED` | 1 | `sell_store`: sell until the balance is not negative |
| `SELL_CLOSED` | 2 | `sell_store`: a store closed for now, or not |
| `SELL_CLOSED_FORCED` | 3 | `sell_store`: a store closed for now, without "Do not sell" |
| `STOP`, `REVERSE` | 0, 1 | `turn_back` |
| `OFF`, `ON` | 0, 1 | `set_shoes`, `stores_close` |
| `CPU` | 0x0010 | the value of `$cur_is_cpu` / `$is_cpu0`-`$is_cpu5` for a CPU player |
| `NEXT`, `END` | -2, -1 | `frame` and `step` data: go on with the entry that follows, stop |
| `RIGHT`, `L1`, `R2`, `SELECT`, `TRIANGLE`, `CIRCLE`, `CROSS`, `UP`, `DOWN`, `LEFT` | 0x0001, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0400, 0x1000, 0x2000, 0x4000 | the script's button bits (`buttons`, the pad variables): cross and square give the same bit, so do circle and R1; no button gives 0x0100, 0x0200 or 0x0800 |
| `IMG_BOARD`, `IMG_BOARD30`, `IMG_FACE` | 0x1000, 0x2000, 0x3000 | `image` + n: the board's sprites (the player pieces), the same from their image 30, the portrait's faces; a plain number is a scene image |
| `FLIP_X`, `FLIP_Y` | 0x0002, 0x0004 | `attr`: mirrored left to right, top to bottom |
| `SEMITRANS`, `SEMITRANS2` | 0x0008, 0x0400 | `attr`: drawn semi-transparent |
| `OVER` | 0x0100 | `attr`: drawn 32 ordering-table slots further; a sprite with it is hidden while the view is open |
| `DIM` | 0x0200 | `attr`: darkened to the level of `set_dim` |
| `TOP_LEFT`, `TOP_CENTER`, `TOP_RIGHT`, `MIDDLE_LEFT`, `MIDDLE_CENTER`, `MIDDLE_RIGHT`, `BOTTOM_LEFT`, `BOTTOM_CENTER`, `BOTTOM_RIGHT` | 0x0000, 0x0010, 0x0020, 0x0040, 0x0050, 0x0060, 0x0080, 0x0090, 0x00A0 | `attr`: the point of the image that goes at x, y (none: `TOP_LEFT`) |
| `TILED` | 0x0001 | layer `flags`: the image repeated over the view |
| `LINES` | 0x0002 | layer `flags`: drawn band by band, each band shifted (`layer_lines`, `layer_line_steps`, `layer_wave`) |
| `TRANSLUCENT` | 0x0004 | layer `flags`: drawn semi-transparent |
| `BAND2`-`BAND16` | 0x1000 to 0xF000 | layer `flags`: the lines in a band (with `LINES`; none: 1) |
| `CARD_*`, `CORNERS_*`, `FX_*`, `CARD_DONE` | | the event card scene (below) |

### Data

Data the code points at is written with a label:

    anim_1679:
        frame 7, time=5, flip=0, next=NEXT          ; image, frames to show it, FLIP_X/FLIP_Y,
        frame 8, time=5, flip=0, next=anim_1679     ; then NEXT (the following frame), END or a label
    path_0551:
        step speed=-1, x=240, y=119, anim=anim_0575, next=path_055B   ; a path: move (as
                                                    ; sprite_move_to) while playing anim, then next
    table_0090:
        words text_01F8, 3, 12                      ; 16-bit words
    text_01F8:
        string "..."                                ; a text and its terminating NUL
        bytes 0x00, 0x10                            ; bytes that are none of these

Labels are named after what they hold and their offset: `sub_XXXX` (called), `L_XXXX` (jumped
to), `anim_`, `path_`, `table_`, `text_`; `unused_XXXX` starts code nothing reaches (shown as
code anyway). "(nothing names this)" marks data no instruction points at. `frame` and `step` are
4 and 5 words; `words` writes 16-bit words, `bytes` bytes, `string` a text and its NUL.

### How statements are stored

- A statement is big-endian 16-bit words: its opcode, then one word per operand, in the order of
  the table (the keys' order). `switch` is the count of labels, the value, then the labels;
  `talk "text"` and `marker "name"` are the opcode, then the text's Shift-JIS bytes and a NUL.
- An operand word 0x8000-0x80FF names variable 0x00-0xFF; any other word is a constant, read as
  signed. So constants from -32768 to -32513 can't be operands (the assembler refuses them);
  data (`words`, `bytes`) may hold any value.
- Offsets (labels) are signed words too: a script must stay under 0x8000 bytes for its labels to
  work.
- Nothing needs aligning: the interpreter reads every word a byte at a time, so code and data may
  start at any byte (after a text of odd length, say).

### Texts

Texts are Shift-JIS on the disc and written here in UTF-8 between quotes; the assembler converts
them back. `\"` and `\\` stand for `"` and `\`, `\xNN` for a byte that is no character.
Codes in the text: `%NMY` the current player's name (the letter after `M` is not read),
`%Nnn` player nn's name, `%K` the next line, `%Snn` nn spaces. The panel has 3 lines of 38
half-width characters. A text with double-byte characters has a comment: the English for a
Japanese text (`en: ...`; in the talk scripts, which are in English, the Japanese: `ja: ...`),
else the text as it reads in plain ASCII (`%K` as ` / `, names as `<player>`). The game's font (`FONT0.FNT`) draws some symbols as small pictures, so they
read as emoticons in the talk: ≧ a cat's face, ☆ a heart, ∝ an outlined heart, ∈ a star,
★ a sparkle, ♪ and ♭ music notes, ∴ a cup of tea, ≦ two drops, ∵ a puff, ∥ and ｜ a long
dash (a drawn-out sound). The files keep the Shift-JIS characters.

**Where an inline text ends.** The game does not look for the NUL after `talk "text"`: it
measures the text as it prints it (`Script_OpTalkPrint`, `0x80080C90`) and goes on after the
length it counted and one byte for the NUL. A character counts 2 bytes when its first byte is
0x80-0x9F or 0xE0-0xFF, or it is the full-width space (81 40), else 1; `%Nxx` and `%Sxx` count 4,
`%K` 2, another `%` 1. So:

- a code must be whole (`%N` with its two characters, `%S` with its two digits);
- an inline text must fit the panel: past the end of its third row the game drops characters
  without noting their width, and may count a two-byte character as one and go on inside the
  text;
- `marker "name"` is measured in a 14-byte copy: 13 characters at most.

Event 73 shows what a missing NUL does (below). A text shown with `talk_data` is read from where
it is, but its full-width spaces print as one empty space and an `@` (data text counts them as
one byte).

The character talk scripts (`talk/`) and events 2-23, 38, 39, 55-57, 73, 74, 77-79, 89-99
were left in Japanese in this release, and none of them is shown in normal play: the events
are never drawn, and the talk is switched off. Board_Task plays a talk script only on a CPU
player's turn and only while `g_optSkipCpuTalk` is 0 (or under `RULE_ALWAYS_TALK`, which nothing
turns on). The game sets it to 1 at start-up (`Game_InitDefaults`), the options menu's row for
it can't be reached (the cursor wraps at "Exit"), and the save file keeps it as it is. With it
set to 0 (in an emulator), the talk shows, the character's line in the talk panel and the
portrait, and then the turn waits for the voice forever (the voices' positions are wrong). A
build with `CPU_TALK` (`make CPU_TALK=1`) plays the talk, with its voices. (A guess: it was
switched off because it was not translated.)

The talk scripts here are edited. Each text is in English, the English of
`tools/tss_english.json` (where `<player>` stands for `%NMY` and ` / ` for a new row) fitted to
the talk panel, with the Japanese in the comment; and the text stays up 90 frames before the
talk goes on, not 30 (`wait_buttons_for`). The panel gives a talk text 2 rows of 240 pixels below
the speaker's name: a half-width character is 7 pixels wide, a full-width one (an emoticon) 14,
a name up to 12 half-width characters. A text keeps the rows of the Japanese when they fit, else
its words are wrapped again; a percent sign is the full-width ％, since the game reads % as the
start of a code. The talk scripts of characters 14 and 15, whom no one can choose, are left
empty: with the English texts all 32 would not fit in `TALK.J`'s 22 sectors. The disc image the
build makes takes the built `TALK.J`, so the talk is in English.

## The event card scene

Events 2-99 are built on one framework, the card scene: the card's picture (sprite 0, scene
image 7) enters the screen, framed by four corner pieces (sprites 1-4, image 1) that can play
effects, over a background of 30 tiles (sprites 10-39, image 10). The main code of an event
sets the effects and texts and calls the routines below; most of the rest of the file is the
framework, the same in every event. Routines are recognized by their code, so they have the same
names in every file.

| Routine | What it does |
|---|---|
| `card_bounce` | CARD_BOUNCE: the card bounces up and down, less and less, into the center |
| `card_close` | close the view and take everything away (corners_leave); wait until the view and the talk panel are closed |
| `card_enter` | one frame of the card's entrance, by $card_entrance |
| `card_rise` | CARD_RISE: the card grows out of the top of the talk panel, moving right from the center (it ends at x 240, standing on the panel) |
| `card_show` | music 3; open the view and the talk panel; then every frame the corner entrance ($corner_entrance), the effects ($corner_effects) and the card's entrance ($card_entrance), until the voice has ended and cross is pressed (a CPU player: 60 frames); then the corners back home and a sound |
| `card_sink` | the card shrinks into the talk panel, moving back to the center, and is hidden |
| `card_slide_left` | CARD_SLIDE_LEFT: the card slides in from the left, slowing down, to the center (160, 120) |
| `card_slide_right` | CARD_SLIDE_RIGHT: the card slides in from the right |
| `card_spiral` | CARD_SPIRAL: the card spirals in to the center |
| `card_zoom` | CARD_ZOOM: the card appears 16 times its size at the center and shrinks to its size |
| `clear_vars` | variables 0x00-0xA4 = 0 |
| `corners_animate` | start the animations of the card and the four corners |
| `corners_animate_alt` | other animations for the corners (not used) |
| `corners_apply` | corner positions plus this frame's offsets to sprites 1-4, their scale = $corner_stretch_x/y * $corner_zoom / 50; offsets, zoom and stretch back to neutral; wait a frame |
| `corners_fly_in` | corner entrance 2: the corners fly in from the edges of the screen over $corner_steps frames |
| `corners_leave` | the corners draw in for 10 frames, the talk panel closes, then they fly off for 15 frames; meanwhile the card sinks into the talk panel (card_sink) and the tiles shrink |
| `corners_spread` | corner entrance 1: all four start at the center and spread out to their home positions over $corner_steps frames |
| `effects_default` | no corner entrance; only FX_TILES |
| `effects_run` | one frame of the effects that keep going (FX_SPIN, FX_PULSE_X, FX_PULSE_Y, FX_PULSE, FX_TILES), then corners_apply |
| `effects_settle` | (not used) run the effects until the corner entrance, FX_GROW, FX_SHRINK, FX_CIRCLE and the squeezes have run down, then until the spin and the pulses are back at rest |
| `finish` | stop the music; end of the script |
| `fx_circle` | FX_CIRCLE: the corners circle around their places; the radius $circle_radius falls by 1 a frame |
| `fx_grow` | FX_GROW: zoom = 1 - $grow_amount/256; the amount falls by 10 a frame |
| `fx_pulse` | FX_PULSE: the corners throb (size 1 to 1.5, $pulse_angle) |
| `fx_pulse_stop` | stop FX_PULSE at the end of a swing (not used) |
| `fx_pulse_x` | FX_PULSE_X: the corners turn over sideways, around and around (their width swings between -1 and 1, $pulse_x_angle) |
| `fx_pulse_x_stop` | stop FX_PULSE_X at the end of a swing (not used) |
| `fx_pulse_y` | FX_PULSE_Y: the same, turning over top to bottom ($pulse_y_angle) |
| `fx_pulse_y_stop` | stop FX_PULSE_Y at the end of a swing (not used) |
| `fx_shrink` | FX_SHRINK: zoom = 1 + $shrink_amount/256; the amount falls by 10 a frame |
| `fx_spin` | FX_SPIN: the corners turn (1 and 3 one way, 2 and 4 the other) |
| `fx_spin_stop` | stop FX_SPIN once the angle is back at 0 (not used) |
| `fx_squeeze_x` | FX_SQUEEZE_X: the corners start 100 pixels further apart sideways and ease back ($squeeze_x_angle) |
| `fx_squeeze_y` | FX_SQUEEZE_Y: the same up and down ($squeeze_y_angle) |
| `fx_tiles_grow` | FX_TILES: the background tiles grow to full size |
| `fx_tiles_scroll` | FX_TILES: the background tiles drift down and to the left, wrapping around |
| `money_by_years` | $v00 = 100 * ($months / 24 + 1): an amount that grows every two years |
| `scene_setup` | the scene: layer 0 (scene image 0); the card (sprite 0, image 7) off screen; the four corners (sprites 1-4, image 1) at their home positions, worked out from the size of image 1 (the frame fills 320 x 170); the 30 background tiles (sprites 10-39, image 10) in rows of 6; dim 128; $corner_steps = 20, the zoom and squeeze counters set |
| `tiles_shrink` | the background tiles shrink; hidden once gone |
| `wait_cross` | keep the effects going until cross is pressed (a CPU player: 30 frames); a sound |

### The scene's variables

The `var` lines at the head of each event file name these. The corners are sprites 1-4: corner 1
at the top left, 2 at the top right, 3 at the bottom left, 4 at the bottom right; a row with N
covers the four corners, their variables one after the other (corner 1's, then corner 2's...).

| Variable | Index | Holds |
|---|---|---|
| `$cornerN_x`, `$cornerN_y` | `$v0A`-`$v11` | corner N's position |
| `$corner_zoom` | `$v12` | the corners' size this frame, 256 = full size |
| `$corner_stretch_x`, `$corner_stretch_y` | `$v13`, `$v14` | the corners' stretch this frame, 50 = none |
| `$cornerN_dx`, `$cornerN_dy` | `$v15`-`$v1C` | corner N's offset for this frame only |
| `$cornerN_home_x`, `$cornerN_home_y` | `$v1D`-`$v24` | corner N's place in the frame |
| `$corner_entrance` | `$v25` | how the corners come in: `CORNERS_*` (below) |
| `$corner_effects` | `$v26` | the corners' effects: `FX_*` bits (below) |
| `$card_entrance` | `$v28` | how the card comes in: `CARD_*` (below) |
| `$card_state` | `$v8C` | the card's entrance: 0 not started (`card_bounce` and `card_rise` count up from there), `CARD_DONE` (32767) once it is over |
| `$card_angle` | `$v8D` | the angle that drives the card's slides, spiral and bounce |
| `$card_sink_step` | `$v93` | `card_sink`: the frames the card has been sinking (it moves back to the center and shrinks by an eighth each frame) |
| `$card_rise_step` | `$v94` | `card_rise`: the frames the card has been rising |
| `$corner_steps` | `$v9B` | the frames left of the corners' entrance (`corners_spread`, `corners_fly_in`; 20 to start with) |
| `$grow_amount` | `$v9A` | `FX_GROW`: the zoom is 1 - amount / 256; it falls by 10 a frame |
| `$shrink_amount` | `$v99` | `FX_SHRINK`: the zoom is 1 + amount / 256; it falls by 10 a frame |
| `$circle_radius` | `$v9E` | `FX_CIRCLE`: the circle's radius; it falls by 1 a frame |
| `$circle_angle` | `$v9F` | `FX_CIRCLE`: where on the circle the corners are |
| `$squeeze_x_angle`, `$squeeze_y_angle` | `$v9D`, `$v9C` | `FX_SQUEEZE_X`, `FX_SQUEEZE_Y`: how far the corners have eased back |
| `$pulse_angle` | `$v96` | `FX_PULSE`: where in the throb the corners are |
| `$pulse_x_angle`, `$pulse_y_angle` | `$v97`, `$v98` | `FX_PULSE_X`, `FX_PULSE_Y`: where in the turn the corners are |

### The corners' effects and the entrances

`$corner_effects` is a set of bits; each effect runs by itself, so they combine.

| Bit | Name | Effect |
|---|---|---|
| 0x0001 | `FX_GROW` | the corners grow to their size |
| 0x0002 | `FX_SHRINK` | the corners shrink to their size |
| 0x0004 | `FX_CIRCLE` | the corners circle around their places, closer and closer |
| 0x0008 | `FX_SQUEEZE_X` | the corners start 100 pixels further apart sideways and ease back |
| 0x0010 | `FX_SQUEEZE_Y` | the same, up and down |
| 0x0100 | `FX_SPIN` | the corners turn (1 and 3 one way, 2 and 4 the other) |
| 0x0200 | `FX_PULSE_X` | the corners turn over sideways, around and around |
| 0x0400 | `FX_PULSE_Y` | the corners turn over top to bottom, around and around |
| 0x0800 | `FX_PULSE` | the corners throb (size 1 to 1.5) |
| 0x4000 | `FX_TILES` | the background tiles grow to their size and drift down and to the left, wrapping around |

`$card_entrance`, how the card comes in:

| Value | Name | Entrance |
|---|---|---|
| 0 | `CARD_STILL` | none: the card is not moved |
| 1 | `CARD_SLIDE_LEFT` | it slides in from the left, slowing down, to the center (160, 120) |
| 2 | `CARD_SLIDE_RIGHT` | it slides in from the right |
| 3 | `CARD_ZOOM` | it appears 16 times its size at the center and shrinks to its size |
| 4 | `CARD_RISE` | it grows out of the top of the talk panel, moving right from the center (to x 240, standing on the panel) |
| 5 | `CARD_SPIRAL` | it spirals in to the center |
| 6 | `CARD_BOUNCE` | it bounces up and down, less and less, into the center |

`$corner_entrance`, how the corners come in:

| Value | Name | Entrance |
|---|---|---|
| 0 | `CORNERS_HOME` | none: the corners are at their places from the start |
| 1 | `CORNERS_SPREAD` | all four start at the center and spread out to their places over `$corner_steps` frames |
| 2 | `CORNERS_FLY_IN` | they fly in from the edges of the screen over `$corner_steps` frames |

## Things the scripts show about the game

- Opcode 0x194 (`stores_goods_pricier`) is used by event 49 ("Due to an error in ordering, all
  items in <player>'s store have changed to cheaper items."). The original interpreter's jump
  table has it; the decompiled `Script_Run` had lost the case, so event 49 stopped on an
  "unknown code" message in the rebuilt game. Fixed in `src/script.c`; checked in an emulator.
- Event 73 (a quiz) writes `talk ""` without its terminating NUL: the game takes the next
  instruction's first byte for it and goes on at 0x00CE in the middle of an instruction, a word
  that is no opcode (0x2C80), and the event stops there with the interpreter's "unknown code"
  message (seen in the emulator in the original and the rebuilt game). It is never drawn.
  Its answer menu, like event 77's, waits for button bit 0x0200, which no button gives.
- Events 95-99 hold a test version of the framework: the player types the corner effects as
  numbers, confirmed with the same unobtainable bit 0x0200. They are never drawn.
- Events 2-23, 38, 39 and 95-99 announce price changes made with opcodes 0x182 / 0x183, which
  do nothing in this game.
- Only events 24-37, 40-54, 58-72, 75 and 76 are drawn at random, and 80-88 as the events of
  the months; 0 and 1 play on bankruptcy and on losing a story stage. `CAL_13.J` (a copy of
  January) is never loaded.
