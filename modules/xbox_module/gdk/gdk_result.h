/**************************************************************************/
/*  gdk_result.h                                                          */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

class GDKResult : public RefCounted {
	GDCLASS(GDKResult, RefCounted);

	bool m_ok = false;
	int64_t m_hresult = 0;
	String m_code;
	String m_message;
	Variant m_data;

protected:
	static void _bind_methods();

public:
	bool is_ok() const;
	int64_t get_hresult() const;
	String get_code() const;
	String get_message() const;
	Variant get_data() const;

	void set_values(bool p_ok, HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant());

	static Ref<GDKResult> ok_result(const Variant &p_data = Variant());
	static Ref<GDKResult> error_result(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant());
	static Ref<GDKResult> hresult_error(HRESULT p_hresult, const String &p_action, const String &p_code = String(), const Variant &p_data = Variant());
	static Ref<GDKResult> cancelled(const String &p_message = "Operation cancelled.");
	static String format_hresult(HRESULT p_hresult);
};
