/**************************************************************************/
/*  eol_normalize.cpp                                                     */
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

#include "common/attributes/eol_normalize.h"

#include <algorithm>
#include <cstddef>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace coldstorage {

bool isProbablyText(const std::vector<uint8_t> &data) {
	if (data.empty()) {
		return true;
	}
	size_t check = std::min(data.size(), size_t(8192));
	for (size_t i = 0; i < check; ++i) {
		if (data[i] == 0) {
			return false;
		}
	}
	return true;
}

static EolStyle nativeEol() {
#ifdef _WIN32
	return EolStyle::Crlf;
#else
	return EolStyle::Lf;
#endif
}

std::vector<uint8_t> normalizeEol(const std::vector<uint8_t> &data, EolStyle target) {
	if (target == EolStyle::Unspecified) {
		target = EolStyle::Native;
	}
	if (target == EolStyle::Native) {
		target = nativeEol();
	}

	std::vector<uint8_t> out;
	out.reserve(data.size());
	for (size_t i = 0; i < data.size(); ++i) {
		if (data[i] == '\r' && i + 1 < data.size() && data[i + 1] == '\n') {
			if (target == EolStyle::Lf) {
				out.push_back('\n');
			} else {
				out.push_back('\r');
				out.push_back('\n');
			}
			++i;
		} else if (data[i] == '\n') {
			if (target == EolStyle::Crlf) {
				out.push_back('\r');
				out.push_back('\n');
			} else {
				out.push_back('\n');
			}
		} else if (data[i] == '\r') {
			if (target == EolStyle::Lf) {
				out.push_back('\n');
			} else {
				out.push_back('\r');
				out.push_back('\n');
			}
		} else {
			out.push_back(data[i]);
		}
	}
	return out;
}

} //namespace coldstorage
