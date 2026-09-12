/**************************************************************************/
/*  gdk_runtime.cpp                                                       */
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

#include "gdk_runtime.h"

#include <algorithm>

#include "core/config/project_settings.h"

#include "gdk_pending_signal.h"
#include "gdk_result.h"

namespace {

#if defined(XBOX_MODULE_GDK_ENABLED)
constexpr bool GDK_PLATFORM_AVAILABLE = true;
#else
constexpr bool GDK_PLATFORM_AVAILABLE = false;
#endif

constexpr const char *GAME_CONFIG_RES_PATH = "res://MicrosoftGame.config";

CharString g_xgame_runtime_config_path;

#ifdef XBOX_MODULE_GDK_ENABLED
String _resolve_game_config_path() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (settings == nullptr) {
		return String();
	}

	String resolved = settings->globalize_path(GAME_CONFIG_RES_PATH);
	if (resolved.is_empty() || resolved.begins_with("res://")) {
		return String();
	}

	Char16String wide_path = resolved.utf16();
	WIN32_FILE_ATTRIBUTE_DATA attrs = {};
	BOOL ok = GetFileAttributesExW(
			reinterpret_cast<LPCWSTR>(wide_path.get_data()),
			GetFileExInfoStandard,
			&attrs);
	if (!ok) {
		return String();
	}
	if ((attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		return String();
	}
	return resolved;
}
#endif

#ifdef XBOX_MODULE_GDK_ENABLED
HRESULT _initialize_xgame_runtime(String &r_config_path) {
	String resolved = _resolve_game_config_path();
	if (resolved.is_empty()) {
		r_config_path = String();
		return XGameRuntimeInitialize();
	}

	g_xgame_runtime_config_path = resolved.utf8();
	XGameRuntimeOptions options = {};
	options.gameConfigSource = XGameRuntimeGameConfigSource::File;
	options.gameConfig = g_xgame_runtime_config_path.get_data();
	r_config_path = resolved;
	return XGameRuntimeInitializeWithOptions(&options);
}
#endif

} //namespace

#ifdef XBOX_MODULE_GDK_ENABLED
GDKRuntime::GDKRuntime() {
}

GDKRuntime::~GDKRuntime() {
	shutdown();

	if (m_xgame_runtime_initialized) {
		XGameRuntimeUninitialize();
		m_xgame_runtime_initialized = false;
	}
}

Ref<GDKResult> GDKRuntime::initialize() {
	if (m_initialized) {
		Ref<GDKResult> result = GDKResult::error_result(E_FAIL, "already_initialized", "GDK runtime is already initialized.");
		return result;
	}

	if (!m_xgame_runtime_initialized) {
		String attempted_config_path;
		HRESULT hr = _initialize_xgame_runtime(attempted_config_path);
		if (FAILED(hr)) {
			String message = "Failed to initialize GDK runtime.";
			if (!attempted_config_path.is_empty()) {
				message += " Tried game config at: " + attempted_config_path;
			}
			Ref<GDKResult> result = GDKResult::hresult_error(hr, message, "runtime_initialize_failed");
			return result;
		}
		m_xgame_runtime_initialized = true;
	}

	HRESULT hr = XTaskQueueCreate(
			XTaskQueueDispatchMode::ThreadPool,
			XTaskQueueDispatchMode::Manual,
			&m_task_queue);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to create the shared XTaskQueue.", "task_queue_create_failed");
		return result;
	}

	m_initialized = true;
	m_shutting_down = false;
	return GDKResult::ok_result();
}

void GDKRuntime::shutdown() {
	if (!m_initialized) {
		return;
	}

	m_shutting_down = true;

	std::vector<Ref<GDKPendingSignal>> active_pending_signals = m_active_pending_signals;
	for (const Ref<GDKPendingSignal> &pending_signal : active_pending_signals) {
		if (pending_signal.is_valid()) {
			pending_signal->cancel();
			if (!pending_signal->is_done()) {
				pending_signal->complete(GDKResult::cancelled("GDK runtime shutdown cancelled the async request."));
			}
		}
	}

	if (m_task_queue) {
		bool terminated = false;
		HRESULT terminate_hr = XTaskQueueTerminate(m_task_queue, false, &terminated, _queue_terminated);
		if (SUCCEEDED(terminate_hr)) {
			while (!terminated) {
				XTaskQueueDispatch(m_task_queue, XTaskQueuePort::Completion, 10);
			}
		}

		XTaskQueueCloseHandle(m_task_queue);
		m_task_queue = nullptr;
	}

	for (Ref<GDKPendingSignal> &pending_signal : m_active_pending_signals) {
		if (pending_signal.is_valid()) {
			pending_signal->clear_cancel_handler();
			pending_signal->clear_release_handler();
		}
	}
	m_active_pending_signals.clear();

	m_initialized = false;
	m_shutting_down = false;
}

int GDKRuntime::dispatch() {
	if (!m_initialized || !m_task_queue) {
		return 0;
	}

	int dispatched = 0;
	while (XTaskQueueDispatch(m_task_queue, XTaskQueuePort::Completion, 0)) {
		++dispatched;
	}

	return dispatched;
}

bool GDKRuntime::is_initialized() const {
	return m_initialized;
}

bool GDKRuntime::is_shutting_down() const {
	return m_shutting_down;
}

bool GDKRuntime::is_available() const {
	return GDK_PLATFORM_AVAILABLE;
}

XTaskQueueHandle GDKRuntime::get_task_queue() const {
	return m_task_queue;
}

void GDKRuntime::retain_pending_signal(const Ref<GDKPendingSignal> &p_pending_signal) {
	if (!p_pending_signal.is_valid() || p_pending_signal->is_done()) {
		return;
	}

	p_pending_signal->set_release_handler([this](GDKPendingSignal *p_completed_signal) {
		release_pending_signal(p_completed_signal);
	});
	m_active_pending_signals.push_back(p_pending_signal);
}

void GDKRuntime::release_pending_signal(GDKPendingSignal *p_pending_signal) {
	m_active_pending_signals.erase(
			std::remove_if(
					m_active_pending_signals.begin(),
					m_active_pending_signals.end(),
					[p_pending_signal](const Ref<GDKPendingSignal> &candidate) {
						return candidate.is_null() || candidate.operator->() == p_pending_signal;
					}),
			m_active_pending_signals.end());
}

Ref<GDKPendingSignal> GDKRuntime::make_pending_signal() {
	Ref<GDKPendingSignal> pending_signal;
	pending_signal.instantiate();
	retain_pending_signal(pending_signal);
	return pending_signal;
}

Signal GDKRuntime::make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) {
	Ref<GDKPendingSignal> pending_signal = make_pending_signal();
	Ref<GDKResult> result = GDKResult::error_result(p_hresult, p_code, p_message, p_data);
	pending_signal->complete_deferred(result);
	return pending_signal->get_completed_signal();
}

void CALLBACK GDKRuntime::_queue_terminated(void *p_context) {
	bool *terminated = static_cast<bool *>(p_context);
	if (terminated != nullptr) {
		*terminated = true;
	}
}

#else

GDKRuntime::GDKRuntime() {
}

GDKRuntime::~GDKRuntime() {
	shutdown();
}

Ref<GDKResult> GDKRuntime::initialize() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKRuntime::shutdown() {
	m_initialized = false;
	m_shutting_down = false;
	m_active_pending_signals.clear();
}

int GDKRuntime::dispatch() {
	return 0;
}

bool GDKRuntime::is_initialized() const {
	return false;
}

bool GDKRuntime::is_shutting_down() const {
	return false;
}

bool GDKRuntime::is_available() const {
	return false;
}

XTaskQueueHandle GDKRuntime::get_task_queue() const {
	return nullptr;
}

void GDKRuntime::retain_pending_signal(const Ref<GDKPendingSignal> &p_pending_signal) {
}

void GDKRuntime::release_pending_signal(GDKPendingSignal *p_pending_signal) {
}

Ref<GDKPendingSignal> GDKRuntime::make_pending_signal() {
	Ref<GDKPendingSignal> pending;
	pending.instantiate();
	pending->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return pending;
}

Signal GDKRuntime::make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) {
	Ref<GDKPendingSignal> pending = make_pending_signal();
	Ref<GDKResult> result = GDKResult::error_result(p_hresult, p_code, p_message, p_data);
	pending->complete_deferred(result);
	return pending->get_completed_signal();
}

#endif
