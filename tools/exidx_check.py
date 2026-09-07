#!/usr/bin/env python3
"""Static EHABI check on a dump-elfs image (relocated p_vaddr).

Usage:
  tools/exidx_check.py path/to/0x81000000-0x81xxxxxx_eboot.elf
  tools/exidx_check.py dumped.elf --expect-exidx

Prints EXIDX VA range, entry count, CANTUNWIND ratio, and whether PT_ARM_EXIDX
matches the first PT_LOAD-relative table.
"""

from __future__ import annotations

import argparse
import struct
import sys

PT_LOAD = 1
PT_ARM_EXIDX = 0x70000001
EXIDX_CANTUNWIND = 0x00000001
SHT_ARM_EXIDX = 0x70000001


def read_elf32(path: str) -> bytes:
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 1:
        raise SystemExit(f"{path}: not ELF32")
    return data


def phdrs(data: bytes):
    e_phoff = struct.unpack_from("<I", data, 28)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 42)
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, off
        )
        yield {
            "type": p_type,
            "offset": p_offset,
            "vaddr": p_vaddr,
            "filesz": p_filesz,
            "memsz": p_memsz,
        }


def prel31(value: int, place: int) -> int:
    offset = value & 0x7FFFFFFF
    if offset & 0x40000000:
        offset -= 0x80000000
    return (place + offset) & 0xFFFFFFFF


def scan_exidx(data: bytes, vaddr: int, filesz: int, fileoff: int) -> dict:
    n = filesz // 8
    cant = 0
    compact = 0
    for i in range(n):
        off = fileoff + i * 8
        word0, word1 = struct.unpack_from("<II", data, off)
        fn = prel31(word0, vaddr + i * 8)
        if word1 == EXIDX_CANTUNWIND:
            cant += 1
        elif word1 & 0x80000000:
            compact += 1
    return {"entries": n, "cantunwind": cant, "compact": compact, "fn0": prel31(struct.unpack_from("<I", data, fileoff)[0], vaddr) if n else 0}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--expect-exidx", action="store_true")
    args = ap.parse_args()
    data = read_elf32(args.elf)
    loads = [p for p in phdrs(data) if p["type"] == PT_LOAD]
    exidx_ph = next((p for p in phdrs(data) if p["type"] == PT_ARM_EXIDX), None)
    if not exidx_ph:
        print("PT_ARM_EXIDX: none")
        if args.expect_exidx:
            return 2
        return 0
    print(
        f"PT_ARM_EXIDX vaddr=0x{exidx_ph['vaddr']:08X} filesz=0x{exidx_ph['filesz']:X} "
        f"fileoff=0x{exidx_ph['offset']:X}"
    )
    owner = None
    for seg in loads:
        if seg["vaddr"] <= exidx_ph["vaddr"] < seg["vaddr"] + seg["memsz"]:
            owner = seg
            break
    if owner is None:
        print("PT_ARM_EXIDX: not inside any PT_LOAD")
        return 3
    print(f"owner PT_LOAD vaddr=0x{owner['vaddr']:08X} memsz=0x{owner['memsz']:X}")
    stats = scan_exidx(data, exidx_ph["vaddr"], exidx_ph["filesz"], exidx_ph["offset"])
    print(
        f"exidx entries={stats['entries']} cantunwind={stats['cantunwind']} "
        f"compact={stats['compact']} first_fn=0x{stats['fn0']:08X}"
    )
    if stats["entries"] and stats["cantunwind"] == stats["entries"]:
        print("all entries CANTUNWIND — throws in this module will terminate")
        return 4
    return 0


if __name__ == "__main__":
    sys.exit(main())
