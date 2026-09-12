/**************************************************************************/
/*  test_justamcp_resource_providers.h                                    */
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

void test_justamcp_tags_resource_provider_reads();
void test_justamcp_semantic_resource_provider_reads();
void test_justamcp_resource_provider_registry_coverage();
void test_justamcp_project_resource_provider_reads();
void test_justamcp_logs_resource_provider_reads();
void test_justamcp_guides_resource_provider_reads();
void test_justamcp_materials_resource_provider_reads();
void test_justamcp_selection_resource_provider_reads();

TEST_CASE("[Modules][JustAMCP] tags resource provider reads") {
	test_justamcp_tags_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] semantic resource provider reads") {
	test_justamcp_semantic_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] resource provider registry coverage") {
	test_justamcp_resource_provider_registry_coverage();
}

TEST_CASE("[Modules][JustAMCP] project resource provider reads") {
	test_justamcp_project_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] logs resource provider reads") {
	test_justamcp_logs_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] guides resource provider reads") {
	test_justamcp_guides_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] materials resource provider reads") {
	test_justamcp_materials_resource_provider_reads();
}

TEST_CASE("[Modules][JustAMCP] selection resource provider reads") {
	test_justamcp_selection_resource_provider_reads();
}
