/**************************************************************************/
/*  anticheat_ed25519.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/string/ustring.h"

#include <cstddef>
#include <cstdint>

bool anticheat_parse_runtime_sig(const String &p_body, String &r_sha256_hex, String &r_sig_b64);
int anticheat_b64_decode(const String &p_s, uint8_t *p_out, int p_cap);
int anticheat_hex_decode(const String &p_s, uint8_t *p_out, int p_want);
bool anticheat_ed25519_verify(const uint8_t p_pk[32], const uint8_t *p_msg, size_t p_msg_len, const uint8_t p_sig[64]);
