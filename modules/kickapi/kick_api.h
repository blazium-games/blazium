/**************************************************************************/
/*  kick_api.h                                                            */
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

#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/script_language.h"
#include "kick_http_client.h"
#include "requests/kick_categories_requests.h"
#include "requests/kick_channels_requests.h"
#include "requests/kick_chat_requests.h"
#include "requests/kick_events_requests.h"
#include "requests/kick_livestreams_requests.h"
#include "requests/kick_moderation_requests.h"
#include "requests/kick_oauth_requests.h"
#include "requests/kick_users_requests.h"

class KickAPI : public Object {
	GDCLASS(KickAPI, Object);

private:
	Ref<KickHTTPClient> http_client;

	// Request category handlers
	Ref<KickCategoriesRequests> categories;
	Ref<KickChannelsRequests> channels;
	Ref<KickChatRequests> chat;
	Ref<KickEventsRequests> events;
	Ref<KickLivestreamsRequests> livestreams;
	Ref<KickModerationRequests> moderation;
	Ref<KickOAuthRequests> oauth;
	Ref<KickUsersRequests> users;

	// Response handler
	void _on_response(const String &p_signal_name, int p_response_code, const Dictionary &p_data);

	// Automatic polling
	void _idle_callback();
	void _ensure_polling_connected();
	bool polling_connected = false;

protected:
	static void _bind_methods();

public:
	// Configuration
	void configure(const String &p_access_token);
	void set_access_token(const String &p_token);
	void poll(); // Manual poll (optional - automatic polling is enabled by default)

	// Utility methods
	String query_string_from_dict(const Dictionary &p_params) const;
	int get_rate_limit_remaining() const;
	int get_rate_limit_reset() const;
	bool is_busy() const;

	// Access to request category handlers
	Ref<KickCategoriesRequests> get_categories();
	Ref<KickChannelsRequests> get_channels();
	Ref<KickChatRequests> get_chat();
	Ref<KickEventsRequests> get_events();
	Ref<KickLivestreamsRequests> get_livestreams();
	Ref<KickModerationRequests> get_moderation();
	Ref<KickOAuthRequests> get_oauth();
	Ref<KickUsersRequests> get_users();

	KickAPI();
	~KickAPI();
};
