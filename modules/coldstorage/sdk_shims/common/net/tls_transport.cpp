/**************************************************************************/
/*  tls_transport.cpp                                                     */
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

#include "common/net/tls_transport.h"
#include "common/net/tls_cert.h"
#include "common/util/net_trace.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace coldstorage {

namespace {

struct Drbg {
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr;
	bool ok = false;
	Drbg() {
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr);
		const char *pers = "cs-tls-transport";
		ok = mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy,
					 reinterpret_cast<const unsigned char *>(pers), std::strlen(pers)) == 0;
	}
	~Drbg() {
		mbedtls_ctr_drbg_free(&ctr);
		mbedtls_entropy_free(&entropy);
	}
};

Drbg &drbg() {
	static Drbg d;
	return d;
}

int bio_send(void *ctx, const unsigned char *buf, size_t len) {
	socket_t sock = *static_cast<socket_t *>(ctx);
#ifdef _WIN32
	int n = ::send(sock, reinterpret_cast<const char *>(buf), static_cast<int>(len), 0);
#else
	ssize_t n = ::send(sock, buf, len, 0);
#endif
	if (n < 0) {
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}
	return static_cast<int>(n);
}

int bio_recv(void *ctx, unsigned char *buf, size_t len) {
	socket_t sock = *static_cast<socket_t *>(ctx);
#ifdef _WIN32
	int n = ::recv(sock, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
	ssize_t n = ::recv(sock, buf, len, 0);
#endif
	if (n < 0) {
		return MBEDTLS_ERR_SSL_WANT_READ;
	}
	if (n == 0) {
		return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
	}
	return static_cast<int>(n);
}

struct OwnedTls {
	mbedtls_ssl_config conf;
	mbedtls_x509_crt cacert;
	mbedtls_x509_crt clicert;
	mbedtls_pk_context pkey;
	bool has_ca = false;
	bool has_cli = false;
	socket_t sock_holder = kInvalidSocket;

	OwnedTls() {
		mbedtls_ssl_config_init(&conf);
		mbedtls_x509_crt_init(&cacert);
		mbedtls_x509_crt_init(&clicert);
		mbedtls_pk_init(&pkey);
	}
	~OwnedTls() {
		mbedtls_pk_free(&pkey);
		mbedtls_x509_crt_free(&clicert);
		mbedtls_x509_crt_free(&cacert);
		mbedtls_ssl_config_free(&conf);
	}
};

bool setup_client_owned(OwnedTls &o, const TLSContext &ctx) {
	if (!drbg().ok) {
		return false;
	}
	if (ctx.verifyPeer() && ctx.caFile().empty()) {
		return false;
	}
	if (mbedtls_ssl_config_defaults(&o.conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
				MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
		return false;
	}
	mbedtls_ssl_conf_rng(&o.conf, mbedtls_ctr_drbg_random, &drbg().ctr);
	mbedtls_ssl_conf_authmode(&o.conf, ctx.verifyPeer() ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
	if (!ctx.caFile().empty()) {
		if (mbedtls_x509_crt_parse_file(&o.cacert, ctx.caFile().c_str()) != 0) {
			return false;
		}
		mbedtls_ssl_conf_ca_chain(&o.conf, &o.cacert, nullptr);
		o.has_ca = true;
	}
	if (!ctx.certFile().empty() && !ctx.keyFile().empty()) {
		if (mbedtls_x509_crt_parse_file(&o.clicert, ctx.certFile().c_str()) != 0) {
			return false;
		}
		if (mbedtls_pk_parse_keyfile(&o.pkey, ctx.keyFile().c_str(), nullptr, mbedtls_ctr_drbg_random, &drbg().ctr) != 0) {
			return false;
		}
		mbedtls_ssl_conf_own_cert(&o.conf, &o.clicert, &o.pkey);
		o.has_cli = true;
	}
	return true;
}

} //namespace

TlsTransport::TlsTransport(socket_t sock, void *ssl, void *conf, void *cacert, void *clicert, void *pkey) :
		sock_(sock), ssl_(ssl), conf_(conf), cacert_(cacert), clicert_(clicert), pkey_(pkey) {}

TlsTransport::~TlsTransport() {
	close();
}

TransportPtr TlsTransport::accept(socket_t, const TLSContext &) {
	return nullptr;
}

TransportPtr TlsTransport::connect(socket_t sock, const std::string &host, const TLSContext &ctx) {
	if (ctx.mode() != TLSContext::Mode::Client) {
		return nullptr;
	}
	auto *owned = new OwnedTls;
	if (!setup_client_owned(*owned, ctx)) {
		delete owned;
		return nullptr;
	}
	auto *ssl = new mbedtls_ssl_context;
	mbedtls_ssl_init(ssl);
	if (mbedtls_ssl_setup(ssl, &owned->conf) != 0) {
		mbedtls_ssl_free(ssl);
		delete ssl;
		delete owned;
		return nullptr;
	}
	if (!host.empty()) {
		if (mbedtls_ssl_set_hostname(ssl, host.c_str()) != 0) {
			CS_NET_TRACE("TlsTransport", "set_hostname failed");
			mbedtls_ssl_free(ssl);
			delete ssl;
			delete owned;
			return nullptr;
		}
	}
	owned->sock_holder = sock;
	mbedtls_ssl_set_bio(ssl, &owned->sock_holder, bio_send, bio_recv, nullptr);

	int ret;
	while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			CS_NET_TRACE("TlsTransport", "handshake failed ret=" << ret);
			mbedtls_ssl_free(ssl);
			delete ssl;
			delete owned;
			return nullptr;
		}
	}

	return TransportPtr(new TlsTransport(sock, ssl, owned, nullptr, nullptr, nullptr));
}

bool TlsTransport::readAll(uint8_t *data, size_t len) {
	size_t got = 0;
	auto *ssl = static_cast<mbedtls_ssl_context *>(ssl_);
	while (got < len) {
		int n = mbedtls_ssl_read(ssl, data + got, len - got);
		if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (n <= 0) {
			return false;
		}
		got += static_cast<size_t>(n);
	}
	return true;
}

bool TlsTransport::writeAll(const uint8_t *data, size_t len) {
	size_t sent = 0;
	auto *ssl = static_cast<mbedtls_ssl_context *>(ssl_);
	while (sent < len) {
		int n = mbedtls_ssl_write(ssl, data + sent, len - sent);
		if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (n <= 0) {
			return false;
		}
		sent += static_cast<size_t>(n);
	}
	return true;
}

void TlsTransport::close() {
	if (ssl_) {
		mbedtls_ssl_close_notify(static_cast<mbedtls_ssl_context *>(ssl_));
		mbedtls_ssl_free(static_cast<mbedtls_ssl_context *>(ssl_));
		delete static_cast<mbedtls_ssl_context *>(ssl_);
		ssl_ = nullptr;
	}
	if (conf_) {
		delete static_cast<OwnedTls *>(conf_);
		conf_ = nullptr;
	}
	if (sock_ != kInvalidSocket) {
#ifdef _WIN32
		closesocket(sock_);
#else
		::close(sock_);
#endif
		sock_ = kInvalidSocket;
	}
}

bool TlsTransport::setRecvTimeout(int seconds) {
	recv_timeout_ms_ = seconds > 0 ? seconds * 1000 : 0;
#ifdef _WIN32
	DWORD ms = static_cast<DWORD>(recv_timeout_ms_);
	setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&ms), sizeof(ms));
#else
	timeval tv{};
	tv.tv_sec = seconds;
	setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
	return true;
}

std::string TlsTransport::peerFingerprint() const {
	if (!ssl_) {
		return {};
	}
	const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(static_cast<const mbedtls_ssl_context *>(ssl_));
	if (!crt) {
		return {};
	}
	unsigned char hash[32];
	if (mbedtls_sha256(crt->raw.p, crt->raw.len, hash, 0) != 0) {
		return {};
	}
	std::string hex;
	static const char *digits = "0123456789abcdef";
	for (int i = 0; i < 32; ++i) {
		if (i) {
			hex += ':';
		}
		hex += digits[(hash[i] >> 4) & 0xf];
		hex += digits[hash[i] & 0xf];
	}
	return hex;
}

std::optional<std::string> TlsTransport::peerCertIdentity(const std::string &field) const {
	if (!ssl_) {
		return std::nullopt;
	}
	const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(static_cast<const mbedtls_ssl_context *>(ssl_));
	if (!crt) {
		return std::nullopt;
	}
	char buf[256];
	if (field == "CN" || field == "cn") {
		if (mbedtls_x509_dn_gets(buf, sizeof(buf), &crt->subject) < 0) {
			return std::nullopt;
		}
		return std::string(buf);
	}
	return std::nullopt;
}

} //namespace coldstorage
