/**************************************************************************/
/*  sync_target.cpp                                                       */
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

#include "common/util/sync_target.h"
#include "common/util/sanitize.h"
#include <cctype>

namespace coldstorage {
namespace {

bool parsePositiveInt(const std::string &s, int64_t &out) {
	if (s.empty()) {
		return false;
	}
	try {
		size_t idx = 0;
		long long v = std::stoll(s, &idx, 10);
		if (idx != s.size() || v <= 0) {
			return false;
		}
		out = static_cast<int64_t>(v);
		return true;
	} catch (...) {
		return false;
	}
}

bool parseRevSuffix(const std::string &suffix, SyncTarget &out) {
	if (suffix.empty() || suffix == "#head") {
		out.revKind = SyncRevKind::HeadChange;
		out.revNum = 0;
		return true;
	}
	if (suffix[0] == '@') {
		int64_t n = 0;
		if (!parsePositiveInt(suffix.substr(1), n)) {
			out.error = "Invalid changelist in target: " + suffix;
			return false;
		}
		out.revKind = SyncRevKind::AtChange;
		out.revNum = n;
		return true;
	}
	if (suffix[0] == '#') {
		if (suffix == "#head") {
			out.revKind = SyncRevKind::HeadChange;
			out.revNum = 0;
			return true;
		}
		int64_t n = 0;
		if (!parsePositiveInt(suffix.substr(1), n)) {
			out.error = "Invalid revision in target: " + suffix;
			return false;
		}
		out.revKind = SyncRevKind::FileRev;
		out.revNum = n;
		return true;
	}
	out.error = "Invalid revision suffix: " + suffix;
	return false;
}

bool setPathScope(const std::string &pathPart, SyncTarget &out) {
	if (pathPart.empty()) {
		return true;
	}
	std::string path = pathPart;
	if (path.size() >= 3 && path.compare(path.size() - 3, 3, "...") == 0) {
		out.exactPath = false;
		out.pathPrefix = path.substr(0, path.size() - 3);
		if (out.pathPrefix.size() < 3 || out.pathPrefix.rfind("//", 0) != 0) {
			out.error = "Invalid path in target: " + pathPart;
			return false;
		}
		if (out.pathPrefix.find("/../") != std::string::npos ||
				out.pathPrefix.find('\\') != std::string::npos) {
			out.error = "Invalid path in target: " + pathPart;
			return false;
		}
		return true;
	}
	if (!sanitize::isValidDepotPath(path)) {
		out.error = "Invalid depot path in target: " + pathPart;
		return false;
	}
	out.exactPath = true;
	out.pathPrefix = path;
	return true;
}

} //namespace

SyncTarget parseSyncTarget(const std::string &spec) {
	SyncTarget out;
	std::string s = sanitize::trim(spec);
	if (s.size() > 256) {
		out.error = "Target specification too long";
		return out;
	}
	if (s.empty() || s == "#head") {
		out.valid = true;
		out.revKind = SyncRevKind::HeadChange;
		return out;
	}
	if (s[0] == '@') {
		if (!parseRevSuffix(s, out)) {
			return out;
		}
		out.valid = true;
		return out;
	}
	if (s[0] == '#') {
		if (!parseRevSuffix(s, out)) {
			return out;
		}
		out.valid = true;
		return out;
	}
	if (s.rfind("//", 0) != 0) {
		out.error = "Invalid target specification: " + s;
		return out;
	}

	size_t revAt = std::string::npos;
	for (size_t i = s.size(); i > 2;) {
		--i;
		if (s[i] == '#' || s[i] == '@') {
			revAt = i;
			break;
		}
	}

	std::string pathPart = s;
	std::string revPart;
	if (revAt != std::string::npos) {
		pathPart = s.substr(0, revAt);
		revPart = s.substr(revAt);

		SyncTarget tmp;
		if (!parseRevSuffix(revPart, tmp)) {
			out.error = tmp.error;
			return out;
		}
		out.revKind = tmp.revKind;
		out.revNum = tmp.revNum;
	} else {
		out.revKind = SyncRevKind::HeadChange;
		out.revNum = 0;
	}

	if (pathPart.empty()) {
		out.error = "Invalid target specification: " + s;
		return out;
	}
	if (!setPathScope(pathPart, out)) {
		return out;
	}
	out.valid = true;
	return out;
}

bool syncPathMatches(const SyncTarget &target, const std::string &depotPath) {
	if (target.pathPrefix.empty()) {
		return true;
	}
	if (target.exactPath) {
		return depotPath == target.pathPrefix;
	}
	if (depotPath.size() < target.pathPrefix.size()) {
		return false;
	}
	return depotPath.compare(0, target.pathPrefix.size(), target.pathPrefix) == 0;
}

} //namespace coldstorage
