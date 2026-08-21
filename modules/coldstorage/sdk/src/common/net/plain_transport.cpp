/**************************************************************************/
/*  plain_transport.cpp                                                   */
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

#include "common/net/plain_transport.h"

#include <climits>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace coldstorage {

PlainSocketTransport::PlainSocketTransport(socket_t sock) :
		sock_(sock) {}

PlainSocketTransport::~PlainSocketTransport() {
	close();
}

bool PlainSocketTransport::readAll(uint8_t *data, size_t len) {
	size_t totalRecv = 0;
	while (totalRecv < len) {
#ifdef _WIN32
		int toRecv = static_cast<int>(
				(len - totalRecv) > INT_MAX ? INT_MAX : (len - totalRecv));
		int received = ::recv(sock_, reinterpret_cast<char *>(data + totalRecv),
				toRecv, 0);
		if (received <= 0) {
			return false;
		}
#else
		ssize_t received = ::recv(sock_, data + totalRecv, len - totalRecv, 0);
		if (received <= 0) {
			return false;
		}
#endif
		totalRecv += static_cast<size_t>(received);
	}
	return true;
}

bool PlainSocketTransport::writeAll(const uint8_t *data, size_t len) {
	size_t totalSent = 0;
	while (totalSent < len) {
#ifdef _WIN32
		int toSend = static_cast<int>(
				(len - totalSent) > INT_MAX ? INT_MAX : (len - totalSent));
		int sent = ::send(sock_, reinterpret_cast<const char *>(data + totalSent),
				toSend, 0);
		if (sent == SOCKET_ERROR) {
			return false;
		}
#else
		ssize_t sent = ::send(sock_, data + totalSent, len - totalSent, MSG_NOSIGNAL);
		if (sent <= 0) {
			return false;
		}
#endif
		totalSent += static_cast<size_t>(sent);
	}
	return true;
}

void PlainSocketTransport::close() {
	if (sock_ != kInvalidSocket) {
#ifdef _WIN32
		closesocket(sock_);
#else
		::close(sock_);
#endif
		sock_ = kInvalidSocket;
	}
}

bool PlainSocketTransport::setRecvTimeout(int seconds) {
	if (sock_ == kInvalidSocket) {
		return false;
	}
#ifdef _WIN32
	DWORD timeout = static_cast<DWORD>(seconds) * 1000;
	return setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
				   reinterpret_cast<const char *>(&timeout), sizeof(timeout)) == 0;
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	return setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
				   reinterpret_cast<const char *>(&tv), sizeof(tv)) == 0;
#endif
}

socket_t PlainSocketTransport::release() {
	socket_t s = sock_;
	sock_ = kInvalidSocket;
	return s;
}

TransportPtr makePlainTransport(socket_t sock) {
	return std::make_unique<PlainSocketTransport>(sock);
}

} //namespace coldstorage
