/**************************************************************************/
/*  asset_tags_dialog.h                                                   */
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

#include "scene/gui/dialogs.h"

class AssetTagPicker;
class Label;
class LineEdit;

class AssetTagsEditorDialog : public ConfirmationDialog {
	GDCLASS(AssetTagsEditorDialog, ConfirmationDialog);

	Label *path_label = nullptr;
	Label *current_tags_label = nullptr;
	LineEdit *new_tag_edit = nullptr;
	AssetTagPicker *picker = nullptr;

	Vector<String> editing_paths;

	PackedStringArray initial_common_tags;

	void _apply_tags();
	void _on_create_tag_pressed();
	void _on_new_tag_submitted(const String &p_text);

protected:
	static void _bind_methods();
	virtual void ok_pressed() override;

public:
	void edit(const Vector<String> &p_paths);

	AssetTagsEditorDialog();
};

#endif
