/**************************************************************************/
/*  asset_tags_context_menu_plugin.cpp                                    */
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
#include "asset_tags_context_menu_plugin.h"

#include "../asset_tag_storage.h"
#include "asset_tags_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"

void AssetTagsContextMenuPlugin::_bind_methods() {}

void AssetTagsContextMenuPlugin::_edit_tags(const Variant &p_paths) {
	PackedStringArray paths;
	if (p_paths.get_type() == Variant::PACKED_STRING_ARRAY) {
		paths = p_paths;
	} else if (p_paths.get_type() == Variant::ARRAY) {
		Array arr = p_paths;
		for (int i = 0; i < arr.size(); i++) {
			paths.push_back(arr[i]);
		}
	} else {
		return;
	}

	Vector<String> edit_paths;
	for (int i = 0; i < paths.size(); i++) {
		edit_paths.push_back(paths[i]);
	}

	if (!dialog) {
		dialog = memnew(AssetTagsEditorDialog);
		EditorNode::get_singleton()->get_gui_base()->add_child(dialog);
	}
	dialog->edit(edit_paths);
}

void AssetTagsContextMenuPlugin::get_options(const Vector<String> &p_paths) {
	bool any_taggable = false;
	for (int i = 0; i < p_paths.size(); i++) {
		const String &path = p_paths[i];
		if (!path.ends_with("/") && AssetTagStorage::is_taggable_extension(path)) {
			any_taggable = true;
			break;
		}
	}
	if (!any_taggable) {
		return;
	}

	add_context_menu_item(TTR("Edit Asset Tags..."), callable_mp(this, &AssetTagsContextMenuPlugin::_edit_tags), Ref<Texture2D>());
}

#endif
