/**************************************************************************/
/*  twitch_constants.h                                                    */
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

// Twitch IRC constants and command definitions

namespace TwitchIRC {

// Twitch IRC Server Addresses
constexpr const char *SERVER_SSL = "irc.chat.twitch.tv";
constexpr const char *SERVER_WEBSOCKET_SSL = "irc-ws.chat.twitch.tv";
constexpr int PORT_SSL = 6697;
constexpr int PORT_NON_SSL = 6667;
constexpr int PORT_WEBSOCKET_SSL = 443;

// Twitch Capabilities
namespace Capabilities {
constexpr const char *COMMANDS = "twitch.tv/commands";
constexpr const char *MEMBERSHIP = "twitch.tv/membership";
constexpr const char *TAGS = "twitch.tv/tags";
} // namespace Capabilities

// Twitch-Specific IRC Commands
namespace Commands {
constexpr const char *GLOBALUSERSTATE = "GLOBALUSERSTATE";
constexpr const char *ROOMSTATE = "ROOMSTATE";
constexpr const char *USERSTATE = "USERSTATE";
constexpr const char *CLEARMSG = "CLEARMSG";
constexpr const char *CLEARCHAT = "CLEARCHAT";
constexpr const char *HOSTTARGET = "HOSTTARGET";
constexpr const char *RECONNECT = "RECONNECT";
constexpr const char *USERNOTICE = "USERNOTICE";
constexpr const char *WHISPER = "WHISPER";
} // namespace Commands

// Twitch NOTICE msg-id Values
namespace NoticeIDs {
// Room mode changes
constexpr const char *EMOTE_ONLY_OFF = "emote_only_off";
constexpr const char *EMOTE_ONLY_ON = "emote_only_on";
constexpr const char *FOLLOWERS_OFF = "followers_off";
constexpr const char *FOLLOWERS_ON = "followers_on";
constexpr const char *FOLLOWERS_ON_ZERO = "followers_on_zero";
constexpr const char *SLOW_OFF = "slow_off";
constexpr const char *SLOW_ON = "slow_on";
constexpr const char *SUBS_OFF = "subs_off";
constexpr const char *SUBS_ON = "subs_on";

// Message rejections
constexpr const char *MSG_BANNED = "msg_banned";
constexpr const char *MSG_BAD_CHARACTERS = "msg_bad_characters";
constexpr const char *MSG_CHANNEL_BLOCKED = "msg_channel_blocked";
constexpr const char *MSG_CHANNEL_SUSPENDED = "msg_channel_suspended";
constexpr const char *MSG_DUPLICATE = "msg_duplicate";
constexpr const char *MSG_EMOTEONLY = "msg_emoteonly";
constexpr const char *MSG_FOLLOWERSONLY = "msg_followersonly";
constexpr const char *MSG_FOLLOWERSONLY_FOLLOWED = "msg_followersonly_followed";
constexpr const char *MSG_FOLLOWERSONLY_ZERO = "msg_followersonly_zero";
constexpr const char *MSG_R9K = "msg_r9k";
constexpr const char *MSG_RATELIMIT = "msg_ratelimit";
constexpr const char *MSG_REJECTED = "msg_rejected";
constexpr const char *MSG_REJECTED_MANDATORY = "msg_rejected_mandatory";
constexpr const char *MSG_REQUIRES_VERIFIED_PHONE = "msg_requires_verified_phone_number";
constexpr const char *MSG_SLOWMODE = "msg_slowmode";
constexpr const char *MSG_SUBSONLY = "msg_subsonly";
constexpr const char *MSG_SUSPENDED = "msg_suspended";
constexpr const char *MSG_TIMEDOUT = "msg_timedout";
constexpr const char *MSG_VERIFIED_EMAIL = "msg_verified_email";
constexpr const char *TOS_BAN = "tos_ban";
constexpr const char *UNRECOGNIZED_CMD = "unrecognized_cmd";
} // namespace NoticeIDs

// USERNOTICE msg-id Values
namespace UserNoticeIDs {
constexpr const char *SUB = "sub";
constexpr const char *RESUB = "resub";
constexpr const char *SUBGIFT = "subgift";
constexpr const char *SUBMYSTERYGIFT = "submysterygift";
constexpr const char *GIFTPAIDUPGRADE = "giftpaidupgrade";
constexpr const char *REWARDGIFT = "rewardgift";
constexpr const char *ANONGIFTPAIDUPGRADE = "anongiftpaidupgrade";
constexpr const char *RAID = "raid";
constexpr const char *UNRAID = "unraid";
constexpr const char *RITUAL = "ritual";
constexpr const char *BITSBADGETIER = "bitsbadgetier";
} // namespace UserNoticeIDs

// Twitch Tag Keys
namespace Tags {
constexpr const char *BADGE_INFO = "badge-info";
constexpr const char *BADGES = "badges";
constexpr const char *BITS = "bits";
constexpr const char *COLOR = "color";
constexpr const char *DISPLAY_NAME = "display-name";
constexpr const char *EMOTE_ONLY = "emote-only";
constexpr const char *EMOTES = "emotes";
constexpr const char *EMOTE_SETS = "emote-sets";
constexpr const char *FIRST_MSG = "first-msg";
constexpr const char *FLAGS = "flags";
constexpr const char *FOLLOWERS_ONLY = "followers-only";
constexpr const char *ID = "id";
constexpr const char *LOGIN = "login";
constexpr const char *MOD = "mod";
constexpr const char *MSG_ID = "msg-id";
constexpr const char *MSG_PARAM_CUMULATIVE_MONTHS = "msg-param-cumulative-months";
constexpr const char *MSG_PARAM_DISPLAYNAME = "msg-param-displayName";
constexpr const char *MSG_PARAM_LOGIN = "msg-param-login";
constexpr const char *MSG_PARAM_MONTHS = "msg-param-months";
constexpr const char *MSG_PARAM_PROMO_GIFT_TOTAL = "msg-param-promo-gift-total";
constexpr const char *MSG_PARAM_PROMO_NAME = "msg-param-promo-name";
constexpr const char *MSG_PARAM_RECIPIENT_DISPLAY_NAME = "msg-param-recipient-display-name";
constexpr const char *MSG_PARAM_RECIPIENT_ID = "msg-param-recipient-id";
constexpr const char *MSG_PARAM_RECIPIENT_USER_NAME = "msg-param-recipient-user-name";
constexpr const char *MSG_PARAM_SENDER_LOGIN = "msg-param-sender-login";
constexpr const char *MSG_PARAM_SENDER_NAME = "msg-param-sender-name";
constexpr const char *MSG_PARAM_SHOULD_SHARE_STREAK = "msg-param-should-share-streak";
constexpr const char *MSG_PARAM_STREAK_MONTHS = "msg-param-streak-months";
constexpr const char *MSG_PARAM_SUB_PLAN = "msg-param-sub-plan";
constexpr const char *MSG_PARAM_SUB_PLAN_NAME = "msg-param-sub-plan-name";
constexpr const char *MSG_PARAM_VIEWERCOUNT = "msg-param-viewerCount";
constexpr const char *MSG_PARAM_RITUAL_NAME = "msg-param-ritual-name";
constexpr const char *MSG_PARAM_THRESHOLD = "msg-param-threshold";
constexpr const char *R9K = "r9k";
constexpr const char *REPLY_PARENT_MSG_ID = "reply-parent-msg-id";
constexpr const char *REPLY_PARENT_USER_ID = "reply-parent-user-id";
constexpr const char *REPLY_PARENT_USER_LOGIN = "reply-parent-user-login";
constexpr const char *REPLY_PARENT_DISPLAY_NAME = "reply-parent-display-name";
constexpr const char *REPLY_PARENT_MSG_BODY = "reply-parent-msg-body";
constexpr const char *ROOM_ID = "room-id";
constexpr const char *SLOW = "slow";
constexpr const char *SUBSCRIBER = "subscriber";
constexpr const char *SUBS_ONLY = "subs-only";
constexpr const char *SYSTEM_MSG = "system-msg";
constexpr const char *TARGET_MSG_ID = "target-msg-id";
constexpr const char *TARGET_USER_ID = "target-user-id";
constexpr const char *TMI_SENT_TS = "tmi-sent-ts";
constexpr const char *TURBO = "turbo";
constexpr const char *USER_ID = "user-id";
constexpr const char *USER_TYPE = "user-type";
constexpr const char *VIPS = "vip";
} // namespace Tags

} // namespace TwitchIRC
