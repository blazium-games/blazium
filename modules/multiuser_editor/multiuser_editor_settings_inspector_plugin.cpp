/**************************************************************************/
/*  multiuser_editor_settings_inspector_plugin.cpp                        */
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

#include "multiuser_editor_settings_inspector_plugin.h"

#include "editor/editor_properties.h"
#include "multiuser_editor_network.h"
#include "multiuser_editor_plugin.h"
#include "multiuser_editor_settings_ui.h"
#include "scene/gui/label.h"

bool MultiuserEditorSettingsInspectorPlugin::can_handle(Object *p_object) {
	String cname = p_object->get_class();
	return cname == "EditorSettings" || cname == "SectionedInspectorFilter";
}

bool MultiuserEditorSettingsInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "blazium/multiuser_editor/z_connection_controls" || p_path == "z_connection_controls") {
		add_custom_control(memnew(MultiuserEditorSettingsUI));
		return true;
	}

	if (!p_path.begins_with("blazium/multiuser_editor/")) {
		return false;
	}

	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin) {
		return false;
	}

	const bool is_killswitch = p_path == "blazium/multiuser_editor/enabled" ||
			p_path == "blazium/multiuser_editor/role" ||
			p_path == "blazium/multiuser_editor/auto_host";

	const bool client_locked = is_killswitch && plugin->is_connected_as_client();
	const bool non_admin_locked = !plugin->is_local_admin();

	if (!client_locked && !non_admin_locked) {
		return false;
	}

	String lock_suffix;
	if (client_locked) {
		lock_suffix = " (Host-only while connected)";
	} else if (non_admin_locked) {
		lock_suffix = " (Admin only)";
	} else {
		lock_suffix = " (Locked)";
	}

	EditorProperty *editor = EditorInspectorDefaultPlugin::get_editor_for_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide);
	if (!editor) {
		Label *locked_label = memnew(Label);
		locked_label->set_text(vformat("Locked%s", lock_suffix));
		locked_label->set_tooltip_text(vformat("'%s' is locked%s. Only an Admin can modify multiuser editor settings while a session is active.", p_path, lock_suffix));
		add_custom_control(locked_label);
		return true;
	}
	editor->set_read_only(true);
	editor->set_tooltip_text(vformat("%s%s", p_path, lock_suffix));
	add_property_editor(p_path, editor);
	return true;
}

#endif
