/**************************************************************************/
/*  luau_wait_signal_task.cpp                                             */
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

#include "core/object/class_db.h"
#include "scheduler/luau_wait_signal_task.h"

#include "scheduler/luau_wait_signal_task.h"

#include <lualib.h>

using namespace luau_module;

void LuauSignalWaiter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_signal"), &LuauSignalWaiter::on_signal);
}

void LuauSignalWaiter::setup(LuauWaitSignalTask *p_task, Signal p_signal) {
	task = p_task;
	signal = p_signal;
	callable = Callable(this, "on_signal");
	p_signal.connect(callable);
}

void LuauSignalWaiter::disconnect_signal() {
	if (!signal.is_null() && callable.is_valid()) {
		signal.disconnect(callable);
		signal = Signal();
	}
}

void LuauSignalWaiter::on_signal() {
	if (task) {
		task->mark_signal_received();
	}
}

LuauWaitSignalTask::LuauWaitSignalTask(lua_State *p_L, Signal p_signal, double p_timeout_secs) :
		LuauScheduledTask(p_L) {
	until_timeout_usec = (uint64_t)(p_timeout_secs * 1e6);
	waiter.instantiate();
	waiter->setup(this, p_signal);
}

LuauWaitSignalTask::~LuauWaitSignalTask() {
	if (waiter.is_valid()) {
		waiter->disconnect_signal();
	}
}

bool LuauWaitSignalTask::is_complete() {
	return got_signal || timed_out;
}

bool LuauWaitSignalTask::should_resume() {
	return got_signal;
}

int LuauWaitSignalTask::push_results(lua_State *p_L) {
	lua_pushboolean(p_L, got_signal);
	return 1;
}

void LuauWaitSignalTask::update(double p_delta) {
	if (got_signal) {
		return;
	}
	const uint64_t delta_usec = (uint64_t)(p_delta * 1e6);
	if (delta_usec >= until_timeout_usec) {
		timed_out = true;
		until_timeout_usec = 0;
		if (waiter.is_valid()) {
			waiter->disconnect_signal();
		}
	} else {
		until_timeout_usec -= delta_usec;
	}
}

void LuauWaitSignalTask::mark_signal_received() {
	got_signal = true;
	if (waiter.is_valid()) {
		waiter->disconnect_signal();
	}
}
