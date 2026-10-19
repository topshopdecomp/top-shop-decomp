# CD and file loading, the sector heap, LZSS, image upload

The code is in `src/fileio.c` and `src/lzss.c`, declared in `include/fileio.h`, `include/lzss.h`
and `include/fileid.h`. Types: `CdRequest`, `FileHandle`, `SecBlock`, `XaTrack` in
`include/fileio.h`. Everything here is confirmed from the code unless it says otherwise.

## Files are loaded by index

The game never uses file names. Two static tables in the executable give every file's place on
the disc:

| Table | Address | Type | Holds |
|---|---|---|---|
| `g_fileLocTable` | `0x800A5750` | `CdlLOC[930]` | the position: BCD minute, second, sector, track |
| `g_fileSectorTable` | `0x800A65D8` | `int[930]` | the size in 2048-byte sectors |

Decoded and matched against the ISO directory of the `.bin`, **all 930 entries match a file, and the
sizes agree**. The order is the disc's (LBA) order. [filetable.txt](filetable.txt) lists index → name,
and `include/fileid.h` has the `FileId` enum.
`File_ReadAsync` and `File_ReadSync` take it, so calls read like `File_ReadSync(FILE_FONT0_FNT, ...)`.

**Not in the table** (17 files): `TIT_A.TIM`, `WAIWAI.TIM` and the 15 `*.XA` streams, the last 17
files in the disc's order. The XA voices are played through `Xa_Play(set, track)`, which takes the
track from `g_xaSets` (three sets of `XaTrack`: a file index, a length in units of 32 sectors, an XA
channel). Their file indices, 932–946, are the XA files' places in the disc's order (930 and 931
would be `TIT_A.TIM` and `WAIWAI.TIM`), as if the table went on past 929. It does not, so `Xa_Play`
reads the position from `g_fileSectorTable` instead, and the voice streams from no real position on
the disc: a bug of the original, kept (see the note at `Xa_Play`). A build with `CPU_TALK` takes the
real positions from a table of its own ([script.md](script.md)).

### The XA files

| Files (indices) | Set | Tracks | What they hold |
|---|---|---|---|
| `DM00.XA`–`DM03.XA` (932–935) | `g_xaSets[0]` | 99 | XA audio; nothing plays set 0 |
| `GM00.XA`–`GM06.XA` (936–942) | `g_xaSets[1]` | 224 | the characters' voices, played by `play_xa` (the talk scripts, events 0 and 1): track `char * 16 + n` is in `GM0<char / 2>.XA`, channel `(char % 2) * 16 + n` |
| `TL00.XA`–`TL03.XA` (943–946) | `g_xaSets[2]` | 98 | XA audio; nothing plays set 2 |

- Each file interleaves 32 channels (18.9 kHz mono). A track is one channel, from the start of its
  file; its length reaches past the channel's last audio sector (checked for all 421 tracks).
- **The `GM` files are stored as plain data.** The disc holds them as ordinary Mode 2 Form 1 sectors
  (submode 8, file 0, channel 0) whose 2048 bytes are, in sequence, the bytes of 2336-byte XA sectors
  (subheader, then audio), e.g. `GM00.XA`: 621 data sectors holding 544 XA sectors. The drive skips
  data sectors when it plays XA, so set 1 is silent even from the right position. The `DM` and `TL`
  files are real XA sectors (Form 2, audio, file 1).
- **The voices restored.** `tools/extract_voices.py` (run by `make extract`, or `make voices`) checks
  that each `GM` file is such a stream of XA sectors and writes it to `voices/`, with every voice
  decoded to a WAV file: `voices/wav/NN_Name_MM.wav` is character NN's voice MM (voices 0–14 are the
  talk's, the talk tables' XA channel column; 15 is the one events 0 and 1 play). A `CPU_TALK` disc
  image ([script.md](script.md)) takes the `GM` files from `voices/` and writes them as XA sectors,
  each followed by padding up to its original sector count, so that no file after it moves. Other
  `GM*.XA` files of the same layout put in `voices/` go on the disc instead.

## Functions

| Function | Address | What it does |
|---|---|---|
| `File_ReadAsync(id, dst, maxSectors, keep)` | `0x8001C958` | queues a read. With `dst` NULL it allocates from the sector heap. Returns a handle 0–31, or -1, -2 or -3. With `keep` = 0 the handle frees itself when the read completes (flags 1 → 0, in `Cd_Update` at `0x8001CF34`); `keep` ≠ 0 sets `FILEH_KEEP` (0x10), so the handle stays allocated for polling with `File_IsDone` until `File_ReleaseHandle` (the asynchronous loaders, e.g. `LoadJWithTim_Task`) |
| `File_SeekAsync(id, keep)` | `0x8001CB5C` | queues a seek without a read (`CdRequest.kind` 1) |
| `Xa_Play(set, track)` | `0x8001CCB4` | starts streaming an XA voice track, unless one is playing |
| `Cd_Update()` | `0x8001CD60` | the CD driver's state machine, pumped by `Sys_VSyncCallback` |
| `Cd_InitQueue()` | `0x8001D25C` | |
| `File_ReadSync(id, dst, maxSectors)` | `0x8001D2E0` | an asynchronous read, then spins on `Cd_Update` until it is done |
| `File_SeekSync(id)` | `0x8001D344` | the same for a seek |
| `File_IsDone(h)` | `0x8001D3A8` | |
| `File_ReleaseHandle(h)` | `0x8001D3E8` | does **not** free the heap block |
| `Xa_RequestStop()`, `Cd_IsXaPlaying()` | `0x8001D424`, `0x8001D460` | |
| `SecHeap_Alloc(n)`, `SecHeap_Free(b)`, `SecHeap_Shrink(b, n)`, `SecHeap_Init()` | `0x8001D48C`, `0x8001D6B4`, `0x8001D7BC`, `0x8001D8D4` | the sector heap (below) |
| `Lzss_Decompress(src, dst)` | `0x8001A8A8` | |
| `Tim_UploadStream(data, vramOffsets)` | `0x80024FA4` | uploads an image stream to VRAM (below) |
| `CdFlush`, `CdSync`, `CdReady` | `0x8009C8F0`, `0x8009C910`, `0x8009C930` | SDK functions, named from what they do |

Callers often pass `maxSectors = 0x514C8` (`FILE_WHOLE`) for "the whole file"; it is clamped to the
size in the table. `File_ReadSync` takes three arguments ([below](#check-the-arguments-a-callee-really-reads)).

**Helpers of the C version** (the original repeats this code inline): `File_ReadUnpacked`,
`File_LoadPackedSprites`, `File_LoadPackedImages` and `File_LoadTim` (read a TIM file
whole, upload it, free the block), in `src/fileio.c`. `.J` archives are read through
`ARCHIVE_ENTRY(file, i)` (`include/fileio.h`), e.g. in `LoadJWithTim_Task` and for the event card
pictures.

## The CD driver (`Cd_Update`)

`g_cdMode` (`0x800B2470`) says what it serves: 0 (`CD_MODE_DATA`) the request queue, 1
(`CD_MODE_XA`) an XA stream. It is guarded by `g_cdBusy`.

- **The request queue**: `g_cdQueue[16]` at `0x800CBEF0`, with its state, head and tail at
  `0x800CBFF0`, `0x800CBFF1` and `0x800CBFF2`. A read is `CdlSeekL`, then `CdSync` (the seek is
  retried on `CdlDiskError`), then `CdRead(n, buf, CdlModeSpeed)`, then `CdReadSync` (the read is
  retried on -1), and finally `FILEH_PENDING` is cleared. A request of `kind` 1 only seeks; they come
  from `File_SeekAsync`, which `ShopOpen_Task` uses to move the drive to `SH00L1_1.TIM` (the store
  pictures) when it has no picture to load. (`File_SeekSync` would queue one too, but nothing calls it.)
- **An XA stream** (`g_xaLoc`, `g_xaLength`, `g_xaChannel`, `g_xaState` at `0x800CC0C0`–`0x800CC0CF`;
  the length in units of 32 sectors): seek, then `CdlSetfilter` (file 1, the channel) and the
  serial volume up, then `CdRead2(Speed | RT | SF)`, then poll `CdlGetlocP` until past the end, then
  the volume down, `CdlPause`, and back to mode 0. `Xa_Play` sets it up; it is called by the script
  opcode `play_xa` (0x36, set 1), and by the options screen's voice test, whose row can't be
  reached ([modes.md](modes.md)).

## The sector heap

296 sectors of 2 KB at `0x8015C000`–`0x801EFFFF`:
- a bitmap `g_secHeapBitmap[10]` (`0x800CC0D0`);
- 32 block descriptors `g_secHeapBlocks` (`0x800CC0F8`);
- the counters of use and peak use, `g_secHeapUsed` / `g_secHeapPeak` (`0x800B2478` /
  `0x800B247C`).

Allocation is first-fit. A quirk: sector 295 can never end an allocation.

## LZSS

Confirmed by running `src/lzss.c` on the disc's data. It is Okumura's LZSS with N = 4096 and
F = 18, a ring buffer zero-filled at the start, writing from 0xFEE. The input begins with a `u32`,
its total size (the end marker). Decompressing the last entry of `CAL_01.J`, `CNT00.J`, `EVE_000.J`
and `KAO0P.J` gives complete image streams whose block lengths all check out.

Two details of the game's decompressor (`Lzss_Decompress`): it clears ring positions 0–0xFED only
(0xFEE–0xFFF keep what the last call left there), and it stops as soon as the next item would end
past the total size. `tools/tss.py` packs edited talk scripts to match ([script.md](script.md)).

## Image streams (`Tim_UploadStream`)

A sequence of records, ended by any word that is not `0x10` or `0x11`:
- `0x10`: a standard TIM (flags, an optional CLUT block, the pixel block). The pixel rectangle is
  offset by `vramOffsets[0..1]`, the CLUT rectangle by `vramOffsets[2..3]`.
- `0x11`: one block only (the game uses it for palette rows, e.g. 256×1 at VRAM y = 489), offset by
  `vramOffsets[2..3]`.

## `LoadJWithTim_Task` (`0x80042EC4`)

A task that loads a `.J` archive (the file index in its work area: `work[0]`,
`LoadJWithTimWork.file`) for an event, a story scene or a calendar page:
1. reads the file whole, keeping the handle (`g_scriptBlock` gets the block);
2. if the archive has more than one entry, LZSS-decompresses entry 2 (the last, an image stream)
   into a temporary 80-sector block, uploads it, frees that block, and shrinks the file's block to
   drop the compressed entry;
3. publishes the entries: entry 0, the script, in `g_scriptData` (`0x800B269C`); entry 1, the scene
   images, in `g_sceneImages` (`0x800B2760`) and `g_sceneSheet.frames` (`g_sceneSheet` at
   `0x80117C60`).

## Check the arguments a callee really reads

`File_ReadSync` (`0x8001D2E0`) takes **three** arguments and always calls `File_ReadAsync(..., 0)`
(`move a3,zero` in the delay slot at `0x8001D300`). Its callers leave unrelated values in `$a3` (e.g.
`g_spriteGroups[3].base` at `0x80048118`; `Sys_LoadFonts` calls it with three), which can make it
look like it takes a fourth parameter. Check a callee's real use of `$a0`–`$a3` before trusting a
decompiled parameter list.

## Answered questions

- Who sets `g_cdMode = 1` and the XA request: `Xa_Play`, from the script opcode `play_xa` (and the
  options screen's unreachable voice test); the track comes from `g_xaSets`.
- Who queues `CdRequest.kind == 1`: `File_SeekAsync`, used by `ShopOpen_Task` (`File_SeekSync` is
  never called).
- What `0x800B269C`, `0x800B2760` and `0x80117C60` point to: `g_scriptData` (entry 0 of the `.J`),
  `g_sceneImages` (entry 1) and `g_sceneSheet` (whose frames are entry 1).
