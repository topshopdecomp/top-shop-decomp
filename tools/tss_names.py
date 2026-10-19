"""Names and descriptions tools/tss.py gives the routines and variables the scripts share.

The event scripts (EVE_002-EVE_099) are built on one framework: the card scene of an event.
The card picture (sprite 0, scene image 7) enters the screen in one of seven ways; four corner
pieces (sprites 1-4, scene image 1) frame it and play effects (zoom, squeeze, circle, spin,
pulse); 30 tiles (sprites 10-39, scene image 10) make the background. A routine is recognized
by its code (the same instructions, wherever it sits in the file); its name comes from its copy
in the reference script below. Offsets are those of the reference copy.
"""

# reference script -> {offset: (name, description)}
ROUTINES = {
    "EVE_002": {
        0x0F9A: ("clear_vars", "variables 0x00-0xA4 = 0"),
        0x0204: ("scene_setup", "the scene: layer 0 (scene image 0); the card (sprite 0, image 7) "
                 "off screen; the four corners (sprites 1-4, image 1) at their home positions, "
                 "worked out from the size of image 1 (the frame fills 320 x 170); the 30 "
                 "background tiles (sprites 10-39, image 10) in rows of 6; dim 128; "
                 "$corner_steps = 20, the zoom and squeeze counters set"),
        0x00AE: ("corners_animate", "start the animations of the card and the four corners"),
        0x00CE: ("corners_animate_alt", "other animations for the corners (not used)"),
        0x052C: ("effects_settle", "(not used) run the effects until the corner entrance, "
                 "FX_GROW, FX_SHRINK, FX_CIRCLE and the squeezes have run down, then until the "
                 "spin and the pulses are back at rest"),
        0x0390: ("effects_default", "no corner entrance; only FX_TILES"),
        0x03A4: ("card_show", "music 3; open the view and the talk panel; then every frame the "
                 "corner entrance ($corner_entrance), the effects ($corner_effects) and the card's "
                 "entrance ($card_entrance), until the voice has ended and cross is pressed (a "
                 "CPU player: 60 frames); then the corners back home and a sound"),
        0x0132: ("card_close", "close the view and take everything away (corners_leave); wait "
                 "until the view and the talk panel are closed"),
        0x0148: ("corners_leave", "the corners draw in for 10 frames, the talk panel closes, "
                 "then they fly off for 15 frames; meanwhile the card sinks into the talk panel "
                 "(card_sink) and the tiles shrink"),
        0x0200: ("finish", "stop the music; end of the script"),
        0x0F60: ("wait_cross", "keep the effects going until cross is pressed (a CPU player: 30 "
                 "frames); a sound"),
        0x060C: ("effects_run", "one frame of the effects that keep going (FX_SPIN, FX_PULSE_X, "
                 "FX_PULSE_Y, FX_PULSE, FX_TILES), then corners_apply"),
        0x0658: ("corners_apply", "corner positions plus this frame's offsets to sprites 1-4, "
                 "their scale = $corner_stretch_x/y * $corner_zoom / 50; offsets, zoom and stretch "
                 "back to neutral; wait a frame"),
        0x0758: ("corners_spread", "corner entrance 1: all four start at the center and spread "
                 "out to their home positions over $corner_steps frames"),
        0x086A: ("corners_fly_in", "corner entrance 2: the corners fly in from the edges of the "
                 "screen over $corner_steps frames"),
        0x0928: ("fx_grow", "FX_GROW: zoom = 1 - $grow_amount/256; the amount falls by 10 a frame"),
        0x0946: ("fx_shrink", "FX_SHRINK: zoom = 1 + $shrink_amount/256; the amount falls by 10 "
                 "a frame"),
        0x0A10: ("fx_circle", "FX_CIRCLE: the corners circle around their places; the radius "
                 "$circle_radius falls by 1 a frame"),
        0x0964: ("fx_squeeze_x", "FX_SQUEEZE_X: the corners start 100 pixels further apart "
                 "sideways and ease back ($squeeze_x_angle)"),
        0x09BA: ("fx_squeeze_y", "FX_SQUEEZE_Y: the same up and down ($squeeze_y_angle)"),
        0x0B86: ("fx_spin", "FX_SPIN: the corners turn (1 and 3 one way, 2 and 4 the other)"),
        0x0ADE: ("fx_pulse_x", "FX_PULSE_X: the corners turn over sideways, around and around "
                 "(their width swings between -1 and 1, $pulse_x_angle)"),
        0x0B32: ("fx_pulse_y", "FX_PULSE_Y: the same, turning over top to bottom ($pulse_y_angle)"),
        0x0A7C: ("fx_pulse", "FX_PULSE: the corners throb (size 1 to 1.5, $pulse_angle)"),
        0x165A: ("fx_tiles_scroll", "FX_TILES: the background tiles drift down and to the left, "
                 "wrapping around"),
        0x16B2: ("fx_tiles_grow", "FX_TILES: the background tiles grow to full size"),
        0x1714: ("tiles_shrink", "the background tiles shrink; hidden once gone"),
        0x0BA0: ("fx_spin_stop", "stop FX_SPIN once the angle is back at 0 (not used)"),
        0x0B10: ("fx_pulse_x_stop", "stop FX_PULSE_X at the end of a swing (not used)"),
        0x0B64: ("fx_pulse_y_stop", "stop FX_PULSE_Y at the end of a swing (not used)"),
        0x0ABC: ("fx_pulse_stop", "stop FX_PULSE at the end of a swing (not used)"),
        0x0F1A: ("card_enter", "one frame of the card's entrance, by $card_entrance"),
        0x0BDA: ("card_slide_left", "CARD_SLIDE_LEFT: the card slides in from the left, slowing "
                 "down, to the center (160, 120)"),
        0x0C20: ("card_slide_right", "CARD_SLIDE_RIGHT: the card slides in from the right"),
        0x0C6C: ("card_zoom", "CARD_ZOOM: the card appears 16 times its size at the center and "
                 "shrinks to its size"),
        0x0CC2: ("card_rise", "CARD_RISE: the card grows out of the top of the talk panel, "
                 "moving right from the center (it ends at x 240, standing on the panel)"),
        0x0E1E: ("card_spiral", "CARD_SPIRAL: the card spirals in to the center"),
        0x0E8E: ("card_bounce", "CARD_BOUNCE: the card bounces up and down, less and less, into "
                 "the center"),
        0x0D72: ("card_sink", "the card shrinks into the talk panel, moving back to the center, "
                 "and is hidden"),
        0x1760: ("money_by_years", "$v00 = 100 * ($months / 24 + 1): an amount that grows every "
                 "two years"),
    },
    "CAL_02": {
        0x05FA: ("snowball_new", "snowball $vA0 (sprite $vA0): a random x along the top, a random "
                 "size (150-229, 256 = full) and sway angle. Particles are records of 6 variables "
                 "from $v0A: x and y in tenths of a pixel, size, sway angle, sway x, sway y"),
        0x0654: ("snowball_fall", "snowball $vA0 sways (wider for bigger ones) and falls 5-9 + "
                 "size/16 tenths of a pixel"),
    },
    "CAL_03": {
        0x0563: ("petal_new", "petal $vA0: a random place along the top or the right edge, a "
                 "random size (records of 3 from $v0A: x, y in tenths of a pixel, size)"),
        0x05C5: ("petal_fall", "petal $vA0 drifts left and down, bigger ones faster"),
    },
    "CAL_04": {
        0x055A: ("butterfly_new", "butterfly $vA0: a random place, size, wing state and heading "
                 "(records of 5 from $v0A)"),
        0x05C2: ("butterfly_fly", "butterfly $vA0 changes its wing animation one frame in 100 "
                 "(one of three colors by $vA0 % 3) and flutters along, weaving up and down"),
    },
    "CAL_06": {
        0x05A1: ("raindrop_new", "raindrop $vA0: a random place along the top or the right edge, "
                 "a random size (records of 3 from $v0A: x, y in tenths of a pixel, size)"),
        0x0603: ("raindrop_fall", "raindrop $vA0 falls slanting to the left, bigger ones faster"),
    },
    "CAL_07": {
        0x05BF: ("rocket_new", "rocket $vA0: a random x along the bottom, a random size (records "
                 "of 6 from $v0A)"),
        0x061F: ("rocket_rise", "rocket $vA0 rises, wobbling (y = 30000: it has burst and stays)"),
        0x06D1: ("fireworks_flash", "a burst lights up the watchers: the dimming of the DIM "
                 "sprites ($v9E, 255 = dark) falls to 0 and comes back, at random moments"),
    },
    "CAL_09": {
        0x04E9: ("dragonfly_new", "dragonfly $vA0: a random place and size (records of 7 from $v0A)"),
        0x0543: ("dragonfly_fly", "dragonfly $vA0 hovers and now and then darts off in a random "
                 "direction, its picture turned to face it"),
        0x06DD: ("dragonfly_keep_in", "keep dragonfly $vA0 within -50..370, -50..290 pixels"),
    },
    "CAL_12": {
        0x05EB: ("snow_new", "snowflake $vA0: a random x along the top, a random size and sway "
                 "angle (records of 6 from $v0A, as February's snowballs)"),
        0x0645: ("snow_fall", "snowflake $vA0 sways and falls"),
        0x06EB: ("reindeer_prance", "reindeer $vA0 (sprite 30 + $vA0) moves up to the right 5 "
                 "pixels a frame, bobbing, and starts again from the left when far off screen"),
    },
    "EVE_040": {
        0x0D57: ("card_sink_from_right", "the card shrinks into the talk panel, moving from x 260 "
                 "to the center, and is hidden (this event's card rises to x 260)"),
    },
    "EVE_083": {
        0x0D5F: ("card_sink_center", "the card shrinks into the talk panel at the center and is hidden"),
    },
    "EVE_073": {
        0x02E3: ("quiz_print_answer", "add answer $v00 to the text: 0 the right one ($v04; its row "
                 "$v03 goes to $v07), 1 $v05, 2 $v06"),
        0x0305: ("quiz_choose", "the player picks one of three rows with up and down; the choice "
                 "is confirmed with button bit 0x0200, which no button gives, so only a CPU player "
                 "gets through (it picks row $v00, the right one, 70% of the time); the row blinks "
                 "three times; $v00 = the row"),
        0x045B: ("effects_for", "keep the effects going for $v00 frames"),
    },
    "EVE_095": {
        0x0208: ("scene_setup_at_home", "scene_setup, but the corners start at their home positions"),
        0x0394: ("effects_test_input", "a test: the player types $corner_entrance and then "
                 "$corner_effects as numbers (up/down 1, right/left 10, R2/triangle 100, circle/L1 "
                 "1000), each confirmed with button bit 0x0200, which no button gives: it never ends"),
        0x04AC: ("fx_pick_pulse", "(not used) meant to keep one of FX_PULSE_X / FX_PULSE_Y at "
                 "random; as written it leaves $corner_effects = the other one if it was set, else 0"),
        0x04E8: ("card_show_test", "card_show without the music or the wait for the voice; a CPU "
                 "player waits exactly 60 frames"),
    },
}

# Variables of the event card scene (the scripts with scene_setup)
SCENE_VARS = {
    0x0A: "corner1_x", 0x0B: "corner1_y", 0x0C: "corner2_x", 0x0D: "corner2_y",
    0x0E: "corner3_x", 0x0F: "corner3_y", 0x10: "corner4_x", 0x11: "corner4_y",
    0x12: "corner_zoom", 0x13: "corner_stretch_x", 0x14: "corner_stretch_y",
    0x15: "corner1_dx", 0x16: "corner1_dy", 0x17: "corner2_dx", 0x18: "corner2_dy",
    0x19: "corner3_dx", 0x1A: "corner3_dy", 0x1B: "corner4_dx", 0x1C: "corner4_dy",
    0x1D: "corner1_home_x", 0x1E: "corner1_home_y", 0x1F: "corner2_home_x", 0x20: "corner2_home_y",
    0x21: "corner3_home_x", 0x22: "corner3_home_y", 0x23: "corner4_home_x", 0x24: "corner4_home_y",
    0x25: "corner_entrance", 0x26: "corner_effects", 0x28: "card_entrance",
    0x8C: "card_state", 0x8D: "card_angle", 0x93: "card_sink_step", 0x94: "card_rise_step",
    0x96: "pulse_angle", 0x97: "pulse_x_angle", 0x98: "pulse_y_angle",
    0x99: "shrink_amount", 0x9A: "grow_amount", 0x9B: "corner_steps",
    0x9C: "squeeze_y_angle", 0x9D: "squeeze_x_angle", 0x9E: "circle_radius", 0x9F: "circle_angle",
}
SCENE_VAR_NOTES = [
    "corner1-4: top left, top right, bottom left, bottom right (sprites 1-4)",
    "$cornerN_x/_y: position; _dx/_dy: offset for this frame only; _home_x/_y: place in the frame",
    "$corner_zoom: 256 = full size, this frame; $corner_stretch_x/_y: 50 = no stretch",
    "$corner_entrance: CORNERS_*; $corner_effects: FX_* bits; $card_entrance: CARD_*",
    "$card_state: 0 not started, 32767 done (the card routines count in it)",
]

# Values of the scene variables
FX_BITS = [("FX_GROW", 0x0001), ("FX_SHRINK", 0x0002), ("FX_CIRCLE", 0x0004),
           ("FX_SQUEEZE_X", 0x0008), ("FX_SQUEEZE_Y", 0x0010), ("FX_SPIN", 0x0100),
           ("FX_PULSE_X", 0x0200), ("FX_PULSE_Y", 0x0400), ("FX_PULSE", 0x0800),
           ("FX_TILES", 0x4000)]
CARD_ENTRANCES = {0: "CARD_STILL", 1: "CARD_SLIDE_LEFT", 2: "CARD_SLIDE_RIGHT", 3: "CARD_ZOOM",
                  4: "CARD_RISE", 5: "CARD_SPIRAL", 6: "CARD_BOUNCE"}
CORNER_ENTRANCES = {0: "CORNERS_HOME", 1: "CORNERS_SPREAD", 2: "CORNERS_FLY_IN"}
CARD_DONE = 32767

# What the calendar pages show (their scene images, see scripts/README.md)
CALENDAR = {
    1: "a cat in a lion-dance costume (image 0) sways and bobs in front of a snowy hill with the "
       "word JANUARY (image 3); spin lines turn at the bottom right (images 1-2); the sheet also "
       "holds spinning tops, an owl and a kite that the script does not show",
    2: "a snowman (images 0-8) on the page; eight snowballs (image 13) drift about at random; a "
       "small snowman head (image 9) rolls in from the right and back, spinning",
    3: "cherry blossoms: a character under a blossoming tree (images 0, 1, 5, 7), the cactus "
       "swinging on a swing; petals (image 9) fall at random",
    4: "spring: a sleeping character and the cactus waking up (images 0-6); butterflies "
       "(images 12-17) flutter across in three flights",
    5: "carp streamers (koinobori) for Children's Day: the cactus in its pot flies as a streamer "
       "(images 0-3), with streamers (images 4-7) and wind wheels (images 8-9) turning",
    6: "the rainy season: two snails (images 0-5) crawl in from the right, frogs (images 6-8) "
       "croak; raindrops (image 12) fall at random",
    7: "summer fireworks: two characters seen from behind on a hill (image 9) watch rockets "
       "(image 3) burst into fireworks (images 0-2, 5-8) at random",
    8: "the beach: a parasol (image 0), bubbles, a green dragon, the TV robot, the cactus buried "
       "in the sand and a surfer chased by sharks (images 2-9)",
    9: "autumn: dragonflies (images 0-1) dart about at random; a small flying saucer (image 2) "
       "crosses the sky",
    10: "Halloween: a werewolf howls (images 0-2) with the word AOHOOON (image 5); a witch on a "
        "broom (image 3) and a cat (image 4) fly across",
    11: "November: the frog prince, the cactus under a November sign and a character in a "
        "bandana (images 0-11) take turns",
    12: "Christmas: reindeer (image 0) prance in a row, a girl in a Santa hat (images 1-2), chicks "
        "(images 3-5); snowflakes (image 6) fall at random",
    13: "the same script and pictures as January (CAL_01)",
}

# What each opcode does (scripts/README.md); handlers in src/script.c, src/sceneops.c, src/gameops.c
OP_DOCS = {
    0x00: "end of the script (frees the talk panel's glyphs)",
    0x01: "wait n frames (FOREVER: never goes on)",
    0x02: "wait until a button of the mask is pressed (pad 1); $result = the buttons",
    0x03: "the same for at most n frames ($result = 0 on time-out); FOREVER: stays here",
    0x10: "jump",
    0x12: "jump to the label in the list picked by the value (0 the first); past the list if out of range",
    0x13: "call a routine: push the return point and $l0-$l15",
    0x14: "return from a call, restoring $l0-$l15",
    0x15: "stay here for good",
    0x20: "jump if the test holds: == != > < >= <=, & (all bits of b set in a), !& (no bit in common)",
    0x21: "set a variable (every computing statement also sets $result)",
    0x23: "add", 0x24: "subtract", 0x25: "multiply", 0x26: "divide (rounds toward 0)",
    0x27: "remainder", 0x28: "bitwise and", 0x29: "bitwise or", 0x2A: "bitwise exclusive or",
    0x2B: "$x = -$x",
    0x2C: "a random number 0 .. n-1",
    0x2D: "read a table: word [index] of a table in the script, or variable $base+index",
    0x2E: "write variable $base+index",
    0x30: "play song n", 0x31: "stop the music", 0x32: "play sound effect n (game sound n+0x17)",
    0x33: "no effect", 0x34: "no effect (reads its operand)", 0x35: "no effect",
    0x36: "play voice track file*16+channel of XA set 1", 0x37: "stop the voice",
    0x38: "go on next frame", 0x39: "$x = 1 while a voice plays",
    0x40: "fade in over n frames", 0x41: "fade out", 0x42: "fade in from white",
    0x43: "fade out to white", 0x44: "set the fade level (r, g, b) for n frames, then put it back",
    0x45: "wait until the fade is done",
    0x60: "a name in the script, skipped (costs a frame; 13 characters at most)",
    0x61: "the view rectangle (x or y -1: centered); closed", 0x62: "open the view over n frames",
    0x63: "close the view", 0x64: "wait for the view to open or close",
    0x70: "background layer n shows scene image (priority, LAYER flags, BANDn: lines per band)",
    0x71: "layer position", 0x72: "layer scrolls by vx, vy a frame",
    0x73: "x offset of bands first..first+count", 0x74: "x offset steps of bands (each frame)",
    0x75: "bands follow a sine wave", 0x76: "the wave moves by step a frame",
    0x80: "sprite n shows an image at x, y (scale 1, angle 0)", 0x81: "remove sprite n",
    0x82: "image and attr of sprite n (stops its animation and movement)", 0x83: "attr of sprite n",
    0x84: "move sprite n by dx, dy", 0x85: "position of sprite n",
    0x86: "sprite n moves to x, y at speed pixels a frame (0: waits x frames, -1: jumps there)",
    0x87: "add to the scales (256 = 1)", 0x88: "set the scales (256 = 1)", 0x89: "no effect",
    0x8D: "turn sprite n (32768 = a full turn)", 0x8E: "set the angle", 0x8F: "no effect",
    0x90: "sprite n plays an animation (frames)", 0x91: "sprite n follows a path (steps)",
    0x92: "$x = sprite n's state (1 animating, 2 moving, 0x10 on a path)",
    0x93: "position of sprite n", 0x94: "scales of sprite n", 0x95: "angle of sprite n",
    0x96: "width and height of scene image n", 0x97: "how dark DIM sprites are (0-255)",
    0xA0: "rectangle n (never used for anything)", 0xA1: "rectangle n off",
    0xA2: "rectangle n = the view", 0xB0: "$to_x, $to_y = x, y moved distance at angle",
    0xC0: "slide the talk panel in", 0xC1: "slide it out", 0xC2: "wait until it is in",
    0xC3: "wait until it is out", 0xC4: "highlight row n", 0xC5: "no highlighted row",
    0xC6: "who speaks: HOST (wide panel) or PLAYER (the current player's frame)",
    0xC8: "clear the text", 0xC9: "show the text (operand 0; another value does nothing)",
    0xCA: "add a text to the panel (codes: %NMY, %Nnn, %K, %Snn)",
    0xCB: "add a number (width digits)", 0xCC: "add player n's name",
    0xCD: "add the store type name of store n", 0xCE: "$x = the talk panel's y",
    0xCF: "add the text at a label (or an offset in a variable)", 0xD0: "no effect",
    0xD1: "no effect", 0xF1: "no effect", 0xF2: "no effect",
    0x100: "show the current player's portrait with a face, unsquashing it",
    0x101: "squash the portrait away", 0x102: "change the portrait's face",
    0x140: "$x = how many stores the player(s) own", 0x141: "$x = their n-th store (out of range: 0 here, a leftover value in the original)",
    0x180: "the player(s) earn an amount (negative: pay)",
    0x181: "the stores get count goods slots at a level (0 the most expensive)",
    0x182: "no effect", 0x183: "no effect", 0x184: "the current player may open a store on a vacant lot along the route",
    0x185: "the current player sells a store for a percentage of its value (SELL_*)",
    0x186: "no effect", 0x187: "no effect", 0x188: "result 1: another event follows",
    0x189: "the calendar moves on n months", 0x18A: "the current player moves n squares (result 3)",
    0x18B: "the current player stops (STOP) or turns around (REVERSE)",
    0x18C: "the player(s) can't take the elevator for n months",
    0x18D: "the stores sell at a percentage for n months",
    0x18E: "the player(s)' money * percent / 100", 0x18F: "the player(s)' money = player n's",
    0x190: "result 2: the player rolls again", 0x191: "the stores close for n months",
    0x192: "the stores close (only_empty ON: those without goods); the owner gets a percentage",
    0x193: "the player(s) put on (ON) or take off the Chirp-Chirp shoes",
    0x194: "the stores' goods n levels more expensive (negative: cheaper)",
}
