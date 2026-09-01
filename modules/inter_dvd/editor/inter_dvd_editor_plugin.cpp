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
#include "modules/inter_dvd/scene/inter_dvd_chapter.h"
#include "modules/inter_dvd/scene/inter_dvd_disc.h"
#include "modules/inter_dvd/scene/inter_dvd_hotspot.h"
#include "modules/inter_dvd/scene/inter_dvd_menu_page.h"
#include "modules/inter_dvd/scene/inter_dvd_title.h"
#include "modules/inter_dvd/scene/inter_dvd_title_set.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/inspector_dock.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/resources/packed_scene.h"

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
	String err;
	Ref<InterDVDProject> bake_project;
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_edited_scene()) {
		if (InterDVDDisc *disc = InterDVDDisc::find_in_tree(EditorNode::get_singleton()->get_edited_scene())) {
			bake_project = disc->build_project();
		}
	}
	InterDVDSettings::ActiveProjectGuard bake_guard(bake_project);
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	EditorProgress ep("inter_dvd_bake", TTR("Interactive DVD"), 1);
	ep.step(vformat(TTR("Baking scene…  %s elapsed"), InterDVDExportProgress::format_clock(0)), 0, true);
	const Error code = InterDVDSceneBaker::bake_cell(Ref<InterDVDCell>(cell), ffmpeg, auto_find, &err);
	const String elapsed = InterDVDExportProgress::format_clock(int64_t(OS::get_singleton()->get_ticks_usec() - started));
	ep.step(vformat(TTR("Bake finished  %s elapsed"), elapsed), 1, true);
	ERR_FAIL_COND_MSG(code != OK, err.is_empty() ? String("InterDVD scene bake failed.") : err);
}

namespace {
String inter_dvd_enum_option(const String &p_label, int p_value) {
	return p_label.replace(":", " - ") + ":" + itos(p_value);
}

String inter_dvd_title_label(const Ref<InterDVDPGC> &p_title, int p_index) {
	String label = p_title.is_valid() ? p_title->get_name() : String();
	if (label.is_empty() && p_title.is_valid() && p_title->get_cells().size() > 0) {
		const Ref<InterDVDCell> cell = p_title->get_cells()[0];
		if (cell.is_valid()) {
			label = cell->get_display_name();
		}
	}
	if (label.is_empty()) {
		label = vformat("Title %d", p_index + 1);
	}
	return vformat("%d - %s", p_index + 1, label);
}

Ref<InterDVDProject> inter_dvd_project_for_button(const InterDVDButton *p_btn) {
	if (p_btn) {
		const String path = p_btn->get_path();
		const int sep = path.find("::");
		if (sep > 0) {
			Ref<InterDVDProject> from_owner = ResourceLoader::load(path.substr(0, sep));
			if (from_owner.is_valid()) {
				return from_owner;
			}
		}
	}
	if (InspectorDock::get_inspector_singleton()) {
		InterDVDProject *project = Object::cast_to<InterDVDProject>(InspectorDock::get_inspector_singleton()->get_edited_object());
		if (project) {
			return Ref<InterDVDProject>(project);
		}
	}
	const String project_path = GLOBAL_GET("blazium/inter_dvd/project");
	if (!project_path.is_empty()) {
		return ResourceLoader::load(project_path);
	}
	return Ref<InterDVDProject>();
}
} // namespace

bool EditorInspectorPluginInterDVDButton::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDButton>(p_object) != nullptr;
}

bool EditorInspectorPluginInterDVDButton::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	(void)p_type;
	(void)p_hint;
	(void)p_hint_text;
	(void)p_wide;
	if (!(p_usage & PROPERTY_USAGE_EDITOR)) {
		return false;
	}
	if (p_path != "target" && p_path != "title_n") {
		return false;
	}
	InterDVDButton *btn = Object::cast_to<InterDVDButton>(p_object);
	if (!btn) {
		return false;
	}

	Vector<String> options;
	String label;
	const InterDVDButton::Action action = btn->get_action();
	const Ref<InterDVDProject> project = inter_dvd_project_for_button(btn);
	if (p_path == "title_n" && (action == InterDVDButton::ACTION_JUMP_TITLE || action == InterDVDButton::ACTION_JUMP_CHAPTER) && project.is_valid()) {
		const TypedArray<InterDVDPGC> titles = project->get_titles();
		for (int i = 0; i < titles.size(); i++) {
			options.push_back(inter_dvd_enum_option(inter_dvd_title_label(titles[i], i), i + 1));
		}
		label = TTR("Title");
	} else if (p_path == "target" && action == InterDVDButton::ACTION_JUMP_MENU) {
		options.push_back(inter_dvd_enum_option("Title Menu", 2));
		options.push_back(inter_dvd_enum_option("Root Menu", 3));
		options.push_back(inter_dvd_enum_option("Subpicture Menu", 4));
		options.push_back(inter_dvd_enum_option("Audio Menu", 5));
		options.push_back(inter_dvd_enum_option("Angle Menu", 6));
		options.push_back(inter_dvd_enum_option("Chapter Menu", 7));
		label = TTR("Menu");
	} else if (p_path == "target" && action == InterDVDButton::ACTION_JUMP_CHAPTER && project.is_valid()) {
		const TypedArray<InterDVDPGC> titles = project->get_titles();
		const int title_idx = CLAMP(btn->get_title_n() - 1, 0, MAX(titles.size() - 1, 0));
		if (title_idx < titles.size()) {
			const Ref<InterDVDPGC> title = titles[title_idx];
			if (title.is_valid()) {
				const TypedArray<InterDVDCell> cells = title->get_cells();
				for (int c = 0; c < cells.size(); c++) {
					const Ref<InterDVDCell> cell = cells[c];
					String chapter = cell.is_valid() ? cell->get_display_name() : String();
					if (chapter.is_empty()) {
						chapter = vformat("Chapter %d", c + 1);
					}
					options.push_back(inter_dvd_enum_option(vformat("%d - %s", c + 1, chapter), c + 1));
				}
			}
		}
		label = TTR("Chapter");
	} else if (p_path == "target" && action == InterDVDButton::ACTION_JUMP_PGC && project.is_valid()) {
		const TypedArray<InterDVDPGC> titles = project->get_titles();
		for (int i = 0; i < titles.size(); i++) {
			options.push_back(inter_dvd_enum_option(vformat("PGC %s", inter_dvd_title_label(titles[i], i)), i + 1));
		}
		label = TTR("PGC");
	}
	if (options.is_empty()) {
		return false;
	}

	EditorPropertyEnum *editor = memnew(EditorPropertyEnum);
	editor->setup(options);
	add_property_editor(p_path, editor, false, label);
	return true;
}

namespace {
const char *INTER_DVD_VIDEO_FILTER = "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob,*.m2v";

EditorFileDialog *inter_dvd_popup_file_dialog(EditorFileDialog::FileMode p_mode, EditorFileDialog::Access p_access, const String &p_filter, const String &p_filter_name) {
	EditorFileDialog *fd = memnew(EditorFileDialog);
	fd->set_file_mode(p_mode);
	fd->set_access(p_access);
	fd->add_filter(p_filter, p_filter_name);
	EditorNode::get_singleton()->get_gui_base()->add_child(fd);
	fd->connect(SNAME("canceled"), callable_mp(static_cast<Node *>(fd), &Node::queue_free));
	fd->popup_file_dialog();
	return fd;
}

void inter_dvd_add_inspector_button(VBoxContainer *p_box, const String &p_text, const Callable &p_pressed) {
	Button *btn = memnew(Button);
	btn->set_text(p_text);
	btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	btn->connect(SNAME("pressed"), p_pressed);
	p_box->add_child(btn);
}
} // namespace

bool EditorInspectorPluginInterDVDProject::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDProject>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDProject::parse_begin(Object *p_object) {
	InterDVDProject *project = Object::cast_to<InterDVDProject>(p_object);
	if (!project) {
		return;
	}
	const ObjectID id = project->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Title from Video(s)…"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_pick_videos).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Chapter to Last Title…"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_pick_chapter).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Title from Scene…"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_pick_scene).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Title Menu"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_add_title_menu).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Menu Title"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_add_menu_title).bind(id));
	inter_dvd_add_inspector_button(box, TTR("First Play → Main Title"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_ensure_first_play).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Use as Export Project"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_use_as_export_project).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDProject::_pick_videos(ObjectID p_id) {
	EditorFileDialog *fd = inter_dvd_popup_file_dialog(EditorFileDialog::FILE_MODE_OPEN_FILES, EditorFileDialog::ACCESS_FILESYSTEM, INTER_DVD_VIDEO_FILTER, TTR("Video"));
	fd->connect(SNAME("files_selected"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_videos_selected).bind(p_id, fd->get_instance_id()));
}

void EditorInspectorPluginInterDVDProject::_videos_selected(const PackedStringArray &p_paths, ObjectID p_id, ObjectID p_dialog_id) {
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(p_dialog_id))) {
		dialog->queue_free();
	}
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (project) {
		project->add_titles_from_videos(p_paths);
	}
}

void EditorInspectorPluginInterDVDProject::_pick_chapter(ObjectID p_id) {
	EditorFileDialog *fd = inter_dvd_popup_file_dialog(EditorFileDialog::FILE_MODE_OPEN_FILE, EditorFileDialog::ACCESS_FILESYSTEM, INTER_DVD_VIDEO_FILTER, TTR("Video"));
	fd->connect(SNAME("file_selected"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_chapter_selected).bind(p_id, fd->get_instance_id()));
}

void EditorInspectorPluginInterDVDProject::_chapter_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id) {
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(p_dialog_id))) {
		dialog->queue_free();
	}
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (project) {
		project->add_chapter_from_video(0, p_path);
	}
}

void EditorInspectorPluginInterDVDProject::_pick_scene(ObjectID p_id) {
	EditorFileDialog *fd = inter_dvd_popup_file_dialog(EditorFileDialog::FILE_MODE_OPEN_FILE, EditorFileDialog::ACCESS_RESOURCES, "*.tscn,*.scn,*.res", TTR("Scene"));
	fd->connect(SNAME("file_selected"), callable_mp(this, &EditorInspectorPluginInterDVDProject::_scene_selected).bind(p_id, fd->get_instance_id()));
}

void EditorInspectorPluginInterDVDProject::_scene_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id) {
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(p_dialog_id))) {
		dialog->queue_free();
	}
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (!project) {
		return;
	}
	Ref<PackedScene> scene = ResourceLoader::load(p_path, "PackedScene");
	if (scene.is_null()) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Could not load PackedScene from %s."), p_path));
		return;
	}
	project->add_title_from_scene(scene, 4.0);
}

void EditorInspectorPluginInterDVDProject::_add_title_menu(ObjectID p_id) {
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (project) {
		project->add_title_menu();
	}
}

void EditorInspectorPluginInterDVDProject::_add_menu_title(ObjectID p_id) {
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (project) {
		project->add_menu_title();
	}
}

void EditorInspectorPluginInterDVDProject::_ensure_first_play(ObjectID p_id) {
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (project) {
		project->ensure_first_play(1);
	}
}

void EditorInspectorPluginInterDVDProject::_use_as_export_project(ObjectID p_id) {
	InterDVDProject *project = Object::cast_to<InterDVDProject>(ObjectDB::get_instance(p_id));
	if (!project) {
		return;
	}
	const String path = project->get_path();
	if (path.is_empty() || path.contains("::")) {
		EditorNode::get_singleton()->show_warning(TTR("Save this InterDVDProject resource first, then use it as the export project."));
		return;
	}
	ProjectSettings::get_singleton()->set_setting("blazium/inter_dvd/project", path);
	const Error err = ProjectSettings::get_singleton()->save();
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Could not save ProjectSettings (error %d)."), err));
	}
}

bool EditorInspectorPluginInterDVDPGC::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDPGC>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDPGC::parse_begin(Object *p_object) {
	InterDVDPGC *pgc = Object::cast_to<InterDVDPGC>(p_object);
	if (!pgc) {
		return;
	}
	const ObjectID id = pgc->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Video Cell…"), callable_mp(this, &EditorInspectorPluginInterDVDPGC::_pick_video_cell).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Jump Title Button"), callable_mp(this, &EditorInspectorPluginInterDVDPGC::_add_jump_title_button).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDPGC::_pick_video_cell(ObjectID p_id) {
	EditorFileDialog *fd = inter_dvd_popup_file_dialog(EditorFileDialog::FILE_MODE_OPEN_FILE, EditorFileDialog::ACCESS_FILESYSTEM, INTER_DVD_VIDEO_FILTER, TTR("Video"));
	fd->connect(SNAME("file_selected"), callable_mp(this, &EditorInspectorPluginInterDVDPGC::_video_cell_selected).bind(p_id, fd->get_instance_id()));
}

void EditorInspectorPluginInterDVDPGC::_video_cell_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id) {
	if (Node *dialog = Object::cast_to<Node>(ObjectDB::get_instance(p_dialog_id))) {
		dialog->queue_free();
	}
	InterDVDPGC *pgc = Object::cast_to<InterDVDPGC>(ObjectDB::get_instance(p_id));
	if (pgc) {
		pgc->add_cell_from_video(p_path);
	}
}

void EditorInspectorPluginInterDVDPGC::_add_jump_title_button(ObjectID p_id) {
	InterDVDPGC *pgc = Object::cast_to<InterDVDPGC>(ObjectDB::get_instance(p_id));
	if (pgc) {
		pgc->add_jump_title_button(1);
	}
}

namespace {
Node *inter_dvd_scene_owner(Node *p_node) {
	if (!p_node) {
		return nullptr;
	}
	if (p_node->get_owner()) {
		return p_node->get_owner();
	}
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_edited_scene()) {
		return EditorNode::get_singleton()->get_edited_scene();
	}
	return p_node;
}

void inter_dvd_own_new(Node *p_disc_or_parent) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(p_disc_or_parent);
	if (!disc) {
		disc = InterDVDDisc::find_in_tree(p_disc_or_parent);
		Node *walk = p_disc_or_parent;
		while (!disc && walk) {
			disc = Object::cast_to<InterDVDDisc>(walk);
			walk = walk->get_parent();
		}
	}
	if (disc) {
		disc->apply_scene_owner(inter_dvd_scene_owner(disc));
	}
}
} //namespace

bool EditorInspectorPluginInterDVDDisc::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDDisc>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDDisc::parse_begin(Object *p_object) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(p_object);
	if (!disc) {
		return;
	}
	const ObjectID id = disc->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Title"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_title).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Chapter to Last Title"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_chapter).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Title Menu"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_title_menu).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Root Menu"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_root_menu).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Title Set"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_title_set).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Menu Title"), callable_mp(this, &EditorInspectorPluginInterDVDDisc::_add_menu_title).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDDisc::_add_title(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		disc->add_title();
		inter_dvd_own_new(disc);
	}
}

void EditorInspectorPluginInterDVDDisc::_add_chapter(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		disc->add_chapter_to_last();
		inter_dvd_own_new(disc);
	}
}

void EditorInspectorPluginInterDVDDisc::_add_title_menu(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		InterDVDMenuPage *menu = disc->add_title_menu();
		InterDVDHotspot *play = menu->add_hotspot("Play");
		InterDVDTitle *dest = nullptr;
		for (int i = 0; i < disc->get_child_count(); i++) {
			if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(disc->get_child(i))) {
				if (!title->is_menu_title()) {
					dest = title;
					break;
				}
			} else if (InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(disc->get_child(i))) {
				for (int t = 0; t < set->get_child_count(); t++) {
					if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(set->get_child(t))) {
						if (!title->is_menu_title()) {
							dest = title;
							break;
						}
					}
				}
				if (dest) {
					break;
				}
			}
		}
		if (dest) {
			play->set_destination(play->get_path_to(dest));
		}
		inter_dvd_own_new(disc);
	}
}

void EditorInspectorPluginInterDVDDisc::_add_root_menu(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		disc->add_root_menu();
		inter_dvd_own_new(disc);
	}
}

void EditorInspectorPluginInterDVDDisc::_add_title_set(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		disc->add_title_set();
		inter_dvd_own_new(disc);
	}
}

void EditorInspectorPluginInterDVDDisc::_add_menu_title(ObjectID p_id) {
	InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(ObjectDB::get_instance(p_id));
	if (disc) {
		InterDVDTitle *title = disc->add_menu_title();
		title->add_hotspot("Play");
		inter_dvd_own_new(disc);
	}
}

bool EditorInspectorPluginInterDVDTitle::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDTitle>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDTitle::parse_begin(Object *p_object) {
	InterDVDTitle *title = Object::cast_to<InterDVDTitle>(p_object);
	if (!title) {
		return;
	}
	const ObjectID id = title->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Chapter"), callable_mp(this, &EditorInspectorPluginInterDVDTitle::_add_chapter).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Hotspot"), callable_mp(this, &EditorInspectorPluginInterDVDTitle::_add_hotspot).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Menu Hotspot"), callable_mp(this, &EditorInspectorPluginInterDVDTitle::_add_menu_hotspot).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Resume Hotspot"), callable_mp(this, &EditorInspectorPluginInterDVDTitle::_add_resume_hotspot).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDTitle::_add_chapter(ObjectID p_id) {
	InterDVDTitle *title = Object::cast_to<InterDVDTitle>(ObjectDB::get_instance(p_id));
	if (title) {
		title->add_chapter();
		inter_dvd_own_new(title);
	}
}

void EditorInspectorPluginInterDVDTitle::_add_hotspot(ObjectID p_id) {
	InterDVDTitle *title = Object::cast_to<InterDVDTitle>(ObjectDB::get_instance(p_id));
	if (title) {
		title->add_hotspot();
		inter_dvd_own_new(title);
	}
}

void EditorInspectorPluginInterDVDTitle::_add_menu_hotspot(ObjectID p_id) {
	InterDVDTitle *title = Object::cast_to<InterDVDTitle>(ObjectDB::get_instance(p_id));
	if (title) {
		title->add_menu_hotspot();
		inter_dvd_own_new(title);
	}
}

void EditorInspectorPluginInterDVDTitle::_add_resume_hotspot(ObjectID p_id) {
	InterDVDTitle *title = Object::cast_to<InterDVDTitle>(ObjectDB::get_instance(p_id));
	if (title) {
		title->add_resume_hotspot();
		inter_dvd_own_new(title);
	}
}

bool EditorInspectorPluginInterDVDTitleSet::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDTitleSet>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDTitleSet::parse_begin(Object *p_object) {
	InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(p_object);
	if (!set) {
		return;
	}
	const ObjectID id = set->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Title"), callable_mp(this, &EditorInspectorPluginInterDVDTitleSet::_add_title).bind(id));
	inter_dvd_add_inspector_button(box, TTR("Add Root Menu"), callable_mp(this, &EditorInspectorPluginInterDVDTitleSet::_add_root_menu).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDTitleSet::_add_title(ObjectID p_id) {
	InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(ObjectDB::get_instance(p_id));
	if (set) {
		set->add_title();
		inter_dvd_own_new(set);
	}
}

void EditorInspectorPluginInterDVDTitleSet::_add_root_menu(ObjectID p_id) {
	InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(ObjectDB::get_instance(p_id));
	if (set) {
		set->add_root_menu();
		inter_dvd_own_new(set);
	}
}

bool EditorInspectorPluginInterDVDMenuPage::can_handle(Object *p_object) {
	return Object::cast_to<InterDVDMenuPage>(p_object) != nullptr;
}

void EditorInspectorPluginInterDVDMenuPage::parse_begin(Object *p_object) {
	InterDVDMenuPage *page = Object::cast_to<InterDVDMenuPage>(p_object);
	if (!page) {
		return;
	}
	const ObjectID id = page->get_instance_id();
	VBoxContainer *box = memnew(VBoxContainer);
	Label *heading = memnew(Label);
	heading->set_text(TTR("DVD Authoring"));
	box->add_child(heading);
	inter_dvd_add_inspector_button(box, TTR("Add Hotspot"), callable_mp(this, &EditorInspectorPluginInterDVDMenuPage::_add_hotspot).bind(id));
	add_custom_control(box);
}

void EditorInspectorPluginInterDVDMenuPage::_add_hotspot(ObjectID p_id) {
	InterDVDMenuPage *page = Object::cast_to<InterDVDMenuPage>(ObjectDB::get_instance(p_id));
	if (page) {
		page->add_hotspot();
		inter_dvd_own_new(page);
	}
}

void InterDVDEditorPlugin::_create_dvd_scene() {
	Node *edited = EditorNode::get_singleton()->get_edited_scene();
	if (InterDVDDisc *existing = Object::cast_to<InterDVDDisc>(edited)) {
		EditorNode::get_singleton()->push_item(existing);
		return;
	}
	InterDVDDisc *disc = InterDVDDisc::create_starter();
	if (!edited) {
		EditorNode::get_singleton()->set_edited_scene(disc);
		disc->apply_scene_owner(disc);
	} else {
		edited->add_child(disc);
		disc->apply_scene_owner(edited);
	}
	EditorNode::get_singleton()->push_item(disc);
}

void InterDVDEditorPlugin::_bind_methods() {
}

void InterDVDEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		EDITOR_DEF_BASIC("export/inter_dvd/toolchain", "");
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/inter_dvd/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe"));
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
		project_inspector_plugin.instantiate();
		add_inspector_plugin(project_inspector_plugin);
		pgc_inspector_plugin.instantiate();
		add_inspector_plugin(pgc_inspector_plugin);
		disc_inspector_plugin.instantiate();
		add_inspector_plugin(disc_inspector_plugin);
		title_inspector_plugin.instantiate();
		add_inspector_plugin(title_inspector_plugin);
		title_set_inspector_plugin.instantiate();
		add_inspector_plugin(title_set_inspector_plugin);
		menu_page_inspector_plugin.instantiate();
		add_inspector_plugin(menu_page_inspector_plugin);
		add_tool_menu_item(TTR("Create Interactive DVD Scene"), callable_mp(this, &InterDVDEditorPlugin::_create_dvd_scene));
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_tool_menu_item(TTR("Create Interactive DVD Scene"));
		if (menu_page_inspector_plugin.is_valid()) {
			remove_inspector_plugin(menu_page_inspector_plugin);
			menu_page_inspector_plugin.unref();
		}
		if (title_set_inspector_plugin.is_valid()) {
			remove_inspector_plugin(title_set_inspector_plugin);
			title_set_inspector_plugin.unref();
		}
		if (title_inspector_plugin.is_valid()) {
			remove_inspector_plugin(title_inspector_plugin);
			title_inspector_plugin.unref();
		}
		if (disc_inspector_plugin.is_valid()) {
			remove_inspector_plugin(disc_inspector_plugin);
			disc_inspector_plugin.unref();
		}
		if (pgc_inspector_plugin.is_valid()) {
			remove_inspector_plugin(pgc_inspector_plugin);
			pgc_inspector_plugin.unref();
		}
		if (project_inspector_plugin.is_valid()) {
			remove_inspector_plugin(project_inspector_plugin);
			project_inspector_plugin.unref();
		}
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
