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

#include <util/types.h>

#include <array>
#include <atomic>
#include <mutex>

struct ImportFlightRecord {
    uint32_t nid = 0;
    uint32_t lr = 0;
    uint32_t r0 = 0;
    uint32_t r1 = 0;
    uint32_t r2 = 0;
    uint32_t r3 = 0;
    uint32_t ret = 0;
    uint32_t gen = 0;
    SceUID thread_id = 0;
};

// Ring of HLE import records. finish() writes ret only when gen still matches
// the seq that occupied the slot, so wraparound cannot clobber a live finish.
struct ImportFlightRing {
    static constexpr size_t size = 64;

    uint32_t record(const ImportFlightRecord &rec) {
        std::lock_guard<std::mutex> lock(mutex);
        const uint32_t seq = seq_counter.fetch_add(1, std::memory_order_relaxed);
        ImportFlightRecord stored = rec;
        stored.gen = seq + 1;
        slots[seq % size] = stored;
        return seq;
    }

    void finish(uint32_t seq, uint32_t ret) {
        std::lock_guard<std::mutex> lock(mutex);
        auto &slot = slots[seq % size];
        if (slot.gen == seq + 1)
            slot.ret = ret;
    }

    void copy(std::array<ImportFlightRecord, size> &out, uint32_t &seq) const {
        std::lock_guard<std::mutex> lock(mutex);
        out = slots;
        seq = seq_counter.load(std::memory_order_relaxed);
    }

private:
    mutable std::mutex mutex;
    std::array<ImportFlightRecord, size> slots{};
    std::atomic<uint32_t> seq_counter{ 0 };
};
