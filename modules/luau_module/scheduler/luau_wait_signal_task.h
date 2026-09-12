/**************************************************************************/
/*  luau_wait_signal_task.h                                               */
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

#include "scheduler/luau_task_scheduler.h"

#include "core/object/ref_counted.h"
#include "core/variant/callable.h"

namespace luau_module {

class LuauWaitSignalTask;

class LuauSignalWaiter : public RefCounted {
	GDCLASS(LuauSignalWaiter, RefCounted);

	LuauWaitSignalTask *task = nullptr;
	Signal signal;
	Callable callable;

protected:
	static void _bind_methods();

public:
	void setup(LuauWaitSignalTask *p_task, Signal p_signal);
	void disconnect_signal();
	void on_signal();
};

class LuauWaitSignalTask : public LuauScheduledTask {
	uint64_t until_timeout_usec = 0;
	bool got_signal = false;
	bool timed_out = false;
	Ref<LuauSignalWaiter> waiter;

public:
	LuauWaitSignalTask(lua_State *p_L, Signal p_signal, double p_timeout_secs);
	~LuauWaitSignalTask() override;

	bool is_complete() override;
	bool should_resume() override;
	int push_results(lua_State *p_L) override;
	void update(double p_delta) override;

	void mark_signal_received();
};

} //namespace luau_module
