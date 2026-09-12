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

#include "irc_channel.h"
#include "irc_client.h"
#include "irc_client_node.h"
#include "irc_dcc.h"
#include "irc_message.h"
#include "irc_user.h"

#include "twitch/twitch_irc_client.h"
#include "twitch/twitch_irc_client_node.h"
#include "twitch/twitch_message.h"

void initialize_ircclient_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Register IRC classes
	GDREGISTER_CLASS(IRCMessage);
	GDREGISTER_CLASS(IRCUser);
	GDREGISTER_CLASS(IRCChannel);
	GDREGISTER_CLASS(IRCDCCTransfer);
	GDREGISTER_CLASS(IRCClient);
	GDREGISTER_CLASS(IRCClientNode);

	// Register Twitch IRC classes
	GDREGISTER_CLASS(TwitchMessage);
	GDREGISTER_CLASS(TwitchIRCClient);
	GDREGISTER_CLASS(TwitchIRCClientNode);
}

void uninitialize_ircclient_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Nothing to uninitialize
}
