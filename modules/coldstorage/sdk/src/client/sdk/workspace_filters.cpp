/**************************************************************************/
/*  workspace_filters.cpp                                                 */
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

#include "client/sdk/workspace_filters.h"
#include "common/attributes/eol_normalize.h"
#include "common/util/sanitize.h"
#include <algorithm>

namespace fs = std::filesystem;
namespace coldstorage {
namespace {

bool underColdstorageMeta(const fs::path &rel) {
	std::string s = rel.generic_string();
	return s == ".coldstorage" || s.rfind(".coldstorage/", 0) == 0;
}

} //namespace

WorkspaceFilters::WorkspaceFilters(std::string workspaceRoot, IgnoreLoadOptions ignoreOpts) :
		workspaceRoot_(std::move(workspaceRoot)), ignoreOpts_(std::move(ignoreOpts)) {}

void WorkspaceFilters::ensureLoaded() const {
	if (loaded_) {
		IgnoreLoader loader(workspaceRoot_);
		auto files = loader.listIgnoreFiles(ignoreOpts_);
		if (files == trackedIgnoreFiles_) {
			return;
		}
	}
	IgnoreLoader loader(workspaceRoot_);
	matcher_ = loader.buildMatcher(ignoreOpts_);
	loadGitAttributesTree(workspaceRoot_, attributes_);
	trackedIgnoreFiles_ = loader.listIgnoreFiles(ignoreOpts_);
	loaded_ = true;
}

void WorkspaceFilters::reloadIfStale() {
	loaded_ = false;
	ensureLoaded();
}

std::string WorkspaceFilters::depotFromLocalRel(const std::string &rel) const {
	if (!sanitize::isValidDepotPath("//depot/" + rel)) {
		return {};
	}
	return "//depot/" + rel;
}

std::string WorkspaceFilters::localRelFromArg(const fs::path &root, const std::string &arg) {
	if (arg.find("//") == 0) {
		if (arg.rfind("//depot/", 0) != 0) {
			return {};
		}
		return arg.substr(8);
	}
	fs::path p(arg);
	if (p.is_absolute()) {
		std::error_code ec;
		p = fs::relative(p, root, ec);
		if (ec) {
			return {};
		}
	}
	return p.lexically_normal().generic_string();
}

std::optional<std::string> WorkspaceFilters::localToDepot(const std::string &arg) const {
	ensureLoaded();
	if (arg.find("//") == 0) {
		if (sanitize::isValidDepotPath(arg)) {
			return arg;
		}
		return std::nullopt;
	}
	const std::string rel = localRelFromArg(fs::path(workspaceRoot_), arg);
	if (rel.empty() || rel.find("..") != std::string::npos) {
		return std::nullopt;
	}
	return depotFromLocalRel(rel);
}

std::vector<AddCandidate> WorkspaceFilters::collectAddPaths(const std::string &arg) const {
	ensureLoaded();
	std::vector<AddCandidate> out;
	fs::path root(workspaceRoot_);
	std::string rel = localRelFromArg(root, arg);
	if (rel.empty()) {
		return out;
	}

	fs::path target = root / rel;
	std::error_code ec;
	if (!fs::exists(target, ec)) {
		return out;
	}

	auto emitFile = [&](const fs::path &file, const std::string &fileRel) {
		if (underColdstorageMeta(fs::path(fileRel))) {
			return;
		}
		const bool isDir = fs::is_directory(file, ec);
		if (matcher_.isIgnored(fileRel, isDir)) {
			return;
		}
		if (isDir) {
			return;
		}
		AddCandidate c;
		c.localPath = file.lexically_normal().string();
		c.depotPath = depotFromLocalRel(fileRel);
		if (!c.depotPath.empty()) {
			out.push_back(std::move(c));
		}
	};

	if (fs::is_directory(target, ec)) {
		for (auto it = fs::recursive_directory_iterator(
					 target, fs::directory_options::skip_permission_denied, ec);
				!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (!it->is_regular_file(ec)) {
				continue;
			}
			std::error_code ec2;
			auto fileRel = fs::relative(it->path(), root, ec2).generic_string();
			if (ec2) {
				continue;
			}
			emitFile(it->path(), fileRel);
		}
	} else {
		emitFile(target, rel);
	}
	return out;
}

IgnoreMatchInfo WorkspaceFilters::matchIgnore(const std::string &localRelPath, bool isDir) const {
	ensureLoaded();
	return matcher_.match(localRelPath, isDir);
}

ResolvedGitAttributes WorkspaceFilters::attrsForDepotPath(const std::string &depotPath) const {
	ensureLoaded();
	std::string rel = depotPath;
	if (rel.rfind("//depot/", 0) == 0) {
		rel = rel.substr(8);
	}
	return attributes_.resolve(rel);
}

std::vector<uint8_t> WorkspaceFilters::normalizeForSubmit(const std::string &depotPath,
		const std::vector<uint8_t> &data) const {
	auto attrs = attrsForDepotPath(depotPath);
	if (attrs.isBinary || !attrs.normalizeEol) {
		return data;
	}
	if (!isProbablyText(data)) {
		return data;
	}
	EolStyle style = attrs.eol == EolStyle::Unspecified ? EolStyle::Lf : attrs.eol;
	return normalizeEol(data, style);
}

std::vector<uint8_t> WorkspaceFilters::normalizeForSyncWrite(const std::string &depotPath,
		const std::vector<uint8_t> &data) const {
	auto attrs = attrsForDepotPath(depotPath);
	if (attrs.isBinary || !attrs.normalizeEol) {
		return data;
	}
	if (!isProbablyText(data)) {
		return data;
	}
	EolStyle style = attrs.eol == EolStyle::Unspecified ? EolStyle::Native : attrs.eol;
	return normalizeEol(data, style);
}

} //namespace coldstorage
