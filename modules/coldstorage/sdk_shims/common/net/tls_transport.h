/**************************************************************************/
/*  tls_transport.h                                                       */
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
#include "common/net/stream_transport.h"
#include "common/net/tls_context.h"
#include <optional>
#include <string>

namespace coldstorage {

class TlsTransport : public StreamTransport {
public:
	static TransportPtr accept(socket_t sock, const TLSContext &ctx);
	static TransportPtr connect(socket_t sock, const std::string &host, const TLSContext &ctx);

	~TlsTransport() override;

	TlsTransport(const TlsTransport &) = delete;
	TlsTransport &operator=(const TlsTransport &) = delete;

	bool readAll(uint8_t *data, size_t len) override;
	bool writeAll(const uint8_t *data, size_t len) override;
	void close() override;
	bool setRecvTimeout(int seconds) override;
	socket_t rawSocket() const override { return sock_; }
	std::string peerFingerprint() const override;
	std::optional<std::string> peerCertIdentity(const std::string &field) const override;

private:
	TlsTransport(socket_t sock, void *ssl, void *conf, void *cacert, void *clicert, void *pkey);

	socket_t sock_;
	void *ssl_ = nullptr;
	void *conf_ = nullptr;
	void *cacert_ = nullptr;
	void *clicert_ = nullptr;
	void *pkey_ = nullptr;
	int recv_timeout_ms_ = 0;
};

} //namespace coldstorage
