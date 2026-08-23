/**************************************************************************/
/*  justamcp_mcp_client_oauth.h                                           */
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

#ifdef TOOLS_ENABLED

#include "core/variant/dictionary.h"

class JustAMCPMCPClientOAuth {
public:
	struct TokenSet {
		String access_token;
		String refresh_token;
		String token_type = "Bearer";
		String issuer;
		int expires_in = 0;
	};

	static String parse_www_authenticate_metadata_url(const String &p_header);
	static Dictionary parse_protected_resource_metadata(const Dictionary &p_json);
	static Dictionary parse_authorization_server_metadata(const Dictionary &p_json);
	static Dictionary build_dcr_request(const String &p_redirect_uri, const String &p_application_type = "native", const String &p_token_auth_method = "none");
	static bool validate_cimd_url(const String &p_url, String &r_error);
	static bool validate_iss(const String &p_expected_issuer, const String &p_returned_iss, String &r_error);
	static String issuer_storage_key(const String &p_issuer);
	static String pkce_challenge_s256(const String &p_verifier);
	static String generate_pkce_verifier();
	static String generate_state();
	static String to_base64url(const String &p_b64);

	static Dictionary load_tokens(const String &p_issuer);
	static void store_tokens(const String &p_issuer, const Dictionary &p_tokens);
	static void clear_tokens(const String &p_issuer);
	static String editor_token_path(const String &p_issuer);

	static Dictionary client_metadata_document(int p_loopback_port, const String &p_cimd_url);
	static Dictionary handle_loopback_callback(const Dictionary &p_query);
	static bool register_pending_auth(const Dictionary &p_pending);
	static Dictionary take_pending_auth(const String &p_state);

	static Dictionary resolve_bearer_token(const Dictionary &p_bridge_config, const Dictionary &p_http_response);
	static Dictionary begin_authorization(const Dictionary &p_bridge_config, const Dictionary &p_as_metadata);
	static Dictionary exchange_code(const Dictionary &p_pending, const String &p_code);
	static Dictionary refresh_access_token(const Dictionary &p_tokens, const Dictionary &p_as_metadata, const Dictionary &p_bridge_config);
};

#endif
