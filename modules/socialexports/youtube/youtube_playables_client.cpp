/**************************************************************************/
/*  youtube_playable_client.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2025 Nicholas Santos Shiden.                             */
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

#include "youtube_playables_client.h"
#include "platform/web/api/javascript_bridge_singleton.h"
#include "youtube_playables_response.h"

void YoutubePlayablesClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_emit_audio_enabled_change"), &YoutubePlayablesClient::_emit_audio_enabled_change);
	ClassDB::bind_method(D_METHOD("_emit_pause"), &YoutubePlayablesClient::_emit_pause);
	ClassDB::bind_method(D_METHOD("_emit_resume"), &YoutubePlayablesClient::_emit_resume);

	ClassDB::bind_method(D_METHOD("is_youtube_environment"), &YoutubePlayablesClient::is_youtube_environment);
	ClassDB::bind_method(D_METHOD("get_sdk_version"), &YoutubePlayablesClient::get_sdk_version);
	ClassDB::bind_method(D_METHOD("send_score", "value"), &YoutubePlayablesClient::send_score);
	ClassDB::bind_method(D_METHOD("open_yt_content", "video_id"), &YoutubePlayablesClient::open_yt_content);
	ClassDB::bind_method(D_METHOD("load_data"), &YoutubePlayablesClient::load_data);
	ClassDB::bind_method(D_METHOD("save_data", "data"), &YoutubePlayablesClient::save_data);
	ClassDB::bind_method(D_METHOD("log_error"), &YoutubePlayablesClient::log_error);
	ClassDB::bind_method(D_METHOD("log_warning"), &YoutubePlayablesClient::log_warning);
	ClassDB::bind_method(D_METHOD("is_audio_enabled"), &YoutubePlayablesClient::is_audio_enabled);
	ClassDB::bind_method(D_METHOD("get_language"), &YoutubePlayablesClient::get_language);

	ADD_SIGNAL(MethodInfo("audio_enabled_change", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("pause"));
	ADD_SIGNAL(MethodInfo("resume"));
}

void YoutubePlayablesClient::_emit_audio_enabled_change(Array p_array) {
	ERR_FAIL_COND(p_array.is_empty());

	audio_enabled = p_array[0];
	emit_signal("audio_enabled_change", p_array[0]);
}

void YoutubePlayablesClient::_emit_pause() {
	emit_signal("pause");
}

void YoutubePlayablesClient::_emit_resume() {
	emit_signal("resume");
}

void YoutubePlayablesClient::log_error() {
	if (is_youtube_environment()) {
		ytgameRef->call("logError");
	}
}

void YoutubePlayablesClient::log_warning() {
	if (is_youtube_environment()) {
		ytgameRef->call("logWarning");
	}
}

bool YoutubePlayablesClient::is_audio_enabled() {
	return audio_enabled;
}

bool YoutubePlayablesClient::is_youtube_environment() {
	if (ytgameRef.is_valid()) {
		return ytgameRef->call("isYoutubePlayables");
	}
	return false;
}

String YoutubePlayablesClient::get_sdk_version() {
	if (is_youtube_environment()) {
		return ytgameRef->call("getSdkVersion");
	}
	return String();
}

Ref<YoutubePlayablesResponse> YoutubePlayablesClient::load_data() {
	Ref<YoutubePlayablesResponse> response = Ref<YoutubePlayablesResponse>();
	response.instantiate();

	if (is_youtube_environment()) {
		response->create_signal_finish_callback();
		ytgameRef->call_deferred("loadData", response->get_signal_finish_callback());
	} else {
		Callable callable = callable_mp(*response, &YoutubePlayablesResponse::_signal_finish_error);
		callable.call_deferred("Not in Youtube Playables environment.");
	}
	return response;
}

Ref<YoutubePlayablesResponse> YoutubePlayablesClient::save_data(String p_data) {
	Ref<YoutubePlayablesResponse> response = Ref<YoutubePlayablesResponse>();
	response.instantiate();

	if (is_youtube_environment()) {
		response->create_signal_finish_callback();
		ytgameRef->call_deferred("saveData", p_data, response->get_signal_finish_callback());
	} else {
		Callable callable = callable_mp(*response, &YoutubePlayablesResponse::_signal_finish_error);
		callable.call_deferred("Not in Youtube Playables environment.");
	}
	return response;
}

Ref<YoutubePlayablesResponse> YoutubePlayablesClient::send_score(int32_t p_value) {
	Ref<YoutubePlayablesResponse> response = Ref<YoutubePlayablesResponse>();
	response.instantiate();

	if (is_youtube_environment()) {
		response->create_signal_finish_callback();
		ytgameRef->call_deferred("sendScore", p_value, response->get_signal_finish_callback());
	} else {
		Callable callable = callable_mp(*response, &YoutubePlayablesResponse::_signal_finish_error);
		callable.call_deferred("Not in Youtube Playables environment.");
	}
	return response;
}

Ref<YoutubePlayablesResponse> YoutubePlayablesClient::open_yt_content(String p_video_id) {
	Ref<YoutubePlayablesResponse> response = Ref<YoutubePlayablesResponse>();
	response.instantiate();

	if (is_youtube_environment()) {
		response->create_signal_finish_callback();
		ytgameRef->call_deferred("openYTContent", p_video_id, response->get_signal_finish_callback());
	} else {
		Callable callable = callable_mp(*response, &YoutubePlayablesResponse::_signal_finish_error);
		callable.call_deferred("Not in Youtube Playables environment.");
	}
	return response;
}

Ref<YoutubePlayablesResponse> YoutubePlayablesClient::get_language() {
	Ref<YoutubePlayablesResponse> response = Ref<YoutubePlayablesResponse>();
	response.instantiate();

	if (is_youtube_environment()) {
		response->create_signal_finish_callback();
		ytgameRef->call_deferred("getLanguage", response->get_signal_finish_callback());
	} else {
		Callable callable = callable_mp(*response, &YoutubePlayablesResponse::_signal_finish_error);
		callable.call_deferred("Not in Youtube Playables environment.");
	}
	return response;
}

YoutubePlayablesClient::YoutubePlayablesClient() {
	JavaScriptBridge *singleton = JavaScriptBridge::get_singleton();
	if (!singleton) {
		ERR_PRINT("JavaScriptBridge singleton is invalid");
		return;
	}

	// YoutubePlayables defined in platform/web/export/export_plugin.cpp
	if (!singleton->eval("window.YoutubePlayables?.isYoutubePlayables() ?? false", true)) {
		return;
	}
	ytgameRef = singleton->get_interface("YoutubePlayables");
	if (!ytgameRef.is_valid()) {
		return;
	}

	on_audio_enabled_change_callback = singleton->create_callback(Callable(this, "_emit_audio_enabled_change"));
	on_pause_callback = singleton->create_callback(Callable(this, "_emit_pause"));
	on_resume_callback = singleton->create_callback(Callable(this, "_emit_resume"));

	ytgameRef->call("onAudioEnabledChange", on_audio_enabled_change_callback);
	ytgameRef->call("onPause", on_pause_callback);
	ytgameRef->call("onResume", on_resume_callback);

	// Needed to have is_audio_enabled return the right value
	audio_enabled = singleton->eval("ytgame.system.isAudioEnabled()");
}
