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

#include "core/config/engine.h"
#include "kick_api.h"
#include "kick_http_client.h"
#include "requests/kick_categories_requests.h"
#include "requests/kick_channels_requests.h"
#include "requests/kick_chat_requests.h"
#include "requests/kick_events_requests.h"
#include "requests/kick_livestreams_requests.h"
#include "requests/kick_moderation_requests.h"
#include "requests/kick_oauth_requests.h"
#include "requests/kick_request_base.h"
#include "requests/kick_users_requests.h"

static KickAPI *kick_api_singleton = nullptr;

void initialize_kickapi_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Register HTTP client
		GDREGISTER_CLASS(KickHTTPClient);

		// Register request base class
		GDREGISTER_CLASS(KickRequestBase);

		// Register request category classes
		GDREGISTER_CLASS(KickCategoriesRequests);
		GDREGISTER_CLASS(KickChannelsRequests);
		GDREGISTER_CLASS(KickChatRequests);
		GDREGISTER_CLASS(KickEventsRequests);
		GDREGISTER_CLASS(KickLivestreamsRequests);
		GDREGISTER_CLASS(KickModerationRequests);
		GDREGISTER_CLASS(KickOAuthRequests);
		GDREGISTER_CLASS(KickUsersRequests);

		// Register and create singleton
		kick_api_singleton = memnew(KickAPI);
		GDREGISTER_CLASS(KickAPI);
		Engine::get_singleton()->add_singleton(Engine::Singleton("KickAPI", kick_api_singleton));
	}
}

void uninitialize_kickapi_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (kick_api_singleton) {
			Engine::get_singleton()->remove_singleton("KickAPI");
			memdelete(kick_api_singleton);
			kick_api_singleton = nullptr;
		}
	}
}
