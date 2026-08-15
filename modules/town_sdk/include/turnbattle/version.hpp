/**************************************************************************/
/*  version.hpp                                                           */
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
#include <cstdlib>
#include <string>

namespace turnbattle {
namespace version {

constexpr int MAJOR = 0;
constexpr int MINOR = 1;
constexpr int PATCH = 0;

inline std::string get_version_string() {
	return std::to_string(MAJOR) + "." +
			std::to_string(MINOR) + "." +
			std::to_string(PATCH);
}

inline bool matches(int major, int minor, int patch) {
	return MAJOR == major && MINOR == minor && PATCH == patch;
}

inline bool matches(const std::string &version_str) {
	// Parse version string "X.Y.Z"
	size_t first_dot = version_str.find('.');
	if (first_dot == std::string::npos) {
		return false;
	}

	size_t second_dot = version_str.find('.', first_dot + 1);
	if (second_dot == std::string::npos) {
		return false;
	}

	std::string major_str = version_str.substr(0, first_dot);
	std::string minor_str = version_str.substr(first_dot + 1, second_dot - first_dot - 1);
	std::string patch_str = version_str.substr(second_dot + 1);

	if (major_str.empty() || minor_str.empty() || patch_str.empty()) {
		return false;
	}

	char *end_ptr = nullptr;
	int major = std::strtol(major_str.c_str(), &end_ptr, 10);
	if (*end_ptr != '\0') {
		return false;
	}

	int minor = std::strtol(minor_str.c_str(), &end_ptr, 10);
	if (*end_ptr != '\0') {
		return false;
	}

	int patch = std::strtol(patch_str.c_str(), &end_ptr, 10);
	if (*end_ptr != '\0') {
		return false;
	}

	return matches(major, minor, patch);
}

} // namespace version
} // namespace turnbattle
