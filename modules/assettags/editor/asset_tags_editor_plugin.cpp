/**************************************************************************/
/*  asset_tags_editor_plugin.cpp                                          */
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

#include "core/object/callable_mp.h"
#include "asset_tags_editor_plugin.h"

#include "../asset_tag_coordinator.h"
#include "../asset_tag_manager.h"
#include "../asset_tag_registry.h"
#include "asset_tags_context_menu_plugin.h"
#include "editor/inspector/editor_context_menu_plugin.h"
#include "editor/settings/project_settings_editor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"

void AssetTagsEditorPlugin::_bind_methods() {}

String AssetTagsEditorPlugin::get_plugin_name() const {
	return "Asset Tags";
}

void AssetTagsEditorPlugin::_refresh_tag_tree() {
	if (!tag_tree) {
		return;
	}
	tag_tree->clear();
	TreeItem *root = tag_tree->create_item();
	root->set_text(0, TTR("Tags"));
	root->set_selectable(0, false);

	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return;
	}

	_populate_tag_tree_item(root, String());
}

void AssetTagsEditorPlugin::_populate_tag_tree_item(TreeItem *p_parent, const String &p_parent_tag) {
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return;
	}

	PackedStringArray tags = manager->list_tags(p_parent_tag);
	for (int i = 0; i < tags.size(); i++) {
		TreeItem *item = tag_tree->create_item(p_parent);
		item->set_text(0, tags[i]);
		item->set_metadata(0, tags[i]);
		Dictionary info = manager->get_tag_info(tags[i]);
		if (info.get("ok", false)) {
			const String comment = info.get("comment", "");
			if (!comment.is_empty()) {
				item->set_tooltip_text(0, comment);
			}
		}
		_populate_tag_tree_item(item, tags[i]);
	}
}

void AssetTagsEditorPlugin::_on_tag_selected() {
	if (!tag_tree) {
		return;
	}
	TreeItem *item = tag_tree->get_selected();
	if (!item || !item->get_metadata(0)) {
		selected_tag = String();
		return;
	}
	selected_tag = item->get_metadata(0);
	if (rename_edit) {
		rename_edit->set_text(selected_tag);
	}
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (manager && comment_edit) {
		Dictionary info = manager->get_tag_info(selected_tag);
		comment_edit->set_text(info.get("comment", ""));
	}
}

void AssetTagsEditorPlugin::_on_add_tag_pressed() {
	if (!new_tag_edit) {
		return;
	}
	const String tag = new_tag_edit->get_text().strip_edges();
	if (tag.is_empty()) {
		return;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return;
	}
	const String comment = comment_edit ? comment_edit->get_text() : String();
	if (coordinator->add_tag(tag, comment) == OK) {
		new_tag_edit->clear();
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_on_save_comment_pressed() {
	if (selected_tag.is_empty() || !comment_edit) {
		return;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return;
	}
	if (coordinator->update_tag_comment(selected_tag, comment_edit->get_text()) == OK) {
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_on_remove_tag_pressed() {
	if (selected_tag.is_empty()) {
		return;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return;
	}
	if (coordinator->remove_tag(selected_tag) == OK) {
		selected_tag = String();
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_on_rename_tag_pressed() {
	if (selected_tag.is_empty() || !rename_edit) {
		return;
	}
	const String new_name = rename_edit->get_text().strip_edges();
	if (new_name.is_empty() || new_name == selected_tag) {
		return;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return;
	}
	const String old_name = selected_tag;
	if (coordinator->rename_tag(old_name, new_name) == OK) {
		selected_tag = new_name;
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_on_undo_pressed() {
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator || !coordinator->can_undo()) {
		return;
	}
	if (coordinator->undo_last_change() == OK) {
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_on_cleanup_unused_pressed() {
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!registry || !manager) {
		return;
	}
	PackedStringArray unused = registry->get_unused_tags();
	if (unused.is_empty()) {
		return;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (coordinator) {
		coordinator->begin_transaction();
		for (int i = 0; i < unused.size(); i++) {
			coordinator->remove_tag(unused[i]);
		}
		coordinator->commit_transaction();
	} else {
		manager->begin_batch();
		for (int i = 0; i < unused.size(); i++) {
			manager->remove_tag(unused[i]);
		}
		(void)manager->commit_batch();
	}
	_refresh_tag_tree();
}

void AssetTagsEditorPlugin::_on_tag_dictionary_changed() {
	_refresh_tag_tree();
}

void AssetTagsEditorPlugin::_on_index_reloaded() {
	_refresh_tag_tree();
}

void AssetTagsEditorPlugin::_on_project_settings_visibility_changed() {
	ProjectSettingsEditor *pse = ProjectSettingsEditor::get_singleton();
	if (pse && pse->is_visible()) {
		_refresh_tag_tree();
	}
}

void AssetTagsEditorPlugin::_build_tab() {
	if (tags_tab) {
		return;
	}

	VBoxContainer *tab = memnew(VBoxContainer);
	tab->set_name(TTR("Asset Tags"));
	tab->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tab->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tags_tab = tab;

	Label *dict_label = memnew(Label);
	dict_label->set_text(TTR("Tag Dictionary"));
	tab->add_child(dict_label);

	tag_tree = memnew(Tree);
	tag_tree->set_hide_root(true);
	tag_tree->set_select_mode(Tree::SELECT_SINGLE);
	tag_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_tree->connect("item_selected", callable_mp(this, &AssetTagsEditorPlugin::_on_tag_selected));
	tab->add_child(tag_tree);

	HBoxContainer *add_row = memnew(HBoxContainer);
	new_tag_edit = memnew(LineEdit);
	new_tag_edit->set_placeholder(TTR("New tag (e.g. Environment.Nature.Tree)"));
	new_tag_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_row->add_child(new_tag_edit);
	Button *add_button = memnew(Button);
	add_button->set_text(TTR("Add"));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_add_tag_pressed));
	add_row->add_child(add_button);
	tab->add_child(add_row);

	HBoxContainer *comment_row = memnew(HBoxContainer);
	comment_edit = memnew(LineEdit);
	comment_edit->set_placeholder(TTR("Tag comment"));
	comment_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	comment_row->add_child(comment_edit);
	Button *save_comment_button = memnew(Button);
	save_comment_button->set_text(TTR("Save Comment"));
	save_comment_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_save_comment_pressed));
	comment_row->add_child(save_comment_button);
	tab->add_child(comment_row);

	HBoxContainer *rename_row = memnew(HBoxContainer);
	rename_edit = memnew(LineEdit);
	rename_edit->set_placeholder(TTR("Rename selected tag"));
	rename_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rename_row->add_child(rename_edit);
	Button *rename_button = memnew(Button);
	rename_button->set_text(TTR("Rename"));
	rename_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_rename_tag_pressed));
	rename_row->add_child(rename_button);
	tab->add_child(rename_row);

	HBoxContainer *action_row = memnew(HBoxContainer);
	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_remove_tag_pressed));
	action_row->add_child(remove_button);
	Button *cleanup_button = memnew(Button);
	cleanup_button->set_text(TTR("Cleanup Unused"));
	cleanup_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_cleanup_unused_pressed));
	action_row->add_child(cleanup_button);
	Button *undo_button = memnew(Button);
	undo_button->set_text(TTR("Undo Last Change"));
	undo_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorPlugin::_on_undo_pressed));
	action_row->add_child(undo_button);
	tab->add_child(action_row);

	add_control_to_container(CONTAINER_PROJECT_SETTING_TAB_RIGHT, tags_tab);

	// Place immediately after General so the dictionary is not buried behind tab-bar scroll.
	if (ProjectSettingsEditor *pse = ProjectSettingsEditor::get_singleton()) {
		if (TabContainer *tabs = pse->get_tabs()) {
			if (tags_tab->get_parent() == tabs && tabs->get_child_count() > 1) {
				tabs->move_child(tags_tab, 1);
			}
		}
		const Callable vis_cb = callable_mp(this, &AssetTagsEditorPlugin::_on_project_settings_visibility_changed);
		if (!pse->is_connected(SceneStringName(visibility_changed), vis_cb)) {
			pse->connect(SceneStringName(visibility_changed), vis_cb);
		}
	}

	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		manager->connect("tag_dictionary_changed", callable_mp(this, &AssetTagsEditorPlugin::_on_tag_dictionary_changed));
	}
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		registry->connect("index_reloaded", callable_mp(this, &AssetTagsEditorPlugin::_on_index_reloaded));
	}

	_refresh_tag_tree();
}

void AssetTagsEditorPlugin::_teardown_tab() {
	if (ProjectSettingsEditor *pse = ProjectSettingsEditor::get_singleton()) {
		const Callable vis_cb = callable_mp(this, &AssetTagsEditorPlugin::_on_project_settings_visibility_changed);
		if (pse->is_connected(SceneStringName(visibility_changed), vis_cb)) {
			pse->disconnect(SceneStringName(visibility_changed), vis_cb);
		}
	}
	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		const Callable cb = callable_mp(this, &AssetTagsEditorPlugin::_on_tag_dictionary_changed);
		if (manager->is_connected("tag_dictionary_changed", cb)) {
			manager->disconnect("tag_dictionary_changed", cb);
		}
	}
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		const Callable reload_cb = callable_mp(this, &AssetTagsEditorPlugin::_on_index_reloaded);
		if (registry->is_connected("index_reloaded", reload_cb)) {
			registry->disconnect("index_reloaded", reload_cb);
		}
	}
	if (tags_tab) {
		remove_control_from_container(CONTAINER_PROJECT_SETTING_TAB_RIGHT, tags_tab);
		tags_tab->queue_free();
		tags_tab = nullptr;
		tag_tree = nullptr;
		new_tag_edit = nullptr;
		comment_edit = nullptr;
		rename_edit = nullptr;
	}
}

void AssetTagsEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_build_tab();
			if (fs_context_plugin.is_null()) {
				fs_context_plugin.instantiate();
				add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, fs_context_plugin);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (fs_context_plugin.is_valid()) {
				remove_context_menu_plugin(fs_context_plugin);
				fs_context_plugin.unref();
			}
			_teardown_tab();
		} break;
	}
}

AssetTagsEditorPlugin::AssetTagsEditorPlugin() {
}

AssetTagsEditorPlugin::~AssetTagsEditorPlugin() {
}

#endif
