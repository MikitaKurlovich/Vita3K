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

#include <module/module.h>
#include <modules/module_parent.h>

EXPORT(int, _Unwind_Backtrace) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xA22B2436) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_Complete) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x8A5F29D8) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_DeleteException) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x4BB45B70) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_ForcedUnwind) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x7772C028) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetCFA) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xDBE840D6) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetDataRelBase) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xAC15DBA5) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetLanguageSpecificData) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xBAC00FF7) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetRegionStart) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xDA5097CE) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_GetTextRelBase) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x8D4953C7) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_RaiseException) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x12472ADD) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, __Unwind_Resume) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x74274866) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_Resume_or_Rethrow) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x7DFC519A) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Get) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0x0DFF2B2C) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Pop) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xF16E32FC) ? 0 : UNIMPLEMENTED();
}

EXPORT(int, _Unwind_VRS_Set) {
    return hle_stopped_unbound_unwind(emuenv, thread_id, export_name, 0xDAB28374) ? 0 : UNIMPLEMENTED();
}
