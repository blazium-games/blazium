/**************************************************************************/
/*  justamcp_mcp_tool_macros.h                                            */
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

inline Dictionary justamcp_mcp_success(const Variant &p_data) {
	if (p_data.get_type() == Variant::DICTIONARY) {
		Dictionary r = Dictionary(p_data).duplicate();
		r["ok"] = true;
		return r;
	}
	Dictionary r;
	r["ok"] = true;
	r["value"] = p_data;
	return r;
}

inline Dictionary justamcp_mcp_error(int p_code, const String &p_msg) {
	Dictionary e;
	e["code"] = p_code;
	e["message"] = p_msg;
	Dictionary r;
	r["ok"] = false;
	r["error"] = e;
	return r;
}

inline Dictionary justamcp_mcp_error_data(int p_code, const String &p_msg, const Variant &p_data) {
	Dictionary e;
	e["code"] = p_code;
	e["message"] = p_msg;
	e["data"] = p_data;
	Dictionary r;
	r["ok"] = false;
	r["error"] = e;
	return r;
}

#undef MCP_SUCCESS
#undef MCP_ERROR
#undef MCP_ERROR_DATA
#undef MCP_INVALID_PARAMS
#undef MCP_NOT_FOUND
#undef MCP_INTERNAL

#define MCP_SUCCESS(data) justamcp_mcp_success(data)
#define MCP_ERROR(code, msg) justamcp_mcp_error(code, msg)
#define MCP_ERROR_DATA(code, msg, data) justamcp_mcp_error_data(code, msg, data)
#define MCP_INVALID_PARAMS(msg) justamcp_mcp_error(-32602, msg)
#define MCP_NOT_FOUND(msg) justamcp_mcp_error_data(-32001, String(msg) + " not found", Dictionary())
#define MCP_INTERNAL(msg) justamcp_mcp_error(-32603, String("Internal error: ") + msg)
