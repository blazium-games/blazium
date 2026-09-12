/**************************************************************************/
/*  tls_cert.cpp                                                          */
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

#include "common/net/tls_cert.h"

#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace coldstorage {

std::string normalizeFingerprint(const std::string &fp) {
	std::string out;
	out.reserve(fp.size());
	for (unsigned char c : fp) {
		if (std::isxdigit(c)) {
			out.push_back(static_cast<char>(std::tolower(c)));
		}
	}
	return out;
}

std::string getCertFingerprint(const std::string &certPath) {
	mbedtls_x509_crt crt;
	mbedtls_x509_crt_init(&crt);
	if (mbedtls_x509_crt_parse_file(&crt, certPath.c_str()) != 0) {
		mbedtls_x509_crt_free(&crt);
		return {};
	}
	unsigned char hash[32];
	if (mbedtls_sha256(crt.raw.p, crt.raw.len, hash, 0) != 0) {
		mbedtls_x509_crt_free(&crt);
		return {};
	}
	mbedtls_x509_crt_free(&crt);
	std::ostringstream oss;
	static const char *digits = "0123456789abcdef";
	for (int i = 0; i < 32; ++i) {
		if (i) {
			oss << ':';
		}
		oss << digits[(hash[i] >> 4) & 0xf] << digits[hash[i] & 0xf];
	}
	return oss.str();
}

bool generateSelfSignedCert(const std::string &, const std::string &, const std::string &, int) {
	return false;
}

bool generateCaCert(const std::string &, const std::string &, int) {
	return false;
}

bool generateSignedCert(const std::string &, const std::string &, const std::string &,
		const std::string &, const std::string &, int, bool) {
	return false;
}

std::optional<std::time_t> getCertExpiry(const std::string &certPath) {
	mbedtls_x509_crt crt;
	mbedtls_x509_crt_init(&crt);
	if (mbedtls_x509_crt_parse_file(&crt, certPath.c_str()) != 0) {
		mbedtls_x509_crt_free(&crt);
		return std::nullopt;
	}

	mbedtls_x509_crt_free(&crt);
	return std::nullopt;
}

int daysUntilExpiry(const std::string &) {
	return 0;
}

bool isCertExpired(const std::string &certPath) {
	return daysUntilExpiry(certPath) < 0;
}

std::string formatCertExpiryDate(const std::string &) {
	return {};
}

} //namespace coldstorage
