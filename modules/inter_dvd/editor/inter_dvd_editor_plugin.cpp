/**************************************************************************/
/*  inter_dvd_editor_plugin.cpp                                           */
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

#include "inter_dvd_editor_plugin.h"

#include "export/windows_inter_dvd_export_platform.h"
#include "inter_dvd_scene_baker.h"
#include "modules/inter_dvd/author/inter_dvd_ifo_writer.h"
#include "modules/inter_dvd/author/inter_dvd_project.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"
#include "scene/gui/button.h"

bool EditorInspectorPluginInterDVDCell::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDCell>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDCell::parse_begin(Object *p_object) {
	InterDVDCell *cell = Object::cast_to<InterDVDCell>(p_object);
	if (!cell) {
		return;
	}
	Button *bake = memnew(Button);
	bake->set_text(TTR("Bake Scene Now"));
	bake->connect(SNAME("pressed"), callable_mp(this, &EditorInspectorPluginInterDVDCell::_bake_pressed).bind(cell->get_instance_id()));
	add_custom_control(bake);
}

void EditorInspectorPluginInterDVDCell::_bake_pressed(ObjectID p_id) {
	InterDVDCell *cell = Object::cast_to<InterDVDCell>(ObjectDB::get_instance(p_id));
	if (!cell) {
		return;
	}
	String ffmpeg;
	bool auto_find = true;
	if (EditorSettings::get_singleton()) {
		ffmpeg = EDITOR_GET("export/inter_dvd/ffmpeg");
		auto_find = EDITOR_GET("export/inter_dvd/auto_find_ffmpeg");
	}
	String err;
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	EditorProgress ep("inter_dvd_bake", TTR("Interactive DVD"), 1);
	ep.step(vformat(TTR("Baking scene…  %s elapsed"), InterDVDExportProgress::format_clock(0)), 0, true);
	const Error code = InterDVDSceneBaker::bake_cell(Ref<InterDVDCell>(cell), ffmpeg, auto_find, &err);
	const String elapsed = InterDVDExportProgress::format_clock(int64_t(OS::get_singleton()->get_ticks_usec() - started));
	ep.step(vformat(TTR("Bake finished  %s elapsed"), elapsed), 1, true);
	ERR_FAIL_COND_MSG(code != OK, err.is_empty() ? String("InterDVD scene bake failed.") : err);
}

bool EditorInspectorPluginInterDVDButton::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDButton>(p_object) != nullptr;
}

bool EditorInspectorPluginInterDVDButton::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	(void)p_type;
	(void)p_hint;
	(void)p_hint_text;
	(void)p_usage;
	(void)p_wide;
	if (p_path != "target" && p_path != "title_n") {
		return false;
	}
	InterDVDButton *btn = Object::cast_to<InterDVDButton>(p_object);
	if (!btn) {
		return false;
	}

	Vector<String> options;
	const InterDVDButton::Action action = btn->get_action();
	Ref<InterDVDProject> project;
	const String project_path = GLOBAL_GET("blazium/inter_dvd/project");
	if (!project_path.is_empty()) {
		project = ResourceLoader::load(project_path);
	}
	if (p_path == "title_n" && action == InterDVDButton::ACTION_JUMP_CHAPTER && project.is_valid()) {
		const TypedArray<InterDVDPGC> titles = project->get_titles();
		for (int i = 0; i < titles.size(); i++) {
			const Ref<InterDVDPGC> title = titles[i];
			String label = title.is_valid() ? title->get_name() : String();
			if (label.is_empty() && title.is_valid() && title->get_cells().size() > 0) {
				const Ref<InterDVDCell> cell = title->get_cells()[0];
				if (cell.is_valid()) {
					label = cell->get_display_name();
				}
			}
			if (label.is_empty()) {
				label = vformat("Title %d", i + 1);
			}
			options.push_back(vformat("%d — %s:%d", i + 1, label, i + 1));
		}
	} else if (action == InterDVDButton::ACTION_JUMP_MENU) {
		options.push_back("Title Menu:2");
		options.push_back("Root Menu:3");
		options.push_back("Subpicture Menu:4");
		options.push_back("Audio Menu:5");
		options.push_back("Angle Menu:6");
		options.push_back("Chapter Menu:7");
	} else if (action == InterDVDButton::ACTION_JUMP_CHAPTER && p_path == "target") {
		if (project.is_valid()) {
			const TypedArray<InterDVDPGC> titles = project->get_titles();
			const int title_idx = CLAMP(btn->get_title_n() - 1, 0, MAX(titles.size() - 1, 0));
			if (title_idx < titles.size()) {
				const Ref<InterDVDPGC> title = titles[title_idx];
				if (title.is_valid()) {
					const TypedArray<InterDVDCell> cells = title->get_cells();
					for (int c = 0; c < cells.size(); c++) {
						const Ref<InterDVDCell> cell = cells[c];
						String label = cell.is_valid() ? cell->get_display_name() : String();
						if (label.is_empty()) {
							label = vformat("Chapter %d", c + 1);
						}
						options.push_back(vformat("%d — %s:%d", c + 1, label, c + 1));
					}
				}
			}
		}
	} else if (action == InterDVDButton::ACTION_JUMP_TITLE || action == InterDVDButton::ACTION_JUMP_PGC) {
		if (project.is_valid()) {
			const TypedArray<InterDVDPGC> titles = project->get_titles();
			for (int i = 0; i < titles.size(); i++) {
				const Ref<InterDVDPGC> title = titles[i];
				String label = title.is_valid() ? title->get_name() : String();
				if (label.is_empty() && title.is_valid() && title->get_cells().size() > 0) {
					const Ref<InterDVDCell> cell = title->get_cells()[0];
					if (cell.is_valid()) {
						label = cell->get_display_name();
					}
				}
				if (label.is_empty()) {
					label = vformat("Title %d", i + 1);
				}
				if (action == InterDVDButton::ACTION_JUMP_PGC) {
					options.push_back(vformat("PGC %d — %s:%d", i + 1, label, i + 1));
				} else {
					options.push_back(vformat("%d — %s:%d", i + 1, label, i + 1));
				}
			}
		}
	}
	if (options.is_empty()) {
		return false;
	}

	EditorPropertyEnum *editor = memnew(EditorPropertyEnum);
	editor->setup(options);
	add_property_editor(p_path, editor);
	return true;
}

void InterDVDEditorPlugin::_bind_methods() {
}

void InterDVDEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		EDITOR_DEF_BASIC("export/inter_dvd/auto_find_ffmpeg", true);
		EDITOR_DEF_BASIC("export/inter_dvd/ffmpeg", "");
		EDITOR_DEF_BASIC("export/inter_dvd/iso_tool", "");
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "export/inter_dvd/auto_find_ffmpeg"));
			EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/inter_dvd/ffmpeg", PROPERTY_HINT_GLOBAL_FILE));
			EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/inter_dvd/iso_tool", PROPERTY_HINT_GLOBAL_FILE));
		}

		if (EditorExport::get_singleton()) {
			Ref<EditorExportPlatformWindowsInterDVD> platform;
			platform.instantiate();
			export_platform = platform;
			EditorExport::get_singleton()->add_export_platform(export_platform);
		}

		inspector_plugin.instantiate();
		add_inspector_plugin(inspector_plugin);
		button_inspector_plugin.instantiate();
		add_inspector_plugin(button_inspector_plugin);
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (button_inspector_plugin.is_valid()) {
			remove_inspector_plugin(button_inspector_plugin);
			button_inspector_plugin.unref();
		}
		if (inspector_plugin.is_valid()) {
			remove_inspector_plugin(inspector_plugin);
			inspector_plugin.unref();
		}
		if (export_platform.is_valid() && EditorExport::get_singleton()) {
			EditorExport::get_singleton()->remove_export_platform(export_platform);
			export_platform.unref();
		}
	}
}

#endif
