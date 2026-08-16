/**************************************************************************/
/*  justamcp_json_rpc_router.h                                            */
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

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class JustAMCPResourceExecutor;
class JustAMCPTaskManager;
class JustAMCPPromptExecutor;
class JustAMCPServer;

class JustAMCPJsonRpcRouter {
public:
	static String extract_list_cursor(const Dictionary &p_payload);
	static Dictionary finalize_list_result(const Dictionary &p_result, const Variant &p_req_id);
	static Dictionary finalize_action_result(const Dictionary &p_result, const Variant &p_req_id);
	static Dictionary make_invalid_params(const Variant &p_req_id, const String &p_message);
	static Dictionary route(const String &p_method, const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPResourceExecutor *p_resources, JustAMCPTaskManager *p_tasks);
	static Dictionary route_tools_list(const String &p_cursor, const Variant &p_req_id_var);
	static Dictionary route_prompts_list(const String &p_cursor, const Variant &p_req_id_var, JustAMCPPromptExecutor *p_prompts);
	static Dictionary route_prompts_get(const Dictionary &p_payload, const Variant &p_req_id_var, JustAMCPPromptExecutor *p_prompts);
	static Dictionary route_initialize(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var);
	static Dictionary route_discover(JustAMCPServer *p_server, const Variant &p_req_id_var);
	static Dictionary route_ping(const Variant &p_req_id_var);
	static Dictionary route_logging_set_level(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var);
	static Dictionary route_tasks_cancel(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var);
	static Dictionary route_completion_complete(JustAMCPServer *p_server, const Dictionary &p_payload, const Variant &p_req_id_var);
};

#endif
