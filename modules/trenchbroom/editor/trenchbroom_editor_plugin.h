/**************************************************************************/
/*  trenchbroom_editor_plugin.h                                           */
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

#include "editor/plugins/editor_plugin.h"

class Label;
class ProgressBar;
class TrenchbroomMap;

class TrenchbroomEditorPlugin : public EditorPlugin {
	GDCLASS(TrenchbroomEditorPlugin, EditorPlugin);

protected:
	static void _bind_methods();
	void _notification(int p_what);

	ObjectID edited_map_id;
	Control *progress_container = nullptr;
	ProgressBar *progress_bar = nullptr;
	Label *progress_label = nullptr;

	void _disconnect_map_signals(TrenchbroomMap *p_map);
	void _connect_map_signals(TrenchbroomMap *p_map);
	void _on_build_progress(const String &p_step, real_t p_progress);
	void _on_build_finished();
	void _on_build_failed();
	Control *_create_progress_bar();

public:
	virtual String get_plugin_name() const override { return "Trenchbroom"; }
	virtual bool handles(Object *p_object) const override;
	virtual void edit(Object *p_object) override;
	virtual void make_visible(bool p_visible) override;

	~TrenchbroomEditorPlugin();
};
