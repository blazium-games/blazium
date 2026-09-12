/**************************************************************************/
/*  gif_editor_plugin.cpp                                                 */
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

#include "gif_editor_plugin.h"

#include "modules/gif/gif_texture.h"

#include "core/config/engine.h"
#include "core/os/os.h"
#include "editor/editor_data.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/plugins/game_view_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/slider.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/animation.h"
#include "scene/resources/image_texture.h"
#include "servers/display_server.h"

class GIFInspectorControls : public VBoxContainer {
	GDCLASS(GIFInspectorControls, VBoxContainer);

	Ref<GIFTexture> target;
	TextureRect *preview = nullptr;
	Label *info = nullptr;
	Button *play_btn = nullptr;
	CheckButton *loop_btn = nullptr;
	HSlider *frame_slider = nullptr;

	void _load_pressed() {
		EditorFileDialog *fd = memnew(EditorFileDialog);
		fd->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		fd->set_access(EditorFileDialog::ACCESS_RESOURCES);
		fd->add_filter("*.gif", "GIF");
		add_child(fd);
		fd->connect("file_selected", callable_mp(this, &GIFInspectorControls::_file_loaded));
		fd->popup_file_dialog();
	}

	void _file_loaded(const String &p_path) {
		if (target.is_valid()) {
			target->load_from_path(p_path);
			_refresh();
		}
	}

	void _save_pressed() {
		EditorFileDialog *fd = memnew(EditorFileDialog);
		fd->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		fd->set_access(EditorFileDialog::ACCESS_RESOURCES);
		fd->add_filter("*.gif", "GIF");
		add_child(fd);
		fd->connect("file_selected", callable_mp(this, &GIFInspectorControls::_file_saved));
		fd->popup_file_dialog();
	}

	void _file_saved(const String &p_path) {
		if (target.is_valid()) {
			target->save_to_path(p_path);
		}
	}

	void _rebake_pressed() {
		if (target.is_valid()) {
			target->bake_frames();
			_refresh();
		}
	}

	void _play_toggled(bool p_pressed) {
		if (target.is_valid()) {
			target->set_play(p_pressed);
		}
		if (play_btn) {
			play_btn->set_text(p_pressed ? TTR("Stop") : TTR("Play"));
			if (preview) {
				preview->queue_redraw();
			}
		}
	}

	void _loop_toggled(bool p_pressed) {
		if (target.is_valid()) {
			target->set_loop(p_pressed);
		}
	}

	void _frame_changed(double p_value) {
		if (target.is_valid()) {
			target->set_current_frame(int(p_value));
		}
		if (play_btn && target.is_valid()) {
			const bool playing = target->get_play();
			play_btn->set_text(playing ? TTR("Stop") : TTR("Play"));
			play_btn->set_pressed_no_signal(playing);
		}
		if (preview) {
			preview->queue_redraw();
		}
	}

	void _refresh() {
		if (target.is_null()) {
			return;
		}
		if (info) {
			info->set_text(vformat("%d frames, %dx%d, loop %d", target->get_frame_count(), target->get_canvas_size().x, target->get_canvas_size().y, target->get_netscape_loop_count()));
		}
		if (play_btn) {
			const bool playing = target->get_play();
			play_btn->set_pressed_no_signal(playing);
			play_btn->set_text(playing ? TTR("Stop") : TTR("Play"));
		}
		if (loop_btn) {
			loop_btn->set_pressed_no_signal(target->get_loop());
		}
		if (frame_slider) {
			frame_slider->set_block_signals(true);
			frame_slider->set_max(MAX(0, target->get_frame_count() - 1));
			frame_slider->set_value(target->get_current_frame());
			frame_slider->set_block_signals(false);
		}
	}

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_ENTER_TREE:
			case NOTIFICATION_VISIBILITY_CHANGED: {
				set_process(is_visible_in_tree());
			} break;
			case NOTIFICATION_PROCESS: {
				if (target.is_valid() && target->get_play() && frame_slider) {
					frame_slider->set_block_signals(true);
					frame_slider->set_value(target->get_current_frame());
					frame_slider->set_block_signals(false);
				}
				if (preview) {
					preview->queue_redraw();
				}
			} break;
		}
	}

public:
	void set_target(const Ref<GIFTexture> &p_texture) {
		target = p_texture;
		if (preview) {
			preview->set_texture(target);
		}
		_refresh();
	}

	GIFInspectorControls() {
		set_process(true);
		preview = memnew(TextureRect);
		preview->set_custom_minimum_size(Size2(0, 160) * EDSCALE);
		preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		add_child(preview);

		info = memnew(Label);
		add_child(info);

		HBoxContainer *row = memnew(HBoxContainer);
		add_child(row);

		play_btn = memnew(Button);
		play_btn->set_toggle_mode(true);
		play_btn->connect(SceneStringName(toggled), callable_mp(this, &GIFInspectorControls::_play_toggled));
		play_btn->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
		row->add_child(play_btn);

		loop_btn = memnew(CheckButton);
		loop_btn->set_text(TTR("Loop"));
		loop_btn->connect(SceneStringName(toggled), callable_mp(this, &GIFInspectorControls::_loop_toggled));
		row->add_child(loop_btn);

		HBoxContainer *slider_row = memnew(HBoxContainer);
		add_child(slider_row);
		Label *frame_label = memnew(Label);
		frame_label->set_text(TTR("Frame"));
		slider_row->add_child(frame_label);
		frame_slider = memnew(HSlider);
		frame_slider->set_min(0);
		frame_slider->set_step(1);
		frame_slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		frame_slider->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		frame_slider->set_custom_minimum_size(Size2(0, 20) * EDSCALE);
		frame_slider->connect(SNAME("value_changed"), callable_mp(this, &GIFInspectorControls::_frame_changed));
		slider_row->add_child(frame_slider);

		HBoxContainer *btns = memnew(HBoxContainer);
		add_child(btns);
		Button *load = memnew(Button);
		load->set_text(TTR("Load"));
		load->connect(SceneStringName(pressed), callable_mp(this, &GIFInspectorControls::_load_pressed));
		btns->add_child(load);
		Button *save = memnew(Button);
		save->set_text(TTR("Save as GIF"));
		save->connect(SceneStringName(pressed), callable_mp(this, &GIFInspectorControls::_save_pressed));
		btns->add_child(save);
		Button *rebake = memnew(Button);
		rebake->set_text(TTR("Rebake"));
		rebake->connect(SceneStringName(pressed), callable_mp(this, &GIFInspectorControls::_rebake_pressed));
		btns->add_child(rebake);

		_refresh();
	}
};

bool EditorInspectorPluginGIF::can_handle(Object *p_object) {
	return Object::cast_to<GIFTexture>(p_object) != nullptr;
}

void EditorInspectorPluginGIF::parse_begin(Object *p_object) {
	Ref<GIFTexture> tex = Object::cast_to<GIFTexture>(p_object);
	if (tex.is_null()) {
		return;
	}
	GIFInspectorControls *controls = memnew(GIFInspectorControls);
	controls->set_target(tex);
	add_custom_control(controls);
}

bool GIFPreviewGenerator::handles(const String &p_type) const {
	return p_type == "GIFTexture";
}

Ref<Texture2D> GIFPreviewGenerator::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<GIFTexture> tex = p_from;
	if (tex.is_null() || tex->get_frame_count() == 0) {
		return Ref<Texture2D>();
	}
	Ref<Image> img = tex->get_baked_image(0);
	if (img.is_null()) {
		img = tex->get_source_image(0);
	}
	if (img.is_null()) {
		return Ref<Texture2D>();
	}
	img = img->duplicate();
	if (img->is_compressed()) {
		img->decompress();
	}
	if (p_size.x > 0 && p_size.y > 0) {
		img->resize(MAX(1, int(p_size.x)), MAX(1, int(p_size.y)), Image::INTERPOLATE_BILINEAR);
	}
	return ImageTexture::create_from_image(img);
}

Viewport *GIFEditorPlugin::_get_active_editor_viewport() const {
	EditorInterface *ei = EditorInterface::get_singleton();
	if (!ei) {
		return nullptr;
	}
	Viewport *vp2d = ei->get_editor_viewport_2d();
	Viewport *vp3d = ei->get_editor_viewport_3d(0);
	if (vp2d && vp2d->is_inside_tree()) {
		return vp2d;
	}
	if (vp3d) {
		return vp3d;
	}
	return vp2d;
}

void GIFEditorPlugin::_toggle_recording(GIFRecorder::Source p_source, Viewport *p_viewport) {
	if (recorder.is_valid() && recorder->is_recording()) {
		Ref<GIFTexture> tex = recorder->stop();
		pending_save_kind = "record";
		if (!save_dialog) {
			return;
		}
		save_dialog->set_meta("gif_texture", tex);
		save_dialog->popup_file_dialog();
		return;
	}
	recorder.instantiate();
	Error err = OK;
	if (p_source == GIFRecorder::SOURCE_WINDOW) {
		err = recorder->start_window();
	} else if (p_viewport) {
		err = recorder->start_viewport(p_viewport);
	} else {
		err = ERR_UNCONFIGURED;
	}
	if (err != OK) {
		recorder.unref();
		ERR_FAIL_MSG("Could not start GIF recording.");
	}
	print_line("GIF recording started. Choose the same menu item again to stop and save.");
}

void GIFEditorPlugin::_record_editor_viewport() {
	Viewport *vp = _get_active_editor_viewport();
	ERR_FAIL_NULL_MSG(vp, "No editor viewport is available to record.");
	_toggle_recording(GIFRecorder::SOURCE_VIEWPORT, vp);
}

void GIFEditorPlugin::_process_game_capture() {
	if (!capturing_game || recorder.is_null()) {
		return;
	}
	Control *gv = Object::cast_to<Control>(ObjectDB::get_instance(game_view_id));
	if (!gv) {
		return;
	}
	const Rect2 rect = gv->get_global_rect();
	const Vector2i win = DisplayServer::get_singleton()->window_get_position();
	Ref<Image> shot = DisplayServer::get_singleton()->screen_get_image_rect(Rect2i(win + Vector2i(rect.position), Vector2i(rect.size)));
	if (shot.is_valid()) {
		recorder->add_frame(shot);
	}
}

void GIFEditorPlugin::_record_game_viewport() {
	if (capturing_game && recorder.is_valid()) {
		capturing_game = false;
		SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (tree && tree->is_connected(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture))) {
			tree->disconnect(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture));
		}
		Ref<GIFTexture> tex = recorder->stop();
		save_dialog->set_meta("gif_texture", tex);
		save_dialog->popup_file_dialog();
		return;
	}
	if (!EditorRunBar::get_singleton() || !EditorRunBar::get_singleton()->is_playing()) {
		ERR_FAIL_MSG("Start the project before recording the game viewport.");
	}
	GameView *gv = nullptr;
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_gui_base()) {
		TypedArray<Node> nodes = EditorNode::get_singleton()->get_gui_base()->find_children("*", "GameView", true, false);
		if (nodes.size()) {
			gv = Object::cast_to<GameView>(nodes[0]);
		}
	}
	ERR_FAIL_NULL_MSG(gv, "Game View is not available.");
	recorder.instantiate();
	game_view_id = gv->get_instance_id();
	capturing_game = true;
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree && !tree->is_connected(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture))) {
		tree->connect(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture));
	}
	print_line("GIF game viewport recording started. Choose the same menu item again to stop and save.");
}

void GIFEditorPlugin::_record_full_window() {
	_toggle_recording(GIFRecorder::SOURCE_WINDOW, nullptr);
}

void GIFEditorPlugin::_export_animation_player() {
	EditorSelection *sel = EditorInterface::get_singleton()->get_selection();
	ERR_FAIL_NULL(sel);
	AnimationPlayer *ap = nullptr;
	for (Node *n : sel->get_selected_node_list()) {
		ap = Object::cast_to<AnimationPlayer>(n);
		if (ap) {
			break;
		}
	}
	ERR_FAIL_NULL_MSG(ap, "Select an AnimationPlayer to export as GIF.");
	const StringName current = ap->get_assigned_animation();
	ERR_FAIL_COND_MSG(String(current).is_empty(), "AnimationPlayer has no current animation.");
	Ref<Animation> anim = ap->get_animation(current);
	ERR_FAIL_COND(anim.is_null());
	Viewport *vp = _get_active_editor_viewport();
	ERR_FAIL_NULL(vp);
	const double length = anim->get_length();
	const double step = MAX(1.0 / 12.0, anim->get_step() > 0.0 ? anim->get_step() : 1.0 / 12.0);
	Ref<GIFRecorder> rec;
	rec.instantiate();
	rec->set_fps(int(Math::round(1.0 / step)));
	for (double t = 0.0; t <= length + 0.0001; t += step) {
		ap->seek(t, true, true);
		if (vp->get_texture().is_valid()) {
			rec->add_frame(vp->get_texture()->get_image());
		}
	}
	Ref<GIFTexture> result = rec->stop();
	if (result.is_null()) {
		result.instantiate();
	}
	pending_save_kind = "anim";
	save_dialog->set_meta("gif_texture", result);
	save_dialog->popup_file_dialog();
}

void GIFEditorPlugin::_save_dialog_file_selected(const String &p_path) {
	Ref<GIFTexture> tex = save_dialog->get_meta("gif_texture");
	if (tex.is_valid()) {
		tex->save_to_path(p_path);
		print_line(vformat("Saved GIF: %s", p_path));
	}
}

GIFEditorPlugin::GIFEditorPlugin() {
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
	preview_generator.instantiate();
	EditorResourcePreview::get_singleton()->add_preview_generator(preview_generator);

	add_tool_menu_item(TTR("Record Editor Viewport GIF"), callable_mp(this, &GIFEditorPlugin::_record_editor_viewport));
	add_tool_menu_item(TTR("Record Game Viewport GIF"), callable_mp(this, &GIFEditorPlugin::_record_game_viewport));
	add_tool_menu_item(TTR("Record Full Editor Window GIF"), callable_mp(this, &GIFEditorPlugin::_record_full_window));
	add_tool_menu_item(TTR("Export AnimationPlayer as GIF"), callable_mp(this, &GIFEditorPlugin::_export_animation_player));

	save_dialog = memnew(EditorFileDialog);
	save_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	save_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	save_dialog->add_filter("*.gif", "GIF");
	save_dialog->connect("file_selected", callable_mp(this, &GIFEditorPlugin::_save_dialog_file_selected));
	EditorNode::get_singleton()->get_gui_base()->add_child(save_dialog);
}

GIFEditorPlugin::~GIFEditorPlugin() {
	remove_tool_menu_item(TTR("Record Editor Viewport GIF"));
	remove_tool_menu_item(TTR("Record Game Viewport GIF"));
	remove_tool_menu_item(TTR("Record Full Editor Window GIF"));
	remove_tool_menu_item(TTR("Export AnimationPlayer as GIF"));

	if (capturing_game) {
		capturing_game = false;
		SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton() ? OS::get_singleton()->get_main_loop() : nullptr);
		if (tree && tree->is_connected(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture))) {
			tree->disconnect(SNAME("process_frame"), callable_mp(this, &GIFEditorPlugin::_process_game_capture));
		}
	}
	if (inspector_plugin.is_valid()) {
		remove_inspector_plugin(inspector_plugin);
		inspector_plugin.unref();
	}
	if (EditorResourcePreview::get_singleton() && preview_generator.is_valid()) {
		EditorResourcePreview::get_singleton()->remove_preview_generator(preview_generator);
		preview_generator.unref();
	}
	recorder.unref();
}

#endif
