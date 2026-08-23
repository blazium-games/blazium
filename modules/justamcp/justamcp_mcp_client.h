/**************************************************************************/
/*  justamcp_mcp_client.h                                                 */
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
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/safe_refcount.h"

class JustAMCPMCPClient : public RefCounted {
	GDCLASS(JustAMCPMCPClient, RefCounted);

public:
	struct CacheEntry {
		Dictionary payload;
		uint64_t expires_usec = 0;
	};

protected:
	static void _bind_methods();

	Dictionary config;
	String negotiated_protocol = "2026-07-28";
	bool modern_era = true;
	bool connected = false;
	String last_error;
	String last_status = "disconnected";

	mutable Mutex cache_mutex;
	HashMap<String, CacheEntry> result_cache;

	Thread listen_thread;
	SafeFlag listen_stop;
	SafeFlag listen_running;

	Dictionary _rpc(const String &p_method, const Dictionary &p_params, bool p_use_cache = false);
	Dictionary _rpc_once(const String &p_method, const Dictionary &p_params);
	void _attach_client_meta(Dictionary &r_params) const;
	Dictionary _handle_input_required(const String &p_method, const Dictionary &p_params, const Dictionary &p_result);
	void _listen_worker();
	static void _listen_thread_cb(void *p_userdata);

public:
	static Dictionary client_capabilities();
	static Dictionary client_info();
	static void attach_client_meta(Dictionary &r_params, const String &p_protocol, bool p_debug_log);
	static Dictionary build_json_rpc(const String &p_method, const Dictionary &p_params, int64_t p_id);

	void configure(const Dictionary &p_config);
	Dictionary get_config() const { return config; }
	String get_name() const;
	String get_url() const;
	String get_negotiated_protocol() const { return negotiated_protocol; }
	bool is_modern() const { return modern_era; }
	bool is_remote_connected() const { return connected; }
	String get_status() const { return last_status; }

	Dictionary connect_remote();
	void disconnect_remote();
	Dictionary status() const;

	Dictionary discover();
	Dictionary tools_list(const Dictionary &p_params = Dictionary());
	Dictionary tools_call(const Dictionary &p_params);
	Dictionary prompts_list(const Dictionary &p_params = Dictionary());
	Dictionary prompts_get(const Dictionary &p_params);
	Dictionary resources_list(const Dictionary &p_params = Dictionary());
	Dictionary resources_templates_list(const Dictionary &p_params = Dictionary());
	Dictionary resources_read(const Dictionary &p_params);
	Dictionary completion_complete(const Dictionary &p_params);
	Dictionary notifications_cancelled(const Dictionary &p_params);
	Dictionary start_subscriptions_listen();
	void stop_subscriptions_listen();

	Dictionary rpc(const String &p_method, const Dictionary &p_params);

	JustAMCPMCPClient();
	~JustAMCPMCPClient();
};

#endif
