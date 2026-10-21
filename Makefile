# Board Game - Top Shop (USA), SLUS-01333: build the game from the C sources, with the assets of
# your own disc image.
#
#   make setup        install the toolchain into tools/ (tools/setup.sh), once
#   make              the full build:
#                       1. extract your disc image into disc/ (once; see `make extract`)
#                       2. split the original EXE's PsyQ library code into asm/ (once)
#                       3. compile and link build/SLUS_013.33
#                       4. make the disc image build/topshop.bin / build/topshop.cue, with
#                          the scripts built from scripts/ (the talk in English)
#   make extract      step 1 alone: CUE=path/to/image.cue, or the one .cue in rom/; the .bin
#                     must have the MD5 below (ROM_MD5) or the build stops with an error
#   make split        step 2 alone: regenerate asm/ from disc/SLUS_013.33 with splat
#   make exe          step 3 alone
#   make scripts      assemble scripts/ (edited or not) into the disc files that hold the
#                     scripts, in scripts/build/ (needs disc/, see step 1; scripts/README.md)
#   make CPU_TALK=1 ...  a build that plays the CPU players' talk scripts, which the retail game
#                     never shows, with their voices (docs/script.md)
#   make voices       write voices/ again: the characters' voices, as XA files for the CPU_TALK
#                     disc image and as WAV files (make extract does it too)
#   make clean        remove the build output
#   make distclean    also remove disc/, disc.xml, voices/, asm/ and the files splat generates

# every stage of a pipeline must succeed (cpp | cc1 | maspsx | as)
SHELL       := /bin/bash
.SHELLFLAGS := -o pipefail -ec
# a failed compile must not leave a partial object behind
.DELETE_ON_ERROR:

BUILD   := build
TOOLS   := tools
CROSS   := $(TOOLS)/bin/mipsel-linux-gnu-
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
SPLAT   := $(TOOLS)/venv/bin/splat
MKPSXISO := $(TOOLS)/mkpsxiso/bin/mkpsxiso

ASFLAGS := -EL -march=r3000 -mtune=r3000 -no-pad-sections -G0 -Iinclude

# C: PsyQ-era GCC 2.8.1 (decompals/old-gcc) -> maspsx (ASPSX behavior) -> GNU as.
# -G0: small data stays in the generated assembly, C reaches it by absolute address.
# -fsigned-char: this GCC defaults to unsigned char; the C here uses plain char as signed.
CPP      := $(TOOLS)/gcc-2.8.1-psx/cpp
CC1      := $(TOOLS)/gcc-2.8.1-psx/cc1
MASPSX   := python3 $(TOOLS)/maspsx/maspsx.py
CPPFLAGS := -nostdinc -Iinclude -undef -lang-c -D__GNUC__=2 -D__GNUC_MINOR__=8 \
            -Dmips -D__mips__ -D__mips -D_MIPSEL -D__psx__ -D_LANGUAGE_C -DLANGUAGE_C \
            -DINCLUDE_ASM_USE_MACRO_INC=1   # global jump-table labels in INCLUDE_ASM functions
CFLAGS   := -quiet -O2 -G0 -mips1 -mcpu=3000 -mgas -msoft-float -fno-builtin -fsigned-char \
            -Werror-implicit-function-declaration
MASPSXFLAGS := --aspsx-version=2.79 --expand-div

# make CPU_TALK=1: a build that plays the CPU players' talk at the start and the end of their turns,
# which the retail game never shows (src/board.c), and streams the XA voices from the XA files'
# real positions (src/fileio.c), without which the talk never ends. The disc stores the voices'
# files (A/GM*.XA) as plain data, which the drive does not play, so the CPU_TALK disc image puts
# them on the disc as XA: from voices/, which tools/extract_voices.py makes from disc/A (make
# extract runs it).
# The build's options are kept in $(BUILD)/options.txt, which the C objects depend on, so
# changing them rebuilds the C.
VOICES := voices
ifeq ($(CPU_TALK),1)
CPPFLAGS += -DCPU_TALK
DISC_VOICES := $(VOICES)/GM00.XA
MKDISCFLAGS := --voices $(VOICES)
endif
BUILD_OPTIONS := $(BUILD)/options.txt
$(shell mkdir -p $(BUILD) && { echo 'CPU_TALK=$(CPU_TALK)' | cmp -s - $(BUILD_OPTIONS) 2>/dev/null || echo 'CPU_TALK=$(CPU_TALK)' > $(BUILD_OPTIONS); })

TARGET   := $(BUILD)/SLUS_013.33
ELF      := $(BUILD)/topshop.elf
LDSCRIPT := $(BUILD)/topshop.final.ld
IMAGE    := $(BUILD)/topshop.bin

# MD5 of the original disc image's .bin: Board Game - Top Shop (USA), SLUS-01333
ROM_MD5  := e886c77d61880e3ff655af46ac82c323

ASM_FILES := $(shell find asm -name '*.s' -not -path 'asm/nonmatchings/*' 2>/dev/null)
C_FILES   := $(wildcard src/*.c)
O_FILES   := $(patsubst %.s,$(BUILD)/%.o,$(ASM_FILES)) $(patsubst %.c,$(BUILD)/%.o,$(C_FILES))

.PHONY: all setup extract split exe image voices scripts clean distclean toolchain
.NOTPARALLEL: all

# the steps run in sub-makes: asm/ has to exist before the object list above is worked out
all: toolchain
	@[ -f disc/SLUS_013.33 ] && [ -f disc.xml ] || $(MAKE) --no-print-directory extract
	@[ -f asm/sdk.s ] && [ -f asm/topshop.ld ] || $(MAKE) --no-print-directory split
	@$(MAKE) --no-print-directory image

setup:
	$(TOOLS)/setup.sh

toolchain:
	@[ -x $(CC1) ] && [ -x $(AS) ] && [ -x $(SPLAT) ] && [ -f $(TOOLS)/maspsx/maspsx.py ] && \
	    [ -x $(MKPSXISO) ] && [ -x $(TOOLS)/mkpsxiso/bin/dumpsxiso ] || \
	    { echo "the toolchain is not installed: run make setup first"; exit 1; }

# your disc image -> disc/ and disc.xml (tools/extract_disc.sh: checks the .bin's MD5, runs
# dumpsxiso, checks the extracted files)
extract:
	$(TOOLS)/extract_disc.sh $(ROM_MD5) $(if $(CUE),"$(CUE)")

# asm/ is regenerated from scratch from the original EXE (only the PsyQ library and the
# header stay assembly). Labels the C code refers to must exist in the data even when no
# assembly uses them. splat's linker script goes into asm/ with the rest of what it made, so
# that make clean (which removes build/) does not take it away.
split: toolchain
	@[ -f disc/SLUS_013.33 ] || { echo "no disc/SLUS_013.33: run make extract first"; exit 1; }
	rm -rf asm
	grep -ohE '\bD_8[0-9A-F]{7}\b' src/*.c include/*.h | sort -u | \
	    sed -E 's/^D_(.*)$$/D_\1 = 0x\1;/' > config/symbol_addrs_c.txt
	$(SPLAT) split config/topshop.yaml
	mv $(BUILD)/topshop.ld asm/topshop.ld

exe: $(TARGET)

image: $(IMAGE)

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# INCLUDE_ASM pulls asm/nonmatchings/<file>/*.s in at assembly time
$(BUILD)/src/%.o: src/%.c $(wildcard include/*.h) Makefile $(BUILD_OPTIONS)
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) $< | $(CC1) $(CFLAGS) | $(MASPSX) $(MASPSXFLAGS) | $(AS) $(ASFLAGS) -o $@

# splat's script with the game's C objects in the order of config/c_files.txt (the sources
# are organized by subsystem, not by the original's split; tools/ldobjects.py), and _gp tied
# to .sdata (PsyQ puts $$gp at the start of .sdata)
asm/topshop.ld:
	@echo "no asm/topshop.ld: run make split (make runs it when asm/ is incomplete)"; exit 1

$(LDSCRIPT): asm/topshop.ld Makefile config/c_files.txt $(TOOLS)/ldobjects.py $(C_FILES)
	python3 -I $(TOOLS)/ldobjects.py $< config/c_files.txt | \
	sed -e 's/^\( *\)_gp = 0x[0-9A-Fa-f]*;/\1_gp = main_SDATA_START;/' > $@

# only addresses no object defines (see tools/filter_undefined.py)
$(BUILD)/undefined.ld: $(O_FILES) config/undefined_syms_auto.txt config/undefined_funcs_auto.txt
	python3 -I $(TOOLS)/filter_undefined.py $(CROSS)readelf $@ \
	    config/undefined_syms_auto.txt config/undefined_funcs_auto.txt -- $(O_FILES)
	python3 -I $(TOOLS)/check_layout.py $@

$(ELF): $(O_FILES) $(LDSCRIPT) $(BUILD)/undefined.ld
	$(LD) -nostdlib --no-check-sections --emit-relocs -Map $(BUILD)/topshop.map \
	    -T $(LDSCRIPT) -T $(BUILD)/undefined.ld -o $@

$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $< $@
	truncate -s %2048 $@
	python3 -I $(TOOLS)/fixexe.py $(CROSS)nm $< $@

# the original disc's files and layout (disc.xml) with the EXE from the build, the scripts built
# from scripts/ (tools/tss.py build: the talk scripts are in English) and, in a CPU_TALK build, the
# voices of voices/ as XA
SCRIPT_SOURCES := $(wildcard scripts/*/*.tss)
$(IMAGE): $(TARGET) disc.xml $(BUILD_OPTIONS) $(DISC_VOICES) $(SCRIPT_SOURCES)
	python3 -I $(TOOLS)/tss.py build
	python3 -I $(TOOLS)/mkdisc.py --scripts scripts/build $(MKDISCFLAGS) disc.xml $(TARGET) $(BUILD)/disc.xml
	$(MKPSXISO) -y -q $(BUILD)/disc.xml
	@echo "disc image: $(BUILD)/topshop.cue / $(BUILD)/topshop.bin"

scripts:
	@[ -d disc/A ] || { echo "no disc/A: run make extract first"; exit 1; }
	python3 -I $(TOOLS)/tss.py build

# the voices' files as XA sectors, and every voice as a WAV file (tools/extract_voices.py);
# tools/extract_disc.sh makes them when it extracts the disc
voices:
	@[ -d disc/A ] || { echo "no disc/A: run make extract first"; exit 1; }
	python3 -I $(TOOLS)/extract_voices.py disc/A -o $(VOICES)

$(VOICES)/GM00.XA:
	@$(MAKE) --no-print-directory voices

clean:
	rm -rf $(BUILD) scripts/build

distclean: clean
	rm -rf disc disc.xml $(VOICES) asm config/undefined_funcs_auto.txt config/undefined_syms_auto.txt \
	    include/include_asm.h include/macro.inc include/labels.inc include/gte_macros.inc
