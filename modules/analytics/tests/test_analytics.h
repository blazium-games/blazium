/**************************************************************************/
/*  test_analytics.h                                                      */
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

#include "tests/test_macros.h"

void test_analytics_queue_roundtrip();
void test_analytics_consent_and_identity();
void test_analytics_anonymous_omits_device_uid();
void test_analytics_identified_includes_device_uid();
void test_analytics_payload_shape();
void test_analytics_shared_identity();

TEST_CASE("[Modules][Analytics] queue JSONL roundtrip") {
	test_analytics_queue_roundtrip();
}

TEST_CASE("[Modules][Analytics] consent gates queue") {
	test_analytics_consent_and_identity();
}

TEST_CASE("[Modules][Analytics] anonymous omits device_uid") {
	test_analytics_anonymous_omits_device_uid();
}

TEST_CASE("[Modules][Analytics] identified includes OS unique id") {
	test_analytics_identified_includes_device_uid();
}

TEST_CASE("[Modules][Analytics] payload identity envelope") {
	test_analytics_payload_shape();
}

TEST_CASE("[Modules][Analytics] shared app_id and build_id") {
	test_analytics_shared_identity();
}
