"""Exercise the actual streaming C reader against generated ZIP fixtures."""
import ctypes
import pathlib
import subprocess
import tempfile
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    out = pathlib.Path(directory) / "db.so"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC", "-Iinclude",
                    "src/maizo_cheatdb.c", "-lz", "-o", str(out)], cwd=root, check=True)
    lib = ctypes.CDLL(str(out))
    reader = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_size_t)
    lib.maizoCheatZipFind.argtypes = [reader, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_char_p,
                                      ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p]
    lib.maizoCheatZipFind.restype = ctypes.c_int
    libc = ctypes.CDLL(None)
    libc.free.argtypes = [ctypes.c_void_p]

    def find(data, serial=b"SLUS_123.45", title=b"Game One"):
        @reader
        def read(ctx, offset, dst, length):
            if offset + length > len(data):
                return 0
            ctypes.memmove(dst, data[offset:offset+length], length)
            return 1
        pointer = ctypes.c_void_p()
        heading = ctypes.create_string_buffer(256)
        result = lib.maizoCheatZipFind(read, None, len(data), serial, title, ctypes.byref(pointer), heading)
        text = ctypes.string_at(pointer) if pointer else b""
        libc.free(pointer)
        return result, text, heading.value

    def archive(members, method=zipfile.ZIP_DEFLATED):
        p = pathlib.Path(directory) / "fixture.zip"
        with zipfile.ZipFile(p, "w", compression=method) as z:
            for name, content in members:
                z.writestr(name, content)
        return p.read_bytes()

    valid = "Master Code\n90000000 12345678\nMoney\n20123456 00000001"
    aggregate = '"Game One (PAL)"\nWrong Region\n20000000 00000002\n"Game One (USA)"\r\n' + valid + '\n"Other Game"\nWrong\n20000000 FFFFFFFF'
    for method in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
        blob = archive([("CheatDatabase.txt", aggregate)], method)
        result, text, heading = find(blob)
        assert result == 1 and b"Money" in text and b"Wrong" not in text, (result, text, heading)
        assert find(blob, b"SLES_123.45")[1].startswith(b"Wrong Region")
        assert find(blob, b"SLUS_999.99", b"Missing")[0] == 0
        assert find(blob[:-5])[0] < 0
        result, text, _ = find(archive([("CheatDatabase.txt", aggregate), ("CHT/SLUS_123.45.cht", valid)], method))
        assert result == 2 and b"Money" in text
        assert find(archive([("one.txt", aggregate), ("two.txt", '"Game One (USA)"\nAnother\n20123456 12345678')]))[0] == -2
        assert find(archive([("SLUS_999.99.cht", valid)]))[0] == 0
        # Deflate spans chunks and EOF can be missing a newline.
        result, text, _ = find(archive([("huge.txt", '"Other"\n' + 'x' * 8000 + '\n' + aggregate)]))
        assert result == 1 and b"Money" in text
    print("CheatDevice ZIP tests passed")
