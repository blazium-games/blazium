/**************************************************************************/
/*  justamcp_runtime_query_ui.cpp                                         */
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
#include "servers/audio/audio_server.h"

#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h"
#endif

void JustAMCPRuntime::_find_ui_elements_recursive(Node *p_node, const String &p_text, const String &p_type, bool p_visible_only, int p_limit, Array &r_results) {
	if (r_results.size() >= p_limit) {
		return;
	}

	Control *control = Object::cast_to<Control>(p_node);
	if (control) {
		bool visible = control->is_visible_in_tree();
		bool hit = !p_visible_only || visible;
		if (!p_type.is_empty()) {
			hit = hit && control->is_class(p_type);
		}
		String text;
		bool valid = false;
		Variant raw_text = control->get("text", &valid);
		if (valid) {
			text = raw_text;
		}
		if (!p_text.is_empty()) {
			hit = hit && text.containsn(p_text);
		}
		if (hit) {
			Dictionary entry;
			entry["name"] = String(control->get_name());
			entry["type"] = control->get_class();
			entry["path"] = String(control->get_path());
			entry["text"] = text;
			entry["visible"] = visible;
			entry["position"] = control->get_global_position();
			entry["size"] = control->get_size();
			r_results.push_back(entry);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_ui_elements_recursive(p_node->get_child(i), p_text, p_type, p_visible_only, p_limit, r_results);
	}
}

Node *JustAMCPRuntime::_find_button_by_text_recursive(Node *p_node, const String &p_text) {
	BaseButton *button = Object::cast_to<BaseButton>(p_node);
	if (button) {
		bool valid = false;
		Variant raw_text = button->get("text", &valid);
		if (valid && String(raw_text).containsn(p_text)) {
			return button;
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *found = _find_button_by_text_recursive(p_node->get_child(i), p_text);
		if (found) {
			return found;
		}
	}
	return nullptr;
}

Node *JustAMCPRuntime::_find_button_recursive(Node *p_node, const String &p_name) {
	if (String(p_node->get_name()) == p_name) {
		return p_node;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *found = _find_button_recursive(p_node->get_child(i), p_name);
		if (found) {
			return found;
		}
	}
	return nullptr;
}

Dictionary JustAMCPRuntime::_cmd_capture_screenshot(const Dictionary &p_params) {
	uint64_t now_ms = Time::get_singleton()->get_ticks_msec();
	uint64_t cutoff = now_ms > (uint64_t)_SCREENSHOT_RATE_WINDOW_MS ? now_ms - (uint64_t)_SCREENSHOT_RATE_WINDOW_MS : 0;

	Vector<uint64_t> kept;
	for (int i = 0; i < _screenshot_timestamps.size(); i++) {
		if (_screenshot_timestamps[i] > cutoff) {
			kept.push_back(_screenshot_timestamps[i]);
		}
	}
	_screenshot_timestamps = kept;

	if (_screenshot_timestamps.size() >= _SCREENSHOT_RATE_MAX) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = vformat("Screenshot rate limit exceeded (max %d per second)", _SCREENSHOT_RATE_MAX);
		return ret;
	}
	_screenshot_timestamps.push_back(now_ms);

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree or viewport available";
		return ret;
	}

	Ref<ViewportTexture> tex = tree->get_root()->get_texture();
	if (tex.is_null()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No viewport texture";
		return ret;
	}

	Ref<Image> img = tex->get_image();
	if (img.is_null()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "get_image() failed";
		return ret;
	}

	Vector<uint8_t> png_data = img->save_png_to_buffer();
	String b64 = CryptoCore::b64_encode_str(png_data.ptr(), png_data.size());

	Dictionary ret;
	ret["type"] = "screenshot";
	ret["width"] = img->get_width();
	ret["height"] = img->get_height();
	ret["png_base64"] = b64;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_capture_viewport(const Dictionary &p_params) {
	return _cmd_capture_screenshot(p_params);
}

Dictionary JustAMCPRuntime::_cmd_press_button(const Dictionary &p_params) {
	String button_name = p_params.get("name", "");
	if (button_name.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing 'name'";
		return ret;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Node *found = _find_button_recursive(tree->get_root(), button_name);
	if (!found) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node not found: " + button_name;
		return ret;
	}

	BaseButton *btn = Object::cast_to<BaseButton>(found);
	if (!btn) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Node is not a BaseButton: " + found->get_class();
		return ret;
	}

	if (btn->is_toggle_mode()) {
		btn->set_pressed(!btn->is_pressed());
	}

	List<Object::Connection> connection_list;
	btn->get_signal_connection_list("pressed", &connection_list);
	int connection_count = 0;
	for (const Object::Connection &c : connection_list) {
		c.callable.call();
		connection_count++;
	}

	Dictionary ret;
	ret["type"] = "button_pressed";
	ret["node"] = String(found->get_path());
	ret["connections_triggered"] = connection_count;
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_find_ui_elements(const Dictionary &p_params) {
	String text = p_params.get("text", "");
	String type = p_params.get("ui_type", p_params.get("type", ""));
	bool visible_only = p_params.get("visible_only", true);
	int limit = p_params.get("limit", 100);
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "No SceneTree available";
		return ret;
	}

	Array elements;
	_find_ui_elements_recursive(tree->get_root(), text, type, visible_only, limit, elements);
	Dictionary ret;
	ret["type"] = "ui_elements";
	ret["elements"] = elements;
	ret["count"] = elements.size();
	return ret;
}

Dictionary JustAMCPRuntime::_cmd_click_button_by_text(const Dictionary &p_params) {
	String text = p_params.get("text", "");
	if (text.is_empty()) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Missing 'text'";
		return ret;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *found = tree && tree->get_root() ? _find_button_by_text_recursive(tree->get_root(), text) : nullptr;
	BaseButton *button = Object::cast_to<BaseButton>(found);
	if (!button) {
		Dictionary ret;
		ret["type"] = "error";
		ret["message"] = "Button not found with text: " + text;
		return ret;
	}

	List<Object::Connection> connection_list;
	button->get_signal_connection_list("pressed", &connection_list);
	int connection_count = 0;
	for (const Object::Connection &c : connection_list) {
		c.callable.call();
		connection_count++;
	}
	Dictionary ret;
	ret["type"] = "button_pressed";
	ret["node"] = String(button->get_path());
	ret["text"] = text;
	ret["connections_triggered"] = connection_count;
	return ret;
}
