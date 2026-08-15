/**************************************************************************/
/*  remote_control_registry.h                                             */
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

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class RemoteControlRegistry : public Object {
	GDCLASS(RemoteControlRegistry, Object);

	struct CommandEntry {
		String name;
		String description;
		Dictionary schema;
		Callable callable;
	};

	static RemoteControlRegistry *singleton;
	mutable Mutex mutex;
	HashMap<String, CommandEntry> commands;

protected:
	static void _bind_methods();

public:
	static RemoteControlRegistry *get_singleton();

	void register_command(const String &p_name, const Callable &p_callable, const String &p_description = String(), const Dictionary &p_schema = Dictionary());
	void unregister_command(const String &p_name);
	bool has_command(const String &p_name) const;
	Dictionary execute(const String &p_name, const Dictionary &p_args);
	Array list_commands() const;
	void clear();
	void register_builtins();

	RemoteControlRegistry();
	~RemoteControlRegistry();
};
