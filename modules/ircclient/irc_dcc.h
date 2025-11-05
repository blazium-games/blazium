/**************************************************************************/
/*  irc_dcc.h                                                             */
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

#include "core/io/ip_address.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class IRCDCCTransfer : public RefCounted {
	GDCLASS(IRCDCCTransfer, RefCounted);

	friend class IRCClient;

public:
	enum Type {
		TYPE_FILE_SEND,
		TYPE_FILE_RECEIVE,
		TYPE_CHAT,
	};

	enum Status {
		DCC_STATUS_PENDING,
		DCC_STATUS_CONNECTING,
		DCC_STATUS_TRANSFERRING,
		DCC_STATUS_COMPLETED,
		DCC_STATUS_FAILED,
		DCC_STATUS_CANCELLED,
	};

private:
	Type type = TYPE_FILE_SEND;
	Status status = DCC_STATUS_PENDING;

	String remote_nick;
	String filename;
	String local_path;
	int64_t file_size = 0;
	int64_t transferred = 0;
	int64_t resume_position = 0;

	IPAddress address;
	int port = 0;
	bool use_ipv6 = false;

	Ref<StreamPeerTCP> connection;
	Ref<TCPServer> server; // For sending
	Ref<FileAccess> file;

	String error_message;

	// Internal DCC protocol helpers
	String _create_dcc_send_message() const;

protected:
	static void _bind_methods();

public:
	void set_transfer_type(Type p_type);
	Type get_transfer_type() const;

	void set_status(Status p_status);
	Status get_status() const;

	void set_remote_nick(const String &p_nick);
	String get_remote_nick() const;

	void set_filename(const String &p_filename);
	String get_filename() const;

	void set_local_path(const String &p_path);
	String get_local_path() const;

	void set_file_size(int64_t p_size);
	int64_t get_file_size() const;

	void set_transferred(int64_t p_transferred);
	int64_t get_transferred() const;

	void set_resume_position(int64_t p_position);
	int64_t get_resume_position() const;

	void set_address(const IPAddress &p_address);
	IPAddress get_address() const;

	void set_port(int p_port);
	int get_port() const;

	void set_use_ipv6(bool p_use_ipv6);
	bool get_use_ipv6() const;

	void set_error_message(const String &p_message);
	String get_error_message() const;

	// Transfer control
	Error accept();
	Error reject();
	Error cancel();

	// Progress
	float get_progress() const;

	// Internal - do not call directly from scripts
	Error start_send(const String &p_file_path);
	Error start_receive();
	Error poll(); // Called by IRC client to update transfer

	IRCDCCTransfer();
	~IRCDCCTransfer();
};

VARIANT_ENUM_CAST(IRCDCCTransfer::Type);
VARIANT_ENUM_CAST(IRCDCCTransfer::Status);
