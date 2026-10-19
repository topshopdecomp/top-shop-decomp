# Sound

The code is `src/sound.c` (`0x8002B418`, `Snd_Init`, to `0x8002BED0`, `Snd_Unused`), declared in
`include/sound.h`. It uses libsnd (`Ss*`).

- **Set-up**: `Snd_Init(vabFile, seqFile)` (called at boot with `VAB.J`, file 0xDD, and `SEQ.J`)
  sets up libsnd, loads the sound bank (`Snd_LoadVab`: the header, entry 1 of `VAB.J`, is copied to
  `g_vabHeader`, the samples, entry 0, go to the sound chip) and the sequence archive `SEQ.J`, whose
  table of offsets `Snd_RelocateTable` turns into pointers (`g_seqTable`).
- **Music**: 5 BGM sequences (`BGM_SEQS`); `Bgm_Play(song, mode, volL, volR)`, `Bgm_Stop`. Songs
  0–22 are LZSS-packed in `SEQ.J`, unpacked into one of two buffers (`g_bgmBufs`) when played.
- **Sound effects**: SEQ sequences played in 5 round-robin slots (`SE_SLOTS`, `g_seSlots`,
  `SeSlot`): `Se_Play(id, volL, volR, keep)` (`keep`: do not take this slot over while another is
  free), `Se_Stop(id)`, `Se_StopSlot(slot)`. A sound effect's id is its entry in `SEQ.J`: entries
  23–58, not packed, opened where they are. The menus' sounds have names (`SE_CURSOR`, `SE_OK`,
  `SE_CANCEL`). The script opcode `play_se` (0x32) plays sound effect n + 0x17 (entry 23 + n), and
  the options screen's test plays the same 0–35 ([modes.md](modes.md)).
- `Snd_StopAll`, and `Snd_SetMono` (the options screen's mono setting, `g_optMono`).
- **XA voices** (streamed from the CD) are in `src/fileio.c`: `Xa_Play`, `Xa_RequestStop`,
  `Cd_IsXaPlaying` ([fileio.md](fileio.md)); the script opcodes `play_xa` (0x36), `stop_xa` (0x37)
  and `$x = xa_playing` (0x39).
