#!/usr/bin/env python3
"""Fail closed when the OPL EE resident core would have an unusable stack.

Beta-39 map: _end_bss 0x93900 => 14,080 bytes.
Beta-82 map: _end_bss 0x96d40 => 704 bytes (LINKED, black screen on PS2).
The original crt0.S passes (0x97000 - _stack) to SetupThread().
"""
import argparse
import pathlib
import re
import sys

STACK_TOP = 0x97000
STACK_START = 0x84000


def check_map(content: str, minimum: int = 8192, stack_top: int = STACK_TOP) -> tuple[int, int]:
    if stack_top not in (STACK_TOP, 0x9B000, 0x9B800):
        raise ValueError("unsupported EE-core memory layout")
    match = re.search(r"0x([0-9a-fA-F]+)\s+_end_bss\s*=\s*\.", content)
    if not match:
        raise ValueError("linker map lacks _end_bss (cannot verify stack)")
    end = int(match.group(1), 16)
    if not STACK_START <= end <= stack_top:
        raise ValueError(f"invalid EE-core BSS end: 0x{end:08x}")
    return end, stack_top - end


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("map", type=pathlib.Path)
    p.add_argument("minimum", type=int, nargs="?", default=8192)
    p.add_argument("--stack-top", type=lambda value: int(value, 0),
                   default=STACK_TOP, choices=(STACK_TOP, 0x9B000, 0x9B800))
    p.add_argument("--warn-only", action="store_true",
                   help="Legacy IGS/debug build; report without rejecting")
    args = p.parse_args()
    try:
        data = args.map.read_text(encoding="utf-8")
        end, stack = check_map(data, args.minimum, args.stack_top)
    except (OSError, ValueError) as exc:
        print(f"[MAIZO EE CORE] ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"[MAIZO EE CORE] BSS end=0x{end:05x}, "
          f"SetupThread stack={stack} bytes, minimum={args.minimum} bytes, top=0x{args.stack_top:05x}")
    if stack < args.minimum:
        print("[MAIZO EE CORE] UNSAFE: resident core may corrupt its "
              "stack and black-screen all games", file=sys.stderr)
        return 0 if args.warn_only else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
