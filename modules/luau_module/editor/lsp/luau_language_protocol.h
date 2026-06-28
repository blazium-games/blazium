/**************************************************************************/
/*  luau_language_protocol.h                                              */
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

#include "editor/lsp/luau_text_document.h"
#include "editor/lsp/luau_workspace.h"

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_JSONRPC_ENABLED
#include "modules/jsonrpc/jsonrpc.h"
#else
#define LUAU_NO_LSP
#endif

#ifndef LUAU_NO_LSP

#define LUAU_LSP_MAX_BUFFER_SIZE 4194304
#define LUAU_LSP_MAX_CLIENTS 8

class LuauLanguageProtocol : public JSONRPC {
	GDCLASS(LuauLanguageProtocol, JSONRPC);

private:
	struct LSPeer : RefCounted {
		Ref<StreamPeerTCP> connection;
		uint8_t req_buf[LUAU_LSP_MAX_BUFFER_SIZE];
		int req_pos = 0;
		bool has_header = false;
		int content_length = 0;
		Vector<CharString> res_queue;
		int res_sent = 0;

		Error handle_data();
		Error send_data();
	};

	static LuauLanguageProtocol *singleton;

	HashMap<int, Ref<LSPeer>> clients;
	Ref<TCPServer> server;
	int latest_client_id = 0;
	int next_client_id = 0;
	bool _initialized = false;

	Ref<LuauTextDocument> text_document;
	Ref<LuauWorkspace> workspace;

	Error on_client_connected();
	void on_client_disconnected(int p_client_id);
	String process_message(const String &p_text);
	String format_output(const String &p_text);

protected:
	static void _bind_methods();

	Dictionary initialize(const Dictionary &p_params);
	void initialized(const Variant &p_params);
	void shutdown(const Variant &p_params);

public:
	static LuauLanguageProtocol *get_singleton() { return singleton; }
	bool is_initialized() const { return _initialized; }

	void poll(int p_limit_usec);
	Error start(int p_port, const IPAddress &p_bind_ip);
	void stop();
	void notify_client(const String &p_method, const Variant &p_params = Variant(), int p_client_id = -1);

	LuauLanguageProtocol();
};

#endif

#endif
