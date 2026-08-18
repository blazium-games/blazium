/**************************************************************************/
/*  ignore_loader.h                                                       */
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

#include "common/ignore/ignore_matcher.h"
#include <string>
#include <vector>

namespace coldstorage {

struct IgnoreLoadOptions {
	bool csignore = true;
	bool gitignore = true;
	bool p4ignore = true;
	std::vector<std::string> extraFiles;
};

class IgnoreLoader {
public:
	explicit IgnoreLoader(std::string workspaceRoot);

	IgnoreMatcher buildMatcher(const IgnoreLoadOptions &opts) const;
	std::vector<std::string> listIgnoreFiles(const IgnoreLoadOptions &opts) const;

private:
	std::string workspaceRoot_;
};

std::vector<std::string> splitP4IgnoreEnv(const std::string &value);

} //namespace coldstorage
