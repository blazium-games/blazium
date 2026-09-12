/**************************************************************************/
/*  connection.cpp                                                        */
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

#include "client/sdk/connection.h"
#include "common/net/plain_transport.h"
#include "common/net/tls_context.h"
#include "common/net/tls_transport.h"
#include "common/util/net_trace.h"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace coldstorage {

namespace {

constexpr size_t kMaxStoredInstructions = 10000;

}

std::once_flag ServerConnection::wsaInitFlag_;

void ServerConnection::initWsa() {
#ifdef _WIN32
	std::call_once(wsaInitFlag_, []() {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	});
#endif
}

ServerConnection::ServerConnection() :
		connected_(false) {
	initWsa();
}

ServerConnection::~ServerConnection() {
	disconnect();
}

bool ServerConnection::connectTransport(const std::string &host, int port, const TlsOptions &tls) {
	CS_NET_TRACE("ServerConnection", "connectTransport begin host=" << host << " port=" << port << " tls=" << (tls.enabled ? "on" : "off"));

	struct addrinfo hints{}, *result;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	std::string portStr = std::to_string(port);
	CS_NET_TRACE("ServerConnection", "getaddrinfo begin");
	if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
		CS_NET_TRACE("ServerConnection", "getaddrinfo failed");
		return false;
	}
	CS_NET_TRACE("ServerConnection", "getaddrinfo ok");

	socket_t sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock == kInvalidSocket) {
		CS_NET_TRACE("ServerConnection", "socket() failed");
		freeaddrinfo(result);
		return false;
	}
	CS_NET_TRACE("ServerConnection", "socket() ok fd=" << static_cast<long long>(sock));

	CS_NET_TRACE("ServerConnection", "TCP connect begin");
	if (::connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) != 0) {
		CS_NET_TRACE("ServerConnection", "TCP connect failed");
#ifdef _WIN32
		closesocket(sock);
#else
		::close(sock);
#endif
		freeaddrinfo(result);
		return false;
	}
	CS_NET_TRACE("ServerConnection", "TCP connect ok");

	freeaddrinfo(result);

	if (tls.enabled) {
		CS_NET_TRACE("ServerConnection", "TLS configureClient begin");
		TLSContext ctx;
		if (!ctx.configureClient(tls.caFile, tls.verifyPeer, tls.certFile, tls.keyFile)) {
			CS_NET_TRACE("ServerConnection", "TLS configureClient failed");
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}
		CS_NET_TRACE("ServerConnection", "TlsTransport::connect begin");
		transport_ = TlsTransport::connect(sock, host, ctx);
		if (!transport_) {
			CS_NET_TRACE("ServerConnection", "TlsTransport::connect failed");
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}
		CS_NET_TRACE("ServerConnection", "TlsTransport::connect ok");
	} else {
		CS_NET_TRACE("ServerConnection", "plain transport");
		transport_ = makePlainTransport(sock);
	}

	connected_ = true;
	CS_NET_TRACE("ServerConnection", "connectTransport ok");
	return true;
}

bool ServerConnection::connect(const std::string &host, int port, const TlsOptions &tls) {
	CS_NET_TRACE("ServerConnection", "connect begin");
	if (!connectTransport(host, port, tls)) {
		return false;
	}
	CS_NET_TRACE("ServerConnection", "negotiate begin");
	const bool ok = negotiate();
	CS_NET_TRACE("ServerConnection", "negotiate " << (ok ? "ok" : "failed"));
	if (!ok) {
		disconnect();
	}
	return ok;
}

std::string ServerConnection::peerFingerprint() const {
	if (!transport_) {
		return {};
	}
	return transport_->peerFingerprint();
}

void ServerConnection::disconnect() {
	if (transport_) {
		transport_->close();
		transport_.reset();
	}
	connected_ = false;
}

bool ServerConnection::isConnected() const {
	return connected_ && transport_ != nullptr;
}

bool ServerConnection::negotiate() {
	if (!transport_) {
		CS_NET_TRACE("ServerConnection", "negotiate failed: no transport");
		return false;
	}
	transport_->setRecvTimeout(30);
	CS_NET_TRACE("ServerConnection", "negotiate send protocol request");

	ProtocolInfo info;
	info.capabilities = { "upload_resume", "download_resume" };
	auto msg = makeProtocolRequest(info);
	if (!sendMessage(msg)) {
		CS_NET_TRACE("ServerConnection", "negotiate sendMessage failed");
		return false;
	}
	CS_NET_TRACE("ServerConnection", "negotiate waiting for ack");

	std::vector<uint8_t> frameData;
	if (!readFrame(*transport_, frameData)) {
		CS_NET_TRACE("ServerConnection", "negotiate readFrame failed");
		return false;
	}
	auto ack = Message::deserialize(frameData);
	if (!ack || ack->func != "protocol") {
		CS_NET_TRACE("ServerConnection", "negotiate ack invalid");
		return false;
	}

	if (ack->args.value("type", "") == "error" ||
			ack->args.contains("error")) {
		CS_NET_TRACE("ServerConnection", "negotiate rejected by server");
		return false;
	}
	const int peerVer = ack->args.value("version", -1);
	if (peerVer != coldstorage::version::PROTOCOL_VERSION) {
		CS_NET_TRACE("ServerConnection", "negotiate version mismatch peer=" << peerVer);
		return false;
	}
	CS_NET_TRACE("ServerConnection", "negotiate ack ok");
	return true;
}

bool ServerConnection::sendMessage(const Message &msg) {
	CS_NET_TRACE("ServerConnection", "sendMessage func=" << msg.func << " bytes=pending");
	auto data = msg.serialize();
	CS_NET_TRACE("ServerConnection", "sendMessage func=" << msg.func << " bytes=" << data.size());
	return writeFrame(*transport_, data);
}

std::optional<Message> ServerConnection::readMessage() {
	std::vector<uint8_t> frameData;
	if (!readFrame(*transport_, frameData)) {
		return std::nullopt;
	}
	return Message::deserialize(frameData);
}

bool ServerConnection::sendInstruction(const Instruction &instr) {
	auto data = instr.serialize();
	return writeFrame(*transport_, data);
}

std::optional<Instruction> ServerConnection::readInstruction() {
	std::vector<uint8_t> frameData;
	if (!readFrame(*transport_, frameData)) {
		return std::nullopt;
	}
	return Instruction::deserialize(frameData);
}

ServerConnection::CommandResult ServerConnection::executeCommand(const Message &cmd) {
	CommandResult result;
	if (!sendMessage(cmd)) {
		result.error = "Failed to send command";
		return result;
	}

	while (true) {
		auto instr = readInstruction();
		if (!instr) {
			result.error = "Connection lost";
			return result;
		}

		if (result.instructions.size() < kMaxStoredInstructions) {
			result.instructions.push_back(*instr);
		} else {
			result.instructionsTruncated = true;
		}

		if (instr->op == InstructionOp::Release) {
			result.data = instr->data;
			result.success = instr->data.value("success", false);
			return result;
		} else if (instr->op == InstructionOp::Error) {
			result.error = instr->data.value("text", "Unknown error");
		}
	}
}

} //namespace coldstorage
