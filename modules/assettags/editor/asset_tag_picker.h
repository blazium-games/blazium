/**************************************************************************/
/*  asset_tag_picker.h                                                    */
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

#include "scene/gui/box_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"

class AssetTagPicker : public VBoxContainer {
	GDCLASS(AssetTagPicker, VBoxContainer);

	LineEdit *filter_edit = nullptr;
	ItemList *tag_list = nullptr;
	ItemList *selected_list = nullptr;

	PackedStringArray current_tags;
	PackedStringArray cached_all_tags;
	bool cached_all_tags_valid = false;

	void _refresh_tag_list();
	void _refresh_selected_list();
	void _on_filter_changed(const String &p_text);
	void _on_tag_activated(int p_index);
	void _on_selected_activated(int p_index);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_tags(const PackedStringArray &p_tags);
	PackedStringArray get_tags() const;
	void refresh_available_tags();

	AssetTagPicker();
	~AssetTagPicker();
};

#endif
