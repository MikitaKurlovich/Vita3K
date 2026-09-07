# Upstream PR-1: EXIDX VA + GetModuleInfo harden

## Summary

`sce_module_info` stores EXIDX/EXTAB as offsets from the module_info segment (vitasdk `sce_module_info_raw`), the same way as `module_start` / `tls_start`. Vita3K already relocated start/tls to process VAs but left EXIDX as raw offsets. LLE `libc.suprx` then calls `sceKernelGetModuleInfoByAddr` and walks tables at a junk address, so a caught C++ throw becomes `terminate` / `abort`.

This change:

- relocates EXIDX/EXTAB with `kernel/module_info.h` (offset 0 is valid; sentinel is only `0xffffffff`; exclusive-end may equal `seg_size`)
- drops degenerate pairs (`end-top < 8`, including old-SDK fake `(0,1)` from vita-toolchain `sce-elf.c`)
- maps `PT_ARM_EXIDX` via original `p_vaddr` of the owning PT_LOAD (`p_vaddr == 0` is valid for relocatable ELF; missing phdr is `p_filesz == 0`)
- on phdr vs module_info mismatch, prefers the normalized PT_ARM_EXIDX range (`source=phdr-preferred`)
- copies `SceKernelModuleInfo` under one `kernel.mutex` lock (`copy_module_info_by_addr`), `Ptr<>` + `is_valid_addr_range`, `memcpy(min(guest size, sizeof))`; if the guest size field is non-zero it is restored after the copy
- uses a strict upper bound `addr < vaddr+memsz` in `find_module_by_addr`

EHABI: ARM IHI0038 §5 (8-byte EXIDX entries).

## What this is not

No game stubs, no swallow of `__cxa_throw`, no AppUtil/NGS lab hacks.

## Tests

```
ctest -R kernel --output-on-failure
```

Host `kernel-tests` covers relocate / normalize / phdr / `segment_contains` / guest copy. Pi Docker builds with `BUILD_TESTING=OFF`; run tests on the host.

## Repro without a commercial image (F2)

Homebrew built with an old SDK that emits fake exidx offsets `(0, 1)`:

- log: `[EHABI] module=... exidx=none reason=degenerate-pair hint="module built with old SDK fake exidx; expected, harmless"`
- process must not crash at load

HLE-libc game (no `libc.suprx`): log line still prints; unwind NIDs may stay as HLE `UNIMPLEMENTED()` (`return 0`). That is unchanged.

## Regression matrix

| Guest | Mac OpenGL | Pi Vulkan | x86 CI |
|---|---|---|---|
| PCSE01056 + LLE libc | New Game, `[EHABI] relocated=yes` for eboot and libc | same | n/a (no image on CI) |
| old-SDK homebrew + LLE libc | `degenerate-pair`, no crash | same | `ctest -R kernel` |
| HLE-libc game | log only, behavior unchanged | same | `ctest -R kernel` |

## Logs

Before: no `[EHABI]` line, or EXIDX printed as a small offset such as `0x83A008`.

After:

```
[EHABI] module=eboot exidx=0x........-0x........ extab=0x........-0x........ relocated=yes
[EHABI] module=libc.suprx exidx=0x........-0x........ extab=0x........-0x........ relocated=yes
```

Related: Vita3K/Vita3K#305, Vita3K/Vita3K#3047.

## Checklist

- [ ] `clang-format` (`.github/workflows/format.yml`)
- [ ] `ctest -R kernel`
- [ ] clang preset `ci-linux-clang-appimage` before merge
- [ ] no lab files (AppUtil/NGS stubs, flight recorder)
