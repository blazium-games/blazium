/**************************************************************************/
/*  connection.h                                                          */
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

#include "client/sdk/tls_options.h"
#include "common/net/stream_transport.h"
#include "common/protocol/framing.h"
#include "common/protocol/messages.h"
#include "common/protocol/session.h"
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace coldstorage {

class ServerConnection {
public:
	ServerConnection();
	~ServerConnection();

	bool connectTransport(const std::string &host, int port, const TlsOptions &tls = {});
	bool connect(const std::string &host, int port, const TlsOptions &tls = {});
	void disconnect();
	bool isConnected() const;
	std::string peerFingerprint() const;
	bool negotiate();
	bool sendMessage(const Message &msg);
	std::optional<Message> readMessage();
	bool sendInstruction(const Instruction &instr);
	std::optional<Instruction> readInstruction();

	struct CommandResult {
		bool success = false;
		std::string error;
		nlohmann::json data;
		std::vector<Instruction> instructions;
		bool instructionsTruncated = false;
	};
	CommandResult executeCommand(const Message &cmd);

private:
	TransportPtr transport_;
	bool connected_;
	static std::once_flag wsaInitFlag_;
	static void initWsa();
};

} //namespace coldstorage
