/**************************************************************************/
/*  justamcp_agent_policy.h                                               */
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

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class JustAMCPToolExecutor;

class JustAMCPAgentPolicy {
public:
	static String instance_bearer();
	static bool bearer_authorizes(const String &p_authorization);
	static bool require_local_bearer();
	static bool save_requires_confirmation();

	static void open_session(const String &p_id, const String &p_name, bool p_from_initialize);
	static void close_session(const String &p_id);
	static Array list_sessions();
	static String current_session_id();
	static Dictionary session_state(const String &p_id);
	static Dictionary set_access(const String &p_id, const String &p_mode);

	static bool before_execute(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_early);
	static Dictionary after_execute(const String &p_tool_name, const Dictionary &p_args, Dictionary p_result);

	static void note_grouped_undo(int p_steps);
	static int take_grouped_undo();
	static void clear_grouped_undo();
	static int undo_snapshots(int p_steps);

	static void note_tool_name(const String &p_name);
	static void attach_annotations(Dictionary &p_schema);
	static String suggest_tool(const String &p_name);

	static bool read_extra_guide(const String &p_slug, String &r_title, String &r_body);
	static bool can_read_agent_resource(const String &p_canonical);
	static Dictionary read_agent_resource(const String &p_uri, const String &p_canonical);

	static String client_config(const String &p_client);
	static Dictionary write_client_config(const String &p_client, const String &p_path);

	static int probe_value();
	static void probe_reset();
	static int probe_increment();

	static Dictionary claim_path(const String &p_path, bool p_subtree);
	static Dictionary release_claim(const String &p_path);
	static Array list_claims();

	static Dictionary make_checkpoint(const Array &p_paths);
	static Array list_checkpoints();
	static Dictionary diff_checkpoint(const String &p_id);
	static Dictionary restore_checkpoint(const String &p_id);
	static String last_checkpoint_id();

	static Dictionary apply_change_plan(const String &p_plan_id);
	static Dictionary revert_change_plan(const String &p_plan_id);

	static Array audit_log();
	static Dictionary usage_report();
	static void store_screenshot_summary(const String &p_summary);

	static Dictionary commit_knobs(const Dictionary &p_knobs);
	static Dictionary current_knobs();
};

#endif
