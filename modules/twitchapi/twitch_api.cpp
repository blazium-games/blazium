/**************************************************************************/
/*  twitch_api.cpp                                                        */
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

#include "twitch_api.h"

#include "core/config/engine.h"

void TwitchAPI::_bind_methods() {
	// Configuration
	ClassDB::bind_method(D_METHOD("configure", "client_id", "access_token"), &TwitchAPI::configure);
	ClassDB::bind_method(D_METHOD("set_access_token", "token"), &TwitchAPI::set_access_token);
	ClassDB::bind_method(D_METHOD("set_client_id", "client_id"), &TwitchAPI::set_client_id);
	ClassDB::bind_method(D_METHOD("poll"), &TwitchAPI::poll);

	// Utility methods
	ClassDB::bind_method(D_METHOD("query_string_from_dict", "params"), &TwitchAPI::query_string_from_dict);
	ClassDB::bind_method(D_METHOD("get_rate_limit_remaining"), &TwitchAPI::get_rate_limit_remaining);
	ClassDB::bind_method(D_METHOD("get_rate_limit_reset"), &TwitchAPI::get_rate_limit_reset);
	ClassDB::bind_method(D_METHOD("is_busy"), &TwitchAPI::is_busy);

	// Category handlers
	ClassDB::bind_method(D_METHOD("get_ads"), &TwitchAPI::get_ads);
	ClassDB::bind_method(D_METHOD("get_analytics"), &TwitchAPI::get_analytics);
	ClassDB::bind_method(D_METHOD("get_bits"), &TwitchAPI::get_bits);
	ClassDB::bind_method(D_METHOD("get_channel_points"), &TwitchAPI::get_channel_points);
	ClassDB::bind_method(D_METHOD("get_channels"), &TwitchAPI::get_channels);
	ClassDB::bind_method(D_METHOD("get_chat"), &TwitchAPI::get_chat);
	ClassDB::bind_method(D_METHOD("get_clips"), &TwitchAPI::get_clips);
	ClassDB::bind_method(D_METHOD("get_games"), &TwitchAPI::get_games);
	ClassDB::bind_method(D_METHOD("get_moderation"), &TwitchAPI::get_moderation);
	ClassDB::bind_method(D_METHOD("get_streams"), &TwitchAPI::get_streams);
	ClassDB::bind_method(D_METHOD("get_users"), &TwitchAPI::get_users);

	// Signals - one generic signal for all responses
	ADD_SIGNAL(MethodInfo("request_completed", PropertyInfo(Variant::STRING, "signal_name"),
			PropertyInfo(Variant::INT, "response_code"), PropertyInfo(Variant::DICTIONARY, "data")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "signal_name"),
			PropertyInfo(Variant::INT, "error_code"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("rate_limit_warning", PropertyInfo(Variant::INT, "remaining"), PropertyInfo(Variant::INT, "reset_time")));
}

TwitchAPI::TwitchAPI() {
	// Initialize HTTP client
	http_client.instantiate();

	// Initialize category handlers
	ads.instantiate();
	analytics.instantiate();
	bits.instantiate();
	channel_points.instantiate();
	channels.instantiate();
	chat.instantiate();
	clips.instantiate();
	games.instantiate();
	moderation.instantiate();
	streams.instantiate();
	users.instantiate();

	// Set HTTP client for all categories
	ads->set_http_client(http_client);
	analytics->set_http_client(http_client);
	bits->set_http_client(http_client);
	channel_points->set_http_client(http_client);
	channels->set_http_client(http_client);
	chat->set_http_client(http_client);
	clips->set_http_client(http_client);
	games->set_http_client(http_client);
	moderation->set_http_client(http_client);
	streams->set_http_client(http_client);
	users->set_http_client(http_client);

	// Set response callback
	http_client->set_response_callback(callable_mp(this, &TwitchAPI::_on_response));
}

TwitchAPI::~TwitchAPI() {
	// Disconnect from process frame if connected
	if (polling_connected) {
		Engine *engine = Engine::get_singleton();
		if (engine && engine->has_singleton("SceneTree")) {
			Object *scene_tree = engine->get_singleton_object("SceneTree");
			if (scene_tree && scene_tree->is_connected("physics_frame", callable_mp(this, &TwitchAPI::_idle_callback))) {
				scene_tree->disconnect("physics_frame", callable_mp(this, &TwitchAPI::_idle_callback));
			}
		}
	}
}

void TwitchAPI::configure(const String &p_client_id, const String &p_access_token) {
	ERR_FAIL_COND_MSG(p_client_id.is_empty(), "client_id cannot be empty");
	ERR_FAIL_COND_MSG(p_access_token.is_empty(), "access_token cannot be empty");

	http_client->set_credentials(p_client_id, p_access_token);

	// Ensure automatic polling is set up
	_ensure_polling_connected();
}

void TwitchAPI::set_access_token(const String &p_token) {
	http_client->set_access_token(p_token);
}

void TwitchAPI::set_client_id(const String &p_client_id) {
	http_client->set_client_id(p_client_id);
}

void TwitchAPI::poll() {
	// Manual poll - normally not needed as automatic polling is enabled
	http_client->poll();
}

void TwitchAPI::_idle_callback() {
	// Automatic polling called every frame
	http_client->poll();
}

void TwitchAPI::_ensure_polling_connected() {
	// Only connect once and only if SceneTree is available
	if (polling_connected) {
		return;
	}

	Engine *engine = Engine::get_singleton();
	if (engine && engine->has_singleton("SceneTree")) {
		Object *scene_tree = engine->get_singleton_object("SceneTree");
		if (scene_tree) {
			scene_tree->connect("physics_frame", callable_mp(this, &TwitchAPI::_idle_callback));
			polling_connected = true;
		}
	}
}

String TwitchAPI::query_string_from_dict(const Dictionary &p_params) const {
	return TwitchHTTPClient::query_string_from_dict(p_params);
}

int TwitchAPI::get_rate_limit_remaining() const {
	return http_client->get_rate_limit_remaining();
}

int TwitchAPI::get_rate_limit_reset() const {
	return http_client->get_rate_limit_reset();
}

bool TwitchAPI::is_busy() const {
	return http_client->is_busy();
}

Ref<TwitchAdsRequests> TwitchAPI::get_ads() {
	return ads;
}

Ref<TwitchAnalyticsRequests> TwitchAPI::get_analytics() {
	return analytics;
}

Ref<TwitchBitsRequests> TwitchAPI::get_bits() {
	return bits;
}

Ref<TwitchChannelPointsRequests> TwitchAPI::get_channel_points() {
	return channel_points;
}

Ref<TwitchChannelsRequests> TwitchAPI::get_channels() {
	return channels;
}

Ref<TwitchChatRequests> TwitchAPI::get_chat() {
	return chat;
}

Ref<TwitchClipsRequests> TwitchAPI::get_clips() {
	return clips;
}

Ref<TwitchGamesRequests> TwitchAPI::get_games() {
	return games;
}

Ref<TwitchModerationRequests> TwitchAPI::get_moderation() {
	return moderation;
}

Ref<TwitchStreamsRequests> TwitchAPI::get_streams() {
	return streams;
}

Ref<TwitchUsersRequests> TwitchAPI::get_users() {
	return users;
}

void TwitchAPI::_on_response(const String &p_signal_name, int p_response_code, const Dictionary &p_data) {
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
