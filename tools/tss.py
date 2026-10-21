"""tss.py - the event scripts of Top Shop as readable text, and back.

The game's scripts (run by src/script.c) are bytecode: big-endian 16-bit words, an opcode word
and its operand words; an operand 0x8000-0x80FF names a variable. This tool turns each script
into a .tss text file (labels, named variables and constants, decoded text, the data tables
the scripts read) and assembles .tss files back into bytecode.

    python3 -I tools/tss.py build [DIR] [-o OUT]
                                               assemble every .tss in DIR (default scripts/) and
                                               write the disc files that hold them (EVE_*.J,
                                               CNT*.J, CAL_*.J, TALK.J) into OUT (default
                                               DIR/build); the rest of each file comes from the
                                               disc. Edited scripts may change size; each file must
                                               still fit in the sectors the game reads (a shorter
                                               one is padded to its size on the disc)
    python3 -I tools/tss.py extract [OUTDIR] [--force]
                                               disassemble every script on the disc into OUTDIR
                                               (default scripts/) and check that each assembles
                                               back to the same bytes; a file already in OUTDIR
                                               that holds an edited script (one that assembles to
                                               other bytes than the disc's) is kept, unless
                                               --force
    python3 -I tools/tss.py asm FILE.tss OUT   assemble one file into raw bytecode
    python3 -I tools/tss.py dis FILE.J [ENTRY] disassemble one script to stdout (ENTRY: the
                                               TALK.J entry)

The language is described in scripts/README.md (written by extract with tools/tss_readme.py,
which only the main repository has), the interpreter in docs/script.md. Names and descriptions of
the routines the scripts share, the event scene's variables and the calendar pictures are in
tools/tss_names.py; the English of the Japanese texts in tools/tss_english.json (keyed by the text
as written in the .tss files).
"""
import hashlib
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tss_names  # noqa: E402

DISC = "disc/A"

# ---------------------------------------------------------------------------------------------
# Containers: .J archives and LZSS (src/lzss.c)


def archive_entries(data):
    """A .J archive: u32 count, u32 offsets[count + 1] (the last = the file size)"""
    count = struct.unpack_from("<I", data, 0)[0]
    offsets = struct.unpack_from("<%dI" % (count + 1), data, 4)
    return [data[offsets[i]:offsets[i + 1]] for i in range(count)]


def lzss_decompress(data):
    """Lzss_Decompress: u32 packed size (with that word), flag bytes LSB first (1 a literal,
    0 a copy: b0 | (b1 & 0xF0) << 4 the ring position, (b1 & 15) + 3 bytes); ring of 4096
    zeroed bytes, writing from 0xFEE"""
    total = struct.unpack_from("<I", data, 0)[0]
    ring = bytearray(4096)
    r = 0xFEE
    out = bytearray()
    i = 4
    while i < total:
        flags = data[i]
        i += 1
        for bit in range(8):
            if i >= total:
                break
            if flags & (1 << bit):
                c = data[i]
                i += 1
                out.append(c)
                ring[r] = c
                r = (r + 1) & 0xFFF
            else:
                b0, b1 = data[i], data[i + 1]
                i += 2
                pos = b0 | (b1 & 0xF0) << 4
                for k in range((b1 & 0x0F) + 3):
                    c = ring[(pos + k) & 0xFFF]
                    out.append(c)
                    ring[r] = c
                    r = (r + 1) & 0xFFF
    return bytes(out)


def lzss_compress(data):
    """Bytes Lzss_Decompress turns back into data: greedy matches of 3-18 bytes up to 4095 bytes
    back, found through the 3-byte prefixes seen so far (the original's packer is not known, so
    an edited talk script packs to other bytes than the original's; unchanged ones are copied)"""
    n = len(data)
    body = bytearray()
    seen = {}                     # 3 bytes -> where they occurred, oldest first
    i = 0
    group = []                    # up to 8 items: (literal?, bytes)

    def flush():
        flags = 0
        for k, (literal, _) in enumerate(group):
            if literal:
                flags |= 1 << k
        body.append(flags)
        for _, b in group:
            body.extend(b)
        group.clear()

    def remember(pos):
        if pos + 3 <= n:
            seen.setdefault(bytes(data[pos:pos + 3]), []).append(pos)

    while i < n:
        best, where = 0, 0
        if i + 3 <= n:
            limit = min(18, n - i)
            for j in reversed(seen.get(bytes(data[i:i + 3]), [])[-512:]):
                if i - j > 4095:
                    break
                k = 3
                while k < limit and data[j + k] == data[i + k]:
                    k += 1
                if k > best:
                    best, where = k, j
                    if k == limit:
                        break
        if best >= 3:
            pos = (0xFEE + where) & 0xFFF
            group.append((False, bytes([pos & 0xFF, (pos >> 4) & 0xF0 | (best - 3)])))
            for k in range(best):
                remember(i + k)
            i += best
        else:
            group.append((True, bytes([data[i]])))
            remember(i)
            i += 1
        if len(group) == 8:
            flush()
    if group:
        flush()
    return struct.pack("<I", 4 + len(body)) + bytes(body)


def pad4(data):
    return data + b"\0" * (-len(data) % 4)


def pack_archive(entries):
    """A .J archive of entries, each padded to 4 bytes (as on the disc)"""
    entries = [pad4(e) for e in entries]
    offsets = [4 + 4 * (len(entries) + 1)]
    for e in entries:
        offsets.append(offsets[-1] + len(e))
    return struct.pack("<%dI" % (len(entries) + 2), len(entries), *offsets) + b"".join(entries)


SECTOR = 2048


def sectors(size):
    return (size + SECTOR - 1) // SECTOR


CHARACTERS = ["Meower", "Maiko", "Dona", "Gummi", "Merlin", "Cindy", "Rob", "Patty", "Chat",
              "Oilow", "Bro", "Squosh", "Santa", "Colin"]         # g_cpuNames (0..13)
MONTHS = ["", "January", "February", "March", "April", "May", "June", "July", "August",
          "September", "October", "November", "December"]

# TurnStart_PickScript (src/talk.c) and TurnEnd_PickScript (TURN_*, include/board.h): the entry
# a talk script starts at ($entry)
TURN_START = ["the first month", "assets at 7/8 of the goal", "last at the start of the round, first now",
              "first at the start of the round, last now", "ahead: a rival store it can't afford",
              "ahead: a rival store in a chain of 4", "ahead: a rival store closed, without goods",
              "ahead: its own store without goods", "the bank on the next square",
              "ranked better than at the start of the round", "ranked worse",
              "ahead: a rival store in a chain", "ahead: a vacant lot",
              "ahead: a rival store costing over 1/4 of its money", "ahead: the bank",
              "none of its stores ahead", "ahead: its own store with free goods slots",
              "no direction (after the bank)", "no direction, on an elevator square", "none of these"]
TURN_END = ["(no comment)", "a chain of 4", "a chain of 2 or 3", "landed on a rival store closed for now",
            "landed on a rival store with lowered prices", "landed on its own store / opened a store",
            "did not take the elevator", "landed on the bank", "took the elevator",
            "shopping cost a fifth of its money or more", "shopping: got the money back",
            "shopping", "bought a rival's store", "shopped this turn and the last",
            "assets passed half the goal", "sold a store", "declined a vacant lot it could not afford",
            "declined a vacant lot", "an event card (the default)"]


# Which events the game plays: Event_PickRandom draws from g_eventWeights, or when that gives
# none the month's event (g_monthEvents, month 0 = April); nothing turns on RULE_PICK_EVENT
RANDOM_EVENTS = set(range(24, 38)) | set(range(40, 55)) | set(range(58, 73)) | {75, 76}
MONTH_EVENTS = {80: "January", 81: "June, July and September", 82: "October", 83: "December",
                84: "March", 85: "April and May", 86: "August", 87: "November", 88: "February"}


def event_how(n):
    if n == 0:
        how = ["Played when a player goes bankrupt (Board_Task step 10104): what the player's",
               "character says. (Event 0 itself is never drawn.)"]
    elif n == 1:
        how = ["Played when a story stage is lost (Board_Task step 12004). (Event 1 itself is",
               "never drawn.)"]
    elif n in RANDOM_EVENTS:
        how = ["Event %d: drawn at random after a roll of 8 (Event_PickRandom, g_eventWeights)." % n]
    elif n in MONTH_EVENTS:
        how = ["Event %d: the event of %s (g_monthEvents), when the random draw gives none." % (n, MONTH_EVENTS[n])]
    else:
        how = ["Event %d: never played. Event_PickRandom never draws it, and nothing turns on" % n,
               "RULE_PICK_EVENT, the debug rule that lets the player pick any event."]
    if n >= 2:
        how += ["Result ($result of the task): 0 the turn goes on, 1 another event (result_another),",
                "2 roll again (result_reroll), 3 move the steps set (move_steps)."]
    return how


def talk_row_note(situations):
    """The talk scripts read row ($entry * 3 + random 3) of their table: text, face, XA channel"""
    def note(row, words):
        entry, variant = divmod(row, 3)
        if entry >= len(situations):
            return "row %d" % row
        return "%d.%d %s" % (entry, variant, situations[entry])
    return note


TALK_HOW = ["The code picks row $entry * 3 + (random 3) of the table: the offset of a text, the",
            "portrait's face and the voice's XA channel. It shows the speaker's name and that text in",
            "the talk panel, the portrait (face, at a random position), plays the voice, and waits for",
            "cross (30 frames at most). Table comments: entry.variant situation: what is said."]


def script_sources():
    """(path in the output directory, title, lines of description, bytes, note on the talk table
    rows, disc file, entry in it, LZSS-packed) of every script"""
    out = []
    for n in range(100):
        name = "EVE_%03d.J" % n
        data = archive_entries(open(os.path.join(DISC, name), "rb").read())[0]
        how = event_how(n)
        out.append(("events/eve_%03d.tss" % n, name + " entry 0", how, data, None, name, 0, False))
    for n in range(20):
        name = "CNT%02d.J" % n
        data = archive_entries(open(os.path.join(DISC, name), "rb").read())[0]
        if n < 10:
            how = ["Story mode: the introduction of stage %d (Board_Task, FILE_CNT00_J + stage)." % (n + 1)]
        else:
            how = ["Story mode: stage %d won (Board_Task step 1396, FILE_CNT10_J + stage)." % (n - 9)]
        how += ["<player> is the human player's character, <player 1> the stage's rival (player 1)."]
        out.append(("stages/cnt%02d.tss" % n, name + " entry 0", how, data, None, name, 0, False))
    for n in range(1, 14):
        name = "CAL_%02d.J" % n
        data = archive_entries(open(os.path.join(DISC, name), "rb").read())[0]
        if n <= 12:
            how = ["The calendar page of %s (Board_Task steps 2100-2120, with event scenes on)." % MONTHS[n]]
        else:
            how = ["Not loaded by the game (CalendarFile in src/board.c gives CAL_01-CAL_12 only)."]
        how += ["The picture: " + tss_names.CALENDAR[n] + "."]
        how += ["It fades in over 15 frames, plays for 300 frames or until anyone presses cross, and",
                "fades out."]
        out.append(("calendar/cal_%02d.tss" % n, name + " entry 0", how, data, None, name, 0, False))
    talk = archive_entries(open(os.path.join(DISC, "TALK.J"), "rb").read())
    for n, packed in enumerate(talk):
        char = n % 16
        who = CHARACTERS[char] if char < len(CHARACTERS) else "character %d (no such character)" % char
        if n < 16:
            how = ["What %s says at the start of a turn (Board_Task step 3201): on a CPU player's turn, "
                   "and only while g_optSkipCpuTalk is 0, which the game never sets (scripts/README.md)." % who,
                   "$entry (TurnStart_PickScript):"]
            how += ["  %2d %s" % (i, s) for i, s in enumerate(TURN_START)]
            how += [""] + TALK_HOW
            kind = "start"
        else:
            how = ["What %s says at the end of a turn (Board_Task step 9200): on a CPU player's turn, "
                   "and only while g_optSkipCpuTalk is 0, which the game never sets (scripts/README.md)." % who,
                   "$entry (TurnEnd_PickScript, the TURN_* flag):"]
            how += ["  %2d %s" % (i, s) for i, s in enumerate(TURN_END)]
            how += [""] + TALK_HOW
            kind = "end"
        out.append(("talk/%s_%02d.tss" % (kind, char), "TALK.J entry %d (LZSS-packed)" % n, how,
                    lzss_decompress(packed), talk_row_note(TURN_START if n < 16 else TURN_END),
                    "TALK.J", n, True))
    return out


# ---------------------------------------------------------------------------------------------
# Names

VAR_NAMES = {0xC0: "result", 0xC1: "cur_held", 0xC8: "cur_pressed", 0xCF: "num_players",
             0xD0: "cur_player", 0xD1: "frame", 0xD2: "entry", 0xD3: "cur_is_cpu", 0xDA: "floors",
             0xDB: "months", 0xDC: "start_money", 0xDD: "goal", 0xDE: "pad_held",
             0xDF: "pad_pressed", 0xE0: "any_held", 0xE1: "any_pressed", 0xE2: "char",
             0xE3: "active_players"}
for _p in range(6):
    VAR_NAMES[0xC2 + _p] = "held%d" % _p
    VAR_NAMES[0xC9 + _p] = "pressed%d" % _p
    VAR_NAMES[0xD4 + _p] = "is_cpu%d" % _p
for _i in range(16):
    VAR_NAMES[0xF0 + _i] = "l%d" % _i
VAR_INDEX = {v: k for k, v in VAR_NAMES.items()}


PAD_VARS = set(range(0xC1, 0xCF)) | {0xDE, 0xDF, 0xE0, 0xE1}   # hold the script's button bits


def var_name(index):
    return "$" + VAR_NAMES.get(index, "v%02X" % index)


# Script button bits (Scene_Update / Script_Buttons)
BUTTONS = [("RIGHT", 0x0001), ("L1", 0x0008), ("R2", 0x0010), ("SELECT", 0x0020),
           ("TRIANGLE", 0x0040), ("CIRCLE", 0x0080), ("CROSS", 0x0400), ("UP", 0x1000),
           ("DOWN", 0x2000), ("LEFT", 0x4000)]
IMAGE_GROUPS = [("IMG_BOARD", 0x1000), ("IMG_BOARD30", 0x2000), ("IMG_FACE", 0x3000)]
ALIGN_ROWS = ["TOP", "MIDDLE", "BOTTOM"]
ALIGN_COLS = ["LEFT", "CENTER", "RIGHT"]
ATTR_FLAGS = [("FLIP_X", 0x0002), ("FLIP_Y", 0x0004), ("SEMITRANS", 0x0008), ("OVER", 0x0100),
              ("DIM", 0x0200), ("SEMITRANS2", 0x0400)]
LAYER_FLAGS = [("TILED", 0x0001), ("LINES", 0x0002), ("TRANSLUCENT", 0x0004)]

ENUMS = {
    "frames": {-1: "FOREVER"},
    "who": {-1: "ALL_PLAYERS", -2: "CUR_PLAYER", -3: "OTHER_PLAYERS"},
    "owner": {-1: "ALL_PLAYERS", -2: "CUR_PLAYER", -3: "OTHER_PLAYERS"},
    "stores": dict([(-1, "ALL_STORES"), (-2, "CUR_STORES"), (-3, "OTHER_STORES")] +
                   [(-4 - p, "STORES_OF_P%d" % p) for p in range(6)]),
    "stores_f": dict([(-1, "ALL_STORES"), (-2, "CUR_STORES"), (-3, "OTHER_STORES")] +
                     [(-4 - p, "STORES_OF_P%d" % p) for p in range(6)] +
                     [(-10 - f, "P0_STORES_ON_FLOOR%d" % f) for f in range(6)]),
    "style": {0: "AT_ONCE", 1: "GROW", 2: "SLIDE"},
    "ppos": {0: "POS_MIDDLE", 1: "POS_LEFT", 2: "POS_RIGHT"},
    "speaker": {0: "HOST", 1: "PLAYER"},
    "sell": {0: "SELL_CHOICE", 1: "SELL_FORCED", 2: "SELL_CLOSED", 3: "SELL_CLOSED_FORCED"},
    "turn": {0: "STOP", 1: "REVERSE"},
    "onoff": {0: "OFF", 1: "ON"},
    "card": tss_names.CARD_ENTRANCES,
    "corners": tss_names.CORNER_ENTRANCES,
    "cardstate": {tss_names.CARD_DONE: "CARD_DONE"},
    "cpuflag": {0x10: "CPU"},
}

SYMBOLS = {}
for _kind, _table in ENUMS.items():
    for _v, _n in _table.items():
        if SYMBOLS.get(_n, _v) != _v:
            raise SystemExit("symbol %s defined twice" % _n)
        SYMBOLS[_n] = _v
for _n, _v in BUTTONS + IMAGE_GROUPS + ATTR_FLAGS + LAYER_FLAGS + tss_names.FX_BITS:
    SYMBOLS[_n] = _v
for _r, _rn in enumerate(ALIGN_ROWS):
    for _c, _cn in enumerate(ALIGN_COLS):
        SYMBOLS["%s_%s" % (_rn, _cn)] = (_r * 4 + _c) << 4
for _b in range(2, 17):
    SYMBOLS["BAND%d" % _b] = (_b - 1) << 12
SYMBOLS.update({"NEXT": -2, "END": -1})


def flags_text(value, names, rest_hex=True):
    parts = []
    for n, bit in names:
        if value & bit:
            parts.append(n)
            value &= ~bit
    if value:
        parts.append("0x%X" % value if rest_hex else str(value))
    return "|".join(parts) if parts else "0"


def fmt_const(kind, value):
    """A constant operand of a kind as text (value: signed)"""
    if kind in ENUMS and value in ENUMS[kind]:
        return ENUMS[kind][value]
    if kind == "btn":
        return flags_text(value & 0xFFFF, BUTTONS)
    if kind == "fx":
        return flags_text(value & 0xFFFF, tss_names.FX_BITS)
    if kind == "img" and 0 <= value < 0x8000:
        group = value & 0x7000
        for n, g in IMAGE_GROUPS:
            if g == group:
                return "%s+%d" % (n, value & 0xFFF) if value & 0xFFF else n
        return str(value)
    if kind == "attr" and 0 <= value < 0x8000:
        parts = []
        align = (value >> 4) & 0xF
        if align:
            r, c = divmod(align, 4)
            if r < 3 and c < 3:
                parts.append("%s_%s" % (ALIGN_ROWS[r], ALIGN_COLS[c]))
            else:
                parts.append("0x%X" % (align << 4))
        rest = value & ~0xF0
        if rest:
            parts.append(flags_text(rest, ATTR_FLAGS))
        return "|".join(parts) if parts else "0"
    if kind == "lflags" and 0 <= value < 0x10000:
        parts = []
        band = (value >> 12) & 0xF
        if band:
            parts.append("BAND%d" % (band + 1))
        rest = value & 0x0FFF
        if rest:
            parts.append(flags_text(rest, LAYER_FLAGS))
        return "|".join(parts) if parts else "0"
    if kind == "hex":
        return "0x%X" % (value & 0xFFFF)
    return str(value)


def is_var(word):
    return 0x8000 <= word <= 0x80FF


def s16(word):
    return word - 0x10000 if word & 0x8000 else word


# ---------------------------------------------------------------------------------------------
# The opcodes. Operand kinds: n number, v variable written, L code label, anim / path / table /
# text offsets of data in the script, and the named kinds of fmt_const. The form says how a
# statement is written; "{0}" is operand 0. Statements without a form are "name a, b, c" (with
# key=value for the names in keys).

class Op:
    def __init__(self, code, name, kinds=(), form=None, keys=None, ends=False, note=None):
        self.code, self.name, self.kinds = code, name, list(kinds)
        self.form, self.keys, self.ends, self.note = form, keys, ends, note
        self.size = 2 + 2 * len(self.kinds)


OPS = [
    Op(0x00, "end", ends=True),
    Op(0x01, "wait", ["frames"]),
    Op(0x02, "wait_buttons", ["btn"]),
    Op(0x03, "wait_buttons_for", ["btn", "frames"]),
    Op(0x10, "goto", ["L"], ends=True),
    Op(0x12, "switch"),                                     # count, index, targets[count]
    Op(0x13, "call", ["L"]),
    Op(0x14, "return", ends=True),
    Op(0x15, "halt", ends=True),
    Op(0x20, "if", ["L", "n", "n", "n"]),                   # target, a, b, test
    Op(0x21, "set", ["v", "n"], "{0} = {1}"),
    Op(0x23, "add", ["v", "n"], "{0} += {1}"),
    Op(0x24, "sub", ["v", "n"], "{0} -= {1}"),
    Op(0x25, "mul", ["v", "n"], "{0} *= {1}"),
    Op(0x26, "div", ["v", "n"], "{0} /= {1}"),
    Op(0x27, "mod", ["v", "n"], "{0} %= {1}"),
    Op(0x28, "and", ["v", "n"], "{0} &= {1}"),
    Op(0x29, "or", ["v", "n"], "{0} |= {1}"),
    Op(0x2A, "xor", ["v", "n"], "{0} ^= {1}"),
    Op(0x2B, "negate", ["v"]),
    Op(0x2C, "random", ["v", "n"], "{0} = random {1}"),
    Op(0x2D, "get_indexed", ["v", "table", "n"], "{0} = {1}[{2}]"),
    Op(0x2E, "set_indexed", ["v", "n", "n"], "{0}[{1}] = {2}"),
    Op(0x30, "play_bgm", ["n"]),
    Op(0x31, "stop_bgm"),
    Op(0x32, "play_se", ["n"]),
    Op(0x33, "nop_33"),
    Op(0x34, "nop_34", ["n"]),
    Op(0x35, "nop_35"),
    Op(0x36, "play_xa", ["n", "n"]),
    Op(0x37, "stop_xa"),
    Op(0x38, "yield"),
    Op(0x39, "is_xa_playing", ["v"], "{0} = xa_playing"),
    Op(0x40, "fade_in", ["n"]),
    Op(0x41, "fade_out", ["n"]),
    Op(0x42, "fade_from_white", ["n"]),
    Op(0x43, "fade_to_white", ["n"]),
    Op(0x44, "fade_level_for", ["n", "n", "n", "n"], keys=["r", "g", "b", "frames"]),
    Op(0x45, "wait_fade"),
    Op(0x60, "marker"),                                     # inline NUL-terminated text
    Op(0x61, "view_set", ["n", "n", "n", "n"], keys=["x", "y", "w", "h"]),
    Op(0x62, "view_open", ["style", "n"]),
    Op(0x63, "view_close", ["style", "n"]),
    Op(0x64, "view_wait"),
    Op(0x70, "layer_init", ["n", "n", "n", "lflags"], keys=[None, "image", "pri", "flags"]),
    Op(0x71, "layer_pos", ["n", "n", "n"]),
    Op(0x72, "layer_scroll", ["n", "n", "n"]),
    Op(0x73, "layer_lines", ["n", "n", "n", "n"], keys=[None, "first", "count", "x"]),
    Op(0x74, "layer_line_steps", ["n", "n", "n", "n"], keys=[None, "first", "count", "step"]),
    Op(0x75, "layer_wave", ["n", "n", "n", "n"], keys=[None, "length", "amp", "phase"]),
    Op(0x76, "layer_wave_scroll", ["n", "n"]),
    Op(0x80, "sprite_init", ["n", "img", "attr", "n", "n", "n"],
       keys=[None, "image", "attr", "x", "y", "pri"]),
    Op(0x81, "sprite_hide", ["n"]),
    Op(0x82, "sprite_image", ["n", "img", "attr"], keys=[None, "image", "attr"]),
    Op(0x83, "sprite_attr", ["n", "attr"]),
    Op(0x84, "sprite_move_by", ["n", "n", "n"]),
    Op(0x85, "sprite_pos", ["n", "n", "n"]),
    Op(0x86, "sprite_move_to", ["n", "n", "n", "n"], keys=[None, "x", "y", "speed"]),
    Op(0x87, "sprite_scale_by", ["n", "n", "n"]),
    Op(0x88, "sprite_scale", ["n", "n", "n"]),
    Op(0x89, "nop_89", ["n", "n", "n"]),
    Op(0x8D, "sprite_rotate", ["n", "n"]),
    Op(0x8E, "sprite_angle", ["n", "n"]),
    Op(0x8F, "nop_8F", ["n", "n"]),
    Op(0x90, "sprite_animate", ["n", "anim"]),
    Op(0x91, "sprite_path", ["n", "path"]),
    Op(0x92, "sprite_get_state", ["n", "v"]),
    Op(0x93, "sprite_get_pos", ["n", "v", "v"]),
    Op(0x94, "sprite_get_scale", ["n", "v", "v"]),
    Op(0x95, "sprite_get_angle", ["n", "v"]),
    Op(0x96, "get_image_size", ["img", "v", "v"]),
    Op(0x97, "set_dim", ["n"]),
    Op(0xA0, "rect_set", ["n", "n", "n", "n", "n", "n"], keys=[None, "x", "y", "w", "h", "value"]),
    Op(0xA1, "rect_hide", ["n"]),
    Op(0xA2, "rect_to_view", ["n", "n"]),
    Op(0xB0, "step_toward", ["n", "n", "n", "n", "v", "v"],
       keys=["x", "y", "distance", "angle", "to_x", "to_y"]),
    Op(0xC0, "talk_open"),
    Op(0xC1, "talk_close"),
    Op(0xC2, "talk_wait_open"),
    Op(0xC3, "talk_wait_closed"),
    Op(0xC4, "talk_cursor", ["n"]),
    Op(0xC5, "talk_no_cursor"),
    Op(0xC6, "talk_speaker", ["speaker"]),
    Op(0xC8, "talk_clear"),
    Op(0xC9, "talk_show", ["n"]),
    Op(0xCA, "talk"),                                       # inline text
    Op(0xCB, "talk_number", ["n", "n"]),
    Op(0xCC, "talk_player", ["n"]),
    Op(0xCD, "talk_store_type", ["n"]),
    Op(0xCE, "get_talk_y", ["v"], "{0} = talk_y"),
    Op(0xCF, "talk_data", ["text"]),
    Op(0xD0, "nop_D0"),
    Op(0xD1, "nop_D1"),
    Op(0xF1, "nop_F1", ["n", "n", "n"]),
    Op(0xF2, "nop_F2", ["n", "n", "n"]),
    Op(0x100, "portrait_show", ["n", "n", "ppos"], keys=["unused", "face", "pos"]),
    Op(0x101, "portrait_hide", ["n"]),
    Op(0x102, "portrait_face", ["n"]),
    Op(0x140, "count_stores", ["owner", "v"]),
    Op(0x141, "get_store", ["owner", "n", "v"]),
    Op(0x180, "earn", ["who", "n"]),
    Op(0x181, "stores_set_goods", ["stores", "n", "n"], keys=[None, "count", "level"]),
    Op(0x182, "nop_182", ["n", "n"]),
    Op(0x183, "nop_183", ["n", "n"]),
    Op(0x184, "buy_lot", ["n"]),
    Op(0x185, "sell_store", ["n", "sell"], keys=["percent", "kind"]),
    Op(0x186, "nop_186", ["n", "n"]),
    Op(0x187, "nop_187", ["n", "n"]),
    Op(0x188, "result_another"),
    Op(0x189, "advance_months", ["n"]),
    Op(0x18A, "move_steps", ["n"]),
    Op(0x18B, "turn_back", ["turn"]),
    Op(0x18C, "no_elevator", ["who", "n"], keys=[None, "months"]),
    Op(0x18D, "stores_set_price", ["stores", "n", "n"], keys=[None, "percent", "months"]),
    Op(0x18E, "money_percent", ["who", "n"]),
    Op(0x18F, "copy_money", ["who", "n"], keys=[None, "from_player"]),
    Op(0x190, "result_reroll"),
    Op(0x191, "stores_close_for", ["stores_f", "n"], keys=[None, "months"]),
    Op(0x192, "stores_close", ["stores", "n", "onoff"], keys=[None, "percent", "only_empty"]),
    Op(0x193, "set_shoes", ["who", "onoff"]),
    Op(0x194, "stores_goods_pricier", ["stores", "n"]),
]
BY_CODE = {op.code: op for op in OPS}
BY_NAME = {op.name: op for op in OPS}

TESTS = ["==", "!=", ">", "<", ">=", "<=", "&", "!&"]

# ---------------------------------------------------------------------------------------------
# Text: Shift-JIS with escapes for what does not decode


def sjis_text(raw):
    """bytes -> the text between quotes: cp932 characters that encode back to the same bytes,
    \\xNN for anything else, \\" and \\\\ escaped"""
    out = []
    i = 0
    while i < len(raw):
        c = raw[i]
        if 0x20 <= c < 0x7F:
            ch = chr(c)
            out.append("\\" + ch if ch in "\"\\" else ch)
            i += 1
            continue
        if (0x81 <= c <= 0x9F or 0xE0 <= c <= 0xFC) and i + 1 < len(raw):
            pair = raw[i:i + 2]
            try:
                ch = pair.decode("cp932")
                if ch.encode("cp932") == pair and (ch.isprintable() or ch == "\u3000"):
                    out.append(ch)
                    i += 2
                    continue
            except UnicodeDecodeError:
                pass
        out.append("\\x%02X" % c)
        i += 1
    return '"' + "".join(out) + '"'


def parse_text(s, line):
    """the inside of a quoted string -> bytes"""
    out = bytearray()
    i = 0
    while i < len(s):
        ch = s[i]
        if ch == "\\":
            if s[i + 1:i + 2] == "x":
                out.append(int(s[i + 2:i + 4], 16))
                i += 4
                continue
            out += s[i + 1].encode("cp932")
            i += 2
            continue
        out += ch.encode("cp932")
        i += 1
    return bytes(out)


def talk_scan(data, start, inline):
    """Talk_Print's walk over a text (src/scene.c): the offset of the NUL it stops at. A
    character takes two bytes when its lead byte's high nibble is 8, 9, E or F (inline text:
    the full-width space 81 40 too); %N and %S codes take 4 bytes, %K 2, another % 1."""
    i = start
    while i < len(data) and data[i] != 0:
        if data[i] == 0x25:
            nxt = data[i + 1] if i + 1 < len(data) else 0
            i += 4 if nxt in (0x4E, 0x53) else 2 if nxt == 0x4B else 1
            continue
        lead = (data[i] & 0xF0) in (0x80, 0x90, 0xE0, 0xF0)
        space = data[i] == 0x81 and i + 1 < len(data) and data[i + 1] == 0x40
        i += 2 if lead and (inline or not space) else 1
    return i


def looks_like_text(raw):
    """Bytes that read as text: printable ASCII and Shift-JIS characters, two at least"""
    if len(raw) < 2:
        return False
    i = 0
    while i < len(raw):
        c = raw[i]
        if 0x20 <= c < 0x7F:
            i += 1
        elif (0x81 <= c <= 0x9F or 0xE0 <= c <= 0xEF) and i + 1 < len(raw) and 0x40 <= raw[i + 1] <= 0xFC:
            i += 2
        else:
            return False
    return True




# ---------------------------------------------------------------------------------------------
# Decoding instructions


class Insn:
    def __init__(self, at, op, words, size, text=None, broken=False):
        self.at, self.op, self.words, self.size, self.text = at, op, words, size, text
        self.broken = broken        # 0xCA without its NUL (see Disassembly.walk)
        self.dead = False           # nothing reaches it


def word_at(data, at):
    if at < 0 or at + 2 > len(data):
        raise IndexError
    return data[at] << 8 | data[at + 1]


def decode(data, at):
    """The instruction at offset at, or None"""
    try:
        op = BY_CODE.get(word_at(data, at))
        if op is None:
            return None
        if op.code == 0x12:
            count = word_at(data, at + 2)
            if is_var(count) or count > 256:
                return None
            words = [word_at(data, at + 2 + 2 * k) for k in range(count + 2)]
            return Insn(at, op, words, 6 + 2 * count)
        if op.code == 0x60:
            end = data.find(b"\0", at + 2)
            if end < 0 or end - (at + 2) > 13:          # the original measures a 14-byte copy
                return None
            return Insn(at, op, [], end - at + 1, data[at + 2:end])
        if op.code == 0xCA:
            end = talk_scan(data, at + 2, True)
            if end >= len(data):
                return None
            return Insn(at, op, [], end - at + 1, data[at + 2:end])
        words = [word_at(data, at + 2 + 2 * k) for k in range(len(op.kinds))]
        return Insn(at, op, words, op.size)
    except IndexError:
        return None


def jump_targets(insn):
    """Constant code offsets an instruction may continue at besides the next one"""
    op, w = insn.op, insn.words
    if op.code in (0x10, 0x13, 0x20):
        return [] if is_var(w[0]) else [s16(w[0])]
    if op.code == 0x12:
        return [s16(t) for t in w[2:] if not is_var(t)]
    return []


DATA_KINDS = ("anim", "path", "table", "text")


# ---------------------------------------------------------------------------------------------
# Disassembly: which bytes are code, which are data


class Disassembly:
    def __init__(self, data):
        self.data = data
        self.n = len(data)
        self.owner = [None] * self.n        # byte -> the item (offset) that holds it
        self.items = {}                     # offset -> ("insn", Insn) / ("frame", words) / ...
        self.code_refs = {}                 # offset -> "call" / "jump"
        self.data_refs = {}                 # offset -> kind
        self.problems = []
        self.dead_starts = set()            # where code nothing reaches begins
        self.unreferenced = set()           # data nothing names (found by its shape)
        self.odd_refs = set()               # addresses of data that hold something else
        self.walk([0], dead=False)
        self.claim_data()
        self.recover_dead_code()
        self.recover_paths()
        self.claim_rest()
        self.check_refs()

    # -- items
    def free(self, at, size):
        return 0 <= at and at + size <= self.n and all(self.owner[k] is None for k in range(at, at + size))

    def put(self, at, size, item):
        for k in range(at, at + size):
            self.owner[k] = at
        self.items[at] = item

    # -- code
    def walk(self, starts, dead):
        work = list(starts)
        while work:
            at = work.pop()
            while 0 <= at < self.n:
                if at in self.items:
                    break
                insn = decode(self.data, at)
                if insn is not None and insn.op.code == 0xCA and not insn.text and \
                        decode(self.data, at + 3) is None and decode(self.data, at + 2) is not None:
                    # 0xCA written without its NUL: the game takes the next instruction's first
                    # byte for the NUL and goes on in the middle of that instruction; the script
                    # is shown as written (the instruction from at + 2)
                    insn = Insn(at, insn.op, [], 2, b"", broken=True)
                    self.problems.append("0x%04X: talk without its NUL: the game continues at "
                                         "0x%04X, inside the next instruction" % (at, at + 3))
                if insn is None or not self.free(at, insn.size):
                    if not dead:
                        self.problems.append("0x%04X: %s" % (at, "no instruction" if insn is None
                                                             else "overlaps another item"))
                    break
                insn.dead = dead
                self.put(at, insn.size, ("insn", insn))
                for t in jump_targets(insn):
                    self.code_refs.setdefault(t, "call" if insn.op.code == 0x13 else "jump")
                    work.append(t)
                for kind, word in zip(insn.op.kinds, insn.words):
                    if kind in DATA_KINDS and not is_var(word):
                        self.data_refs.setdefault(s16(word), kind)
                if insn.op.ends:
                    break
                at += insn.size

    def recover_dead_code(self):
        """Code nothing reaches: from the start of each gap after code, instructions up to one that ends
        (end, goto, return, halt) or up to code already found, all of them inside the gap"""
        changed = True
        while changed:
            changed = False
            for start, end in self.gaps():
                if start > 0 and self.items[self.owner[start - 1]][0] != "insn":
                    continue                        # after data: data too
                at = start
                ok = False
                while at < end:
                    insn = decode(self.data, at)
                    if insn is None or at + insn.size > end:
                        break
                    at += insn.size
                    if insn.op.ends or at == end and (at == self.n or self.owner[at] is not None
                                                      and self.items[self.owner[at]][0] == "insn"):
                        ok = True
                        break
                if ok and any(self.data[start:at]):         # not zero padding
                    self.dead_starts.add(start)
                    self.walk([start], dead=True)
                    self.claim_data()
                    changed = True
                    break

    def is_item(self, at, kind):
        return at in self.items and self.items[at][0] == kind

    def frame_ok(self, pos, depth=0):
        """8 bytes at pos read as an animation frame (its next one too)"""
        if not self.free(pos, 8):
            return False
        image, time, flip, nxt = [s16(word_at(self.data, pos + 2 * k)) for k in range(4)]
        if not (0 <= image < 0x4000 and 0 <= time <= 255 and flip in (0, 2, 4, 6)):
            return False
        if nxt in (-2, -1) or nxt == pos or self.is_item(nxt, "frame"):
            return True
        return depth < 4 and 0 <= nxt < self.n and self.frame_ok(nxt, depth + 1)

    def step_ok(self, pos, depth=0):
        """10 bytes at pos read as a path step: its animation a frame, its next a step"""
        if not self.free(pos, 10):
            return False
        speed, x, y, anim, nxt = [s16(word_at(self.data, pos + 2 * k)) for k in range(5)]
        if not (-1 <= speed <= 64 and 0 < anim < self.n and (self.is_item(anim, "frame") or self.frame_ok(anim))):
            return False
        if nxt in (-2, -1) or nxt == pos or self.is_item(nxt, "step"):
            return True
        return depth < 4 and 0 <= nxt < self.n and self.step_ok(nxt, depth + 1)

    def chain(self, pos, size, ok):
        """Entries of size bytes from pos while ok, up to one whose last word is not -2 (NEXT);
        their count, or 0 if the chain breaks off"""
        count = 0
        while ok(pos + count * size):
            count += 1
            if s16(word_at(self.data, pos + count * size - 2)) != -2:
                return count
        return 0

    def text_table_at(self, pos, end):
        """Two or more words from pos that each point at a text"""
        count = 0
        while pos + 2 * count + 2 <= end:
            w = word_at(self.data, pos + 2 * count)
            if not (self.is_item(w, "string") or self.text_at(w) and
                    (self.data[w - 1] == 0 or w >= pos + 2 * count + 2)):
                break
            count += 1
        return count if count >= 2 else 0

    def recover_paths(self):
        """Tables of text offsets, paths and animations nothing names, by their shape"""
        changed = True
        while changed:
            changed = False
            for start, end in self.gaps():
                for pos in range(start, end - 3):
                    count = self.text_table_at(pos, end)
                    if count:
                        self.data_refs.setdefault(pos, "table")
                        self.unreferenced.add(pos)
                        self.claim_table(pos)
                        changed = True
                        break
                    for kind, size, ok in (("path", 10, self.step_ok), ("anim", 8, self.frame_ok)):
                        if pos + size <= end and self.chain(pos, size, ok):
                            self.data_refs.setdefault(pos, kind)
                            self.unreferenced.add(pos)
                            self.claim_data()
                            changed = True
                            break
                    if changed:
                        break
                if changed:
                    break

    def gaps(self):
        out = []
        i = 0
        while i < self.n:
            if self.owner[i] is not None:
                i += 1
                continue
            j = i
            while j < self.n and self.owner[j] is None:
                j += 1
            out.append((i, j))
            i = j
        return out

    def check_refs(self):
        """Notes on addresses of data that hold something else (the printer names them anyway, so
        that they move with the code)"""
        kinds = {"anim": "frame", "path": "step", "text": "string", "table": "words"}
        what = {"insn": "an instruction", "frame": "an animation frame", "step": "a path step",
                "string": "a text", "words": "words", "bytes": "bytes"}
        for at in sorted(self.items):
            kind, item = self.items[at]
            refs = []
            if kind == "frame":
                refs = [("frame's next", item[3], "anim")]
            elif kind == "step":
                refs = [("step's animation", item[3], "anim"), ("step's next", item[4], "path")]
            for name, target, want in refs:
                if target >= 0 and target in self.items and self.items[target][0] != kinds[want]:
                    self.odd_refs.add(target)
                    self.problems.append("0x%04X: the %s, 0x%04X, is %s, not %s: what got there would "
                                         "read it as one" % (at, name, target, what[self.items[target][0]],
                                                             what[kinds[want]]))

    # -- data
    def claim_data(self):
        data = self.data
        work = sorted(self.data_refs.items())
        done = set()
        while work:
            at, kind = work.pop(0)
            if (at, kind) in done or at in self.items:
                continue
            done.add((at, kind))
            if kind == "anim":
                pos = at
                while self.free(pos, 8):
                    frame = [s16(word_at(data, pos + 2 * k)) for k in range(4)]
                    self.put(pos, 8, ("frame", frame))
                    if frame[3] >= 0:
                        self.data_refs.setdefault(frame[3], "anim")
                        work.append((frame[3], "anim"))
                    if frame[3] != -2:
                        break
                    pos += 8
            elif kind == "path":
                pos = at
                while self.free(pos, 10):
                    step = [s16(word_at(data, pos + 2 * k)) for k in range(5)]
                    self.put(pos, 10, ("step", step))
                    if step[3] > 0:
                        self.data_refs.setdefault(step[3], "anim")
                        work.append((step[3], "anim"))
                    if step[4] >= 0:
                        self.data_refs.setdefault(step[4], "path")
                        work.append((step[4], "path"))
                    if step[4] != -2:
                        break
                    pos += 10
            elif kind == "text":
                self.claim_string(at)
            elif kind == "table":
                self.claim_table(at)

    def claim_string(self, at):
        if not self.free(at, 1):
            return False
        end = talk_scan(self.data, at, False)
        if end >= self.n or not self.free(at, end - at + 1):
            return False
        self.put(at, end - at + 1, ("string", self.data[at:end]))
        return True

    def text_at(self, at):
        """A NUL-terminated text starts at at (after a NUL or other data)"""
        if not (0 < at < self.n) or self.owner[at] is not None:
            return False
        end = self.data.find(b"\0", at)
        return end > at and looks_like_text(self.data[at:end])

    def claim_table(self, at):
        """Words up to the next item. Words that point at a text after the table make the texts
        items too, and the table ends at the first of them (the talk scripts' tables of text
        offsets are followed by the texts)"""
        stop = at
        while stop < self.n and self.owner[stop] is None:
            stop += 1
        words = [word_at(self.data, p) for p in range(at, stop - 1, 2)]
        texts = sorted({w for w in words if not is_var(w) and at < w < self.n and self.text_at(w)})
        end = min([stop] + texts)
        words = words[:(end - at) // 2]
        if not words:
            return
        self.put(at, 2 * len(words), ("words", words))
        for t in sorted({w for w in words if w in texts}):
            if self.claim_string(t):
                self.data_refs.setdefault(t, "text")

    def claim_rest(self):
        """Whatever is left: texts, words, bytes"""
        for start, end in self.gaps():
            pos = start
            while pos < end:
                nul = self.data.find(b"\0", pos, end)
                if nul > pos and looks_like_text(self.data[pos:nul]):
                    self.put(pos, nul - pos + 1, ("string", self.data[pos:nul]))
                    pos = nul + 1
                    continue
                # words up to the next text (or the end)
                nxt = pos + 1
                while nxt < end:
                    nul = self.data.find(b"\0", nxt, end)
                    if nul > nxt and self.data[nxt - 1] == 0 and looks_like_text(self.data[nxt:nul]):
                        break
                    nxt += 1
                if (nxt - pos) % 2 == 0 and pos % 2 == 0:
                    self.put(pos, nxt - pos, ("words", [word_at(self.data, k) for k in range(pos, nxt, 2)]))
                else:
                    self.put(pos, nxt - pos, ("bytes", list(self.data[pos:nxt])))
                pos = nxt


# ---------------------------------------------------------------------------------------------
# Printing


def routine_sig(dis, start):
    """The code of a routine (up to its first end / return / halt in a straight line) with its
    label and data operands left out: the same routine in another script has the same one"""
    out = []
    at = start
    while at in dis.items and dis.items[at][0] == "insn" and len(out) < 400:
        i = dis.items[at][1]
        ops = ["L" if k in ("L",) + DATA_KINDS and not is_var(w) else w for k, w in zip(i.op.kinds, i.words)]
        if i.op.code == 0x12:
            ops = [i.words[1], len(i.words)]
        out.append((i.op.code, tuple(ops), i.text))
        if i.op.code in (0x00, 0x14, 0x15):
            break
        at += i.size
    return hashlib.sha1(repr(out).encode()).hexdigest()


_REF_SIGS = None


def reference_routines():
    """signature -> (name, description) of the routines in tss_names.ROUTINES"""
    global _REF_SIGS
    if _REF_SIGS is None:
        _REF_SIGS = {}
        files = {}
        for ref in tss_names.ROUTINES:
            raw = open(os.path.join(DISC, ref + ".J"), "rb").read()
            files[ref] = Disassembly(archive_entries(raw)[0])
        for ref, table in tss_names.ROUTINES.items():
            for off, named in table.items():
                if off not in files[ref].items:
                    raise SystemExit("tss_names: no routine at %s 0x%04X" % (ref, off))
                _REF_SIGS.setdefault(routine_sig(files[ref], off), named)
    return _REF_SIGS


def pattern_name(dis, at):
    """(name, description) of a small helper recognized by what it does, or None"""
    body = []
    while at in dis.items and dis.items[at][0] == "insn" and len(body) < 6:
        insn = dis.items[at][1]
        body.append(insn)
        if insn.op.ends:
            break
        at += insn.size
    names = [i.op.name for i in body]
    const = lambda i, k: None if is_var(body[i].words[k]) else s16(body[i].words[k])   # noqa: E731
    if names == ["talk_show", "talk_open", "return"] and const(0, 0) == 0:
        return "show_and_open", "show the text and slide the talk panel in"
    if names == ["talk_show", "return"] and const(0, 0) == 0:
        return "show_text", "show the text"
    if names == ["wait_buttons_for", "talk_clear", "return"]:
        return "wait_then_clear", "wait for cross (%s frames at most), then clear the text" % const(0, 1)
    if names == ["wait_buttons_for", "fade_out", "wait_fade", "stop_bgm", "end"]:
        return "wait_then_finish", ("wait for cross (%s frames at most), fade out, stop the music; "
                                    "end of the script" % const(0, 1))
    if names == ["talk_clear", "play_bgm", "fade_in", "return"]:
        return "start_scene", "clear the text, music %s, fade in" % const(1, 0)
    if names == ["talk_show", "wait_buttons_for", "talk_clear", "return"] and const(0, 0) == 0:
        return "show_wait_clear", ("show the text, wait for cross (%s frames at most), clear the text"
                                   % const(1, 1))
    if names == ["stop_bgm", "end"]:
        return "finish", "stop the music; end of the script"
    if names == ["end"]:
        return "stop", "end of the script"
    if names == ["return"]:
        return "empty", "returns at once"
    return None


# ---------------------------------------------------------------------------------------------
# Comments: what the game-effect opcodes do, what the texts say

WHO = {-1: "every player still in", -2: "the current player", -3: "every other player still in"}
STORES = {-1: "every player's stores", -2: "the current player's stores",
          -3: "the other players' stores"}
NO_EFFECT = {0x33, 0x34, 0x35, 0x89, 0x8F, 0xD0, 0xD1, 0xF1, 0xF2, 0x182, 0x183, 0x186, 0x187}


def who_text(word, pr):
    if is_var(word):
        return "the player(s) chosen by %s" % pr.var(word & 0xFF)
    v = s16(word)
    return WHO.get(v, "player %d" % v)


def stores_text(word, pr, floors=False):
    if is_var(word):
        return "the stores chosen by %s" % pr.var(word & 0xFF)
    v = s16(word)
    if v in STORES:
        return STORES[v]
    if -9 <= v <= -4:
        return "player %d's stores" % (-4 - v)
    if floors and -15 <= v <= -10:
        return "player 0's stores on floor %d from the top" % (-10 - v)
    return "store %d" % v


def num_text(word, pr):
    return pr.var(word & 0xFF) if is_var(word) else str(s16(word))


def months_text(word, pr):
    return num_text(word, pr) + (" month" if not is_var(word) and s16(word) == 1 else " months")


def explain(insn, pr):
    """A comment for an opcode whose name does not say it all, or None"""
    code, w = insn.op.code, insn.words
    n = lambda i: num_text(w[i], pr)              # noqa: E731
    if code in NO_EFFECT:
        return "no effect in this game"
    if code == 0x180:
        return "%s: money + %s" % (who_text(w[0], pr), n(1))
    if code == 0x181:
        return ("%s: %s goods slots at level %s (0 = the most expensive)"
                % (stores_text(w[0], pr), n(1), n(2)))
    if code == 0x18D:
        return "%s sell at %s%% for %s" % (stores_text(w[0], pr), n(1), months_text(w[2], pr))
    if code == 0x191:
        return "%s closed for %s" % (stores_text(w[0], pr, True), months_text(w[1], pr))
    if code == 0x192:
        only = "" if not is_var(w[2]) and s16(w[2]) == 0 else " (those without goods)"
        return "%s%s close; the owner gets %s%% of the value" % (stores_text(w[0], pr), only, n(1))
    if code == 0x194:
        if not is_var(w[1]) and s16(w[1]) < 0:
            k = -s16(w[1])
        return "%s: goods %d level%s cheaper" % (stores_text(w[0], pr), k, "" if k == 1 else "s")
        return "%s: goods %s levels more expensive" % (stores_text(w[0], pr), n(1))
    if code == 0x18E:
        return "%s: money * %s%%" % (who_text(w[0], pr), n(1))
    if code == 0x18F:
        return "%s: money = player %s's money" % (who_text(w[0], pr), n(1))
    if code == 0x18C:
        return "%s can't take the elevator for %s" % (who_text(w[0], pr), months_text(w[1], pr))
    if code == 0x193:
        on = not is_var(w[1]) and s16(w[1]) != 0
        return "%s %s the Chirp-Chirp shoes" % (who_text(w[0], pr), "put on" if on else "take off")
    if code == 0x18B:
        return ("the current player stops (no direction)" if not is_var(w[0]) and s16(w[0]) == 0
                else "the current player turns around")
    if code == 0x18A:
        return "the current player moves %s squares (result 3)" % n(0)
    if code == 0x189:
        return "the calendar moves on %s" % months_text(w[0], pr)
    if code == 0x188:
        return "result 1: another event follows"
    if code == 0x190:
        return "result 2: the player rolls again"
    if code == 0x184:
        return "the current player may open a store on a vacant lot along the route (a dialog)"
    if code == 0x185:
        kind = {0: "or not", 1: "until the money is not negative", 2: "a store closed for now, or not",
                3: "a store closed for now"}.get(None if is_var(w[1]) else s16(w[1]), "")
        return "the current player sells a store for %s%% of its value (%s)" % (n(0), kind)
    if code == 0x140:
        return "%s = how many stores %s has" % (pr.var(w[1] & 0xFF) if is_var(w[1]) else "$result",
                                                  who_text(w[0], pr))
    if code == 0x141:
        return "%s = store number %s of %s" % (pr.var(w[2] & 0xFF) if is_var(w[2]) else "$result",
                                               n(1), who_text(w[0], pr))
    if code == 0x36:
        if not is_var(w[0]) and not is_var(w[1]):
            return "voice: XA track %d (g_xaSets[1])" % (s16(w[0]) * 16 + s16(w[1]))
        return "voice: XA track %s * 16 + %s (g_xaSets[1])" % (n(0), n(1))
    if code == 0x03 and not is_var(w[1]):
        if s16(w[1]) == -1:
            return "stays here for good"
        buttons = "cross" if not is_var(w[0]) and s16(w[0]) & 0x7FFF == 0x700 else \
            "a button (%s)" % fmt_const("btn", s16(w[0]))
        return "wait for %s, %d frames at most" % (buttons, s16(w[1]))
    if code == 0x32:
        return None
    if code == 0x60:
        return "a name for the script; costs a frame"
    return None


def has_japanese(text):
    return re.search("[\u3040-\u30ff\u4e00-\u9fff\uff66-\uff9f]", text) is not None


FULL_WIDTH = {"【": "[", "】": "]", "…": "...", "・": ".", "　": " ", "～": "~", "“": '"', "”": '"',
              "‘": "'", "’": "'", "－": "-", "＄": "$"}


def plain(text):
    """A literal's text as it reads: %K a new line (/), %NMY / %Nnn a player's name, %Snn spaces,
    full-width letters, digits and signs as ASCII"""
    text = re.sub(r"%NM.", "<player>", text)
    text = re.sub(r"%N(\d\d)", lambda m: "<player %d>" % int(m.group(1)), text)
    text = re.sub(r"%S\d\d", " ", text)
    text = re.sub(r"\s*%K\s*", " / ", text)
    out = []
    for ch in text:
        if ch in FULL_WIDTH:
            out.append(FULL_WIDTH[ch])
        elif 0xFF01 <= ord(ch) <= 0xFF5E:
            out.append(chr(ord(ch) - 0xFEE0))
        else:
            out.append(ch)
    return "".join(out).strip(" /")


_ENGLISH = None


def english(literal):
    """The English of a text with Japanese in it (tools/tss_english.json), or None"""
    global _ENGLISH
    if _ENGLISH is None:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tss_english.json")
        _ENGLISH = json.load(open(path, encoding="utf-8")) if os.path.exists(path) else {}
    return _ENGLISH.get(literal)


MISSING_ENGLISH = set()


def text_note(raw):
    """The comment next to a text: for one in Japanese its English, for one with other double-byte
    characters how it reads in plain ASCII; None for plain ASCII"""
    literal = sjis_text(raw)[1:-1]
    if all(ord(ch) < 0x80 for ch in literal):
        return None
    if has_japanese(literal):
        en = english(literal)
        if en is None:
            MISSING_ENGLISH.add(literal)
            return None
        return "en: " + en
    return plain(literal)


# ---------------------------------------------------------------------------------------------
# The talk scripts in English

# The panel a CPU player's talk is shown in (Script_OpTalkOpen, 0x800853E4: the player's frame at
# x 0x20, 0x100 wide, the text from 8 pixels in; Scene_Update draws a half-width character 7
# pixels wide and a full-width one 14): 240 pixels to a row. Of its 3 rows the first holds the
# speaker's name, so a talk text has 2. A player's name is up to 12 half-width characters
# (NE_MAX_LEN in src/setup.c). tools/tss.py build checks the texts of an edited talk script
# against these.
TALK_ROW_PX = 240
TALK_TEXT_ROWS = 2
TALK_NAME_PX = 12 * 7
STRING_LINE = re.compile(r'^(    string )"((?:[^"\\]|\\.)*)"')


def talk_px(text):
    """How wide a row of game text is in the talk panel, in pixels (a name at its longest)"""
    px = 0
    i = 0
    while i < len(text):
        if text.startswith("%N", i):
            px += TALK_NAME_PX
            i += 4
        elif text.startswith("%S", i):
            px += 7 * int(text[i + 2:i + 4])
            i += 4
        else:
            ch = text[i]
            px += 7 if ord(ch) < 0x80 or 0xFF61 <= ord(ch) <= 0xFF9F else 14
            i += 1
    return px


def talk_fits(literal):
    """Whether a text, as written between quotes in a .tss file, fits the talk panel"""
    text = re.sub(r"\\(x[0-9A-Fa-f]{2}|.)", lambda m: "x" if len(m.group(1)) == 3 else m.group(1), literal)
    rows = text.split("%K")
    return len(rows) <= TALK_TEXT_ROWS and all(talk_px(r) <= TALK_ROW_PX for r in rows)


class Printer:
    def __init__(self, dis, row_note=None):
        self.dis = dis
        self.row_note = row_note
        self.labels = {}
        self.about = {}                     # label offset -> description
        for at, how in dis.code_refs.items():
            item = dis.items.get(at)
            if item and item[0] == "insn":
                self.labels[at] = ("sub_%04X" if how == "call" else "L_%04X") % at
        item_kind = {"anim": "frame", "path": "step", "text": "string", "table": "words"}
        for at, kind in dis.data_refs.items():
            if dis.is_item(at, item_kind[kind]) and at not in self.labels:
                self.labels[at] = "%s_%04X" % (kind, at)
        for at in sorted(set(dis.data_refs) | dis.odd_refs):
            if at in dis.items and at not in self.labels:
                # data named where something else is (a NOTE says so): a label all the same, so
                # that the address moves with the code
                self.labels[at] = ("L_%04X" if dis.items[at][0] == "insn" else "at_%04X") % at
        for at in dis.dead_starts:
            self.labels.setdefault(at, "unused_%04X" % at)
        # routines the scripts share, and small helpers
        refs = reference_routines()
        used = set()
        for at in sorted(self.labels):
            if not (self.labels[at].startswith("sub_") or self.labels[at].startswith("unused_")):
                continue
            named = refs.get(routine_sig(dis, at)) or pattern_name(dis, at)
            if named:
                name, about = named
                k = 2
                while name in used:
                    name = "%s_%d" % (named[0], k)
                    k += 1
                used.add(name)
                self.labels[at] = name
                self.about[at] = about
        # the event card scene's variables
        self.aliases = {}
        self.var_kinds = {i: "cpuflag" for i in range(0xD3, 0xDA)}
        if any(name.startswith("scene_setup") for name in used):
            self.aliases = dict(tss_names.SCENE_VARS)
            self.var_kinds.update({0x26: "fx", 0x28: "card", 0x25: "corners", 0x8C: "cardstate"})
        # what the cases of a switch on the character / the entry are
        self.cases = {}
        for at, item in dis.items.items():
            if item[0] != "insn" or item[1].op.code != 0x12 or not is_var(item[1].words[1]):
                continue
            index = item[1].words[1] & 0xFF
            for k, t in enumerate(item[1].words[2:]):
                if is_var(t):
                    continue
                if index == 0xE2:
                    what = "$char %d: %s" % (k, CHARACTERS[k] if k < len(CHARACTERS) else "?")
                else:
                    what = "%s = %d" % (self.var(index), k)
                self.cases.setdefault(s16(t), []).append(what)

    def var(self, index):
        if index in self.aliases:
            return "$" + self.aliases[index]
        return var_name(index)

    def kind_for(self, word, default):
        """The kind to print a value compared with / stored in the variable word in"""
        if is_var(word) and (word & 0xFF) in self.var_kinds:
            return self.var_kinds[word & 0xFF]
        return default

    def ref(self, word):
        if is_var(word):
            return self.var(word & 0xFF)
        value = s16(word)
        return self.labels.get(value, str(value))

    def value(self, kind, word):
        if is_var(word):
            return self.var(word & 0xFF)
        if kind in ("L",) + DATA_KINDS:
            return self.ref(word)
        return fmt_const(kind, s16(word))

    def insn(self, insn):
        text, note = self.statement(insn)
        if note is None and insn.op.code in (0x60, 0xCA):
            note = text_note(insn.text)
        if note is None:
            note = explain(insn, self)
        return text, note

    def statement(self, insn):
        op, w = insn.op, insn.words
        code = op.code
        if insn.broken:
            return ("bytes 0x00, 0xCA", "talk \"\" without its NUL: the game reads the next byte as "
                    "the NUL and goes on at 0x%04X, inside the next instruction" % (insn.at + 3))
        if code in (0x60, 0xCA):
            return "%s %s" % (op.name, sjis_text(insn.text)), None
        if code == 0x12:
            return "switch %s: %s" % (self.value("n", w[1]), ", ".join(self.ref(t) for t in w[2:])), None
        if code == 0x20:
            a, b = self.value("n", w[1]), self.value("n", w[2])
            test = w[3]
            if not is_var(test) and 0 <= s16(test) < len(TESTS):
                kind = "n"
                if TESTS[s16(test)] in ("&", "!&"):
                    kind = "btn" if is_var(w[1]) and (w[1] & 0xFF) in PAD_VARS else "hex"
                b = self.value(self.kind_for(w[1], kind), w[2])
                return "if %s %s %s goto %s" % (a, TESTS[s16(test)], b, self.ref(w[0])), None
            return "if %s ?%s %s goto %s" % (a, self.value("n", test), b, self.ref(w[0])), \
                "test %s: never holds" % self.value("n", test)
        if code in (0x28, 0x29, 0x2A) and is_var(w[0]):          # bit operations: hex
            kind = "btn" if (w[0] & 0xFF) in PAD_VARS else "hex"
            return op.form.format(self.value("v", w[0]), self.value(self.kind_for(w[0], kind), w[1])), None
        if code == 0x21 and is_var(w[0]):
            return op.form.format(self.value("v", w[0]), self.value(self.kind_for(w[0], "n"), w[1])), None
        if op.form and (op.kinds[0] != "v" or is_var(w[0])):
            if code == 0x2D and not is_var(w[1]) and s16(w[1]) not in self.labels:
                pass                                # a table at an odd place: plain form
            else:
                args = [self.value(k, x) for k, x in zip(op.kinds, w)]
                return op.form.format(*args), None
        args = []
        for i, (k, x) in enumerate(zip(op.kinds, w)):
            text = self.value(k, x)
            if op.keys and op.keys[i]:
                text = "%s=%s" % (op.keys[i], text)
            args.append(text)
        note = None
        if "v" in op.kinds and any(k == "v" and not is_var(x) for k, x in zip(op.kinds, w)):
            note = "a constant where a variable belongs: only $result is set"
        return (op.name + (" " + ", ".join(args) if args else "")), note

    def data_item(self, item):
        kind, payload = item
        if kind == "frame":
            image, time, flip, nxt = payload
            return "frame %s, time=%d, flip=%s, next=%s" % (
                fmt_const("img", image), time, fmt_const("attr", flip), self.link(nxt, "anim"))
        if kind == "step":
            speed, x, y, anim, nxt = payload
            return "step speed=%d, x=%d, y=%d, anim=%s, next=%s" % (
                speed, x, y, self.labels.get(anim, str(anim)), self.link(nxt, "path"))
        if kind == "string":
            return "string " + sjis_text(payload)
        if kind == "words":
            return "words " + ", ".join(self.word(w) for w in payload)
        if kind == "bytes":
            return "bytes " + ", ".join("0x%02X" % b for b in payload)
        raise ValueError(kind)

    def link(self, value, kind):
        if value == -2:
            return "NEXT"
        if value == -1:
            return "END"
        return self.labels.get(value, str(value))

    def word_rows(self, words):
        """A table of words: rows of the distance between its text offsets when that is regular
        (with row_note's comment), else rows of 8"""
        at = [i for i, w in enumerate(words) if self.word(w).startswith("text_")]
        steps = {b - a for a, b in zip(at, at[1:])}
        width = steps.pop() if len(steps) == 1 and at[0] < min(steps | {at[1] - at[0]}) else 8
        rows = []
        for r in range(0, len(words), width):
            line = "words " + ", ".join(self.word(w) for w in words[r:r + width])
            if width != 8 and self.row_note:
                note = self.row_note(r // width, words[r:r + width])
                text = s16(words[r])
                if note and self.dis.is_item(text, "string"):
                    raw = self.dis.items[text][1]
                    said = text_note(raw) or sjis_text(raw)
                    note += ": " + (said[4:] if said.startswith("en: ") else said)
                if note:
                    line = line.ljust(36) + "  ; " + note
            rows.append(line)
        return rows

    def word(self, w):
        value = s16(w)
        if value in self.labels and self.labels[value].startswith("text_"):
            return self.labels[value]
        return str(value)

    def lines(self):
        dis = self.dis
        out = []
        dead_run = False
        for at in sorted(dis.items):
            item = dis.items[at]
            is_insn = item[0] == "insn"
            if at in self.labels:
                out.append("")
                out.append("%s:" % self.labels[at])
                if at in self.about:
                    out += ["    ; " + line for line in wrap(self.about[at], 92)]
                if at in self.cases:
                    out.append("    ; " + ", ".join(self.cases[at]))
                if at in dis.unreferenced:
                    out.append("    ; (nothing names this)")
            if is_insn:
                insn = item[1]
                if insn.dead and not dead_run:
                    out.append("    ; (not reached: nothing jumps here)")
                elif not insn.dead and dead_run:
                    out.append("    ; (reached again)")
                dead_run = insn.dead
                text, note = self.insn(insn)
            else:
                if at not in self.labels and (at == 0 or dis.items[dis.owner[at - 1]][0] == "insn"):
                    out.append("    ; data")
                dead_run = False
                if item[0] in ("words", "bytes") and not any(item[1]) and \
                        at + (2 if item[0] == "words" else 1) * len(item[1]) == dis.n:
                    out.append("    " + self.data_item(item) + "    ; padding")
                    continue
                if item[0] == "words":
                    out += ["    " + row for row in self.word_rows(item[1])]
                    continue
                text = self.data_item(item)
                note = text_note(item[1]) if item[0] == "string" else None
                if item[0] == "step":
                    speed, x, y = item[1][:3]
                    note = ("wait %d frames" % x if speed == 0 else "jump to %d, %d" % (x, y)
                            if speed < 0 else "move to %d, %d, %d pixels a frame" % (x, y, speed))
            line = "    " + text
            if note:
                line = line.ljust(40) + "  ; " + note
            out.append(line)
        return out


def wrap(text, width):
    """text in lines of at most width characters"""
    out, cur = [], ""
    for word in text.split():
        if cur and len(cur) + 1 + len(word) > width:
            out.append(cur)
            cur = word
        else:
            cur = (cur + " " + word) if cur else word
    if cur:
        out.append(cur)
    return out


def summary(dis, pr):
    """What the code that runs does and says, for the head of the file"""
    effects, said = [], []
    for at in sorted(dis.items):
        item = dis.items[at]
        if item[0] != "insn" or item[1].dead:
            continue
        insn = item[1]
        code = insn.op.code
        if 0x180 <= code <= 0x194 and code not in NO_EFFECT:
            text = explain(insn, pr)
            if text and text not in effects:
                effects.append(text)
        if code == 0xCA and len(insn.text) > 3:
            note = text_note(insn.text)
            line = note[4:] if note and note.startswith("en: ") else note or plain(sjis_text(insn.text)[1:-1])
            if line and line not in said:
                said.append(line)
    out = []
    if effects:
        out.append("What it does: " + "; ".join(effects) + ".")
    if said:
        shown = said[:6]
        out.append("What it says: " + " | ".join('"%s"' % t for t in shown) + (" | ..." if len(said) > 6 else ""))
    return out


def disassemble(data, title, how, row_note=None):
    dis = Disassembly(data)
    pr = Printer(dis, row_note)
    head = ["; " + title, ";"]
    for h in how:
        head += ["; " + l for l in wrap(h, 96)] if h else [";"]
    for line in summary(dis, pr):
        head += [";"] + ["; " + l for l in wrap(line, 96)]
    head += [";", "; %d bytes on the disc. Written by `tools/tss.py extract`; `make scripts` (`tools/tss.py "
             "build`)" % len(data), "; assembles the scripts into the disc's files again, in scripts/build/."]
    for p in dis.problems:
        head.append("; NOTE " + p)
    if pr.aliases:
        head += ["", "; The event card scene's variables (scripts/README.md):"]
        head += ["; " + n for n in tss_names.SCENE_VAR_NOTES]
        head += ["var $%s = $v%02X" % (name, index) for index, name in sorted(pr.aliases.items())]
    return "\n".join(head + pr.lines()) + "\n", dis


# ---------------------------------------------------------------------------------------------
# Assembling


class AsmError(Exception):
    pass


def split_args(text):
    """Comma-separated arguments (commas inside quotes kept)"""
    out, cur, quote = [], "", False
    i = 0
    while i < len(text):
        ch = text[i]
        if quote and ch == "\\":
            cur += text[i:i + 2]
            i += 2
            continue
        if ch == '"':
            quote = not quote
        if ch == "," and not quote:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    if cur.strip():
        out.append(cur.strip())
    return out


def strip_comment(line):
    quote = False
    i = 0
    while i < len(line):
        ch = line[i]
        if quote and ch == "\\":
            i += 2
            continue
        if ch == '"':
            quote = not quote
        elif ch == ";" and not quote:
            return line[:i]
        i += 1
    return line


TOKEN = re.compile(r"\s*(?:(0x[0-9A-Fa-f]+|\d+)|(\$\w+)|([A-Za-z_]\w*)|([-+|]))")


class Assembler:
    def __init__(self, text):
        self.lines = text.split("\n")
        self.labels = {}
        self.aliases = {}

    def var(self, name, line):
        name = name[1:]
        if name in self.aliases:
            return 0x8000 | self.aliases[name]
        if name in VAR_INDEX:
            return 0x8000 | VAR_INDEX[name]
        m = re.fullmatch(r"v([0-9A-Fa-f]{2})", name)
        if m:
            return 0x8000 | int(m.group(1), 16)
        raise AsmError("line %d: no variable $%s" % (line, name))

    def expr(self, text, line, pass_no, data=False):
        """An operand: a variable, or a constant expression (numbers, names, labels, + - |);
        data: a word of data, not an operand (any value)"""
        text = text.strip()
        if "=" in text and re.match(r"^\w+=", text):
            text = text.split("=", 1)[1].strip()
        if text.startswith("$"):
            return self.var(text, line)
        pos = 0
        value = None
        op = "+"
        sign = 1
        while pos < len(text):
            m = TOKEN.match(text, pos)
            if not m or m.end() == pos:
                raise AsmError("line %d: cannot read %r" % (line, text))
            pos = m.end()
            num, varname, name, oper = m.groups()
            if oper:
                if oper == "-" and (value is None or op is not None):
                    sign = -sign
                else:
                    op = oper
                continue
            if varname:
                raise AsmError("line %d: a variable inside an expression: %r" % (line, text))
            if num:
                v = int(num, 0)
            elif name in SYMBOLS:
                v = SYMBOLS[name]
            elif name in self.labels:
                v = self.labels[name]
            elif pass_no == 1:
                v = 0
            else:
                raise AsmError("line %d: unknown name %s" % (line, name))
            v *= sign
            sign = 1
            if value is None:
                value = v
            elif op == "+":
                value += v
            elif op == "-":
                value -= v
            elif op == "|":
                value |= v
            op = None
        if value is None:
            raise AsmError("line %d: no value in %r" % (line, text))
        if not -0x8000 <= value <= 0xFFFF:
            raise AsmError("line %d: %d does not fit in a word" % (line, value))
        word = value & 0xFFFF
        if is_var(word) and not data:
            raise AsmError("line %d: constant %d would read as a variable" % (line, value))
        return word

    def string(self, text, line):
        text = text.strip()
        if not (text.startswith('"') and text.endswith('"') and len(text) >= 2):
            raise AsmError("line %d: expected a quoted text" % line)
        return parse_text(text[1:-1], line)

    def statement(self, text, line, pass_no):
        """bytes of one statement"""
        ev = lambda t: self.expr(t, line, pass_no)       # noqa: E731
        dv = lambda t: self.expr(t, line, pass_no, True)  # noqa: E731
        words = lambda ws: b"".join(struct.pack(">H", w) for w in ws)   # noqa: E731
        m = re.fullmatch(r"(\w+)\s*(.*)", text)
        head, rest = (m.group(1), m.group(2)) if m else ("", text)
        # data
        if head == "bytes":
            return bytes(dv(a) & 0xFF for a in split_args(rest))
        if head in ("words", "frame", "step"):
            return words(dv(a) for a in split_args(rest))
        if head == "string":
            return self.string(rest, line) + b"\0"
        if head in ("talk", "marker") and rest.startswith('"'):
            return words([BY_NAME[head].code]) + self.string(rest, line) + b"\0"
        # flow
        m = re.fullmatch(r"if\s+(.+?)\s+(==|!=|>=|<=|>|<|!&|&|\?\S+)\s+(.+?)\s+goto\s+(\S+)", text)
        if m:
            a, test, b, target = m.groups()
            t = ev(test[1:]) if test.startswith("?") else TESTS.index(test)
            return words([0x20, ev(target), ev(a), ev(b), t])
        m = re.fullmatch(r"switch\s+(.+?):\s*(.*)", text)
        if m:
            targets = split_args(m.group(2))
            return words([0x12, len(targets), ev(m.group(1))] + [ev(t) for t in targets])
        # assignments
        m = re.fullmatch(r"(\$\w+)\s*\[(.+)\]\s*=\s*(.+)", text)
        if m:
            return words([0x2E, ev(m.group(1)), ev(m.group(2)), ev(m.group(3))])
        m = re.fullmatch(r"(\$\w+)\s*(\+|-|\*|/|%|&|\||\^)?=\s*(.+)", text)
        if m:
            dest, oper, src = m.groups()
            if oper:
                code = {"+": 0x23, "-": 0x24, "*": 0x25, "/": 0x26, "%": 0x27, "&": 0x28,
                        "|": 0x29, "^": 0x2A}[oper]
                return words([code, ev(dest), ev(src)])
            src = src.strip()
            mm = re.fullmatch(r"random\s+(.+)", src)
            if mm:
                return words([0x2C, ev(dest), ev(mm.group(1))])
            mm = re.fullmatch(r"(\S+)\s*\[(.+)\]", src)
            if mm:
                return words([0x2D, ev(dest), ev(mm.group(1)), ev(mm.group(2))])
            if src == "xa_playing":
                return words([0x39, ev(dest)])
            if src == "talk_y":
                return words([0xCE, ev(dest)])
            return words([0x21, ev(dest), ev(src)])
        # plain: name args
        op = BY_NAME.get(head)
        if op is None:
            raise AsmError("line %d: unknown statement %r" % (line, text))
        args = split_args(rest)
        if op.code == 0x12 or op.code in (0x60, 0xCA):
            raise AsmError("line %d: bad %s" % (line, head))
        if len(args) != len(op.kinds):
            raise AsmError("line %d: %s takes %d operands" % (line, head, len(op.kinds)))
        if op.keys:
            for a, key in zip(args, op.keys):
                given = re.match(r"^(\w+)=", a)
                if given and given.group(1) != key:
                    raise AsmError("line %d: operand %s= where %s= belongs" % (line, given.group(1), key))
        return words([op.code] + [ev(a) for a in args])

    def run(self):
        for pass_no in (1, 2):
            out = bytearray()
            for n, raw in enumerate(self.lines, 1):
                line = strip_comment(raw).strip()
                if not line:
                    continue
                m = re.fullmatch(r"var\s+\$(\w+)\s*=\s*(\$\w+)", line)
                if m:
                    if m.group(1) in VAR_INDEX:
                        raise AsmError("line %d: $%s is a name of the interpreter's" % (n, m.group(1)))
                    self.aliases[m.group(1)] = self.var(m.group(2), n) & 0xFF
                    continue
                m = re.fullmatch(r"([A-Za-z_]\w*):", line)
                if m:
                    if pass_no == 1:
                        if m.group(1) in self.labels:
                            raise AsmError("line %d: label %s defined twice" % (n, m.group(1)))
                        self.labels[m.group(1)] = len(out)
                    continue
                out += self.statement(line, n, pass_no)
        return bytes(out)


def assemble(text):
    return Assembler(text).run()


# ---------------------------------------------------------------------------------------------
# Commands


def edited(path, data):
    """Whether the script file at path holds a script other than data (it does not assemble to it)"""
    try:
        return assemble(open(path, encoding="utf-8").read()) != data
    except (AsmError, OSError, UnicodeDecodeError):
        return True


def cmd_extract(outdir, force=False):
    """Write every script on the disc into outdir and check that each assembles back to the same
    bytes. A script file already there that holds an edited script is kept, unless force."""
    sources = script_sources()
    os.makedirs(outdir, exist_ok=True)
    try:
        import tss_readme               # not in the distribution, which ships scripts/README.md
    except ImportError:
        print("README.md not written (tools/tss_readme.py writes it; the one in scripts/ is current)")
    else:
        open(os.path.join(outdir, "README.md"), "w", encoding="utf-8").write(
            tss_readme.readme(sys.modules[__name__], tss_names))
    bad = 0
    problems = 0
    kept = []
    for path, title, how, data, row_note, _, _, _ in sources:
        text, dis = disassemble(data, title, how, row_note)
        problems += len(dis.problems)
        full = os.path.join(outdir, path)
        if not force and os.path.exists(full) and edited(full, data):
            kept.append(path)
            continue
        os.makedirs(os.path.dirname(full), exist_ok=True)
        open(full, "w", encoding="utf-8").write(text)
        try:
            again = assemble(text)
        except AsmError as e:
            print("%s: %s" % (path, e))
            bad += 1
            continue
        if again != data:
            k = next((i for i in range(min(len(again), len(data))) if again[i] != data[i]),
                     min(len(again), len(data)))
            print("%s: assembles to different bytes (first at 0x%04X; %d vs %d bytes)"
                  % (path, k, len(again), len(data)))
            bad += 1
    print("%d scripts written to %s; %d do not assemble back; %d notes" % (len(sources) - len(kept), outdir,
                                                                        bad, problems))
    if kept:
        print("%d edited scripts kept as they are (--force writes the disc's over them): %s"
              % (len(kept), ", ".join(kept)))
    return bad


# The most a talk script may unpack to. The original unpacks a talk script into a buffer the size
# of its packed form rounded up to whole sectors (Board_Task steps 3201 and 9200), which the talk
# scripts on the disc already overrun by up to 416 bytes (2464 at most); it never plays them. A
# CPU_TALK build, which plays them, unpacks one into 2 sectors (TALK_SCRIPT_SECTORS in src/board.c).
TALK_MAX_UNPACKED = 2 * 2048


def build_files(srcdir, disc=DISC):
    """Assemble the scripts in srcdir into the disc files that hold them. Returns ({file: bytes},
    [errors], [warnings], {file: [scripts changed]}); a file with an error is left out."""
    if not os.path.isdir(disc):
        raise SystemExit("no %s: extract the disc image first (make extract)" % disc)
    files, errors, warnings, changed = {}, [], [], {}
    by_file = {}
    for path, title, how, data, row_note, name, entry, packed in script_sources():
        by_file.setdefault(name, []).append((path, entry, packed, data))
    for name, scripts in by_file.items():
        raw = open(os.path.join(disc, name), "rb").read()
        entries = archive_entries(raw)
        bad = False
        for path, entry, packed, data in scripts:
            full = os.path.join(srcdir, path)
            try:
                source = open(full, encoding="utf-8").read()
                code = assemble(source)
            except (AsmError, OSError) as e:
                errors.append("%s: %s" % (full, e))
                bad = True
                continue
            if code == data:
                continue                    # as on the disc: the original bytes stay
            changed.setdefault(name, []).append(path)
            if packed:
                for line in source.split("\n"):
                    m = STRING_LINE.match(line)
                    if m and not talk_fits(m.group(2)):
                        errors.append('%s: "%s" does not fit the talk panel (%d rows of %d pixels: a half-width '
                                      'character 7, a full-width one 14, a name %d)' % (full, m.group(2),
                                      TALK_TEXT_ROWS, TALK_ROW_PX, TALK_NAME_PX))
                entries[entry] = lzss_compress(code)
                if len(code) > TALK_MAX_UNPACKED:
                    errors.append("%s: unpacks to %d bytes; a CPU_TALK build unpacks a talk script into "
                                  "%d bytes, so shorten it by %d" % (full, len(code), TALK_MAX_UNPACKED,
                                                                     len(code) - TALK_MAX_UNPACKED))
            else:
                entries[entry] = code
        if bad:
            continue
        out = pack_archive(entries) if name in changed else raw
        # the game finds its files at fixed places on the disc (g_fileLocTable): a file that got
        # shorter is padded to its size on the disc, so that it and the files after it keep their
        # places when a disc image is built with it
        out += b"\0" * (len(raw) - len(out))
        if sectors(len(out)) > sectors(len(raw)):
            errors.append("%s: %d bytes, %d sectors; the game reads %d sectors of it (the size in "
                          "its file table, g_fileSectorTable), so shorten %s by %d bytes"
                          % (name, len(out), sectors(len(out)), sectors(len(raw)),
                             " / ".join(changed[name]), len(out) - sectors(len(raw)) * SECTOR))
        files[name] = out
    return files, errors, warnings, changed


def cmd_build(srcdir, outdir):
    files, errors, warnings, changed = build_files(srcdir)
    os.makedirs(outdir, exist_ok=True)
    for name, data in files.items():
        open(os.path.join(outdir, name), "wb").write(data)
    for w in warnings:
        print("warning: " + w)
    for e in errors:
        print("error: " + e)
    print("%d files written to %s; %d with edited scripts%s" % (
        len(files), outdir, len(changed),
        (": " + ", ".join("%s (%s)" % (n, ", ".join(changed[n])) for n in sorted(changed))) if changed else ""))
    return len(errors)


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd = argv[1]
    if cmd == "extract":
        args = argv[2:]
        force = "--force" in args
        args = [a for a in args if a != "--force"]
        if len(args) > 1:
            sys.exit(__doc__)
        sys.exit(1 if cmd_extract(args[0] if args else "scripts", force) else 0)
    if cmd == "build":
        args = argv[2:]
        outdir = None
        if "-o" in args:
            k = args.index("-o")
            if k + 1 >= len(args):
                sys.exit("build: -o needs a directory")
            outdir = args[k + 1]
            del args[k:k + 2]
        if len(args) > 1:
            sys.exit(__doc__)
        srcdir = args[0] if args else "scripts"
        sys.exit(1 if cmd_build(srcdir, outdir or os.path.join(srcdir, "build")) else 0)
    if cmd == "asm":
        open(argv[3], "wb").write(assemble(open(argv[2], encoding="utf-8").read()))
        return
    if cmd == "dis":
        raw = open(argv[2], "rb").read()
        entries = archive_entries(raw)
        if len(argv) > 3:
            data = lzss_decompress(entries[int(argv[3])])
        else:
            data = entries[0]
        sys.stdout.write(disassemble(data, os.path.basename(argv[2]), [])[0])
        return
    sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
