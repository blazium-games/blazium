/**************************************************************************/
/*  justamcp_runtime_input.cpp                                            */
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

#include "justamcp_runtime.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "main/performance.h"
#include "scene/gui/base_button.h"
#include "scene/gui/control.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/audio_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

void JustAMCPRuntime::_inject_event(const Ref<InputEvent> &p_event) {
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree && tree->get_root()) {
		tree->get_root()->push_input(p_event);

		static String input_mode = OS::get_singleton()->get_environment("BLAZIUM_MCP_INPUT_MODE");
		if (input_mode == "os") {
			Ref<InputEventMouse> mouse_ev = p_event;
			if (mouse_ev.is_valid() && Input::get_singleton()) {
				Input::get_singleton()->warp_mouse(mouse_ev->get_global_position());
			}
		}
	}
}

Dictionary JustAMCPRuntime::_cmd_inject_action(const Dictionary &p_params) {
	String action = p_params.get("action", "");
	if (action.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing 'action'";
		return ret;
	}

	const bool has_pressed = p_params.has("pressed") || p_params.has("hold");
	const bool pressed = p_params.has("pressed") ? bool(p_params.get("pressed", true)) : true;

	if (!has_pressed || pressed) {
		Ref<InputEventAction> press;
		press.instantiate();
		press->set_action(action);
		press->set_pressed(true);
		_inject_event(press);
	}
	if (!has_pressed || !pressed) {
		Ref<InputEventAction> release;
		release.instantiate();
		release->set_action(action);
		release->set_pressed(false);
		_inject_event(release);
	}

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["action"] = action;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_key(const Dictionary &p_params) {
	String action = p_params.get("action", "");
	int keycode = p_params.get("keycode", -1);

	if (!action.is_empty()) {
		return _cmd_inject_action(p_params);
	}

	if (keycode < 0) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Provide 'action' or 'keycode'";
		return ret;
	}

	const bool has_pressed = p_params.has("pressed") || p_params.has("hold");
	const bool pressed = p_params.has("pressed") ? bool(p_params.get("pressed", true)) : true;

	if (!has_pressed || pressed) {
		Ref<InputEventKey> press;
		press.instantiate();
		press->set_keycode((Key)keycode);
		press->set_pressed(true);
		_inject_event(press);
	}
	if (!has_pressed || !pressed) {
		Ref<InputEventKey> release;
		release.instantiate();
		release->set_keycode((Key)keycode);
		release->set_pressed(false);
		_inject_event(release);
	}

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["keycode"] = keycode;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_mouse_click(const Dictionary &p_params) {
	float x = p_params.get("x", 0.0f);
	float y = p_params.get("y", 0.0f);
	Vector2 pos(x, y);

	const bool has_pressed = p_params.has("pressed") || p_params.has("hold");
	const bool pressed = p_params.has("pressed") ? bool(p_params.get("pressed", true)) : true;
	MouseButton button = MouseButton::LEFT;
	const int button_index = int(p_params.get("button", 1));
	if (button_index == 2) {
		button = MouseButton::RIGHT;
	} else if (button_index == 3) {
		button = MouseButton::MIDDLE;
	}

	if (!has_pressed || pressed) {
		Ref<InputEventMouseButton> press;
		press.instantiate();
		press->set_position(pos);
		press->set_global_position(pos);
		press->set_button_index(button);
		press->set_button_mask(MouseButtonMask::LEFT);
		press->set_pressed(true);
		_inject_event(press);
	}
	if (!has_pressed || !pressed) {
		Ref<InputEventMouseButton> release;
		release.instantiate();
		release->set_position(pos);
		release->set_global_position(pos);
		release->set_button_index(button);
		release->set_button_mask(MouseButtonMask::NONE);
		release->set_pressed(false);
		_inject_event(release);
	}

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["x"] = x;
	ret["y"] = y;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_mouse_motion(const Dictionary &p_params) {
	float x = p_params.get("x", 0.0f);
	float y = p_params.get("y", 0.0f);
	float rel_x = p_params.get("relative_x", 0.0f);
	float rel_y = p_params.get("relative_y", 0.0f);
	Vector2 pos(x, y);

	Ref<InputEventMouseMotion> motion;
	motion.instantiate();
	motion->set_position(pos);
	motion->set_global_position(pos);
	motion->set_relative(Vector2(rel_x, rel_y));
	_inject_event(motion);

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["x"] = x;
	ret["y"] = y;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_drag(const Dictionary &p_params) {
	Variant from_v = p_params.get("from", Variant());
	Variant to_v = p_params.get("to", Variant());

	if (from_v.get_type() != Variant::ARRAY || to_v.get_type() != Variant::ARRAY) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "'from' and 'to' must be [x, y] arrays";
		return ret;
	}

	Array from_arr = from_v;
	Array to_arr = to_v;
	if (from_arr.size() < 2 || to_arr.size() < 2) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "'from' and 'to' must each contain at least two numbers [x, y]";
		return ret;
	}

	Vector2 pos_from = Vector2(float(from_arr[0]), float(from_arr[1]));
	Vector2 pos_to = Vector2(float(to_arr[0]), float(to_arr[1]));

	Ref<InputEventMouseMotion> motion_start;
	motion_start.instantiate();
	motion_start->set_position(pos_from);
	motion_start->set_global_position(pos_from);
	motion_start->set_relative(Vector2());
	_inject_event(motion_start);

	Ref<InputEventMouseButton> press;
	press.instantiate();
	press->set_position(pos_from);
	press->set_global_position(pos_from);
	press->set_button_index(MouseButton::LEFT);
	press->set_button_mask(MouseButtonMask::LEFT);
	press->set_pressed(true);
	_inject_event(press);

	Ref<InputEventMouseMotion> motion_drag;
	motion_drag.instantiate();
	motion_drag->set_position(pos_to);
	motion_drag->set_global_position(pos_to);
	motion_drag->set_relative(pos_to - pos_from);
	motion_drag->set_button_mask(MouseButtonMask::LEFT);
	_inject_event(motion_drag);

	Ref<InputEventMouseButton> release;
	release.instantiate();
	release->set_position(pos_to);
	release->set_global_position(pos_to);
	release->set_button_index(MouseButton::LEFT);
	release->set_button_mask(MouseButtonMask::NONE);
	release->set_pressed(false);
	_inject_event(release);

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["command"] = "inject_drag";
	ret["from"] = from_v;
	ret["to"] = to_v;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_scroll(const Dictionary &p_params) {
	float x = p_params.get("x", 0.0f);
	float y = p_params.get("y", 0.0f);
	float delta = p_params.get("delta", -3.0f);
	Vector2 pos(x, y);

	MouseButton button = delta < 0 ? MouseButton::WHEEL_DOWN : MouseButton::WHEEL_UP;

	Ref<InputEventMouseButton> press;
	press.instantiate();
	press->set_position(pos);
	press->set_global_position(pos);
	press->set_button_index(button);
	press->set_factor(Math::abs(delta));
	press->set_pressed(true);
	_inject_event(press);

	Ref<InputEventMouseButton> release;
	release.instantiate();
	release->set_position(pos);
	release->set_global_position(pos);
	release->set_button_index(button);
	release->set_factor(Math::abs(delta));
	release->set_pressed(false);
	_inject_event(release);

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["command"] = "inject_scroll";
	ret["x"] = x;
	ret["y"] = y;
	ret["delta"] = delta;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_gesture(const Dictionary &p_params) {
	String gtype = String(p_params.get("type", "")).to_lower();
	Dictionary params_dict = p_params.get("params", Dictionary());

	Variant center_v = params_dict.get("center", Variant());
	Vector2 center;
	if (center_v.get_type() == Variant::ARRAY) {
		Array ca = center_v;
		if (ca.size() >= 2) {
			center = Vector2(float(ca[0]), float(ca[1]));
		}
	}

	if (gtype == "pinch") {
		float scale_val = params_dict.get("scale", 1.1f);
		Ref<InputEventMagnifyGesture> ev;
		ev.instantiate();
		ev->set_position(center);
		ev->set_factor(scale_val);
		_inject_event(ev);

		Dictionary ret;
		ret["type"] = "input_injected";
		ret["command"] = "inject_gesture";
		ret["gesture"] = "pinch";
		return ret;
	} else if (gtype == "swipe") {
		Variant delta_v = params_dict.get("delta", Variant());
		Vector2 delta_vec;
		if (delta_v.get_type() == Variant::ARRAY) {
			Array da = delta_v;
			if (da.size() >= 2) {
				delta_vec = Vector2(float(da[0]), float(da[1]));
			}
		}
		Ref<InputEventPanGesture> ev;
		ev.instantiate();
		ev->set_position(center);
		ev->set_delta(delta_vec);
		_inject_event(ev);

		Dictionary ret;
		ret["type"] = "input_injected";
		ret["command"] = "inject_gesture";
		ret["gesture"] = "swipe";
		return ret;
	}

	Dictionary ret;
	ret["type"] = "error";
	ret["message"] = "gesture 'type' must be 'pinch' or 'swipe'";
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_inject_gamepad(const Dictionary &p_params) {
	int device = p_params.get("device", 0);
	String input_type = p_params.get("type", "button");

	if (input_type == "button") {
		int button_index = p_params.get("index", (int)JoyButton::A);
		float pressure = p_params.get("pressure", 1.0f);
		bool pressed = p_params.get("pressed", true);

		Ref<InputEventJoypadButton> ev;
		ev.instantiate();
		ev->set_device(device);
		ev->set_button_index((JoyButton)button_index);
		ev->set_pressure(pressure);
		ev->set_pressed(pressed);
		_inject_event(ev);

	} else if (input_type == "axis") {
		int axis_index = p_params.get("index", (int)JoyAxis::LEFT_X);
		float value = p_params.get("value", 1.0f);

		Ref<InputEventJoypadMotion> ev;
		ev.instantiate();
		ev->set_device(device);
		ev->set_axis((JoyAxis)axis_index);
		ev->set_axis_value(value);
		_inject_event(ev);
	}

	Dictionary ret;
	ret["type"] = "input_injected";
	ret["device"] = device;
	return ret;
}
