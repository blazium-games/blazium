/**************************************************************************/
/*  csignore_parser.cpp                                                   */
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

#include "common/ignore/csignore_parser.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace coldstorage {

IgnoreSyntax syntaxFromFilename(const std::string &filename) {
	fs::path p(filename);
	const std::string name = p.filename().string();
	if (name == ".p4ignore" || name == "p4ignore.txt") {
		return IgnoreSyntax::P4;
	}
	return IgnoreSyntax::Git;
}

static std::string trim(const std::string &s) {
	size_t a = 0;
	while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
		++a;
	}
	size_t b = s.size();
	while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
		--b;
	}
	return s.substr(a, b - a);
}

static std::string toLower(std::string s) {
	for (char &c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

static bool iequals(const std::string &a, const std::string &b) {
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

std::vector<IgnoreRule> parseIgnoreFileContent(IgnoreSyntax syntax, const std::string &content,
		const std::string &sourceFile) {
	std::vector<IgnoreRule> rules;
	std::istringstream in(content);
	std::string line;
	int lineNo = 0;
	while (std::getline(in, line)) {
		++lineNo;
		if (auto rule = IgnoreMatcher::parseRuleLine(syntax, line, sourceFile, lineNo)) {
			rules.push_back(*rule);
		}
	}
	return rules;
}

CsignoreParseResult parseCsignoreContent(const std::string &content, const std::string &sourceFile,
		const std::string &sourceDir) {
	CsignoreParseResult result;
	IgnoreSyntax current = IgnoreSyntax::Git;
	std::istringstream in(content);
	std::string line;
	int lineNo = 0;
	while (std::getline(in, line)) {
		++lineNo;
		std::string t = trim(line);
		if (t.empty() || t[0] == '#') {
			continue;
		}
		if (t[0] == '@') {
			std::string lower = toLower(t);
			if (lower.rfind("@syntax", 0) == 0) {
				std::string rest = trim(t.substr(7));
				if (iequals(rest, "p4")) {
					current = IgnoreSyntax::P4;
				} else {
					current = IgnoreSyntax::Git;
				}
				continue;
			}
			if (lower.rfind("@include", 0) == 0) {
				std::string rest = trim(t.substr(8));
				IgnoreSyntax incSyntax = IgnoreSyntax::Git;
				auto sp = rest.find(" syntax=");
				std::string pathPart = rest;
				if (sp != std::string::npos) {
					pathPart = trim(rest.substr(0, sp));
					std::string syn = toLower(trim(rest.substr(sp + 8)));
					if (syn == "p4") {
						incSyntax = IgnoreSyntax::P4;
					} else if (syn == "auto") {
						incSyntax = syntaxFromFilename(pathPart);
					}
				}
				fs::path incPath = fs::path(sourceDir) / pathPart;
				result.includes.emplace_back(incPath.lexically_normal().string(), incSyntax);
				continue;
			}
			continue;
		}
		if (auto rule = IgnoreMatcher::parseRuleLine(current, line, sourceFile, lineNo)) {
			result.rules.push_back(*rule);
		}
	}
	return result;
}

} //namespace coldstorage
