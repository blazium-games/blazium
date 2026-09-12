/**************************************************************************/
/*  gdk_signal_xasync_context.cpp                                         */
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

#include "gdk_signal_xasync_context.h"

#ifdef XBOX_MODULE_GDK_ENABLED

GDKSignalXAsyncContext::GDKSignalXAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal) :
		m_runtime(p_runtime),
		m_pending_signal(p_pending_signal) {
	m_async_block.queue = m_runtime != nullptr ? m_runtime->get_task_queue() : nullptr;
	m_async_block.context = this;
	m_async_block.callback = _completion_thunk;
}

XAsyncBlock *GDKSignalXAsyncContext::get_async_block() {
	return &m_async_block;
}

GDKRuntime *GDKSignalXAsyncContext::get_runtime() const {
	return m_runtime;
}

Ref<GDKPendingSignal> GDKSignalXAsyncContext::get_pending_signal() const {
	return m_pending_signal;
}

void GDKSignalXAsyncContext::bind_cancel_handler() {
	if (m_pending_signal.is_valid()) {
		m_pending_signal->set_cancel_handler([this]() {
			XAsyncCancel(&m_async_block);
		});
	}
}

void GDKSignalXAsyncContext::clear_cancel_handler() {
	if (m_pending_signal.is_valid()) {
		m_pending_signal->clear_cancel_handler();
	}
}

void CALLBACK GDKSignalXAsyncContext::_completion_thunk(XAsyncBlock *p_async_block) {
	auto *context = static_cast<GDKSignalXAsyncContext *>(p_async_block->context);

	context->clear_cancel_handler();
	context->finalize(p_async_block);
	delete context;
}

#else

GDKSignalXAsyncContext::GDKSignalXAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal) :
		m_runtime(p_runtime),
		m_pending_signal(p_pending_signal) {
}

XAsyncBlock *GDKSignalXAsyncContext::get_async_block() {
	return nullptr;
}

GDKRuntime *GDKSignalXAsyncContext::get_runtime() const {
	return m_runtime;
}

Ref<GDKPendingSignal> GDKSignalXAsyncContext::get_pending_signal() const {
	return m_pending_signal;
}

void GDKSignalXAsyncContext::bind_cancel_handler() {
}

void GDKSignalXAsyncContext::clear_cancel_handler() {
}

#endif
