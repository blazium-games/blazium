/**************************************************************************/
/*  irc_dcc.cpp                                                           */
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

#include "irc_dcc.h"

#include "core/io/file_access.h"

void IRCDCCTransfer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_transfer_type", "type"), &IRCDCCTransfer::set_transfer_type);
	ClassDB::bind_method(D_METHOD("get_transfer_type"), &IRCDCCTransfer::get_transfer_type);

	ClassDB::bind_method(D_METHOD("set_status", "status"), &IRCDCCTransfer::set_status);
	ClassDB::bind_method(D_METHOD("get_status"), &IRCDCCTransfer::get_status);

	ClassDB::bind_method(D_METHOD("set_remote_nick", "nick"), &IRCDCCTransfer::set_remote_nick);
	ClassDB::bind_method(D_METHOD("get_remote_nick"), &IRCDCCTransfer::get_remote_nick);

	ClassDB::bind_method(D_METHOD("set_filename", "filename"), &IRCDCCTransfer::set_filename);
	ClassDB::bind_method(D_METHOD("get_filename"), &IRCDCCTransfer::get_filename);

	ClassDB::bind_method(D_METHOD("set_local_path", "path"), &IRCDCCTransfer::set_local_path);
	ClassDB::bind_method(D_METHOD("get_local_path"), &IRCDCCTransfer::get_local_path);

	ClassDB::bind_method(D_METHOD("set_file_size", "size"), &IRCDCCTransfer::set_file_size);
	ClassDB::bind_method(D_METHOD("get_file_size"), &IRCDCCTransfer::get_file_size);

	ClassDB::bind_method(D_METHOD("get_transferred"), &IRCDCCTransfer::get_transferred);

	ClassDB::bind_method(D_METHOD("set_resume_position", "position"), &IRCDCCTransfer::set_resume_position);
	ClassDB::bind_method(D_METHOD("get_resume_position"), &IRCDCCTransfer::get_resume_position);

	ClassDB::bind_method(D_METHOD("set_address", "address"), &IRCDCCTransfer::set_address);
	ClassDB::bind_method(D_METHOD("get_address"), &IRCDCCTransfer::get_address);

	ClassDB::bind_method(D_METHOD("set_port", "port"), &IRCDCCTransfer::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &IRCDCCTransfer::get_port);

	ClassDB::bind_method(D_METHOD("set_use_ipv6", "use_ipv6"), &IRCDCCTransfer::set_use_ipv6);
	ClassDB::bind_method(D_METHOD("get_use_ipv6"), &IRCDCCTransfer::get_use_ipv6);

	ClassDB::bind_method(D_METHOD("get_error_message"), &IRCDCCTransfer::get_error_message);

	ClassDB::bind_method(D_METHOD("accept"), &IRCDCCTransfer::accept);
	ClassDB::bind_method(D_METHOD("reject"), &IRCDCCTransfer::reject);
	ClassDB::bind_method(D_METHOD("cancel"), &IRCDCCTransfer::cancel);

	ClassDB::bind_method(D_METHOD("get_progress"), &IRCDCCTransfer::get_progress);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "type"), "set_transfer_type", "get_transfer_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "status"), "set_status", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "remote_nick"), "set_remote_nick", "get_remote_nick");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "filename"), "set_filename", "get_filename");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "local_path"), "set_local_path", "get_local_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "file_size"), "set_file_size", "get_file_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transferred"), "", "get_transferred");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "address"), "set_address", "get_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_ipv6"), "set_use_ipv6", "get_use_ipv6");

	BIND_ENUM_CONSTANT(TYPE_FILE_SEND);
	BIND_ENUM_CONSTANT(TYPE_FILE_RECEIVE);
	BIND_ENUM_CONSTANT(TYPE_CHAT);

	BIND_ENUM_CONSTANT(DCC_STATUS_PENDING);
	BIND_ENUM_CONSTANT(DCC_STATUS_CONNECTING);
	BIND_ENUM_CONSTANT(DCC_STATUS_TRANSFERRING);
	BIND_ENUM_CONSTANT(DCC_STATUS_COMPLETED);
	BIND_ENUM_CONSTANT(DCC_STATUS_FAILED);
	BIND_ENUM_CONSTANT(DCC_STATUS_CANCELLED);
}

void IRCDCCTransfer::set_transfer_type(Type p_type) {
	type = p_type;
}

IRCDCCTransfer::Type IRCDCCTransfer::get_transfer_type() const {
	return type;
}

void IRCDCCTransfer::set_status(Status p_status) {
	status = p_status;
}

IRCDCCTransfer::Status IRCDCCTransfer::get_status() const {
	return status;
}

void IRCDCCTransfer::set_remote_nick(const String &p_nick) {
	remote_nick = p_nick;
}

String IRCDCCTransfer::get_remote_nick() const {
	return remote_nick;
}

void IRCDCCTransfer::set_filename(const String &p_filename) {
	filename = p_filename;
}

String IRCDCCTransfer::get_filename() const {
	return filename;
}

void IRCDCCTransfer::set_local_path(const String &p_path) {
	local_path = p_path;
}

String IRCDCCTransfer::get_local_path() const {
	return local_path;
}

void IRCDCCTransfer::set_file_size(int64_t p_size) {
	file_size = p_size;
}

int64_t IRCDCCTransfer::get_file_size() const {
	return file_size;
}

void IRCDCCTransfer::set_transferred(int64_t p_transferred) {
	transferred = p_transferred;
}

int64_t IRCDCCTransfer::get_transferred() const {
	return transferred;
}

void IRCDCCTransfer::set_resume_position(int64_t p_position) {
	resume_position = p_position;
	transferred = p_position;
}

int64_t IRCDCCTransfer::get_resume_position() const {
	return resume_position;
}

void IRCDCCTransfer::set_address(const IPAddress &p_address) {
	address = p_address;
}

IPAddress IRCDCCTransfer::get_address() const {
	return address;
}

void IRCDCCTransfer::set_port(int p_port) {
	port = p_port;
}

int IRCDCCTransfer::get_port() const {
	return port;
}

void IRCDCCTransfer::set_use_ipv6(bool p_use_ipv6) {
	use_ipv6 = p_use_ipv6;
}

bool IRCDCCTransfer::get_use_ipv6() const {
	return use_ipv6;
}

void IRCDCCTransfer::set_error_message(const String &p_message) {
	error_message = p_message;
}

String IRCDCCTransfer::get_error_message() const {
	return error_message;
}

String IRCDCCTransfer::_create_dcc_send_message() const {
	if (use_ipv6) {
		// IPv6 DCC uses string format instead of integer
		Array args;
		args.push_back(filename);
		args.push_back(address.get_ipv6());
		args.push_back(String::num_int64(port));
		args.push_back(String::num_int64(file_size));
		return String("SEND {0} {1} {2} {3}").format(args);
	} else {
		// IPv4 DCC uses 32-bit integer
		const uint8_t *ipv4 = address.get_ipv4();
		uint32_t ip_int = (ipv4[0] << 24) | (ipv4[1] << 16) |
				(ipv4[2] << 8) | ipv4[3];
		Array args;
		args.push_back(filename);
		args.push_back(String::num_int64(ip_int));
		args.push_back(String::num_int64(port));
		args.push_back(String::num_int64(file_size));
		return String("SEND {0} {1} {2} {3}").format(args);
	}
}

Error IRCDCCTransfer::accept() {
	if (status != DCC_STATUS_PENDING) {
		return ERR_INVALID_PARAMETER;
	}

	if (type == TYPE_FILE_RECEIVE) {
		return start_receive();
	}

	return OK;
}

Error IRCDCCTransfer::reject() {
	if (status != DCC_STATUS_PENDING) {
		return ERR_INVALID_PARAMETER;
	}

	status = DCC_STATUS_CANCELLED;
	return OK;
}

Error IRCDCCTransfer::cancel() {
	if (status == DCC_STATUS_COMPLETED || status == DCC_STATUS_FAILED) {
		return ERR_INVALID_PARAMETER;
	}

	status = DCC_STATUS_CANCELLED;

	if (connection.is_valid()) {
		connection->disconnect_from_host();
		connection.unref();
	}

	if (server.is_valid()) {
		server->stop();
		server.unref();
	}

	if (file.is_valid()) {
		file->close();
		file.unref();
	}

	return OK;
}

float IRCDCCTransfer::get_progress() const {
	if (file_size == 0) {
		return 0.0f;
	}

	return (float)transferred / (float)file_size;
}

Error IRCDCCTransfer::start_send(const String &p_file_path) {
	ERR_FAIL_COND_V(type != TYPE_FILE_SEND, ERR_INVALID_PARAMETER);

	file = FileAccess::open(p_file_path, FileAccess::READ);
	if (file.is_null()) {
		error_message = "Failed to open file for reading";
		status = DCC_STATUS_FAILED;
		return ERR_FILE_CANT_OPEN;
	}

	file_size = file->get_length();
	local_path = p_file_path;

	// Create TCP server to listen for incoming connection
	server.instantiate();

	Error err = server->listen(0); // Use random port
	if (err != OK) {
		error_message = "Failed to start listening socket";
		status = DCC_STATUS_FAILED;
		return err;
	}

	port = server->get_local_port();
	status = DCC_STATUS_CONNECTING;

	return OK;
}

Error IRCDCCTransfer::start_receive() {
	ERR_FAIL_COND_V(type != TYPE_FILE_RECEIVE, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(local_path.is_empty(), ERR_FILE_BAD_PATH);

	// Open file for writing
	int flags = FileAccess::WRITE;
	if (resume_position > 0) {
		flags |= FileAccess::READ; // Need read access for append mode
	}

	file = FileAccess::open(local_path, (FileAccess::ModeFlags)flags);
	if (file.is_null()) {
		error_message = "Failed to open file for writing";
		status = DCC_STATUS_FAILED;
		return ERR_FILE_CANT_WRITE;
	}

	if (resume_position > 0) {
		file->seek(resume_position);
	}

	// Connect to sender
	connection.instantiate();
	Error err = connection->connect_to_host(address, port);
	if (err != OK) {
		error_message = "Failed to connect to sender";
		status = DCC_STATUS_FAILED;
		return err;
	}

	status = DCC_STATUS_CONNECTING;
	return OK;
}

Error IRCDCCTransfer::poll() {
	if (status == DCC_STATUS_COMPLETED || status == DCC_STATUS_FAILED || status == DCC_STATUS_CANCELLED) {
		return OK;
	}

	if (type == TYPE_FILE_SEND) {
		// Sending file
		if (status == DCC_STATUS_CONNECTING) {
			if (server.is_valid() && server->is_connection_available()) {
				connection = server->take_connection();
				server->stop();
				server.unref();
				status = DCC_STATUS_TRANSFERRING;
			}
		}

		if (status == DCC_STATUS_TRANSFERRING && connection.is_valid()) {
			Error err = connection->poll();
			if (err != OK && err != ERR_BUSY) {
				error_message = "Connection error";
				status = DCC_STATUS_FAILED;
				return err;
			}

			if (connection->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
				// Send file data in chunks
				const int CHUNK_SIZE = 4096; // Standard DCC chunk size
				uint8_t buffer[CHUNK_SIZE];

				int to_send = MIN(CHUNK_SIZE, file_size - transferred);
				if (to_send > 0) {
					int read = file->get_buffer(buffer, to_send);
					if (read > 0) {
						int sent;
						err = connection->put_partial_data(buffer, read, sent);
						if (err == OK) {
							transferred += sent;
						}
					}
				}

				if (transferred >= file_size) {
					status = DCC_STATUS_COMPLETED;
					connection->disconnect_from_host();
					file->close();
				}
			}
		}
	} else if (type == TYPE_FILE_RECEIVE) {
		// Receiving file
		if (status == DCC_STATUS_CONNECTING && connection.is_valid()) {
			Error err = connection->poll();
			if (err != OK && err != ERR_BUSY) {
				error_message = "Connection error";
				status = DCC_STATUS_FAILED;
				return err;
			}

			if (connection->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
				status = DCC_STATUS_TRANSFERRING;
			}
		}

		if (status == DCC_STATUS_TRANSFERRING && connection.is_valid()) {
			Error err = connection->poll();
			if (err != OK && err != ERR_BUSY) {
				error_message = "Connection error";
				status = DCC_STATUS_FAILED;
				return err;
			}

			// Receive file data
			int available = connection->get_available_bytes();
			if (available > 0) {
				const int CHUNK_SIZE = 4096;
				uint8_t buffer[CHUNK_SIZE];

				int to_read = MIN(CHUNK_SIZE, available);
				to_read = MIN(to_read, file_size - transferred);

				int received;
				err = connection->get_partial_data(buffer, to_read, received);
				if (err == OK && received > 0) {
					file->store_buffer(buffer, received);
					transferred += received;

					// Send acknowledgment (4-byte big-endian integer)
					uint32_t ack = transferred;
					uint8_t ack_bytes[4];
					ack_bytes[0] = (ack >> 24) & 0xFF;
					ack_bytes[1] = (ack >> 16) & 0xFF;
					ack_bytes[2] = (ack >> 8) & 0xFF;
					ack_bytes[3] = ack & 0xFF;
					connection->put_data(ack_bytes, 4);
				}
			}

			if (transferred >= file_size) {
				status = DCC_STATUS_COMPLETED;
				connection->disconnect_from_host();
				file->close();
			}
		}
	}

	return OK;
}

IRCDCCTransfer::IRCDCCTransfer() {
}

IRCDCCTransfer::~IRCDCCTransfer() {
	if (connection.is_valid()) {
		connection->disconnect_from_host();
	}

	if (server.is_valid()) {
		server->stop();
	}

	if (file.is_valid()) {
		file->close();
	}
}
