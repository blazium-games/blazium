/**************************************************************************/
/*  justamcp_prompt_executor.cpp                                          */
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

#ifdef TOOLS_ENABLED

#include "justamcp_prompt_executor.h"

#include "../justamcp_mcp_spec.h"
#include "../justamcp_pagination.h"
#include "prompts/justamcp_prompt_asset_tagging_workflow.h" // IWYU pragma: keep
#include "prompts/justamcp_prompt_autowork_failure_analyzer.h"
#include "prompts/justamcp_prompt_autowork_test_generator.h"
#include "prompts/justamcp_prompt_blazium_context.h"
#include "prompts/justamcp_prompt_blazium_gdscript_linter.h"
#include "prompts/justamcp_prompt_blazium_multiplayer_architect.h"
#include "prompts/justamcp_prompt_blazium_optimization.h"
#include "prompts/justamcp_prompt_blazium_scene_architect.h"
#include "prompts/justamcp_prompt_blazium_shader_expert.h"
#include "prompts/justamcp_prompt_blazium_ui_scaffolder.h"
#include "prompts/justamcp_prompt_blazium_workflows.h"
#include "prompts/justamcp_prompt_editor_state.h"
#include "prompts/justamcp_prompt_project_info.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"

void JustAMCPPromptExecutor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_prompt", "name", "args"), &JustAMCPPromptExecutor::get_prompt);
	ClassDB::bind_method(D_METHOD("list_prompts", "cursor"), &JustAMCPPromptExecutor::list_prompts, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("complete_prompt", "ref", "argument", "context"), &JustAMCPPromptExecutor::complete_prompt, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_prompt", "prompt"), &JustAMCPPromptExecutor::add_prompt);
}

void JustAMCPPromptExecutor::register_settings() {
	JustAMCPPromptExecutor exec;

	Dictionary dict = exec.list_prompts();
	if (dict.has("prompts")) {
		Array prompts = dict["prompts"];
		for (int i = 0; i < prompts.size(); i++) {
			Dictionary p = prompts[i];
			String name = p["name"];
			String desc = p["description"];
			String path = "blazium/justamcp/prompts/" + name;

			GLOBAL_DEF_NOVAL_BASIC(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), String());
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(path, String());
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
			}
		}
	}
}

JustAMCPPromptExecutor::JustAMCPPromptExecutor() {
	add_prompt(memnew(JustAMCPPromptBlaziumContext));
	add_prompt(memnew(JustAMCPPromptBlaziumWorkflow(JustAMCPPromptBlaziumWorkflow::PROJECT_INTAKE)));
	add_prompt(memnew(JustAMCPPromptBlaziumWorkflow(JustAMCPPromptBlaziumWorkflow::SCENE_BUILD)));
	add_prompt(memnew(JustAMCPPromptBlaziumWorkflow(JustAMCPPromptBlaziumWorkflow::RUNTIME_TEST_LOOP)));
	add_prompt(memnew(JustAMCPPromptBlaziumWorkflow(JustAMCPPromptBlaziumWorkflow::AUTOWORK_FIX_LOOP)));
	add_prompt(memnew(JustAMCPPromptBlaziumWorkflow(JustAMCPPromptBlaziumWorkflow::DIAGNOSTICS_TRIAGE)));
	add_prompt(memnew(JustAMCPPromptProjectInfo));
	add_prompt(memnew(JustAMCPPromptEditorState));
	add_prompt(memnew(JustAMCPPromptAutoworkTestGenerator));
	add_prompt(memnew(JustAMCPPromptAutoworkFailureAnalyzer));
	add_prompt(memnew(JustAMCPPromptBlaziumOptimization));
	add_prompt(memnew(JustAMCPPromptBlaziumSceneArchitect));
	add_prompt(memnew(JustAMCPPromptBlaziumGDScriptLinter));
	add_prompt(memnew(JustAMCPPromptBlaziumMultiplayerArchitect));
	add_prompt(memnew(JustAMCPPromptBlaziumUIScaffolder));
	add_prompt(memnew(JustAMCPPromptBlaziumShaderExpert));
#ifdef MODULE_ASSETTAGS_ENABLED
	add_prompt(memnew(JustAMCPPromptAssetTaggingWorkflow));
#endif
}

JustAMCPPromptExecutor::~JustAMCPPromptExecutor() {}

void JustAMCPPromptExecutor::add_prompt(const Ref<JustAMCPPrompt> &p_prompt) {
	if (p_prompt.is_valid()) {
		registered_prompts.push_back(p_prompt);
	}
}

Dictionary JustAMCPPromptExecutor::list_prompts(const String &cursor) {
	Array prompts;
	for (int i = 0; i < registered_prompts.size(); i++) {
		if (registered_prompts[i].is_valid()) {
			Dictionary prompt = registered_prompts[i]->get_prompt();
			justamcp_attach_icons(prompt);
			prompts.push_back(prompt);
		}
	}
	return justamcp_pagination_slice_array(prompts, cursor, "prompts");
}

Dictionary JustAMCPPromptExecutor::get_prompt(const String &p_name, const Dictionary &p_args) {
	for (int i = 0; i < registered_prompts.size(); i++) {
		if (registered_prompts[i].is_valid() && registered_prompts[i]->get_name() == p_name) {
			Dictionary schema = registered_prompts[i]->get_prompt();
			Array arguments = schema.get("arguments", Array());
			for (int j = 0; j < arguments.size(); j++) {
				Dictionary argument = arguments[j];
				bool required = argument.get("required", false);
				String arg_name = argument.get("name", "");
				if (!required || arg_name.is_empty()) {
					continue;
				}
				if (!p_args.has(arg_name) || p_args[arg_name].get_type() == Variant::NIL || (p_args[arg_name].get_type() == Variant::STRING && String(p_args[arg_name]).strip_edges().is_empty())) {
					Dictionary result;
					result["ok"] = false;
					result["error_code"] = -32602;
					result["error"] = "Missing required prompt argument: " + arg_name;
					return result;
				}
			}
			return registered_prompts[i]->get_messages(p_args);
		}
	}

	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = "Unknown prompt: " + p_name;
	return result;
}

Dictionary JustAMCPPromptExecutor::complete_prompt(const Dictionary &p_ref, const Dictionary &p_argument, const Dictionary &p_context) {
	String ref_name = p_ref.has("name") ? String(Variant(p_ref["name"])) : "";
	Dictionary argument = p_argument.duplicate();
	if (!p_context.is_empty()) {
		argument["context"] = p_context;
	}

	for (int i = 0; i < registered_prompts.size(); i++) {
		if (registered_prompts[i].is_valid() && registered_prompts[i]->get_name() == ref_name) {
			Dictionary result;
			result["completion"] = registered_prompts[i]->complete(argument);
			return result;
		}
	}

	Dictionary result;
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	result["completion"] = completion;
	return result;
}

#endif
