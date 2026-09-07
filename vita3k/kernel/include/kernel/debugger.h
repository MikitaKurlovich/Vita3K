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
#include <cpu/state.h>
#include <mem/state.h>
#include <mem/util.h>
#include <util/types.h>

#include <array>
#include <atomic>
#include <map>

struct KernelState;

struct Breakpoint {
    bool thumb_mode;
    unsigned char data[4];
};

struct WatchMemory {
    Address start;
    size_t size;
};

typedef std::map<Address, WatchMemory> WatchMemoryAddrs;
typedef std::map<Address, Breakpoint> Breakpoints;

struct ImportFlightRecord {
    uint32_t nid = 0;
    uint32_t lr = 0;
    uint32_t r0 = 0;
    uint32_t r1 = 0;
    uint32_t r2 = 0;
    uint32_t r3 = 0;
    uint32_t ret = 0;
    SceUID thread_id = 0;
};

struct Debugger {
    Debugger() = delete;
    explicit Debugger(KernelState &kernel);

    bool wait_for_debugger = false;
    bool watch_import_calls = false;
    bool watch_code = false;
    bool watch_memory = false;

    bool log_imports = false;
    bool log_exports = false;
    bool dump_elfs = false;
    bool log_ehabi = false;
    bool dump_abort_state = false;

    // Ring buffer of recent HLE imports. Dumped on guest throw/abort.
    static constexpr size_t import_flight_size = 64;
    std::array<ImportFlightRecord, import_flight_size> import_flight{};
    std::atomic<uint32_t> import_flight_seq{ 0 };

    void add_watch_memory_addr(Address addr, size_t size);
    void remove_watch_memory_addr(KernelState &state, Address addr);
    void add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode);
    void remove_breakpoint(MemState &mem, uint32_t addr);
    Address get_watch_memory_addr(Address addr);
    void update_watches();
    void deinit();

private:
    std::mutex mutex;
    KernelState &parent;
    WatchMemoryAddrs watch_memory_addrs;
    Breakpoints breakpoints;
};
