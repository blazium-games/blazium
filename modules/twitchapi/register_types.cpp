/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "requests/twitch_ads_requests.h"
#include "requests/twitch_analytics_requests.h"
#include "requests/twitch_bits_requests.h"
#include "requests/twitch_channel_points_requests.h"
#include "requests/twitch_channels_requests.h"
#include "requests/twitch_chat_requests.h"
#include "requests/twitch_clips_requests.h"
#include "requests/twitch_games_requests.h"
#include "requests/twitch_moderation_requests.h"
#include "requests/twitch_request_base.h"
#include "requests/twitch_streams_requests.h"
#include "requests/twitch_users_requests.h"
#include "twitch_api.h"
#include "twitch_http_client.h"

static TwitchAPI *twitch_api_singleton = nullptr;

void initialize_twitchapi_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Register base classes
		GDREGISTER_CLASS(TwitchHTTPClient);
		GDREGISTER_CLASS(TwitchRequestBase);

		// Register request category classes
		GDREGISTER_CLASS(TwitchAdsRequests);
		GDREGISTER_CLASS(TwitchAnalyticsRequests);
		GDREGISTER_CLASS(TwitchBitsRequests);
		GDREGISTER_CLASS(TwitchChannelPointsRequests);
		GDREGISTER_CLASS(TwitchChannelsRequests);
		GDREGISTER_CLASS(TwitchChatRequests);
		GDREGISTER_CLASS(TwitchClipsRequests);
		GDREGISTER_CLASS(TwitchGamesRequests);
		GDREGISTER_CLASS(TwitchModerationRequests);
		GDREGISTER_CLASS(TwitchStreamsRequests);
		GDREGISTER_CLASS(TwitchUsersRequests);

		// Register main API singleton
		twitch_api_singleton = memnew(TwitchAPI);
		GDREGISTER_CLASS(TwitchAPI);
		Engine::get_singleton()->add_singleton(Engine::Singleton("TwitchAPI", twitch_api_singleton));
	}
}

void uninitialize_twitchapi_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		Engine::get_singleton()->remove_singleton("TwitchAPI");
		memdelete(twitch_api_singleton);
		twitch_api_singleton = nullptr;
	}
}
