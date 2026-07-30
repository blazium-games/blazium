/**************************************************************************/
/*  sanitize.h                                                            */
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

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace coldstorage {
namespace sanitize {

inline std::string trim(const std::string &s) {
	auto start = s.find_first_not_of(" \t\n\r\f\v");
	if (start == std::string::npos) {
		return "";
	}
	auto end = s.find_last_not_of(" \t\n\r\f\v");
	return s.substr(start, end - start + 1);
}

inline std::string trimLeft(const std::string &s) {
	auto start = s.find_first_not_of(" \t\n\r\f\v");
	return (start == std::string::npos) ? "" : s.substr(start);
}

inline std::string trimRight(const std::string &s) {
	auto end = s.find_last_not_of(" \t\n\r\f\v");
	return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline std::string stripControlChars(const std::string &s) {
	std::string result;
	result.reserve(s.size());
	for (char c : s) {
		if (c == '\t' || c == '\n' || c == '\r') {
			result += c;
			continue;
		}
		if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7F) {
			result += c;
		}
	}
	return result;
}

inline std::string stripNonPrintable(const std::string &s) {
	std::string result;
	result.reserve(s.size());
	for (unsigned char c : s) {
		if (c >= 0x20 && c < 0x7F) {
			result += static_cast<char>(c);
		}
	}
	return result;
}

inline bool checkMaxLength(const std::string &s, size_t maxLen) {
	return s.size() <= maxLen;
}

inline std::string truncate(const std::string &s, size_t maxLen) {
	return s.size() <= maxLen ? s : s.substr(0, maxLen);
}

inline bool isValidName(const std::string &s, size_t minLen = 1, size_t maxLen = 128) {
	if (s.size() < minLen || s.size() > maxLen) {
		return false;
	}
	if (s.empty()) {
		return false;
	}

	if (!std::isalnum(static_cast<unsigned char>(s[0]))) {
		return false;
	}

	if (!std::isalnum(static_cast<unsigned char>(s.back()))) {
		return false;
	}

	for (char c : s) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') {
			return false;
		}
	}

	for (size_t i = 1; i < s.size(); i++) {
		if (!std::isalnum(static_cast<unsigned char>(s[i])) &&
				!std::isalnum(static_cast<unsigned char>(s[i - 1]))) {
			return false;
		}
	}
	return true;
}

inline std::string sanitizeName(const std::string &s, size_t maxLen = 128) {
	std::string result;
	result.reserve(std::min(s.size(), maxLen));
	for (char c : s) {
		if (result.size() >= maxLen) {
			break;
		}
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') {
			result += c;
		}
	}

	while (!result.empty() && !std::isalnum(static_cast<unsigned char>(result.front()))) {
		result.erase(result.begin());
	}
	while (!result.empty() && !std::isalnum(static_cast<unsigned char>(result.back()))) {
		result.pop_back();
	}
	return result;
}

inline bool hasDotDotSegment(const std::string &path) {
	if (path.size() < 3) {
		return false;
	}
	size_t i = 2;
	while (i < path.size()) {
		size_t j = path.find('/', i);
		if (j == std::string::npos) {
			j = path.size();
		}
		std::string seg = trim(path.substr(i, j - i));
		if (seg == ".." || seg == ".") {
			return true;
		}
		i = j + 1;
	}
	return false;
}

inline bool hasEmptySegment(const std::string &path) {
	if (path.size() < 3) {
		return false;
	}
	size_t i = 2;
	while (i < path.size()) {
		size_t j = path.find('/', i);
		if (j == std::string::npos) {
			j = path.size();
		}

		if (j == i) {
			return true;
		}
		std::string seg = trim(path.substr(i, j - i));
		if (seg.empty()) {
			return true;
		}
		i = j + 1;
	}
	return false;
}

inline bool isReservedDepotPath(const std::string &path) {
	if (path == "//.shelf" || path == "//.cs.shelf") {
		return true;
	}
	if (path.rfind("//.shelf/", 0) == 0) {
		return true;
	}
	if (path.rfind("//.cs.shelf/", 0) == 0) {
		return true;
	}
	return false;
}

inline bool isHexDigest64(const std::string &s) {
	if (s.size() != 64) {
		return false;
	}
	for (unsigned char c : s) {
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
					(c >= 'A' && c <= 'F'))) {
			return false;
		}
	}
	return true;
}

inline bool isShelfContentPath(const std::string &path) {
	static constexpr const char kPrefix[] = "//.cs.shelf/";
	constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
	if (path.size() <= kPrefixLen || path.compare(0, kPrefixLen, kPrefix) != 0) {
		return false;
	}
	if (path.find('\0') != std::string::npos) {
		return false;
	}
	if (path.find('\\') != std::string::npos) {
		return false;
	}
	if (path.find("//", 2) != std::string::npos) {
		return false;
	}
	if (hasDotDotSegment(path) || hasEmptySegment(path)) {
		return false;
	}

	std::string rest = path.substr(kPrefixLen);
	auto slash = rest.find('/');
	if (slash == std::string::npos || slash == 0) {
		return false;
	}
	std::string name = rest.substr(0, slash);
	std::string hex = rest.substr(slash + 1);
	if (hex.find('/') != std::string::npos) {
		return false;
	}
	if (!isValidName(name) || !isHexDigest64(hex)) {
		return false;
	}
	return true;
}

inline std::string collapseDepotSlashes(const std::string &path) {
	if (path.size() < 2) {
		return path;
	}
	std::string out;
	out.reserve(path.size());
	out.push_back(path[0]);
	out.push_back(path[1]);
	for (size_t i = 2; i < path.size(); ++i) {
		if (path[i] == '/' && !out.empty() && out.back() == '/') {
			continue;
		}
		out.push_back(path[i]);
	}
	return out;
}

inline bool isWindowsReservedSegment(const std::string &seg) {
	if (seg.empty()) {
		return false;
	}

	std::string base = seg;
	while (!base.empty() && (base.back() == ' ' || base.back() == '.')) {
		base.pop_back();
	}
	auto dot = base.find('.');
	if (dot != std::string::npos) {
		base = base.substr(0, dot);
	}
	for (char &c : base) {
		if (c >= 'a' && c <= 'z') {
			c = static_cast<char>(c - 'a' + 'A');
		}
	}
	static const char *kNames[] = {
		"CON",
		"PRN",
		"AUX",
		"NUL",
		"COM1",
		"COM2",
		"COM3",
		"COM4",
		"COM5",
		"COM6",
		"COM7",
		"COM8",
		"COM9",
		"LPT1",
		"LPT2",
		"LPT3",
		"LPT4",
		"LPT5",
		"LPT6",
		"LPT7",
		"LPT8",
		"LPT9",
	};
	for (const char *n : kNames) {
		if (base == n) {
			return true;
		}
	}
	return false;
}

inline bool hasWindowsReservedDepotSegment(const std::string &path) {
	if (path.size() < 3) {
		return false;
	}
	size_t i = 2;
	while (i < path.size()) {
		size_t j = path.find('/', i);
		if (j == std::string::npos) {
			j = path.size();
		}
		if (j > i && isWindowsReservedSegment(path.substr(i, j - i))) {
			return true;
		}
		i = j + 1;
	}
	return false;
}

inline bool isSafeProtectPath(const std::string &path) {
	if (path.size() < 3 || path.compare(0, 2, "//") != 0) {
		return false;
	}
	if (path.find("//", 2) != std::string::npos) {
		return false;
	}

	if (path == "//*" || path == "//..." || path == "//?") {
		return false;
	}
	if (path.size() >= 3 && path.compare(path.size() - 3, 3, "...") == 0) {
		std::string prefix = path.substr(0, path.size() - 3);
		if (prefix.size() <= 2) {
			return false;
		}
		if (prefix.back() != '/') {
			return false;
		}
		return true;
	}

	if (path.find('/', 2) == std::string::npos) {
		return false;
	}
	return true;
}

inline bool isValidDepotPath(const std::string &path) {
	if (path.size() < 3 || path.size() > 4096) {
		return false;
	}
	if (path.substr(0, 2) != "//") {
		return false;
	}

	if (path.find('\0') != std::string::npos) {
		return false;
	}

	if (path.find('\\') != std::string::npos) {
		return false;
	}
	if (path.find(':') != std::string::npos) {
		return false;
	}

	if (path.find("//", 2) != std::string::npos) {
		return false;
	}

	if (path.find("/../") != std::string::npos) {
		return false;
	}
	if (path.find("/./") != std::string::npos) {
		return false;
	}
	if (hasDotDotSegment(path)) {
		return false;
	}
	if (hasEmptySegment(path)) {
		return false;
	}
	if (hasWindowsReservedDepotSegment(path)) {
		return false;
	}

	if (path.size() <= 2 || path[2] == '/') {
		return false;
	}

	for (size_t i = 2; i < path.size(); i++) {
		unsigned char c = static_cast<unsigned char>(path[i]);
		if (c < 0x20 || c == 0x7F) {
			return false;
		}
	}
	if (isReservedDepotPath(path)) {
		return false;
	}
	return true;
}

inline bool isAllowedContentDepotPath(const std::string &path) {
	return isValidDepotPath(path) || isShelfContentPath(path);
}

inline bool isValidRole(const std::string &role) {
	return role == "owner" || role == "admin" || role == "write" ||
			role == "read" || role == "none";
}

inline bool isValidPort(int port) {
	return port >= 1 && port <= 65535;
}

inline int clampPort(int port) {
	if (port < 1) {
		return 1666;
	}
	if (port > 65535) {
		return 1666;
	}
	return port;
}

inline int clampInt(int val, int minVal, int maxVal) {
	if (val < minVal) {
		return minVal;
	}
	if (val > maxVal) {
		return maxVal;
	}
	return val;
}

inline size_t clampSize(size_t val, size_t minVal, size_t maxVal) {
	if (val < minVal) {
		return minVal;
	}
	if (val > maxVal) {
		return maxVal;
	}
	return val;
}

inline bool isValidHost(const std::string &host) {
	if (host.empty() || host.size() > 253) {
		return false;
	}

	for (unsigned char c : host) {
		if (c < 0x20 || c == 0x7F) {
			return false;
		}
		if (c == ';' || c == '|' || c == '&' || c == '$' || c == '`') {
			return false;
		}
		if (c == '\'' || c == '"' || c == '(' || c == ')') {
			return false;
		}
	}
	return true;
}

inline bool isValidJWTFormat(const std::string &token) {
	if (token.empty() || token.size() > 16384) {
		return false;
	}

	int dots = 0;
	for (char c : token) {
		if (c == '.') {
			dots++;
		}

		if (!std::isalnum(static_cast<unsigned char>(c)) &&
				c != '-' && c != '_' && c != '.' && c != '=') {
			return false;
		}
	}
	return dots == 2;
}

inline std::string sanitizeText(const std::string &s, size_t maxLen = 65536) {
	std::string result = stripControlChars(s);
	return truncate(result, maxLen);
}

inline bool isValidPassword(const std::string &pw) {
	return pw.size() >= 1 && pw.size() <= 1024;
}

inline std::string safeGetenv(const char *name, size_t maxLen = 4096) {
	const char *val = std::getenv(name);
	if (!val) {
		return "";
	}
	std::string s(val);
	return truncate(trim(s), maxLen);
}

} //namespace sanitize
} //namespace coldstorage
