/**************************************************************************/
/*  luau_language_protocol.cpp                                            */
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

#ifdef TOOLS_ENABLED

#ifndef LUAU_NO_LSP

#include "editor/lsp/luau_language_protocol.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/os.h"

LuauLanguageProtocol *LuauLanguageProtocol::singleton = nullptr;

Error LuauLanguageProtocol::LSPeer::handle_data() {
	int read = 0;
	if (!has_header) {
		while (true) {
			if (req_pos >= LUAU_LSP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				return ERR_OUT_OF_MEMORY;
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
			if (err != OK) {
				return FAILED;
			}
			if (read != 1) {
				return ERR_BUSY;
			}
			char *r = (char *)req_buf;
			int l = req_pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0';
				String header;
				header.parse_utf8(r);
				content_length = header.substr(16).to_int();
				has_header = true;
				req_pos = 0;
				break;
			}
			req_pos++;
		}
	}
	if (has_header) {
		while (req_pos < content_length) {
			if (req_pos >= LUAU_LSP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				has_header = false;
				return ERR_OUT_OF_MEMORY;
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
			if (err != OK) {
				return FAILED;
			}
			if (read != 1) {
				return ERR_BUSY;
			}
			req_pos++;
		}
		has_header = false;
		String message;
		message.parse_utf8((const char *)req_buf, content_length);
		req_pos = 0;
		LuauLanguageProtocol *protocol = LuauLanguageProtocol::get_singleton();
		if (protocol) {
			String response = protocol->process_message(message);
			if (!response.is_empty()) {
				res_queue.push_back(response.utf8());
			}
		}
	}
	return OK;
}

Error LuauLanguageProtocol::LSPeer::send_data() {
	while (!res_queue.is_empty()) {
		const CharString &msg = res_queue[0];
		if (res_sent < msg.length()) {
			int sent = 0;
			Error err = connection->put_partial_data(reinterpret_cast<const uint8_t *>(msg.get_data()) + res_sent, msg.length() - res_sent, sent);
			if (err != OK) {
				return err;
			}
			res_sent += sent;
			if (res_sent < msg.length()) {
				return ERR_BUSY;
			}
		}
		res_queue.remove_at(0);
		res_sent = 0;
	}
	return OK;
}

String LuauLanguageProtocol::format_output(const String &p_text) {
	CharString utf8 = p_text.utf8();
	return "Content-Length: " + itos(utf8.length()) + "\r\n\r\n" + p_text;
}

String LuauLanguageProtocol::process_message(const String &p_text) {
	const Variant parsed = JSON::parse_string(p_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary action = parsed;
	if (!action.has("method")) {
		return String();
	}
	const Variant result = process_action(action);
	if (result.get_type() == Variant::NIL) {
		return String();
	}
	Dictionary response;
	response["jsonrpc"] = "2.0";
	if (action.has("id")) {
		response["id"] = action["id"];
	}
	response["result"] = result;
	return format_output(Variant(response).to_json_string());
}

Error LuauLanguageProtocol::on_client_connected() {
	ERR_FAIL_COND_V(clients.size() >= LUAU_LSP_MAX_CLIENTS, ERR_UNAVAILABLE);
	Ref<StreamPeerTCP> connection = server->take_connection();
	ERR_FAIL_COND_V(connection.is_null(), FAILED);
	Ref<LSPeer> peer;
	peer.instantiate();
	peer->connection = connection;
	clients[next_client_id] = peer;
	latest_client_id = next_client_id;
	next_client_id++;
	return OK;
}

void LuauLanguageProtocol::on_client_disconnected(int p_client_id) {
	if (clients.has(p_client_id)) {
		clients.erase(p_client_id);
	}
}

void LuauLanguageProtocol::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "params"), &LuauLanguageProtocol::initialize);
	ClassDB::bind_method(D_METHOD("initialized", "params"), &LuauLanguageProtocol::initialized);
	ClassDB::bind_method(D_METHOD("shutdown", "params"), &LuauLanguageProtocol::shutdown);
}

Dictionary LuauLanguageProtocol::initialize(const Dictionary &p_params) {
	(void)p_params;
	if (!_initialized) {
		workspace->initialize();
		text_document->initialize();
		_initialized = true;
	}

	Dictionary result;
	Dictionary capabilities;
	Dictionary text_doc_caps;
	Dictionary completion_caps;
	Array triggers;
	triggers.push_back(".");
	triggers.push_back(":");
	triggers.push_back("_");
	completion_caps["triggerCharacters"] = triggers;
	text_doc_caps["completion"] = completion_caps;
	text_doc_caps["hover"] = Dictionary();
	text_doc_caps["definition"] = Dictionary();
	text_doc_caps["documentSymbol"] = Dictionary();
	text_doc_caps["formatting"] = Dictionary();
	text_doc_caps["rangeFormatting"] = Dictionary();
	capabilities["textDocument"] = text_doc_caps;
	result["capabilities"] = capabilities;
	Dictionary server_info;
	server_info["name"] = "Blazium Luau Language Server";
	server_info["version"] = "1.0";
	result["serverInfo"] = server_info;
	return result;
}

void LuauLanguageProtocol::initialized(const Variant &p_params) {
	(void)p_params;
}

void LuauLanguageProtocol::shutdown(const Variant &p_params) {
	(void)p_params;
	_initialized = false;
}

void LuauLanguageProtocol::poll(int p_limit_usec) {
	uint64_t target_ticks = OS::get_singleton()->get_ticks_usec() + p_limit_usec;

	if (server->is_connection_available()) {
		on_client_connected();
	}

	HashMap<int, Ref<LSPeer>>::Iterator E = clients.begin();
	while (E != clients.end()) {
		Ref<LSPeer> peer = E->value;
		peer->connection->poll();
		StreamPeerTCP::Status status = peer->connection->get_status();
		if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
			on_client_disconnected(E->key);
			E = clients.begin();
			continue;
		}

		Error err = OK;
		while (peer->connection->get_available_bytes() > 0) {
			latest_client_id = E->key;
			err = peer->handle_data();
			if (err != OK || OS::get_singleton()->get_ticks_usec() >= target_ticks) {
				break;
			}
		}

		if (err != OK && err != ERR_BUSY) {
			on_client_disconnected(E->key);
			E = clients.begin();
			continue;
		}

		err = peer->send_data();
		if (err != OK && err != ERR_BUSY) {
			on_client_disconnected(E->key);
			E = clients.begin();
			continue;
		}
		++E;
	}
}

Error LuauLanguageProtocol::start(int p_port, const IPAddress &p_bind_ip) {
	return server->listen(p_port, p_bind_ip);
}

void LuauLanguageProtocol::stop() {
	for (const KeyValue<int, Ref<LSPeer>> &E : clients) {
		E.value->connection->disconnect_from_host();
	}
	clients.clear();
	server->stop();
}

void LuauLanguageProtocol::notify_client(const String &p_method, const Variant &p_params, int p_client_id) {
	if (p_client_id == -1) {
		p_client_id = latest_client_id;
	}
	if (!clients.has(p_client_id)) {
		return;
	}
	Ref<LSPeer> peer = clients[p_client_id];
	Dictionary message = make_notification(p_method, p_params);
	String msg = Variant(message).to_json_string();
	msg = format_output(msg);
	peer->res_queue.push_back(msg.utf8());
}

LuauLanguageProtocol::LuauLanguageProtocol() {
	server.instantiate();
	singleton = this;
	workspace.instantiate();
	text_document.instantiate();
	set_scope("textDocument", text_document.ptr());
	set_scope("workspace", workspace.ptr());
	if (ProjectSettings::get_singleton()) {
		workspace->initialize();
	}
}

#endif

#endif
