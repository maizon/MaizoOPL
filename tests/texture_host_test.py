#!/usr/bin/env python3
"""Exercise the real PNG decoder on the host; GS allocation sizes are stubbed.

This validates decoding/cleanup, not GS rendering or an EE build. Requires a C
compiler with ASan/UBSan, pkg-config and the host libpng development package.
"""
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]


def chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data)))


def png(width, height, body):
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(body)) + chunk(b"IEND", b""))


def main():
    with tempfile.TemporaryDirectory(prefix="maizo-texture-") as directory:
        work = Path(directory)
        include = work / "include"
        include.mkdir()
        # Only the GS storage descriptor/size API is replaced; texLoadAll,
        # libpng transforms, bounds checks and error cleanup are production code.
        (include / "opl.h").write_text(r'''
#ifndef TEST_OPL_H
#define TEST_OPL_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef struct {
    int Width, Height, PSM, ClutPSM, TBW, Filter, ClutStorageMode, Delayed;
    void *Mem, *Clut;
    unsigned int Vram, VramClut;
} GSTEXTURE;
#define GS_PSM_CT32 0
#define GS_PSM_CT24 1
#define GS_PSM_T8 19
#define GS_PSM_T4 20
#define GS_FILTER_LINEAR 1
#define GS_CLUT_STORAGE_CSM1 0
#define LOG(...) ((void)0)
u32 gsKit_texture_size_ee(int width, int height, int psm);
u32 gsKit_texture_size(int width, int height, int psm);
#endif
''')
        (include / "util.h").write_text("")
        (include / "ioman.h").write_text("")
        source = (ROOT / "src/textures.c").read_text()
        names = re.findall(r"^extern void \*(\w+);", source, re.MULTILINE)
        (work / "storage.c").write_text(
            '#include "include/opl.h"\n'
            + "".join("void *" + name + ";\n" for name in names)
            + '''
u32 gsKit_texture_size_ee(int w, int h, int psm) {
    (void)psm; return (u32)w * h * 4;
}
u32 gsKit_texture_size(int w, int h, int psm) {
    return gsKit_texture_size_ee(w, h, psm);
}
''')
        valid = png(2, 3, (b"\0" + bytes([255, 0, 128, 255]) * 2) * 3)
        (work / "valid.png").write_bytes(valid)
        (work / "huge.png").write_bytes(png(1025, 1, b"\0" + b"\xff" * 4100))
        for depth in (4, 8):
            header = struct.pack(">IIBBBBB", 2, 3, depth, 3, 0, 0, 0)
            pixels = (b"\0\1" if depth == 4 else b"\0\0\1") * 3
            palette = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                       + chunk(b"PLTE", bytes([255, 0, 0, 0, 255, 0]))
                       + chunk(b"tRNS", bytes([255, 64]))
                       + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))
            (work / f"palette{depth}.png").write_bytes(palette)
            # The palette and texture have been allocated when IDAT CRC fails.
            corrupted = bytearray(palette)
            corrupted[-13] ^= 1
            (work / f"palette_bad{depth}.png").write_bytes(corrupted)
        odd = (b"\x89PNG\r\n\x1a\n"
               + chunk(b"IHDR", struct.pack(">IIBBBBB", 3, 2, 4, 3, 0, 0, 0))
               + chunk(b"PLTE", bytes([255, 0, 0, 0, 255, 0]))
               + chunk(b"IDAT", zlib.compress(b"\0\x01\0" * 2))
               + chunk(b"IEND", b""))
        (work / "odd_palette.png").write_bytes(odd)
        with (work / "compressed_limit.png").open("wb") as image:
            image.truncate(8 * 1024 * 1024 + 1)
        for i in range(len(valid)):
            (work / f"bad{i}.png").write_bytes(valid[:i])
        (work / "test.c").write_text(r'''
#include "include/opl.h"
#include "include/textures.h"
#include <assert.h>
int main(void) {
    GSTEXTURE texture;
    assert(texDiscoverLoad(&texture, "valid", -1) == 0);
    assert(texture.Width == 2 && texture.Height == 3 && texture.Mem);
    unsigned char *pixels = texture.Mem;
    assert(pixels[0] == 255 && pixels[1] == 0 && pixels[2] == 128 && pixels[3] == 127);
    texFree(&texture);
    assert(texDiscoverLoad(&texture, "huge", -1) < 0);
    texFree(&texture);
    assert(texDiscoverLoad(&texture, "valid", TEXTURES_COUNT) < 0);
    assert(texDiscoverLoad(&texture, "compressed_limit", -1) < 0);
    assert(texDiscoverLoad(&texture, "odd_palette", -1) == 0);
    assert(texture.Width == 3 && texture.Height == 2 && texture.PSM == GS_PSM_T8);
    pixels = texture.Mem;
    assert(pixels[0] == 0 && pixels[1] == 1 && pixels[2] == 0);
    texFree(&texture);
    for (int depth = 4; depth <= 8; depth += 4) {
        char name[64];
        snprintf(name, sizeof(name), "palette%d", depth);
        assert(texDiscoverLoad(&texture, name, -1) == 0);
        assert(texture.Mem && texture.Clut);
        texFree(&texture);
        snprintf(name, sizeof(name), "palette_bad%d", depth);
        assert(texDiscoverLoad(&texture, name, -1) < 0);
        assert(!texture.Mem && !texture.Clut);
    }
    for (int repeat = 0; repeat < 5; repeat++) {
        for (int i = 0; i < VALID_SIZE; i++) {
            char name[64];
            snprintf(name, sizeof(name), "bad%d", i);
            assert(texDiscoverLoad(&texture, name, -1) < 0);
            assert(!texture.Mem && !texture.Clut);
        }
    }
    puts("PNG: RGBA/palettes, truncation, CRC errors, size limits and cleanup passed");
    return 0;
}
'''.replace("VALID_SIZE", str(len(valid))))
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "libpng"], text=True))
        compiler = shlex.split(os.environ.get("CC", "cc"))
        subprocess.run(compiler + ["-std=c99", "-fsanitize=address,undefined",
                                  "-g", "-I" + str(work), "-I" + str(ROOT),
                                  str(ROOT / "src/textures.c"), str(work / "storage.c"),
                                  str(work / "test.c"), "-o", str(work / "test")]
                       + flags, check=True)
        result = subprocess.run([str(work / "test")], cwd=work,
                                capture_output=True, text=True)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        print(result.stdout, end="")


if __name__ == "__main__":
    main()
