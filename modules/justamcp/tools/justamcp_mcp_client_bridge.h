/**************************************************************************/
/*  justamcp_mcp_client_bridge.h                                          */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_set.h"

class JustAMCPMCPClientBridge : public Object {
	GDCLASS(JustAMCPMCPClientBridge, Object);

	static JustAMCPMCPClientBridge *singleton;

protected:
	static void _bind_methods();

	Dictionary _rpc_request(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const;
	Dictionary _rpc_request_sync(const String &p_bridge_name, const String &p_method, const Dictionary &p_params) const;
	Dictionary _get_bridge_config(const String &p_bridge_name) const;
	Error _ensure_initialized(const String &p_bridge_name) const;
	void _save_bridges(const Array &p_bridges) const;
	Array _load_bridges() const;

	mutable HashSet<String> initialized_bridges;
	mutable Mutex initialized_bridges_mutex;

	bool _try_schedule_remote_tool(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_pending) const;

public:
	Dictionary _execute_remote_tool_sync(const String &p_tool_name, const Dictionary &p_args);

	static JustAMCPMCPClientBridge *get_singleton();

	static Array get_tool_schemas(bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	Array provide_tool_schemas(bool p_register_only = false, bool p_ignore_settings = false, bool p_include_disabled_tools = false);
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary list_bridges(const Dictionary &p_args);
	Dictionary add_bridge(const Dictionary &p_args);
	Dictionary list_remote_tools(const Dictionary &p_args);
	Dictionary call_remote_tool(const Dictionary &p_args);
	Dictionary read_remote_resource(const Dictionary &p_args);

	JustAMCPMCPClientBridge();
	~JustAMCPMCPClientBridge();
};

#endif
