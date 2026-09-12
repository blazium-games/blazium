/**************************************************************************/
/*  text_diff.cpp                                                         */
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

#include "common/util/text_diff.h"
#include <algorithm>
#include <sstream>

namespace coldstorage {
namespace {

std::vector<std::string> splitLines(const std::string &text) {
	std::vector<std::string> lines;
	std::string cur;
	for (char c : text) {
		if (c == '\n') {
			lines.push_back(cur);
			cur.clear();
		} else if (c != '\r') {
			cur.push_back(c);
		}
	}
	lines.push_back(cur);
	return lines;
}

struct HunkLine {
	char tag;
	std::string text;
};

} //namespace

bool looksBinary(const std::string &data) {
	if (data.find('\0') != std::string::npos) {
		return true;
	}
	return false;
}

std::string unifiedDiffLines(const std::vector<std::string> &oldLines,
		const std::vector<std::string> &newLines,
		const std::string &oldLabel, const std::string &newLabel,
		const std::string &pathLabel) {
	const size_t n = oldLines.size();
	const size_t m = newLines.size();
	std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));
	for (size_t i = n; i-- > 0;) {
		for (size_t j = m; j-- > 0;) {
			if (oldLines[i] == newLines[j]) {
				dp[i][j] = dp[i + 1][j + 1] + 1;
			} else {
				dp[i][j] = std::max(dp[i + 1][j], dp[i][j + 1]);
			}
		}
	}

	std::vector<HunkLine> ops;
	size_t i = 0, j = 0;
	while (i < n && j < m) {
		if (oldLines[i] == newLines[j]) {
			ops.push_back({ ' ', oldLines[i] });
			++i;
			++j;
		} else if (dp[i + 1][j] >= dp[i][j + 1]) {
			ops.push_back({ '-', oldLines[i] });
			++i;
		} else {
			ops.push_back({ '+', newLines[j] });
			++j;
		}
	}
	while (i < n) {
		ops.push_back({ '-', oldLines[i++] });
	}
	while (j < m) {
		ops.push_back({ '+', newLines[j++] });
	}

	if (ops.empty()) {
		return {};
	}

	std::ostringstream out;
	out << "--- " << oldLabel << "\n";
	out << "+++ " << newLabel << "\n";
	out << "@@ " << pathLabel << " @@\n";
	for (const auto &op : ops) {
		out << op.tag << op.text << "\n";
	}
	return out.str();
}

std::string unifiedDiffText(const std::string &oldText, const std::string &newText,
		const std::string &oldLabel, const std::string &newLabel,
		const std::string &pathLabel) {
	if (oldText == newText) {
		return {};
	}
	return unifiedDiffLines(splitLines(oldText), splitLines(newText), oldLabel, newLabel,
			pathLabel);
}

} //namespace coldstorage
