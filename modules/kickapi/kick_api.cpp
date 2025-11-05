/**************************************************************************/
/*  kick_api.cpp                                                          */
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

#include "kick_api.h"

#include "core/config/engine.h"

void KickAPI::_bind_methods() {
	// Configuration
	ClassDB::bind_method(D_METHOD("configure", "access_token"), &KickAPI::configure);
	ClassDB::bind_method(D_METHOD("set_access_token", "token"), &KickAPI::set_access_token);
	ClassDB::bind_method(D_METHOD("poll"), &KickAPI::poll);

	// Utility methods
	ClassDB::bind_method(D_METHOD("query_string_from_dict", "params"), &KickAPI::query_string_from_dict);
	ClassDB::bind_method(D_METHOD("get_rate_limit_remaining"), &KickAPI::get_rate_limit_remaining);
	ClassDB::bind_method(D_METHOD("get_rate_limit_reset"), &KickAPI::get_rate_limit_reset);
	ClassDB::bind_method(D_METHOD("is_busy"), &KickAPI::is_busy);

	// Category handlers
	ClassDB::bind_method(D_METHOD("get_categories"), &KickAPI::get_categories);
	ClassDB::bind_method(D_METHOD("get_channels"), &KickAPI::get_channels);
	ClassDB::bind_method(D_METHOD("get_chat"), &KickAPI::get_chat);
	ClassDB::bind_method(D_METHOD("get_events"), &KickAPI::get_events);
	ClassDB::bind_method(D_METHOD("get_livestreams"), &KickAPI::get_livestreams);
	ClassDB::bind_method(D_METHOD("get_moderation"), &KickAPI::get_moderation);
	ClassDB::bind_method(D_METHOD("get_oauth"), &KickAPI::get_oauth);
	ClassDB::bind_method(D_METHOD("get_users"), &KickAPI::get_users);

	// Signals - one generic signal for all responses
	ADD_SIGNAL(MethodInfo("request_completed", PropertyInfo(Variant::STRING, "signal_name"),
			PropertyInfo(Variant::INT, "response_code"), PropertyInfo(Variant::DICTIONARY, "data")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "signal_name"),
			PropertyInfo(Variant::INT, "error_code"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("rate_limit_warning", PropertyInfo(Variant::INT, "remaining"), PropertyInfo(Variant::INT, "reset_time")));
}

KickAPI::KickAPI() {
	// Initialize HTTP client
	http_client.instantiate();

	// Initialize category handlers
	categories.instantiate();
	channels.instantiate();
	chat.instantiate();
	events.instantiate();
	livestreams.instantiate();
	moderation.instantiate();
	oauth.instantiate();
	users.instantiate();

	// Set HTTP client for all categories
	categories->set_http_client(http_client);
	channels->set_http_client(http_client);
	chat->set_http_client(http_client);
	events->set_http_client(http_client);
	livestreams->set_http_client(http_client);
	moderation->set_http_client(http_client);
	oauth->set_http_client(http_client);
	users->set_http_client(http_client);

	// Set response callback
	http_client->set_response_callback(callable_mp(this, &KickAPI::_on_response));
}

KickAPI::~KickAPI() {
	// Disconnect from process frame if connected
	if (polling_connected) {
		Engine *engine = Engine::get_singleton();
		if (engine && engine->has_singleton("SceneTree")) {
			Object *scene_tree = engine->get_singleton_object("SceneTree");
			if (scene_tree && scene_tree->is_connected("physics_frame", callable_mp(this, &KickAPI::_idle_callback))) {
				scene_tree->disconnect("physics_frame", callable_mp(this, &KickAPI::_idle_callback));
			}
		}
	}
}

void KickAPI::configure(const String &p_access_token) {
	ERR_FAIL_COND_MSG(p_access_token.is_empty(), "access_token cannot be empty");

	http_client->set_access_token(p_access_token);

	// Ensure automatic polling is set up
	_ensure_polling_connected();
}

void KickAPI::set_access_token(const String &p_token) {
	http_client->set_access_token(p_token);
}

void KickAPI::poll() {
	// Manual poll - normally not needed as automatic polling is enabled
	http_client->poll();
}

void KickAPI::_idle_callback() {
	// Automatic polling called every frame
	http_client->poll();
}

void KickAPI::_ensure_polling_connected() {
	// Only connect once and only if SceneTree is available
	if (polling_connected) {
		return;
	}

	Engine *engine = Engine::get_singleton();
	if (engine && engine->has_singleton("SceneTree")) {
		Object *scene_tree = engine->get_singleton_object("SceneTree");
		if (scene_tree) {
			scene_tree->connect("physics_frame", callable_mp(this, &KickAPI::_idle_callback));
			polling_connected = true;
		}
	}
}

String KickAPI::query_string_from_dict(const Dictionary &p_params) const {
	return KickHTTPClient::query_string_from_dict(p_params);
}

int KickAPI::get_rate_limit_remaining() const {
	return http_client->get_rate_limit_remaining();
}

int KickAPI::get_rate_limit_reset() const {
	return http_client->get_rate_limit_reset();
}

bool KickAPI::is_busy() const {
	return http_client->is_busy();
}

Ref<KickCategoriesRequests> KickAPI::get_categories() {
	return categories;
}

Ref<KickChannelsRequests> KickAPI::get_channels() {
	return channels;
}

Ref<KickChatRequests> KickAPI::get_chat() {
	return chat;
}

Ref<KickEventsRequests> KickAPI::get_events() {
	return events;
}

Ref<KickLivestreamsRequests> KickAPI::get_livestreams() {
	return livestreams;
}

Ref<KickModerationRequests> KickAPI::get_moderation() {
	return moderation;
}

Ref<KickOAuthRequests> KickAPI::get_oauth() {
	return oauth;
}

Ref<KickUsersRequests> KickAPI::get_users() {
	return users;
}

void KickAPI::_on_response(const String &p_signal_name, int p_response_code, const Dictionary &p_data) {
	// Emit rate limit warning if needed
	int remaining = http_client->get_rate_limit_remaining();
	if (remaining >= 0 && remaining < 100) {
		emit_signal("rate_limit_warning", remaining, http_client->get_rate_limit_reset());
	}

	// Emit appropriate signal based on response code
	if (p_response_code >= 200 && p_response_code < 300) {
		// Success
		emit_signal("request_completed", p_signal_name, p_response_code, p_data);
	} else {
		// Error
		String error_message = "Request failed";
		if (p_data.has("message")) {
			error_message = p_data["message"];
		} else if (p_data.has("error")) {
			error_message = p_data["error"];
		}
		emit_signal("request_failed", p_signal_name, p_response_code, error_message);
	}
}
