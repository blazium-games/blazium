/**************************************************************************/
/*  youtube_playable_client.h                                             */
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

#pragma once

#include "../third_party_client.h"
#include "platform/web/api/javascript_bridge_singleton.h"
#include "scene/main/node.h"
#include "youtube_playables_response.h"

// https://developers.google.com/youtube/gaming/playables/reference/sdk
// https://github.com/google/web-game-samples/blob/main/phaser/src/YouTubePlayables.js
class YoutubePlayablesClient : public ThirdPartyClient {
	GDCLASS(YoutubePlayablesClient, ThirdPartyClient);

	Ref<JavaScriptObject> ytgameRef;

	Ref<JavaScriptObject> on_audio_enabled_change_callback;
	Ref<JavaScriptObject> on_pause_callback;
	Ref<JavaScriptObject> on_resume_callback;

	// Needed to have is_audio_enabled return the right value
	bool audio_enabled = true;

protected:
	static void _bind_methods();

public:
	void _emit_audio_enabled_change(Array);
	void _emit_pause();
	void _emit_resume();

	void log_error();
	void log_warning();
	bool is_audio_enabled();
	bool is_youtube_environment();
	String get_sdk_version();
	Ref<YoutubePlayablesResponse> load_data();
	Ref<YoutubePlayablesResponse> save_data(String);
	Ref<YoutubePlayablesResponse> send_score(int32_t);
	Ref<YoutubePlayablesResponse> open_yt_content(String);
	Ref<YoutubePlayablesResponse> get_language();

	YoutubePlayablesClient();
};
