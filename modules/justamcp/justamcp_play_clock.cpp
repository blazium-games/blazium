/**************************************************************************/
/*  justamcp_play_clock.cpp                                               */
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

#include "justamcp_play_clock.h"

#include "justamcp_runtime.h"

#ifdef TOOLS_ENABLED
#include "justamcp_editor_scene_access.h"
#endif

#include "core/config/engine.h"
#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display_server.h"
#include "servers/physics_server_3d.h"

struct PlayClockState {
	bool has_seed = false;
	int seed = 0;
	bool has_fixed_fps = false;
	int fixed_fps = 0;
	bool frozen = false;
	double time_scale = 1.0;
};

static PlayClockState g_play_clock;

static Dictionary _clock_error(const String &p_message) {
	Dictionary ret;
	ret["type"] = "error";
	ret["ok"] = false;
	ret["message"] = p_message;
	ret["error"] = p_message;
	return ret;
}

static SceneTree *_main_tree() {
	if (!OS::get_singleton()) {
		return nullptr;
	}
	return Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
}

static void _apply_seed(int p_seed) {
	Math::seed(uint64_t(p_seed));
	g_play_clock.has_seed = true;
	g_play_clock.seed = p_seed;
}

static void _apply_fixed_fps(int p_fps) {
	if (Engine::get_singleton()) {
		Engine::get_singleton()->set_physics_ticks_per_second(p_fps);
		Engine::get_singleton()->set_max_fps(p_fps);
	}
	g_play_clock.has_fixed_fps = true;
	g_play_clock.fixed_fps = p_fps;
}

static void _apply_time_scale(double p_scale) {
	if (Engine::get_singleton()) {
		Engine::get_singleton()->set_time_scale(p_scale);
	}
	g_play_clock.time_scale = p_scale;
}

static void _apply_frozen(bool p_frozen) {
	g_play_clock.frozen = p_frozen;
	if (SceneTree *tree = _main_tree()) {
		tree->set_pause(p_frozen);
	}
}

static int _pump_slices(int p_slices) {
	p_slices = CLAMP(p_slices, 1, 300);
	const uint64_t frame_start = Engine::get_singleton() ? Engine::get_singleton()->get_process_frames() : 0;
	for (int i = 0; i < p_slices; i++) {
		OS::get_singleton()->delay_usec(16000);
		if (DisplayServer::get_singleton()) {
			DisplayServer::get_singleton()->process_events();
		}
	}
	if (Engine::get_singleton()) {
		const int advanced = int(Engine::get_singleton()->get_process_frames() - frame_start);
		if (advanced > 0) {
			return advanced;
		}
	}
	return p_slices;
}

static void _deliver_step_inputs(const Dictionary &p_params, Array &r_delivered) {
	if (!p_params.has("inputs") || p_params["inputs"].get_type() != Variant::ARRAY) {
		return;
	}
	JustAMCPRuntime *runtime = JustAMCPRuntime::get_singleton();
	if (!runtime) {
		return;
	}
	Array inputs = p_params["inputs"];
	const int limit = MIN(inputs.size(), 64);
	for (int i = 0; i < limit; i++) {
		if (inputs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary input = inputs[i];
		const String kind = String(input.get("kind", input.get("type", "action"))).to_lower();
		String command = "inject_action";
		if (kind == "key") {
			command = "inject_key";
		} else if (kind == "mouse" || kind == "mouse_click" || kind == "click") {
			command = "inject_mouse_click";
		}
		Dictionary delivered = runtime->execute_command(command, input);
		delivered["kind"] = kind;
		r_delivered.push_back(delivered);
	}
}

static Dictionary _step_window(const Dictionary &p_params, int p_slices) {
	SceneTree *tree = _main_tree();
	if (!tree) {
		return _clock_error("SceneTree is not available.");
	}
	const bool was_paused = tree->is_paused();
	tree->set_pause(false);
	Array delivered;
	_deliver_step_inputs(p_params, delivered);
	const int frames = _pump_slices(p_slices);
	if (was_paused || g_play_clock.frozen) {
		tree->set_pause(true);
	}
	Dictionary ret;
	ret["type"] = "step";
	ret["ok"] = true;
	ret["frames"] = frames;
	ret["paused"] = tree->is_paused();
	ret["inputs"] = delivered;
	ret["time_scale"] = Engine::get_singleton() ? Engine::get_singleton()->get_time_scale() : g_play_clock.time_scale;
	return ret;
}

Dictionary justamcp_validate_play_launch_args(const Dictionary &p_args) {
	if (p_args.has("fixed_fps")) {
		const int fps = int(p_args["fixed_fps"]);
		if (fps < 1 || fps > 240) {
			return _clock_error("fixed_fps must be from 1 to 240.");
		}
	}
	if (p_args.has("seed") && p_args["seed"].get_type() != Variant::INT && p_args["seed"].get_type() != Variant::FLOAT) {
		return _clock_error("seed must be a number.");
	}
	return Dictionary();
}

Dictionary justamcp_validate_runtime_step_args(const Dictionary &p_args) {
	const bool has_ms = p_args.has("duration_ms");
	const bool has_frames = p_args.has("frames");
	if (has_ms == has_frames) {
		return _clock_error("Provide duration_ms or frames, not both.");
	}
	if (has_frames) {
		const int frames = int(p_args["frames"]);
		if (frames < 1 || frames > 120) {
			return _clock_error("frames must be from 1 to 120.");
		}
	}
	if (has_ms) {
		const int duration_ms = int(p_args["duration_ms"]);
		if (duration_ms < 1 || duration_ms > 5000) {
			return _clock_error("duration_ms must be from 1 to 5000.");
		}
	}
	return Dictionary();
}

Dictionary justamcp_validate_runtime_step_until_args(const Dictionary &p_args) {
	const String expr = String(p_args.get("expr", p_args.get("expression", "")));
	if (expr.is_empty()) {
		return _clock_error("step_until requires expr.");
	}
	if (expr.length() > 512) {
		return _clock_error("expr is capped at 512 characters.");
	}
	const int max_frames = int(p_args.get("max_frames", 60));
	if (max_frames < 1 || max_frames > 300) {
		return _clock_error("max_frames must be from 1 to 300.");
	}
	return Dictionary();
}

Dictionary justamcp_play_clock_snapshot() {
	Dictionary clock;
	clock["has_seed"] = g_play_clock.has_seed;
	clock["seed"] = g_play_clock.seed;
	clock["has_fixed_fps"] = g_play_clock.has_fixed_fps;
	clock["fixed_fps"] = g_play_clock.fixed_fps;
	clock["frozen"] = g_play_clock.frozen;
	clock["time_scale"] = Engine::get_singleton() ? Engine::get_singleton()->get_time_scale() : g_play_clock.time_scale;
	if (SceneTree *tree = _main_tree()) {
		clock["paused"] = tree->is_paused();
	} else {
		clock["paused"] = g_play_clock.frozen;
	}
	if (Engine::get_singleton()) {
		clock["physics_ticks_per_second"] = Engine::get_singleton()->get_physics_ticks_per_second();
		clock["max_fps"] = Engine::get_singleton()->get_max_fps();
	}
	return clock;
}

void justamcp_note_play_launch_args(const Dictionary &p_args) {
	if (p_args.has("seed")) {
		g_play_clock.has_seed = true;
		g_play_clock.seed = int(p_args["seed"]);
	}
	if (p_args.has("fixed_fps")) {
		g_play_clock.has_fixed_fps = true;
		g_play_clock.fixed_fps = int(p_args["fixed_fps"]);
	}
	if (p_args.has("frozen")) {
		g_play_clock.frozen = bool(p_args["frozen"]);
	}
}

void justamcp_note_play_frozen(bool p_frozen) {
	g_play_clock.frozen = p_frozen;
}

void justamcp_note_play_time_scale(double p_scale) {
	g_play_clock.time_scale = p_scale;
}

void justamcp_prepare_play_clock_environment(const Dictionary &p_args) {
	OS *os = OS::get_singleton();
	if (!os) {
		return;
	}
	if (p_args.has("seed")) {
		os->set_environment("JUSTAMCP_PLAY_SEED", String::num_int64(int(p_args["seed"])));
	} else {
		os->unset_environment("JUSTAMCP_PLAY_SEED");
	}
	if (p_args.has("fixed_fps")) {
		os->set_environment("JUSTAMCP_PLAY_FIXED_FPS", String::num_int64(int(p_args["fixed_fps"])));
	} else {
		os->unset_environment("JUSTAMCP_PLAY_FIXED_FPS");
	}
	if (p_args.has("frozen") && bool(p_args["frozen"])) {
		os->set_environment("JUSTAMCP_PLAY_FROZEN", "1");
	} else {
		os->unset_environment("JUSTAMCP_PLAY_FROZEN");
	}
	justamcp_note_play_launch_args(p_args);
}

void justamcp_clear_play_clock_environment() {
	OS *os = OS::get_singleton();
	if (!os) {
		return;
	}
	os->unset_environment("JUSTAMCP_PLAY_SEED");
	os->unset_environment("JUSTAMCP_PLAY_FIXED_FPS");
	os->unset_environment("JUSTAMCP_PLAY_FROZEN");
	os->unset_environment("JUSTAMCP_PLAY_TIME_SCALE");
}

bool justamcp_try_play_clock_command(const String &p_command, const Dictionary &p_params, Dictionary &r_result) {
	if (p_command == "freeze" || p_command == "runtime_freeze") {
		const bool paused = bool(p_params.get("paused", true));
		_apply_frozen(paused);
		r_result["type"] = "paused";
		r_result["ok"] = true;
		r_result["paused"] = paused;
		r_result["frozen"] = g_play_clock.frozen;
		return true;
	}
	if (p_command == "set_time_scale" || p_command == "runtime_set_time_scale") {
		if (!p_params.has("time_scale") && !p_params.has("scale")) {
			r_result = _clock_error("time_scale is required.");
			return true;
		}
		const double scale = double(p_params.get("time_scale", p_params.get("scale", 1.0)));
		if (scale < 0.0 || scale > 16.0) {
			r_result = _clock_error("time_scale must be from 0 to 16.");
			return true;
		}
		_apply_time_scale(scale);
		r_result["type"] = "time_scale";
		r_result["ok"] = true;
		r_result["time_scale"] = scale;
		return true;
	}
	if (p_command == "step" || p_command == "runtime_step") {
		Dictionary invalid = justamcp_validate_runtime_step_args(p_params);
		if (!invalid.is_empty()) {
			r_result = invalid;
			return true;
		}
		int slices = 1;
		if (p_params.has("frames")) {
			slices = int(p_params["frames"]);
		} else {
			slices = MAX(1, int(p_params["duration_ms"]) / 16);
		}
		r_result = _step_window(p_params, slices);
		return true;
	}
	if (p_command == "step_until" || p_command == "runtime_step_until") {
		Dictionary invalid = justamcp_validate_runtime_step_until_args(p_params);
		if (!invalid.is_empty()) {
			r_result = invalid;
			return true;
		}
		JustAMCPRuntime *runtime = JustAMCPRuntime::get_singleton();
		if (!runtime) {
			r_result = _clock_error("JustAMCPRuntime is not live.");
			return true;
		}
		const String expr = String(p_params.get("expr", p_params.get("expression", "")));
		const int max_frames = int(p_params.get("max_frames", 60));
		bool matched = false;
		int frames = 0;
		Variant last_value;
		Dictionary step_params = p_params.duplicate();
		for (int i = 0; i < max_frames; i++) {
			Dictionary stepped = _step_window(step_params, 1);
			step_params.erase("inputs");
			if (String(stepped.get("type", "")) == "error") {
				r_result = stepped;
				return true;
			}
			frames += int(stepped.get("frames", 1));
			Dictionary eval_args;
			eval_args["expr"] = expr;
			Dictionary evaluated = runtime->execute_command("eval_expression", eval_args);
			if (String(evaluated.get("type", "")) == "error") {
				r_result = evaluated;
				r_result["ok"] = false;
				r_result["frames"] = frames;
				return true;
			}
			last_value = evaluated.get("value", Variant());
			if (bool(last_value)) {
				matched = true;
				break;
			}
		}
		r_result["type"] = "step_until";
		r_result["ok"] = true;
		r_result["matched"] = matched;
		r_result["frames"] = frames;
		r_result["value"] = last_value;
		r_result["capped"] = !matched;
		return true;
	}
	if (p_command == "click_world" || p_command == "runtime_click_world") {
		SceneTree *tree = _main_tree();
		if (!tree || !tree->get_root()) {
			r_result = _clock_error("SceneTree is not available.");
			return true;
		}
		Camera3D *camera = tree->get_root()->get_camera_3d();
#ifdef TOOLS_ENABLED
		if (!camera) {
			Node *edited = JustAMCPEditorSceneAccess::get_edited_root();
			Camera3D *scene_camera = Object::cast_to<Camera3D>(JustAMCPEditorSceneAccess::find_node(edited, "Camera3D"));
			if (scene_camera && scene_camera->is_inside_tree() && scene_camera->get_viewport()) {
				camera = scene_camera;
			}
		}
#endif
		if (!camera) {
			r_result = _clock_error("No current Camera3D.");
			return true;
		}
		Vector2 screen;
		if (p_params.has("x") || p_params.has("y")) {
			screen = Vector2(float(p_params.get("x", 0.0)), float(p_params.get("y", 0.0)));
		} else if (p_params.has("node") || p_params.has("node_path")) {
			const String node_path = String(p_params.get("node", p_params.get("node_path", "")));
			Node *node = tree->get_root()->get_node_or_null(NodePath(node_path));
			Node3D *node3d = Object::cast_to<Node3D>(node);
			if (!node3d) {
				r_result = _clock_error("3D node not found: " + node_path);
				return true;
			}
			screen = camera->unproject_position(node3d->get_global_position());
		} else {
			r_result = _clock_error("Provide x and y, or a 3D node path.");
			return true;
		}
		const Vector3 origin = camera->project_ray_origin(screen);
		const Vector3 dir = camera->project_ray_normal(screen);
		const float length = float(p_params.get("length", 1000.0));
		Dictionary hit;
		if (camera->get_world_3d().is_valid()) {
			PhysicsDirectSpaceState3D *space = camera->get_world_3d()->get_direct_space_state();
			if (space) {
				Ref<PhysicsRayQueryParameters3D> query;
				query.instantiate();
				query->set_from(origin);
				query->set_to(origin + dir * length);
				PhysicsDirectSpaceState3D::RayResult ray_result;
				if (space->intersect_ray(query->get_parameters(), ray_result)) {
					hit["position"] = ray_result.position;
					hit["normal"] = ray_result.normal;
					if (ray_result.collider) {
						hit["collider"] = ray_result.collider->get_class();
					}
				}
			}
		}
		JustAMCPRuntime *runtime = JustAMCPRuntime::get_singleton();
		Dictionary click;
		if (runtime) {
			Dictionary mouse;
			mouse["x"] = screen.x;
			mouse["y"] = screen.y;
			mouse["button"] = int(p_params.get("button", 1));
			click = runtime->execute_command("inject_mouse_click", mouse);
		}
		r_result["type"] = "click_world";
		r_result["ok"] = true;
		r_result["screen"] = screen;
		r_result["hit"] = hit;
		r_result["click"] = click;
		return true;
	}
	return false;
}

void JustAMCPRuntime::_apply_inherited_play_clock() {
	OS *os = OS::get_singleton();
	if (!os) {
		return;
	}
	if (os->has_environment("JUSTAMCP_PLAY_SEED")) {
		_apply_seed(os->get_environment("JUSTAMCP_PLAY_SEED").to_int());
	}
	if (os->has_environment("JUSTAMCP_PLAY_FIXED_FPS")) {
		const int fps = os->get_environment("JUSTAMCP_PLAY_FIXED_FPS").to_int();
		if (fps >= 1 && fps <= 240) {
			_apply_fixed_fps(fps);
		}
	}
	if (os->has_environment("JUSTAMCP_PLAY_TIME_SCALE")) {
		_apply_time_scale(os->get_environment("JUSTAMCP_PLAY_TIME_SCALE").to_float());
	}
	if (os->has_environment("JUSTAMCP_PLAY_FROZEN")) {
		_apply_frozen(os->get_environment("JUSTAMCP_PLAY_FROZEN") == "1");
	}
}
