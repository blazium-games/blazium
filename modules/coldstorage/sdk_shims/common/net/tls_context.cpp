/**************************************************************************/
/*  tls_context.cpp                                                       */
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

#include "common/net/tls_context.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <cstring>

namespace coldstorage {

namespace {

struct DrbgHolders {
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	bool ready = false;

	DrbgHolders() {
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		const char *pers = "coldstorage-blazium";
		if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
					reinterpret_cast<const unsigned char *>(pers), std::strlen(pers)) == 0) {
			ready = true;
		}
	}
	~DrbgHolders() {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
	}
};

DrbgHolders &drbg() {
	static DrbgHolders h;
	return h;
}

} //namespace

void TLSContext::initLibraries() {
	(void)drbg();
}

TLSContext::TLSContext() = default;

TLSContext::~TLSContext() {
	freeContext();
}

void TLSContext::freeContext() {
	if (conf_) {
		mbedtls_ssl_config_free(static_cast<mbedtls_ssl_config *>(conf_));
		delete static_cast<mbedtls_ssl_config *>(conf_);
		conf_ = nullptr;
	}
	if (cacert_) {
		mbedtls_x509_crt_free(static_cast<mbedtls_x509_crt *>(cacert_));
		delete static_cast<mbedtls_x509_crt *>(cacert_);
		cacert_ = nullptr;
	}
	if (clicert_) {
		mbedtls_x509_crt_free(static_cast<mbedtls_x509_crt *>(clicert_));
		delete static_cast<mbedtls_x509_crt *>(clicert_);
		clicert_ = nullptr;
	}
	if (pkey_) {
		mbedtls_pk_free(static_cast<mbedtls_pk_context *>(pkey_));
		delete static_cast<mbedtls_pk_context *>(pkey_);
		pkey_ = nullptr;
	}
}

bool TLSContext::configureServer(const std::string &certFile, const std::string &keyFile,
		const std::string &caFile) {
	freeContext();
	initLibraries();
	if (!drbg().ready) {
		return false;
	}
	mode_ = Mode::Server;
	cert_file_ = certFile;
	key_file_ = keyFile;
	ca_file_ = caFile;

	auto *conf = new mbedtls_ssl_config;
	mbedtls_ssl_config_init(conf);
	if (mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
				MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
		mbedtls_ssl_config_free(conf);
		delete conf;
		return false;
	}
	mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &drbg().ctr_drbg);

	auto *clicert = new mbedtls_x509_crt;
	mbedtls_x509_crt_init(clicert);
	if (mbedtls_x509_crt_parse_file(clicert, certFile.c_str()) != 0) {
		mbedtls_x509_crt_free(clicert);
		delete clicert;
		mbedtls_ssl_config_free(conf);
		delete conf;
		return false;
	}
	auto *pkey = new mbedtls_pk_context;
	mbedtls_pk_init(pkey);
	if (mbedtls_pk_parse_keyfile(pkey, keyFile.c_str(), nullptr, mbedtls_ctr_drbg_random, &drbg().ctr_drbg) != 0) {
		mbedtls_pk_free(pkey);
		delete pkey;
		mbedtls_x509_crt_free(clicert);
		delete clicert;
		mbedtls_ssl_config_free(conf);
		delete conf;
		return false;
	}
	mbedtls_ssl_conf_own_cert(conf, clicert, pkey);

	if (!caFile.empty()) {
		auto *cacert = new mbedtls_x509_crt;
		mbedtls_x509_crt_init(cacert);
		if (mbedtls_x509_crt_parse_file(cacert, caFile.c_str()) == 0) {
			mbedtls_ssl_conf_ca_chain(conf, cacert, nullptr);
			mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
			cacert_ = cacert;
		} else {
			mbedtls_x509_crt_free(cacert);
			delete cacert;
		}
	}

	conf_ = conf;
	clicert_ = clicert;
	pkey_ = pkey;
	return true;
}

bool TLSContext::configureClient(const std::string &caFile, bool verifyPeer,
		const std::string &certFile, const std::string &keyFile) {
	// Store/validate options only. TlsTransport::connect builds the mbedtls
	// config via setup_client_owned — avoid a dead duplicate conf_ here.
	freeContext();
	initLibraries();
	if (!drbg().ready) {
		return false;
	}
	if (verifyPeer && caFile.empty()) {
		return false;
	}
	if ((!certFile.empty() && keyFile.empty()) || (certFile.empty() && !keyFile.empty())) {
		return false;
	}
	mode_ = Mode::Client;
	ca_file_ = caFile;
	verify_peer_ = verifyPeer;
	cert_file_ = certFile;
	key_file_ = keyFile;
	return true;
}

} //namespace coldstorage
