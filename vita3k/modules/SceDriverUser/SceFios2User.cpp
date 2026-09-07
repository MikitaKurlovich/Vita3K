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

#include "io/functions.h"

#include <config/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <cstring>
#include <string>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceFios2User);

enum SceFiosErrorCode {
    SCE_FIOS_OK = 0,
    SCE_FIOS_ERROR_INVALID = static_cast<int>(0x80420005)
};

typedef SceUID SceFiosOverlayID;

enum SceFiosOverlayResolveMode {
    SCE_FIOS_OVERLAY_RESOLVE_FOR_READ = 0,
    SCE_FIOS_OVERLAY_RESOLVE_FOR_WRITE = 1
};

template <>
std::string to_debug_str<SceFiosOverlayResolveMode>(const MemState &mem, SceFiosOverlayResolveMode type) {
    switch (type) {
    case SCE_FIOS_OVERLAY_RESOLVE_FOR_READ:
        return "SCE_FIOS_OVERLAY_RESOLVE_FOR_READ";
    case SCE_FIOS_OVERLAY_RESOLVE_FOR_WRITE:
        return "SCE_FIOS_OVERLAY_RESOLVE_FOR_WRITE";
    }
    return std::to_string(type);
}

EXPORT(int, sceFiosOverlayAddForProcess02, SceUID processId, SceFiosProcessOverlay *pOverlay, SceFiosOverlayID *pOutID) {
    TRACY_FUNC(sceFiosOverlayAddForProcess02, processId, pOverlay, pOutID);
    if (pOverlay->type != SCE_FIOS_OVERLAY_TYPE_OPAQUE)
        LOG_WARN("Using unimplemented overlay type {}.", fmt::underlying(pOverlay->type));

    *pOutID = create_overlay(emuenv.io, pOverlay);

    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayGetInfoForProcess02, SceUID processId, SceFiosOverlayID id, SceFiosProcessOverlay *pOverlay) {
    TRACY_FUNC(sceFiosOverlayGetInfoForProcess02, processId, id, pOverlay);
    (void)processId;
    if (!pOverlay)
        return RET_ERROR(SCE_FIOS_ERROR_INVALID);
    if (!get_overlay(emuenv.io, id, pOverlay))
        return RET_ERROR(SCE_FIOS_ERROR_INVALID);
    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayGetList02, SceUID processId, uint32_t minOrder, uint32_t maxOrder, SceFiosOverlayID *pOutIDs, SceUInt32 maxIDs, SceUInt32 *pActualIDs) {
    TRACY_FUNC(sceFiosOverlayGetList02, processId, minOrder, maxOrder, pOutIDs, maxIDs, pActualIDs);
    const std::lock_guard<std::mutex> guard(emuenv.io.overlay_mutex);

    std::vector<SceFiosOverlayID> overlay_ids;
    for (const auto &overlay : emuenv.io.overlays) {
        if (overlay.order >= minOrder && overlay.order <= maxOrder)
            overlay_ids.push_back(overlay.id);
    }

    if (pActualIDs)
        *pActualIDs = overlay_ids.size();

    if (pOutIDs)
        memcpy(pOutIDs, overlay_ids.data(), std::min<uint32_t>(overlay_ids.size(), maxIDs) * sizeof(SceFiosOverlayID));

    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayGetRecommendedScheduler02, int param1, const char *path) {
    TRACY_FUNC(sceFiosOverlayGetRecommendedScheduler02, param1, path);
    // reversed engineered
    if (param1 <= 1)
        return 0;

    // returns if path starts with hostk: with k an integer
    if (strlen(path) < strlen("host0:"))
        return 0;

    return memcmp(path, "host", 4) == 0 && path[4] <= '9' && path[5] == ':';
}

EXPORT(int, sceFiosOverlayModifyForProcess02, SceUID processId, SceFiosOverlayID id, SceFiosProcessOverlay *pOverlay) {
    TRACY_FUNC(sceFiosOverlayModifyForProcess02, processId, id, pOverlay);
    (void)processId;
    if (!pOverlay)
        return RET_ERROR(SCE_FIOS_ERROR_INVALID);
    if (pOverlay->type != SCE_FIOS_OVERLAY_TYPE_OPAQUE)
        LOG_WARN("Using unimplemented overlay type {}.", fmt::underlying(pOverlay->type));
    if (!modify_overlay(emuenv.io, id, pOverlay))
        return RET_ERROR(SCE_FIOS_ERROR_INVALID);
    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayRemoveForProcess02, SceUID processId, SceFiosOverlayID id) {
    TRACY_FUNC(sceFiosOverlayRemoveForProcess02, processId, id);
    (void)processId;
    if (!remove_overlay(emuenv.io, id))
        return RET_ERROR(SCE_FIOS_ERROR_INVALID);
    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayResolveWithRangeSync02, SceUID processId, SceFiosOverlayResolveMode resolveFlag, const char *pInPath, char *pOutPath, SceUInt32 maxPath, SceUInt32 min_order, SceUInt32 max_order) {
    TRACY_FUNC(sceFiosOverlayResolveWithRangeSync02, processId, resolveFlag, pInPath, pOutPath, maxPath, min_order, max_order);

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    const bool disabled = emuenv.cfg.fios_overlay_per_thread && thread && thread->fios_overlay_disabled.load() != 0;

    std::string resolved;
    const char *src = pInPath ? pInPath : "";
    if (!disabled) {
        resolved = resolve_path(emuenv.io, src, min_order, max_order);
        src = resolved.c_str();
    }

    if (pOutPath && maxPath > 0) {
        std::strncpy(pOutPath, src, maxPath);
        pOutPath[maxPath - 1] = '\0';
    }

    LOG_DEBUG("[FIOS] overlay resolve tid={} disabled={} in={} out={}",
        thread_id, disabled ? 1 : 0, pInPath ? pInPath : "", pOutPath ? pOutPath : "");

    return SCE_FIOS_OK;
}

EXPORT(int, sceFiosOverlayResolveSync02, SceUID processId, SceFiosOverlayResolveMode resolveFlag, const char *pInPath, char *pOutPath, SceUInt32 maxPath) {
    TRACY_FUNC(sceFiosOverlayResolveSync02, processId, resolveFlag, pInPath, pOutPath, maxPath);
    return CALL_EXPORT(sceFiosOverlayResolveWithRangeSync02, processId, resolveFlag, pInPath, pOutPath, maxPath, 0u, 0x7Fu);
}

EXPORT(int, sceFiosOverlayThreadIsDisabled02) {
    TRACY_FUNC(sceFiosOverlayThreadIsDisabled02);
    if (!emuenv.cfg.fios_overlay_per_thread)
        return 0;
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    return thread ? thread->fios_overlay_disabled.load() : 0;
}

EXPORT(int, sceFiosOverlayThreadSetDisabled02, int disabled) {
    TRACY_FUNC(sceFiosOverlayThreadSetDisabled02, disabled);
    if (const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id))
        thread->fios_overlay_disabled.store(disabled);
    return SCE_FIOS_OK;
}
