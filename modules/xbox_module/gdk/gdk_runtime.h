/**************************************************************************/
/*  gdk_runtime.h                                                         */
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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <vector>

#include "core/variant/callable.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XGameRuntimeInit.h>
#include <XTaskQueue.h>
#endif

#include "gdk_gdk_stubs.h"

class GDKPendingSignal;
class GDKResult;

class GDKRuntime {
	bool m_initialized = false;
	bool m_shutting_down = false;

	bool m_xgame_runtime_initialized = false;
	XTaskQueueHandle m_task_queue = nullptr;
	std::vector<Ref<GDKPendingSignal>> m_active_pending_signals;

	static void CALLBACK _queue_terminated(void *p_context);

public:
	GDKRuntime();
	~GDKRuntime();

	Ref<GDKResult> initialize();
	void shutdown();
	int dispatch();

	bool is_initialized() const;
	bool is_shutting_down() const;
	bool is_available() const;
	XTaskQueueHandle get_task_queue() const;

	void retain_pending_signal(const Ref<GDKPendingSignal> &p_pending_signal);
	void release_pending_signal(GDKPendingSignal *p_pending_signal);
	Ref<GDKPendingSignal> make_pending_signal();
	Signal make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant());
};
