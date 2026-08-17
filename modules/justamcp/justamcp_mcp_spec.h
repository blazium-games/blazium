/**************************************************************************/
/*  justamcp_mcp_spec.h                                                   */
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

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

enum JustAMCPProtocolFeature {
	JUSTAMCP_FEATURE_COMPLETIONS,
	JUSTAMCP_FEATURE_JSONRPC_BATCH,
	JUSTAMCP_FEATURE_ELICITATION_FORM,
	JUSTAMCP_FEATURE_ELICITATION_URL,
	JUSTAMCP_FEATURE_STRUCTURED_CONTENT,
	JUSTAMCP_FEATURE_RESOURCE_LINKS,
	JUSTAMCP_FEATURE_SERVER_TITLE,
	JUSTAMCP_FEATURE_ICONS,
	JUSTAMCP_FEATURE_TASKS_INITIALIZE,
};

int justamcp_protocol_rank(const String &p_version);
bool justamcp_protocol_at_least(const String &p_version, const String &p_minimum);
bool justamcp_protocol_supports(const String &p_version, JustAMCPProtocolFeature p_feature);
String justamcp_active_protocol_version();

// SEP-973: optional icons on tools, prompts, resources, and templates.
Array justamcp_default_icons();
void justamcp_attach_icons(Dictionary &p_schema);
void justamcp_apply_protocol_to_schema(Dictionary &p_schema, const String &p_protocol);
void justamcp_apply_protocol_to_list_result(Dictionary &p_result, const String &p_protocol);

Dictionary justamcp_resource_link_content(const String &p_uri, const String &p_name = String(), const String &p_mime_type = String());

// SEP-986: tool names are lowercase snake_case, max 64 characters.
bool justamcp_is_valid_mcp_tool_name(const String &p_name);
String justamcp_invalid_mcp_tool_name_message(const String &p_name);

// SEP-1034 / SEP-1330: form elicitation schemas and ElicitResult validation.
Dictionary justamcp_confirm_enum_schema();
Dictionary justamcp_apply_schema_defaults(const Dictionary &p_schema, const Dictionary &p_content);
bool justamcp_validate_elicit_content(const Dictionary &p_schema, const Dictionary &p_content, String &r_error);
bool justamcp_parse_elicit_result(const Dictionary &p_result, String &r_action, Dictionary &r_content, String &r_error);
bool justamcp_elicit_content_is_confirmed(const Dictionary &p_content);

Dictionary justamcp_url_elicitation_error_rpc(const Variant &p_request_id, const String &p_elicitation_id, const String &p_url, const String &p_message);
