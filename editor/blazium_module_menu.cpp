/**************************************************************************/
/*  blazium_module_menu.cpp                                               */
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

#include "blazium_module_menu.h"

#include "core/error/error_macros.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/scroll_container.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

// Drawn inside the window so the screen can be dragged even when the OS frame
// is missing. The title bar moves the parent Window.
class BlaziumModuleWindowFrame : public PanelContainer {
	GDCLASS(BlaziumModuleWindowFrame, PanelContainer);

	bool dragging = false;
	Point2i drag_mouse;
	Point2i drag_origin;

	void _title_input(const Ref<InputEvent> &p_event) {
		Window *host = get_window();
		if (!host) {
			return;
		}
		Ref<InputEventMouseButton> button = p_event;
		if (button.is_valid() && button->get_button_index() == MouseButton::LEFT) {
			if (button->is_pressed()) {
				dragging = true;
				drag_mouse = DisplayServer::get_singleton()->mouse_get_position();
				drag_origin = host->get_position();
			} else {
				dragging = false;
			}
		}
		if (!dragging) {
			return;
		}
		Ref<InputEventMouseMotion> motion = p_event;
		if (motion.is_valid()) {
			const Point2i mouse = DisplayServer::get_singleton()->mouse_get_position();
			host->set_position(drag_origin + (mouse - drag_mouse));
		}
	}

protected:
	static void _bind_methods() {}

public:
	void build(Window *p_window, const String &p_title, Control *p_content) {
		VBoxContainer *box = memnew(VBoxContainer);
		box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		add_child(box);

		HBoxContainer *title = memnew(HBoxContainer);
		title->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		title->set_custom_minimum_size(Size2(0, 32 * EDSCALE));
		title->set_default_cursor_shape(Control::CURSOR_MOVE);
		title->connect(SNAME("gui_input"), callable_mp(this, &BlaziumModuleWindowFrame::_title_input));
		box->add_child(title);

		Label *label = memnew(Label);
		label->set_text(p_title);
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		title->add_child(label);

		Button *close = memnew(Button);
		close->set_text(TTR("Close"));
		close->connect(SceneStringName(pressed), callable_mp(p_window, &Window::hide));
		title->add_child(close);

		// Module panels can be taller than the window (autowrapped labels
		// report a large minimum height before they have a width), so scroll
		// instead of letting the content overflow the window.
		ScrollContainer *scroll = memnew(ScrollContainer);
		scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
		scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		box->add_child(scroll);

		p_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		p_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		scroll->add_child(p_content);
	}
};

BlaziumModuleMenu *BlaziumModuleMenu::singleton = nullptr;

BlaziumModuleMenu::BlaziumModuleMenu() {
	singleton = this;
	root_menu = memnew(PopupMenu);
	root_menu->set_name("Blazium");
	root_menu->hide();
}

BlaziumModuleMenu::~BlaziumModuleMenu() {
	// Submenus are children of the editor menu bar and are freed with the
	// scene tree before EditorNode deletes this object.
	module_menus.clear();
	if (singleton == this) {
		singleton = nullptr;
	}
}

PopupMenu *BlaziumModuleMenu::_get_module_menu(const String &p_module) {
	if (PopupMenu **found = module_menus.getptr(p_module)) {
		return *found;
	}
	PopupMenu *menu = memnew(PopupMenu);
	menu->set_name(p_module);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &BlaziumModuleMenu::_on_id_pressed));
	root_menu->add_submenu_node_item(p_module, menu);
	module_menus.insert(p_module, menu);
	return menu;
}

void BlaziumModuleMenu::add_window(const String &p_module, const String &p_title, Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_COND_MSG(p_control->get_parent() != nullptr, "Blazium window control must not already have a parent.");

	MenuEntry entry;
	entry.id = next_id++;
	entry.module = p_module;
	entry.title = p_title;
	entry.control = p_control;
	entry.is_window = true;
	entries.push_back(entry);
	_get_module_menu(p_module)->add_item(p_title, entry.id);
}

void BlaziumModuleMenu::add_action(const String &p_module, const String &p_title, const Callable &p_callable) {
	MenuEntry entry;
	entry.id = next_id++;
	entry.module = p_module;
	entry.title = p_title;
	entry.action = p_callable;
	entry.is_window = false;
	entries.push_back(entry);
	_get_module_menu(p_module)->add_item(p_title, entry.id);
}

void BlaziumModuleMenu::_release_entry(int p_index) {
	const MenuEntry entry = entries[p_index];
	if (PopupMenu **found = module_menus.getptr(entry.module)) {
		PopupMenu *menu = *found;
		const int idx = menu->get_item_index(entry.id);
		if (idx >= 0) {
			menu->remove_item(idx);
		}
	}
	if (entry.control && windows.has(entry.control)) {
		Window *window = windows[entry.control];
		windows.erase(entry.control);
		if (window) {
			if (Node *parent = entry.control->get_parent()) {
				parent->remove_child(entry.control);
			}
			window->hide();
			window->queue_free();
		}
	}
	entries.remove_at(p_index);
}

void BlaziumModuleMenu::remove_item(const String &p_module, const String &p_title) {
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].module == p_module && entries[i].title == p_title) {
			_release_entry(i);
			return;
		}
	}
}

void BlaziumModuleMenu::_open_window(Control *p_control, const String &p_title) {
	ERR_FAIL_NULL(p_control);
	Window *window = nullptr;
	if (Window **found = windows.getptr(p_control)) {
		window = *found;
	}
	if (!window) {
		window = memnew(Window);
		// Windows start visible. Hide it so the settings below (force_native
		// can't change while shown) and the placement further down apply.
		window->set_visible(false);
		window->set_title(p_title);
		window->set_force_native(true);
		// The OS frame was not showing, so the frame below is the border and the drag handle.
		window->set_flag(Window::FLAG_BORDERLESS, true);
		window->set_flag(Window::FLAG_POPUP, false);
		window->set_flag(Window::FLAG_RESIZE_DISABLED, false);
		window->set_transient(false);
		window->set_exclusive(false);
		window->set_wrap_controls(false);
		window->set_keep_title_visible(true);
		window->set_min_size(Size2i(int(480 * EDSCALE), int(320 * EDSCALE)));
		window->connect("close_requested", callable_mp(window, &Window::hide));
		EditorNode::get_singleton()->get_gui_base()->add_child(window);
		BlaziumModuleWindowFrame *frame = memnew(BlaziumModuleWindowFrame);
		frame->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		frame->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		frame->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		frame->build(window, p_title, p_control);
		window->add_child(frame);
		windows.insert(p_control, window);
	}
	if (!window->is_visible()) {
		Window *editor_window = EditorNode::get_singleton()->get_window();
		int screen = DisplayServerEnums::SCREEN_PRIMARY;
		if (editor_window && editor_window->get_window_id() != DisplayServerEnums::INVALID_WINDOW_ID) {
			screen = DisplayServer::get_singleton()->window_get_current_screen(editor_window->get_window_id());
		}
		if (screen < 0) {
			screen = DisplayServer::get_singleton()->get_primary_screen();
		}
		const Rect2i usable = DisplayServer::get_singleton()->screen_get_usable_rect(screen);
		// Leave room for the OS caption. A client rect flush with the top of the
		// screen puts the title bar above the monitor, so the window cannot be dragged.
		const int chrome = MAX(48, int(32 * EDSCALE));
		Size2i wanted = Size2i(int(960 * EDSCALE), int(640 * EDSCALE));
		Size2i limit = usable.size - Size2i(chrome * 2, chrome * 2);
		limit.x = MAX(limit.x, 1);
		limit.y = MAX(limit.y, 1);
		wanted.x = MIN(wanted.x, limit.x);
		wanted.y = MIN(wanted.y, limit.y);
		window->set_min_size(Size2i(MIN(window->get_min_size().x, wanted.x), MIN(window->get_min_size().y, wanted.y)));
		window->set_current_screen(screen);
		window->set_size(wanted);

		Point2i pos = usable.position + (usable.size - wanted) / 2;
		const int step = int(28 * EDSCALE);
		pos += Point2i(window_cascade * step, window_cascade * step);
		window_cascade = (window_cascade + 1) % 8;

		const int min_x = usable.position.x + chrome;
		const int min_y = usable.position.y + chrome;
		const int max_x = MAX(min_x, usable.position.x + usable.size.x - wanted.x - chrome);
		const int max_y = MAX(min_y, usable.position.y + usable.size.y - wanted.y - chrome);
		pos.x = CLAMP(pos.x, min_x, max_x);
		pos.y = CLAMP(pos.y, min_y, max_y);
		window->set_position(pos);
		window->set_visible(true);
		// The size was set while the window was hidden. Apply it again now that
		// the native window exists, or it can stay blank until it's resized.
		window->set_size(wanted);

		const Point2i decorated = window->get_position_with_decorations();
		Point2i nudge;
		if (decorated.x < usable.position.x) {
			nudge.x = usable.position.x - decorated.x;
		}
		if (decorated.y < usable.position.y) {
			nudge.y = usable.position.y - decorated.y;
		}
		if (nudge != Point2i()) {
			window->set_position(window->get_position() + nudge);
		}
	}
	window->grab_focus();
}

void BlaziumModuleMenu::_on_id_pressed(int p_id) {
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].id != p_id) {
			continue;
		}
		if (entries[i].is_window) {
			_open_window(entries[i].control, entries[i].module + " - " + entries[i].title);
		} else if (entries[i].action.is_valid()) {
			entries[i].action.call();
		}
		return;
	}
}
