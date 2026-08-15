/**************************************************************************/
/*  asset_tag_picker.cpp                                                  */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "asset_tag_picker.h"

#include "../asset_tag_manager.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"

void AssetTagPicker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tags", "tags"), &AssetTagPicker::set_tags);
	ClassDB::bind_method(D_METHOD("get_tags"), &AssetTagPicker::get_tags);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "tags"), "set_tags", "get_tags");
}

void AssetTagPicker::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		_refresh_tag_list();
		_refresh_selected_list();
	}
}

void AssetTagPicker::_refresh_tag_list() {
	if (!tag_list) {
		return;
	}
	tag_list->clear();
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return;
	}

	const String filter = filter_edit ? filter_edit->get_text().to_lower() : String();
	if (!cached_all_tags_valid) {
		cached_all_tags = manager->list_all_tags();
		cached_all_tags_valid = true;
	}
	for (int i = 0; i < cached_all_tags.size(); i++) {
		const String tag = cached_all_tags[i];
		if (!filter.is_empty() && !tag.to_lower().contains(filter)) {
			continue;
		}
		if (current_tags.has(tag)) {
			continue;
		}
		tag_list->add_item(tag);
	}
}

void AssetTagPicker::_refresh_selected_list() {
	if (!selected_list) {
		return;
	}
	selected_list->clear();
	for (int i = 0; i < current_tags.size(); i++) {
		selected_list->add_item(current_tags[i]);
	}
}

void AssetTagPicker::_on_filter_changed(const String &p_text) {
	(void)p_text;
	_refresh_tag_list();
}

void AssetTagPicker::_on_tag_activated(int p_index) {
	if (!tag_list || p_index < 0 || p_index >= tag_list->get_item_count()) {
		return;
	}
	const String tag = tag_list->get_item_text(p_index);
	if (!current_tags.has(tag)) {
		current_tags.push_back(tag);
		current_tags.sort();
		_refresh_tag_list();
		_refresh_selected_list();
	}
}

void AssetTagPicker::_on_selected_activated(int p_index) {
	if (!selected_list || p_index < 0 || p_index >= selected_list->get_item_count()) {
		return;
	}
	const String tag = selected_list->get_item_text(p_index);
	for (int i = 0; i < current_tags.size(); i++) {
		if (current_tags[i] == tag) {
			current_tags.remove_at(i);
			break;
		}
	}
	_refresh_tag_list();
	_refresh_selected_list();
}

void AssetTagPicker::set_tags(const PackedStringArray &p_tags) {
	current_tags = p_tags;
	current_tags.sort();
	_refresh_tag_list();
	_refresh_selected_list();
}

void AssetTagPicker::refresh_available_tags() {
	cached_all_tags_valid = false;
	_refresh_tag_list();
}

PackedStringArray AssetTagPicker::get_tags() const {
	return current_tags;
}

AssetTagPicker::AssetTagPicker() {
	Label *available_label = memnew(Label);
	available_label->set_text(TTR("Available Tags"));
	add_child(available_label);

	filter_edit = memnew(LineEdit);
	filter_edit->set_placeholder(TTR("Filter tags..."));
	filter_edit->connect(SceneStringName(text_changed), callable_mp(this, &AssetTagPicker::_on_filter_changed));
	add_child(filter_edit);

	tag_list = memnew(ItemList);
	tag_list->set_max_columns(1);
	tag_list->set_select_mode(ItemList::SELECT_SINGLE);
	tag_list->set_custom_minimum_size(Size2(0, 72) * EDSCALE);
	tag_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_list->connect("item_activated", callable_mp(this, &AssetTagPicker::_on_tag_activated));
	add_child(tag_list);

	Label *selected_label = memnew(Label);
	selected_label->set_text(TTR("Selected Tags"));
	add_child(selected_label);

	selected_list = memnew(ItemList);
	selected_list->set_max_columns(1);
	selected_list->set_select_mode(ItemList::SELECT_SINGLE);
	selected_list->set_custom_minimum_size(Size2(0, 72) * EDSCALE);
	selected_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	selected_list->connect("item_activated", callable_mp(this, &AssetTagPicker::_on_selected_activated));
	add_child(selected_list);
}

AssetTagPicker::~AssetTagPicker() {}

#endif
