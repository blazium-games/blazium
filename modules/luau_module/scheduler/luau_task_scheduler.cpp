/**************************************************************************/
/*  luau_task_scheduler.cpp                                               */
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

#include "scheduler/luau_task_scheduler.h"

#include "bindings/variant.h"
#include "scheduler/luau_wait_signal_task.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/os/time.h"
#include <lualib.h>

using namespace luau_module;

namespace {

static LuauTaskScheduler *g_scheduler = nullptr;

static int resume_thread(lua_State *p_L, int p_nresults) {
	return lua_resume(p_L, nullptr, p_nresults);
}

} //namespace

LuauScheduledTask::LuauScheduledTask(lua_State *p_L) :
		L(p_L) {
	lua_pushthread(p_L);
	thread_ref = lua_ref(p_L, -1);
	lua_pop(p_L, 1);
}

LuauScheduledTask::~LuauScheduledTask() {
	if (L && thread_ref != -1) {
		lua_unref(L, thread_ref);
	}
}

LuauWaitTask::LuauWaitTask(lua_State *p_L, double p_duration_secs) :
		LuauScheduledTask(p_L) {
	remaining_usec = (uint64_t)(p_duration_secs * 1e6);
	start_time_usec = Time::get_singleton()->get_ticks_usec();
}

bool LuauWaitTask::is_complete() {
	return remaining_usec == 0;
}

int LuauWaitTask::push_results(lua_State *p_L) {
	const double actual = (Time::get_singleton()->get_ticks_usec() - start_time_usec) / 1e6;
	lua_pushnumber(p_L, actual);
	return 1;
}

void LuauWaitTask::update(double p_delta) {
	const uint64_t delta_usec = (uint64_t)(p_delta * 1e6);
	if (delta_usec >= remaining_usec) {
		remaining_usec = 0;
	} else {
		remaining_usec -= delta_usec;
	}
}

void LuauTaskScheduler::frame(double p_delta) {
	List<Pair<lua_State *, LuauScheduledTask *>>::Element *elem = tasks.front();
	while (elem) {
		Pair<lua_State *, LuauScheduledTask *> &entry = elem->get();
		lua_State *L = entry.first;
		LuauScheduledTask *task = entry.second;

		task->update(p_delta);

		if (task->is_complete()) {
			if (task->should_resume()) {
				const int results = task->push_results(L);
				const int status = resume_thread(L, results);
				if (status != LUA_OK && status != LUA_YIELD) {
					if (lua_gettop(L) > 0 && lua_isstring(L, -1)) {
						ERR_PRINT(String("LuauTaskScheduler resume error: ") + lua_tostring(L, -1));
						lua_pop(L, 1);
					}
				}
			}

			memdelete(task);
			List<Pair<lua_State *, LuauScheduledTask *>>::Element *to_remove = elem;
			elem = elem->next();
			to_remove->erase();
			continue;
		}

		elem = elem->next();
	}
}

void LuauTaskScheduler::register_task(lua_State *p_L, LuauScheduledTask *p_task) {
	tasks.push_front(Pair<lua_State *, LuauScheduledTask *>(p_L, p_task));
}

static int luau_wait(lua_State *p_L) {
	ERR_FAIL_NULL_V(g_scheduler, 0);
	const double duration = luaL_checknumber(p_L, 1);
	g_scheduler->register_task(p_L, memnew(LuauWaitTask(p_L, duration)));
	return lua_yield(p_L, 0);
}

static int luau_wait_signal(lua_State *p_L) {
	ERR_FAIL_NULL_V(g_scheduler, 0);
	const Variant sig_var = to_variant(p_L, 1);
	ERR_FAIL_COND_V(sig_var.get_type() != Variant::SIGNAL, 0);
	Signal signal = sig_var;
	const double timeout = luaL_optnumber(p_L, 2, 10.0);
	g_scheduler->register_task(p_L, memnew(LuauWaitSignalTask(p_L, signal, timeout)));
	return lua_yield(p_L, 0);
}

void luau_module::install_scheduler_libs(lua_State *p_L, LuauTaskScheduler *p_scheduler) {
	ERR_FAIL_NULL(p_L);
	g_scheduler = p_scheduler;
	lua_pushcfunction(p_L, luau_wait, "wait");
	lua_setglobal(p_L, "wait");
	lua_pushcfunction(p_L, luau_wait_signal, "wait_signal");
	lua_setglobal(p_L, "wait_signal");
	lua_pushcfunction(p_L, luau_wait_signal, "await");
	lua_setglobal(p_L, "await");
}
