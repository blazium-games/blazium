/**************************************************************************/
/*  gif_editor_plugin.h                                                   */
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

#include "editor/editor_inspector.h"
#include "editor/editor_resource_preview.h"
#include "editor/plugins/editor_plugin.h"
#include "modules/gif/gif_recorder.h"
#include "modules/gif/gif_texture.h"

class Button;
class EditorFileDialog;
class TextureRect;

class EditorInspectorPluginGIF : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginGIF, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class GIFPreviewGenerator : public EditorResourcePreviewGenerator {
	GDCLASS(GIFPreviewGenerator, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual bool can_generate_small_preview() const override { return true; }
};

class GIFEditorPlugin : public EditorPlugin {
	GDCLASS(GIFEditorPlugin, EditorPlugin);

	Ref<EditorInspectorPluginGIF> inspector_plugin;
	Ref<GIFPreviewGenerator> preview_generator;
	Ref<GIFRecorder> recorder;
	EditorFileDialog *save_dialog = nullptr;
	String pending_save_kind;
	ObjectID game_view_id;
	bool capturing_game = false;

	void _record_editor_viewport();
	void _record_game_viewport();
	void _record_full_window();
	void _export_animation_player();
	void _toggle_recording(GIFRecorder::Source p_source, Viewport *p_viewport);
	void _save_dialog_file_selected(const String &p_path);
	void _process_game_capture();
	Viewport *_get_active_editor_viewport() const;

public:
	virtual String get_plugin_name() const override { return "GIF"; }
	GIFEditorPlugin();
	~GIFEditorPlugin();
};

#endif
