/**************************************************************************/
/*  justamcp_prompt.h                                                     */
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

#include "core/object/ref_counted.h"

class JustAMCPPrompt : public RefCounted {
	GDCLASS(JustAMCPPrompt, RefCounted);

protected:
	static void _bind_methods();
	static Dictionary _make_text_message(const String &p_text, const String &p_role = "user");
	static Dictionary _make_resource_message(const String &p_uri);
	static Dictionary _make_prompt_argument(const String &p_name, const String &p_description, bool p_required);
	static Dictionary _make_error_result(const String &p_error, int p_error_code = -32602);
	static bool _has_non_empty_argument(const Dictionary &p_args, const String &p_name);
	static Dictionary _validate_required_arguments(const Dictionary &p_args, const Vector<String> &p_required);
	static Dictionary _make_completion(const Vector<String> &p_values, const String &p_prefix);
	static Dictionary _make_empty_completion();

public:
	virtual String get_name() const { return ""; }
	virtual Dictionary get_prompt() const { return Dictionary(); }
	virtual Dictionary get_messages(const Dictionary &p_args) { return Dictionary(); }
	virtual Dictionary complete(const Dictionary &p_argument) { return Dictionary(); }

	JustAMCPPrompt();
	~JustAMCPPrompt();
};

#endif
