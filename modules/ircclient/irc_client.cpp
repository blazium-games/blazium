/**************************************************************************/
/*  irc_client.cpp                                                        */
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

#include "irc_client.h"

#include "irc_numerics.h"

#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/ip.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "modules/regex/regex.h"

void IRCClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port", "use_ssl", "nick", "username", "realname", "password"), &IRCClient::connect_to_server, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("disconnect_from_server", "quit_message"), &IRCClient::disconnect_from_server, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_irc_connected"), &IRCClient::is_irc_connected);
	ClassDB::bind_method(D_METHOD("get_status"), &IRCClient::get_status);

	ClassDB::bind_method(D_METHOD("poll"), &IRCClient::poll);

	ClassDB::bind_method(D_METHOD("send_raw", "message"), &IRCClient::send_raw);
	ClassDB::bind_method(D_METHOD("send_privmsg", "target", "message"), &IRCClient::send_privmsg);
	ClassDB::bind_method(D_METHOD("send_notice", "target", "message"), &IRCClient::send_notice);
	ClassDB::bind_method(D_METHOD("send_action", "target", "action"), &IRCClient::send_action);

	ClassDB::bind_method(D_METHOD("join_channel", "channel", "key"), &IRCClient::join_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("part_channel", "channel", "message"), &IRCClient::part_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("set_topic", "channel", "topic"), &IRCClient::set_topic);

	ClassDB::bind_method(D_METHOD("set_nick", "new_nick"), &IRCClient::set_nick);
	ClassDB::bind_method(D_METHOD("set_mode", "target", "modes", "params"), &IRCClient::set_mode, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("send_whois", "nick"), &IRCClient::send_whois);
	ClassDB::bind_method(D_METHOD("set_realname", "realname"), &IRCClient::set_realname);

	ClassDB::bind_method(D_METHOD("monitor_add", "nick"), &IRCClient::monitor_add);
	ClassDB::bind_method(D_METHOD("monitor_remove", "nick"), &IRCClient::monitor_remove);
	ClassDB::bind_method(D_METHOD("monitor_clear"), &IRCClient::monitor_clear);
	ClassDB::bind_method(D_METHOD("monitor_list"), &IRCClient::monitor_list);
	ClassDB::bind_method(D_METHOD("monitor_status"), &IRCClient::monitor_status);
	ClassDB::bind_method(D_METHOD("get_monitored_nicks"), &IRCClient::get_monitored_nicks);

	ClassDB::bind_method(D_METHOD("request_capability", "capability"), &IRCClient::request_capability);
	ClassDB::bind_method(D_METHOD("get_available_capabilities"), &IRCClient::get_available_capabilities);
	ClassDB::bind_method(D_METHOD("get_enabled_capabilities"), &IRCClient::get_enabled_capabilities);

	ClassDB::bind_method(D_METHOD("enable_sasl", "username", "password"), &IRCClient::enable_sasl);
	ClassDB::bind_method(D_METHOD("enable_sasl_plain", "username", "password"), &IRCClient::enable_sasl_plain);
	ClassDB::bind_method(D_METHOD("enable_sasl_external"), &IRCClient::enable_sasl_external);
	ClassDB::bind_method(D_METHOD("enable_sasl_scram_sha256", "username", "password"), &IRCClient::enable_sasl_scram_sha256);
	ClassDB::bind_method(D_METHOD("disable_sasl"), &IRCClient::disable_sasl);

	ClassDB::bind_method(D_METHOD("send_dcc_file", "nick", "file_path"), &IRCClient::send_dcc_file);
	ClassDB::bind_method(D_METHOD("accept_dcc_transfer", "transfer_index"), &IRCClient::accept_dcc_transfer);
	ClassDB::bind_method(D_METHOD("reject_dcc_transfer", "transfer_index"), &IRCClient::reject_dcc_transfer);
	ClassDB::bind_method(D_METHOD("cancel_dcc_transfer", "transfer_index"), &IRCClient::cancel_dcc_transfer);
	ClassDB::bind_method(D_METHOD("get_active_transfers"), &IRCClient::get_active_transfers);

	ClassDB::bind_method(D_METHOD("get_channel", "channel"), &IRCClient::get_channel);
	ClassDB::bind_method(D_METHOD("get_joined_channels"), &IRCClient::get_joined_channels);
	ClassDB::bind_method(D_METHOD("get_current_nick"), &IRCClient::get_current_nick);

	ClassDB::bind_method(D_METHOD("set_messages_per_second", "rate"), &IRCClient::set_messages_per_second);
	ClassDB::bind_method(D_METHOD("get_messages_per_second"), &IRCClient::get_messages_per_second);

	ClassDB::bind_method(D_METHOD("set_ping_timeout", "timeout_ms"), &IRCClient::set_ping_timeout);
	ClassDB::bind_method(D_METHOD("get_ping_timeout"), &IRCClient::get_ping_timeout);

	ClassDB::bind_method(D_METHOD("set_dcc_local_ip", "ip"), &IRCClient::set_dcc_local_ip);
	ClassDB::bind_method(D_METHOD("get_dcc_local_ip"), &IRCClient::get_dcc_local_ip);
	ClassDB::bind_method(D_METHOD("clear_dcc_local_ip"), &IRCClient::clear_dcc_local_ip);

	ClassDB::bind_method(D_METHOD("set_token_bucket_size", "size"), &IRCClient::set_token_bucket_size);
	ClassDB::bind_method(D_METHOD("get_token_bucket_size"), &IRCClient::get_token_bucket_size);

#ifdef MODULE_MBEDTLS_ENABLED
	ClassDB::bind_method(D_METHOD("set_tls_options", "options"), &IRCClient::set_tls_options);
	ClassDB::bind_method(D_METHOD("get_tls_options"), &IRCClient::get_tls_options);
#endif

	ClassDB::bind_method(D_METHOD("strip_formatting", "text"), &IRCClient::strip_formatting);
	ClassDB::bind_method(D_METHOD("parse_formatting", "text"), &IRCClient::parse_formatting);

	ClassDB::bind_method(D_METHOD("set_encoding", "encoding"), &IRCClient::set_encoding);
	ClassDB::bind_method(D_METHOD("get_encoding"), &IRCClient::get_encoding);
	ClassDB::bind_method(D_METHOD("set_auto_detect_encoding", "auto"), &IRCClient::set_auto_detect_encoding);
	ClassDB::bind_method(D_METHOD("get_auto_detect_encoding"), &IRCClient::get_auto_detect_encoding);
	ClassDB::bind_method(D_METHOD("get_supported_encodings"), &IRCClient::get_supported_encodings);

	ClassDB::bind_method(D_METHOD("set_history_enabled", "enabled"), &IRCClient::set_history_enabled);
	ClassDB::bind_method(D_METHOD("get_history_enabled"), &IRCClient::get_history_enabled);
	ClassDB::bind_method(D_METHOD("set_max_history_size", "size"), &IRCClient::set_max_history_size);
	ClassDB::bind_method(D_METHOD("get_max_history_size"), &IRCClient::get_max_history_size);
	ClassDB::bind_method(D_METHOD("get_message_history"), &IRCClient::get_message_history);
	ClassDB::bind_method(D_METHOD("clear_message_history"), &IRCClient::clear_message_history);

	ClassDB::bind_method(D_METHOD("enable_auto_reconnect", "enabled"), &IRCClient::enable_auto_reconnect);
	ClassDB::bind_method(D_METHOD("is_auto_reconnect_enabled"), &IRCClient::is_auto_reconnect_enabled);
	ClassDB::bind_method(D_METHOD("set_reconnect_delay", "seconds"), &IRCClient::set_reconnect_delay);
	ClassDB::bind_method(D_METHOD("get_reconnect_delay"), &IRCClient::get_reconnect_delay);
	ClassDB::bind_method(D_METHOD("set_max_reconnect_attempts", "max"), &IRCClient::set_max_reconnect_attempts);
	ClassDB::bind_method(D_METHOD("get_max_reconnect_attempts"), &IRCClient::get_max_reconnect_attempts);
	ClassDB::bind_method(D_METHOD("get_reconnect_attempts"), &IRCClient::get_reconnect_attempts);

	ClassDB::bind_method(D_METHOD("set_alternative_nicks", "nicks"), &IRCClient::set_alternative_nicks);
	ClassDB::bind_method(D_METHOD("add_alternative_nick", "nick"), &IRCClient::add_alternative_nick);
	ClassDB::bind_method(D_METHOD("clear_alternative_nicks"), &IRCClient::clear_alternative_nicks);
	ClassDB::bind_method(D_METHOD("get_alternative_nicks"), &IRCClient::get_alternative_nicks);

	ClassDB::bind_method(D_METHOD("add_autojoin_channel", "channel", "key"), &IRCClient::add_autojoin_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("remove_autojoin_channel", "channel"), &IRCClient::remove_autojoin_channel);
	ClassDB::bind_method(D_METHOD("clear_autojoin_channels"), &IRCClient::clear_autojoin_channels);
	ClassDB::bind_method(D_METHOD("get_autojoin_channels"), &IRCClient::get_autojoin_channels);
	ClassDB::bind_method(D_METHOD("enable_autojoin", "enabled"), &IRCClient::enable_autojoin);
	ClassDB::bind_method(D_METHOD("is_autojoin_enabled"), &IRCClient::is_autojoin_enabled);

	ClassDB::bind_method(D_METHOD("ignore_user", "mask"), &IRCClient::ignore_user);
	ClassDB::bind_method(D_METHOD("unignore_user", "mask"), &IRCClient::unignore_user);
	ClassDB::bind_method(D_METHOD("clear_ignores"), &IRCClient::clear_ignores);
	ClassDB::bind_method(D_METHOD("get_ignored_users"), &IRCClient::get_ignored_users);
	ClassDB::bind_method(D_METHOD("is_ignored", "nick"), &IRCClient::is_ignored);

	ClassDB::bind_method(D_METHOD("op_user", "channel", "nick"), &IRCClient::op_user);
	ClassDB::bind_method(D_METHOD("deop_user", "channel", "nick"), &IRCClient::deop_user);
	ClassDB::bind_method(D_METHOD("voice_user", "channel", "nick"), &IRCClient::voice_user);
	ClassDB::bind_method(D_METHOD("devoice_user", "channel", "nick"), &IRCClient::devoice_user);
	ClassDB::bind_method(D_METHOD("kick_user", "channel", "nick", "reason"), &IRCClient::kick_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("ban_user", "channel", "mask"), &IRCClient::ban_user);
	ClassDB::bind_method(D_METHOD("unban_user", "channel", "mask"), &IRCClient::unban_user);
	ClassDB::bind_method(D_METHOD("kickban_user", "channel", "nick", "reason"), &IRCClient::kickban_user, DEFVAL(""));

	ClassDB::bind_method(D_METHOD("add_fallback_server", "host", "port", "use_ssl"), &IRCClient::add_fallback_server, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("clear_fallback_servers"), &IRCClient::clear_fallback_servers);
	ClassDB::bind_method(D_METHOD("get_fallback_server_count"), &IRCClient::get_fallback_server_count);

	ClassDB::bind_method(D_METHOD("identify_nickserv", "password"), &IRCClient::identify_nickserv);
	ClassDB::bind_method(D_METHOD("ghost_nick", "nick", "password"), &IRCClient::ghost_nick);
	ClassDB::bind_method(D_METHOD("register_nick", "email", "password"), &IRCClient::register_nick);
	ClassDB::bind_method(D_METHOD("group_nick", "password"), &IRCClient::group_nick);
	ClassDB::bind_method(D_METHOD("register_channel", "channel"), &IRCClient::register_channel);
	ClassDB::bind_method(D_METHOD("identify_chanserv", "channel", "password"), &IRCClient::identify_chanserv);

	ClassDB::bind_method(D_METHOD("list_channels", "pattern", "min_users", "max_users"), &IRCClient::list_channels, DEFVAL(""), DEFVAL(0), DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("request_chathistory", "target", "timestamp_start", "timestamp_end", "limit"), &IRCClient::request_chathistory, DEFVAL(100));
	ClassDB::bind_method(D_METHOD("request_chathistory_before", "target", "msgid", "limit"), &IRCClient::request_chathistory_before, DEFVAL(100));
	ClassDB::bind_method(D_METHOD("request_chathistory_after", "target", "msgid", "limit"), &IRCClient::request_chathistory_after, DEFVAL(100));
	ClassDB::bind_method(D_METHOD("request_chathistory_latest", "target", "limit"), &IRCClient::request_chathistory_latest, DEFVAL(100));

	ClassDB::bind_method(D_METHOD("send_multiline_privmsg", "target", "lines"), &IRCClient::send_multiline_privmsg);
	ClassDB::bind_method(D_METHOD("send_multiline_notice", "target", "lines"), &IRCClient::send_multiline_notice);

	ClassDB::bind_method(D_METHOD("get_matching_nicks", "channel", "prefix"), &IRCClient::get_matching_nicks);
	ClassDB::bind_method(D_METHOD("complete_nick", "channel", "partial", "cycle"), &IRCClient::complete_nick, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("add_highlight_pattern", "pattern"), &IRCClient::add_highlight_pattern);
	ClassDB::bind_method(D_METHOD("remove_highlight_pattern", "pattern"), &IRCClient::remove_highlight_pattern);
	ClassDB::bind_method(D_METHOD("clear_highlight_patterns"), &IRCClient::clear_highlight_patterns);
	ClassDB::bind_method(D_METHOD("get_highlight_patterns"), &IRCClient::get_highlight_patterns);
	ClassDB::bind_method(D_METHOD("is_highlighted", "message", "nick"), &IRCClient::is_highlighted, DEFVAL(""));

	ClassDB::bind_method(D_METHOD("extract_urls", "message"), &IRCClient::extract_urls);

	ClassDB::bind_method(D_METHOD("get_message_text_by_id", "message_id"), &IRCClient::get_message_text_by_id);
	ClassDB::bind_method(D_METHOD("set_max_tracked_messages", "max"), &IRCClient::set_max_tracked_messages);
	ClassDB::bind_method(D_METHOD("get_max_tracked_messages"), &IRCClient::get_max_tracked_messages);

	ClassDB::bind_method(D_METHOD("get_connection_stats"), &IRCClient::get_connection_stats);
	ClassDB::bind_method(D_METHOD("reset_connection_stats"), &IRCClient::reset_connection_stats);
	ClassDB::bind_method(D_METHOD("get_average_latency"), &IRCClient::get_average_latency);

	ClassDB::bind_method(D_METHOD("send_oper", "username", "password"), &IRCClient::send_oper);
	ClassDB::bind_method(D_METHOD("knock_channel", "channel", "message"), &IRCClient::knock_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("silence_user", "mask"), &IRCClient::silence_user);
	ClassDB::bind_method(D_METHOD("unsilence_user", "mask"), &IRCClient::unsilence_user);
	ClassDB::bind_method(D_METHOD("list_silence"), &IRCClient::list_silence);
	ClassDB::bind_method(D_METHOD("who_channel", "channel"), &IRCClient::who_channel);
	ClassDB::bind_method(D_METHOD("who_user", "mask"), &IRCClient::who_user);
	ClassDB::bind_method(D_METHOD("whox", "mask", "fields", "querytype"), &IRCClient::whox, DEFVAL("tcuihsnfdlar"), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("whowas_user", "nick", "count"), &IRCClient::whowas_user, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("invite_user", "channel", "nick"), &IRCClient::invite_user);
	ClassDB::bind_method(D_METHOD("userhost", "nick"), &IRCClient::userhost);
	ClassDB::bind_method(D_METHOD("register_account", "account", "password", "email"), &IRCClient::register_account, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("verify_account", "account", "code"), &IRCClient::verify_account);

	ClassDB::bind_method(D_METHOD("set_channel_key", "channel", "key"), &IRCClient::set_channel_key);
	ClassDB::bind_method(D_METHOD("get_channel_key", "channel"), &IRCClient::get_channel_key);
	ClassDB::bind_method(D_METHOD("clear_channel_key", "channel"), &IRCClient::clear_channel_key);
	ClassDB::bind_method(D_METHOD("request_ban_list", "channel"), &IRCClient::request_ban_list);
	ClassDB::bind_method(D_METHOD("set_ban", "channel", "mask"), &IRCClient::set_ban);
	ClassDB::bind_method(D_METHOD("remove_ban", "channel", "mask"), &IRCClient::remove_ban);
	ClassDB::bind_method(D_METHOD("request_exception_list", "channel"), &IRCClient::request_exception_list);
	ClassDB::bind_method(D_METHOD("set_exception", "channel", "mask"), &IRCClient::set_exception);
	ClassDB::bind_method(D_METHOD("request_invite_list", "channel"), &IRCClient::request_invite_list);
	ClassDB::bind_method(D_METHOD("set_invite_exception", "channel", "mask"), &IRCClient::set_invite_exception);
	ClassDB::bind_method(D_METHOD("quiet_user", "channel", "mask"), &IRCClient::quiet_user);
	ClassDB::bind_method(D_METHOD("unquiet_user", "channel", "mask"), &IRCClient::unquiet_user);
	ClassDB::bind_method(D_METHOD("request_quiet_list", "channel"), &IRCClient::request_quiet_list);

	ClassDB::bind_method(D_METHOD("set_away", "message"), &IRCClient::set_away);
	ClassDB::bind_method(D_METHOD("set_back"), &IRCClient::set_back);
	ClassDB::bind_method(D_METHOD("get_is_away"), &IRCClient::get_is_away);
	ClassDB::bind_method(D_METHOD("get_away_message"), &IRCClient::get_away_message);

	ClassDB::bind_method(D_METHOD("get_user", "nick"), &IRCClient::get_user);
	ClassDB::bind_method(D_METHOD("get_common_channels", "nick"), &IRCClient::get_common_channels);
	ClassDB::bind_method(D_METHOD("get_user_info", "nick"), &IRCClient::get_user_info);

	ClassDB::bind_method(D_METHOD("send_read_marker", "channel", "timestamp"), &IRCClient::send_read_marker);
	ClassDB::bind_method(D_METHOD("send_typing_notification", "channel", "typing"), &IRCClient::send_typing_notification);
	ClassDB::bind_method(D_METHOD("send_reaction", "channel", "msgid", "reaction"), &IRCClient::send_reaction);
	ClassDB::bind_method(D_METHOD("remove_reaction", "channel", "msgid", "reaction"), &IRCClient::remove_reaction);

	ClassDB::bind_method(D_METHOD("set_bot_mode", "enabled"), &IRCClient::set_bot_mode);
	ClassDB::bind_method(D_METHOD("get_bot_mode"), &IRCClient::get_bot_mode);

	ClassDB::bind_method(D_METHOD("send_reply", "target", "message", "reply_to_msgid"), &IRCClient::send_reply);
	ClassDB::bind_method(D_METHOD("send_reply_notice", "target", "message", "reply_to_msgid"), &IRCClient::send_reply_notice);
	ClassDB::bind_method(D_METHOD("get_reply_to_msgid", "tags"), &IRCClient::get_reply_to_msgid);

	ClassDB::bind_method(D_METHOD("has_sts_policy", "hostname"), &IRCClient::has_sts_policy);
	ClassDB::bind_method(D_METHOD("clear_sts_policy", "hostname"), &IRCClient::clear_sts_policy);
	ClassDB::bind_method(D_METHOD("clear_all_sts_policies"), &IRCClient::clear_all_sts_policies);

	// Signals
	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("disconnected", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("connection_error", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("status_changed", PropertyInfo(Variant::INT, "status")));

	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::OBJECT, "message", PROPERTY_HINT_RESOURCE_TYPE, "IRCMessage")));
	ADD_SIGNAL(MethodInfo("privmsg", PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "target"), PropertyInfo(Variant::STRING, "text"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("notice", PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "target"), PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("ctcp_received", PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "params")));
	ADD_SIGNAL(MethodInfo("ctcp_reply", PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "params")));

	ADD_SIGNAL(MethodInfo("joined", PropertyInfo(Variant::STRING, "channel")));
	ADD_SIGNAL(MethodInfo("parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("kicked", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "kicker"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("user_joined", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user"), PropertyInfo(Variant::STRING, "account"), PropertyInfo(Variant::STRING, "realname")));
	ADD_SIGNAL(MethodInfo("user_parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("user_quit", PropertyInfo(Variant::STRING, "user"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("user_kicked", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "kicker"), PropertyInfo(Variant::STRING, "kicked"), PropertyInfo(Variant::STRING, "reason")));

	ADD_SIGNAL(MethodInfo("nick_changed", PropertyInfo(Variant::STRING, "old_nick"), PropertyInfo(Variant::STRING, "new_nick")));
	ADD_SIGNAL(MethodInfo("mode_changed", PropertyInfo(Variant::STRING, "target"), PropertyInfo(Variant::STRING, "modes"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "params")));
	ADD_SIGNAL(MethodInfo("topic_changed", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::STRING, "setter")));

	ADD_SIGNAL(MethodInfo("numeric_001_welcome", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("numeric_005_isupport", PropertyInfo(Variant::DICTIONARY, "features")));
	ADD_SIGNAL(MethodInfo("numeric_332_topic", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "topic")));
	ADD_SIGNAL(MethodInfo("numeric_353_names", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "names")));
	ADD_SIGNAL(MethodInfo("numeric_366_endofnames", PropertyInfo(Variant::STRING, "channel")));
	ADD_SIGNAL(MethodInfo("numeric_372_motd", PropertyInfo(Variant::STRING, "line")));
	ADD_SIGNAL(MethodInfo("numeric_433_nicknameinuse", PropertyInfo(Variant::STRING, "nick")));
	ADD_SIGNAL(MethodInfo("numeric_received", PropertyInfo(Variant::INT, "code"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "params")));

	ADD_SIGNAL(MethodInfo("numeric_730_mononline", PropertyInfo(Variant::PACKED_STRING_ARRAY, "nicks")));
	ADD_SIGNAL(MethodInfo("numeric_731_monoffline", PropertyInfo(Variant::PACKED_STRING_ARRAY, "nicks")));

	ADD_SIGNAL(MethodInfo("dcc_request", PropertyInfo(Variant::OBJECT, "transfer", PROPERTY_HINT_RESOURCE_TYPE, "IRCDCCTransfer")));
	ADD_SIGNAL(MethodInfo("dcc_progress", PropertyInfo(Variant::INT, "transfer_index"), PropertyInfo(Variant::INT, "bytes"), PropertyInfo(Variant::INT, "total")));
	ADD_SIGNAL(MethodInfo("dcc_completed", PropertyInfo(Variant::INT, "transfer_index")));
	ADD_SIGNAL(MethodInfo("dcc_failed", PropertyInfo(Variant::INT, "transfer_index"), PropertyInfo(Variant::STRING, "error")));

	ADD_SIGNAL(MethodInfo("capability_list", PropertyInfo(Variant::PACKED_STRING_ARRAY, "capabilities")));
	ADD_SIGNAL(MethodInfo("capability_acknowledged", PropertyInfo(Variant::STRING, "capability")));
	ADD_SIGNAL(MethodInfo("capability_denied", PropertyInfo(Variant::STRING, "capability")));
	ADD_SIGNAL(MethodInfo("sasl_success"));
	ADD_SIGNAL(MethodInfo("sasl_failed", PropertyInfo(Variant::STRING, "reason")));

	// Account Registration signals (IRCv3 draft)
	ADD_SIGNAL(MethodInfo("account_registration_success", PropertyInfo(Variant::STRING, "account")));
	ADD_SIGNAL(MethodInfo("account_registration_failed", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("account_verification_required", PropertyInfo(Variant::STRING, "account"), PropertyInfo(Variant::STRING, "method")));
	ADD_SIGNAL(MethodInfo("account_verification_success", PropertyInfo(Variant::STRING, "account")));
	ADD_SIGNAL(MethodInfo("account_verification_failed", PropertyInfo(Variant::STRING, "reason")));

	ADD_SIGNAL(MethodInfo("tag_json_data", PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::DICTIONARY, "data")));
	ADD_SIGNAL(MethodInfo("tag_base64_data", PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::STRING, "encoded"), PropertyInfo(Variant::STRING, "decoded")));

	ADD_SIGNAL(MethodInfo("standard_reply_fail", PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "context"), PropertyInfo(Variant::STRING, "description"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("standard_reply_warn", PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "context"), PropertyInfo(Variant::STRING, "description"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("standard_reply_note", PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "context"), PropertyInfo(Variant::STRING, "description"), PropertyInfo(Variant::DICTIONARY, "tags")));

	ADD_SIGNAL(MethodInfo("batch_started", PropertyInfo(Variant::STRING, "ref_tag"), PropertyInfo(Variant::STRING, "batch_type"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "params")));
	ADD_SIGNAL(MethodInfo("batch_ended", PropertyInfo(Variant::STRING, "ref_tag"), PropertyInfo(Variant::STRING, "batch_type"), PropertyInfo(Variant::ARRAY, "messages")));

	ADD_SIGNAL(MethodInfo("highlighted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("latency_measured", PropertyInfo(Variant::INT, "latency_ms")));

	BIND_ENUM_CONSTANT(STATUS_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATUS_CONNECTING);
	BIND_ENUM_CONSTANT(STATUS_REGISTERING);
	BIND_ENUM_CONSTANT(STATUS_CONNECTED);
	BIND_ENUM_CONSTANT(STATUS_ERROR);
}

Error IRCClient::connect_to_server(const String &p_host, int p_port, bool p_use_ssl, const String &p_nick, const String &p_username, const String &p_realname, const String &p_password) {
	if (status != STATUS_DISCONNECTED) {
		disconnect_from_server();
	}

	// Check for STS policy (IRCv3 Strict Transport Security)
	int actual_port = p_port;
	bool actual_use_ssl = p_use_ssl;

	if (has_sts_policy(p_host)) {
		const STSPolicy &policy = sts_policies[p_host];
		// STS policy requires TLS upgrade
		if (!p_use_ssl) {
			actual_port = policy.port;
			actual_use_ssl = true;
		}
	}

	current_server = p_host;
	current_port = actual_port;
	current_nick = p_nick;
	username = p_username;
	realname = p_realname;
	password = p_password;

#ifdef MODULE_MBEDTLS_ENABLED
	use_tls = actual_use_ssl;
#else
	if (actual_use_ssl) {
		ERR_PRINT("SSL/TLS requested but mbedtls module is not available");
	}
#endif

	tcp_connection.instantiate();

	IPAddress ip;
	if (p_host.is_valid_ip_address()) {
		ip = IPAddress(p_host);
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_host);
		if (!ip.is_valid()) {
			emit_signal("connection_error", "Failed to resolve hostname");
			status = STATUS_ERROR;
			return ERR_CANT_RESOLVE;
		}
	}

	Error err = tcp_connection->connect_to_host(ip, actual_port);
	if (err != OK) {
		emit_signal("connection_error", "Failed to connect");
		status = STATUS_ERROR;
		return err;
	}

	status = STATUS_CONNECTING;
	emit_signal("status_changed", status);
	last_ping_time = Time::get_singleton()->get_ticks_msec();

	// Save connection params for auto-reconnect
	_save_connection_params(p_host, p_port, p_use_ssl, p_nick, p_username, p_realname, p_password);

	return OK;
}

void IRCClient::disconnect_from_server(const String &p_quit_message) {
	if (status != STATUS_DISCONNECTED) {
		if (!p_quit_message.is_empty()) {
			_send_immediate("QUIT :" + p_quit_message);
		}

		if (tcp_connection.is_valid()) {
			tcp_connection->disconnect_from_host();
			tcp_connection.unref();
		}

#ifdef MODULE_MBEDTLS_ENABLED
		if (tls_connection.is_valid()) {
			tls_connection->disconnect_from_stream();
			tls_connection.unref();
		}
#endif

		status = STATUS_DISCONNECTED;
		emit_signal("status_changed", status);
		emit_signal("disconnected", p_quit_message);

		// Clear state
		channels.clear();
		users.clear();
		send_queue.clear();
		receive_buffer = "";
		available_capabilities.clear();
		enabled_capabilities.clear();
		requested_capabilities.clear();
		cap_negotiation_active = false;
		sasl_in_progress = false;
		monitored_nicks.clear();

		// Don't clear message history on disconnect - user might want to review it
	}
}

bool IRCClient::is_irc_connected() const {
	return status == STATUS_CONNECTED;
}

IRCClient::Status IRCClient::get_status() const {
	return status;
}

Error IRCClient::poll() {
	if (status == STATUS_DISCONNECTED || status == STATUS_ERROR) {
		// Attempt auto-reconnect if enabled
		if (auto_reconnect_enabled) {
			_attempt_reconnect();
		}
		return OK;
	}

	// Poll DCC transfers
	for (int i = active_transfers.size() - 1; i >= 0; i--) {
		Ref<IRCDCCTransfer> transfer = active_transfers[i];
		if (transfer.is_valid()) {
			transfer->poll();

			IRCDCCTransfer::Status transfer_status = transfer->get_status();
			if (transfer_status == IRCDCCTransfer::DCC_STATUS_TRANSFERRING) {
				emit_signal("dcc_progress", i, transfer->get_transferred(), transfer->get_file_size());
			} else if (transfer_status == IRCDCCTransfer::DCC_STATUS_COMPLETED) {
				emit_signal("dcc_completed", i);
				active_transfers.remove_at(i);
			} else if (transfer_status == IRCDCCTransfer::DCC_STATUS_FAILED) {
				emit_signal("dcc_failed", i, transfer->get_error_message());
				active_transfers.remove_at(i);
			}
		}
	}

	// Check connection status
	if (tcp_connection.is_null()) {
		return OK;
	}

	Error err = tcp_connection->poll();
	if (err != OK && err != ERR_BUSY) {
		disconnect_from_server();
		emit_signal("connection_error", "Connection lost");
		status = STATUS_ERROR;
		return err;
	}

	StreamPeerTCP::Status tcp_status = tcp_connection->get_status();

	if (status == STATUS_CONNECTING) {
		if (tcp_status == StreamPeerTCP::STATUS_CONNECTED) {
#ifdef MODULE_MBEDTLS_ENABLED
			if (use_tls) {
				tls_connection = Ref<StreamPeerTLS>(StreamPeerTLS::create());
				Ref<TLSOptions> tls_opts = tls_options.is_valid() ? tls_options : TLSOptions::client();
				Error tls_err = tls_connection->connect_to_stream(tcp_connection, current_server, tls_opts);
				if (tls_err != OK) {
					disconnect_from_server();
					emit_signal("connection_error", "TLS handshake failed");
					status = STATUS_ERROR;
					return tls_err;
				}
			}
#endif
			status = STATUS_REGISTERING;
			emit_signal("status_changed", status);

			// Start capability negotiation (IRCv3)
			_start_capability_negotiation();
		} else if (tcp_status == StreamPeerTCP::STATUS_ERROR) {
			disconnect_from_server();
			emit_signal("connection_error", "Connection failed");
			status = STATUS_ERROR;
			return FAILED;
		}
	}

#ifdef MODULE_MBEDTLS_ENABLED
	if (use_tls && tls_connection.is_valid()) {
		tls_connection->poll();
	}
#endif

	if (tcp_status == StreamPeerTCP::STATUS_CONNECTED) {
		// Receive data
		StreamPeer *stream = tcp_connection.ptr();
#ifdef MODULE_MBEDTLS_ENABLED
		if (use_tls && tls_connection.is_valid()) {
			stream = tls_connection.ptr();
		}
#endif

		int available = stream->get_available_bytes();
		if (available > 0) {
			PackedByteArray data;
			data.resize(available);
			int received = stream->get_data(data.ptrw(), available);

			if (received > 0) {
				// Detect encoding if auto-detection enabled
				String detected_encoding = encoding;
				if (auto_detect_encoding && encoding == "UTF-8") {
					detected_encoding = _detect_encoding(data);
					if (detected_encoding != encoding) {
						encoding = detected_encoding;
					}
				}

				// Convert from detected/configured encoding
				String received_text = _convert_from_encoding(data, encoding);
				receive_buffer += received_text;

				// Process complete lines
				int line_end;
				while ((line_end = receive_buffer.find("\n")) != -1) {
					String line = receive_buffer.substr(0, line_end).strip_edges();
					receive_buffer = receive_buffer.substr(line_end + 1);

					if (!line.is_empty()) {
						_process_message(line);
					}
				}
			}

			last_ping_time = Time::get_singleton()->get_ticks_msec();
		}

		// Process send queue
		_process_send_queue();

		// Check ping timeout
		uint64_t current_time = Time::get_singleton()->get_ticks_msec();
		if (current_time - last_ping_time > ping_timeout) {
			disconnect_from_server();
			emit_signal("disconnected", "Ping timeout");
			return ERR_TIMEOUT;
		}
	}

	return OK;
}

void IRCClient::send_raw(const String &p_message) {
	// Validate message length
	if (!_validate_message_length(p_message)) {
		WARN_PRINT(vformat("IRC message exceeds 512 byte limit (%d bytes): %s",
				p_message.utf8().length() + 2, p_message.substr(0, 50)));
		// Still queue it but warn - server will likely truncate or reject
	}
	_send_with_priority(p_message, 0); // Normal priority
}

void IRCClient::send_privmsg(const String &p_target, const String &p_message) {
	// Check if message needs to be split
	String full_message = "PRIVMSG " + p_target + " :" + p_message;
	if (!_validate_message_length(full_message)) {
		// Split long message into multiple messages
		Vector<String> parts = _split_long_message(p_target, p_message);
		for (int i = 0; i < parts.size(); i++) {
			send_raw("PRIVMSG " + p_target + " :" + parts[i]);
		}
	} else {
		send_raw(full_message);
	}
}

void IRCClient::send_notice(const String &p_target, const String &p_message) {
	send_raw("NOTICE " + p_target + " :" + p_message);
}

void IRCClient::send_action(const String &p_target, const String &p_action) {
	String ctcp = IRCMessage::encode_ctcp("ACTION", p_action);
	send_privmsg(p_target, ctcp);
}

void IRCClient::join_channel(const String &p_channel, const String &p_key) {
	if (p_key.is_empty()) {
		send_raw("JOIN " + p_channel);
	} else {
		send_raw("JOIN " + p_channel + " " + p_key);
	}
}

void IRCClient::part_channel(const String &p_channel, const String &p_message) {
	if (p_message.is_empty()) {
		send_raw("PART " + p_channel);
	} else {
		send_raw("PART " + p_channel + " :" + p_message);
	}
}

void IRCClient::set_topic(const String &p_channel, const String &p_topic) {
	send_raw("TOPIC " + p_channel + " :" + p_topic);
}

void IRCClient::set_nick(const String &p_new_nick) {
	send_raw("NICK " + p_new_nick);
	current_nick = p_new_nick;
}

void IRCClient::set_mode(const String &p_target, const String &p_modes, const PackedStringArray &p_params) {
	String cmd = "MODE " + p_target + " " + p_modes;
	for (int i = 0; i < p_params.size(); i++) {
		cmd += " " + p_params[i];
	}
	send_raw(cmd);
}

void IRCClient::send_whois(const String &p_nick) {
	send_raw("WHOIS " + p_nick);
}

void IRCClient::set_realname(const String &p_realname) {
	realname = p_realname;
	// IRCv3.3 SETNAME capability
	send_raw("SETNAME :" + p_realname);
}

void IRCClient::monitor_add(const String &p_nick) {
	if (!monitored_nicks.has(p_nick)) {
		monitored_nicks.push_back(p_nick);
	}
	send_raw("MONITOR + " + p_nick);
}

void IRCClient::monitor_remove(const String &p_nick) {
	int idx = monitored_nicks.find(p_nick);
	if (idx != -1) {
		monitored_nicks.remove_at(idx);
	}
	send_raw("MONITOR - " + p_nick);
}

void IRCClient::monitor_clear() {
	monitored_nicks.clear();
	send_raw("MONITOR C");
}

void IRCClient::monitor_list() {
	send_raw("MONITOR L");
}

void IRCClient::monitor_status() {
	send_raw("MONITOR S");
}

PackedStringArray IRCClient::get_monitored_nicks() const {
	return monitored_nicks;
}

void IRCClient::request_capability(const String &p_capability) {
	if (!requested_capabilities.has(p_capability)) {
		requested_capabilities.push_back(p_capability);
	}

	if (cap_negotiation_active) {
		send_raw("CAP REQ :" + p_capability);
	}
}

PackedStringArray IRCClient::get_available_capabilities() const {
	return available_capabilities;
}

PackedStringArray IRCClient::get_enabled_capabilities() const {
	return enabled_capabilities;
}

void IRCClient::enable_sasl(const String &p_username, const String &p_password) {
	enable_sasl_plain(p_username, p_password);
}

void IRCClient::enable_sasl_plain(const String &p_username, const String &p_password) {
	sasl_enabled = true;
	sasl_mechanism = SASL_PLAIN;
	sasl_username = p_username;
	sasl_password = p_password;
	request_capability(IRCNumerics::Capabilities::SASL);
}

void IRCClient::enable_sasl_external() {
	sasl_enabled = true;
	sasl_mechanism = SASL_EXTERNAL;
	sasl_username = "";
	sasl_password = "";
	request_capability(IRCNumerics::Capabilities::SASL);
}

void IRCClient::enable_sasl_scram_sha256(const String &p_username, const String &p_password) {
	WARN_PRINT("SASL SCRAM-SHA-256 requires full HMAC-SHA-256 + PBKDF2 implementation (RFC 7677). Using SASL PLAIN as fallback. For production SCRAM support, use enable_sasl_plain() or enable_sasl_external().");
	// Fallback to PLAIN - Full SCRAM-SHA-256 implementation requires:
	// - HMAC-SHA-256 wrapper around mbedtls
	// - PBKDF2 key derivation
	// - Challenge-response flow with client/server nonces
	// - Multiple AUTHENTICATE message exchanges
	// - ~500-800 lines of cryptographic code
	enable_sasl_plain(p_username, p_password);
}

void IRCClient::disable_sasl() {
	sasl_enabled = false;
	sasl_mechanism = SASL_PLAIN;
	sasl_username = "";
	sasl_password = "";
}

Error IRCClient::send_dcc_file(const String &p_nick, const String &p_file_path) {
	Ref<IRCDCCTransfer> transfer;
	transfer.instantiate();
	transfer->set_transfer_type(IRCDCCTransfer::TYPE_FILE_SEND);
	transfer->set_remote_nick(p_nick);

	// Get filename from path and sanitize it
	String filename = _sanitize_dcc_filename(p_file_path.get_file());
	transfer->set_filename(filename);

	Error err = transfer->start_send(p_file_path);
	if (err != OK) {
		return err;
	}

	active_transfers.push_back(transfer);

	// Get configured or auto-detected local IP
	IPAddress local_ip = get_dcc_local_ip();
	if (!local_ip.is_valid()) {
		ERR_PRINT("Failed to determine local IP address for DCC");
		return ERR_CANT_RESOLVE;
	}

	transfer->set_address(local_ip);
	transfer->set_use_ipv6(!local_ip.is_ipv4());

	// Create DCC SEND message with appropriate format
	String dcc_msg = transfer->_create_dcc_send_message();

	String ctcp = IRCMessage::encode_ctcp("DCC", dcc_msg);
	send_privmsg(p_nick, ctcp);

	return OK;
}

void IRCClient::accept_dcc_transfer(int p_transfer_index) {
	ERR_FAIL_INDEX(p_transfer_index, active_transfers.size());
	active_transfers[p_transfer_index]->accept();
}

void IRCClient::reject_dcc_transfer(int p_transfer_index) {
	ERR_FAIL_INDEX(p_transfer_index, active_transfers.size());
	active_transfers[p_transfer_index]->reject();
	active_transfers.remove_at(p_transfer_index);
}

void IRCClient::cancel_dcc_transfer(int p_transfer_index) {
	ERR_FAIL_INDEX(p_transfer_index, active_transfers.size());
	active_transfers[p_transfer_index]->cancel();
	active_transfers.remove_at(p_transfer_index);
}

TypedArray<IRCDCCTransfer> IRCClient::get_active_transfers() const {
	TypedArray<IRCDCCTransfer> result;
	for (const Ref<IRCDCCTransfer> &transfer : active_transfers) {
		result.push_back(transfer);
	}
	return result;
}

Ref<IRCChannel> IRCClient::get_channel(const String &p_channel) const {
	if (channels.has(p_channel)) {
		return channels[p_channel];
	}
	return Ref<IRCChannel>();
}

PackedStringArray IRCClient::get_joined_channels() const {
	PackedStringArray result;
	for (const KeyValue<String, Ref<IRCChannel>> &kv : channels) {
		result.push_back(kv.key);
	}
	return result;
}

String IRCClient::get_current_nick() const {
	return current_nick;
}

void IRCClient::set_messages_per_second(int p_rate) {
	messages_per_second = MAX(1, p_rate);
}

int IRCClient::get_messages_per_second() const {
	return messages_per_second;
}

void IRCClient::set_ping_timeout(int p_timeout_ms) {
	ping_timeout = p_timeout_ms;
}

int IRCClient::get_ping_timeout() const {
	return ping_timeout;
}

void IRCClient::set_dcc_local_ip(const IPAddress &p_ip) {
	dcc_local_ip = p_ip;
	dcc_local_ip_configured = true;
}

IPAddress IRCClient::get_dcc_local_ip() const {
	if (dcc_local_ip_configured) {
		return dcc_local_ip;
	}

	// Auto-detect local IP
	List<IPAddress> local_addresses;
	IP::get_singleton()->get_local_addresses(&local_addresses);
	if (local_addresses.size() > 0) {
		// Try to find a non-loopback IPv4 address first
		for (const IPAddress &addr : local_addresses) {
			if (addr.is_valid() && !addr.is_wildcard() && addr.is_ipv4()) {
				return addr;
			}
		}
		// Fall back to first address
		return local_addresses.front()->get();
	}

	return IPAddress();
}

void IRCClient::clear_dcc_local_ip() {
	dcc_local_ip_configured = false;
	dcc_local_ip = IPAddress();
}

void IRCClient::_process_message(const String &p_raw_message) {
	Ref<IRCMessage> message = IRCMessage::parse(p_raw_message);
	emit_signal("message_received", message);

	// Track metrics
	metrics.messages_received++;
	metrics.bytes_received += p_raw_message.utf8().length() + 2; // +2 for \r\n

	// Track PONG latency
	if (message->get_command() == "PONG" && last_ping_sent > 0) {
		uint64_t current_time = Time::get_singleton()->get_ticks_msec();
		int latency = current_time - last_ping_sent;
		metrics.total_latency_ms += latency;
		metrics.ping_count++;
		last_ping_sent = 0;

		emit_signal("latency_measured", latency);
	}

	// Process tags for base64/JSON detection
	Dictionary tags = message->get_tags();
	if (!tags.is_empty()) {
		Array keys = tags.keys();
		for (int i = 0; i < keys.size(); i++) {
			String key = keys[i];
			Variant value = tags[key];

			// Only process string values
			if (value.get_type() == Variant::STRING) {
				String str_value = value;

				// Check if it's base64
				if (IRCMessage::is_base64(str_value)) {
					String decoded = IRCMessage::decode_base64_string(str_value);

					// Check if decoded value is JSON
					if (IRCMessage::is_json(decoded)) {
						Variant json_data = IRCMessage::parse_json_string(decoded);
						if (json_data.get_type() == Variant::DICTIONARY) {
							emit_signal("tag_json_data", key, json_data);
						}
					} else {
						// Emit base64 data signal (not JSON)
						emit_signal("tag_base64_data", key, str_value, decoded);
					}
				}
			}
		}
	}

	_handle_message(message);
}

void IRCClient::_handle_message(Ref<IRCMessage> p_message) {
	// Check if message is part of a batch
	Dictionary tags = p_message->get_tags();
	if (tags.has("batch")) {
		String batch_ref = tags["batch"];
		if (active_batches.has(batch_ref)) {
			// Add message to batch
			active_batches[batch_ref].messages.push_back(p_message);
		}
	}

	if (p_message->is_numeric()) {
		_handle_numeric(p_message);
	} else {
		_handle_command(p_message);
	}
}

void IRCClient::_handle_numeric(Ref<IRCMessage> p_message) {
	int numeric = p_message->get_numeric();
	PackedStringArray params = p_message->get_params();

	emit_signal("numeric_received", numeric, params);

	switch (numeric) {
		case IRCNumerics::RPL_WELCOME: {
			// Successfully registered
			status = STATUS_CONNECTED;
			emit_signal("status_changed", status);
			emit_signal("connected");

			if (params.size() > 1) {
				emit_signal("numeric_001_welcome", params[1]);
			}

			// Reset reconnect attempts on successful connection
			reconnect_attempts = 0;

			// Initialize connection metrics
			metrics.connection_time = Time::get_singleton()->get_ticks_msec();

			// Set bot mode if enabled (IRCv3)
			if (bot_mode_enabled) {
				send_raw("MODE " + current_nick + " +B");
			}

			// Perform auto-join
			_perform_autojoin();
		} break;

		case IRCNumerics::RPL_ISUPPORT: {
			// Server features (005)
			Dictionary features;
			for (int i = 1; i < params.size() - 1; i++) {
				String param = params[i];
				int eq_pos = param.find("=");
				if (eq_pos != -1) {
					String key = param.substr(0, eq_pos);
					String value = param.substr(eq_pos + 1);
					features[key] = value;

					// Check for UTF8ONLY token (IRCv3)
					if (key == "UTF8ONLY") {
						// Server only accepts UTF-8
						encoding = "UTF-8";
						auto_detect_encoding = false; // Disable auto-detection
					}
				} else {
					features[param] = true;

					// Check for UTF8ONLY token without value (IRCv3)
					if (param == "UTF8ONLY") {
						// Server only accepts UTF-8
						encoding = "UTF-8";
						auto_detect_encoding = false; // Disable auto-detection
					}
				}
			}
			emit_signal("numeric_005_isupport", features);
		} break;

		case IRCNumerics::RPL_TOPIC: {
			// Channel topic (332)
			if (params.size() >= 3) {
				String channel = params[1];
				String topic = params[2];

				Ref<IRCChannel> chan = _get_or_create_channel(channel);
				chan->set_topic(topic);

				emit_signal("numeric_332_topic", channel, topic);
			}
		} break;

		case IRCNumerics::RPL_NAMREPLY: {
			// Names list (353)
			if (params.size() >= 4) {
				String channel = params[2];
				String names = params[3];
				_parse_names_reply(channel, names);

				PackedStringArray names_array = names.split(" ", false);
				emit_signal("numeric_353_names", channel, names_array);
			}
		} break;

		case IRCNumerics::RPL_ENDOFNAMES: {
			// End of names (366)
			if (params.size() >= 2) {
				String channel = params[1];
				emit_signal("numeric_366_endofnames", channel);
			}
		} break;

		case IRCNumerics::RPL_MOTD: {
			// MOTD line (372)
			if (params.size() >= 2) {
				emit_signal("numeric_372_motd", params[1]);
			}
		} break;

		case IRCNumerics::ERR_NICKNAMEINUSE: {
			// Nick in use (433)
			if (params.size() >= 2) {
				emit_signal("numeric_433_nicknameinuse", params[1]);
			}

			// Try alternative nicknames if available
			if (alternative_nicks.size() > 0 && current_nick_index < alternative_nicks.size()) {
				String alt_nick = alternative_nicks[current_nick_index];
				current_nick_index++;
				set_nick(alt_nick);
			} else if (alternative_nicks.size() > 0 && current_nick_index >= alternative_nicks.size()) {
				// All alternatives exhausted, try with random suffix
				String base_nick = alternative_nicks.size() > 0 ? alternative_nicks[0] : current_nick;
				String new_nick = base_nick + String::num_int64(OS::get_singleton()->get_ticks_usec() % 10000);
				set_nick(new_nick);
			}
		} break;

		case IRCNumerics::RPL_LOGGEDIN: {
			// SASL logged in (900)
			emit_signal("sasl_success");
		} break;

		case IRCNumerics::RPL_SASLSUCCESS: {
			// SASL success (903)
			emit_signal("sasl_success");
			sasl_in_progress = false;
		} break;

		case IRCNumerics::ERR_SASLFAIL:
		case IRCNumerics::ERR_SASLTOOLONG:
		case IRCNumerics::ERR_SASLABORTED: {
			// SASL failed
			String reason = params.size() > 1 ? params[params.size() - 1] : "Authentication failed";
			emit_signal("sasl_failed", reason);
			sasl_in_progress = false;
		} break;

		case IRCNumerics::RPL_MONONLINE: {
			// MONITOR online notification (730)
			if (params.size() >= 2) {
				String targets = params[1];
				PackedStringArray nicks = targets.split(",", false);
				emit_signal("numeric_730_mononline", nicks);
			}
		} break;

		case IRCNumerics::RPL_MONOFFLINE: {
			// MONITOR offline notification (731)
			if (params.size() >= 2) {
				String targets = params[1];
				PackedStringArray nicks = targets.split(",", false);
				emit_signal("numeric_731_monoffline", nicks);
			}
		} break;
	}
}

void IRCClient::_handle_command(Ref<IRCMessage> p_message) {
	String command = p_message->get_command();
	PackedStringArray params = p_message->get_params();
	String prefix = p_message->get_prefix();
	Dictionary tags = p_message->get_tags();

	if (command == "PING") {
		// Auto-respond to PING with high priority
		if (params.size() > 0) {
			_send_with_priority("PONG :" + params[0], 100); // Highest priority
		}
	} else if (command == "CAP") {
		_handle_capability_response(p_message);
	} else if (command == "AUTHENTICATE") {
		_handle_sasl(p_message);
	} else if (command == "PRIVMSG") {
		if (params.size() >= 2) {
			String sender = p_message->get_nick();
			String target = params[0];
			String text = params[1];

			// Check ignore list
			if (_is_ignored(sender)) {
				return; // Silently ignore messages from ignored users
			}

			// Check for CTCP
			if (p_message->is_ctcp()) {
				String ctcp_cmd = p_message->get_ctcp_command();
				String ctcp_params = p_message->get_ctcp_params();

				emit_signal("ctcp_received", sender, ctcp_cmd, ctcp_params);

				// Handle DCC requests
				if (ctcp_cmd == "DCC") {
					PackedStringArray dcc_parts = ctcp_params.split(" ", false);
					if (dcc_parts.size() >= 5 && dcc_parts[0] == "SEND") {
						Ref<IRCDCCTransfer> transfer;
						transfer.instantiate();
						transfer->set_transfer_type(IRCDCCTransfer::TYPE_FILE_RECEIVE);
						transfer->set_remote_nick(sender);

						// Sanitize the filename for security
						String sanitized_filename = _sanitize_dcc_filename(dcc_parts[1]);
						transfer->set_filename(sanitized_filename);

						// Parse IP address (could be IPv4 integer or IPv6 string)
						String ip_str = dcc_parts[2];
						IPAddress ip;

						if (ip_str.contains(":")) {
							// IPv6 address string
							ip = IPAddress(ip_str);
							transfer->set_use_ipv6(true);
						} else if (ip_str.is_valid_ip_address()) {
							// IPv4 address string
							ip = IPAddress(ip_str);
							transfer->set_use_ipv6(false);
						} else {
							// IPv4 as integer (legacy format)
							uint32_t ip_int = ip_str.to_int();
							ip.set_ipv4((uint8_t *)&ip_int);
							transfer->set_use_ipv6(false);
						}

						transfer->set_address(ip);
						transfer->set_port(dcc_parts[3].to_int());
						transfer->set_file_size(dcc_parts[4].to_int());

						active_transfers.push_back(transfer);
						emit_signal("dcc_request", transfer);
					}
				} else if (ctcp_cmd == "VERSION") {
					// Auto-respond to VERSION
					String version_reply = IRCMessage::encode_ctcp("VERSION", "Blazium IRC Client 1.0");
					send_notice(sender, version_reply);
				} else if (ctcp_cmd == "PING") {
					// Auto-respond to PING
					String ping_reply = IRCMessage::encode_ctcp("PING", ctcp_params);
					send_notice(sender, ping_reply);
				} else if (ctcp_cmd == "TIME") {
					// Auto-respond to TIME
					Dictionary time_dict = Time::get_singleton()->get_datetime_dict_from_system();
					Array time_args;
					time_args.push_back(time_dict["year"]);
					time_args.push_back(time_dict["month"]);
					time_args.push_back(time_dict["day"]);
					time_args.push_back(time_dict["hour"]);
					time_args.push_back(time_dict["minute"]);
					time_args.push_back(time_dict["second"]);
					String time_str = String("{0}-{1}-{2} {3}:{4}:{5}").format(time_args);
					String time_reply = IRCMessage::encode_ctcp("TIME", time_str);
					send_notice(sender, time_reply);
				}
			} else {
				// Track message ID if present
				if (tags.has("msgid")) {
					String msg_id = tags["msgid"];
					message_id_to_text[msg_id] = text;

					// Limit tracked messages
					if ((int)message_id_to_text.size() > max_tracked_messages) {
						// Remove oldest (simple approach - could be improved with LRU)
						// HashMap doesn't have keys() method - iterate and remove first
						if (message_id_to_text.size() > 0) {
							HashMap<String, String>::Iterator it = message_id_to_text.begin();
							if (it) {
								message_id_to_text.remove(it);
							}
						}
					}
				}

				// Check for highlights
				if (is_highlighted(text)) {
					emit_signal("highlighted", target, sender, text, tags);
				}

				emit_signal("privmsg", sender, target, text, tags);

				// Add to message history if enabled
				if (history_enabled) {
					HistoryMessage hist_msg;
					hist_msg.timestamp = Time::get_singleton()->get_ticks_msec();
					hist_msg.sender = sender;
					hist_msg.target = target;
					hist_msg.message = text;
					hist_msg.tags = tags;

					message_history.push_back(hist_msg);

					// Trim history if it exceeds max size
					if (message_history.size() > max_history_size) {
						message_history.remove_at(0);
					}
				}
			}
		}
	} else if (command == IRCNumerics::StandardReplies::FAIL) {
		// IRCv3.4 Standard Replies - FAIL
		if (params.size() >= 3) {
			String failed_command = params[0];
			String error_code = params[1];
			String context = params.size() >= 3 ? params[2] : "";
			String description = params.size() >= 4 ? params[3] : "";
			emit_signal("standard_reply_fail", failed_command, error_code, context, description, tags);
		}
	} else if (command == IRCNumerics::StandardReplies::WARN) {
		// IRCv3.4 Standard Replies - WARN
		if (params.size() >= 3) {
			String warned_command = params[0];
			String warning_code = params[1];
			String context = params.size() >= 3 ? params[2] : "";
			String description = params.size() >= 4 ? params[3] : "";
			emit_signal("standard_reply_warn", warned_command, warning_code, context, description, tags);
		}
	} else if (command == IRCNumerics::StandardReplies::NOTE) {
		// IRCv3.4 Standard Replies - NOTE
		if (params.size() >= 3) {
			String noted_command = params[0];
			String note_code = params[1];
			String context = params.size() >= 3 ? params[2] : "";
			String description = params.size() >= 4 ? params[3] : "";
			emit_signal("standard_reply_note", noted_command, note_code, context, description, tags);
		}
	} else if (command == "BATCH") {
		// IRCv3 BATCH command
		if (params.size() >= 1) {
			String batch_ref = params[0];

			if (batch_ref.begins_with("+")) {
				// Start batch
				String ref_tag = batch_ref.substr(1);
				String batch_type = params.size() >= 2 ? params[1] : "";

				BatchInfo batch;
				batch.batch_type = batch_type;
				for (int i = 2; i < params.size(); i++) {
					batch.params.push_back(params[i]);
				}
				active_batches[ref_tag] = batch;

				emit_signal("batch_started", ref_tag, batch_type, batch.params);
			} else if (batch_ref.begins_with("-")) {
				// End batch
				String ref_tag = batch_ref.substr(1);

				if (active_batches.has(ref_tag)) {
					const BatchInfo &batch = active_batches[ref_tag];
					// Convert Vector to Array for signal emission
					Array messages_array;
					for (int i = 0; i < batch.messages.size(); i++) {
						messages_array.push_back(batch.messages[i]);
					}
					emit_signal("batch_ended", ref_tag, batch.batch_type, messages_array);
					active_batches.erase(ref_tag);
				}
			}
		}
	} else if (command == "NOTICE") {
		if (params.size() >= 2) {
			String sender = p_message->get_nick();
			String target = params[0];
			String text = params[1];

			// Check for CTCP reply
			if (p_message->is_ctcp()) {
				String ctcp_cmd = p_message->get_ctcp_command();
				String ctcp_params = p_message->get_ctcp_params();
				emit_signal("ctcp_reply", sender, ctcp_cmd, ctcp_params);
			} else {
				emit_signal("notice", sender, target, text);
			}
		}
	} else if (command == "JOIN") {
		if (params.size() >= 1) {
			String nick = p_message->get_nick();
			String channel = params[0];

			// IRCv3 extended-join
			String account = "";
			String user_realname = "";
			if (params.size() >= 3) {
				account = params[1];
				user_realname = params[2];
			}

			if (nick == current_nick) {
				// We joined
				_get_or_create_channel(channel);
				emit_signal("joined", channel);
			} else {
				// Someone else joined
				Ref<IRCChannel> chan = _get_or_create_channel(channel);
				chan->add_user(nick);
				emit_signal("user_joined", channel, nick, account, user_realname);
			}
		}
	} else if (command == "PART") {
		if (params.size() >= 1) {
			String nick = p_message->get_nick();
			String channel = params[0];
			String message = params.size() >= 2 ? params[1] : "";

			if (nick == current_nick) {
				// We left
				channels.erase(channel);
				emit_signal("parted", channel, message);
			} else {
				// Someone else left
				if (channels.has(channel)) {
					channels[channel]->remove_user(nick);
				}
				emit_signal("user_parted", channel, nick, message);
			}
		}
	} else if (command == "QUIT") {
		String nick = p_message->get_nick();
		String message = params.size() >= 1 ? params[0] : "";

		// Remove user from all channels
		for (KeyValue<String, Ref<IRCChannel>> &kv : channels) {
			kv.value->remove_user(nick);
		}

		emit_signal("user_quit", nick, message);
	} else if (command == "KICK") {
		if (params.size() >= 2) {
			String kicker = p_message->get_nick();
			String channel = params[0];
			String kicked = params[1];
			String reason = params.size() >= 3 ? params[2] : "";

			if (kicked == current_nick) {
				// We were kicked
				channels.erase(channel);
				emit_signal("kicked", channel, kicker, reason);
			} else {
				// Someone else was kicked
				if (channels.has(channel)) {
					channels[channel]->remove_user(kicked);
				}
				emit_signal("user_kicked", channel, kicker, kicked, reason);
			}
		}
	} else if (command == "NICK") {
		if (params.size() >= 1) {
			String old_nick = p_message->get_nick();
			String new_nick = params[0];

			if (old_nick == current_nick) {
				current_nick = new_nick;
			}

			// Update nick in all channels
			for (KeyValue<String, Ref<IRCChannel>> &kv : channels) {
				if (kv.value->has_user(old_nick)) {
					String modes = kv.value->get_user_modes(old_nick);
					kv.value->remove_user(old_nick);
					kv.value->add_user(new_nick, modes);
				}
			}

			emit_signal("nick_changed", old_nick, new_nick);
		}
	} else if (command == "TOPIC") {
		if (params.size() >= 2) {
			String setter = p_message->get_nick();
			String channel = params[0];
			String topic = params[1];

			if (channels.has(channel)) {
				channels[channel]->set_topic(topic);
				channels[channel]->set_topic_setter(setter);
			}

			emit_signal("topic_changed", channel, topic, setter);
		}
	} else if (command == "MODE") {
		if (params.size() >= 2) {
			String target = params[0];
			String modes = params[1];

			PackedStringArray mode_params;
			for (int i = 2; i < params.size(); i++) {
				mode_params.push_back(params[i]);
			}

			emit_signal("mode_changed", target, modes, mode_params);
		}
	} else if (command == "SETNAME") {
		// IRCv3.3 SETNAME - realname change
		if (params.size() >= 1) {
			String nick = p_message->get_nick();
			String new_realname = params[0];

			if (users.has(nick)) {
				users[nick]->set_realname(new_realname);
			}
		}
	} else if (command == "MONITOR") {
		// MONITOR command responses handled by numerics
	}
}

void IRCClient::_send_immediate(const String &p_message) {
	if (tcp_connection.is_null()) {
		return;
	}

	StreamPeer *stream = tcp_connection.ptr();
#ifdef MODULE_MBEDTLS_ENABLED
	if (use_tls && tls_connection.is_valid()) {
		stream = tls_connection.ptr();
	}
#endif

	String message = p_message + "\r\n";

	// Convert to configured encoding
	PackedByteArray encoded_data = _convert_to_encoding(message, encoding);
	stream->put_data(encoded_data.ptr(), encoded_data.size());

	// Track metrics
	metrics.messages_sent++;
	metrics.bytes_sent += encoded_data.size();

	// Track PING for latency measurement
	if (p_message.begins_with("PING ")) {
		last_ping_sent = Time::get_singleton()->get_ticks_msec();
	}
}

void IRCClient::_send_with_priority(const String &p_message, int p_priority) {
	QueuedMessage qmsg;
	qmsg.message = p_message;
	qmsg.priority = p_priority;

	// Insert into queue based on priority (higher priority first)
	bool inserted = false;
	for (List<QueuedMessage>::Element *E = send_queue.front(); E; E = E->next()) {
		if (p_priority > E->get().priority) {
			send_queue.insert_before(E, qmsg);
			inserted = true;
			break;
		}
	}

	if (!inserted) {
		send_queue.push_back(qmsg);
	}
}

void IRCClient::_process_send_queue() {
	if (send_queue.is_empty()) {
		return;
	}

	uint64_t current_time = Time::get_singleton()->get_ticks_msec();

	// Refill token bucket
	_refill_token_bucket();

	// Check if we have tokens available and enough time has passed
	uint64_t time_between_messages = 1000 / messages_per_second;

	if (token_bucket_tokens > 0 && current_time - last_send_time >= time_between_messages) {
		QueuedMessage qmsg = send_queue.front()->get();
		send_queue.pop_front();

		_send_immediate(qmsg.message);
		last_send_time = current_time;
		token_bucket_tokens--;
	}
}

void IRCClient::_refill_token_bucket() {
	uint64_t current_time = Time::get_singleton()->get_ticks_msec();

	if (last_token_refill == 0) {
		last_token_refill = current_time;
		return;
	}

	// Refill one token per second
	uint64_t elapsed = current_time - last_token_refill;
	int tokens_to_add = elapsed / 1000;

	if (tokens_to_add > 0) {
		token_bucket_tokens = MIN(token_bucket_tokens + tokens_to_add, token_bucket_size);
		last_token_refill = current_time;
	}
}

void IRCClient::_start_capability_negotiation() {
	cap_negotiation_active = true;
	_send_immediate("CAP LS 302");

	// Don't send NICK/USER yet - wait for capability negotiation to complete
}

void IRCClient::_handle_capability_response(Ref<IRCMessage> p_message) {
	PackedStringArray params = p_message->get_params();
	if (params.size() < 2) {
		return;
	}

	String subcommand = params[1];

	if (subcommand == "LS") {
		// List of available capabilities
		if (params.size() >= 3) {
			String caps_string = params[params.size() - 1];
			PackedStringArray caps = caps_string.split(" ", false);

			for (int i = 0; i < caps.size(); i++) {
				String cap = caps[i];

				// Check for capability with value (e.g., sts=6697,2592000)
				int eq_pos = cap.find("=");
				if (eq_pos != -1) {
					String cap_name = cap.substr(0, eq_pos);
					String cap_value = cap.substr(eq_pos + 1);

					if (!available_capabilities.has(cap_name)) {
						available_capabilities.push_back(cap_name);
					}

					// Handle STS capability
					if (cap_name == "sts") {
						_handle_sts_capability(cap_value);
					}
				} else {
					if (!available_capabilities.has(cap)) {
						available_capabilities.push_back(cap);
					}
				}
			}

			// Check for multiline (CAP LS 302)
			bool is_multiline = params.size() >= 3 && params[2] == "*";
			if (!is_multiline) {
				// End of capability list
				emit_signal("capability_list", available_capabilities);

				// Request desired capabilities
				if (!requested_capabilities.is_empty()) {
					String req_caps = "";
					for (int i = 0; i < requested_capabilities.size(); i++) {
						if (available_capabilities.has(requested_capabilities[i])) {
							if (!req_caps.is_empty()) {
								req_caps += " ";
							}
							req_caps += requested_capabilities[i];
						}
					}

					if (!req_caps.is_empty()) {
						_send_immediate("CAP REQ :" + req_caps);
					} else {
						_end_capability_negotiation();
					}
				} else {
					_end_capability_negotiation();
				}
			}
		}
	} else if (subcommand == "ACK") {
		// Capabilities acknowledged
		if (params.size() >= 3) {
			String caps_string = params[2];
			PackedStringArray caps = caps_string.split(" ", false);

			for (int i = 0; i < caps.size(); i++) {
				if (!enabled_capabilities.has(caps[i])) {
					enabled_capabilities.push_back(caps[i]);
				}
				emit_signal("capability_acknowledged", caps[i]);

				// Start SASL if enabled and SASL capability was acknowledged
				if (caps[i] == IRCNumerics::Capabilities::SASL && sasl_enabled) {
					sasl_in_progress = true;
					if (sasl_mechanism == SASL_EXTERNAL) {
						_send_immediate("AUTHENTICATE EXTERNAL");
					} else {
						_send_immediate("AUTHENTICATE PLAIN");
					}
				}
			}
		}

		// End capability negotiation if SASL is not in progress
		if (!sasl_in_progress) {
			_end_capability_negotiation();
		}
	} else if (subcommand == "NAK") {
		// Capabilities denied
		if (params.size() >= 3) {
			String caps_string = params[2];
			PackedStringArray caps = caps_string.split(" ", false);

			for (int i = 0; i < caps.size(); i++) {
				emit_signal("capability_denied", caps[i]);
			}
		}

		_end_capability_negotiation();
	}
}

void IRCClient::_end_capability_negotiation() {
	if (cap_negotiation_active) {
		cap_negotiation_active = false;
		_send_immediate("CAP END");

		// Now send registration
		if (!password.is_empty()) {
			_send_immediate("PASS " + password);
		}
		_send_immediate("NICK " + current_nick);
		_send_immediate("USER " + username + " 0 * :" + realname);
	}
}

void IRCClient::_handle_sasl(Ref<IRCMessage> p_message) {
	PackedStringArray params = p_message->get_params();
	if (params.size() < 1) {
		return;
	}

	String param = params[0];

	if (param == "+") {
		// Server ready for authentication data
		if (sasl_mechanism == SASL_EXTERNAL) {
			// EXTERNAL mechanism: just send +
			// Authentication is via client certificate
			_send_immediate("AUTHENTICATE +");
		} else {
			// PLAIN mechanism: send base64(username\0username\0password)
			String auth_string = sasl_username + String::chr(0) + sasl_username + String::chr(0) + sasl_password;
			PackedByteArray auth_bytes = auth_string.to_utf8_buffer();

			// Base64 encode
			String auth_base64 = CryptoCore::b64_encode_str(auth_bytes.ptr(), auth_bytes.size());

			// Split into SASL_CHUNK_SIZE chunks if needed
			if (auth_base64.length() <= SASL_CHUNK_SIZE) {
				_send_immediate("AUTHENTICATE " + auth_base64);
			} else {
				// Send in chunks
				int offset = 0;
				while (offset < auth_base64.length()) {
					String chunk = auth_base64.substr(offset, SASL_CHUNK_SIZE);
					_send_immediate("AUTHENTICATE " + chunk);
					offset += SASL_CHUNK_SIZE;
				}
				// Send empty line to indicate end
				if (auth_base64.length() % SASL_CHUNK_SIZE == 0) {
					_send_immediate("AUTHENTICATE +");
				}
			}
		}
	}
}

void IRCClient::_parse_names_reply(const String &p_channel, const String &p_names) {
	Ref<IRCChannel> channel = _get_or_create_channel(p_channel);

	PackedStringArray names = p_names.split(" ", false);
	for (int i = 0; i < names.size(); i++) {
		String name = names[i];
		String modes = "";

		// Strip mode prefixes (@, +, etc.)
		while (!name.is_empty() && (name[0] == '@' || name[0] == '+' || name[0] == '%' || name[0] == '~' || name[0] == '&')) {
			modes += name[0];
			name = name.substr(1);
		}

		channel->add_user(name, modes);
	}
}

Ref<IRCChannel> IRCClient::_get_or_create_channel(const String &p_channel) {
	if (channels.has(p_channel)) {
		return channels[p_channel];
	}

	Ref<IRCChannel> channel;
	channel.instantiate();
	channel->set_name(p_channel);
	channels[p_channel] = channel;
	return channel;
}

Ref<IRCUser> IRCClient::_get_or_create_user(const String &p_nick) {
	if (users.has(p_nick)) {
		return users[p_nick];
	}

	Ref<IRCUser> user;
	user.instantiate();
	user->set_nick(p_nick);
	users[p_nick] = user;
	return user;
}

bool IRCClient::_validate_message_length(const String &p_message) const {
	// IRC messages are limited to 512 bytes including CRLF
	// Calculate actual wire length with CRLF
	int wire_length = p_message.utf8().length() + 2; // +2 for \r\n
	return wire_length <= MAX_IRC_MESSAGE_LENGTH;
}

Vector<String> IRCClient::_split_long_message(const String &p_target, const String &p_message) const {
	Vector<String> result;

	// Calculate overhead: "PRIVMSG <target> :" + CRLF
	String prefix = "PRIVMSG " + p_target + " :";
	int overhead = prefix.utf8().length() + 2; // +2 for \r\n
	int max_content_length = MAX_IRC_MESSAGE_LENGTH - overhead;

	if (p_message.utf8().length() <= max_content_length) {
		result.push_back(p_message);
		return result;
	}

	// Split message into chunks
	String remaining = p_message;
	while (!remaining.is_empty()) {
		// Find split point that doesn't exceed max length
		int split_pos = max_content_length;
		if (remaining.utf8().length() > max_content_length) {
			// Try to split at a space to avoid breaking words
			int last_space = remaining.substr(0, split_pos).rfind(" ");
			if (last_space > split_pos / 2) { // Only if space is in latter half
				split_pos = last_space;
			}
		} else {
			split_pos = remaining.length();
		}

		result.push_back(remaining.substr(0, split_pos).strip_edges());
		remaining = remaining.substr(split_pos).strip_edges();
	}

	return result;
}

String IRCClient::_sanitize_dcc_filename(const String &p_filename) const {
	String sanitized = p_filename;

	// Remove any directory components
	sanitized = sanitized.get_file();

	// Replace any remaining path separators
	sanitized = sanitized.replace("/", "_");
	sanitized = sanitized.replace("\\", "_");

	// Remove any null bytes
	sanitized = sanitized.replace(String::chr(0), "");

	// Check for path traversal attempts
	if (sanitized.contains("..")) {
		sanitized = sanitized.replace("..", "_");
	}

	// Replace spaces with underscores for better compatibility
	sanitized = sanitized.replace(" ", "_");

	// Ensure filename is not empty
	if (sanitized.is_empty()) {
		sanitized = "file";
	}

	// Limit length to reasonable value (255 is typical filesystem limit)
	if (sanitized.length() > 255) {
		// Preserve file extension if present
		String ext = sanitized.get_extension();
		String base = sanitized.get_basename();
		if (!ext.is_empty()) {
			base = base.substr(0, 255 - ext.length() - 1);
			sanitized = base + "." + ext;
		} else {
			sanitized = sanitized.substr(0, 255);
		}
	}

	return sanitized;
}

void IRCClient::set_token_bucket_size(int p_size) {
	token_bucket_size = MAX(1, p_size);
	token_bucket_tokens = MIN(token_bucket_tokens, token_bucket_size);
}

int IRCClient::get_token_bucket_size() const {
	return token_bucket_size;
}

#ifdef MODULE_MBEDTLS_ENABLED
void IRCClient::set_tls_options(const Ref<TLSOptions> &p_options) {
	tls_options = p_options;
}

Ref<TLSOptions> IRCClient::get_tls_options() const {
	return tls_options;
}
#endif

String IRCClient::strip_formatting(const String &p_text) {
	return _strip_irc_formatting(p_text);
}

Dictionary IRCClient::parse_formatting(const String &p_text) {
	return _parse_irc_formatting(p_text);
}

String IRCClient::_strip_irc_formatting(const String &p_text) const {
	String result;

	for (int i = 0; i < p_text.length(); i++) {
		char32_t c = p_text[i];

		// mIRC color codes: ^C[foreground][,background]
		if (c == 0x03) { // Color code
			i++; // Skip color code char
			// Skip up to 2 digits for foreground
			int digits = 0;
			while (i < p_text.length() && is_digit(p_text[i]) && digits < 2) {
				i++;
				digits++;
			}
			// Check for comma and background color
			if (i < p_text.length() && p_text[i] == ',') {
				i++; // Skip comma
				digits = 0;
				while (i < p_text.length() && is_digit(p_text[i]) && digits < 2) {
					i++;
					digits++;
				}
			}
			i--; // Adjust for loop increment
		}
		// Bold: ^B (0x02)
		else if (c == 0x02) {
			// Skip
		}
		// Italic: ^I (0x1D)
		else if (c == 0x1D) {
			// Skip
		}
		// Underline: ^U (0x1F)
		else if (c == 0x1F) {
			// Skip
		}
		// Strikethrough: ^S (0x1E)
		else if (c == 0x1E) {
			// Skip
		}
		// Monospace: ^M (0x11)
		else if (c == 0x11) {
			// Skip
		}
		// Reverse: ^R (0x16)
		else if (c == 0x16) {
			// Skip
		}
		// Reset: ^O (0x0F)
		else if (c == 0x0F) {
			// Skip
		}
		// Hex color: ^x[RRGGBB]
		else if (c == 0x04) {
			i++; // Skip hex color code
			// Skip 6 hex digits for color
			int hex_digits = 0;
			while (i < p_text.length() && hex_digits < 6) {
				char32_t h = p_text[i];
				if ((h >= '0' && h <= '9') || (h >= 'A' && h <= 'F') || (h >= 'a' && h <= 'f')) {
					i++;
					hex_digits++;
				} else {
					break;
				}
			}
			i--; // Adjust for loop increment
		} else {
			result += c;
		}
	}

	return result;
}

Dictionary IRCClient::_parse_irc_formatting(const String &p_text) const {
	Dictionary result;
	Array segments;
	String current_text;
	Dictionary current_format;
	current_format["bold"] = false;
	current_format["italic"] = false;
	current_format["underline"] = false;
	current_format["strikethrough"] = false;
	current_format["monospace"] = false;
	current_format["reverse"] = false;
	current_format["foreground"] = "";
	current_format["background"] = "";

	for (int i = 0; i < p_text.length(); i++) {
		char32_t c = p_text[i];

		bool format_changed = false;

		// Color code
		if (c == 0x03) {
			i++;
			String fg_color;
			String bg_color;

			// Parse foreground color (up to 2 digits)
			int digits = 0;
			while (i < p_text.length() && is_digit(p_text[i]) && digits < 2) {
				fg_color += p_text[i];
				i++;
				digits++;
			}

			// Check for background color
			if (i < p_text.length() && p_text[i] == ',') {
				i++; // Skip comma
				digits = 0;
				while (i < p_text.length() && is_digit(p_text[i]) && digits < 2) {
					bg_color += p_text[i];
					i++;
					digits++;
				}
			}

			i--; // Adjust for loop increment

			if (!fg_color.is_empty()) {
				current_format["foreground"] = fg_color.to_int();
			}
			if (!bg_color.is_empty()) {
				current_format["background"] = bg_color.to_int();
			}
			format_changed = true;
		}
		// Hex color
		else if (c == 0x04) {
			i++;
			String hex_color;
			int hex_digits = 0;
			while (i < p_text.length() && hex_digits < 6) {
				char32_t h = p_text[i];
				if ((h >= '0' && h <= '9') || (h >= 'A' && h <= 'F') || (h >= 'a' && h <= 'f')) {
					hex_color += h;
					i++;
					hex_digits++;
				} else {
					break;
				}
			}
			i--;

			if (hex_color.length() == 6) {
				current_format["hex_color"] = "#" + hex_color;
			}
			format_changed = true;
		}
		// Bold
		else if (c == 0x02) {
			current_format["bold"] = !(bool)current_format["bold"];
			format_changed = true;
		}
		// Italic
		else if (c == 0x1D) {
			current_format["italic"] = !(bool)current_format["italic"];
			format_changed = true;
		}
		// Underline
		else if (c == 0x1F) {
			current_format["underline"] = !(bool)current_format["underline"];
			format_changed = true;
		}
		// Strikethrough
		else if (c == 0x1E) {
			current_format["strikethrough"] = !(bool)current_format["strikethrough"];
			format_changed = true;
		}
		// Monospace
		else if (c == 0x11) {
			current_format["monospace"] = !(bool)current_format["monospace"];
			format_changed = true;
		}
		// Reverse
		else if (c == 0x16) {
			current_format["reverse"] = !(bool)current_format["reverse"];
			format_changed = true;
		}
		// Reset
		else if (c == 0x0F) {
			current_format["bold"] = false;
			current_format["italic"] = false;
			current_format["underline"] = false;
			current_format["strikethrough"] = false;
			current_format["monospace"] = false;
			current_format["reverse"] = false;
			current_format["foreground"] = "";
			current_format["background"] = "";
			current_format["hex_color"] = "";
			format_changed = true;
		} else {
			current_text += c;
		}

		// If format changed and we have text, save current segment
		if (format_changed && !current_text.is_empty()) {
			Dictionary segment;
			segment["text"] = current_text;
			segment["format"] = current_format.duplicate();
			segments.push_back(segment);
			current_text = "";
		}
	}

	// Add final segment
	if (!current_text.is_empty()) {
		Dictionary segment;
		segment["text"] = current_text;
		segment["format"] = current_format.duplicate();
		segments.push_back(segment);
	}

	result["segments"] = segments;
	result["plain_text"] = _strip_irc_formatting(p_text);

	return result;
}

String IRCClient::_detect_encoding(const PackedByteArray &p_data) const {
	// Simple heuristic-based encoding detection
	// Check for UTF-8 validity first
	bool valid_utf8 = true;
	int i = 0;

	while (i < p_data.size()) {
		uint8_t byte = p_data[i];

		// ASCII (0x00-0x7F) - compatible with UTF-8
		if (byte <= 0x7F) {
			i++;
			continue;
		}

		// Check UTF-8 multi-byte sequences
		int seq_length = 0;
		if ((byte & 0xE0) == 0xC0) {
			seq_length = 2; // 110xxxxx
		} else if ((byte & 0xF0) == 0xE0) {
			seq_length = 3; // 1110xxxx
		} else if ((byte & 0xF8) == 0xF0) {
			seq_length = 4; // 11110xxx
		} else {
			valid_utf8 = false;
			break;
		}

		// Verify continuation bytes
		for (int j = 1; j < seq_length; j++) {
			if (i + j >= p_data.size() || (p_data[i + j] & 0xC0) != 0x80) {
				valid_utf8 = false;
				break;
			}
		}

		if (!valid_utf8) {
			break;
		}
		i += seq_length;
	}

	if (valid_utf8) {
		return "UTF-8";
	}

	// Check for common non-ASCII patterns suggesting ISO-8859-1/Windows-1252
	// These encodings allow all byte values 0x80-0xFF
	// If we have high bytes but invalid UTF-8, likely ISO-8859-1
	for (int j = 0; j < p_data.size(); j++) {
		if (p_data[j] >= 0x80) {
			return "ISO-8859-1"; // Most common fallback for IRC
		}
	}

	// Pure ASCII, UTF-8 compatible
	return "UTF-8";
}

String IRCClient::_convert_from_encoding(const PackedByteArray &p_data, const String &p_encoding) const {
	if (p_encoding == "UTF-8") {
		return String::utf8((const char *)p_data.ptr(), p_data.size());
	} else if (p_encoding == "ISO-8859-1" || p_encoding == "LATIN1") {
		// ISO-8859-1 to UTF-8 conversion
		// Each byte 0x00-0xFF maps directly to Unicode codepoint
		String result;
		for (int i = 0; i < p_data.size(); i++) {
			result += char32_t(p_data[i]);
		}
		return result;
	} else if (p_encoding == "CP1252" || p_encoding == "Windows-1252") {
		// Windows-1252 to UTF-8 conversion (similar to ISO-8859-1)
		// Bytes 0x80-0x9F have special mappings
		static const char32_t cp1252_mapping[] = {
			0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
			0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
			0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
			0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
		};

		String result;
		for (int i = 0; i < p_data.size(); i++) {
			uint8_t byte = p_data[i];
			if (byte >= 0x80 && byte <= 0x9F) {
				result += cp1252_mapping[byte - 0x80];
			} else {
				result += char32_t(byte);
			}
		}
		return result;
	} else {
		// Fallback to UTF-8
		return String::utf8((const char *)p_data.ptr(), p_data.size());
	}
}

PackedByteArray IRCClient::_convert_to_encoding(const String &p_text, const String &p_encoding) const {
	PackedByteArray result;

	if (p_encoding == "UTF-8") {
		CharString utf8 = p_text.utf8();
		result.resize(utf8.length());
		memcpy(result.ptrw(), utf8.get_data(), utf8.length());
	} else if (p_encoding == "ISO-8859-1" || p_encoding == "LATIN1") {
		// Convert Unicode to ISO-8859-1 (lossy for characters > 0xFF)
		for (int i = 0; i < p_text.length(); i++) {
			char32_t c = p_text[i];
			if (c <= 0xFF) {
				result.push_back((uint8_t)c);
			} else {
				result.push_back('?'); // Replace unmappable characters
			}
		}
	} else if (p_encoding == "CP1252" || p_encoding == "Windows-1252") {
		// Convert to Windows-1252 (best effort)
		static const char32_t cp1252_reverse[] = {
			0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6,
			0x2030, 0x0160, 0x2039, 0x0152, 0x017D, 0x2018, 0x2019, 0x201C,
			0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
			0x0153, 0x017E, 0x0178
		};

		for (int i = 0; i < p_text.length(); i++) {
			char32_t c = p_text[i];
			if (c <= 0x7F) {
				result.push_back((uint8_t)c);
			} else if (c >= 0xA0 && c <= 0xFF) {
				result.push_back((uint8_t)c);
			} else {
				// Check special mappings
				bool found = false;
				for (int j = 0; j < 27; j++) {
					if (cp1252_reverse[j] == c) {
						result.push_back((uint8_t)(0x80 + j));
						found = true;
						break;
					}
				}
				if (!found) {
					result.push_back('?'); // Replace unmappable
				}
			}
		}
	} else {
		// Default to UTF-8
		CharString utf8 = p_text.utf8();
		result.resize(utf8.length());
		memcpy(result.ptrw(), utf8.get_data(), utf8.length());
	}

	return result;
}

void IRCClient::set_encoding(const String &p_encoding) {
	encoding = p_encoding;
}

String IRCClient::get_encoding() const {
	return encoding;
}

void IRCClient::set_auto_detect_encoding(bool p_auto) {
	auto_detect_encoding = p_auto;
}

bool IRCClient::get_auto_detect_encoding() const {
	return auto_detect_encoding;
}

PackedStringArray IRCClient::get_supported_encodings() const {
	PackedStringArray encodings;
	encodings.push_back("UTF-8");
	encodings.push_back("ISO-8859-1");
	encodings.push_back("LATIN1");
	encodings.push_back("CP1252");
	encodings.push_back("Windows-1252");
	return encodings;
}

void IRCClient::set_history_enabled(bool p_enabled) {
	history_enabled = p_enabled;
}

bool IRCClient::get_history_enabled() const {
	return history_enabled;
}

void IRCClient::set_max_history_size(int p_size) {
	max_history_size = MAX(0, p_size);

	// Trim existing history if new size is smaller
	while (message_history.size() > max_history_size) {
		message_history.remove_at(0);
	}
}

int IRCClient::get_max_history_size() const {
	return max_history_size;
}

Array IRCClient::get_message_history() const {
	Array result;

	for (int i = 0; i < message_history.size(); i++) {
		Dictionary msg_dict;
		msg_dict["timestamp"] = message_history[i].timestamp;
		msg_dict["sender"] = message_history[i].sender;
		msg_dict["target"] = message_history[i].target;
		msg_dict["message"] = message_history[i].message;
		msg_dict["tags"] = message_history[i].tags;
		result.push_back(msg_dict);
	}

	return result;
}

void IRCClient::clear_message_history() {
	message_history.clear();
}

// Auto-reconnect implementation
void IRCClient::enable_auto_reconnect(bool p_enabled) {
	auto_reconnect_enabled = p_enabled;
	if (!p_enabled) {
		reconnect_attempts = 0;
		next_reconnect_time = 0;
	}
}

bool IRCClient::is_auto_reconnect_enabled() const {
	return auto_reconnect_enabled;
}

void IRCClient::set_reconnect_delay(int p_seconds) {
	reconnect_delay = MAX(1, p_seconds);
}

int IRCClient::get_reconnect_delay() const {
	return reconnect_delay;
}

void IRCClient::set_max_reconnect_attempts(int p_max) {
	max_reconnect_attempts = p_max;
}

int IRCClient::get_max_reconnect_attempts() const {
	return max_reconnect_attempts;
}

int IRCClient::get_reconnect_attempts() const {
	return reconnect_attempts;
}

void IRCClient::_save_connection_params(const String &p_host, int p_port, bool p_use_ssl, const String &p_nick, const String &p_username, const String &p_realname, const String &p_password) {
	last_host = p_host;
	last_port = p_port;
	last_use_ssl = p_use_ssl;
	last_nick = p_nick;
	last_username = p_username;
	last_realname = p_realname;
	last_password = p_password;
}

void IRCClient::_attempt_reconnect() {
	if (!auto_reconnect_enabled || last_host.is_empty()) {
		return;
	}

	if (max_reconnect_attempts >= 0 && reconnect_attempts >= max_reconnect_attempts) {
		return; // Max attempts reached
	}

	uint64_t current_time = Time::get_singleton()->get_ticks_msec();
	if (current_time < next_reconnect_time) {
		return; // Not time yet
	}

	reconnect_attempts++;

	// Exponential backoff: delay * 2^(attempts-1), capped at 5 minutes
	int actual_delay = MIN(reconnect_delay * (1 << (reconnect_attempts - 1)), 300);
	next_reconnect_time = current_time + (actual_delay * 1000);

	// Try fallback servers if available
	String host = last_host;
	int port = last_port;
	bool use_ssl = last_use_ssl;

	if (fallback_servers.size() > 0) {
		// Try fallback servers in order
		if (reconnect_attempts > 1 && current_server_index < fallback_servers.size()) {
			const FallbackServer &fallback = fallback_servers[current_server_index];
			host = fallback.host;
			port = fallback.port;
			use_ssl = fallback.use_ssl;
			current_server_index++;

			// Reset to primary server after trying all fallbacks
			if (current_server_index >= fallback_servers.size()) {
				current_server_index = 0;
			}
		}
	}

	// Try to reconnect
	Error err = connect_to_server(host, port, use_ssl, last_nick, last_username, last_realname, last_password);

	if (err == OK) {
		reconnect_attempts = 0; // Reset on successful connection
		current_server_index = 0; // Reset to primary server
	}
}

void IRCClient::_perform_autojoin() {
	if (!autojoin_enabled || autojoin_channels.size() == 0) {
		return;
	}

	for (int i = 0; i < autojoin_channels.size(); i++) {
		const AutoJoinChannel &ajc = autojoin_channels[i];
		join_channel(ajc.channel, ajc.key);
	}
}

// Nickname alternatives implementation
void IRCClient::set_alternative_nicks(const PackedStringArray &p_nicks) {
	alternative_nicks = p_nicks;
	current_nick_index = 0;
}

void IRCClient::add_alternative_nick(const String &p_nick) {
	if (!alternative_nicks.has(p_nick)) {
		alternative_nicks.push_back(p_nick);
	}
}

void IRCClient::clear_alternative_nicks() {
	alternative_nicks.clear();
	current_nick_index = 0;
}

PackedStringArray IRCClient::get_alternative_nicks() const {
	return alternative_nicks;
}

// Auto-join channels implementation
void IRCClient::add_autojoin_channel(const String &p_channel, const String &p_key) {
	// Check if already in list
	for (int i = 0; i < autojoin_channels.size(); i++) {
		if (autojoin_channels[i].channel == p_channel) {
			autojoin_channels.write[i].key = p_key; // Update key
			return;
		}
	}

	AutoJoinChannel ajc;
	ajc.channel = p_channel;
	ajc.key = p_key;
	autojoin_channels.push_back(ajc);
}

void IRCClient::remove_autojoin_channel(const String &p_channel) {
	for (int i = 0; i < autojoin_channels.size(); i++) {
		if (autojoin_channels[i].channel == p_channel) {
			autojoin_channels.remove_at(i);
			return;
		}
	}
}

void IRCClient::clear_autojoin_channels() {
	autojoin_channels.clear();
}

PackedStringArray IRCClient::get_autojoin_channels() const {
	PackedStringArray result;
	for (int i = 0; i < autojoin_channels.size(); i++) {
		result.push_back(autojoin_channels[i].channel);
	}
	return result;
}

void IRCClient::enable_autojoin(bool p_enabled) {
	autojoin_enabled = p_enabled;
}

bool IRCClient::is_autojoin_enabled() const {
	return autojoin_enabled;
}

// Ignore list implementation
void IRCClient::ignore_user(const String &p_mask) {
	if (!ignored_users.has(p_mask)) {
		ignored_users.push_back(p_mask);
	}
}

void IRCClient::unignore_user(const String &p_mask) {
	for (int i = 0; i < ignored_users.size(); i++) {
		if (ignored_users[i] == p_mask) {
			ignored_users.remove_at(i);
			return;
		}
	}
}

void IRCClient::clear_ignores() {
	ignored_users.clear();
}

PackedStringArray IRCClient::get_ignored_users() const {
	return ignored_users;
}

bool IRCClient::is_ignored(const String &p_nick) const {
	return _is_ignored(p_nick);
}

bool IRCClient::_is_ignored(const String &p_nick_or_mask) const {
	for (int i = 0; i < ignored_users.size(); i++) {
		if (_mask_matches(ignored_users[i], p_nick_or_mask)) {
			return true;
		}
	}
	return false;
}

bool IRCClient::_mask_matches(const String &p_mask, const String &p_nick) const {
	// Simple wildcard matching for IRC masks
	// Supports * (any) and ? (single char)
	// Format: nick!user@host or just nick

	if (p_mask == p_nick) {
		return true; // Exact match
	}

	// Simple wildcard implementation
	if (p_mask.contains("*")) {
		String pattern = p_mask.replace("*", ".*").replace("?", ".");
		RegEx regex;
		regex.compile("^" + pattern + "$");
		Ref<RegExMatch> match = regex.search(p_nick);
		return match.is_valid();
	}

	return p_mask == p_nick;
}

// Fallback servers implementation
void IRCClient::add_fallback_server(const String &p_host, int p_port, bool p_use_ssl) {
	FallbackServer server;
	server.host = p_host;
	server.port = p_port;
	server.use_ssl = p_use_ssl;
	fallback_servers.push_back(server);
}

void IRCClient::clear_fallback_servers() {
	fallback_servers.clear();
	current_server_index = 0;
}

int IRCClient::get_fallback_server_count() const {
	return fallback_servers.size();
}

// NickServ/Services helpers implementation
void IRCClient::identify_nickserv(const String &p_password) {
	send_privmsg("NickServ", "IDENTIFY " + p_password);
}

void IRCClient::ghost_nick(const String &p_nick, const String &p_password) {
	send_privmsg("NickServ", "GHOST " + p_nick + " " + p_password);
}

void IRCClient::register_nick(const String &p_email, const String &p_password) {
	send_privmsg("NickServ", "REGISTER " + p_password + " " + p_email);
}

void IRCClient::group_nick(const String &p_password) {
	send_privmsg("NickServ", "GROUP " + p_password);
}

void IRCClient::register_channel(const String &p_channel) {
	send_privmsg("ChanServ", "REGISTER " + p_channel);
}

void IRCClient::identify_chanserv(const String &p_channel, const String &p_password) {
	send_privmsg("ChanServ", "IDENTIFY " + p_channel + " " + p_password);
}

// Channel LIST with filters
void IRCClient::list_channels(const String &p_pattern, int p_min_users, int p_max_users) {
	String list_cmd = "LIST";

	if (!p_pattern.is_empty()) {
		list_cmd += " " + p_pattern;
	}

	// ELIST support (if server supports it)
	String elist_params;
	if (p_min_users > 0) {
		elist_params += ">=" + String::num_int64(p_min_users);
	}
	if (p_max_users >= 0) {
		if (!elist_params.is_empty()) {
			elist_params += ",";
		}
		elist_params += "<=" + String::num_int64(p_max_users);
	}

	if (!elist_params.is_empty()) {
		list_cmd += " " + elist_params;
	}

	send_raw(list_cmd);
}

// IRCv3 Chathistory implementation
void IRCClient::request_chathistory(const String &p_target, const String &p_timestamp_start, const String &p_timestamp_end, int p_limit) {
	String cmd = vformat("CHATHISTORY BETWEEN %s timestamp=%s timestamp=%s %d",
			p_target, p_timestamp_start, p_timestamp_end, p_limit);
	send_raw(cmd);
}

void IRCClient::request_chathistory_before(const String &p_target, const String &p_msgid, int p_limit) {
	String cmd = vformat("CHATHISTORY BEFORE %s msgid=%s %d", p_target, p_msgid, p_limit);
	send_raw(cmd);
}

void IRCClient::request_chathistory_after(const String &p_target, const String &p_msgid, int p_limit) {
	String cmd = vformat("CHATHISTORY AFTER %s msgid=%s %d", p_target, p_msgid, p_limit);
	send_raw(cmd);
}

void IRCClient::request_chathistory_latest(const String &p_target, int p_limit) {
	String cmd = vformat("CHATHISTORY LATEST %s * %d", p_target, p_limit);
	send_raw(cmd);
}

// IRCv3 Multiline messages
void IRCClient::send_multiline_privmsg(const String &p_target, const PackedStringArray &p_lines) {
	// Multiline uses draft/multiline capability with concat tag
	// For now, send as separate messages if multiline not supported
	for (int i = 0; i < p_lines.size(); i++) {
		send_privmsg(p_target, p_lines[i]);
	}
}

void IRCClient::send_multiline_notice(const String &p_target, const PackedStringArray &p_lines) {
	// Multiline uses draft/multiline capability with concat tag
	// For now, send as separate messages if multiline not supported
	for (int i = 0; i < p_lines.size(); i++) {
		send_notice(p_target, p_lines[i]);
	}
}

// Channel operator helpers implementation
void IRCClient::op_user(const String &p_channel, const String &p_nick) {
	PackedStringArray params;
	params.push_back(p_nick);
	set_mode(p_channel, "+o", params);
}

void IRCClient::deop_user(const String &p_channel, const String &p_nick) {
	PackedStringArray params;
	params.push_back(p_nick);
	set_mode(p_channel, "-o", params);
}

void IRCClient::voice_user(const String &p_channel, const String &p_nick) {
	PackedStringArray params;
	params.push_back(p_nick);
	set_mode(p_channel, "+v", params);
}

void IRCClient::devoice_user(const String &p_channel, const String &p_nick) {
	PackedStringArray params;
	params.push_back(p_nick);
	set_mode(p_channel, "-v", params);
}

void IRCClient::kick_user(const String &p_channel, const String &p_nick, const String &p_reason) {
	String command = "KICK " + p_channel + " " + p_nick;
	if (!p_reason.is_empty()) {
		command += " :" + p_reason;
	}
	_send_with_priority(command, 0);
}

void IRCClient::ban_user(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "+b", params);
}

void IRCClient::unban_user(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "-b", params);
}

void IRCClient::kickban_user(const String &p_channel, const String &p_nick, const String &p_reason) {
	// First ban (by nick!*@*)
	String ban_mask = p_nick + "!*@*";
	ban_user(p_channel, ban_mask);

	// Then kick
	kick_user(p_channel, p_nick, p_reason);
}

// Nick completion helpers
PackedStringArray IRCClient::get_matching_nicks(const String &p_channel, const String &p_prefix) const {
	PackedStringArray matches;

	if (!channels.has(p_channel)) {
		return matches;
	}

	Ref<IRCChannel> channel = channels[p_channel];
	PackedStringArray nicks = channel->get_users();

	String prefix_lower = p_prefix.to_lower();

	for (int i = 0; i < nicks.size(); i++) {
		if (nicks[i].to_lower().begins_with(prefix_lower)) {
			matches.push_back(nicks[i]);
		}
	}

	return matches;
}

String IRCClient::complete_nick(const String &p_channel, const String &p_partial, int p_cycle) const {
	PackedStringArray matches = get_matching_nicks(p_channel, p_partial);

	if (matches.size() == 0) {
		return p_partial;
	}

	int index = p_cycle % matches.size();
	return matches[index];
}

// Highlight/Mention detection
void IRCClient::add_highlight_pattern(const String &p_pattern) {
	if (!highlight_patterns.has(p_pattern)) {
		highlight_patterns.push_back(p_pattern);
	}

	// Always highlight own nick
	if (!highlight_patterns.has(current_nick)) {
		highlight_patterns.push_back(current_nick);
	}
}

void IRCClient::remove_highlight_pattern(const String &p_pattern) {
	for (int i = 0; i < highlight_patterns.size(); i++) {
		if (highlight_patterns[i] == p_pattern) {
			highlight_patterns.remove_at(i);
			return;
		}
	}
}

void IRCClient::clear_highlight_patterns() {
	highlight_patterns.clear();
}

PackedStringArray IRCClient::get_highlight_patterns() const {
	return highlight_patterns;
}

bool IRCClient::is_highlighted(const String &p_message, const String &p_nick) const {
	String check_nick = p_nick.is_empty() ? current_nick : p_nick;

	// Check if message contains current nick
	if (p_message.findn(check_nick) != -1) {
		return true;
	}

	// Check custom highlight patterns
	for (int i = 0; i < highlight_patterns.size(); i++) {
		if (p_message.findn(highlight_patterns[i]) != -1) {
			return true;
		}
	}

	return false;
}

// URL extraction
PackedStringArray IRCClient::extract_urls(const String &p_message) const {
	PackedStringArray urls;

	// Simple URL regex pattern
	RegEx regex;
	regex.compile("(https?://[^\\s]+)|(www\\.[^\\s]+)");

	TypedArray<RegExMatch> matches = regex.search_all(p_message);

	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			urls.push_back(match->get_string(0)); // Get full match (group 0)
		}
	}

	return urls;
}

// Message ID tracking
String IRCClient::get_message_text_by_id(const String &p_message_id) const {
	if (message_id_to_text.has(p_message_id)) {
		return message_id_to_text[p_message_id];
	}
	return "";
}

void IRCClient::set_max_tracked_messages(int p_max) {
	max_tracked_messages = MAX(0, p_max);
}

int IRCClient::get_max_tracked_messages() const {
	return max_tracked_messages;
}

// Connection metrics
Dictionary IRCClient::get_connection_stats() const {
	Dictionary stats;

	uint64_t current_time = Time::get_singleton()->get_ticks_msec();
	uint64_t uptime = (status == STATUS_CONNECTED && metrics.connection_time > 0)
			? (current_time - metrics.connection_time) / 1000
			: 0;

	stats["uptime"] = uptime;
	stats["messages_sent"] = metrics.messages_sent;
	stats["messages_received"] = metrics.messages_received;
	stats["bytes_sent"] = metrics.bytes_sent;
	stats["bytes_received"] = metrics.bytes_received;
	stats["average_latency"] = get_average_latency();

	return stats;
}

void IRCClient::reset_connection_stats() {
	metrics.messages_sent = 0;
	metrics.messages_received = 0;
	metrics.bytes_sent = 0;
	metrics.bytes_received = 0;
	metrics.ping_count = 0;
	metrics.total_latency_ms = 0;
	metrics.connection_time = Time::get_singleton()->get_ticks_msec();
}

int IRCClient::get_average_latency() const {
	if (metrics.ping_count == 0) {
		return 0;
	}
	return metrics.total_latency_ms / metrics.ping_count;
}

// IRC Commands (Low Priority) implementation
void IRCClient::send_oper(const String &p_username, const String &p_password) {
	send_raw("OPER " + p_username + " " + p_password);
}

void IRCClient::knock_channel(const String &p_channel, const String &p_message) {
	if (p_message.is_empty()) {
		send_raw("KNOCK " + p_channel);
	} else {
		send_raw("KNOCK " + p_channel + " :" + p_message);
	}
}

void IRCClient::silence_user(const String &p_mask) {
	send_raw("SILENCE +" + p_mask);
}

void IRCClient::unsilence_user(const String &p_mask) {
	send_raw("SILENCE -" + p_mask);
}

void IRCClient::list_silence() {
	send_raw("SILENCE");
}

void IRCClient::who_channel(const String &p_channel) {
	send_raw("WHO " + p_channel);
}

void IRCClient::who_user(const String &p_mask) {
	send_raw("WHO " + p_mask);
}

void IRCClient::whox(const String &p_mask, const String &p_fields, int p_querytype) {
	// WHOX query with custom fields
	// Fields: t=type, c=channel, u=user, i=ip, h=host, s=server, n=nick, f=flags, d=hopcount, l=idle, a=account, o=oplevel, r=realname
	String query = "WHO " + p_mask;
	if (!p_fields.is_empty()) {
		query += " %" + p_fields;
		if (p_querytype > 0) {
			query += "," + String::num_int64(p_querytype);
		}
	}
	send_raw(query);
}

void IRCClient::whowas_user(const String &p_nick, int p_count) {
	send_raw("WHOWAS " + p_nick + " " + String::num_int64(p_count));
}

void IRCClient::invite_user(const String &p_channel, const String &p_nick) {
	send_raw("INVITE " + p_nick + " " + p_channel);
}

void IRCClient::userhost(const String &p_nick) {
	send_raw("USERHOST " + p_nick);
}

void IRCClient::register_account(const String &p_account, const String &p_password, const String &p_email) {
	// IRCv3 Account Registration (draft)
	// REGISTER <account> <password> [email]
	String cmd = "REGISTER " + p_account + " " + p_password;
	if (!p_email.is_empty()) {
		cmd += " " + p_email;
	}
	send_raw(cmd);
}

void IRCClient::verify_account(const String &p_account, const String &p_code) {
	// IRCv3 Account Registration verification
	// VERIFY <account> <code>
	send_raw("VERIFY " + p_account + " " + p_code);
}

// Channel Management (Low Priority) implementation
void IRCClient::set_channel_key(const String &p_channel, const String &p_key) {
	channel_keys[p_channel] = p_key;
}

String IRCClient::get_channel_key(const String &p_channel) const {
	if (channel_keys.has(p_channel)) {
		return channel_keys[p_channel];
	}
	return "";
}

void IRCClient::clear_channel_key(const String &p_channel) {
	channel_keys.erase(p_channel);
}

void IRCClient::request_ban_list(const String &p_channel) {
	send_raw("MODE " + p_channel + " +b");
}

void IRCClient::set_ban(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "+b", params);
}

void IRCClient::remove_ban(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "-b", params);
}

void IRCClient::request_exception_list(const String &p_channel) {
	send_raw("MODE " + p_channel + " +e");
}

void IRCClient::set_exception(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "+e", params);
}

void IRCClient::request_invite_list(const String &p_channel) {
	send_raw("MODE " + p_channel + " +I");
}

void IRCClient::set_invite_exception(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "+I", params);
}

void IRCClient::quiet_user(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "+q", params);
}

void IRCClient::unquiet_user(const String &p_channel, const String &p_mask) {
	PackedStringArray params;
	params.push_back(p_mask);
	set_mode(p_channel, "-q", params);
}

void IRCClient::request_quiet_list(const String &p_channel) {
	send_raw("MODE " + p_channel + " +q");
}

// Away Status (Low Priority) implementation
void IRCClient::set_away(const String &p_message) {
	is_away_status = true;
	away_message = p_message;
	send_raw("AWAY :" + p_message);
}

void IRCClient::set_back() {
	is_away_status = false;
	away_message = "";
	send_raw("AWAY");
}

bool IRCClient::get_is_away() const {
	return is_away_status;
}

String IRCClient::get_away_message() const {
	return away_message;
}

// User Tracking (Low Priority) implementation
Ref<IRCUser> IRCClient::get_user(const String &p_nick) const {
	if (global_users.has(p_nick)) {
		return global_users[p_nick];
	}

	// Check in channel-specific users
	if (users.has(p_nick)) {
		return users[p_nick];
	}

	return Ref<IRCUser>();
}

PackedStringArray IRCClient::get_common_channels(const String &p_nick) const {
	PackedStringArray common;

	// Iterate over HashMap using const iterator
	for (HashMap<String, Ref<IRCChannel>>::ConstIterator it = channels.begin(); it != channels.end(); ++it) {
		const String &channel_name = it->key;
		const Ref<IRCChannel> &channel = it->value;

		if (channel.is_valid() && channel->has_user(p_nick)) {
			common.push_back(channel_name);
		}
	}

	return common;
}

Dictionary IRCClient::get_user_info(const String &p_nick) const {
	Dictionary info;

	Ref<IRCUser> user = get_user(p_nick);
	if (user.is_valid()) {
		info["nick"] = user->get_nick();
		info["username"] = user->get_username();
		info["hostname"] = user->get_hostname();
		info["realname"] = user->get_realname();
		info["account"] = user->get_account();
	}

	info["common_channels"] = get_common_channels(p_nick);

	return info;
}

// IRCv3 Extensions (Low Priority) implementation
void IRCClient::send_read_marker(const String &p_channel, const String &p_timestamp) {
	// draft/read-marker capability
	read_markers[p_channel] = p_timestamp;
	send_raw("MARKREAD " + p_channel + " timestamp=" + p_timestamp);
}

void IRCClient::send_typing_notification(const String &p_channel, bool p_typing) {
	// typing capability - sends TAGMSG with typing tag
	String typing_value = p_typing ? "active" : "done";
	send_raw("@typing=" + typing_value + " TAGMSG " + p_channel);
}

void IRCClient::send_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction) {
	// draft/react capability - sends TAGMSG with +react tag
	send_raw("@+react=" + p_msgid + ";" + p_reaction + " TAGMSG " + p_channel);
}

void IRCClient::remove_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction) {
	// draft/react capability - sends TAGMSG with -react tag
	send_raw("@-react=" + p_msgid + ";" + p_reaction + " TAGMSG " + p_channel);
}

void IRCClient::set_bot_mode(bool p_enabled) {
	// IRCv3 Bot Mode - MODE <nick> +/-B
	bot_mode_enabled = p_enabled;
	if (status == STATUS_CONNECTED) {
		if (p_enabled) {
			send_raw("MODE " + current_nick + " +B");
		} else {
			send_raw("MODE " + current_nick + " -B");
		}
	}
}

bool IRCClient::get_bot_mode() const {
	return bot_mode_enabled;
}

void IRCClient::send_reply(const String &p_target, const String &p_message, const String &p_reply_to_msgid) {
	// IRCv3 draft/reply - send PRIVMSG with +draft/reply client tag
	// Client tags are sent with @ prefix before the command
	String tagged_msg = "@+draft/reply=" + p_reply_to_msgid + " PRIVMSG " + p_target + " :" + p_message;
	send_raw(tagged_msg);
}

void IRCClient::send_reply_notice(const String &p_target, const String &p_message, const String &p_reply_to_msgid) {
	// IRCv3 draft/reply - send NOTICE with +draft/reply client tag
	String tagged_msg = "@+draft/reply=" + p_reply_to_msgid + " NOTICE " + p_target + " :" + p_message;
	send_raw(tagged_msg);
}

String IRCClient::get_reply_to_msgid(const Dictionary &p_tags) const {
	// Extract reply msgid from tags (draft/reply or +draft/reply)
	if (p_tags.has("draft/reply")) {
		return p_tags["draft/reply"];
	}
	if (p_tags.has("+draft/reply")) {
		return p_tags["+draft/reply"];
	}
	return "";
}

bool IRCClient::has_sts_policy(const String &p_hostname) const {
	if (!sts_policies.has(p_hostname)) {
		return false;
	}

	// Check if policy has expired
	const STSPolicy &policy = sts_policies[p_hostname];
	uint64_t current_time = Time::get_singleton()->get_unix_time_from_system();
	return current_time < policy.expiry_time;
}

void IRCClient::clear_sts_policy(const String &p_hostname) {
	sts_policies.erase(p_hostname);
}

void IRCClient::clear_all_sts_policies() {
	sts_policies.clear();
}

void IRCClient::_handle_sts_capability(const String &p_sts_value) {
	// Parse STS capability value: sts=port,duration[,preload]
	// Example: sts=6697,2592000 or sts=6697,2592000,preload

	PackedStringArray parts = p_sts_value.split(",");
	if (parts.size() < 2) {
		return; // Invalid STS format
	}

	STSPolicy policy;
	policy.port = parts[0].to_int();
	policy.duration = parts[1].to_int();
	policy.preload = parts.size() >= 3 && parts[2] == "preload";

	uint64_t current_time = Time::get_singleton()->get_unix_time_from_system();
	policy.expiry_time = current_time + policy.duration;

	// Store policy for current hostname
	if (!last_host.is_empty()) {
		sts_policies[last_host] = policy;
	}
}

IRCClient::IRCClient() {
	messages_per_second = DEFAULT_MESSAGES_PER_SECOND;
	ping_timeout = DEFAULT_PING_TIMEOUT;
	token_bucket_size = 5;
	token_bucket_tokens = 5;
	last_token_refill = 0;
	encoding = "UTF-8";
	auto_detect_encoding = true;
	history_enabled = false;
	max_history_size = 1000;
}

IRCClient::~IRCClient() {
	disconnect_from_server();
}
