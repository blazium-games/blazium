/**************************************************************************/
/*  irc_client_node.h                                                     */
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

#include "irc_client.h"

#include "scene/main/node.h"

class IRCClientNode : public Node {
	GDCLASS(IRCClientNode, Node);

private:
	Ref<IRCClient> client;

	// Reconnection info
	String last_server;
	int last_port = 6667;
	bool last_use_ssl = false;
	String last_nick;
	String last_username;
	String last_realname;
	String last_password;

	void _on_disconnected(const String &p_reason);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	// Get the underlying client
	Ref<IRCClient> get_client();

	// Forwarded connection methods
	Error connect_to_server(const String &p_host, int p_port, bool p_use_ssl, const String &p_nick, const String &p_username, const String &p_realname, const String &p_password = "");
	void disconnect_from_server(const String &p_quit_message = "");
	bool is_irc_connected() const;
	IRCClient::Status get_status() const;

	// Forwarded message sending methods
	void send_raw(const String &p_message);
	void send_privmsg(const String &p_target, const String &p_message);
	void send_notice(const String &p_target, const String &p_message);
	void send_action(const String &p_target, const String &p_action);

	// Forwarded channel operations
	void join_channel(const String &p_channel, const String &p_key = "");
	void part_channel(const String &p_channel, const String &p_message = "");
	void set_topic(const String &p_channel, const String &p_topic);

	// Forwarded user operations
	void set_nick(const String &p_new_nick);
	void set_mode(const String &p_target, const String &p_modes, const PackedStringArray &p_params = PackedStringArray());
	void send_whois(const String &p_nick);
	void set_realname(const String &p_realname);

	// MONITOR commands (IRCv3.3)
	void monitor_add(const String &p_nick);
	void monitor_remove(const String &p_nick);
	void monitor_clear();
	void monitor_list();
	void monitor_status();
	PackedStringArray get_monitored_nicks() const;

	// Forwarded IRCv3 methods
	void request_capability(const String &p_capability);
	PackedStringArray get_available_capabilities() const;
	PackedStringArray get_enabled_capabilities() const;

	// Forwarded SASL methods
	void enable_sasl(const String &p_username, const String &p_password);
	void enable_sasl_plain(const String &p_username, const String &p_password);
	void enable_sasl_external();
	void enable_sasl_scram_sha256(const String &p_username, const String &p_password);
	void disable_sasl();

	// Fallback servers
	void add_fallback_server(const String &p_host, int p_port, bool p_use_ssl = true);
	void clear_fallback_servers();
	int get_fallback_server_count() const;

	// NickServ/Services helpers
	void identify_nickserv(const String &p_password);
	void ghost_nick(const String &p_nick, const String &p_password);
	void register_nick(const String &p_email, const String &p_password);
	void group_nick(const String &p_password);
	void register_channel(const String &p_channel);
	void identify_chanserv(const String &p_channel, const String &p_password);

	// Channel LIST with filters
	void list_channels(const String &p_pattern = "", int p_min_users = 0, int p_max_users = -1);

	// IRCv3 Chathistory
	void request_chathistory(const String &p_target, const String &p_timestamp_start, const String &p_timestamp_end, int p_limit = 100);
	void request_chathistory_before(const String &p_target, const String &p_msgid, int p_limit = 100);
	void request_chathistory_after(const String &p_target, const String &p_msgid, int p_limit = 100);
	void request_chathistory_latest(const String &p_target, int p_limit = 100);

	// IRCv3 Multiline
	void send_multiline_privmsg(const String &p_target, const PackedStringArray &p_lines);
	void send_multiline_notice(const String &p_target, const PackedStringArray &p_lines);

	// Nick completion helpers
	PackedStringArray get_matching_nicks(const String &p_channel, const String &p_prefix) const;
	String complete_nick(const String &p_channel, const String &p_partial, int p_cycle = 0) const;

	// Highlight/Mention detection
	void add_highlight_pattern(const String &p_pattern);
	void remove_highlight_pattern(const String &p_pattern);
	void clear_highlight_patterns();
	PackedStringArray get_highlight_patterns() const;
	bool is_highlighted(const String &p_message, const String &p_nick = "") const;

	// URL extraction
	PackedStringArray extract_urls(const String &p_message) const;

	// Message ID tracking
	String get_message_text_by_id(const String &p_message_id) const;
	void set_max_tracked_messages(int p_max);
	int get_max_tracked_messages() const;

	// Connection metrics
	Dictionary get_connection_stats() const;
	void reset_connection_stats();
	int get_average_latency() const;

	// IRC Commands (Low Priority)
	void send_oper(const String &p_username, const String &p_password);
	void knock_channel(const String &p_channel, const String &p_message = "");
	void silence_user(const String &p_mask);
	void unsilence_user(const String &p_mask);
	void list_silence();
	void who_channel(const String &p_channel);
	void who_user(const String &p_mask);
	void whox(const String &p_mask, const String &p_fields = "tcuihsnfdlar", int p_querytype = 0);
	void whowas_user(const String &p_nick, int p_count = 1);
	void invite_user(const String &p_channel, const String &p_nick);
	void userhost(const String &p_nick);
	void register_account(const String &p_account, const String &p_password, const String &p_email = "");
	void verify_account(const String &p_account, const String &p_code);

	// Channel Management (Low Priority)
	void set_channel_key(const String &p_channel, const String &p_key);
	String get_channel_key(const String &p_channel) const;
	void clear_channel_key(const String &p_channel);
	void request_ban_list(const String &p_channel);
	void set_ban(const String &p_channel, const String &p_mask);
	void remove_ban(const String &p_channel, const String &p_mask);
	void request_exception_list(const String &p_channel);
	void set_exception(const String &p_channel, const String &p_mask);
	void request_invite_list(const String &p_channel);
	void set_invite_exception(const String &p_channel, const String &p_mask);
	void quiet_user(const String &p_channel, const String &p_mask);
	void unquiet_user(const String &p_channel, const String &p_mask);
	void request_quiet_list(const String &p_channel);

	// Away Status (Low Priority)
	void set_away(const String &p_message);
	void set_back();
	bool get_is_away() const;
	String get_away_message() const;

	// User Tracking (Low Priority)
	Ref<IRCUser> get_user(const String &p_nick) const;
	PackedStringArray get_common_channels(const String &p_nick) const;
	Dictionary get_user_info(const String &p_nick) const;

	// Bot Mode (IRCv3)
	void set_bot_mode(bool p_enabled);
	bool get_bot_mode() const;

	// Reply Threading (IRCv3 draft/reply)
	void send_reply(const String &p_target, const String &p_message, const String &p_reply_to_msgid);
	void send_reply_notice(const String &p_target, const String &p_message, const String &p_reply_to_msgid);
	String get_reply_to_msgid(const Dictionary &p_tags) const;

	// STS (Strict Transport Security) - IRCv3
	bool has_sts_policy(const String &p_hostname) const;
	void clear_sts_policy(const String &p_hostname);
	void clear_all_sts_policies();

	// IRCv3 Extensions (Low Priority)
	void send_read_marker(const String &p_channel, const String &p_timestamp);
	void send_typing_notification(const String &p_channel, bool p_typing);
	void send_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction);
	void remove_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction);

	// Forwarded DCC methods
	Error send_dcc_file(const String &p_nick, const String &p_file_path);
	void accept_dcc_transfer(int p_transfer_index);
	void reject_dcc_transfer(int p_transfer_index);
	void cancel_dcc_transfer(int p_transfer_index);
	TypedArray<IRCDCCTransfer> get_active_transfers() const;

	// Forwarded state queries
	Ref<IRCChannel> get_channel(const String &p_channel) const;
	PackedStringArray get_joined_channels() const;
	String get_current_nick() const;

	// Auto-reconnect
	// Forwarded configuration
	void set_messages_per_second(int p_rate);
	int get_messages_per_second() const;

	void set_ping_timeout(int p_timeout_ms);
	int get_ping_timeout() const;

	// DCC Configuration
	void set_dcc_local_ip(const IPAddress &p_ip);
	IPAddress get_dcc_local_ip() const;
	void clear_dcc_local_ip();

	// Flood protection
	void set_token_bucket_size(int p_size);
	int get_token_bucket_size() const;

#ifdef MODULE_MBEDTLS_ENABLED
	void set_tls_options(const Ref<TLSOptions> &p_options);
	Ref<TLSOptions> get_tls_options() const;
#endif

	// Formatting utilities
	String strip_formatting(const String &p_text);
	Dictionary parse_formatting(const String &p_text);

	// Encoding configuration
	void set_encoding(const String &p_encoding);
	String get_encoding() const;
	void set_auto_detect_encoding(bool p_auto);
	bool get_auto_detect_encoding() const;
	PackedStringArray get_supported_encodings() const;

	// Message history
	void set_history_enabled(bool p_enabled);
	bool get_history_enabled() const;
	void set_max_history_size(int p_size);
	int get_max_history_size() const;
	Array get_message_history() const;
	void clear_message_history();

	// Auto-reconnect
	void enable_auto_reconnect(bool p_enabled);
	bool is_auto_reconnect_enabled() const;
	void set_reconnect_delay(int p_seconds);
	int get_reconnect_delay() const;
	void set_max_reconnect_attempts(int p_max);
	int get_max_reconnect_attempts() const;
	int get_reconnect_attempts() const;

	// Nickname alternatives
	void set_alternative_nicks(const PackedStringArray &p_nicks);
	void add_alternative_nick(const String &p_nick);
	void clear_alternative_nicks();
	PackedStringArray get_alternative_nicks() const;

	// Auto-join channels
	void add_autojoin_channel(const String &p_channel, const String &p_key = "");
	void remove_autojoin_channel(const String &p_channel);
	void clear_autojoin_channels();
	PackedStringArray get_autojoin_channels() const;
	void enable_autojoin(bool p_enabled);
	bool is_autojoin_enabled() const;

	// Ignore list (client-side)
	void ignore_user(const String &p_mask);
	void unignore_user(const String &p_mask);
	void clear_ignores();
	PackedStringArray get_ignored_users() const;
	bool is_ignored(const String &p_nick) const;

	// Channel operator helpers
	void op_user(const String &p_channel, const String &p_nick);
	void deop_user(const String &p_channel, const String &p_nick);
	void voice_user(const String &p_channel, const String &p_nick);
	void devoice_user(const String &p_channel, const String &p_nick);
	void kick_user(const String &p_channel, const String &p_nick, const String &p_reason = "");
	void ban_user(const String &p_channel, const String &p_mask);
	void unban_user(const String &p_channel, const String &p_mask);
	void kickban_user(const String &p_channel, const String &p_nick, const String &p_reason = "");

	IRCClientNode();
	~IRCClientNode();
};
