/**************************************************************************/
/*  twitch_message.cpp                                                    */
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

#include "twitch_message.h"

#include "twitch_constants.h"

void TwitchMessage::_bind_methods() {
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("is_twitch_command", "message"), &TwitchMessage::is_twitch_command);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_twitch_command", "message"), &TwitchMessage::get_twitch_command);

	ClassDB::bind_static_method("TwitchMessage", D_METHOD("parse_badges", "badges_string"), &TwitchMessage::parse_badges);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("parse_emotes", "emotes_string"), &TwitchMessage::parse_emotes);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("parse_emote_sets", "emote_sets_string"), &TwitchMessage::parse_emote_sets);

	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_user_id", "tags"), &TwitchMessage::get_user_id);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_display_name", "tags"), &TwitchMessage::get_display_name);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_color", "tags"), &TwitchMessage::get_color);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_room_id", "tags"), &TwitchMessage::get_room_id);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("get_msg_id", "tags"), &TwitchMessage::get_msg_id);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("is_moderator", "tags"), &TwitchMessage::is_moderator);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("is_subscriber", "tags"), &TwitchMessage::is_subscriber);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("is_vip", "tags"), &TwitchMessage::is_vip);
	ClassDB::bind_static_method("TwitchMessage", D_METHOD("is_turbo", "tags"), &TwitchMessage::is_turbo);

	ClassDB::bind_static_method("TwitchMessage", D_METHOD("parse_system_message", "system_msg"), &TwitchMessage::parse_system_message);
}

bool TwitchMessage::is_twitch_command(const Ref<IRCMessage> &p_message) {
	if (p_message.is_null()) {
		return false;
	}

	String command = p_message->get_command();

	return command == TwitchIRC::Commands::GLOBALUSERSTATE ||
			command == TwitchIRC::Commands::ROOMSTATE ||
			command == TwitchIRC::Commands::USERSTATE ||
			command == TwitchIRC::Commands::CLEARMSG ||
			command == TwitchIRC::Commands::CLEARCHAT ||
			command == TwitchIRC::Commands::HOSTTARGET ||
			command == TwitchIRC::Commands::RECONNECT ||
			command == TwitchIRC::Commands::USERNOTICE ||
			command == TwitchIRC::Commands::WHISPER;
}

String TwitchMessage::get_twitch_command(const Ref<IRCMessage> &p_message) {
	if (p_message.is_null()) {
		return "";
	}

	return p_message->get_command();
}

Dictionary TwitchMessage::parse_badges(const String &p_badges_string) {
	Dictionary badges;

	if (p_badges_string.is_empty()) {
		return badges;
	}

	// Format: "badge1/version1,badge2/version2,..."
	PackedStringArray badge_pairs = p_badges_string.split(",", false);

	for (int i = 0; i < badge_pairs.size(); i++) {
		String badge_pair = badge_pairs[i];
		int slash_pos = badge_pair.find("/");

		if (slash_pos != -1) {
			String badge_name = badge_pair.substr(0, slash_pos);
			String badge_version = badge_pair.substr(slash_pos + 1);
			badges[badge_name] = badge_version;
		}
	}

	return badges;
}

Dictionary TwitchMessage::parse_emotes(const String &p_emotes_string) {
	Dictionary emotes;

	if (p_emotes_string.is_empty()) {
		return emotes;
	}

	// Format: "emote_id:start-end,start-end/emote_id2:start-end/..."
	PackedStringArray emote_groups = p_emotes_string.split("/", false);

	for (int i = 0; i < emote_groups.size(); i++) {
		String emote_group = emote_groups[i];
		int colon_pos = emote_group.find(":");

		if (colon_pos != -1) {
			String emote_id = emote_group.substr(0, colon_pos);
			String positions_str = emote_group.substr(colon_pos + 1);

			PackedStringArray positions_list = positions_str.split(",", false);
			Array positions_array;

			for (int j = 0; j < positions_list.size(); j++) {
				PackedStringArray range = positions_list[j].split("-", false);
				if (range.size() == 2) {
					Dictionary position;
					position["start"] = range[0].to_int();
					position["end"] = range[1].to_int();
					positions_array.push_back(position);
				}
			}

			emotes[emote_id] = positions_array;
		}
	}

	return emotes;
}

Array TwitchMessage::parse_emote_sets(const String &p_emote_sets_string) {
	Array emote_sets;

	if (p_emote_sets_string.is_empty()) {
		return emote_sets;
	}

	// Format: "0,300374282,..."
	PackedStringArray sets = p_emote_sets_string.split(",", false);

	for (int i = 0; i < sets.size(); i++) {
		emote_sets.push_back(sets[i]);
	}

	return emote_sets;
}

String TwitchMessage::get_user_id(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::USER_ID, "");
}

String TwitchMessage::get_display_name(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::DISPLAY_NAME, "");
}

String TwitchMessage::get_color(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::COLOR, "");
}

String TwitchMessage::get_room_id(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::ROOM_ID, "");
}

String TwitchMessage::get_msg_id(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::MSG_ID, "");
}

bool TwitchMessage::is_moderator(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::MOD, "0") == "1";
}

bool TwitchMessage::is_subscriber(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::SUBSCRIBER, "0") == "1";
}

bool TwitchMessage::is_vip(const Dictionary &p_tags) {
	// VIP status can be in badges or vip tag
	String badges_str = p_tags.get(TwitchIRC::Tags::BADGES, "");
	return badges_str.contains("vip/") || p_tags.get(TwitchIRC::Tags::VIPS, "0") == "1";
}

bool TwitchMessage::is_turbo(const Dictionary &p_tags) {
	return p_tags.get(TwitchIRC::Tags::TURBO, "0") == "1";
}

String TwitchMessage::parse_system_message(const String &p_system_msg) {
	// System messages have escaped spaces: \s
	String result = p_system_msg;
	result = result.replace("\\s", " ");
	result = result.replace("\\:", ";");
	result = result.replace("\\\\", "\\");
	result = result.replace("\\r", "\r");
	result = result.replace("\\n", "\n");
	return result;
}

TwitchMessage::TwitchMessage() {
}

TwitchMessage::~TwitchMessage() {
}
