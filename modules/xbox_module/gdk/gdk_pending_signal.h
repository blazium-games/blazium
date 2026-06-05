/**************************************************************************/
/*  gdk_pending_signal.h                                                  */
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

#include <functional>

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#include "gdk_result.h"

class GDKPendingSignal : public RefCounted {
	GDCLASS(GDKPendingSignal, RefCounted);

	bool m_done = false;
	bool m_cancel_requested = false;
	bool m_deferred_completion_queued = false;
	Ref<GDKResult> m_result;
	Ref<GDKResult> m_deferred_result;
	Ref<GDKPendingSignal> m_self_ref;
	std::function<void()> m_cancel_handler;
	std::function<void(GDKPendingSignal *)> m_release_handler;

	void _emit_deferred_completion();

protected:
	static void _bind_methods();
	bool request_cancel();
	void invoke_cancel_handler();

public:
	bool is_done() const;
	bool was_cancel_requested() const;
	Signal get_completed_signal() const;

	void cancel();
	void complete(const Ref<GDKResult> &p_result);
	void complete_deferred(const Ref<GDKResult> &p_result);

	void set_cancel_handler(std::function<void()> p_handler);
	void clear_cancel_handler();
	void set_release_handler(std::function<void(GDKPendingSignal *)> p_handler);
	void clear_release_handler();
};
