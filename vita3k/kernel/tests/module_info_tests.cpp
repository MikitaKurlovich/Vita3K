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

#include <kernel/module_info.h>
#include <mem/functions.h>
#include <mem/state.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

TEST(relocate_module_info_offset, offset_zero_is_valid) {
    EXPECT_EQ(relocate_module_info_offset(0, 0x81000000, 0x1000), 0x81000000u);
}

TEST(relocate_module_info_offset, sentinel_ffffffff_is_null) {
    EXPECT_EQ(relocate_module_info_offset(0xffffffff, 0x81000000, 0x1000), 0u);
}

TEST(relocate_module_info_offset, interior_offset) {
    EXPECT_EQ(relocate_module_info_offset(0x100, 0x81000000, 0x1000), 0x81000100u);
}

TEST(relocate_module_info_offset, start_at_seg_size_is_null) {
    EXPECT_EQ(relocate_module_info_offset(0x1000, 0x81000000, 0x1000), 0u);
}

TEST(relocate_module_info_offset, exclusive_end_at_seg_size) {
    EXPECT_EQ(relocate_module_info_offset(0x1000, 0x81000000, 0x1000, ModuleInfoOffsetKind::ExclusiveEnd), 0x81001000u);
}

TEST(relocate_module_info_offset, exclusive_end_past_seg_size) {
    EXPECT_EQ(relocate_module_info_offset(0x1001, 0x81000000, 0x1000, ModuleInfoOffsetKind::ExclusiveEnd), 0u);
}

TEST(relocate_module_info_offset, last_byte_of_segment) {
    EXPECT_EQ(relocate_module_info_offset(0xfff, 0x81000000, 0x1000), 0x81000fffu);
}

TEST(relocate_module_info_offset, va_overflow) {
    EXPECT_EQ(relocate_module_info_offset(0x20, 0xFFFFFFF0, 0x100), 0u);
}

TEST(normalize_ehabi_range, both_null) {
    const EhabiRange r = normalize_ehabi_range(0, 0);
    EXPECT_EQ(r.top, 0u);
    EXPECT_EQ(r.end, 0u);
    EXPECT_STREQ(r.reason, "no-tables");
}

TEST(normalize_ehabi_range, null_top_is_degenerate) {
    const EhabiRange r = normalize_ehabi_range(0, 0x81000001);
    EXPECT_EQ(r.top, 0u);
    EXPECT_EQ(r.end, 0u);
    EXPECT_STREQ(r.reason, "degenerate");
}

TEST(normalize_ehabi_range, null_end_is_degenerate) {
    const EhabiRange r = normalize_ehabi_range(0x81000000, 0);
    EXPECT_EQ(r.top, 0u);
    EXPECT_STREQ(r.reason, "degenerate");
}

TEST(normalize_ehabi_range, end_not_after_top) {
    const EhabiRange r = normalize_ehabi_range(0x81000010, 0x81000010);
    EXPECT_EQ(r.top, 0u);
    EXPECT_STREQ(r.reason, "degenerate");
}

TEST(normalize_ehabi_range, aligned_pair_kept) {
    const EhabiRange r = normalize_ehabi_range(0x81000000, 0x81000040);
    EXPECT_EQ(r.top, 0x81000000u);
    EXPECT_EQ(r.end, 0x81000040u);
    EXPECT_STREQ(r.reason, "ok");
}

TEST(normalize_ehabi_range, unaligned_length_kept) {
    const EhabiRange r = normalize_ehabi_range(0x81000000, 0x8100000C);
    EXPECT_EQ(r.top, 0x81000000u);
    EXPECT_EQ(r.end, 0x8100000Cu);
    EXPECT_STREQ(r.reason, "ok");
}

TEST(normalize_ehabi_range, exclusive_end_at_seg_limit) {
    const Address top = relocate_module_info_offset(0, 0x81000000, 0x1000);
    const Address end = relocate_module_info_offset(0x1000, 0x81000000, 0x1000, ModuleInfoOffsetKind::ExclusiveEnd);
    const EhabiRange r = normalize_ehabi_range(top, end);
    EXPECT_EQ(r.top, 0x81000000u);
    EXPECT_EQ(r.end, 0x81001000u);
    EXPECT_STREQ(r.reason, "ok");
}

TEST(normalize_ehabi_range, fake_old_sdk_pair) {
    // offsets (0, 1) relocated into a 1-byte span
    const Address top = relocate_module_info_offset(0, 0x81000000, 0x1000);
    const Address end = relocate_module_info_offset(1, 0x81000000, 0x1000);
    const EhabiRange r = normalize_ehabi_range(top, end);
    EXPECT_EQ(r.top, 0u);
    EXPECT_STREQ(r.reason, "degenerate-pair");
}

TEST(exidx_from_phdr, inside_seg0) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x81000000, 0x20000 };
    Address end = 0;
    EXPECT_EQ(exidx_from_phdr(0x81010000, 0x80, segs, &end), 0x81010000u);
    EXPECT_EQ(end, 0x81010080u);
}

TEST(exidx_from_phdr, inside_relocated_seg1) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x00000000, 0x1000 };
    segs[1] = SegmentInfoForReloc{ 0x82000000, 0x00010000, 0x2000 };
    Address end = 0;
    EXPECT_EQ(exidx_from_phdr(0x00011000, 0x10, segs, &end), 0x82001000u);
    EXPECT_EQ(end, 0x82001010u);
}

TEST(exidx_from_phdr, exclusive_end_at_seg_limit) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x81000000, 0x1000 };
    Address end = 0;
    EXPECT_EQ(exidx_from_phdr(0x81000FF8, 0x8, segs, &end), 0x81000FF8u);
    EXPECT_EQ(end, 0x81001000u);
}

TEST(exidx_from_phdr, table_past_segment_rejected) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x81000000, 0x1000 };
    Address end = 0;
    EXPECT_EQ(exidx_from_phdr(0x81000FF0, 0x20, segs, &end), 0u);
    EXPECT_EQ(end, 0u);
}

TEST(exidx_from_phdr, relocatable_p_vaddr_zero) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x00000000, 0x20000 };
    Address end = 0;
    EXPECT_EQ(exidx_from_phdr(0, 0x80, segs, &end), 0x81000000u);
    EXPECT_EQ(end, 0x81000080u);
}

TEST(exidx_from_phdr, filesz_zero_is_missing) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0, 0x1000 };
    EXPECT_EQ(exidx_from_phdr(0, 0, segs, nullptr), 0u);
}

TEST(exidx_from_phdr, outside_segments) {
    SegmentInfosForReloc segs;
    segs[0] = SegmentInfoForReloc{ 0x81000000, 0x81000000, 0x1000 };
    EXPECT_EQ(exidx_from_phdr(0x83000000, 0x8, segs, nullptr), 0u);
}

TEST(segment_contains, start_inclusive) {
    SceKernelSegmentInfo seg{};
    seg.size = sizeof(seg);
    seg.vaddr = Ptr<const void>(0x81000000);
    seg.memsz = 0x1000;
    EXPECT_TRUE(segment_contains(seg, 0x81000000));
}

TEST(segment_contains, end_exclusive) {
    SceKernelSegmentInfo seg{};
    seg.size = sizeof(seg);
    seg.vaddr = Ptr<const void>(0x81000000);
    seg.memsz = 0x1000;
    EXPECT_FALSE(segment_contains(seg, 0x81001000));
}

TEST(segment_contains, last_byte) {
    SceKernelSegmentInfo seg{};
    seg.size = sizeof(seg);
    seg.vaddr = Ptr<const void>(0x81000000);
    seg.memsz = 0x1000;
    EXPECT_TRUE(segment_contains(seg, 0x81000FFF));
}

TEST(clamped_module_info_copy_size, guest_smaller) {
    EXPECT_EQ(clamped_module_info_copy_size(0x40), 0x40u);
}

TEST(effective_module_info_copy_size, zero_means_full_struct) {
    EXPECT_EQ(effective_module_info_copy_size(0), static_cast<uint32_t>(sizeof(SceKernelModuleInfo)));
}

class ModuleInfoCopy : public ::testing::Test {
protected:
    MemState mem;

    void SetUp() override {
        ASSERT_TRUE(init(mem, false));
    }

    void TearDown() override {
        deinit_mem(mem);
    }
};

TEST_F(ModuleInfoCopy, null_info_is_illegal_addr) {
    SceKernelModuleInfo src{};
    EXPECT_EQ(copy_module_info_to_guest(&src, 0, mem), SCE_KERNEL_ERROR_ILLEGAL_ADDR);
}

TEST_F(ModuleInfoCopy, range_outside_memory_is_illegal_addr) {
    // 0x81000000 is a typical guest VA and is not allocated in a fresh MemState.
    // Do not use 0x1000: Apple 16 KiB host pages reserve the null guard covering that address.
    EXPECT_EQ(copy_module_info_to_guest(nullptr, 0x81000000, mem), SCE_KERNEL_ERROR_ILLEGAL_ADDR);
}

TEST_F(ModuleInfoCopy, not_found_is_noent) {
    const Address info_va = alloc(mem, sizeof(SceKernelModuleInfo), "modinfo");
    ASSERT_NE(info_va, 0u);
    auto *guest = Ptr<SceKernelModuleInfo>(info_va).get(mem);
    guest->size = sizeof(SceKernelModuleInfo);
    EXPECT_EQ(copy_module_info_to_guest(nullptr, info_va, mem), SCE_KERNEL_ERROR_MODULEMGR_NOENT);
}

TEST_F(ModuleInfoCopy, copies_min_guest_size) {
    const Address info_va = alloc(mem, sizeof(SceKernelModuleInfo), "modinfo");
    ASSERT_NE(info_va, 0u);
    auto *guest = Ptr<SceKernelModuleInfo>(info_va).get(mem);
    std::memset(guest, 0, sizeof(*guest));
    guest->size = 0x40;

    SceKernelModuleInfo src{};
    src.size = sizeof(SceKernelModuleInfo);
    src.modid = 42;
    std::memcpy(src.module_name, "testmod", 8);

    EXPECT_EQ(copy_module_info_to_guest(&src, info_va, mem), SCE_KERNEL_OK);
    EXPECT_EQ(guest->size, 0x40u);
    EXPECT_EQ(Ptr<SceKernelModuleInfo>(info_va).get(mem)->modid, 42);
}

TEST_F(ModuleInfoCopy, copies_full_struct_when_guest_size_zero) {
    const Address info_va = alloc(mem, sizeof(SceKernelModuleInfo), "modinfo");
    ASSERT_NE(info_va, 0u);
    auto *guest = Ptr<SceKernelModuleInfo>(info_va).get(mem);
    std::memset(guest, 0, sizeof(*guest));

    SceKernelModuleInfo src{};
    src.size = sizeof(SceKernelModuleInfo);
    src.modid = 11;
    src.exidx_top = Ptr<const void>(0x81000000);
    src.exidx_btm = Ptr<const void>(0x81000040);
    EXPECT_EQ(copy_module_info_to_guest(&src, info_va, mem), SCE_KERNEL_OK);
    EXPECT_EQ(guest->modid, 11);
    EXPECT_EQ(guest->exidx_top.address(), 0x81000000u);
    EXPECT_EQ(guest->exidx_btm.address(), 0x81000040u);
}

TEST_F(ModuleInfoCopy, copies_struct_when_guest_size_huge) {
    const Address info_va = alloc(mem, sizeof(SceKernelModuleInfo), "modinfo");
    ASSERT_NE(info_va, 0u);
    auto *guest = Ptr<SceKernelModuleInfo>(info_va).get(mem);
    std::memset(guest, 0, sizeof(*guest));
    guest->size = 0x1000;

    SceKernelModuleInfo src{};
    src.size = sizeof(SceKernelModuleInfo);
    src.modid = 7;
    EXPECT_EQ(copy_module_info_to_guest(&src, info_va, mem), SCE_KERNEL_OK);
    EXPECT_EQ(guest->modid, 7);
    EXPECT_EQ(guest->size, 0x1000u);
}

TEST(module_contains_addr, finds_segment) {
    SceKernelModuleInfo info{};
    info.segments[0].size = sizeof(SceKernelSegmentInfo);
    info.segments[0].vaddr = Ptr<const void>(0x81000000);
    info.segments[0].memsz = 0x1000;
    EXPECT_TRUE(module_contains_addr(info, 0x81000010));
    EXPECT_FALSE(module_contains_addr(info, 0x82000000));
}

TEST(module_has_loaded_code, placeholder_without_segments) {
    SceKernelModuleInfo info{};
    EXPECT_FALSE(module_has_loaded_code(info));
}

TEST(module_has_loaded_code, loaded_segment) {
    SceKernelModuleInfo info{};
    info.segments[0].size = sizeof(SceKernelSegmentInfo);
    info.segments[0].memsz = 0x1000;
    EXPECT_TRUE(module_has_loaded_code(info));
}

TEST(path_basename_is, device_path) {
    EXPECT_TRUE(path_basename_is("vs0:sys/external/libc.suprx", "libc.suprx"));
    EXPECT_FALSE(path_basename_is("app0:libc.suprx.bak", "libc.suprx"));
}

TEST(select_exidx_range, mismatch_prefers_phdr) {
    const EhabiRange info{ 0x81000000, 0x81000040, "ok" };
    const EhabiRange phdr{ 0x81010000, 0x81010080, "ok" };
    const EhabiRange chosen = select_exidx_range(info, phdr);
    EXPECT_EQ(chosen.top, 0x81010000u);
    EXPECT_EQ(chosen.end, 0x81010080u);
    EXPECT_STREQ(chosen.reason, "phdr-preferred");
}

TEST(select_exidx_range, empty_info_falls_back_to_phdr) {
    const EhabiRange info{ 0, 0, "no-tables" };
    const EhabiRange phdr{ 0x81000000, 0x81000040, "ok" };
    const EhabiRange chosen = select_exidx_range(info, phdr);
    EXPECT_EQ(chosen.top, 0x81000000u);
    EXPECT_STREQ(chosen.reason, "phdr-fallback");
}

TEST(select_exidx_range, empty_phdr_keeps_info) {
    const EhabiRange info{ 0x81000000, 0x81000040, "ok" };
    const EhabiRange phdr{ 0, 0, "no-tables" };
    const EhabiRange chosen = select_exidx_range(info, phdr);
    EXPECT_EQ(chosen.top, 0x81000000u);
    EXPECT_STREQ(chosen.reason, "ok");
}

TEST(module_snapshot, copy_under_lock_survives_unload) {
    std::mutex m;
    auto live = std::make_shared<SceKernelModuleInfo>();
    live->modid = 7;
    live->segments[0].size = sizeof(SceKernelSegmentInfo);
    live->segments[0].vaddr = Ptr<const void>(0x81000000);
    live->segments[0].memsz = 0x1000;

    std::atomic<int> snapshots{ 0 };
    std::thread reader([&] {
        for (int i = 0; i < 8000; ++i) {
            SceKernelModuleInfo host{};
            bool found = false;
            {
                const std::lock_guard<std::mutex> lock(m);
                if (live && module_contains_addr(*live, 0x81000010)) {
                    host = *live;
                    found = true;
                }
            }
            if (found) {
                EXPECT_EQ(host.modid, 7);
                snapshots.fetch_add(1);
            }
        }
    });
    std::thread unloader([&] {
        for (int i = 0; i < 8000; ++i) {
            const std::lock_guard<std::mutex> lock(m);
            if ((i % 16) == 0)
                live.reset();
            else if (!live) {
                live = std::make_shared<SceKernelModuleInfo>();
                live->modid = 7;
                live->segments[0].size = sizeof(SceKernelSegmentInfo);
                live->segments[0].vaddr = Ptr<const void>(0x81000000);
                live->segments[0].memsz = 0x1000;
            }
        }
    });
    reader.join();
    unloader.join();
    EXPECT_GT(snapshots.load(), 0);
}
