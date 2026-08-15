/**************************************************************************/
/*  gdk_capture.h                                                         */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XAppCapture.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;

class GDKCaptureMetaData : public RefCounted {
	GDCLASS(GDKCaptureMetaData, RefCounted);

	bool m_valid = false;

	Ref<GDKResult> _check_valid() const;
	static int32_t _clamp_to_int32(int64_t p_value);
	static XAppCaptureMetadataPriority _to_native_priority(int64_t p_priority);
	static Ref<GDKResult> _wrap_hresult(HRESULT p_hr, const char *p_action, const char *p_code);

protected:
	static void _bind_methods();

public:
	enum Priority {
		PRIORITY_GAMEPLAY = 0,
		PRIORITY_IMPORTANT = 1,
	};

	~GDKCaptureMetaData();

	bool is_valid() const;

	void close();

	Ref<GDKResult> stop_all_states();

	int64_t get_remaining_storage_bytes() const;

	Ref<GDKResult> add_string_event(
			const String &p_name,
			const String &p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	Ref<GDKResult> add_double_event(
			const String &p_name,
			double p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	Ref<GDKResult> add_int32_event(
			const String &p_name,
			int64_t p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	Ref<GDKResult> start_string_state(
			const String &p_name,
			const String &p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	Ref<GDKResult> start_double_state(
			const String &p_name,
			double p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	Ref<GDKResult> start_int32_state(
			const String &p_name,
			int64_t p_value,
			int64_t p_priority = PRIORITY_GAMEPLAY);

	void activate_internal();
};

class GDKCapture : public RefCounted {
	GDCLASS(GDKCapture, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

	GDKRuntime *_get_runtime() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message) const;

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Ref<GDKResult> enable_capture();

	Ref<GDKResult> disable_capture();

	Signal record_diagnostic_clip_async(double p_duration);

	Signal take_diagnostic_screenshot_async(const String &p_path_hint);

	Ref<GDKCaptureMetaData> create_metadata(int64_t p_reserved_bytes = 0);
};

VARIANT_ENUM_CAST(GDKCaptureMetaData::Priority);
