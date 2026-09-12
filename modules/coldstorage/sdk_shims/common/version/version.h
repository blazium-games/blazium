/**************************************************************************/
/*  version.h                                                             */
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

namespace coldstorage {
namespace version {

constexpr int MAJOR = 1;
constexpr int MINOR = 0;
constexpr int PATCH = 0;

constexpr const char *VERSION = "1.0.0";
constexpr const char *PRERELEASE = "";
constexpr const char *BUILD_META = "blazium";
constexpr const char *LABEL = "1.0.0";
constexpr const char *FULL = "1.0.0+blazium";
constexpr const char *PRODUCT_BANNER = "ColdStorage 1.0.0";

constexpr int PROTOCOL_VERSION = 1;

constexpr const char *MIN_CLIENT = "1.0.0";
constexpr const char *BUILD_TIMESTAMP = __DATE__ " " __TIME__;

constexpr const char *PRODUCT_NAME = "ColdStorage";
constexpr const char *SERVER_NAME = "ColdStorage Server";
constexpr const char *CLIENT_NAME = "ColdStorage CLI";
constexpr const char *SDK_NAME = "ColdStorage SDK";
constexpr const char *COPYRIGHT = "Copyright (c) 2018-2026 Randolph William Aarseth II, Bioblaze Payne - Blazium Games";

} //namespace version
} //namespace coldstorage
