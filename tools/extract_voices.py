"""Extract the characters' voices from the disc's XA files.

The voices the game plays with the script opcode play_xa (XA set 1: the CPU players' talk, events
0 and 1) are in A/GM00.XA-GM06.XA, two characters to a file, 16 voices each, one per XA channel.
The US disc stores those files as plain data: their contents are 2336-byte XA sectors (subheader,
then audio), but the disc holds them as ordinary data sectors, so the drive never plays them
(docs/fileio.md, "The XA files"). This script checks that each file is such a stream of XA
sectors and writes, into OUT (default voices/):

  GM00.XA-GM06.XA   the files, for the disc image: a CPU_TALK build (make CPU_TALK=1) writes them
                    to the disc as XA sectors, so the voices play
  wav/NN_Name_MM.wav  every voice decoded, character NN, voice MM (18.9 kHz mono): voices 0-14
                    are the talk's (the talk tables' XA channel column), 15 the one events 0 and
                    1 play

Replacing the files in OUT with other GM*.XA files of the same layout (2336-byte XA sectors, 32
channels) puts those voices on the disc instead. Read-only on the disc's files.

Usage: python3 -I tools/extract_voices.py [DISC_A_DIR] [-o OUT] [--no-wav]
       (defaults: disc/A, voices; tools/extract_disc.sh runs it)
"""
import argparse
import os
import struct
import sys

SECTOR = 2336               # an XA sector without sync and header: subheader 8, data 2324, EDC 4
FILES = ["GM%02d.XA" % i for i in range(7)]
CHANNELS = 32
VOICES_PER_CHAR = 16
# g_cpuNames (0x800A8AA8): character n's voices are in file n / 2, channels (n % 2) * 16 + 0..15
NAMES = ["Meower", "Maiko", "Dona", "Gummi", "Merlin", "Cindy", "Rob", "Patty", "Chat", "Oilow",
         "Bro", "Squosh", "Santa", "Colin"]

SM_EOF, SM_FORM2, SM_AUDIO = 0x80, 0x20, 0x04
# XA ADPCM prediction filters
K0 = (0, 60, 115, 98)
K1 = (0, 0, -52, -55)


def sectors(data):
    for k in range(len(data) // SECTOR):
        yield data[k * SECTOR:(k + 1) * SECTOR]


def check(name, data):
    """The file's audio sectors by channel; raises ValueError if it is not an XA stream."""
    if not data or len(data) % SECTOR:
        raise ValueError("%s: %d bytes, not a whole number of %d-byte XA sectors"
                         % (name, len(data), SECTOR))
    chans = {}
    for k, s in enumerate(sectors(data)):
        if s[0:4] != s[4:8]:
            raise ValueError("%s: sector %d: the two copies of the subheader differ" % (name, k))
        filenum, chan, submode, coding = s[0:4]
        if not submode & SM_AUDIO:
            continue
        if filenum != 1 or chan >= CHANNELS or not submode & SM_FORM2:
            raise ValueError("%s: sector %d: audio sector with file %d, channel %d, submode %#x"
                             % (name, k, filenum, chan, submode))
        if coding & 0x33:
            raise ValueError("%s: sector %d: coding %#x, not 4-bit mono" % (name, k, coding))
        chans.setdefault(chan, []).append(s)
    for c in range(CHANNELS):
        if c not in chans:
            raise ValueError("%s: no audio on channel %d" % (name, c))
        if not chans[c][-1][2] & SM_EOF:
            raise ValueError("%s: channel %d does not end with an end-of-file sector" % (name, c))
    return chans


def decode(secs):
    """XA ADPCM, 4-bit mono: 18 sound groups of 128 bytes a sector, 8 units of 28 samples each."""
    out = []
    old = older = 0
    for s in secs:
        for g in range(18):
            grp = s[8 + g * 128:8 + (g + 1) * 128]
            for u in range(8):
                param = grp[4 + u]
                shift = 12 - (param & 0x0F)
                f = (param >> 4) & 3
                k0, k1 = K0[f], K1[f]
                byte, hi = 16 + (u >> 1), (u & 1) * 4
                for j in range(28):
                    t = (grp[byte + j * 4] >> hi) & 0x0F
                    if t >= 8:
                        t -= 16
                    v = (t << shift if shift >= 0 else t >> -shift) + ((old * k0 + older * k1 + 32) >> 6)
                    v = -0x8000 if v < -0x8000 else 0x7FFF if v > 0x7FFF else v
                    out.append(v)
                    older, old = old, v
    return out


def write_wav(path, samples, rate):
    pcm = struct.pack("<%dh" % len(samples), *samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(pcm)) + pcm)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("disc_dir", nargs="?", default="disc/A", help="the disc's A/ directory (default disc/A)")
    ap.add_argument("-o", "--out", default="voices", help="output directory (default voices)")
    ap.add_argument("--no-wav", action="store_true", help="only the XA files, no WAV files")
    args = ap.parse_args()

    files = {}
    try:
        for name in FILES:
            with open(os.path.join(args.disc_dir, name), "rb") as f:
                data = f.read()
            files[name] = (data, check(name, data))
    except (OSError, ValueError) as e:
        sys.exit("extract_voices: %s" % e)

    os.makedirs(args.out, exist_ok=True)
    for name, (data, _) in files.items():
        with open(os.path.join(args.out, name), "wb") as f:
            f.write(data)
    count = 0
    if not args.no_wav:
        wav_dir = os.path.join(args.out, "wav")
        os.makedirs(wav_dir, exist_ok=True)
        for i, name in enumerate(FILES):
            chans = files[name][1]
            for c in range(CHANNELS):
                char = i * 2 + c // VOICES_PER_CHAR
                voice = c % VOICES_PER_CHAR
                rate = 18900 if chans[c][0][3] & 0x04 else 37800
                write_wav(os.path.join(wav_dir, "%02d_%s_%02d.wav" % (char, NAMES[char], voice)),
                          decode(chans[c]), rate)
                count += 1
    print("%s: %d voice files%s" % (args.out, len(files),
                                   ", %d voices in %s/wav" % (count, args.out) if count else ""))


if __name__ == "__main__":
    main()
