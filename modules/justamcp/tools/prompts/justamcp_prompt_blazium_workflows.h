/**************************************************************************/
/*  justamcp_prompt_blazium_workflows.h                                   */
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

#include "justamcp_prompt.h"

class JustAMCPPromptBlaziumWorkflow : public JustAMCPPrompt {
	GDCLASS(JustAMCPPromptBlaziumWorkflow, JustAMCPPrompt);

public:
	enum WorkflowKind {
		PROJECT_INTAKE,
		SCENE_BUILD,
		RUNTIME_TEST_LOOP,
		AUTOWORK_FIX_LOOP,
		DIAGNOSTICS_TRIAGE,
	};

private:
	WorkflowKind kind = PROJECT_INTAKE;

	String _get_title() const;
	String _get_description() const;
	Array _get_arguments() const;
	void _append_common_context(Array &r_messages) const;
	Dictionary _get_project_intake_messages(const Dictionary &p_args);
	Dictionary _get_scene_build_messages(const Dictionary &p_args);
	Dictionary _get_runtime_test_loop_messages(const Dictionary &p_args);
	Dictionary _get_autowork_fix_loop_messages(const Dictionary &p_args);
	Dictionary _get_diagnostics_triage_messages(const Dictionary &p_args);

protected:
	static void _bind_methods();

public:
	virtual String get_name() const override;
	virtual Dictionary get_prompt() const override;
	virtual Dictionary get_messages(const Dictionary &p_args) override;
	virtual Dictionary complete(const Dictionary &p_argument) override;

	JustAMCPPromptBlaziumWorkflow(WorkflowKind p_kind = PROJECT_INTAKE);
	~JustAMCPPromptBlaziumWorkflow();
};

#endif
