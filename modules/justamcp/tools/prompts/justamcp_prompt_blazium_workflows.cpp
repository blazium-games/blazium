/**************************************************************************/
/*  justamcp_prompt_blazium_workflows.cpp                                 */
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

#include "justamcp_prompt_blazium_workflows.h"

void JustAMCPPromptBlaziumWorkflow::_bind_methods() {
}

JustAMCPPromptBlaziumWorkflow::JustAMCPPromptBlaziumWorkflow(WorkflowKind p_kind) {
	kind = p_kind;
}

JustAMCPPromptBlaziumWorkflow::~JustAMCPPromptBlaziumWorkflow() {
}

String JustAMCPPromptBlaziumWorkflow::get_name() const {
	switch (kind) {
		case PROJECT_INTAKE:
			return "blazium_project_intake";

		case SCENE_BUILD:
			return "blazium_scene_build_workflow";

		case RUNTIME_TEST_LOOP:
			return "blazium_runtime_test_loop";

		case AUTOWORK_FIX_LOOP:
			return "blazium_autowork_fix_loop";

		case DIAGNOSTICS_TRIAGE:
			return "blazium_diagnostics_triage";
	}

	return "blazium_project_intake";
}

String JustAMCPPromptBlaziumWorkflow::_get_title() const {
	switch (kind) {
		case PROJECT_INTAKE:
			return "Blazium Project Intake";

		case SCENE_BUILD:
			return "Blazium Scene Build Workflow";

		case RUNTIME_TEST_LOOP:
			return "Blazium Runtime Test Loop";

		case AUTOWORK_FIX_LOOP:
			return "Blazium Autowork Fix Loop";

		case DIAGNOSTICS_TRIAGE:
			return "Blazium Diagnostics Triage";
	}

	return "Blazium Project Intake";
}

String JustAMCPPromptBlaziumWorkflow::_get_description() const {
	switch (kind) {
		case PROJECT_INTAKE:
			return "Loads Blazium project, scene, selection, and JustAMCP guide context before starting substantial engine work. Template v1.";

		case SCENE_BUILD:
			return "Turns a gameplay feature request into a deterministic scene/script/signal/resource build workflow using JustAMCP tools. Template v1.";

		case RUNTIME_TEST_LOOP:
			return "Runs and inspects a Blazium scene through the editor/runtime bridge, including screenshots, logs, input, and clean shutdown. Template v1.";

		case AUTOWORK_FIX_LOOP:
			return "Runs Autowork tests, reads results, diagnoses failures, patches scripts when requested, validates, and reruns. Template v1.";

		case DIAGNOSTICS_TRIAGE:
			return "Aggregates Blazium logs, runtime status, scene validation, project state, and performance context into a structured diagnosis. Template v1.";
	}

	return "";
}

Array JustAMCPPromptBlaziumWorkflow::_get_arguments() const {
	Array arguments;

	switch (kind) {
		case PROJECT_INTAKE:
			arguments.push_back(_make_prompt_argument("scope", "Context scope: all, project, scene, runtime, docs.", false));

			arguments.push_back(_make_prompt_argument("detail_level", "Output detail level: brief, normal, deep.", false));

			break;

		case SCENE_BUILD:
			arguments.push_back(_make_prompt_argument("feature", "Gameplay feature, mechanic, or scene system to build.", true));

			arguments.push_back(_make_prompt_argument("scene_path", "Optional res:// scene path to modify or create.", false));

			arguments.push_back(_make_prompt_argument("output_mode", "Workflow mode: plan, apply, or review.", false));

			break;

		case RUNTIME_TEST_LOOP:
			arguments.push_back(_make_prompt_argument("target", "Run target: current, main, or scene_path.", false));

			arguments.push_back(_make_prompt_argument("scene_path", "res:// scene path when target is scene_path.", false));

			arguments.push_back(_make_prompt_argument("input_sequence", "Optional natural-language input sequence to drive after launch.", false));

			arguments.push_back(_make_prompt_argument("stop_after", "Whether to stop the game after inspection: true or false.", false));

			break;

		case AUTOWORK_FIX_LOOP:
			arguments.push_back(_make_prompt_argument("test_scope", "Autowork scope: all, directory, script, or name.", false));

			arguments.push_back(_make_prompt_argument("target", "Directory, script path, or test name for focused Autowork execution.", false));

			arguments.push_back(_make_prompt_argument("patch_mode", "Patch behavior: suggest or apply.", false));

			break;

		case DIAGNOSTICS_TRIAGE:
			arguments.push_back(_make_prompt_argument("symptom", "Observed symptom, error, or behavior to diagnose.", false));

			arguments.push_back(_make_prompt_argument("scope", "Diagnosis scope: all, editor, runtime, scene, project, performance.", false));

			arguments.push_back(_make_prompt_argument("detail_level", "Output detail level: brief, normal, deep.", false));

			break;
	}

	return arguments;
}

Dictionary JustAMCPPromptBlaziumWorkflow::get_prompt() const {
	Dictionary result;

	result["name"] = get_name();

	result["title"] = _get_title();

	result["description"] = _get_description();

	result["arguments"] = _get_arguments();

	return result;
}

void JustAMCPPromptBlaziumWorkflow::_append_common_context(Array &r_messages) const {
	r_messages.push_back(_make_resource_message("blazium://project/info"));

	r_messages.push_back(_make_resource_message("blazium://scene/current"));

	r_messages.push_back(_make_resource_message("blazium://guide/tool-index"));
}

Dictionary JustAMCPPromptBlaziumWorkflow::_get_project_intake_messages(const Dictionary &p_args) {
	String scope = p_args.has("scope") ? String(p_args["scope"]) : "all";

	String detail_level = p_args.has("detail_level") ? String(p_args["detail_level"]) : "normal";

	Array messages;

	String text = "You are preparing to work inside a Blazium Engine project through JustAMCP.\n\n";

	text += "Goal: perform a project intake before any substantial editing or runtime operation.\n";

	text += "Requested scope: " + scope + "\n";

	text += "Detail level: " + detail_level + "\n\n";

	text += "Process:\n";

	text += "1. Read the embedded project, scene, selection, tool-index, and troubleshooting resources.\n";

	text += "2. Summarize project readiness, active scene state, current selection, and the safest JustAMCP tool families for the next action.\n";

	text += "3. If context is missing, call targeted tools such as `blazium_scene_tree_dump`, `blazium_get_project_info`, `blazium_get_guide`, or `blazium_search_tools`.\n";

	text += "4. Return a concise intake report with: current state, likely next tools, risks, and what information is still needed.\n";

	text += "Do not mutate the project during intake.";

	messages.push_back(_make_text_message(text));

	_append_common_context(messages);

	messages.push_back(_make_resource_message("blazium://selection/current"));

	messages.push_back(_make_resource_message("blazium://guide/troubleshooting"));

	Dictionary result;

	result["description"] = "Blazium Project Intake";

	result["messages"] = messages;

	result["ok"] = true;

	return result;
}

Dictionary JustAMCPPromptBlaziumWorkflow::_get_scene_build_messages(const Dictionary &p_args) {
	Dictionary validation = _validate_required_arguments(p_args, Vector<String>{ "feature" });

	if (!validation.get("ok", false)) {
		return validation;
	}

	String feature = String(p_args["feature"]);

	String scene_path = p_args.has("scene_path") ? String(p_args["scene_path"]) : "";

	String output_mode = p_args.has("output_mode") ? String(p_args["output_mode"]) : "plan";

	Array messages;

	String text = "You are a senior Blazium game developer building a scene workflow through JustAMCP.\n\n";

	text += "Feature to build: " + feature + "\n";

	text += "Scene path: " + (scene_path.is_empty() ? "[current or new scene]" : scene_path) + "\n";

	text += "Output mode: " + output_mode + "\n\n";

	text += "Workflow:\n";

	text += "1. Inspect embedded project and scene context before choosing tools.\n";

	text += "2. Use `blazium_docs_search` or `blazium_classdb_query` before relying on an unfamiliar class, node, method, signal, or property.\n";

	text += "3. For planning, produce a scene tree, script list, signal map, resources, and exact JustAMCP tool sequence.\n";

	text += "4. For apply mode, use deterministic tools in order: scene creation/opening, node creation, script creation/patching, resource assignment, signal connection, validation, and save.\n";

	text += "5. Validate with `blazium_scene_validate`, `blazium_validate_script`, `blazium_list_connections`, and `blazium_scene_tree_dump` before reporting completion.\n\n";

	text += "Rules:\n";

	text += "- Prefer composition, PackedScene boundaries, call-down/signal-up communication, typed GDScript, and explicit exported references.\n";

	text += "- Avoid hardcoded fragile node paths unless the scene owns that internal structure.\n";

	text += "- If output_mode is review, do not mutate files; return a critique and proposed tool sequence.";

	messages.push_back(_make_text_message(text));

	_append_common_context(messages);

	messages.push_back(_make_resource_message("blazium://scene/hierarchy"));

	messages.push_back(_make_resource_message("blazium://guide/scene-editing"));

	Dictionary result;

	result["description"] = "Blazium Scene Build Workflow";

	result["messages"] = messages;

	result["ok"] = true;

	return result;
}

Dictionary JustAMCPPromptBlaziumWorkflow::_get_runtime_test_loop_messages(const Dictionary &p_args) {
	String target = p_args.has("target") ? String(p_args["target"]) : "current";

	String scene_path = p_args.has("scene_path") ? String(p_args["scene_path"]) : "";

	String input_sequence = p_args.has("input_sequence") ? String(p_args["input_sequence"]) : "";

	String stop_after = p_args.has("stop_after") ? String(p_args["stop_after"]) : "true";

	Array messages;

	String text = "You are running a repeatable Blazium runtime test loop through JustAMCP.\n\n";

	text += "Run target: " + target + "\n";

	text += "Scene path: " + (scene_path.is_empty() ? "[not provided]" : scene_path) + "\n";

	text += "Input sequence: " + (input_sequence.is_empty() ? "[none]" : input_sequence) + "\n";

	text += "Stop after inspection: " + stop_after + "\n\n";

	text += "Workflow:\n";

	text += "1. Start the game with `blazium_editor_play_main`, `blazium_editor_play_scene`, or `blazium_project_run` according to the target.\n";

	text += "2. Call `blazium_wait`, then inspect readiness with `blazium_editor_is_playing`, `blazium_get_runtime_status`, `blazium_runtime_info`, and `blazium_runtime_get_errors`.\n";

	text += "3. If an input sequence is provided, drive it with `blazium_simulate_action`, `blazium_simulate_key`, `blazium_simulate_mouse_click`, or runtime UI tools.\n";

	text += "4. Capture evidence with `blazium_take_game_screenshot`, `blazium_editor_get_errors`, `blazium_runtime_get_tree`, and `blazium_runtime_batch_get_properties` when useful.\n";

	text += "5. If stop_after is true, stop with `blazium_editor_stop_play` before editing scripts.\n";

	text += "6. Return observed behavior, errors, screenshots or resource references, and next fixes.\n";

	messages.push_back(_make_text_message(text));

	_append_common_context(messages);

	messages.push_back(_make_resource_message("blazium://guide/testing-loop"));

	messages.push_back(_make_resource_message("blazium://logs/recent"));

	messages.push_back(_make_resource_message("blazium://performance"));

	Dictionary result;

	result["description"] = "Blazium Runtime Test Loop";

	result["messages"] = messages;

	result["ok"] = true;

	return result;
}

Dictionary JustAMCPPromptBlaziumWorkflow::_get_autowork_fix_loop_messages(const Dictionary &p_args) {
	String test_scope = p_args.has("test_scope") ? String(p_args["test_scope"]) : "all";

	String target = p_args.has("target") ? String(p_args["target"]) : "";

	String patch_mode = p_args.has("patch_mode") ? String(p_args["patch_mode"]) : "suggest";

	Array messages;

	String text = "You are running a Blazium Autowork test-fix loop through JustAMCP.\n\n";

	text += "Test scope: " + test_scope + "\n";

	text += "Target: " + (target.is_empty() ? "[not provided]" : target) + "\n";

	text += "Patch mode: " + patch_mode + "\n\n";

	text += "Workflow:\n";

	text += "1. Run tests with `blazium_autowork_run_all_tests`, `blazium_autowork_run_tests_in_directory`, `blazium_autowork_run_test_script`, or `blazium_autowork_run_test_by_name` based on scope.\n";

	text += "2. Read structured results from the embedded `blazium://test/results` resource and recent logs.\n";

	text += "3. Diagnose failures by category: missing scene-tree setup, assertion type mismatch, signal await hang, orphaned nodes, float precision, syntax/type errors, or changed production behavior.\n";

	text += "4. If patch_mode is suggest, return exact drop-in patches without editing. If patch_mode is apply, patch with `blazium_patch_script` or `blazium_edit_script` and then validate with `blazium_validate_script`.\n";

	text += "5. Rerun the smallest relevant test scope after each patch and stop when tests pass or a blocker requires user input.\n";

	text += "6. Return failure summary, root cause, files touched or proposed, final test state, and residual risks.\n";

	messages.push_back(_make_text_message(text));

	_append_common_context(messages);

	messages.push_back(_make_resource_message("blazium://test/results"));

	messages.push_back(_make_resource_message("blazium://logs/recent"));

	messages.push_back(_make_resource_message("blazium://guide/troubleshooting"));

	Dictionary result;

	result["description"] = "Blazium Autowork Fix Loop";

	result["messages"] = messages;

	result["ok"] = true;

	return result;
}

Dictionary JustAMCPPromptBlaziumWorkflow::_get_diagnostics_triage_messages(const Dictionary &p_args) {
	String symptom = p_args.has("symptom") ? String(p_args["symptom"]) : "";

	String scope = p_args.has("scope") ? String(p_args["scope"]) : "all";

	String detail_level = p_args.has("detail_level") ? String(p_args["detail_level"]) : "normal";

	Array messages;

	String text = "You are triaging a Blazium Engine issue through JustAMCP.\n\n";

	text += "Symptom: " + (symptom.is_empty() ? "[not provided]" : symptom) + "\n";

	text += "Scope: " + scope + "\n";

	text += "Detail level: " + detail_level + "\n\n";

	text += "Workflow:\n";

	text += "1. Read embedded project, scene, logs, performance, and troubleshooting resources.\n";

	text += "2. Gather deterministic evidence with `blazium_runtime_diagnose`, `blazium_scene_validate`, `blazium_project_state`, `blazium_profiling_detect_bottlenecks`, and `blazium_editor_get_errors` as relevant.\n";

	text += "3. Correlate symptoms with recent logs, runtime state, scene structure, project settings, missing resources, and performance counters.\n";

	text += "4. Produce a ranked diagnosis: likely cause, evidence, recommended fix, exact tools to run, and confidence.\n";

	text += "5. Do not mutate the project unless the user explicitly asks to apply a fix after triage.\n";

	messages.push_back(_make_text_message(text));

	_append_common_context(messages);

	messages.push_back(_make_resource_message("blazium://scene/hierarchy"));

	messages.push_back(_make_resource_message("blazium://logs/recent"));

	messages.push_back(_make_resource_message("blazium://performance"));

	messages.push_back(_make_resource_message("blazium://guide/troubleshooting"));

	Dictionary result;

	result["description"] = "Blazium Diagnostics Triage";

	result["messages"] = messages;

	result["ok"] = true;

	return result;
}

Dictionary JustAMCPPromptBlaziumWorkflow::get_messages(const Dictionary &p_args) {
	switch (kind) {
		case PROJECT_INTAKE:
			return _get_project_intake_messages(p_args);

		case SCENE_BUILD:
			return _get_scene_build_messages(p_args);

		case RUNTIME_TEST_LOOP:
			return _get_runtime_test_loop_messages(p_args);

		case AUTOWORK_FIX_LOOP:
			return _get_autowork_fix_loop_messages(p_args);

		case DIAGNOSTICS_TRIAGE:
			return _get_diagnostics_triage_messages(p_args);
	}

	return _make_error_result("Unknown Blazium workflow prompt.");
}

Dictionary JustAMCPPromptBlaziumWorkflow::complete(const Dictionary &p_argument) {
	String arg_name = p_argument.has("name") ? String(Variant(p_argument["name"])) : "";

	String arg_value = p_argument.has("value") ? String(Variant(p_argument["value"])) : "";

	if (arg_name == "scope") {
		return _make_completion(Vector<String>{ "all", "project", "scene", "runtime", "docs", "editor", "performance" }, arg_value);
	}

	if (arg_name == "detail_level") {
		return _make_completion(Vector<String>{ "brief", "normal", "deep" }, arg_value);
	}

	if (arg_name == "output_mode") {
		return _make_completion(Vector<String>{ "plan", "apply", "review" }, arg_value);
	}

	if (arg_name == "target") {
		return _make_completion(Vector<String>{ "current", "main", "scene_path" }, arg_value);
	}

	if (arg_name == "stop_after") {
		return _make_completion(Vector<String>{ "true", "false" }, arg_value);
	}

	if (arg_name == "test_scope") {
		return _make_completion(Vector<String>{ "all", "directory", "script", "name" }, arg_value);
	}

	if (arg_name == "patch_mode") {
		return _make_completion(Vector<String>{ "suggest", "apply" }, arg_value);
	}

	return _make_empty_completion();
}

#endif
