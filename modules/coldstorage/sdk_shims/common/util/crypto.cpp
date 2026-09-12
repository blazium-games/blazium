/**************************************************************************/
/*  crypto.cpp                                                            */
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

#include "common/util/crypto.h"

#include "core/crypto/crypto_core.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

namespace coldstorage {

namespace {

std::string to_hex(const uint8_t *data, size_t len) {
	std::ostringstream oss;
	for (size_t i = 0; i < len; ++i) {
		oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
	}
	return oss.str();
}

} //namespace

std::string sha256(const std::string &data) {
	return sha256(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

std::string sha256(const uint8_t *data, size_t len) {
	unsigned char hash[32];
	CryptoCore::sha256(data, len, hash);
	return to_hex(hash, 32);
}

std::string sha256File(const std::string &path, int64_t *sizeOut) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		throw std::runtime_error("Cannot open file for hash: " + path);
	}
	CryptoCore::SHA256Context ctx;
	ctx.start();
	char buf[64 * 1024];
	int64_t total = 0;
	while (in) {
		in.read(buf, sizeof(buf));
		auto got = static_cast<size_t>(in.gcount());
		if (got == 0) {
			break;
		}
		ctx.update(reinterpret_cast<const uint8_t *>(buf), got);
		total += static_cast<int64_t>(got);
	}
	unsigned char hash[32];
	ctx.finish(hash);
	if (sizeOut) {
		*sizeOut = total;
	}
	return to_hex(hash, 32);
}

std::string sha256FileRange(const std::string &path, int64_t offset, int64_t length) {
	if (offset < 0 || length < 0) {
		throw std::runtime_error("Invalid hash range");
	}
	if (length == 0) {
		return sha256(std::string{});
	}
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		throw std::runtime_error("Cannot open file for hash range: " + path);
	}
	in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	if (!in) {
		throw std::runtime_error("Cannot seek for hash range: " + path);
	}
	CryptoCore::SHA256Context ctx;
	ctx.start();
	char buf[64 * 1024];
	int64_t remaining = length;
	while (remaining > 0 && in) {
		auto chunk = static_cast<std::streamsize>(std::min<int64_t>(remaining, sizeof(buf)));
		in.read(buf, chunk);
		auto got = static_cast<size_t>(in.gcount());
		if (got == 0) {
			break;
		}
		ctx.update(reinterpret_cast<const uint8_t *>(buf), got);
		remaining -= static_cast<int64_t>(got);
	}
	if (remaining != 0) {
		throw std::runtime_error("Short read for hash range: " + path);
	}
	unsigned char hash[32];
	ctx.finish(hash);
	return to_hex(hash, 32);
}

struct Sha256Hasher::State {
	CryptoCore::SHA256Context ctx;
	bool started = false;
	bool finished = false;
};

Sha256Hasher::Sha256Hasher() {
	state_ = new State;
	state_->ctx.start();
	state_->started = true;
}

Sha256Hasher::~Sha256Hasher() {
	delete state_;
	state_ = nullptr;
}

void Sha256Hasher::update(const uint8_t *data, size_t len) {
	if (!state_ || state_->finished || !data || len == 0) {
		return;
	}
	state_->ctx.update(data, len);
}

void Sha256Hasher::update(const char *data, size_t len) {
	update(reinterpret_cast<const uint8_t *>(data), len);
}

std::string Sha256Hasher::finalHex() {
	if (!state_ || state_->finished) {
		return {};
	}
	unsigned char hash[32];
	state_->ctx.finish(hash);
	state_->finished = true;
	return to_hex(hash, 32);
}

class Sha256OStream::Buf : public std::streambuf {
public:
	Sha256Hasher hasher;
	std::string digest;
	bool finalized = false;

	int overflow(int ch) override {
		if (ch == traits_type::eof()) {
			return traits_type::not_eof(ch);
		}
		char c = static_cast<char>(ch);
		hasher.update(&c, 1);
		return ch;
	}
	std::streamsize xsputn(const char *s, std::streamsize n) override {
		if (s && n > 0) {
			hasher.update(s, static_cast<size_t>(n));
		}
		return n;
	}
};

Sha256OStream::Sha256OStream() :
		std::ostream(nullptr) {
	buf_ = new Buf;
	rdbuf(buf_);
}

Sha256OStream::~Sha256OStream() {
	delete buf_;
	buf_ = nullptr;
}

std::string Sha256OStream::digest() {
	if (!buf_) {
		return {};
	}
	if (!buf_->finalized) {
		flush();
		buf_->digest = buf_->hasher.finalHex();
		buf_->finalized = true;
	}
	return buf_->digest;
}

std::string hmac_sha256(const std::string &key, const std::string &data) {
	unsigned char out[32];
	const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if (!info) {
		return {};
	}
	mbedtls_md_hmac(info, reinterpret_cast<const unsigned char *>(key.data()), key.size(),
			reinterpret_cast<const unsigned char *>(data.data()), data.size(), out);
	return to_hex(out, 32);
}

std::string hash_password(const std::string &password) {
	return "cs$" + sha256(password);
}

bool verify_password(const std::string &password, const std::string &hash) {
	if (hash.rfind("cs$", 0) == 0) {
		return hash == ("cs$" + sha256(password));
	}
	return false;
}

std::string random_token(size_t bytes) {
	std::vector<uint8_t> buf(bytes);
	CryptoCore::RandomGenerator rng;
	if (rng.init() != OK || rng.get_random_bytes(buf.data(), bytes) != OK) {
		throw std::runtime_error("random_token failed");
	}
	return to_hex(buf.data(), bytes);
}

} //namespace coldstorage
