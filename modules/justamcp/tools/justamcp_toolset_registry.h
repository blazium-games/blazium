/**************************************************************************/
/*  justamcp_toolset_registry.h                                           */
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

#ifdef TOOLS_ENABLED

#include "core/object/object.h"
#include "core/templates/hash_map.h"

class JustAMCPToolsetRegistry : public Object {
	GDCLASS(JustAMCPToolsetRegistry, Object);

public:
	struct ToolsetEntry {
		String name;
		String description;
		String category;
		Callable get_schemas;
		Callable execute_tool;
		Object *owned_provider = nullptr;
		bool enabled = true;
		int tool_count = 0;
	};

private:
	static JustAMCPToolsetRegistry *singleton;
	HashMap<String, ToolsetEntry> toolsets;
	Vector<Object *> owned_providers;

protected:
	static void _bind_methods();

public:
	static JustAMCPToolsetRegistry *get_singleton();

	void register_toolset(const String &p_name, const String &p_description, const Callable &p_get_schemas, const Callable &p_execute_tool);
	void register_toolset_with_owner(const String &p_name, const String &p_description, const Callable &p_get_schemas, const Callable &p_execute_tool, Object *p_owned_provider, const String &p_category = String());
	void unregister_toolset(const String &p_name);

	bool is_discovery_enabled() const;
	Array list_toolset_names() const;
	Dictionary list_toolsets() const;
	Dictionary describe_toolset(const String &p_name) const;
	Array collect_tool_schemas(const String &p_name, bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) const;
	Dictionary call_toolset(const String &p_toolset_name, const String &p_tool_name, const Dictionary &p_args) const;

	JustAMCPToolsetRegistry();
	~JustAMCPToolsetRegistry();
};

#endif
