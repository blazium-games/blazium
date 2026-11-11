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

#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"

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
