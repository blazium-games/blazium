/**************************************************************************/
/*  asset_tags_editor_plugin.h                                            */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "asset_tags_context_menu_plugin.h"
#include "editor/plugins/editor_plugin.h"

class LineEdit;
class Tree;
class TreeItem;

class AssetTagsEditorPlugin : public EditorPlugin {
	GDCLASS(AssetTagsEditorPlugin, EditorPlugin);

	Control *tags_tab = nullptr;
	Tree *tag_tree = nullptr;
	LineEdit *new_tag_edit = nullptr;
	LineEdit *comment_edit = nullptr;
	LineEdit *rename_edit = nullptr;
	Ref<AssetTagsContextMenuPlugin> fs_context_plugin;

	String selected_tag;

	void _refresh_tag_tree();
	void _populate_tag_tree_item(TreeItem *p_parent, const String &p_parent_tag);
	void _on_tag_selected();
	void _on_add_tag_pressed();
	void _on_save_comment_pressed();
	void _on_remove_tag_pressed();
	void _on_rename_tag_pressed();
	void _on_cleanup_unused_pressed();
	void _on_undo_pressed();
	void _on_tag_dictionary_changed();
	void _on_index_reloaded();
	void _on_project_settings_visibility_changed();
	void _build_tab();
	void _teardown_tab();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override;

	AssetTagsEditorPlugin();
	~AssetTagsEditorPlugin();
};

#endif
