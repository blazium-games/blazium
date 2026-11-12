/**************************************************************************/
/*  html_portals.cpp                                                      */
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

#include "html_portals.h"

#include "portal_runtime.h"

#include "core/io/json.h"
#ifndef WEB_ENABLED
#include "core/io/config_file.h"
#include "core/os/os.h"
#endif
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"

#ifdef WEB_ENABLED
#include "platform/web/api/javascript_bridge_singleton.h"
#endif

#ifndef WEB_ENABLED
static const char *BETACADE_SAVE_PATH = "user://betacade.cfg";
static const char *BETACADE_SAVE_SECTION = "saves";
#endif

YouTube::YouTube() {
	host_id = "youtube";
}

void YouTube::_bind_methods() {
	PortalApi<YouTube>::bind_common();
	ClassDB::bind_method(D_METHOD("send_first_frame"), &YouTube::send_first_frame);
	ClassDB::bind_method(D_METHOD("game_ready"), &YouTube::game_ready);
	ClassDB::bind_method(D_METHOD("send_score", "score"), &YouTube::send_score);
	ClassDB::bind_method(D_METHOD("save_data", "data"), &YouTube::save_data);
	ClassDB::bind_method(D_METHOD("load_data"), &YouTube::load_data);
	ClassDB::bind_method(D_METHOD("get_language"), &YouTube::get_language);
	ClassDB::bind_method(D_METHOD("is_audio_enabled"), &YouTube::is_audio_enabled);
	ADD_SIGNAL(MethodInfo("audio_enabled", PropertyInfo(Variant::BOOL, "enabled")));
}

void YouTube::bind_host_extra() {
	audio_callback = PortalRuntime::make_callback(callable_mp(this, &YouTube::_on_audio));
	PortalRuntime::bind(js_interface, "audio", audio_callback);
}

void YouTube::_on_audio(bool p_enabled) {
	emit_signal(SNAME("audio_enabled"), p_enabled);
}

void YouTube::send_first_frame() {
	host_call("firstFrame");
}

void YouTube::game_ready() {
	host_call("gameReady");
}

void YouTube::send_score(int p_score) {
	Dictionary args;
	args["score"] = p_score;
	host_call("sendScore", args);
}

void YouTube::save_data(const String &p_data) {
	Dictionary args;
	args["data"] = p_data;
	host_call("saveData", args);
}

void YouTube::load_data() {
	host_call("loadData");
}

String YouTube::get_language() {
	return host_call("getLanguage");
}

bool YouTube::is_audio_enabled() {
	return host_call("isAudioEnabled") == "true";
}

Telegram::Telegram() {
	host_id = "telegram";
}

void Telegram::_bind_methods() {
	PortalApi<Telegram>::bind_common();
	ClassDB::bind_method(D_METHOD("get_init_data"), &Telegram::get_init_data);
	ClassDB::bind_method(D_METHOD("get_theme"), &Telegram::get_theme);
	ClassDB::bind_method(D_METHOD("get_viewport"), &Telegram::get_viewport);
	ClassDB::bind_method(D_METHOD("get_safe_area"), &Telegram::get_safe_area);
	ClassDB::bind_method(D_METHOD("request_fullscreen"), &Telegram::request_fullscreen);
	ClassDB::bind_method(D_METHOD("send_data", "data"), &Telegram::send_data);
}

String Telegram::get_init_data() {
	return host_call("getInitData");
}

Dictionary Telegram::get_theme() {
	return host_dictionary(host_call("getTheme"));
}

Dictionary Telegram::get_viewport() {
	return host_dictionary(host_call("getViewport"));
}

Dictionary Telegram::get_safe_area() {
	return host_dictionary(host_call("getSafeArea"));
}

void Telegram::request_fullscreen() {
	host_call("requestFullscreen");
}

void Telegram::send_data(const String &p_data) {
	Dictionary args;
	args["data"] = p_data;
	host_call("sendData", args);
}

Reddit::Reddit() {
	host_id = "reddit";
}

void Reddit::_bind_methods() {
	PortalApi<Reddit>::bind_common();
	ClassDB::bind_method(D_METHOD("get_username"), &Reddit::get_username);
	ClassDB::bind_method(D_METHOD("submit_score", "score"), &Reddit::submit_score);
}

String Reddit::get_username() {
	return host_call("getUsername");
}

void Reddit::submit_score(int p_score) {
	Dictionary args;
	args["score"] = p_score;
	host_call("submitScore", args);
}

Newgrounds::Newgrounds() {
	host_id = "newgrounds";
}

void Newgrounds::_bind_methods() {
	PortalApi<Newgrounds>::bind_common();
	ClassDB::bind_method(D_METHOD("log_in"), &Newgrounds::log_in);
	ClassDB::bind_method(D_METHOD("unlock_medal", "id"), &Newgrounds::unlock_medal);
	ClassDB::bind_method(D_METHOD("submit_score", "board", "score"), &Newgrounds::submit_score);
	ClassDB::bind_method(D_METHOD("cloud_save", "key", "value"), &Newgrounds::cloud_save);
	ClassDB::bind_method(D_METHOD("cloud_load", "key"), &Newgrounds::cloud_load);
}

void Newgrounds::log_in() {
	host_call("logIn");
}

void Newgrounds::unlock_medal(int p_id) {
	Dictionary args;
	args["id"] = p_id;
	host_call("unlockMedal", args);
}

void Newgrounds::submit_score(const String &p_board, int p_score) {
	Dictionary args;
	args["board"] = p_board;
	args["score"] = p_score;
	host_call("submitScore", args);
}

void Newgrounds::cloud_save(const String &p_key, const String &p_value) {
	Dictionary args;
	args["key"] = p_key;
	args["value"] = p_value;
	host_call("cloudSave", args);
}

String Newgrounds::cloud_load(const String &p_key) {
	Dictionary args;
	args["key"] = p_key;
	return host_call("cloudLoad", args);
}

CrazyGames::CrazyGames() {
	host_id = "crazygames";
}

void CrazyGames::_bind_methods() {
	PortalApi<CrazyGames>::bind_common();
	ClassDB::bind_method(D_METHOD("gameplay_start"), &CrazyGames::gameplay_start);
	ClassDB::bind_method(D_METHOD("gameplay_stop"), &CrazyGames::gameplay_stop);
	ClassDB::bind_method(D_METHOD("request_midgame_ad"), &CrazyGames::request_midgame_ad);
	ClassDB::bind_method(D_METHOD("request_rewarded_ad"), &CrazyGames::request_rewarded_ad);
	ClassDB::bind_method(D_METHOD("get_environment"), &CrazyGames::get_environment);
}

void CrazyGames::gameplay_start() {
	host_call("gameplayStart");
}

void CrazyGames::gameplay_stop() {
	host_call("gameplayStop");
}

void CrazyGames::request_midgame_ad() {
	host_call("requestMidgameAd");
}

void CrazyGames::request_rewarded_ad() {
	host_call("requestRewardedAd");
}

String CrazyGames::get_environment() {
	return host_call("getEnvironment");
}

Poki::Poki() {
	host_id = "poki";
}

void Poki::_bind_methods() {
	PortalApi<Poki>::bind_common();
	ClassDB::bind_method(D_METHOD("gameplay_start"), &Poki::gameplay_start);
	ClassDB::bind_method(D_METHOD("gameplay_stop"), &Poki::gameplay_stop);
	ClassDB::bind_method(D_METHOD("commercial_break"), &Poki::commercial_break);
	ClassDB::bind_method(D_METHOD("rewarded_break"), &Poki::rewarded_break);
	ClassDB::bind_method(D_METHOD("get_shareable_url"), &Poki::get_shareable_url);
	ADD_SIGNAL(MethodInfo("break_done", PropertyInfo(Variant::STRING, "kind")));
}

void Poki::bind_host_extra() {
	break_callback = PortalRuntime::make_callback(callable_mp(this, &Poki::_on_break_done));
	PortalRuntime::bind(js_interface, "break_done", break_callback);
}

void Poki::_on_break_done(const String &p_kind) {
	emit_signal(SNAME("break_done"), p_kind);
}

void Poki::gameplay_start() {
	host_call("gameplayStart");
}

void Poki::gameplay_stop() {
	host_call("gameplayStop");
}

void Poki::commercial_break() {
	host_call("commercialBreak");
}

void Poki::rewarded_break() {
	host_call("rewardedBreak");
}

String Poki::get_shareable_url() {
	return host_call("getShareableUrl");
}

Meta::Meta() {
	host_id = "meta";
}

void Meta::_bind_methods() {
	PortalApi<Meta>::bind_common();
	ClassDB::bind_method(D_METHOD("get_player_id"), &Meta::get_player_id);
	ClassDB::bind_method(D_METHOD("get_player_name"), &Meta::get_player_name);
	ClassDB::bind_method(D_METHOD("get_player_photo"), &Meta::get_player_photo);
	ClassDB::bind_method(D_METHOD("get_locale"), &Meta::get_locale);
	ClassDB::bind_method(D_METHOD("get_entry_point_data"), &Meta::get_entry_point_data);
	ClassDB::bind_method(D_METHOD("get_context_id"), &Meta::get_context_id);
}

String Meta::get_player_id() {
	return host_call("getPlayerId");
}

String Meta::get_player_name() {
	return host_call("getPlayerName");
}

String Meta::get_player_photo() {
	return host_call("getPlayerPhoto");
}

String Meta::get_locale() {
	return host_call("getLocale");
}

String Meta::get_entry_point_data() {
	return host_call("getEntryPointData");
}

String Meta::get_context_id() {
	return host_call("getContextId");
}

GameDistribution::GameDistribution() {
	host_id = "gamedistribution";
}

void GameDistribution::_bind_methods() {
	PortalApi<GameDistribution>::bind_common();
	ClassDB::bind_method(D_METHOD("show_ad"), &GameDistribution::show_ad);
}

void GameDistribution::show_ad() {
	host_call("showAd");
}

Betacade::Betacade() {
	host_id = "betacade";
#ifdef WEB_ENABLED
	// The page's pause and replay signals reach the game from the first frame, not only after its
	// first save or event. JavaScriptBridge doesn't exist yet when singletons are made.
	callable_mp(this, &Betacade::ensure_bound).call_deferred();
#endif
}

#ifdef WEB_ENABLED
// Saves go to localStorage on every web export, not only under the Betacade preset (whose portal
// runtime answers only on its own host), so the same project keeps its saves on itch.io or Poki.
static Ref<JavaScriptObject> betacade_local_storage() {
	JavaScriptBridge *bridge = JavaScriptBridge::get_singleton();
	if (bridge == nullptr) {
		return Ref<JavaScriptObject>();
	}
	Ref<JavaScriptObject> window = bridge->get_interface("window");
	if (window.is_null()) {
		return Ref<JavaScriptObject>();
	}
	return window->get("localStorage");
}

static bool betacade_storage_call(const StringName &p_method, const Variant &p_key, const Variant &p_value, int p_argc, Variant *r_ret = nullptr) {
	Ref<JavaScriptObject> storage = betacade_local_storage();
	if (storage.is_null()) {
		return false;
	}
	const Variant *args[2] = { &p_key, &p_value };
	Callable::CallError err;
	Variant ret = storage->callp(p_method, args, p_argc, err);
	if (r_ret) {
		*r_ret = ret;
	}
	return err.error == Callable::CallError::CALL_OK;
}
#endif

void Betacade::_bind_methods() {
	PortalApi<Betacade>::bind_common();
	ClassDB::bind_method(D_METHOD("is_available"), &Betacade::is_available);
	ClassDB::bind_method(D_METHOD("is_replaying"), &Betacade::is_replaying);
	ClassDB::bind_method(D_METHOD("set_state", "state"), &Betacade::set_state);
	ClassDB::bind_method(D_METHOD("event", "name", "fields"), &Betacade::event, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("save", "key", "value"), &Betacade::save);
	ClassDB::bind_method(D_METHOD("load", "key", "default"), &Betacade::load, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("remove", "key"), &Betacade::remove);
	ClassDB::bind_method(D_METHOD("submit_score", "score"), &Betacade::submit_score);
	ClassDB::bind_method(D_METHOD("achieve", "key"), &Betacade::achieve);
	ADD_SIGNAL(MethodInfo("replay_entered"));
	ADD_SIGNAL(MethodInfo("replay_state", PropertyInfo(Variant::NIL, "state", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("replay_exited"));
}

#ifndef WEB_ENABLED
Ref<ConfigFile> Betacade::_get_store() {
	if (store.is_null()) {
		store.instantiate();
		store->load(BETACADE_SAVE_PATH);
	}
	return store;
}
#endif

void Betacade::ensure_bound() {
	if (callbacks_bound) {
		return;
	}
	pause_callback = PortalRuntime::make_callback(callable_mp(this, &Betacade::_portal_paused));
	if (pause_callback.get_type() == Variant::NIL) {
		return; // No JavaScriptBridge yet: try again on the next call.
	}
	callbacks_bound = true;
	resume_callback = PortalRuntime::make_callback(callable_mp(this, &Betacade::_portal_resumed));
	replay_entered_callback = PortalRuntime::make_callback(callable_mp(this, &Betacade::_on_replay_entered));
	replay_state_callback = PortalRuntime::make_callback(callable_mp(this, &Betacade::_on_replay_state));
	replay_exited_callback = PortalRuntime::make_callback(callable_mp(this, &Betacade::_on_replay_exited));
	PortalRuntime::bind(js_interface, "paused", pause_callback);
	PortalRuntime::bind(js_interface, "resumed", resume_callback);
	PortalRuntime::bind(js_interface, "replay_entered", replay_entered_callback);
	PortalRuntime::bind(js_interface, "replay_state", replay_state_callback);
	PortalRuntime::bind(js_interface, "replay_exited", replay_exited_callback);
}

String Betacade::web_call(const String &p_method, const Dictionary &p_args) {
#ifdef WEB_ENABLED
	ensure_bound();
	Variant result = PortalRuntime::invoke(js_interface, host_id, p_method, JSON::stringify(p_args));
	if (result.get_type() == Variant::NIL) {
		return String();
	}
	return String(result);
#else
	(void)p_method;
	(void)p_args;
	return String();
#endif
}

void Betacade::_on_replay_entered() {
	emit_signal(SNAME("replay_entered"));
}

void Betacade::_on_replay_state(const String &p_json) {
	emit_signal(SNAME("replay_state"), JSON::parse_string(p_json));
}

void Betacade::_on_replay_exited() {
	emit_signal(SNAME("replay_exited"));
}

bool Betacade::is_available() const {
#ifdef WEB_ENABLED
	Variant result = PortalRuntime::invoke(js_interface, host_id, "available", "{}");
	return String(result) == "true";
#else
	return false;
#endif
}

bool Betacade::is_replaying() const {
#ifdef WEB_ENABLED
	// A game that skips set_state while replaying still registers its replay hooks here, if it
	// connected replay_state: the studio's smooth viewer asks for them before any state is set.
	const_cast<Betacade *>(this)->ensure_bound();
	const bool replay = has_connections(SNAME("replay_state"));
	Variant result = PortalRuntime::invoke(js_interface, host_id, "replaying", replay ? "{\"replay\":true}" : "{}");
	return String(result) == "true";
#else
	return false;
#endif
}

void Betacade::set_state(const Variant &p_state) {
	Dictionary args;
	args["json"] = JSON::stringify(p_state);
	args["replay"] = has_connections(SNAME("replay_state"));
	web_call("setState", args);
}

void Betacade::event(const String &p_name, const Dictionary &p_fields) {
	Dictionary args;
	args["name"] = p_name;
	args["fields"] = p_fields;
	web_call("track", args);
}

bool Betacade::save(const String &p_key, const Variant &p_value) {
	ERR_FAIL_COND_V_MSG(p_key.is_empty(), false, "Betacade.save: the key is empty.");
	const String json = JSON::stringify(p_value);
#ifdef WEB_ENABLED
	return betacade_storage_call(SNAME("setItem"), p_key, json, 2);
#else
	OS::get_singleton()->ensure_user_data_dir();
	Ref<ConfigFile> file = _get_store();
	file->set_value(BETACADE_SAVE_SECTION, p_key, json);
	return file->save(BETACADE_SAVE_PATH) == OK;
#endif
}

Variant Betacade::load(const String &p_key, const Variant &p_default) {
	String json;
	bool found = false;
#ifdef WEB_ENABLED
	Variant stored;
	if (betacade_storage_call(SNAME("getItem"), p_key, Variant(), 1, &stored) && stored.get_type() == Variant::STRING) {
		json = stored;
		found = true;
	}
#else
	Ref<ConfigFile> file = _get_store();
	if (file->has_section_key(BETACADE_SAVE_SECTION, p_key)) {
		json = file->get_value(BETACADE_SAVE_SECTION, p_key);
		found = true;
	}
#endif
	if (!found) {
		return p_default;
	}
	Variant value = JSON::parse_string(json);
	if (value.get_type() == Variant::NIL && json != "null") {
		return p_default;
	}
	return value;
}

void Betacade::remove(const String &p_key) {
#ifdef WEB_ENABLED
	betacade_storage_call(SNAME("removeItem"), p_key, Variant(), 1);
#else
	OS::get_singleton()->ensure_user_data_dir();
	Ref<ConfigFile> file = _get_store();
	if (file->has_section_key(BETACADE_SAVE_SECTION, p_key)) {
		file->erase_section_key(BETACADE_SAVE_SECTION, p_key);
		file->save(BETACADE_SAVE_PATH);
	}
#endif
}

void Betacade::submit_score(double p_score) {
	Dictionary args;
	args["score"] = p_score;
	web_call("submitScore", args);
}

void Betacade::achieve(const String &p_key) {
	Dictionary args;
	args["key"] = p_key;
	web_call("achieve", args);
}
