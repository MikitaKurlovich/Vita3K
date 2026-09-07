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

#include <kernel/import_flight.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <thread>
#include <vector>

TEST(import_flight, wrap_does_not_clobber_stale_finish) {
    ImportFlightRing ring;
    for (uint32_t i = 0; i < ImportFlightRing::size + 8; ++i) {
        ImportFlightRecord rec{};
        rec.nid = i + 1;
        const uint32_t seq = ring.record(rec);
        ring.finish(seq, 0x1000 + i);
    }

    ring.finish(0, 0xDEADBEEF);

    std::array<ImportFlightRecord, ImportFlightRing::size> recs{};
    uint32_t seq = 0;
    ring.copy(recs, seq);
    EXPECT_EQ(seq, ImportFlightRing::size + 8);
    EXPECT_NE(recs[0].ret, 0xDEADBEEFu);
    EXPECT_EQ(recs[0].nid, ImportFlightRing::size + 1);
}

TEST(import_flight, two_threads_seq_is_unique_and_dense) {
    ImportFlightRing ring;
    constexpr int per_thread = 4000;
    std::vector<uint32_t> a;
    std::vector<uint32_t> b;
    a.reserve(per_thread);
    b.reserve(per_thread);

    std::thread t1([&] {
        for (int i = 0; i < per_thread; ++i) {
            ImportFlightRecord rec{};
            rec.nid = 1;
            a.push_back(ring.record(rec));
        }
    });
    std::thread t2([&] {
        for (int i = 0; i < per_thread; ++i) {
            ImportFlightRecord rec{};
            rec.nid = 2;
            b.push_back(ring.record(rec));
        }
    });
    t1.join();
    t2.join();

    std::vector<uint32_t> all;
    all.insert(all.end(), a.begin(), a.end());
    all.insert(all.end(), b.begin(), b.end());
    std::sort(all.begin(), all.end());
    ASSERT_EQ(all.size(), static_cast<size_t>(per_thread * 2));
    EXPECT_EQ(all.front(), 0u);
    EXPECT_EQ(all.back(), static_cast<uint32_t>(per_thread * 2 - 1));
    EXPECT_TRUE(std::adjacent_find(all.begin(), all.end()) == all.end());
}
