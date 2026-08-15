/**************************************************************************/
/*  justamcp_asset_tags_tools.h                                           */
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

class JustAMCPAssetTagsTools : public Object {
	GDCLASS(JustAMCPAssetTagsTools, Object);

protected:
	static void _bind_methods();

	Dictionary _make_error(int p_code, const String &p_message) const;
	Dictionary _require_elicitation(const String &p_action, const Dictionary &p_args) const;
	String _normalize_tool_name(const String &p_tool_name) const;
	String _error_message_for_code(Error p_error) const;

public:
	static Array get_tool_schemas(bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	Array provide_tool_schemas(bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary tags_list(const Dictionary &p_args);
	Dictionary tags_get_info(const Dictionary &p_args);
	Dictionary tags_add(const Dictionary &p_args);
	Dictionary tags_remove(const Dictionary &p_args);
	Dictionary tags_rename(const Dictionary &p_args);
	Dictionary tags_update_comment(const Dictionary &p_args);
	Dictionary tags_find_assets(const Dictionary &p_args);
	Dictionary tags_set_on_asset(const Dictionary &p_args);
	Dictionary tags_get_on_asset(const Dictionary &p_args);
	Dictionary tags_add_to_asset(const Dictionary &p_args);
	Dictionary tags_remove_from_asset(const Dictionary &p_args);
	Dictionary tags_batch_set_on_assets(const Dictionary &p_args);
	Dictionary tags_search_assets(const Dictionary &p_args);
	Dictionary tags_get_unused(const Dictionary &p_args);
	Dictionary tags_rescan(const Dictionary &p_args);
	Dictionary tags_can_undo(const Dictionary &p_args);
	Dictionary tags_undo_last_change(const Dictionary &p_args);

	JustAMCPAssetTagsTools();
	~JustAMCPAssetTagsTools();
};

#endif
