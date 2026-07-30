/**************************************************************************/
/*  ignore_loader.cpp                                                     */
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

#include "common/ignore/ignore_loader.h"
#include "common/ignore/csignore_parser.h"
#include "common/util/sanitize.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
namespace coldstorage {
namespace {

std::string readFile(const fs::path &path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return {};
	}
	std::ostringstream oss;
	oss << in.rdbuf();
	return oss.str();
}

std::string relFromRoot(const fs::path &root, const fs::path &file) {
	std::error_code ec;
	auto rel = fs::relative(file.parent_path(), root, ec);
	if (ec) {
		return {};
	}
	const std::string s = rel.generic_string();
	return s == "." ? std::string{} : s;
}

void collectNamedFiles(const fs::path &root, const char *name, std::vector<fs::path> &out) {
	std::error_code ec;
	if (!fs::exists(root, ec)) {
		return;
	}
	for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
			!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (!it->is_regular_file(ec)) {
			continue;
		}
		if (it->path().filename() == name) {
			out.push_back(it->path());
		}
	}
}

void loadFileIntoMatcher(IgnoreMatcher &matcher, const fs::path &file, const fs::path &root,
		IgnoreSyntax syntax) {
	const std::string content = readFile(file);
	if (content.empty() && !fs::exists(file)) {
		return;
	}
	const std::string baseDir = relFromRoot(root, file);
	if (file.filename() == ".csignore") {
		auto parsed = parseCsignoreContent(content, file.string(), file.parent_path().string());
		if (!parsed.rules.empty()) {
			matcher.addRules(baseDir, parsed.rules);
		}
		for (const auto &[incPath, incSyntax] : parsed.includes) {
			fs::path p(incPath);
			if (!fs::exists(p)) {
				continue;
			}
			auto rules = parseIgnoreFileContent(incSyntax, readFile(p), p.string());
			matcher.addRules(relFromRoot(root, p), rules);
		}
	} else {
		auto rules = parseIgnoreFileContent(syntax, content, file.string());
		matcher.addRules(baseDir, rules);
	}
}

} //namespace

IgnoreLoader::IgnoreLoader(std::string workspaceRoot) :
		workspaceRoot_(std::move(workspaceRoot)) {}

std::vector<std::string> splitP4IgnoreEnv(const std::string &value) {
	std::vector<std::string> parts;
	std::string cur;
	for (char c : value) {
#ifdef _WIN32
		if (c == ';') {
#else
		if (c == ':' || c == ';') {
#endif
			if (!cur.empty()) {
				parts.push_back(cur);
			}
			cur.clear();
		} else {
			cur += c;
		}
	}
	if (!cur.empty()) {
		parts.push_back(cur);
	}
	return parts;
}

std::vector<std::string> IgnoreLoader::listIgnoreFiles(const IgnoreLoadOptions &opts) const {
	std::set<std::string> files;
	fs::path root(workspaceRoot_);
	std::vector<fs::path> found;
	if (opts.csignore) {
		collectNamedFiles(root, ".csignore", found);
	}
	if (opts.gitignore) {
		collectNamedFiles(root, ".gitignore", found);
	}
	if (opts.p4ignore) {
		collectNamedFiles(root, ".p4ignore", found);
		collectNamedFiles(root, "p4ignore.txt", found);
	}
	for (const auto &p : found) {
		files.insert(p.lexically_normal().string());
	}

	std::string env = sanitize::safeGetenv("P4IGNORE");
	if (env.empty()) {
		env = sanitize::safeGetenv("CSTORAGE_P4IGNORE");
	}
	for (const auto &part : splitP4IgnoreEnv(env)) {
		fs::path p(part);
		if (!p.is_absolute()) {
			p = root / p;
		}
		if (fs::exists(p)) {
			files.insert(p.lexically_normal().string());
		}
	}
	for (const auto &extra : opts.extraFiles) {
		fs::path p(extra);
		if (!p.is_absolute()) {
			p = root / p;
		}
		if (fs::exists(p)) {
			files.insert(p.lexically_normal().string());
		}
	}
	return std::vector<std::string>(files.begin(), files.end());
}

IgnoreMatcher IgnoreLoader::buildMatcher(const IgnoreLoadOptions &opts) const {
	IgnoreMatcher matcher;
	fs::path root(workspaceRoot_);
	std::vector<fs::path> csignores, gitignores, p4ignores;

	if (opts.csignore) {
		collectNamedFiles(root, ".csignore", csignores);
	}
	if (opts.gitignore) {
		collectNamedFiles(root, ".gitignore", gitignores);
	}
	if (opts.p4ignore) {
		collectNamedFiles(root, ".p4ignore", p4ignores);
		collectNamedFiles(root, "p4ignore.txt", p4ignores);
	}

	auto sortByDepth = [&root](std::vector<fs::path> &v) {
		std::sort(v.begin(), v.end(), [&root](const fs::path &a, const fs::path &b) {
			return relFromRoot(root, a) < relFromRoot(root, b);
		});
	};
	sortByDepth(csignores);
	sortByDepth(gitignores);
	sortByDepth(p4ignores);

	for (const auto &f : csignores) {
		loadFileIntoMatcher(matcher, f, root, IgnoreSyntax::Git);
	}
	for (const auto &f : gitignores) {
		loadFileIntoMatcher(matcher, f, root, IgnoreSyntax::Git);
	}
	for (const auto &f : p4ignores) {
		loadFileIntoMatcher(matcher, f, root, IgnoreSyntax::P4);
	}

	std::string env = sanitize::safeGetenv("P4IGNORE");
	if (env.empty()) {
		env = sanitize::safeGetenv("CSTORAGE_P4IGNORE");
	}
	for (const auto &part : splitP4IgnoreEnv(env)) {
		fs::path p(part);
		if (!p.is_absolute()) {
			p = root / p;
		}
		if (fs::exists(p)) {
			loadFileIntoMatcher(matcher, p, root, syntaxFromFilename(p.string()));
		}
	}
	for (const auto &extra : opts.extraFiles) {
		fs::path p(extra);
		if (!p.is_absolute()) {
			p = root / p;
		}
		if (fs::exists(p)) {
			loadFileIntoMatcher(matcher, p, root, syntaxFromFilename(p.string()));
		}
	}
	return matcher;
}

} //namespace coldstorage
