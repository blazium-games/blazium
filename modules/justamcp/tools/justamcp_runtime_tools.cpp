/**************************************************************************/
/*  justamcp_runtime_tools.cpp                                            */
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

#include "justamcp_runtime_tools.h"

#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"

#include "core/io/dir_access.h"
#include "core/io/image.h"
#include "core/math/expression.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_interface.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h" // IWYU pragma: keep
#include "servers/display/display_server.h"

#include "modules/gdscript/gdscript.h"

void JustAMCPRuntimeTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_process_frame"), &JustAMCPRuntimeTools::_on_process_frame);
}

Dictionary JustAMCPRuntimeTools::runtime_execute_gdscript(const Dictionary &p_args) {
	Dictionary result;
	String code_snippet = p_args.get("code", "");
	String target_path = p_args.get("target_node", "");

	if (code_snippet.is_empty()) {
		result["ok"] = false;
		result["error"] = "Code snippet is empty.";
		return result;
	}

	Node *base_obj = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface() && !target_path.is_empty()) {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			if (target_path == "." || target_path == "/root") {
				base_obj = root;
			} else {
				base_obj = root->get_node_or_null(NodePath(target_path));
			}
		}
	}

	Expression expr;
	Error expr_err = expr.parse(code_snippet);
	if (expr_err == OK) {
		Variant ret_val = expr.execute(Array(), base_obj, false, true);
		if (!expr.has_execute_failed()) {
			result["ok"] = true;
			result["evaluation"] = "Expression";
			if (ret_val.get_type() != Variant::NIL && ret_val.get_type() != Variant::OBJECT) {
				result["result"] = ret_val;
			} else if (ret_val.get_type() == Variant::OBJECT) {
				Object *obj = ret_val;
				if (obj) {
					result["result"] = String(obj->get_class());
				}
			} else {
				result["result"] = Variant();
			}
			return result;
		}
	}

	String full_script = "extends RefCounted\n";
	full_script += "func eval(node_ref: Node) -> Variant:\n";

	Vector<String> lines = code_snippet.split("\n");
	for (int i = 0; i < lines.size(); ++i) {
		full_script += "\t" + lines[i] + "\n";
	}

	Ref<GDScript> dyn_script;
	dyn_script.instantiate();
	dyn_script->set_source_code(full_script);

	Error reload_err = dyn_script->reload();
	if (reload_err != OK) {
		result["ok"] = false;
		result["error"] = "Failed to compile GDScript snippet.";
		return result;
	}

	Object *instance = ClassDB::instantiate(dyn_script->get_instance_base_type());
	Ref<RefCounted> ref_holder;
	if (Object::cast_to<RefCounted>(instance)) {
		ref_holder = Ref<RefCounted>(Object::cast_to<RefCounted>(instance));
	}

	if (instance) {
		instance->set_script(Variant(dyn_script));

		Callable::CallError err;
		Variant ret_val;

		Array args;
		args.push_back(base_obj);

		const Variant *argptrs[1];
		argptrs[0] = &args[0];

		ret_val = instance->callp(StringName("eval"), argptrs, 1, err);

		if (err.error == Callable::CallError::CALL_OK) {
			result["ok"] = true;
			result["evaluation"] = "Script";

			if (ret_val.get_type() != Variant::NIL && ret_val.get_type() != Variant::OBJECT) {
				result["result"] = ret_val;
			} else if (ret_val.get_type() == Variant::OBJECT) {
				Object *obj = ret_val;
				if (obj) {
					result["result"] = String(obj->get_class());
				}
			} else {
				result["result"] = Variant();
			}
		} else {
			result["ok"] = false;
			result["error"] = "Runtime execution call failed on method eval.";
		}

		if (!Object::cast_to<RefCounted>(instance)) {
			memdelete(instance);
		}

		return result;
	}

	result["ok"] = false;
	result["error"] = "Unable to instantiate execution context.";
	return result;
}

Dictionary JustAMCPRuntimeTools::runtime_signal_emit(const Dictionary &p_args) {
	Dictionary result;
	String target_path = p_args.get("node_path", "");
	String signal_name = p_args.get("signal_name", "");
	Array signal_args = p_args.get("args", Array());

	if (target_path.is_empty() || signal_name.is_empty()) {
		result["ok"] = false;
		result["error"] = "Requires both node_path and signal_name parameters.";
		return result;
	}

	Node *base_obj = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface() && !target_path.is_empty()) {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			if (target_path == "." || target_path == "/root") {
				base_obj = root;
			} else {
				base_obj = root->get_node_or_null(NodePath(target_path));
			}
		}
	}

	if (!base_obj) {
		result["ok"] = false;
		result["error"] = "Target node not found or scene not instantiated.";
		return result;
	}

	if (!base_obj->has_signal(StringName(signal_name))) {
		result["ok"] = false;
		result["error"] = "Signal " + signal_name + " does not exist on target node.";
		return result;
	}

	Vector<Variant> arg_storage;
	arg_storage.resize(signal_args.size());
	for (int i = 0; i < signal_args.size(); i++) {
		arg_storage.write[i] = signal_args[i];
	}

	Vector<const Variant *> argp;
	argp.resize(signal_args.size());
	for (int i = 0; i < signal_args.size(); i++) {
		argp.write[i] = &arg_storage[i];
	}

	Error em_err = base_obj->emit_signalp(StringName(signal_name), (const Variant **)argp.ptr(), argp.size());

	result["ok"] = (em_err == OK);
	if (em_err != OK) {
		result["error"] = "Failed to emit signal bindings.";
	}
	return result;
}

Dictionary JustAMCPRuntimeTools::runtime_capture_output(const Dictionary &p_args) {
	Dictionary result;
	int lines = p_args.get("lines", 50);
	bool do_clear = p_args.get("clear", false);

	Array output;
	int start_idx = MAX(0, _console_buffer.size() - lines);
	for (int i = start_idx; i < _console_buffer.size(); i++) {
		output.push_back(_console_buffer[i]);
	}

	if (do_clear) {
		_console_buffer.clear();
	}

	result["ok"] = true;
	result["line_count"] = output.size();
	result["total_buffered"] = _console_buffer.size();
	result["lines"] = output;
	return result;
}

Dictionary JustAMCPRuntimeTools::runtime_compare_screenshots(const Dictionary &p_args) {
	Dictionary result;
	String image_a_path = p_args.get("image_a", "");
	String image_b_path = p_args.get("image_b", "");
	int threshold = p_args.get("threshold", 10);

	if (image_a_path.is_empty() || image_b_path.is_empty()) {
		result["ok"] = false;
		result["error"] = "Both 'image_a' and 'image_b' paths are required.";
		return result;
	}

	Ref<Image> img_a;
	img_a.instantiate();
	Ref<Image> img_b;
	img_b.instantiate();

	Error err_a = img_a->load(image_a_path);
	Error err_b = img_b->load(image_b_path);

	if (err_a != OK) {
		result["ok"] = false;
		result["error"] = "Failed to load image_a.";
		return result;
	}
	if (err_b != OK) {
		result["ok"] = false;
		result["error"] = "Failed to load image_b.";
		return result;
	}

	if (img_a->get_size() != img_b->get_size()) {
		result["ok"] = true;
		result["match"] = false;
		result["similarity"] = 0.0;
		result["reason"] = "Image dimensions differ.";
		return result;
	}

	int total_pixels = img_a->get_width() * img_a->get_height();
	int matching = 0;
	float threshold_f = threshold / 255.0;

	for (int y = 0; y < img_a->get_height(); y++) {
		for (int x = 0; x < img_a->get_width(); x++) {
			Color ca = img_a->get_pixel(x, y);
			Color cb = img_b->get_pixel(x, y);
			if (Math::abs(ca.r - cb.r) <= threshold_f &&
					Math::abs(ca.g - cb.g) <= threshold_f &&
					Math::abs(ca.b - cb.b) <= threshold_f) {
				matching++;
			}
		}
	}

	float similarity = (float)matching / (float)total_pixels * 100.0f;

	result["ok"] = true;
	result["match"] = similarity >= 99.0f;
	result["similarity"] = similarity;
	result["total_pixels"] = total_pixels;
	result["matching_pixels"] = matching;
	result["differing_pixels"] = total_pixels - matching;
	result["threshold"] = threshold;
	return result;
}

void JustAMCPRuntimeTools::_on_process_frame() {
	if (!_recording_video) {
		return;
	}

	if (DisplayServer::get_singleton()) {
		Ref<Image> frame = DisplayServer::get_singleton()->screen_get_image();
		if (frame.is_valid()) {
			String file_path = _current_recording_dir.path_join(vformat("frame_%06d.png", _recorded_frames));
			frame->save_png(file_path);
			_recorded_frames++;
		}
	}
}

Dictionary JustAMCPRuntimeTools::runtime_record_video(const Dictionary &p_args) {
	Dictionary result;
	String action = p_args.get("action", "");

	if (action == "start") {
		if (_recording_video) {
			result["ok"] = false;
			result["error"] = "Recording is already in progress.";
			return result;
		}

		_current_recording_dir = "res://.video_recordings";
		Ref<DirAccess> dir = DirAccess::open("res://");
		if (dir.is_valid()) {
			if (!dir->dir_exists(".video_recordings")) {
				dir->make_dir(".video_recordings");
			} else {
				Ref<DirAccess> old_dir = DirAccess::open(_current_recording_dir);
				if (old_dir.is_valid()) {
					old_dir->list_dir_begin();
					String file = old_dir->get_next();
					while (!file.is_empty()) {
						if (!old_dir->current_is_dir()) {
							old_dir->remove(file);
						}
						file = old_dir->get_next();
					}
					old_dir->list_dir_end();
				}
			}
		}

		_recording_video = true;
		_recorded_frames = 0;

		if (editor_plugin && editor_plugin->get_editor_interface() && JustAMCPEditorSceneAccess::get_edited_root()) {
			SceneTree *tree = JustAMCPEditorSceneAccess::get_edited_root()->get_tree();
			if (tree && !tree->is_connected("process_frame", callable_mp(this, &JustAMCPRuntimeTools::_on_process_frame))) {
				tree->connect("process_frame", callable_mp(this, &JustAMCPRuntimeTools::_on_process_frame));
			}
		}

		result["ok"] = true;
		result["action"] = "start";
		result["message"] = "Recording started. Sequential PNG frames dumping to res://.video_recordings/";
	} else if (action == "stop") {
		if (!_recording_video) {
			result["ok"] = false;
			result["error"] = "Not currently recording.";
			return result;
		}

		_recording_video = false;

		if (editor_plugin && editor_plugin->get_editor_interface() && JustAMCPEditorSceneAccess::get_edited_root()) {
			SceneTree *tree = JustAMCPEditorSceneAccess::get_edited_root()->get_tree();
			if (tree && tree->is_connected("process_frame", callable_mp(this, &JustAMCPRuntimeTools::_on_process_frame))) {
				tree->disconnect("process_frame", callable_mp(this, &JustAMCPRuntimeTools::_on_process_frame));
			}
		}

		result["ok"] = true;
		result["action"] = "stop";
		result["frames_captured"] = _recorded_frames;
		result["path"] = _current_recording_dir;
		result["message"] = vformat("Saved %d frames.", _recorded_frames);
		_recorded_frames = 0;
	} else {
		result["ok"] = false;
		result["error"] = "Unknown action. Use 'start' or 'stop'.";
	}
	return result;
}

Dictionary JustAMCPRuntimeTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "runtime_execute_gdscript" || p_tool_name == "execute_gdscript_snippet") {
		return runtime_execute_gdscript(p_args);
	}
	if (p_tool_name == "runtime_signal_emit" || p_tool_name == "signal_emit") {
		return runtime_signal_emit(p_args);
	}
	if (p_tool_name == "runtime_capture_output") {
		return runtime_capture_output(p_args);
	}
	if (p_tool_name == "runtime_compare_screenshots") {
		return runtime_compare_screenshots(p_args);
	}
	if (p_tool_name == "runtime_record_video") {
		return runtime_record_video(p_args);
	}
	return Dictionary();
}
