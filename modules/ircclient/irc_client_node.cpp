/**************************************************************************/
/*  irc_client_node.cpp                                                   */
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

#include "irc_client_node.h"

void IRCClientNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_client"), &IRCClientNode::get_client);

	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port", "use_ssl", "nick", "username", "realname", "password"), &IRCClientNode::connect_to_server, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("disconnect_from_server", "quit_message"), &IRCClientNode::disconnect_from_server, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_irc_connected"), &IRCClientNode::is_irc_connected);
	ClassDB::bind_method(D_METHOD("get_status"), &IRCClientNode::get_status);

	ClassDB::bind_method(D_METHOD("send_raw", "message"), &IRCClientNode::send_raw);
	ClassDB::bind_method(D_METHOD("send_privmsg", "target", "message"), &IRCClientNode::send_privmsg);
	ClassDB::bind_method(D_METHOD("send_notice", "target", "message"), &IRCClientNode::send_notice);
	ClassDB::bind_method(D_METHOD("send_action", "target", "action"), &IRCClientNode::send_action);

	ClassDB::bind_method(D_METHOD("join_channel", "channel", "key"), &IRCClientNode::join_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("part_channel", "channel", "message"), &IRCClientNode::part_channel, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("set_topic", "channel", "topic"), &IRCClientNode::set_topic);

	ClassDB::bind_method(D_METHOD("set_nick", "new_nick"), &IRCClientNode::set_nick);
	ClassDB::bind_method(D_METHOD("set_mode", "target", "modes", "params"), &IRCClientNode::set_mode, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("send_whois", "nick"), &IRCClientNode::send_whois);
	ClassDB::bind_method(D_METHOD("set_realname", "realname"), &IRCClientNode::set_realname);

	ClassDB::bind_method(D_METHOD("monitor_add", "nick"), &IRCClientNode::monitor_add);
	ClassDB::bind_method(D_METHOD("monitor_remove", "nick"), &IRCClientNode::monitor_remove);
	ClassDB::bind_method(D_METHOD("monitor_clear"), &IRCClientNode::monitor_clear);
	ClassDB::bind_method(D_METHOD("monitor_list"), &IRCClientNode::monitor_list);
	ClassDB::bind_method(D_METHOD("monitor_status"), &IRCClientNode::monitor_status);
	ClassDB::bind_method(D_METHOD("get_monitored_nicks"), &IRCClientNode::get_monitored_nicks);

	ClassDB::bind_method(D_METHOD("request_capability", "capability"), &IRCClientNode::request_capability);
	ClassDB::bind_method(D_METHOD("get_available_capabilities"), &IRCClientNode::get_available_capabilities);
	ClassDB::bind_method(D_METHOD("get_enabled_capabilities"), &IRCClientNode::get_enabled_capabilities);

	ClassDB::bind_method(D_METHOD("enable_sasl", "username", "password"), &IRCClientNode::enable_sasl);
	ClassDB::bind_method(D_METHOD("disable_sasl"), &IRCClientNode::disable_sasl);

	ClassDB::bind_method(D_METHOD("send_dcc_file", "nick", "file_path"), &IRCClientNode::send_dcc_file);
	ClassDB::bind_method(D_METHOD("accept_dcc_transfer", "transfer_index"), &IRCClientNode::accept_dcc_transfer);
	ClassDB::bind_method(D_METHOD("reject_dcc_transfer", "transfer_index"), &IRCClientNode::reject_dcc_transfer);
	ClassDB::bind_method(D_METHOD("cancel_dcc_transfer", "transfer_index"), &IRCClientNode::cancel_dcc_transfer);
	ClassDB::bind_method(D_METHOD("get_active_transfers"), &IRCClientNode::get_active_transfers);

	ClassDB::bind_method(D_METHOD("get_channel", "channel"), &IRCClientNode::get_channel);
	ClassDB::bind_method(D_METHOD("get_joined_channels"), &IRCClientNode::get_joined_channels);
	ClassDB::bind_method(D_METHOD("get_current_nick"), &IRCClientNode::get_current_nick);

	ClassDB::bind_method(D_METHOD("set_messages_per_second", "rate"), &IRCClientNode::set_messages_per_second);
	ClassDB::bind_method(D_METHOD("get_messages_per_second"), &IRCClientNode::get_messages_per_second);

	ClassDB::bind_method(D_METHOD("set_ping_timeout", "timeout_ms"), &IRCClientNode::set_ping_timeout);
	ClassDB::bind_method(D_METHOD("get_ping_timeout"), &IRCClientNode::get_ping_timeout);

	ClassDB::bind_method(D_METHOD("set_dcc_local_ip", "ip"), &IRCClientNode::set_dcc_local_ip);
	ClassDB::bind_method(D_METHOD("get_dcc_local_ip"), &IRCClientNode::get_dcc_local_ip);
	ClassDB::bind_method(D_METHOD("clear_dcc_local_ip"), &IRCClientNode::clear_dcc_local_ip);

	ClassDB::bind_method(D_METHOD("set_token_bucket_size", "size"), &IRCClientNode::set_token_bucket_size);
	ClassDB::bind_method(D_METHOD("get_token_bucket_size"), &IRCClientNode::get_token_bucket_size);

#ifdef MODULE_MBEDTLS_ENABLED
	ClassDB::bind_method(D_METHOD("set_tls_options", "options"), &IRCClientNode::set_tls_options);
	ClassDB::bind_method(D_METHOD("get_tls_options"), &IRCClientNode::get_tls_options);
#endif

	ClassDB::bind_method(D_METHOD("strip_formatting", "text"), &IRCClientNode::strip_formatting);
	ClassDB::bind_method(D_METHOD("parse_formatting", "text"), &IRCClientNode::parse_formatting);

	ClassDB::bind_method(D_METHOD("set_encoding", "encoding"), &IRCClientNode::set_encoding);
	ClassDB::bind_method(D_METHOD("get_encoding"), &IRCClientNode::get_encoding);
	ClassDB::bind_method(D_METHOD("set_auto_detect_encoding", "auto"), &IRCClientNode::set_auto_detect_encoding);
	ClassDB::bind_method(D_METHOD("get_auto_detect_encoding"), &IRCClientNode::get_auto_detect_encoding);
	ClassDB::bind_method(D_METHOD("get_supported_encodings"), &IRCClientNode::get_supported_encodings);

	ClassDB::bind_method(D_METHOD("set_history_enabled", "enabled"), &IRCClientNode::set_history_enabled);
	ClassDB::bind_method(D_METHOD("get_history_enabled"), &IRCClientNode::get_history_enabled);
	ClassDB::bind_method(D_METHOD("set_max_history_size", "size"), &IRCClientNode::set_max_history_size);
	ClassDB::bind_method(D_METHOD("get_max_history_size"), &IRCClientNode::get_max_history_size);
	ClassDB::bind_method(D_METHOD("get_message_history"), &IRCClientNode::get_message_history);
	ClassDB::bind_method(D_METHOD("clear_message_history"), &IRCClientNode::clear_message_history);

	// Forward all signals from IRCClient
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
}

void IRCClientNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// Connect signals from client to node
			if (client.is_valid()) {
				client->connect("connected", Callable(this, "emit_signal").bind("connected"));
				client->connect("disconnected", callable_mp(this, &IRCClientNode::_on_disconnected));
				client->connect("connection_error", Callable(this, "emit_signal").bind("connection_error"));
				client->connect("status_changed", Callable(this, "emit_signal").bind("status_changed"));
				client->connect("message_received", Callable(this, "emit_signal").bind("message_received"));
				client->connect("privmsg", Callable(this, "emit_signal").bind("privmsg"));
				client->connect("notice", Callable(this, "emit_signal").bind("notice"));
				client->connect("ctcp_received", Callable(this, "emit_signal").bind("ctcp_received"));
				client->connect("ctcp_reply", Callable(this, "emit_signal").bind("ctcp_reply"));
				client->connect("joined", Callable(this, "emit_signal").bind("joined"));
				client->connect("parted", Callable(this, "emit_signal").bind("parted"));
				client->connect("kicked", Callable(this, "emit_signal").bind("kicked"));
				client->connect("user_joined", Callable(this, "emit_signal").bind("user_joined"));
				client->connect("user_parted", Callable(this, "emit_signal").bind("user_parted"));
				client->connect("user_quit", Callable(this, "emit_signal").bind("user_quit"));
				client->connect("user_kicked", Callable(this, "emit_signal").bind("user_kicked"));
				client->connect("nick_changed", Callable(this, "emit_signal").bind("nick_changed"));
				client->connect("mode_changed", Callable(this, "emit_signal").bind("mode_changed"));
				client->connect("topic_changed", Callable(this, "emit_signal").bind("topic_changed"));
				client->connect("numeric_001_welcome", Callable(this, "emit_signal").bind("numeric_001_welcome"));
				client->connect("numeric_005_isupport", Callable(this, "emit_signal").bind("numeric_005_isupport"));
				client->connect("numeric_332_topic", Callable(this, "emit_signal").bind("numeric_332_topic"));
				client->connect("numeric_353_names", Callable(this, "emit_signal").bind("numeric_353_names"));
				client->connect("numeric_366_endofnames", Callable(this, "emit_signal").bind("numeric_366_endofnames"));
				client->connect("numeric_372_motd", Callable(this, "emit_signal").bind("numeric_372_motd"));
				client->connect("numeric_433_nicknameinuse", Callable(this, "emit_signal").bind("numeric_433_nicknameinuse"));
				client->connect("numeric_received", Callable(this, "emit_signal").bind("numeric_received"));
				client->connect("dcc_request", Callable(this, "emit_signal").bind("dcc_request"));
				client->connect("dcc_progress", Callable(this, "emit_signal").bind("dcc_progress"));
				client->connect("dcc_completed", Callable(this, "emit_signal").bind("dcc_completed"));
				client->connect("dcc_failed", Callable(this, "emit_signal").bind("dcc_failed"));
				client->connect("capability_list", Callable(this, "emit_signal").bind("capability_list"));
				client->connect("capability_acknowledged", Callable(this, "emit_signal").bind("capability_acknowledged"));
				client->connect("capability_denied", Callable(this, "emit_signal").bind("capability_denied"));
				client->connect("sasl_success", Callable(this, "emit_signal").bind("sasl_success"));
				client->connect("sasl_failed", Callable(this, "emit_signal").bind("sasl_failed"));

				// Account Registration signals
				client->connect("account_registration_success", Callable(this, "emit_signal").bind("account_registration_success"));
				client->connect("account_registration_failed", Callable(this, "emit_signal").bind("account_registration_failed"));
				client->connect("account_verification_required", Callable(this, "emit_signal").bind("account_verification_required"));
				client->connect("account_verification_success", Callable(this, "emit_signal").bind("account_verification_success"));
				client->connect("account_verification_failed", Callable(this, "emit_signal").bind("account_verification_failed"));

				client->connect("tag_json_data", Callable(this, "emit_signal").bind("tag_json_data"));
				client->connect("tag_base64_data", Callable(this, "emit_signal").bind("tag_base64_data"));
				client->connect("numeric_730_mononline", Callable(this, "emit_signal").bind("numeric_730_mononline"));
				client->connect("numeric_731_monoffline", Callable(this, "emit_signal").bind("numeric_731_monoffline"));
				client->connect("standard_reply_fail", Callable(this, "emit_signal").bind("standard_reply_fail"));
				client->connect("standard_reply_warn", Callable(this, "emit_signal").bind("standard_reply_warn"));
				client->connect("standard_reply_note", Callable(this, "emit_signal").bind("standard_reply_note"));
				client->connect("batch_started", Callable(this, "emit_signal").bind("batch_started"));
				client->connect("batch_ended", Callable(this, "emit_signal").bind("batch_ended"));
				client->connect("highlighted", Callable(this, "emit_signal").bind("highlighted"));
				client->connect("latency_measured", Callable(this, "emit_signal").bind("latency_measured"));
			}
		} break;

		case NOTIFICATION_PROCESS: {
			if (client.is_valid()) {
				client->poll();
			}
		} break;
	}
}

Ref<IRCClient> IRCClientNode::get_client() {
	return client;
}

Error IRCClientNode::connect_to_server(const String &p_host, int p_port, bool p_use_ssl, const String &p_nick, const String &p_username, const String &p_realname, const String &p_password) {
	last_server = p_host;
	last_port = p_port;
	last_use_ssl = p_use_ssl;
	last_nick = p_nick;
	last_username = p_username;
	last_realname = p_realname;
	last_password = p_password;

	return client->connect_to_server(p_host, p_port, p_use_ssl, p_nick, p_username, p_realname, p_password);
}

void IRCClientNode::disconnect_from_server(const String &p_quit_message) {
	client->disconnect_from_server(p_quit_message);
}

bool IRCClientNode::is_irc_connected() const {
	return client->is_irc_connected();
}

IRCClient::Status IRCClientNode::get_status() const {
	return client->get_status();
}

void IRCClientNode::send_raw(const String &p_message) {
	client->send_raw(p_message);
}

void IRCClientNode::send_privmsg(const String &p_target, const String &p_message) {
	client->send_privmsg(p_target, p_message);
}

void IRCClientNode::send_notice(const String &p_target, const String &p_message) {
	client->send_notice(p_target, p_message);
}

void IRCClientNode::send_action(const String &p_target, const String &p_action) {
	client->send_action(p_target, p_action);
}

void IRCClientNode::join_channel(const String &p_channel, const String &p_key) {
	client->join_channel(p_channel, p_key);
}

void IRCClientNode::part_channel(const String &p_channel, const String &p_message) {
	client->part_channel(p_channel, p_message);
}

void IRCClientNode::set_topic(const String &p_channel, const String &p_topic) {
	client->set_topic(p_channel, p_topic);
}

void IRCClientNode::set_nick(const String &p_new_nick) {
	client->set_nick(p_new_nick);
}

void IRCClientNode::set_mode(const String &p_target, const String &p_modes, const PackedStringArray &p_params) {
	client->set_mode(p_target, p_modes, p_params);
}

void IRCClientNode::send_whois(const String &p_nick) {
	client->send_whois(p_nick);
}

void IRCClientNode::set_realname(const String &p_realname) {
	client->set_realname(p_realname);
}

void IRCClientNode::monitor_add(const String &p_nick) {
	client->monitor_add(p_nick);
}

void IRCClientNode::monitor_remove(const String &p_nick) {
	client->monitor_remove(p_nick);
}

void IRCClientNode::monitor_clear() {
	client->monitor_clear();
}

void IRCClientNode::monitor_list() {
	client->monitor_list();
}

void IRCClientNode::monitor_status() {
	client->monitor_status();
}

PackedStringArray IRCClientNode::get_monitored_nicks() const {
	return client->get_monitored_nicks();
}

void IRCClientNode::request_capability(const String &p_capability) {
	client->request_capability(p_capability);
}

PackedStringArray IRCClientNode::get_available_capabilities() const {
	return client->get_available_capabilities();
}

PackedStringArray IRCClientNode::get_enabled_capabilities() const {
	return client->get_enabled_capabilities();
}

void IRCClientNode::enable_sasl(const String &p_username, const String &p_password) {
	client->enable_sasl(p_username, p_password);
}

void IRCClientNode::enable_sasl_plain(const String &p_username, const String &p_password) {
	client->enable_sasl_plain(p_username, p_password);
}

void IRCClientNode::enable_sasl_external() {
	client->enable_sasl_external();
}

void IRCClientNode::enable_sasl_scram_sha256(const String &p_username, const String &p_password) {
	client->enable_sasl_scram_sha256(p_username, p_password);
}

void IRCClientNode::disable_sasl() {
	client->disable_sasl();
}

Error IRCClientNode::send_dcc_file(const String &p_nick, const String &p_file_path) {
	return client->send_dcc_file(p_nick, p_file_path);
}

void IRCClientNode::accept_dcc_transfer(int p_transfer_index) {
	client->accept_dcc_transfer(p_transfer_index);
}

void IRCClientNode::reject_dcc_transfer(int p_transfer_index) {
	client->reject_dcc_transfer(p_transfer_index);
}

void IRCClientNode::cancel_dcc_transfer(int p_transfer_index) {
	client->cancel_dcc_transfer(p_transfer_index);
}

TypedArray<IRCDCCTransfer> IRCClientNode::get_active_transfers() const {
	return client->get_active_transfers();
}

Ref<IRCChannel> IRCClientNode::get_channel(const String &p_channel) const {
	return client->get_channel(p_channel);
}

PackedStringArray IRCClientNode::get_joined_channels() const {
	return client->get_joined_channels();
}

String IRCClientNode::get_current_nick() const {
	return client->get_current_nick();
}

void IRCClientNode::set_messages_per_second(int p_rate) {
	client->set_messages_per_second(p_rate);
}

int IRCClientNode::get_messages_per_second() const {
	return client->get_messages_per_second();
}

void IRCClientNode::set_ping_timeout(int p_timeout_ms) {
	client->set_ping_timeout(p_timeout_ms);
}

int IRCClientNode::get_ping_timeout() const {
	return client->get_ping_timeout();
}

void IRCClientNode::set_dcc_local_ip(const IPAddress &p_ip) {
	client->set_dcc_local_ip(p_ip);
}

IPAddress IRCClientNode::get_dcc_local_ip() const {
	return client->get_dcc_local_ip();
}

void IRCClientNode::clear_dcc_local_ip() {
	client->clear_dcc_local_ip();
}

void IRCClientNode::set_token_bucket_size(int p_size) {
	client->set_token_bucket_size(p_size);
}

int IRCClientNode::get_token_bucket_size() const {
	return client->get_token_bucket_size();
}

#ifdef MODULE_MBEDTLS_ENABLED
void IRCClientNode::set_tls_options(const Ref<TLSOptions> &p_options) {
	client->set_tls_options(p_options);
}

Ref<TLSOptions> IRCClientNode::get_tls_options() const {
	return client->get_tls_options();
}
#endif

String IRCClientNode::strip_formatting(const String &p_text) {
	return client->strip_formatting(p_text);
}

Dictionary IRCClientNode::parse_formatting(const String &p_text) {
	return client->parse_formatting(p_text);
}

void IRCClientNode::set_encoding(const String &p_encoding) {
	client->set_encoding(p_encoding);
}

String IRCClientNode::get_encoding() const {
	return client->get_encoding();
}

void IRCClientNode::set_auto_detect_encoding(bool p_auto) {
	client->set_auto_detect_encoding(p_auto);
}

bool IRCClientNode::get_auto_detect_encoding() const {
	return client->get_auto_detect_encoding();
}

PackedStringArray IRCClientNode::get_supported_encodings() const {
	return client->get_supported_encodings();
}

void IRCClientNode::set_history_enabled(bool p_enabled) {
	client->set_history_enabled(p_enabled);
}

bool IRCClientNode::get_history_enabled() const {
	return client->get_history_enabled();
}

void IRCClientNode::set_max_history_size(int p_size) {
	client->set_max_history_size(p_size);
}

int IRCClientNode::get_max_history_size() const {
	return client->get_max_history_size();
}

Array IRCClientNode::get_message_history() const {
	return client->get_message_history();
}

void IRCClientNode::clear_message_history() {
	client->clear_message_history();
}

// Auto-reconnect forwarding (client-level)
void IRCClientNode::enable_auto_reconnect(bool p_enabled) {
	client->enable_auto_reconnect(p_enabled);
}

bool IRCClientNode::is_auto_reconnect_enabled() const {
	return client->is_auto_reconnect_enabled();
}

void IRCClientNode::set_reconnect_delay(int p_seconds) {
	client->set_reconnect_delay(p_seconds);
}

int IRCClientNode::get_reconnect_delay() const {
	return client->get_reconnect_delay();
}

void IRCClientNode::set_max_reconnect_attempts(int p_max) {
	client->set_max_reconnect_attempts(p_max);
}

int IRCClientNode::get_max_reconnect_attempts() const {
	return client->get_max_reconnect_attempts();
}

int IRCClientNode::get_reconnect_attempts() const {
	return client->get_reconnect_attempts();
}

// Nickname alternatives forwarding
void IRCClientNode::set_alternative_nicks(const PackedStringArray &p_nicks) {
	client->set_alternative_nicks(p_nicks);
}

void IRCClientNode::add_alternative_nick(const String &p_nick) {
	client->add_alternative_nick(p_nick);
}

void IRCClientNode::clear_alternative_nicks() {
	client->clear_alternative_nicks();
}

PackedStringArray IRCClientNode::get_alternative_nicks() const {
	return client->get_alternative_nicks();
}

// Auto-join channels forwarding
void IRCClientNode::add_autojoin_channel(const String &p_channel, const String &p_key) {
	client->add_autojoin_channel(p_channel, p_key);
}

void IRCClientNode::remove_autojoin_channel(const String &p_channel) {
	client->remove_autojoin_channel(p_channel);
}

void IRCClientNode::clear_autojoin_channels() {
	client->clear_autojoin_channels();
}

PackedStringArray IRCClientNode::get_autojoin_channels() const {
	return client->get_autojoin_channels();
}

void IRCClientNode::enable_autojoin(bool p_enabled) {
	client->enable_autojoin(p_enabled);
}

bool IRCClientNode::is_autojoin_enabled() const {
	return client->is_autojoin_enabled();
}

// Ignore list forwarding
void IRCClientNode::ignore_user(const String &p_mask) {
	client->ignore_user(p_mask);
}

void IRCClientNode::unignore_user(const String &p_mask) {
	client->unignore_user(p_mask);
}

void IRCClientNode::clear_ignores() {
	client->clear_ignores();
}

PackedStringArray IRCClientNode::get_ignored_users() const {
	return client->get_ignored_users();
}

bool IRCClientNode::is_ignored(const String &p_nick) const {
	return client->is_ignored(p_nick);
}

// Channel operator helpers forwarding
void IRCClientNode::op_user(const String &p_channel, const String &p_nick) {
	client->op_user(p_channel, p_nick);
}

void IRCClientNode::deop_user(const String &p_channel, const String &p_nick) {
	client->deop_user(p_channel, p_nick);
}

void IRCClientNode::voice_user(const String &p_channel, const String &p_nick) {
	client->voice_user(p_channel, p_nick);
}

void IRCClientNode::devoice_user(const String &p_channel, const String &p_nick) {
	client->devoice_user(p_channel, p_nick);
}

void IRCClientNode::kick_user(const String &p_channel, const String &p_nick, const String &p_reason) {
	client->kick_user(p_channel, p_nick, p_reason);
}

void IRCClientNode::ban_user(const String &p_channel, const String &p_mask) {
	client->ban_user(p_channel, p_mask);
}

void IRCClientNode::unban_user(const String &p_channel, const String &p_mask) {
	client->unban_user(p_channel, p_mask);
}

void IRCClientNode::kickban_user(const String &p_channel, const String &p_nick, const String &p_reason) {
	client->kickban_user(p_channel, p_nick, p_reason);
}

// Fallback servers forwarding
void IRCClientNode::add_fallback_server(const String &p_host, int p_port, bool p_use_ssl) {
	client->add_fallback_server(p_host, p_port, p_use_ssl);
}

void IRCClientNode::clear_fallback_servers() {
	client->clear_fallback_servers();
}

int IRCClientNode::get_fallback_server_count() const {
	return client->get_fallback_server_count();
}

// NickServ/Services helpers forwarding
void IRCClientNode::identify_nickserv(const String &p_password) {
	client->identify_nickserv(p_password);
}

void IRCClientNode::ghost_nick(const String &p_nick, const String &p_password) {
	client->ghost_nick(p_nick, p_password);
}

void IRCClientNode::register_nick(const String &p_email, const String &p_password) {
	client->register_nick(p_email, p_password);
}

void IRCClientNode::group_nick(const String &p_password) {
	client->group_nick(p_password);
}

void IRCClientNode::register_channel(const String &p_channel) {
	client->register_channel(p_channel);
}

void IRCClientNode::identify_chanserv(const String &p_channel, const String &p_password) {
	client->identify_chanserv(p_channel, p_password);
}

// Channel LIST with filters forwarding
void IRCClientNode::list_channels(const String &p_pattern, int p_min_users, int p_max_users) {
	client->list_channels(p_pattern, p_min_users, p_max_users);
}

// IRCv3 Chathistory forwarding
void IRCClientNode::request_chathistory(const String &p_target, const String &p_timestamp_start, const String &p_timestamp_end, int p_limit) {
	client->request_chathistory(p_target, p_timestamp_start, p_timestamp_end, p_limit);
}

void IRCClientNode::request_chathistory_before(const String &p_target, const String &p_msgid, int p_limit) {
	client->request_chathistory_before(p_target, p_msgid, p_limit);
}

void IRCClientNode::request_chathistory_after(const String &p_target, const String &p_msgid, int p_limit) {
	client->request_chathistory_after(p_target, p_msgid, p_limit);
}

void IRCClientNode::request_chathistory_latest(const String &p_target, int p_limit) {
	client->request_chathistory_latest(p_target, p_limit);
}

// IRCv3 Multiline forwarding
void IRCClientNode::send_multiline_privmsg(const String &p_target, const PackedStringArray &p_lines) {
	client->send_multiline_privmsg(p_target, p_lines);
}

void IRCClientNode::send_multiline_notice(const String &p_target, const PackedStringArray &p_lines) {
	client->send_multiline_notice(p_target, p_lines);
}

// Nick completion helpers forwarding
PackedStringArray IRCClientNode::get_matching_nicks(const String &p_channel, const String &p_prefix) const {
	return client->get_matching_nicks(p_channel, p_prefix);
}

String IRCClientNode::complete_nick(const String &p_channel, const String &p_partial, int p_cycle) const {
	return client->complete_nick(p_channel, p_partial, p_cycle);
}

// Highlight/Mention detection forwarding
void IRCClientNode::add_highlight_pattern(const String &p_pattern) {
	client->add_highlight_pattern(p_pattern);
}

void IRCClientNode::remove_highlight_pattern(const String &p_pattern) {
	client->remove_highlight_pattern(p_pattern);
}

void IRCClientNode::clear_highlight_patterns() {
	client->clear_highlight_patterns();
}

PackedStringArray IRCClientNode::get_highlight_patterns() const {
	return client->get_highlight_patterns();
}

bool IRCClientNode::is_highlighted(const String &p_message, const String &p_nick) const {
	return client->is_highlighted(p_message, p_nick);
}

// URL extraction forwarding
PackedStringArray IRCClientNode::extract_urls(const String &p_message) const {
	return client->extract_urls(p_message);
}

// Message ID tracking forwarding
String IRCClientNode::get_message_text_by_id(const String &p_message_id) const {
	return client->get_message_text_by_id(p_message_id);
}

void IRCClientNode::set_max_tracked_messages(int p_max) {
	client->set_max_tracked_messages(p_max);
}

int IRCClientNode::get_max_tracked_messages() const {
	return client->get_max_tracked_messages();
}

// Connection metrics forwarding
Dictionary IRCClientNode::get_connection_stats() const {
	return client->get_connection_stats();
}

void IRCClientNode::reset_connection_stats() {
	client->reset_connection_stats();
}

int IRCClientNode::get_average_latency() const {
	return client->get_average_latency();
}

// IRC Commands (Low Priority) forwarding
void IRCClientNode::send_oper(const String &p_username, const String &p_password) {
	client->send_oper(p_username, p_password);
}

void IRCClientNode::knock_channel(const String &p_channel, const String &p_message) {
	client->knock_channel(p_channel, p_message);
}

void IRCClientNode::silence_user(const String &p_mask) {
	client->silence_user(p_mask);
}

void IRCClientNode::unsilence_user(const String &p_mask) {
	client->unsilence_user(p_mask);
}

void IRCClientNode::list_silence() {
	client->list_silence();
}

void IRCClientNode::who_channel(const String &p_channel) {
	client->who_channel(p_channel);
}

void IRCClientNode::who_user(const String &p_mask) {
	client->who_user(p_mask);
}

void IRCClientNode::whox(const String &p_mask, const String &p_fields, int p_querytype) {
	client->whox(p_mask, p_fields, p_querytype);
}

void IRCClientNode::whowas_user(const String &p_nick, int p_count) {
	client->whowas_user(p_nick, p_count);
}

void IRCClientNode::invite_user(const String &p_channel, const String &p_nick) {
	client->invite_user(p_channel, p_nick);
}

void IRCClientNode::userhost(const String &p_nick) {
	client->userhost(p_nick);
}

void IRCClientNode::register_account(const String &p_account, const String &p_password, const String &p_email) {
	client->register_account(p_account, p_password, p_email);
}

void IRCClientNode::verify_account(const String &p_account, const String &p_code) {
	client->verify_account(p_account, p_code);
}

void IRCClientNode::set_bot_mode(bool p_enabled) {
	client->set_bot_mode(p_enabled);
}

bool IRCClientNode::get_bot_mode() const {
	return client->get_bot_mode();
}

void IRCClientNode::send_reply(const String &p_target, const String &p_message, const String &p_reply_to_msgid) {
	client->send_reply(p_target, p_message, p_reply_to_msgid);
}

void IRCClientNode::send_reply_notice(const String &p_target, const String &p_message, const String &p_reply_to_msgid) {
	client->send_reply_notice(p_target, p_message, p_reply_to_msgid);
}

String IRCClientNode::get_reply_to_msgid(const Dictionary &p_tags) const {
	return client->get_reply_to_msgid(p_tags);
}

bool IRCClientNode::has_sts_policy(const String &p_hostname) const {
	return client->has_sts_policy(p_hostname);
}

void IRCClientNode::clear_sts_policy(const String &p_hostname) {
	client->clear_sts_policy(p_hostname);
}

void IRCClientNode::clear_all_sts_policies() {
	client->clear_all_sts_policies();
}

// Channel Management (Low Priority) forwarding
void IRCClientNode::set_channel_key(const String &p_channel, const String &p_key) {
	client->set_channel_key(p_channel, p_key);
}

String IRCClientNode::get_channel_key(const String &p_channel) const {
	return client->get_channel_key(p_channel);
}

void IRCClientNode::clear_channel_key(const String &p_channel) {
	client->clear_channel_key(p_channel);
}

void IRCClientNode::request_ban_list(const String &p_channel) {
	client->request_ban_list(p_channel);
}

void IRCClientNode::set_ban(const String &p_channel, const String &p_mask) {
	client->set_ban(p_channel, p_mask);
}

void IRCClientNode::remove_ban(const String &p_channel, const String &p_mask) {
	client->remove_ban(p_channel, p_mask);
}

void IRCClientNode::request_exception_list(const String &p_channel) {
	client->request_exception_list(p_channel);
}

void IRCClientNode::set_exception(const String &p_channel, const String &p_mask) {
	client->set_exception(p_channel, p_mask);
}

void IRCClientNode::request_invite_list(const String &p_channel) {
	client->request_invite_list(p_channel);
}

void IRCClientNode::set_invite_exception(const String &p_channel, const String &p_mask) {
	client->set_invite_exception(p_channel, p_mask);
}

void IRCClientNode::quiet_user(const String &p_channel, const String &p_mask) {
	client->quiet_user(p_channel, p_mask);
}

void IRCClientNode::unquiet_user(const String &p_channel, const String &p_mask) {
	client->unquiet_user(p_channel, p_mask);
}

void IRCClientNode::request_quiet_list(const String &p_channel) {
	client->request_quiet_list(p_channel);
}

// Away Status (Low Priority) forwarding
void IRCClientNode::set_away(const String &p_message) {
	client->set_away(p_message);
}

void IRCClientNode::set_back() {
	client->set_back();
}

bool IRCClientNode::get_is_away() const {
	return client->get_is_away();
}

String IRCClientNode::get_away_message() const {
	return client->get_away_message();
}

// User Tracking (Low Priority) forwarding
Ref<IRCUser> IRCClientNode::get_user(const String &p_nick) const {
	return client->get_user(p_nick);
}

PackedStringArray IRCClientNode::get_common_channels(const String &p_nick) const {
	return client->get_common_channels(p_nick);
}

Dictionary IRCClientNode::get_user_info(const String &p_nick) const {
	return client->get_user_info(p_nick);
}

// IRCv3 Extensions (Low Priority) forwarding
void IRCClientNode::send_read_marker(const String &p_channel, const String &p_timestamp) {
	client->send_read_marker(p_channel, p_timestamp);
}

void IRCClientNode::send_typing_notification(const String &p_channel, bool p_typing) {
	client->send_typing_notification(p_channel, p_typing);
}

void IRCClientNode::send_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction) {
	client->send_reaction(p_channel, p_msgid, p_reaction);
}

void IRCClientNode::remove_reaction(const String &p_channel, const String &p_msgid, const String &p_reaction) {
	client->remove_reaction(p_channel, p_msgid, p_reaction);
}

void IRCClientNode::_on_disconnected(const String &p_reason) {
	emit_signal("disconnected", p_reason);
}

IRCClientNode::IRCClientNode() {
	client.instantiate();
	set_process(true);
}

IRCClientNode::~IRCClientNode() {
	if (client.is_valid()) {
		client->disconnect_from_server();
	}
}
