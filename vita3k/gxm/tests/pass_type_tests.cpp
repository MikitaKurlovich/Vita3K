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

#include <gxm/functions.h>
#include <gxm/types.h>

#include <gtest/gtest.h>

TEST(infer_fragment_pass_type, mask_update) {
    EXPECT_EQ(gxm::infer_fragment_pass_type(true, nullptr, nullptr), SCE_GXM_PASS_TYPE_MASK_UPDATE);
}

TEST(infer_fragment_pass_type, opaque_when_no_program) {
    EXPECT_EQ(gxm::infer_fragment_pass_type(false, nullptr, nullptr), SCE_GXM_PASS_TYPE_OPAQUE);
}

TEST(infer_fragment_pass_type, discard_beats_blend) {
    SceGxmProgram program{};
    program.program_flags = SCE_GXM_PROGRAM_FLAG_DISCARD_USED;
    SceGxmBlendInfo blend{};
    blend.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
    blend.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
    EXPECT_EQ(gxm::infer_fragment_pass_type(false, &program, &blend), SCE_GXM_PASS_TYPE_DISCARD);
}

TEST(infer_fragment_pass_type, depth_replace) {
    SceGxmProgram program{};
    program.program_flags = SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED | SCE_GXM_PROGRAM_FLAG_DEPTH_USED;
    EXPECT_EQ(gxm::infer_fragment_pass_type(false, &program, nullptr), SCE_GXM_PASS_TYPE_DEPTH_REPLACE);
}

TEST(infer_fragment_pass_type, translucent_from_blend) {
    SceGxmProgram program{};
    SceGxmBlendInfo blend{};
    blend.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
    blend.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
    EXPECT_EQ(gxm::infer_fragment_pass_type(false, &program, &blend), SCE_GXM_PASS_TYPE_TRANSLUCENT);
}

TEST(infer_fragment_pass_type, opaque_default) {
    SceGxmProgram program{};
    SceGxmBlendInfo blend{};
    blend.colorFunc = SCE_GXM_BLEND_FUNC_NONE;
    blend.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
    EXPECT_EQ(gxm::infer_fragment_pass_type(false, &program, &blend), SCE_GXM_PASS_TYPE_OPAQUE);
}

TEST(program_has_no_effect, empty_output_without_depth_or_discard) {
    SceGxmProgram program{};
    program.program_flags = SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED;
    EXPECT_TRUE(program.has_no_effect());
}

TEST(program_has_no_effect, depth_replace_is_an_effect) {
    SceGxmProgram program{};
    program.program_flags = SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED | SCE_GXM_PROGRAM_FLAG_DEPTH_USED;
    EXPECT_FALSE(program.has_no_effect());
}
