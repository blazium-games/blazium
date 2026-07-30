/**************************************************************************/
/*  trust_store.h                                                         */
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

#include <optional>
#include <string>
#include <vector>

namespace coldstorage {

struct TrustEntry {
	std::string fingerprint;
	std::string firstSeen;
	std::string lastSeen;
};

class TrustStore {
public:
	TrustStore();
	explicit TrustStore(std::string path);

	static std::string makeKey(const std::string &host, int port);
	std::optional<TrustEntry> lookup(const std::string &host, int port) const;
	bool save(const std::string &host, int port, const std::string &fingerprint);
	bool remove(const std::string &host, int port);
	std::vector<std::pair<std::string, TrustEntry>> list() const;

	const std::string &path() const { return path_; }

private:
	std::string path_;
	mutable bool loaded_ = false;

	void load() const;
	bool persist() const;

	mutable std::string cacheRaw_;
	mutable std::vector<std::pair<std::string, TrustEntry>> cache_;
};

} //namespace coldstorage
