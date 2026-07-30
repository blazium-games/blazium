/**************************************************************************/
/*  workspace_filters.h                                                   */
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

#include "common/attributes/attributes_map.h"
#include "common/attributes/gitattributes.h"
#include "common/ignore/ignore_loader.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace coldstorage {

struct AddCandidate {
	std::string depotPath;
	std::string localPath;
};

class WorkspaceFilters {
public:
	WorkspaceFilters(std::string workspaceRoot, IgnoreLoadOptions ignoreOpts = {});

	void reloadIfStale();
	std::optional<std::string> localToDepot(const std::string &arg) const;
	std::vector<AddCandidate> collectAddPaths(const std::string &arg) const;
	IgnoreMatchInfo matchIgnore(const std::string &localRelPath, bool isDir) const;
	ResolvedGitAttributes attrsForDepotPath(const std::string &depotPath) const;
	std::vector<uint8_t> normalizeForSubmit(const std::string &depotPath,
			const std::vector<uint8_t> &data) const;
	std::vector<uint8_t> normalizeForSyncWrite(const std::string &depotPath,
			const std::vector<uint8_t> &data) const;

	const std::string &workspaceRoot() const { return workspaceRoot_; }

private:
	std::string workspaceRoot_;
	IgnoreLoadOptions ignoreOpts_;
	mutable IgnoreMatcher matcher_;
	mutable GitAttributesMap attributes_;
	mutable std::vector<std::string> trackedIgnoreFiles_;
	mutable bool loaded_ = false;

	void ensureLoaded() const;
	std::string depotFromLocalRel(const std::string &rel) const;
	static std::string localRelFromArg(const std::filesystem::path &root, const std::string &arg);
};

} //namespace coldstorage
