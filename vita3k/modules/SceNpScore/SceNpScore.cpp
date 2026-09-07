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

#include <kernel/state.h>

EXPORT(int, sceNpScoreAbortRequest) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreCensorComment) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreCensorCommentAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreChangeModeForOtherSaveDataOwners) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreCreateRequest, int titleCtxId) {
    if (titleCtxId <= 0)
        return -1;
    return emuenv.kernel.get_next_uid();
}

EXPORT(int, sceNpScoreCreateTitleCtx, const void *communicationId, const void *passphrase, const void *signature) {
    if (!communicationId)
        return -1;
    return emuenv.kernel.get_next_uid();
}

EXPORT(int, sceNpScoreDeleteRequest) {
    return 0;
}

EXPORT(int, sceNpScoreDeleteTitleCtx) {
    return 0;
}

EXPORT(int, sceNpScoreGetBoardInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetBoardInfoAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetFriendsRanking) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetFriendsRankingAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetGameData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetGameDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByNpId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByNpIdAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByNpIdPcId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByNpIdPcIdAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByRange) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreGetRankingByRangeAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreInit) {
    return 0;
}

EXPORT(int, sceNpScorePollAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreRecordGameData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreRecordGameDataAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreRecordScore) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreRecordScoreAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreSanitizeComment) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreSanitizeCommentAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreSetPlayerCharacterId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreSetTimeout) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreTerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceNpScoreWaitAsync) {
    return UNIMPLEMENTED();
}
