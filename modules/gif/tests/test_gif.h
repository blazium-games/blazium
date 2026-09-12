/**************************************************************************/
/*  test_gif.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

void test_gif_decode_basic();
void test_gif_encode_roundtrip();
void test_gif_disposal_compose();
void test_gif_interlaced_decode();
void test_gif_corrupt_buffer();
void test_gif_image_hooks();
void test_gif_sprite_frames_convert();
void test_gif_decode_caps();
void test_gif_active_texture_without_rebake();
void test_gif_recorder_add_frame();

TEST_CASE("[Modules][GIF] decode frames delay transparency loop") {
	test_gif_decode_basic();
}

TEST_CASE("[Modules][GIF] encode still and animated roundtrip") {
	test_gif_encode_roundtrip();
}

TEST_CASE("[Modules][GIF] disposal compose restore background and previous") {
	test_gif_disposal_compose();
}

TEST_CASE("[Modules][GIF] interlaced decode") {
	test_gif_interlaced_decode();
}

TEST_CASE("[Modules][GIF] corrupt buffer returns error") {
	test_gif_corrupt_buffer();
}

TEST_CASE("[Modules][GIF] Image load and save hooks") {
	test_gif_image_hooks();
}

TEST_CASE("[Modules][GIF] SpriteFrames conversion") {
	test_gif_sprite_frames_convert();
}

TEST_CASE("[Modules][GIF] decode cap rejection") {
	test_gif_decode_caps();
}

TEST_CASE("[Modules][GIF] get_active_texture without explicit rebake") {
	test_gif_active_texture_without_rebake();
}

TEST_CASE("[Modules][GIF] GIFRecorder add_frame") {
	test_gif_recorder_add_frame();
}
