/**************************************************************************/
/*  justamcp_prompt_executor.h                                            */
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

#include "core/object/object.h"
#include "prompts/justamcp_prompt.h"

class JustAMCPEditorPlugin;

class JustAMCPPromptExecutor : public Object {
	GDCLASS(JustAMCPPromptExecutor, Object);

	Vector<Ref<JustAMCPPrompt>> registered_prompts;

	JustAMCPEditorPlugin *editor_plugin = nullptr;

protected:
	static void _bind_methods();

public:
	static void register_settings();
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	void add_prompt(const Ref<JustAMCPPrompt> &p_prompt);

	Dictionary list_prompts(const String &cursor = "", bool p_include_unlisted = false);
	Dictionary get_prompt(const String &p_name, const Dictionary &p_args);
	Dictionary complete_prompt(const Dictionary &p_ref, const Dictionary &p_argument, const Dictionary &p_context = Dictionary());

	JustAMCPPromptExecutor();
	~JustAMCPPromptExecutor();
};

#endif
