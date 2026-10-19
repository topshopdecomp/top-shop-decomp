#!/bin/sh
# Install the build toolchain into tools/ (nothing system-wide). Idempotent.
#   binutils 2.47 (mipsel cross, built from source, GNU-signed)
#   decompals/old-gcc 0.17: the PsyQ-patched GCC 2.8.1 (cc1 + cpp)
#   maspsx (pinned commit): ASPSX-compatible assembler front end
#   splat64 0.50.0 (+ spimdisasm, rabbitizer) in tools/venv
#   mkpsxiso / dumpsxiso 2.30: the release's Linux build, SHA-256 checked, in tools/mkpsxiso
set -eu
cd "$(dirname "$0")"
T=$(pwd)
JOBS=$(nproc 2>/dev/null || echo 2)

BINUTILS_VER=2.47
OLDGCC_REL=0.17
GCC_VER=2.8.1
MASPSX_COMMIT=7686f845a1
SPLAT_VER=0.50.0
MKPSXISO_VER=2.30
MKPSXISO_SHA256=8e387e6db51dee3eb7cb08f4d59293a24c48f58bd541631acf9301d78015a1fc   # mkpsxiso-2.30-Linux.zip

mkdir -p dl bin

# ---- binutils ----
if [ ! -x bin/mipsel-linux-gnu-as ]; then
    echo "building binutils $BINUTILS_VER (a few minutes)"
    mkdir -p dl/binutils build/binutils
    (
        cd dl/binutils
        [ -f binutils-$BINUTILS_VER.tar.xz ] || curl -sSfLO https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz
        [ -f binutils-$BINUTILS_VER.tar.xz.sig ] || curl -sSfLO https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz.sig
        [ -f gnu-keyring.gpg ] || curl -sSfLO https://ftp.gnu.org/gnu/gnu-keyring.gpg
        gpgv --keyring ./gnu-keyring.gpg binutils-$BINUTILS_VER.tar.xz.sig binutils-$BINUTILS_VER.tar.xz
        [ -d binutils-$BINUTILS_VER ] || tar xf binutils-$BINUTILS_VER.tar.xz
    )
    (
        cd build/binutils
        "$T/dl/binutils/binutils-$BINUTILS_VER/configure" --prefix="$T/binutils" \
            --target=mipsel-linux-gnu --disable-nls --disable-werror --disable-gdb \
            --disable-sim --disable-gprofng --disable-gold --without-zstd >configure.log 2>&1
        make -j"$JOBS" all-gas all-ld all-binutils >make.log 2>&1
        make install-gas install-ld install-binutils >install.log 2>&1
    )
    for t in as ld objcopy objdump nm readelf strip ar; do
        ln -sf "$T/binutils/bin/mipsel-linux-gnu-$t" "bin/mipsel-linux-gnu-$t"
    done
fi

# ---- old GCC (PsyQ build) ----
if [ ! -x gcc-$GCC_VER-psx/cc1 ]; then
    mkdir -p dl/gcc-$GCC_VER gcc-$GCC_VER-psx
    (cd dl/gcc-$GCC_VER && curl -sSfLO https://github.com/decompals/old-gcc/releases/download/$OLDGCC_REL/gcc-$GCC_VER-psx.tar.gz)
    tar xzf dl/gcc-$GCC_VER/gcc-$GCC_VER-psx.tar.gz -C gcc-$GCC_VER-psx
fi

# ---- maspsx ----
if [ ! -f maspsx/maspsx.py ] && [ ! -f maspsx/maspsx/__init__.py ]; then
    git clone -q https://github.com/mkst/maspsx.git maspsx
    git -C maspsx checkout -q $MASPSX_COMMIT
fi

# ---- splat (uv if it is there, else python3 -m venv + pip) ----
if [ ! -x venv/bin/splat ]; then
    if command -v uv >/dev/null 2>&1; then
        uv venv -q venv
        uv pip install -q --python venv/bin/python "splat64[mips]==$SPLAT_VER"
    else
        python3 -m venv venv
        venv/bin/pip install -q "splat64[mips]==$SPLAT_VER"
    fi
fi

# ---- mkpsxiso / dumpsxiso (disc images) ----
if [ ! -x mkpsxiso/bin/mkpsxiso ] || [ ! -x mkpsxiso/bin/dumpsxiso ]; then
    zip=mkpsxiso-$MKPSXISO_VER-Linux.zip
    mkdir -p dl/mkpsxiso
    (
        cd dl/mkpsxiso
        [ -f $zip ] || curl -sSfLO https://github.com/Lameguy64/mkpsxiso/releases/download/v$MKPSXISO_VER/$zip
        if ! echo "$MKPSXISO_SHA256  $zip" | sha256sum -c --quiet - >/dev/null 2>&1; then
            rm -f $zip
            echo "tools/dl/mkpsxiso/$zip is not the release (SHA-256); removed it, run setup again"
            exit 1
        fi
        rm -rf x
        python3 -I -m zipfile -e $zip x
    )
    rm -rf mkpsxiso
    mv dl/mkpsxiso/x/mkpsxiso-$MKPSXISO_VER-Linux mkpsxiso
    chmod +x mkpsxiso/bin/mkpsxiso mkpsxiso/bin/dumpsxiso
fi

echo "toolchain ready:"
bin/mipsel-linux-gnu-as --version | head -1
ls gcc-$GCC_VER-psx
venv/bin/python -c "import splat; print('splat', splat.__version__)"
mkpsxiso/bin/mkpsxiso --help 2>&1 | head -1
