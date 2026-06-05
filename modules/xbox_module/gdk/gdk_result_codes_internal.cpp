/**************************************************************************/
/*  gdk_result_codes_internal.cpp                                         */
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

#include "gdk_result_codes_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <cstdio>

namespace gdk_internal {

char *format_hresult_hex(HRESULT p_hresult, char *out_buffer, std::size_t buffer_size) noexcept {
	if (out_buffer == nullptr || buffer_size == 0) {
		return out_buffer;
	}
	if (buffer_size < HRESULT_HEX_BUFFER_SIZE) {
		out_buffer[0] = '\0';
		return out_buffer;
	}
	std::snprintf(out_buffer, buffer_size, "0x%08X", static_cast<unsigned int>(p_hresult));
	return out_buffer;
}

String format_hresult_string(HRESULT p_hresult) {
	char buffer[16];
	format_hresult_hex(p_hresult, buffer, sizeof(buffer));
	return String(buffer);
}

String format_hresult_message(const String &p_action, HRESULT p_hresult) {
	String message = p_action;
	if (!message.is_empty()) {
		message += " ";
	}
	message += "(HRESULT " + format_hresult_string(p_hresult) + ")";
	return message;
}

String code_or_format_hresult(const String &p_provided, HRESULT p_hresult) {
	return p_provided.is_empty() ? format_hresult_string(p_hresult) : p_provided;
}

} //namespace gdk_internal
