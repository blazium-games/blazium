/**************************************************************************/
/*  crypto.h                                                              */
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

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace coldstorage {

std::string sha256(const std::string &data);
std::string sha256(const uint8_t *data, size_t len);

std::string sha256File(const std::string &path, int64_t *sizeOut = nullptr);

std::string sha256FileRange(const std::string &path, int64_t offset, int64_t length);

class Sha256Hasher {
public:
	Sha256Hasher();
	~Sha256Hasher();
	Sha256Hasher(const Sha256Hasher &) = delete;
	Sha256Hasher &operator=(const Sha256Hasher &) = delete;
	void update(const uint8_t *data, size_t len);
	void update(const char *data, size_t len);
	std::string finalHex();

private:
	struct State;
	State *state_ = nullptr;
};

class Sha256OStream : public std::ostream {
public:
	Sha256OStream();
	~Sha256OStream() override;
	std::string digest();

private:
	class Buf;
	Buf *buf_ = nullptr;
};

std::string hmac_sha256(const std::string &key, const std::string &data);

std::string hash_password(const std::string &password);
bool verify_password(const std::string &password, const std::string &hash);

std::string random_token(size_t bytes = 32);

} //namespace coldstorage
