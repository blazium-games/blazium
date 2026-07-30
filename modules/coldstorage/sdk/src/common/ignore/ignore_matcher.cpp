/**************************************************************************/
/*  ignore_matcher.cpp                                                    */
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

#include "common/ignore/ignore_matcher.h"
#include <algorithm>
#include <cctype>

namespace coldstorage {
namespace {

std::string normalizeSlashes(std::string s) {
	for (char &c : s) {
		if (c == '\\') {
			c = '/';
		}
	}
	while (!s.empty() && s.front() == '/') {
		s.erase(s.begin());
	}
	while (!s.empty() && s.back() == '/') {
		s.pop_back();
	}
	return s;
}

bool iequals(const std::string &a, const std::string &b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>(a[i])) !=
				std::tolower(static_cast<unsigned char>(b[i]))) {
			return false;
		}
	}
	return true;
}

bool matchSegment(const char *pat, const char *patEnd, const char *str, const char *strEnd) {
	while (pat < patEnd && str < strEnd) {
		if (*pat == '*') {
			++pat;
			if (pat < patEnd && *pat == '*') {
				++pat;
				while (pat < patEnd && *pat == '*') {
					++pat;
				}
				if (pat >= patEnd) {
					return true;
				}
				while (str < strEnd) {
					if (matchSegment(pat, patEnd, str, strEnd)) {
						return true;
					}
					++str;
				}
				return false;
			}
			while (str <= strEnd) {
				if (matchSegment(pat, patEnd, str, strEnd)) {
					return true;
				}
				++str;
			}
			return false;
		}
		if (*pat != *str) {
			return false;
		}
		++pat;
		++str;
	}
	while (pat < patEnd && *pat == '*') {
		++pat;
	}
	return pat >= patEnd && str >= strEnd;
}

bool pathMatchPattern(const std::string &path, const std::string &pattern, bool anchored) {
	if (pattern.empty()) {
		return false;
	}
	const char *p = pattern.c_str();
	const char *pe = p + pattern.size();
	const char *s = path.c_str();
	const char *se = s + path.size();

	if (pattern.find('/') == std::string::npos) {
		if (matchSegment(p, pe, s, se)) {
			return true;
		}
		if (anchored) {
			return false;
		}
		size_t last = path.rfind('/');
		std::string base = (last == std::string::npos) ? path : path.substr(last + 1);
		return matchSegment(p, pe, base.c_str(), base.c_str() + base.size());
	}

	return matchSegment(p, pe, s, se);
}

std::string pathRelativeToBase(const std::string &relPath, const std::string &baseDir) {
	std::string base = normalizeSlashes(baseDir);
	std::string path = normalizeSlashes(relPath);
	if (base.empty()) {
		return path;
	}
	if (path.size() <= base.size()) {
		return path;
	}
	if (path.compare(0, base.size(), base) != 0) {
		return {};
	}
	if (path[base.size()] != '/') {
		return {};
	}
	return path.substr(base.size() + 1);
}

} //namespace

void IgnoreMatcher::clear() {
	layers_.clear();
}

void IgnoreMatcher::addLayer(IgnoreLayer layer) {
	layer.baseDir = normalizeSlashes(std::move(layer.baseDir));
	layers_.push_back(std::move(layer));
}

void IgnoreMatcher::addRules(const std::string &baseDir, const std::vector<IgnoreRule> &rules) {
	IgnoreLayer layer;
	layer.baseDir = normalizeSlashes(baseDir);
	layer.rules = rules;
	addLayer(std::move(layer));
}

std::optional<IgnoreRule> IgnoreMatcher::parseRuleLine(IgnoreSyntax syntax, const std::string &line,
		const std::string &sourceFile, int lineNo) {
	std::string trimmed = line;
	while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r')) {
		trimmed.pop_back();
	}
	size_t start = 0;
	while (start < trimmed.size() && (trimmed[start] == ' ' || trimmed[start] == '\t')) {
		++start;
	}
	if (start >= trimmed.size()) {
		return std::nullopt;
	}
	if (trimmed[start] == '#') {
		return std::nullopt;
	}

	IgnoreRule rule;
	rule.syntax = syntax;
	rule.sourceFile = sourceFile;
	rule.sourceLine = lineNo;

	size_t i = start;
	if (trimmed[i] == '\\' && i + 1 < trimmed.size() && trimmed[i + 1] == '#') {
		i += 2;
	} else if (trimmed[i] == '!') {
		rule.negated = true;
		++i;
		while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
			++i;
		}
	}

	std::string pat = trimmed.substr(i);
	pat = normalizeSlashes(pat);
	if (pat.empty()) {
		return std::nullopt;
	}

	if (pat.back() == '/') {
		rule.dirOnly = true;
		pat.pop_back();
	}
	while (!pat.empty() && pat.front() == '/') {
		rule.anchored = true;
		pat.erase(pat.begin());
	}
	rule.pattern = pat;
	return rule;
}

bool IgnoreMatcher::matchPattern(const IgnoreRule &rule, const std::string &relPath, bool isDir) {
	if (rule.dirOnly && !isDir) {
		return false;
	}
	const std::string path = normalizeSlashes(relPath);
	if (rule.anchored) {
		if (rule.pattern.find('/') == std::string::npos) {
			if (path.find('/') != std::string::npos) {
				return false;
			}
		} else if (path.size() < rule.pattern.size() ||
				path.compare(0, rule.pattern.size(), rule.pattern) != 0 ||
				(path.size() > rule.pattern.size() && path[rule.pattern.size()] != '/')) {
			return false;
		}
	}
	return pathMatchPattern(path, rule.pattern, rule.anchored);
}

IgnoreMatchInfo IgnoreMatcher::match(const std::string &relPath, bool isDir) const {
	IgnoreMatchInfo info;
	const std::string normPath = normalizeSlashes(relPath);
	bool ignored = false;

	for (const auto &layer : layers_) {
		const std::string relToBase = pathRelativeToBase(normPath, layer.baseDir);
		if (relToBase.empty() && !layer.baseDir.empty()) {
			continue;
		}
		const std::string &candidate = layer.baseDir.empty() ? normPath : relToBase;

		for (const auto &rule : layer.rules) {
			if (!matchPattern(rule, candidate, isDir)) {
				continue;
			}
			ignored = !rule.negated;
			if (ignored) {
				info.ignored = true;
				info.matchedPattern = rule.pattern;
				info.sourceFile = rule.sourceFile;
				info.sourceLine = rule.sourceLine;
				info.syntax = rule.syntax;
			} else {
				info = {};
			}
		}
	}

	info.ignored = ignored;
	return info;
}

bool IgnoreMatcher::isIgnored(const std::string &relPath, bool isDir) const {
	return match(relPath, isDir).ignored;
}

} //namespace coldstorage
