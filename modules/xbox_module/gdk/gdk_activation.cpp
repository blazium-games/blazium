/**************************************************************************/
/*  gdk_activation.cpp                                                    */
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

#include "gdk_activation.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "core/error/error_macros.h"
#include "core/variant/variant.h"

#include "gdk.h"
#include "gdk_result.h"
#include "gdk_runtime.h"

namespace {

String _utf8_or_empty(const char *p_value) {
	if (p_value == nullptr) {
		return String();
	}
	return String::utf8(p_value);
}

String _normalize_invite_action(const String &p_action) {
	if (p_action == "inviteHandleAccept") {
		return "invite_handle_accept";
	}
	if (p_action == "activityHandleJoin") {
		return "activity_handle_join";
	}
	return p_action.to_lower();
}

String _normalize_invite_key(const String &p_key) {
	if (p_key == "invitedXuid") {
		return "invited_xuid";
	}
	if (p_key == "senderXuid") {
		return "sender_xuid";
	}
	if (p_key == "joinerXuid") {
		return "joiner_xuid";
	}
	if (p_key == "joineeXuid") {
		return "joinee_xuid";
	}
	return p_key.to_lower();
}

} //namespace

void GDKActivation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("accept_pending_invite", "invite_uri"), &GDKActivation::accept_pending_invite);

	ADD_SIGNAL(MethodInfo("protocol_activated", PropertyInfo(Variant::STRING, "uri")));
	ADD_SIGNAL(MethodInfo("file_activated", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("pending_invite_received", PropertyInfo(Variant::DICTIONARY, "invite")));
	ADD_SIGNAL(MethodInfo("invite_accepted", PropertyInfo(Variant::DICTIONARY, "invite")));
	ADD_SIGNAL(MethodInfo("activated", PropertyInfo(Variant::DICTIONARY, "info")));

	BIND_ENUM_CONSTANT(ACTIVATION_TYPE_PROTOCOL);
	BIND_ENUM_CONSTANT(ACTIVATION_TYPE_FILE);
	BIND_ENUM_CONSTANT(ACTIVATION_TYPE_PENDING_GAME_INVITE);
	BIND_ENUM_CONSTANT(ACTIVATION_TYPE_ACCEPTED_GAME_INVITE);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKActivation::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKActivation::on_runtime_initialized() {
	GDKRuntime *runtime = m_owner != nullptr ? m_owner->get_runtime() : nullptr;
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"Cannot initialize the activation service before the GDK runtime.");
	}

	if (m_activation_registered) {
		m_runtime_ready = true;
		return GDKResult::ok_result();
	}

	HRESULT hr = XGameActivationRegisterForEvent(
			runtime->get_task_queue(),
			this,
			_activation_callback,
			&m_activation_token);
	if (FAILED(hr)) {
		char hr_buf[16];
		std::snprintf(hr_buf, sizeof(hr_buf), "0x%08X", static_cast<unsigned int>(hr));
		WARN_PRINT(String("[GDK] XGameActivationRegisterForEvent failed (HRESULT ") +
				String(hr_buf) +
				") — activation inbound events disabled, accept_pending_invite still available.");
		m_activation_registered = false;
		m_runtime_ready = true;
		return GDKResult::ok_result();
	}

	m_runtime_ready = true;
	m_activation_registered = true;
	return GDKResult::ok_result();
}

void GDKActivation::shutdown() {
	m_runtime_ready = false;

	if (m_activation_registered) {
		XGameActivationUnregisterForEvent(m_activation_token, true);
		m_activation_registered = false;
		m_activation_token = {};
	}

	m_activation_listeners.clear();
}

uint64_t GDKActivation::add_activation_listener(std::function<void(const Dictionary &)> p_callback) {
	if (!p_callback) {
		return 0;
	}

	ActivationListener listener;
	listener.id = m_next_activation_listener_id++;
	listener.callback = std::move(p_callback);
	m_activation_listeners.push_back(std::move(listener));
	return m_activation_listeners.back().id;
}

void GDKActivation::remove_activation_listener(uint64_t p_listener_id) {
	if (p_listener_id == 0) {
		return;
	}

	m_activation_listeners.erase(
			std::remove_if(
					m_activation_listeners.begin(),
					m_activation_listeners.end(),
					[p_listener_id](const ActivationListener &p_listener) {
						return p_listener.id == p_listener_id;
					}),
			m_activation_listeners.end());
}

void GDKActivation::notify_activation_listeners_internal(const Dictionary &p_info) {
	if (!m_runtime_ready) {
		return;
	}

	std::vector<std::function<void(const Dictionary &)>> callbacks;
	callbacks.reserve(m_activation_listeners.size());
	for (const ActivationListener &listener : m_activation_listeners) {
		if (listener.callback) {
			callbacks.push_back(listener.callback);
		}
	}

	for (const std::function<void(const Dictionary &)> &callback : callbacks) {
		if (!m_runtime_ready) {
			break;
		}
		callback(p_info);
	}
}

Dictionary GDKActivation::make_invite_dictionary_internal(const String &p_uri, const String &p_activation_type) {
	Dictionary data;
	data["raw_uri"] = p_uri;
	data["activation_type"] = p_activation_type;

	const String uri = p_uri.strip_edges();
	const int64_t scheme_separator = uri.find("://");
	String remainder = uri;
	if (scheme_separator >= 0) {
		data["scheme"] = uri.substr(0, scheme_separator);
		remainder = uri.substr(scheme_separator + 3);
	} else {
		data["scheme"] = String();
	}

	const int64_t query_separator = remainder.find("?");
	String action = remainder;
	String query;
	if (query_separator >= 0) {
		action = remainder.substr(0, query_separator);
		query = remainder.substr(query_separator + 1);
	}

	data["action"] = _normalize_invite_action(action);

	if (query.begins_with("&")) {
		query = query.substr(1);
	}

	PackedStringArray query_parts = query.split("&", false);
	for (int64_t i = 0; i < query_parts.size(); ++i) {
		const String pair = query_parts[i];
		if (pair.is_empty()) {
			continue;
		}

		const int64_t equals_index = pair.find("=");
		const String key = equals_index >= 0 ? pair.substr(0, equals_index) : pair;
		const String value = equals_index >= 0 ? pair.substr(equals_index + 1) : String();
		data[_normalize_invite_key(key)] = value.uri_decode();
	}

	return data;
}

Ref<GDKResult> GDKActivation::accept_pending_invite(const String &p_invite_uri) {
	if (!m_runtime_ready) {
		return GDKResult::error_result(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}

	const String invite_uri = p_invite_uri.strip_edges();
	if (invite_uri.is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_invite_uri",
				"A non-empty invite URI is required.");
	}

	const CharString invite_uri_utf8 = invite_uri.utf8();
	HRESULT hr = XGameActivationAcceptPendingInvite(invite_uri_utf8.get_data());
	if (FAILED(hr)) {
		Dictionary data;
		data["invite_uri"] = invite_uri;
		return GDKResult::hresult_error(
				hr,
				"Failed to accept pending invite.",
				"accept_pending_invite_failed",
				data);
	}

	Dictionary data;
	data["invite_uri"] = invite_uri;
	return GDKResult::ok_result(data);
}

void GDKActivation::handle_activation_internal(const XGameActivationInfo *p_activation_info) {
	if (!m_runtime_ready || p_activation_info == nullptr) {
		return;
	}

	Dictionary info;
	info["type"] = static_cast<int64_t>(p_activation_info->type);

	switch (p_activation_info->type) {
		case XGameActivationType::Protocol: {
			const String uri = _utf8_or_empty(p_activation_info->protocolUri);
			info["uri"] = uri;
			emit_signal("protocol_activated", uri);
			break;
		}
		case XGameActivationType::File: {
			const String file = _utf8_or_empty(p_activation_info->file);
			info["file"] = file;
			emit_signal("file_activated", file);
			break;
		}
		case XGameActivationType::PendingGameInvite: {
			const String invite_uri = _utf8_or_empty(p_activation_info->inviteUri);
			Dictionary invite = make_invite_dictionary_internal(invite_uri, "pending_game_invite");
			info["invite_uri"] = invite_uri;
			info["invite"] = invite;
			emit_signal("pending_invite_received", invite);
			break;
		}
		case XGameActivationType::AcceptedGameInvite: {
			const String invite_uri = _utf8_or_empty(p_activation_info->inviteUri);
			Dictionary invite = make_invite_dictionary_internal(invite_uri, "accepted_game_invite");
			info["invite_uri"] = invite_uri;
			info["invite"] = invite;
			emit_signal("invite_accepted", invite);
			break;
		}
		default:
			break;
	}

	if (!m_runtime_ready) {
		return;
	}

	notify_activation_listeners_internal(info);
	if (!m_runtime_ready) {
		return;
	}

	emit_signal("activated", info);
}

void CALLBACK GDKActivation::_activation_callback(void *p_context, const XGameActivationInfo *p_activation_info) {
	auto *service = static_cast<GDKActivation *>(p_context);
	if (service != nullptr) {
		service->handle_activation_internal(p_activation_info);
	}
}

#else
void GDKActivation::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKActivation::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKActivation::shutdown() {}

void GDKActivation::remove_activation_listener(uint64_t p_listener_id) {}

void GDKActivation::notify_activation_listeners_internal(const Dictionary &p_info) {}

Dictionary GDKActivation::make_invite_dictionary_internal(const String &p_uri, const String &p_activation_type) {
	return Dictionary();
}

Ref<GDKResult> GDKActivation::accept_pending_invite(const String &p_invite_uri) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKActivation::handle_activation_internal(const XGameActivationInfo *p_activation_info) {}
#endif
