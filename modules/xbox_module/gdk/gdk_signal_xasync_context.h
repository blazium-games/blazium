/**************************************************************************/
/*  gdk_signal_xasync_context.h                                           */
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

#include "gdk_gdk_stubs.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XAsync.h>
#endif

#include "gdk_pending_signal.h"
#include "gdk_runtime.h"

class GDKSignalXAsyncContext {
	XAsyncBlock m_async_block = {};

	static void CALLBACK _completion_thunk(XAsyncBlock *p_async_block);

protected:
	GDKRuntime *m_runtime = nullptr;
	Ref<GDKPendingSignal> m_pending_signal;

	virtual void finalize(XAsyncBlock *p_async_block) = 0;

public:
	GDKSignalXAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal);
	virtual ~GDKSignalXAsyncContext() = default;

	XAsyncBlock *get_async_block();
	GDKRuntime *get_runtime() const;
	Ref<GDKPendingSignal> get_pending_signal() const;
	void bind_cancel_handler();
	void clear_cancel_handler();
};
