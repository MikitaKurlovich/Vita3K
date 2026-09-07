#!/usr/bin/env python3
"""Static EHABI check on a dump-elfs image (relocated p_vaddr).

Vita SELF often has no PT_ARM_EXIDX (filesz=0). The checker then reads
sce_module_info via e_entry (top 2 bits = PT_LOAD index).

Usage:
  tools/exidx_check.py path/to/0x81000000-..._eboot.elf --expect-exidx
  tools/exidx_check.py dumped.elf --pc 0x81157993 --pc 0x812dd7bc
"""

from __future__ import annotations

import argparse
import struct
import sys

PT_LOAD = 1
PT_ARM_EXIDX = 0x70000001
EXIDX_CANTUNWIND = 0x00000001


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


def e_entry(data: bytes) -> int:
    return struct.unpack_from("<I", data, 24)[0]


def va_to_off(loads, va: int):
    for seg in loads:
        if seg["vaddr"] <= va < seg["vaddr"] + seg["filesz"]:
            return seg["offset"] + (va - seg["vaddr"])
    return None


def prel31(value: int, place: int) -> int:
    offset = value & 0x7FFFFFFF
    if offset & 0x40000000:
        offset -= 0x80000000
    return (place + offset) & 0xFFFFFFFF


def scan_exidx(data: bytes, loads, vaddr: int, size: int) -> dict:
    n = size // 8
    cant = compact = extab = unsorted = 0
    prev = 0
    fn0 = 0
    for i in range(n):
        place = vaddr + i * 8
        off = va_to_off(loads, place)
        if off is None:
            return {"error": f"EXIDX VA 0x{place:08X} not in any PT_LOAD filesz"}
        word0, word1 = struct.unpack_from("<II", data, off)
        fn = prel31(word0, place)
        if i == 0:
            fn0 = fn
        if i and fn < prev:
            unsorted += 1
        prev = fn
        if word1 == EXIDX_CANTUNWIND:
            cant += 1
        elif word1 & 0x80000000:
            compact += 1
        else:
            extab += 1
    return {
        "entries": n,
        "rem": size % 8,
        "cantunwind": cant,
        "compact": compact,
        "extab": extab,
        "unsorted": unsorted,
        "fn0": fn0,
    }


def lookup_pc(data: bytes, loads, top: int, end: int, pc: int) -> str:
    pc &= ~1
    n = (end - top) // 8
    lo, hi = 0, n - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        place = top + mid * 8
        off = va_to_off(loads, place)
        if off is None:
            return f"pc=0x{pc:08X} error=exidx-va-missing"
        word0, word1 = struct.unpack_from("<II", data, off)
        fn = prel31(word0, place) & ~1
        if fn <= pc:
            best = (mid, fn, word1, place)
            lo = mid + 1
        else:
            hi = mid - 1
    if best is None:
        return f"pc=0x{pc:08X} hit=before-first"
    mid, fn, word1, place = best
    if word1 == EXIDX_CANTUNWIND:
        kind = "CANTUNWIND"
    elif word1 & 0x80000000:
        kind = f"compact=0x{word1:08X}"
    else:
        kind = f"extab=0x{prel31(word1, place + 4):08X}"
    return f"pc=0x{pc:08X} fn=0x{fn:08X} kind={kind}"


def module_info_exidx(data: bytes, loads) -> tuple[int, int] | None:
    if not loads:
        return None
    entry = e_entry(data)
    seg_i = entry >> 30
    off = entry & 0x3FFFFFFF
    if seg_i >= len(loads):
        return None
    mi_va = loads[seg_i]["vaddr"] + off
    mi_off = va_to_off(loads, mi_va)
    if mi_off is None:
        return None
    # sce_module_info_raw: exidx_top at 0x4C, exidx_end at 0x50 (offsets from this segment)
    ex_top_off, ex_end_off = struct.unpack_from("<II", data, mi_off + 0x4C)
    if ex_top_off == 0xFFFFFFFF or ex_end_off == 0xFFFFFFFF:
        return None
    base = loads[seg_i]["vaddr"]
    top, end = base + ex_top_off, base + ex_end_off
    if end <= top or (end - top) < 8:
        return None
    name = data[mi_off + 4 : mi_off + 31].split(b"\x00")[0].decode("ascii", "replace")
    print(f"sce_module_info name={name!r} va=0x{mi_va:08X} exidx=0x{top:08X}-0x{end:08X}")
    return top, end


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--expect-exidx", action="store_true")
    ap.add_argument("--pc", action="append", default=[], help="code VA to classify (repeatable)")
    args = ap.parse_args()
    data = read_elf32(args.elf)
    all_ph = list(phdrs(data))
    loads = [p for p in all_ph if p["type"] == PT_LOAD]
    exidx_ph = next((p for p in all_ph if p["type"] == PT_ARM_EXIDX and p["filesz"] >= 8), None)

    top = end = 0
    source = "none"
    if exidx_ph:
        top, end = exidx_ph["vaddr"], exidx_ph["vaddr"] + exidx_ph["filesz"]
        source = "PT_ARM_EXIDX"
        print(
            f"PT_ARM_EXIDX vaddr=0x{exidx_ph['vaddr']:08X} filesz=0x{exidx_ph['filesz']:X} "
            f"fileoff=0x{exidx_ph['offset']:X}"
        )
        owner = next((s for s in loads if s["vaddr"] <= exidx_ph["vaddr"] < s["vaddr"] + s["memsz"]), None)
        if owner is None:
            print("PT_ARM_EXIDX: not inside any PT_LOAD")
            return 3
        print(f"owner PT_LOAD vaddr=0x{owner['vaddr']:08X} memsz=0x{owner['memsz']:X}")
    else:
        print("PT_ARM_EXIDX: none")
        found = module_info_exidx(data, loads)
        if found:
            top, end = found
            source = "module_info"

    if not top:
        if args.expect_exidx:
            return 2
        return 0

    stats = scan_exidx(data, loads, top, end - top)
    if "error" in stats:
        print(stats["error"])
        return 3
    print(
        f"source={source} exidx entries={stats['entries']} rem={stats['rem']} "
        f"cantunwind={stats['cantunwind']} compact={stats['compact']} extab={stats['extab']} "
        f"unsorted={stats['unsorted']} first_fn=0x{stats['fn0']:08X}"
    )
    for pc_s in args.pc:
        print(lookup_pc(data, loads, top, end, int(pc_s, 0)))
    if stats["entries"] and stats["cantunwind"] == stats["entries"]:
        print("all entries CANTUNWIND — throws in this module will terminate")
        return 4
    return 0


if __name__ == "__main__":
    sys.exit(main())
