/**************************************************************************/
/*  justamcp_project_registry.h                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class JustAMCPProjectRegistry {
public:
	static bool is_valid_entry_name(const String &p_name);
	static void register_tool(const String &p_name, const String &p_description, const Dictionary &p_input_schema, const Callable &p_callable);
	static void unregister_tool(const String &p_name);
	static bool has_tool(const String &p_name);
	static Array list_tool_schemas();
	static Dictionary list_tools();
	static Dictionary call_tool(const String &p_name, const Dictionary &p_args);
	static Dictionary call_tool_mcp(const String &p_name, const Dictionary &p_args);

	static void register_prompt(const String &p_name, const String &p_description, const Callable &p_callable);
	static void unregister_prompt(const String &p_name);
	static bool has_prompt(const String &p_name);
	static Array list_prompt_schemas();
	static Dictionary call_prompt(const String &p_name, const Dictionary &p_args);

	static void clear();

private:
	struct ToolEntry {
		String name;
		String description;
		Dictionary input_schema;
		Callable callable;
	};
	struct PromptEntry {
		String name;
		String description;
		Callable callable;
	};

	static HashMap<String, ToolEntry> &tools();
	static HashMap<String, PromptEntry> &prompts();
};
