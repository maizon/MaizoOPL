#!/bin/sh
# Reproducible BearSSL 0.6 PS2 EE cross-build. MIT-licensed dependency.
# Verified archive SHA-256 is independently published by FreeBSD Ports.
set -eu
ROOT="thirdparty/bearssl-0.6"
LIB="$ROOT/libbearssl.a"
[ ! -f "$LIB" ] || exit 0
mkdir -p thirdparty
if [ ! -f "$ROOT/inc/bearssl.h" ]; then
    curl -fsSL --retry 3 https://bearssl.org/bearssl-0.6.tar.gz -o thirdparty/bearssl-0.6.tar.gz
    echo '6705bba1714961b41a728dfc5debbe348d2966c117649392f8c8139efc83ff14  thirdparty/bearssl-0.6.tar.gz' | sha256sum -c -
    tar -xzf thirdparty/bearssl-0.6.tar.gz -C thirdparty
fi
mkdir -p "$ROOT/ps2objs"
CC="${EE_CC:-mips64r5900el-ps2-elf-gcc}"
AR="${EE_AR:-mips64r5900el-ps2-elf-ar}"
for f in "$ROOT"/src/*/*.c; do
    [ -f "$f" ] || continue
    file=$(basename "$(dirname "$f")")-$(basename "$f" .c).o
    "$CC" -G0 -O2 -ffunction-sections -fdata-sections -I"$ROOT/inc" -I"$ROOT/src" -c "$f" -o "$ROOT/ps2objs/$file"
done
"$AR" rcs "$LIB" "$ROOT"/ps2objs/*.o
test -s "$LIB"
