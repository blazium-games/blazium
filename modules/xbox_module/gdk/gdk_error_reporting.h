/**************************************************************************/
/*  gdk_error_reporting.h                                                 */
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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XError.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;

class GDKErrorReporting : public RefCounted {
	GDCLASS(GDKErrorReporting, RefCounted);

public:
	enum ErrorOptions {
		ERROR_OPTIONS_NONE = 0,
		ERROR_OPTIONS_OUTPUT_DEBUG_STRING_ON_ERROR = static_cast<uint32_t>(XErrorOptions::OutputDebugStringOnError),
		ERROR_OPTIONS_DEBUG_BREAK_ON_ERROR = static_cast<uint32_t>(XErrorOptions::DebugBreakOnError),
		ERROR_OPTIONS_FAIL_FAST_ON_ERROR = static_cast<uint32_t>(XErrorOptions::FailFastOnError),
	};

private:
	struct PendingError {
		HRESULT hr = S_OK;
		std::string message;
	};

	struct CallbackContext {
		GDKErrorReporting *service = nullptr;
		std::atomic_bool active = true;
		std::mutex mutex;
	};

	struct CallbackToken {
		std::weak_ptr<CallbackContext> context;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	bool m_callback_enabled = false;
	std::mutex m_pending_errors_mutex;
	std::vector<PendingError> m_pending_errors;
	std::shared_ptr<CallbackContext> m_callback_context;
	std::shared_ptr<CallbackToken> m_callback_token;
	std::vector<std::shared_ptr<CallbackToken>> m_retired_callback_tokens;

	static bool _error_callback(HRESULT p_hr, const char *p_message, void *p_context);
	static Ref<GDKResult> _parse_options(int64_t p_options_value, XErrorOptions *r_options);
	void _clear_callback_context();
	Ref<GDKResult> _set_callback(XErrorCallback *p_callback);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Ref<GDKResult> configure_options(
			int64_t p_debugger_present_options = ERROR_OPTIONS_NONE,
			int64_t p_debugger_not_present_options = ERROR_OPTIONS_NONE);
	Ref<GDKResult> set_callback_enabled(bool p_enabled);
	bool is_callback_enabled() const;

	GDKRuntime *get_runtime_internal() const;
	void push_error_internal(HRESULT p_hr, const char *p_message);
};

VARIANT_ENUM_CAST(GDKErrorReporting::ErrorOptions);
