/**************************************************************************/
/*  justamcp_tool_result.h                                                */
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

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

namespace JustAMCPToolResult {

inline Dictionary ok(const Variant &p_result = Variant()) {
	Dictionary r;
	r["ok"] = true;
	if (p_result.get_type() != Variant::NIL) {
		r["result"] = p_result;
	}
	return r;
}

inline Dictionary err(const String &p_error, int p_code = -32603) {
	Dictionary r;
	r["ok"] = false;
	r["error"] = p_error;
	r["error_code"] = p_code;
	return r;
}

inline Dictionary busy(const String &p_error = "Busy; retry later", int p_retry_after_ms = 500) {
	Dictionary r;
	r["ok"] = false;
	r["error"] = p_error;
	r["retryAfterMs"] = p_retry_after_ms;
	return r;
}

} //namespace JustAMCPToolResult
