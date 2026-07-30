/**************************************************************************/
/*  attributes_map.cpp                                                    */
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

#include "common/attributes/attributes_map.h"
#include "common/attributes/eol_normalize.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
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
	return s;
}

std::string trim(const std::string &s) {
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

bool fnmatchSimple(const char *pat, const char *str) {
	while (*pat && *str) {
		if (*pat == '*') {
			++pat;
			while (*pat == '*') {
				++pat;
			}
			if (!*pat) {
				return true;
			}
			while (*str) {
				if (fnmatchSimple(pat, str)) {
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
	while (*pat == '*') {
		++pat;
	}
	return !*pat && !*str;
}

} //namespace

void GitAttributesMap::clear() {
	layers_.clear();
}

bool GitAttributesMap::matchPattern(const std::string &pattern, const std::string &path) {
	std::string pat = pattern;
	if (!pat.empty() && pat.back() == '/') {
		pat.pop_back();
	}
	if (pat.find('/') == std::string::npos) {
		size_t slash = path.rfind('/');
		std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
		return fnmatchSimple(pat.c_str(), base.c_str()) || fnmatchSimple(pat.c_str(), path.c_str());
	}
	return fnmatchSimple(pat.c_str(), path.c_str());
}

std::vector<GitAttributesEntry> GitAttributesMap::parseContent(const std::string &content) {
	std::vector<GitAttributesEntry> entries;
	std::istringstream in(content);
	std::string line;
	while (std::getline(in, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}
		std::istringstream ls(line);
		GitAttributesEntry e;
		if (!(ls >> e.pattern)) {
			continue;
		}
		std::string tok;
		while (ls >> tok) {
			if (tok == "text") {
				e.text = true;
			} else if (tok == "-text") {
				e.textUnset = true;
			} else if (tok == "binary") {
				e.binary = true;
			} else if (tok == "text=auto") {
				e.textAuto = true;
			} else if (tok == "eol=lf") {
				e.eol = EolStyle::Lf;
			} else if (tok == "eol=crlf") {
				e.eol = EolStyle::Crlf;
			} else if (tok == "eol=native") {
				e.eol = EolStyle::Native;
			}
		}
		entries.push_back(std::move(e));
	}
	return entries;
}

void GitAttributesMap::addLayer(const std::string &baseDir, std::vector<GitAttributesEntry> entries) {
	layers_.push_back({ normalizeSlashes(baseDir), std::move(entries) });
}

void GitAttributesMap::loadFile(const std::string &path, const std::string &baseDir) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return;
	}
	std::ostringstream oss;
	oss << in.rdbuf();
	addLayer(baseDir, parseContent(oss.str()));
}

ResolvedGitAttributes GitAttributesMap::resolve(const std::string &relPath) const {
	ResolvedGitAttributes out;
	const std::string path = normalizeSlashes(relPath);
	const GitAttributesEntry *last = nullptr;

	for (const auto &layer : layers_) {
		std::string candidate = path;
		if (!layer.baseDir.empty()) {
			if (path.size() <= layer.baseDir.size() || path.compare(0, layer.baseDir.size(), layer.baseDir) != 0) {
				continue;
			}
			if (path[layer.baseDir.size()] != '/') {
				continue;
			}
			candidate = path.substr(layer.baseDir.size() + 1);
		}
		for (const auto &e : layer.entries) {
			if (matchPattern(e.pattern, candidate)) {
				last = &e;
			}
		}
	}

	if (!last) {
		return out;
	}
	if (last->binary || last->textUnset) {
		out.isBinary = true;
		return out;
	}
	if (last->text || last->textAuto) {
		out.normalizeEol = true;
	}
	if (last->eol != EolStyle::Unspecified) {
		out.eol = last->eol;
	} else if (out.normalizeEol) {
		out.eol = EolStyle::Native;
	}
	return out;
}

void loadGitAttributesTree(const std::string &workspaceRoot, GitAttributesMap &map) {
	map.clear();
	fs::path root(workspaceRoot);
	std::error_code ec;
	if (!fs::exists(root, ec)) {
		return;
	}
	std::vector<fs::path> files;
	for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
			!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (!it->is_regular_file(ec)) {
			continue;
		}
		if (it->path().filename() != ".gitattributes") {
			continue;
		}
		files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	for (const auto &f : files) {
		std::error_code ec2;
		auto rel = fs::relative(f.parent_path(), root, ec2);
		std::string base = ec2 ? "" : rel.generic_string();
		map.loadFile(f.string(), base);
	}
}

} //namespace coldstorage
