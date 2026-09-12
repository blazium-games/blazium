/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2024 Dragos Daian, Randolph William Aarseth II.          */
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

#include "discord/discord_embedded_app_client.h"
#include "discord/discord_embedded_app_response.h"
#include "react/react_client.h"
#include "third_party_client.h"
#include "youtube/youtube_playables_client.h"
#include "youtube/youtube_playables_response.h"

void initialize_socialexports_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_ABSTRACT_CLASS(ThirdPartyClient);
		GDREGISTER_CLASS(DiscordEmbeddedAppClient);
		GDREGISTER_CLASS(ReactClient);
		GDREGISTER_CLASS(DiscordEmbeddedAppResponse);
		GDREGISTER_CLASS(DiscordEmbeddedAppResponse::DiscordEmbeddedAppResult);
		GDREGISTER_CLASS(YoutubePlayablesClient);
		GDREGISTER_CLASS(YoutubePlayablesResponse);
		GDREGISTER_CLASS(YoutubePlayablesResponse::YoutubePlayablesResult);
	}
}

void uninitialize_socialexports_module(ModuleInitializationLevel p_level) {
}
