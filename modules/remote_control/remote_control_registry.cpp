/**************************************************************************/
/*  remote_control_registry.cpp                                           */
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

#include "remote_control_registry.h"

#include "remote_control_builtins.h"

#include "core/object/class_db.h"

RemoteControlRegistry *RemoteControlRegistry::singleton = nullptr;

RemoteControlRegistry *RemoteControlRegistry::get_singleton() {
	return singleton;
}

void RemoteControlRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_command", "name", "callable", "description", "schema"), &RemoteControlRegistry::register_command, DEFVAL(String()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("unregister_command", "name"), &RemoteControlRegistry::unregister_command);
	ClassDB::bind_method(D_METHOD("has_command", "name"), &RemoteControlRegistry::has_command);
	ClassDB::bind_method(D_METHOD("execute", "name", "args"), &RemoteControlRegistry::execute);
	ClassDB::bind_method(D_METHOD("list_commands"), &RemoteControlRegistry::list_commands);
	ClassDB::bind_method(D_METHOD("clear"), &RemoteControlRegistry::clear);
	ClassDB::bind_method(D_METHOD("register_builtins"), &RemoteControlRegistry::register_builtins);
}

void RemoteControlRegistry::register_command(const String &p_name, const Callable &p_callable, const String &p_description, const Dictionary &p_schema) {
	ERR_FAIL_COND_MSG(p_name.is_empty(), "Remote control command name cannot be empty.");
	ERR_FAIL_COND_MSG(!p_callable.is_valid(), "Remote control command callable must be valid.");

	CommandEntry entry;
	entry.name = p_name;
	entry.description = p_description;
	entry.schema = p_schema;
	entry.callable = p_callable;
	MutexLock lock(mutex);
	commands[p_name] = entry;
}

void RemoteControlRegistry::unregister_command(const String &p_name) {
	MutexLock lock(mutex);
	commands.erase(p_name);
}

bool RemoteControlRegistry::has_command(const String &p_name) const {
	MutexLock lock(mutex);
	return commands.has(p_name);
}

Dictionary RemoteControlRegistry::execute(const String &p_name, const Dictionary &p_args) {
	Callable callable;
	{
		MutexLock lock(mutex);
		if (!commands.has(p_name)) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Unknown command: " + p_name;
			return ret;
		}
		const CommandEntry &entry = commands[p_name];
		if (!entry.callable.is_valid()) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Command callable is no longer valid: " + p_name;
			return ret;
		}
		callable = entry.callable;
	}

	Dictionary ret;
	Variant call_ret = callable.call(p_args);
	if (call_ret.get_type() == Variant::DICTIONARY) {
		Dictionary payload = call_ret;
		if (!payload.has("ok")) {
			payload["ok"] = !payload.has("error") && String(payload.get("type", "")) != "error";
		}
		return payload;
	}

	ret["ok"] = true;
	ret["result"] = call_ret;
	return ret;
}

Array RemoteControlRegistry::list_commands() const {
	List<CommandEntry> entries;
	{
		MutexLock lock(mutex);
		for (const KeyValue<String, CommandEntry> &E : commands) {
			entries.push_back(E.value);
		}
	}
	struct SortByName {
		bool operator()(const CommandEntry &a, const CommandEntry &b) const {
			return a.name < b.name;
		}
	};
	entries.sort_custom<SortByName>();

	Array out;
	for (const CommandEntry &entry : entries) {
		Dictionary item;
		item["name"] = entry.name;
		item["description"] = entry.description;
		item["schema"] = entry.schema;
		out.push_back(item);
	}
	return out;
}

void RemoteControlRegistry::clear() {
	MutexLock lock(mutex);
	commands.clear();
}

void RemoteControlRegistry::register_builtins() {
	remote_control_register_builtin_commands(this);
}

RemoteControlRegistry::RemoteControlRegistry() {
	singleton = this;
}

RemoteControlRegistry::~RemoteControlRegistry() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
