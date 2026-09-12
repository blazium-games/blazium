/**************************************************************************/
/*  gdk_result.cpp                                                        */
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

#include "gdk_result.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include "gdk_result_codes_internal.h"

void GDKResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ok"), &GDKResult::is_ok);
	ClassDB::bind_method(D_METHOD("get_hresult"), &GDKResult::get_hresult);
	ClassDB::bind_method(D_METHOD("get_code"), &GDKResult::get_code);
	ClassDB::bind_method(D_METHOD("get_message"), &GDKResult::get_message);
	ClassDB::bind_method(D_METHOD("get_data"), &GDKResult::get_data);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ok"), "", "is_ok");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hresult"), "", "get_hresult");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "code"), "", "get_code");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "", "get_message");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "data"), "", "get_data");
}

bool GDKResult::is_ok() const {
	return m_ok;
}

int64_t GDKResult::get_hresult() const {
	return m_hresult;
}

String GDKResult::get_code() const {
	return m_code;
}

String GDKResult::get_message() const {
	return m_message;
}

Variant GDKResult::get_data() const {
	return m_data;
}

void GDKResult::set_values(bool p_ok, HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) {
	m_ok = p_ok;
	m_hresult = static_cast<int64_t>(p_hresult);
	m_code = p_code;
	m_message = p_message;
	m_data = p_data;
}

Ref<GDKResult> GDKResult::ok_result(const Variant &p_data) {
	Ref<GDKResult> result;
	result.instantiate();
	result->set_values(true, S_OK, "ok", "", p_data);
	return result;
}

Ref<GDKResult> GDKResult::error_result(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) {
	Ref<GDKResult> result;
	result.instantiate();
	result->set_values(false, p_hresult, p_code, p_message, p_data);
	return result;
}

Ref<GDKResult> GDKResult::hresult_error(HRESULT p_hresult, const String &p_action, const String &p_code, const Variant &p_data) {
	return error_result(
			p_hresult,
			gdk_internal::code_or_format_hresult(p_code, p_hresult),
			gdk_internal::format_hresult_message(p_action, p_hresult),
			p_data);
}

Ref<GDKResult> GDKResult::cancelled(const String &p_message) {
	return error_result(E_ABORT, "cancelled", p_message);
}

String GDKResult::format_hresult(HRESULT p_hresult) {
	return gdk_internal::format_hresult_string(p_hresult);
}
