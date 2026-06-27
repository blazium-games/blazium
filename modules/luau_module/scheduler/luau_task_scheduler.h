/**************************************************************************/
/*  luau_task_scheduler.h                                                 */
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

#include "core/templates/list.h"
#include "core/templates/pair.h"

struct lua_State;

namespace luau_module {

class LuauScheduledTask {
	int thread_ref = -1;

protected:
	lua_State *L = nullptr;

public:
	virtual ~LuauScheduledTask();

	explicit LuauScheduledTask(lua_State *p_L);
	int get_thread_ref() const { return thread_ref; }

	virtual bool is_complete() = 0;
	virtual bool should_resume() { return true; }
	virtual int push_results(lua_State *p_L) = 0;
	virtual void update(double p_delta) = 0;
};

class LuauWaitTask : public LuauScheduledTask {
	uint64_t remaining_usec = 0;
	uint64_t start_time_usec = 0;

public:
	LuauWaitTask(lua_State *p_L, double p_duration_secs);

	bool is_complete() override;
	int push_results(lua_State *p_L) override;
	void update(double p_delta) override;
};

class LuauTaskScheduler {
	List<Pair<lua_State *, LuauScheduledTask *>> tasks;

public:
	void frame(double p_delta);
	void register_task(lua_State *p_L, LuauScheduledTask *p_task);
};

void install_scheduler_libs(lua_State *p_L, LuauTaskScheduler *p_scheduler);

} //namespace luau_module
