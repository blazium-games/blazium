/**************************************************************************/
/*  attributes_map.h                                                      */
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

#include "common/attributes/gitattributes.h"
#include <string>
#include <vector>

namespace coldstorage {

class GitAttributesMap {
public:
	void clear();
	void addLayer(const std::string &baseDir, std::vector<GitAttributesEntry> entries);
	void loadFile(const std::string &path, const std::string &baseDir);
	ResolvedGitAttributes resolve(const std::string &relPath) const;
	static std::vector<GitAttributesEntry> parseContent(const std::string &content);

private:
	struct Layer {
		std::string baseDir;
		std::vector<GitAttributesEntry> entries;
	};
	std::vector<Layer> layers_;
	static bool matchPattern(const std::string &pattern, const std::string &path);
};

void loadGitAttributesTree(const std::string &workspaceRoot, GitAttributesMap &map);

} //namespace coldstorage
