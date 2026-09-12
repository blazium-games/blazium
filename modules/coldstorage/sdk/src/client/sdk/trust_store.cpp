/**************************************************************************/
/*  trust_store.cpp                                                       */
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

#include "client/sdk/trust_store.h"
#include "common/util/sanitize.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace fs = std::filesystem;

namespace coldstorage {

namespace {

std::string nowIso8601() {
	auto now = std::chrono::system_clock::now();
	auto t = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	return oss.str();
}

std::string defaultTrustPath() {
	fs::path home;
	if (auto envHome = sanitize::safeGetenv("HOME"); !envHome.empty()) {
		home = envHome;
	} else if (auto envProfile = sanitize::safeGetenv("USERPROFILE"); !envProfile.empty()) {
		home = envProfile;
	} else {
		home = fs::temp_directory_path();
	}
	fs::path dir = home / ".cstorage";
	fs::create_directories(dir);
	return (dir / "known_hosts.json").string();
}

std::string lowerHost(const std::string &host) {
	std::string h = sanitize::trim(host);
	std::transform(h.begin(), h.end(), h.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return h;
}

} //namespace

TrustStore::TrustStore() :
		path_(defaultTrustPath()) {}

TrustStore::TrustStore(std::string path) :
		path_(std::move(path)) {}

std::string TrustStore::makeKey(const std::string &host, int port) {
	return lowerHost(host) + ":" + std::to_string(port);
}

void TrustStore::load() const {
	if (loaded_) {
		return;
	}
	loaded_ = true;
	cache_.clear();
	cacheRaw_.clear();

	std::ifstream in(path_);
	if (!in.is_open()) {
		return;
	}

	try {
		nlohmann::json j;
		in >> j;
		if (!j.is_object()) {
			return;
		}
		for (auto it = j.begin(); it != j.end(); ++it) {
			if (!it.value().is_object()) {
				continue;
			}
			TrustEntry entry;
			entry.fingerprint = it.value().value("fingerprint", "");
			entry.firstSeen = it.value().value("first_seen", "");
			entry.lastSeen = it.value().value("last_seen", "");
			cache_.emplace_back(it.key(), entry);
		}
	} catch (...) {
	}
}

bool TrustStore::persist() const {
	nlohmann::json j = nlohmann::json::object();
	for (const auto &[key, entry] : cache_) {
		j[key] = {
			{ "fingerprint", entry.fingerprint },
			{ "first_seen", entry.firstSeen },
			{ "last_seen", entry.lastSeen }
		};
	}

	const std::string tmp = path_ + ".tmp";
	{
		std::ofstream out(tmp, std::ios::trunc);
		if (!out.is_open()) {
			return false;
		}
		out << j.dump(2);
		if (!out.good()) {
			return false;
		}
	}
#ifndef _WIN32
	fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write);
#endif
	std::error_code ec;
	fs::rename(tmp, path_, ec);
	if (ec) {
		fs::remove(tmp, ec);
		return false;
	}
	return true;
}

std::optional<TrustEntry> TrustStore::lookup(const std::string &host, int port) const {
	load();
	std::string key = makeKey(host, port);
	for (const auto &[k, entry] : cache_) {
		if (k == key) {
			return entry;
		}
	}
	return std::nullopt;
}

bool TrustStore::save(const std::string &host, int port, const std::string &fingerprint) {
	load();
	std::string key = makeKey(host, port);
	std::string now = nowIso8601();
	for (auto &[k, entry] : cache_) {
		if (k == key) {
			entry.fingerprint = fingerprint;
			entry.lastSeen = now;
			return persist();
		}
	}
	TrustEntry entry;
	entry.fingerprint = fingerprint;
	entry.firstSeen = now;
	entry.lastSeen = now;
	cache_.emplace_back(key, entry);
	return persist();
}

bool TrustStore::remove(const std::string &host, int port) {
	load();
	std::string key = makeKey(host, port);
	auto it = std::remove_if(cache_.begin(), cache_.end(),
			[&](const auto &p) { return p.first == key; });
	if (it == cache_.end()) {
		return false;
	}
	cache_.erase(it, cache_.end());
	return persist();
}

std::vector<std::pair<std::string, TrustEntry>> TrustStore::list() const {
	load();
	return cache_;
}

} //namespace coldstorage
