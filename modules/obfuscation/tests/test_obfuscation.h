/**************************************************************************/
/*  test_obfuscation.h                                                    */
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

void test_obfuscation_reed_solomon_roundtrip();
void test_obfuscation_seal_png_roundtrip();
void test_obfuscation_seal_tile_decode();
void test_obfuscation_image_mark_roundtrip();
void test_obfuscation_pcm_mark_roundtrip();
void test_obfuscation_path_canonicalize();
void test_obfuscation_scramble_one_way();
void test_obfuscation_identifier_rewrite();
void test_obfuscation_scene_rewrite();
void test_obfuscation_luau_rewrite();
void test_obfuscation_comment_lattice();

TEST_CASE("[Modules][Obfuscation] Reed-Solomon roundtrip") {
	test_obfuscation_reed_solomon_roundtrip();
}

TEST_CASE("[Modules][Obfuscation] seal PNG encode decode") {
	test_obfuscation_seal_png_roundtrip();
}

TEST_CASE("[Modules][Obfuscation] cropped 64x64 tile still decodes") {
	test_obfuscation_seal_tile_decode();
}

TEST_CASE("[Modules][Obfuscation] image DWT mark extract") {
	test_obfuscation_image_mark_roundtrip();
}

TEST_CASE("[Modules][Obfuscation] PCM FFT mark extract") {
	test_obfuscation_pcm_mark_roundtrip();
}

TEST_CASE("[Modules][Obfuscation] path canonicalize is case-insensitive") {
	test_obfuscation_path_canonicalize();
}

TEST_CASE("[Modules][Obfuscation] output name scramble is one-way") {
	test_obfuscation_scramble_one_way();
}

TEST_CASE("[Modules][Obfuscation] identifier rewrite keeps reserved names") {
	test_obfuscation_identifier_rewrite();
}

TEST_CASE("[Modules][Obfuscation] scene tree names scramble with NodePath and groups") {
	test_obfuscation_scene_rewrite();
}

TEST_CASE("[Modules][Obfuscation] Luau identifiers and require paths scramble") {
	test_obfuscation_luau_rewrite();
}

TEST_CASE("[Modules][Obfuscation] comment lattice minify roundtrip and MAC") {
	test_obfuscation_comment_lattice();
}
