/**************************************************************************/
/*  test_irc_message.h                                                    */
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

#include "modules/ircclient/irc_message.h"

#include "tests/test_macros.h"

namespace TestIRCClient {

TEST_CASE("[IRCClient][IRCMessage] Basic message parsing") {
	SUBCASE("[IRCClient][IRCMessage] Simple command") {
		Ref<IRCMessage> msg = IRCMessage::parse("PING :server.example.com");

		CHECK(msg->get_command() == "PING");
		CHECK(msg->get_params().size() == 1);
		CHECK(msg->get_params()[0] == "server.example.com");
		CHECK(msg->get_prefix().is_empty());
	}

	SUBCASE("[IRCClient][IRCMessage] Command with prefix") {
		Ref<IRCMessage> msg = IRCMessage::parse(":nick!user@host PRIVMSG #channel :Hello world");

		CHECK(msg->get_prefix() == "nick!user@host");
		CHECK(msg->get_command() == "PRIVMSG");
		CHECK(msg->get_params().size() == 2);
		CHECK(msg->get_params()[0] == "#channel");
		CHECK(msg->get_params()[1] == "Hello world");
		CHECK(msg->get_nick() == "nick");
		CHECK(msg->get_username() == "user");
		CHECK(msg->get_hostname() == "host");
	}

	SUBCASE("[IRCClient][IRCMessage] Numeric reply") {
		Ref<IRCMessage> msg = IRCMessage::parse(":server.example.com 001 nick :Welcome to IRC");

		CHECK(msg->get_command() == "001");
		CHECK(msg->is_numeric());
		CHECK(msg->get_numeric() == 1);
		CHECK(msg->get_params().size() == 2);
		CHECK(msg->get_params()[0] == "nick");
		CHECK(msg->get_params()[1] == "Welcome to IRC");
	}

	SUBCASE("[IRCClient][IRCMessage] Multiple parameters") {
		Ref<IRCMessage> msg = IRCMessage::parse("MODE #channel +o user");

		CHECK(msg->get_command() == "MODE");
		CHECK(msg->get_params().size() == 3);
		CHECK(msg->get_params()[0] == "#channel");
		CHECK(msg->get_params()[1] == "+o");
		CHECK(msg->get_params()[2] == "user");
	}
}

TEST_CASE("[IRCClient][IRCMessage] IRCv3 tags parsing") {
	SUBCASE("[IRCClient][IRCMessage] Tags with values") {
		Ref<IRCMessage> msg = IRCMessage::parse("@time=2024-01-01T12:00:00.000Z;account=user123 :nick!user@host PRIVMSG #channel :Tagged message");

		CHECK(msg->get_command() == "PRIVMSG");
		Dictionary tags = msg->get_tags();
		CHECK(tags.size() == 2);
		CHECK(tags.has("time"));
		CHECK(tags["time"] == "2024-01-01T12:00:00.000Z");
		CHECK(tags.has("account"));
		CHECK(tags["account"] == "user123");
	}

	SUBCASE("[IRCClient][IRCMessage] Tags without values") {
		Ref<IRCMessage> msg = IRCMessage::parse("@tag1;tag2 PING :server");

		Dictionary tags = msg->get_tags();
		CHECK(tags.size() == 2);
		CHECK(tags.has("tag1"));
		CHECK(tags.has("tag2"));
	}

	SUBCASE("[IRCClient][IRCMessage] Escaped tag values") {
		Ref<IRCMessage> msg = IRCMessage::parse("@tag=value\\:with\\ssemicolon\\sand\\sspace :server NOTICE user :test");

		Dictionary tags = msg->get_tags();
		CHECK(tags.has("tag"));
		String value = tags["tag"];
		CHECK(value == "value;with semicolon and space");
	}
}

TEST_CASE("[IRCClient][IRCMessage] CTCP parsing") {
	SUBCASE("[IRCClient][IRCMessage] CTCP ACTION") {
		Ref<IRCMessage> msg = IRCMessage::parse(":nick!user@host PRIVMSG #channel :\x01"
												"ACTION waves hello\x01");

		CHECK(msg->is_ctcp());
		CHECK(msg->get_ctcp_command() == "ACTION");
		CHECK(msg->get_ctcp_params() == "waves hello");
	}

	SUBCASE("[IRCClient][IRCMessage] CTCP VERSION") {
		Ref<IRCMessage> msg = IRCMessage::parse(":nick!user@host PRIVMSG target :\x01"
												"VERSION\x01");

		CHECK(msg->is_ctcp());
		CHECK(msg->get_ctcp_command() == "VERSION");
		CHECK(msg->get_ctcp_params().is_empty());
	}

	SUBCASE("[IRCClient][IRCMessage] CTCP encoding") {
		String encoded = IRCMessage::encode_ctcp("VERSION", "Blazium IRC 1.0");
		CHECK(encoded == "\x01"
						 "VERSION Blazium IRC 1.0\x01");
	}
}

TEST_CASE("[IRCClient][IRCMessage] Message serialization") {
	SUBCASE("[IRCClient][IRCMessage] Simple message") {
		Ref<IRCMessage> msg;
		msg.instantiate();
		msg->set_command("PING");
		PackedStringArray params;
		params.push_back("server.example.com");
		msg->set_params(params);

		String serialized = msg->to_string();
		CHECK(serialized == "PING :server.example.com");
	}

	SUBCASE("[IRCClient][IRCMessage] Message with prefix and tags") {
		Ref<IRCMessage> msg;
		msg.instantiate();

		Dictionary tags;
		tags["time"] = "2024-01-01T12:00:00.000Z";
		msg->set_tags(tags);

		msg->set_prefix("nick!user@host");
		msg->set_command("PRIVMSG");

		PackedStringArray params;
		params.push_back("#channel");
		params.push_back("Hello world");
		msg->set_params(params);

		String serialized = msg->to_string();
		// Should contain tags, prefix, command, and params
		CHECK(serialized.contains("@time=2024-01-01T12:00:00.000Z"));
		CHECK(serialized.contains(":nick!user@host"));
		CHECK(serialized.contains("PRIVMSG"));
		CHECK(serialized.contains("#channel"));
		CHECK(serialized.contains(":Hello world"));
	}
}

TEST_CASE("[IRCClient][IRCMessage] Edge cases") {
	SUBCASE("[IRCClient][IRCMessage] Empty message") {
		Ref<IRCMessage> msg = IRCMessage::parse("");
		CHECK(msg->get_command().is_empty());
	}

	SUBCASE("[IRCClient][IRCMessage] Message with colons in parameters") {
		Ref<IRCMessage> msg = IRCMessage::parse(":server 332 nick #channel :Topic with : colons : in it");

		CHECK(msg->get_params().size() == 3);
		CHECK(msg->get_params()[2] == "Topic with : colons : in it");
	}

	SUBCASE("[IRCClient][IRCMessage] Prefix extraction from server") {
		Ref<IRCMessage> msg = IRCMessage::parse(":server.example.com 001 nick :Welcome");

		CHECK(msg->get_prefix() == "server.example.com");
		CHECK(msg->get_nick() == "server.example.com"); // No ! in prefix
		CHECK(msg->get_username().is_empty());
		CHECK(msg->get_hostname().is_empty());
	}
}

} // namespace TestIRCClient
