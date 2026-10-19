/*
 * topshop.h - every header of the game
 *
 * The PsyQ subset, the file indices and the header of each C file (src/<name>.c declares what
 * it shares in include/<name>.h), grouped as in docs/index.md. Every C file includes this.
 */
#ifndef TOPSHOP_H
#define TOPSHOP_H

#include "common.h"
#include "psyq.h"
#include "fileid.h"

/* system */
#include "main.h"
#include "sys.h"
#include "pad.h"
#include "task.h"
#include "util.h"
#include "debug.h"

/* files and saves */
#include "fileio.h"
#include "lzss.h"
#include "card.h"
#include "save.h"

/* graphics and sound */
#include "gfx.h"
#include "tilesheet.h"
#include "fade.h"
#include "bgmap.h"
#include "sound.h"

/* text and windows */
#include "font.h"
#include "text.h"
#include "textwin.h"
#include "msg.h"

/* front end */
#include "game.h"
#include "logo.h"
#include "title.h"
#include "menu.h"
#include "newgame.h"
#include "setup.h"

/* the board game */
#include "board.h"
#include "boardsetup.h"
#include "boardmap.h"
#include "boarddraw.h"
#include "shopopen.h"
#include "move.h"
#include "dice.h"
#include "elevator.h"
#include "player.h"
#include "stores.h"
#include "storemenus.h"
#include "bank.h"
#include "hud.h"
#include "turnmenu.h"
#include "options.h"
#include "events.h"
#include "effects.h"
#include "talk.h"
#include "cpu.h"

/* event scripts */
#include "script.h"
#include "sceneops.h"
#include "gameops.h"
#include "scene.h"

/* after the board */
#include "ending.h"
#include "stageclear.h"
#include "staffroll.h"

#endif /* TOPSHOP_H */
