// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <kernel/relocation.h>
#include <kernel/types.h>
#include <mem/functions.h>
#include <util/log.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

enum class ModuleInfoOffsetKind {
    Start,
    ExclusiveEnd
};

// sce_module_info stores EXIDX/EXTAB as offsets from the module_info segment,
// same as module_start/tls_start. Sentinel is only 0xffffffff; offset 0 is a
// valid table at the start of the segment (vitasdk sce_module_info_raw).
// Exclusive-end fields may equal seg_size (one-past-last); start may not.
inline Address relocate_module_info_offset(uint32_t offset, Address seg_base, uint64_t seg_size,
    ModuleInfoOffsetKind kind = ModuleInfoOffsetKind::Start) {
    if (offset == 0xffffffff)
        return 0;
    const bool oob = (kind == ModuleInfoOffsetKind::ExclusiveEnd) ? (offset > seg_size) : (offset >= seg_size);
    if (oob) {
        LOG_ERROR("[EHABI] module_info offset 0x{:08X} out of segment 0x{:08X}+0x{:X} hint=\"C++ throw in this module will terminate; run with --dump-elfs and attach log\"",
            offset, seg_base, seg_size);
        return 0;
    }
    const uint64_t va = static_cast<uint64_t>(seg_base) + offset;
    if (va > 0xffffffffull) {
        LOG_ERROR("[EHABI] module_info offset 0x{:08X} + seg_base 0x{:08X} overflows 32-bit VA", offset, seg_base);
        return 0;
    }
    return static_cast<Address>(va);
}

struct EhabiRange {
    Address top = 0;
    Address end = 0;
    const char *reason = "none";
};

// EHABI IHI0038 §5: each EXIDX entry is 8 bytes. Old SDK toolchains write a
// fake pair at offsets (0, 1) (vita-toolchain sce-elf.c); after relocate that
// is a 1-byte span and must be dropped, not treated as "offset 0 means null".
inline EhabiRange normalize_ehabi_range(Address top, Address end) {
    if (top == 0 && end == 0)
        return { 0, 0, "no-tables" };
    if (top == 0 || end == 0 || end <= top)
        return { 0, 0, "degenerate" };
    if ((end - top) < 8)
        return { 0, 0, "degenerate-pair" };
    if (((end - top) % 8) != 0) {
        LOG_WARN("[EHABI] exidx length 0x{:X} is not a multiple of 8; leaving as-is for libc", end - top);
    }
    return { top, end, "ok" };
}

inline bool segment_contains(const SceKernelSegmentInfo &seg, Address addr) {
    const Address vaddr = seg.vaddr.address();
    return addr >= vaddr && addr < vaddr + seg.memsz;
}

inline bool module_has_loaded_code(const SceKernelModuleInfo &info) {
    for (const auto &seg : info.segments) {
        if (seg.size && seg.memsz)
            return true;
    }
    return false;
}

inline bool path_basename_is(std::string_view path, std::string_view name) {
    const auto pos = path.find_last_of("/\\:");
    const auto base = pos == std::string_view::npos ? path : path.substr(pos + 1);
    return base == name;
}

inline bool module_contains_addr(const SceKernelModuleInfo &info, Address addr) {
    for (const auto &seg : info.segments) {
        if (!seg.size)
            continue;
        if (segment_contains(seg, addr))
            return true;
    }
    return false;
}

// PT_ARM_EXIDX.p_vaddr is a VA in the original ELF, not an offset from the
// module_info segment. Map through the PT_LOAD that owns the whole table.
// p_vaddr == 0 is valid for relocatable ELF (ET_SCE_RELEXEC); missing phdr is filesz == 0.
inline Address exidx_from_phdr(Address p_vaddr, uint32_t p_filesz, const SegmentInfosForReloc &segments, Address *end_out = nullptr) {
    if (end_out)
        *end_out = 0;
    if (p_filesz == 0)
        return 0;
    const uint64_t table_end = static_cast<uint64_t>(p_vaddr) + p_filesz;
    for (const auto &[_, seg] : segments) {
        const uint64_t seg_end = static_cast<uint64_t>(seg.p_vaddr) + seg.size;
        if (p_vaddr >= seg.p_vaddr && table_end <= seg_end) {
            const Address top = seg.addr + (p_vaddr - seg.p_vaddr);
            if (end_out)
                *end_out = top + p_filesz;
            return top;
        }
    }
    return 0;
}

inline uint32_t clamped_module_info_copy_size(uint32_t guest_size) {
    return std::min(guest_size, static_cast<uint32_t>(sizeof(SceKernelModuleInfo)));
}

// size == 0 means the guest did not fill the Sony size field; copy the full
// host struct (old GetModuleInfoByAddr behaviour) so EXIDX is not silently zero.
inline uint32_t effective_module_info_copy_size(uint32_t guest_size) {
    if (guest_size == 0)
        return static_cast<uint32_t>(sizeof(SceKernelModuleInfo));
    return clamped_module_info_copy_size(guest_size);
}

inline bool guest_range_fits32(Address start, uint32_t len) {
    return static_cast<uint64_t>(start) + len <= 0x100000000ull;
}

// Copy host module info into a guest SceKernelModuleInfo. src == nullptr means
// the lookup failed (NOENT) after the pointer/range checks.
inline int copy_module_info_to_guest(const SceKernelModuleInfo *src, Address info_va, MemState &mem) {
    if (!info_va)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (!guest_range_fits32(info_va, sizeof(SceSize)) || !is_valid_addr_range(mem, info_va, info_va + sizeof(SceSize)))
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    const uint32_t guest_size = *Ptr<SceSize>(info_va).get(mem);
    const uint32_t n = effective_module_info_copy_size(guest_size);
    if (n != 0 && (!guest_range_fits32(info_va, n) || !is_valid_addr_range(mem, info_va, info_va + n)))
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (!src)
        return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
    if (n != 0)
        std::memcpy(Ptr<uint8_t>(info_va).get(mem), src, n);
    // Partial copies must not overwrite the guest size field with sizeof(host).
    if (guest_size != 0)
        *Ptr<SceSize>(info_va).get(mem) = guest_size;
    return SCE_KERNEL_OK;
}

inline std::string ehabi_module_label(const char *module_name, const std::string &self_path) {
    if (module_name && module_name[0] != '\0')
        return module_name;
    const auto slash = self_path.find_last_of("/\\");
    if (slash == std::string::npos)
        return self_path;
    return self_path.substr(slash + 1);
}
