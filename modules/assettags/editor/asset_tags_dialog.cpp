/**************************************************************************/
/*  asset_tags_dialog.cpp                                                 */
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

#include "asset_tags_dialog.h"

#include "../asset_tag_coordinator.h"
#include "../asset_tag_manager.h"
#include "../asset_tag_registry.h"
#include "../asset_tag_storage.h"
#include "asset_tag_picker.h"

#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "servers/display_server.h"

void AssetTagsEditorDialog::_bind_methods() {}

void AssetTagsEditorDialog::ok_pressed() {
	_apply_tags();
}

void AssetTagsEditorDialog::edit(const Vector<String> &p_paths) {
	editing_paths.clear();
	for (int i = 0; i < p_paths.size(); i++) {
		const String path = AssetTagStorage::normalize_asset_path(p_paths[i]);
		if (path.ends_with("/") || !AssetTagStorage::is_taggable_extension(path)) {
			continue;
		}
		if (!editing_paths.has(path)) {
			editing_paths.push_back(path);
		}
	}
	if (editing_paths.is_empty()) {
		return;
	}

	if (editing_paths.size() == 1) {
		set_title(vformat(TTR("Asset Tags for: %s"), editing_paths[0].get_file()));
		path_label->set_text(editing_paths[0]);
	} else {
		set_title(vformat(TTR("Asset Tags for %d Files"), editing_paths.size()));
		path_label->set_text(vformat(TTR("%d files selected. Only tags shared by all of them are listed; added or removed tags are applied to every file."), editing_paths.size()));
	}

	initial_common_tags.clear();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (registry) {
		initial_common_tags = registry->get_tags_for_asset(editing_paths[0]);
		for (int i = 1; i < editing_paths.size() && !initial_common_tags.is_empty(); i++) {
			const PackedStringArray tags = registry->get_tags_for_asset(editing_paths[i]);
			PackedStringArray common;
			for (int j = 0; j < initial_common_tags.size(); j++) {
				if (tags.has(initial_common_tags[j])) {
					common.push_back(initial_common_tags[j]);
				}
			}
			initial_common_tags = common;
		}
	}

	const String tag_list_text = initial_common_tags.is_empty() ? String() : String(", ").join(initial_common_tags);
	if (editing_paths.size() == 1) {
		if (tag_list_text.is_empty()) {
			current_tags_label->set_text(TTR("Current tags: (none)"));
		} else {
			current_tags_label->set_text(vformat(TTR("Current tags: %s"), tag_list_text));
		}
	} else {
		if (tag_list_text.is_empty()) {
			current_tags_label->set_text(vformat(TTR("Tags shared by all %d files: (none)"), editing_paths.size()));
		} else {
			current_tags_label->set_text(vformat(TTR("Tags shared by all %d files: %s"), editing_paths.size(), tag_list_text));
		}
	}

	picker->refresh_available_tags();
	picker->set_tags(initial_common_tags);
	new_tag_edit->clear();

	Rect2i avail;
	if (is_embedded()) {
		avail = Rect2i(Point2i(), get_embedder()->get_visible_rect().size);
	} else {
		DisplayServer *ds = DisplayServer::get_singleton();
		const int screen = ds->window_get_current_screen(get_parent_visible_window()->get_window_id());
		avail = ds->screen_get_usable_rect(screen);
	}

	set_max_size(Size2i(Size2(avail.size) * 0.9));

	Size2i target = Size2i(Size2(545, 495) * EDSCALE).min(Size2i(Size2(avail.size) * 0.8));
	reset_size();
	popup(Rect2i(avail.position + (avail.size - target) / 2, target));
}

void AssetTagsEditorDialog::_on_new_tag_submitted(const String &p_text) {
	(void)p_text;
	_on_create_tag_pressed();
}

void AssetTagsEditorDialog::_on_create_tag_pressed() {
	const String tag = new_tag_edit->get_text().strip_edges();
	if (tag.is_empty()) {
		return;
	}

	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (manager && !manager->has_tag_in_dictionary(tag)) {
		AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
		if (!coordinator) {
			return;
		}
		if (coordinator->add_tag(tag, String()) != OK) {
			WARN_PRINT(vformat("AssetTags: failed to create tag '%s'.", tag));
			return;
		}
	}

	picker->refresh_available_tags();
	PackedStringArray tags = picker->get_tags();
	if (!tags.has(tag)) {
		tags.push_back(tag);
		picker->set_tags(tags);
	}
	new_tag_edit->clear();
}

void AssetTagsEditorDialog::_apply_tags() {
	if (editing_paths.is_empty()) {
		return;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return;
	}

	const PackedStringArray new_tags = picker->get_tags();

	PackedStringArray added_tags;
	for (int i = 0; i < new_tags.size(); i++) {
		if (!initial_common_tags.has(new_tags[i])) {
			added_tags.push_back(new_tags[i]);
		}
	}
	PackedStringArray removed_tags;
	for (int i = 0; i < initial_common_tags.size(); i++) {
		if (!new_tags.has(initial_common_tags[i])) {
			removed_tags.push_back(initial_common_tags[i]);
		}
	}
	if (added_tags.is_empty() && removed_tags.is_empty()) {
		return;
	}

	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (coordinator) {
		if (coordinator->begin_transaction() != OK) {
			return;
		}
	} else {
		registry->begin_batch();
	}

	for (int i = 0; i < editing_paths.size(); i++) {
		Error err = OK;
		if (editing_paths.size() == 1) {
			err = registry->set_tags_for_asset(editing_paths[i], new_tags);
		} else {
			if (!added_tags.is_empty()) {
				err = registry->add_tags_to_asset(editing_paths[i], added_tags);
			}
			if (err == OK && !removed_tags.is_empty()) {
				err = registry->remove_tags_from_asset(editing_paths[i], removed_tags);
			}
		}
		if (err != OK) {
			if (coordinator) {
				coordinator->abort_transaction();
			} else {
				registry->abort_batch();
			}
			WARN_PRINT(vformat("AssetTags: failed to update tags on %s", editing_paths[i]));
			return;
		}
	}

	const Error commit_err = coordinator ? coordinator->commit_transaction() : registry->commit_batch();
	if (commit_err != OK) {
		WARN_PRINT("AssetTags: failed to persist asset tags.");
	}
}

AssetTagsEditorDialog::AssetTagsEditorDialog() {
	set_title(TTR("Asset Tags"));
	set_ok_button_text(TTR("Apply"));
	set_min_size(Size2(420, 0) * EDSCALE);

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	path_label = memnew(Label);
	path_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	vb->add_child(path_label);

	current_tags_label = memnew(Label);
	current_tags_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	vb->add_child(current_tags_label);

	HBoxContainer *create_row = memnew(HBoxContainer);
	new_tag_edit = memnew(LineEdit);
	new_tag_edit->set_placeholder(TTR("New tag (e.g. Environment.Nature.Tree)"));
	new_tag_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	new_tag_edit->connect(SceneStringName(text_submitted), callable_mp(this, &AssetTagsEditorDialog::_on_new_tag_submitted));
	create_row->add_child(new_tag_edit);
	Button *create_button = memnew(Button);
	create_button->set_text(TTR("Create & Add"));
	create_button->connect(SceneStringName(pressed), callable_mp(this, &AssetTagsEditorDialog::_on_create_tag_pressed));
	create_row->add_child(create_button);
	vb->add_child(create_row);

	picker = memnew(AssetTagPicker);
	picker->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(picker);
}

#endif
