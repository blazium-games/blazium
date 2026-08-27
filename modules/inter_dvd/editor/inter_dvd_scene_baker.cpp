/**************************************************************************/
/*  inter_dvd_scene_baker.cpp                                             */
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

#include "inter_dvd_scene_baker.h"

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/author/inter_dvd_vob_mux.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/line_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#ifndef _3D_DISABLED
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/node_3d.h"
#endif
#include "scene/animation/animation_player.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/link_button.h"
#include "scene/gui/nine_patch_rect.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/range.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/slider.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/video_stream_player.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/sprite_frames.h"
#include "scene/resources/style_box.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_texture.h"

#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/texture.h"
#include "servers/rendering_server.h"
#include "servers/text_server.h"
namespace {

constexpr int BAKE_WIDTH = 720;
constexpr int BAKE_HEIGHT = 480;
constexpr double BAKE_FPS = 30.0;
constexpr double DURATION_SLACK_SEC = 0.25;

String packed_visual_fingerprint(const Ref<PackedScene> &p_scene);

String cache_root() {
	return InterDVDSettings::cache_path();
}

String scene_key(const Ref<InterDVDCell> &p_cell) {
	const Ref<PackedScene> scene = p_cell.is_valid() ? p_cell->get_packed_scene() : Ref<PackedScene>();
	String path = scene.is_valid() ? scene->get_path() : String();
	if (path.is_empty() && scene.is_valid()) {
		path = vformat("embedded_%d", scene->get_instance_id());
	}
	uint64_t mtime = 0;
	if (!path.is_empty() && !path.begins_with("embedded_")) {
		String abs = path;
		const int sub = path.find("::");
		if (sub >= 0) {
			abs = path.substr(0, sub);
		}
		if (ProjectSettings::get_singleton()) {
			abs = ProjectSettings::get_singleton()->globalize_path(abs);
		}
		mtime = FileAccess::get_modified_time(abs);
	}
	const String pip = p_cell.is_valid() ? p_cell->get_pip_source_path() : String();
	uint64_t pip_mtime = 0;
	if (!pip.is_empty() && FileAccess::exists(pip)) {
		pip_mtime = FileAccess::get_modified_time(pip);
	}
	const String menu_audio = p_cell.is_valid() ? p_cell->get_audio_path() : String();
	uint64_t audio_mtime = 0;
	if (!menu_audio.is_empty() && FileAccess::exists(menu_audio)) {
		audio_mtime = FileAccess::get_modified_time(menu_audio);
	}
	const double duration = p_cell.is_valid() ? p_cell->get_duration_sec() : 0.0;
	const double pad = p_cell.is_valid() ? p_cell->get_loop_pad_sec() : 0.0;
	const bool audio = p_cell.is_valid() && p_cell->get_include_audio();
	const double hold = p_cell.is_valid() ? p_cell->get_bake_hold_sec() : 0.0;
	const double lead = p_cell.is_valid() ? p_cell->get_pip_lead_sec() : 0.0;
	const Rect2 pip_rect = p_cell.is_valid() ? p_cell->get_pip_rect() : Rect2();
	return vformat("%s|%s|%.3f|%d|%s|%s|%s|%s|%.3f|%s|%.3f|%.3f|%s|pipv12", path, uitos(mtime), duration, audio ? 1 : 0, pip, uitos(pip_mtime), menu_audio, uitos(audio_mtime), pad, packed_visual_fingerprint(scene), hold, lead, String(pip_rect));
}

String resource_mtime_token(const Variant &p_value) {
	if (p_value.get_type() != Variant::OBJECT) {
		return String(p_value);
	}
	const Ref<Resource> res = p_value;
	if (res.is_null()) {
		return String();
	}
	String res_path = res->get_path();
	if (res_path.is_empty()) {
		return vformat("res_%d", res->get_instance_id());
	}
	const int sub = res_path.find("::");
	if (sub >= 0) {
		res_path = res_path.substr(0, sub);
	}
	if (ProjectSettings::get_singleton()) {
		res_path = ProjectSettings::get_singleton()->globalize_path(res_path);
	}
	return vformat("%s|%s", res_path, uitos(FileAccess::exists(res_path) ? FileAccess::get_modified_time(res_path) : 0));
}

String packed_visual_fingerprint(const Ref<PackedScene> &p_scene) {
	if (p_scene.is_null()) {
		return String();
	}
	const Ref<SceneState> st = p_scene->get_state();
	if (st.is_null()) {
		return String();
	}
	String acc;
	for (int i = 0; i < st->get_node_count(); i++) {
		acc += String(st->get_node_type(i));
		acc += "/";
		acc += String(st->get_node_name(i));
		acc += ":";
		for (int p = 0; p < st->get_node_property_count(i); p++) {
			const String n = st->get_node_property_name(i, p);
			if (n != "text" && n != "visible" && n != "modulate" && n != "self_modulate" && n != "texture" &&
					n != "position" && n != "offset_left" && n != "offset_top" && n != "offset_right" &&
					n != "offset_bottom" && n != "size" && n != "bbcode_enabled") {
				continue;
			}
			acc += n;
			acc += "=";
			acc += resource_mtime_token(st->get_node_property_value(i, p));
			acc += ";";
		}
	}
	return acc.md5_text().substr(0, 16);
}

void play_animations(Node *p_node) {
	if (AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(p_node)) {
		List<StringName> names;
		ap->get_animation_list(&names);
		if (!names.is_empty() && !ap->is_playing()) {
			ap->play(names.front()->get());
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		play_animations(p_node->get_child(i));
	}
}

void collect_audio_path(Node *p_node, String &r_path, const String &p_wav_out) {
	if (!r_path.is_empty()) {
		return;
	}
	Ref<AudioStream> stream;
	if (AudioStreamPlayer *p = Object::cast_to<AudioStreamPlayer>(p_node)) {
		stream = p->get_stream();
		p->play();
	} else if (AudioStreamPlayer2D *p = Object::cast_to<AudioStreamPlayer2D>(p_node)) {
		stream = p->get_stream();
		p->play();
#ifndef _3D_DISABLED
	} else if (AudioStreamPlayer3D *p = Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		stream = p->get_stream();
		p->play();
#endif
	}
	if (stream.is_valid()) {
		const String res_path = stream->get_path();
		if (!res_path.is_empty()) {
			r_path = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(res_path) : res_path;
		} else if (AudioStreamWAV *wav = Object::cast_to<AudioStreamWAV>(stream.ptr())) {
			if (wav->save_to_wav(p_wav_out) == OK && FileAccess::exists(p_wav_out)) {
				r_path = p_wav_out;
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		collect_audio_path(p_node->get_child(i), r_path, p_wav_out);
	}
}

uint8_t glyph5x7(char32_t p_ch, int p_col) {
	char32_t ch = p_ch;
	if (ch >= 'a' && ch <= 'z') {
		ch = ch - 'a' + 'A';
	}

	static const uint8_t table[27][5] = {
		{ 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x1E, 0x05, 0x05, 0x05, 0x1E },
		{ 0x1F, 0x15, 0x15, 0x15, 0x0A },
		{ 0x0E, 0x11, 0x11, 0x11, 0x0A },
		{ 0x1F, 0x11, 0x11, 0x11, 0x0E },
		{ 0x1F, 0x15, 0x15, 0x11, 0x11 },
		{ 0x1F, 0x05, 0x05, 0x01, 0x01 },
		{ 0x0E, 0x11, 0x15, 0x15, 0x1C },
		{ 0x1F, 0x04, 0x04, 0x04, 0x1F },
		{ 0x11, 0x11, 0x1F, 0x11, 0x11 },
		{ 0x08, 0x10, 0x11, 0x0F, 0x01 },
		{ 0x1F, 0x04, 0x0A, 0x11, 0x11 },
		{ 0x1F, 0x10, 0x10, 0x10, 0x10 },
		{ 0x1F, 0x02, 0x04, 0x02, 0x1F },
		{ 0x1F, 0x02, 0x04, 0x08, 0x1F },
		{ 0x0E, 0x11, 0x11, 0x11, 0x0E },
		{ 0x1F, 0x05, 0x05, 0x05, 0x02 },
		{ 0x0E, 0x11, 0x19, 0x11, 0x1E },
		{ 0x1F, 0x05, 0x0D, 0x15, 0x12 },
		{ 0x12, 0x15, 0x15, 0x15, 0x09 },
		{ 0x01, 0x01, 0x1F, 0x01, 0x01 },
		{ 0x0F, 0x10, 0x10, 0x10, 0x0F },
		{ 0x07, 0x08, 0x10, 0x08, 0x07 },
		{ 0x0F, 0x10, 0x0C, 0x10, 0x0F },
		{ 0x11, 0x0A, 0x04, 0x0A, 0x11 },
		{ 0x01, 0x02, 0x1C, 0x02, 0x01 },
		{ 0x11, 0x19, 0x15, 0x13, 0x11 },
	};
	int idx = 0;
	if (ch >= 'A' && ch <= 'Z') {
		idx = int(ch - 'A') + 1;
	} else if (ch != ' ') {
		idx = 0;
	}
	if (idx > 0 || ch == ' ') {
		return table[idx][CLAMP(p_col, 0, 4)];
	}
	static const uint8_t extra[][5] = {
		{ 0x0E, 0x13, 0x15, 0x19, 0x0E },
		{ 0x00, 0x12, 0x1F, 0x10, 0x00 },
		{ 0x12, 0x19, 0x15, 0x13, 0x12 },
		{ 0x11, 0x15, 0x15, 0x15, 0x0A },
		{ 0x07, 0x04, 0x04, 0x1F, 0x04 },
		{ 0x17, 0x15, 0x15, 0x15, 0x09 },
		{ 0x0E, 0x15, 0x15, 0x15, 0x08 },
		{ 0x01, 0x01, 0x19, 0x05, 0x03 },
		{ 0x0A, 0x15, 0x15, 0x15, 0x0A },
		{ 0x02, 0x15, 0x15, 0x15, 0x0E },
		{ 0x00, 0x10, 0x10, 0x00, 0x00 },
		{ 0x00, 0x10, 0x18, 0x00, 0x00 },
		{ 0x00, 0x0A, 0x0A, 0x00, 0x00 },
		{ 0x00, 0x04, 0x04, 0x04, 0x00 },
		{ 0x10, 0x08, 0x04, 0x02, 0x01 },
		{ 0x00, 0x00, 0x17, 0x00, 0x00 },
		{ 0x02, 0x01, 0x15, 0x05, 0x02 },
		{ 0x0E, 0x11, 0x15, 0x15, 0x06 },
		{ 0x10, 0x10, 0x10, 0x10, 0x10 },
		{ 0x04, 0x04, 0x1F, 0x04, 0x04 },
		{ 0x00, 0x0E, 0x11, 0x00, 0x00 },
		{ 0x00, 0x11, 0x0E, 0x00, 0x00 },
	};
	int extra_idx = -1;
	if (ch >= '0' && ch <= '9') {
		extra_idx = int(ch - '0');
	} else if (ch == '.') {
		extra_idx = 10;
	} else if (ch == ',') {
		extra_idx = 11;
	} else if (ch == ':') {
		extra_idx = 12;
	} else if (ch == '-') {
		extra_idx = 13;
	} else if (ch == '/') {
		extra_idx = 14;
	} else if (ch == '!') {
		extra_idx = 15;
	} else if (ch == '?') {
		extra_idx = 16;
	} else if (ch == '@') {
		extra_idx = 17;
	} else if (ch == '_') {
		extra_idx = 18;
	} else if (ch == '+') {
		extra_idx = 19;
	} else if (ch == '(') {
		extra_idx = 20;
	} else if (ch == ')') {
		extra_idx = 21;
	}
	if (extra_idx >= 0) {
		return extra[extra_idx][CLAMP(p_col, 0, 4)];
	}
	return 0x00;
}

Rect2i control_rect(const Control *p_ctrl) {
	if (!p_ctrl) {
		return Rect2i();
	}
	if (p_ctrl->is_inside_tree()) {
		return Rect2i(p_ctrl->get_global_rect());
	}
	return Rect2i(p_ctrl->get_position(), p_ctrl->get_size());
}

void blit_clipped(Ref<Image> &p_img, const Rect2i &p_rect, const Color &p_color) {
	const Rect2i clipped = p_rect.intersection(Rect2i(0, 0, BAKE_WIDTH, BAKE_HEIGHT));
	if (clipped.has_area()) {
		p_img->fill_rect(clipped, p_color);
	}
}

void draw_text_line(Ref<Image> &p_img, const String &p_line, int p_x, int p_y, const Color &p_color, int p_scale) {
	for (int i = 0; i < p_line.length(); i++) {
		const char32_t ch = p_line[i];
		for (int col = 0; col < 5; col++) {
			const uint8_t bits = glyph5x7(ch, col);
			for (int row = 0; row < 7; row++) {
				if (bits & (1 << row)) {
					blit_clipped(p_img, Rect2i(p_x + i * 6 * p_scale + col * p_scale, p_y + row * p_scale, p_scale, p_scale), p_color);
				}
			}
		}
	}
}

Vector<String> wrap_text_lines(const String &p_text, int p_max_chars) {
	Vector<String> lines;
	const Vector<String> raw = p_text.replace("\r", "").split("\n");
	const int max_chars = MAX(p_max_chars, 1);
	for (int r = 0; r < raw.size(); r++) {
		String rest = raw[r];
		if (rest.is_empty()) {
			lines.push_back(String());
			continue;
		}
		while (rest.length() > max_chars) {
			int cut = rest.rfind(" ", max_chars);
			if (cut <= 0) {
				cut = max_chars;
			}
			lines.push_back(rest.substr(0, cut).strip_edges());
			rest = rest.substr(cut).strip_edges();
		}
		if (!rest.is_empty() || lines.is_empty()) {
			lines.push_back(rest);
		}
	}
	return lines;
}

void draw_text_at(Ref<Image> &p_img, const String &p_text, const Rect2 &p_rect, const Color &p_color, int p_font_size, bool p_center, bool p_wrap = false) {
	if (p_text.is_empty()) {
		return;
	}
	const int scale = MAX(p_font_size / 8, 2);
	const int cell = 6 * scale;
	const int line_h = 8 * scale;
	const int max_chars = p_wrap ? MAX(int(p_rect.size.x) / cell, 1) : p_text.length();
	const Vector<String> lines = p_wrap ? wrap_text_lines(p_text, max_chars) : wrap_text_lines(p_text, 100000);
	const int block_h = lines.size() * line_h;
	int y = int(p_rect.position.y);
	if (!p_wrap) {
		y = int(p_rect.position.y + MAX((p_rect.size.y - block_h) * 0.5, 0.0));
	} else {
		y += 2;
	}
	for (int i = 0; i < lines.size(); i++) {
		const String &line = lines[i];
		int x = int(p_rect.position.x);
		if (p_center) {
			x = int(p_rect.position.x + (p_rect.size.x - line.length() * cell) * 0.5);
		}
		draw_text_line(p_img, line, x, y + i * line_h, p_color, scale);
	}
}

void blit_image(Ref<Image> &p_dst, const Ref<Image> &p_src, const Rect2i &p_dest) {
	if (p_src.is_null() || p_src->is_empty() || !p_dest.has_area()) {
		return;
	}
	Ref<Image> copy = p_src->duplicate();
	if (copy->get_format() != Image::FORMAT_RGBA8) {
		copy->convert(Image::FORMAT_RGBA8);
	}
	if (p_dst->get_format() != Image::FORMAT_RGBA8) {
		p_dst->convert(Image::FORMAT_RGBA8);
	}
	if (copy->get_size() != p_dest.size) {
		copy->resize(MAX(p_dest.size.x, 1), MAX(p_dest.size.y, 1), Image::INTERPOLATE_BILINEAR);
	}
	const Rect2i clipped = p_dest.intersection(Rect2i(0, 0, BAKE_WIDTH, BAKE_HEIGHT));
	if (!clipped.has_area()) {
		return;
	}
	const Point2i src_ofs = clipped.position - p_dest.position;
	p_dst->blend_rect(copy, Rect2i(src_ofs, clipped.size), clipped.position);
}

void blit_image_mod(Ref<Image> &p_dst, const Ref<Image> &p_src, const Rect2i &p_dest, const Color &p_mod) {
	if (p_mod.r >= 0.999f && p_mod.g >= 0.999f && p_mod.b >= 0.999f && p_mod.a >= 0.999f) {
		blit_image(p_dst, p_src, p_dest);
		return;
	}
	if (p_src.is_null() || p_src->is_empty() || !p_dest.has_area()) {
		return;
	}
	Ref<Image> tinted = p_src->duplicate();
	if (tinted->get_format() != Image::FORMAT_RGBA8) {
		tinted->convert(Image::FORMAT_RGBA8);
	}
	for (int y = 0; y < tinted->get_height(); y++) {
		for (int x = 0; x < tinted->get_width(); x++) {
			tinted->set_pixel(x, y, tinted->get_pixel(x, y) * p_mod);
		}
	}
	blit_image(p_dst, tinted, p_dest);
}

Color canvas_modulate(const CanvasItem *p_ci, const Color &p_parent) {
	if (!p_ci) {
		return p_parent;
	}
	return p_parent * p_ci->get_modulate() * p_ci->get_self_modulate();
}

bool skip_software_node(const Node *p_node) {
	if (!p_node) {
		return true;
	}
	if (Object::cast_to<AudioStreamPlayer>(p_node) || Object::cast_to<AudioStreamPlayer2D>(p_node)) {
		return true;
	}
#ifndef _3D_DISABLED
	if (Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		return true;
	}
#endif
	const String type = p_node->get_class();
	return type == "Timer" || type == "CollisionShape2D" || type == "CollisionPolygon2D" || type == "CollisionShape3D" || type == "RayCast2D" || type == "RayCast3D" || type == "VisibleOnScreenNotifier2D" || type == "VisibleOnScreenNotifier3D";
}

bool node_or_packed_has_3d(const Node *p_node) {
#ifndef _3D_DISABLED
	if (Object::cast_to<Node3D>(p_node)) {
		return true;
	}
#endif
	if (!p_node) {
		return false;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (node_or_packed_has_3d(p_node->get_child(i))) {
			return true;
		}
	}
	return false;
}

bool packed_has_node3d(const Ref<PackedScene> &p_scene) {
	if (p_scene.is_null()) {
		return false;
	}
	const Ref<SceneState> st = p_scene->get_state();
	if (st.is_null()) {
		return false;
	}
	for (int i = 0; i < st->get_node_count(); i++) {
		if (ClassDB::is_parent_class(st->get_node_type(i), "Node3D")) {
			return true;
		}
	}
	return false;
}

bool dummy_renderer() {
	return OS::get_singleton() && OS::get_singleton()->get_current_rendering_method() == "dummy";
}

Rect2i texture_dest_rect(const TextureRect *p_tr, const Size2i &p_src) {
	const Rect2i gr = control_rect(p_tr);
	if (p_src.x <= 0 || p_src.y <= 0) {
		return gr;
	}
	const TextureRect::StretchMode mode = p_tr->get_stretch_mode();
	if (mode == TextureRect::STRETCH_KEEP_ASPECT || mode == TextureRect::STRETCH_KEEP_ASPECT_CENTERED) {
		const float scale = MIN(float(gr.size.x) / float(p_src.x), float(gr.size.y) / float(p_src.y));
		const int w = MAX(int(Math::round(p_src.x * scale)), 1);
		const int h = MAX(int(Math::round(p_src.y * scale)), 1);
		int x = gr.position.x;
		int y = gr.position.y;
		if (mode == TextureRect::STRETCH_KEEP_ASPECT_CENTERED) {
			x += (gr.size.x - w) / 2;
			y += (gr.size.y - h) / 2;
		}
		return Rect2i(x, y, w, h);
	}
	if (mode == TextureRect::STRETCH_KEEP_ASPECT_COVERED) {
		const float scale = MAX(float(gr.size.x) / float(p_src.x), float(gr.size.y) / float(p_src.y));
		const int w = MAX(int(Math::round(p_src.x * scale)), 1);
		const int h = MAX(int(Math::round(p_src.y * scale)), 1);
		return Rect2i(gr.position.x + (gr.size.x - w) / 2, gr.position.y + (gr.size.y - h) / 2, w, h);
	}
	return gr;
}

Ref<Image> image_from_texture(const Ref<Texture2D> &p_tex) {
	if (p_tex.is_null()) {
		return Ref<Image>();
	}
	Ref<Image> src = p_tex->get_image();
	if (src.is_valid() && !src->is_empty()) {
		return src;
	}
	String path = p_tex->get_path();
	if (path.is_empty()) {
		return src;
	}
	if (ProjectSettings::get_singleton()) {
		path = ProjectSettings::get_singleton()->globalize_path(path);
	}
	return Image::load_from_file(path);
}

void draw_texture_rect(Ref<Image> &p_img, const TextureRect *p_tr) {
	const Ref<Image> src = image_from_texture(p_tr->get_texture());
	if (src.is_null() || src->is_empty()) {
		return;
	}
	blit_image(p_img, src, texture_dest_rect(p_tr, src->get_size()));
}

void draw_stylebox(Ref<Image> &p_img, const Ref<StyleBox> &p_sb, const Rect2 &p_rect) {
	if (p_sb.is_null() || Object::cast_to<StyleBoxEmpty>(p_sb.ptr())) {
		return;
	}
	if (const StyleBoxTexture *stex = Object::cast_to<StyleBoxTexture>(p_sb.ptr())) {
		const Ref<Image> src = image_from_texture(stex->get_texture());
		if (src.is_valid() && !src->is_empty()) {
			blit_image(p_img, src, Rect2i(p_rect));
		}
		return;
	}
	if (const StyleBoxFlat *flat = Object::cast_to<StyleBoxFlat>(p_sb.ptr())) {
		const Rect2i r(p_rect);
		if (flat->is_draw_center_enabled() && flat->get_bg_color().a > 0.01f) {
			blit_clipped(p_img, r, flat->get_bg_color());
		}
		const Color bc = flat->get_border_color();
		if (bc.a > 0.01f) {
			const int l = MAX(flat->get_border_width(SIDE_LEFT), 0);
			const int t = MAX(flat->get_border_width(SIDE_TOP), 0);
			const int ri = MAX(flat->get_border_width(SIDE_RIGHT), 0);
			const int b = MAX(flat->get_border_width(SIDE_BOTTOM), 0);
			if (l > 0) {
				blit_clipped(p_img, Rect2i(r.position.x, r.position.y, l, r.size.y), bc);
			}
			if (ri > 0) {
				blit_clipped(p_img, Rect2i(r.position.x + r.size.x - ri, r.position.y, ri, r.size.y), bc);
			}
			if (t > 0) {
				blit_clipped(p_img, Rect2i(r.position.x, r.position.y, r.size.x, t), bc);
			}
			if (b > 0) {
				blit_clipped(p_img, Rect2i(r.position.x, r.position.y + r.size.y - b, r.size.x, b), bc);
			}
		}
	}
}

void draw_control_style(Ref<Image> &p_img, const Control *p_ctrl, const StringName &p_name) {
	if (!p_ctrl) {
		return;
	}
	if (!p_ctrl->has_theme_stylebox_override(p_name) && !p_ctrl->is_inside_tree()) {
		return;
	}
	draw_stylebox(p_img, p_ctrl->get_theme_stylebox(p_name), control_rect(p_ctrl));
}

void draw_label_text(Ref<Image> &p_img, const Label *p_label) {
	draw_control_style(p_img, p_label, SNAME("normal"));
	Color color(1, 1, 1);
	if (p_label->has_theme_color_override("font_color")) {
		color = p_label->get_theme_color("font_color");
	} else if (p_label->is_inside_tree()) {
		color = p_label->get_theme_color("font_color");
	}
	int font_size = 0;
	if (p_label->has_theme_font_size_override("font_size") || p_label->is_inside_tree()) {
		font_size = p_label->get_theme_font_size(SNAME("font_size"));
	}
	if (font_size <= 0) {
		return;
	}
	const bool wrap = p_label->get_autowrap_mode() != TextServer::AUTOWRAP_OFF;
	draw_text_at(p_img, p_label->get_text(), control_rect(p_label), color, font_size, p_label->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_CENTER, wrap);
}

void draw_rich_text(Ref<Image> &p_img, const RichTextLabel *p_rtl) {
	draw_control_style(p_img, p_rtl, SNAME("normal"));
	Color color(1, 1, 1);
	int font_size = 0;
	if (p_rtl->has_theme_font_size_override("normal_font_size") || p_rtl->is_inside_tree()) {
		if (p_rtl->has_theme_color("default_color", SNAME("RichTextLabel")) || p_rtl->has_theme_color_override("default_color")) {
			color = p_rtl->get_theme_color("default_color", SNAME("RichTextLabel"));
		}
		font_size = p_rtl->get_theme_font_size(SNAME("normal_font_size"));
	}
	if (font_size <= 0) {
		return;
	}
	const String text = p_rtl->get_parsed_text().is_empty() ? p_rtl->get_text() : p_rtl->get_parsed_text();
	draw_text_at(p_img, text, control_rect(p_rtl), color, font_size, false, true);
}

void draw_sprite_tex(Ref<Image> &p_img, Node2D *p_node, const Ref<Texture2D> &p_tex, const Rect2 &p_local) {
	const Ref<Image> src = image_from_texture(p_tex);
	if (src.is_null() || src->is_empty()) {
		return;
	}
	const Rect2 dest = p_node->get_global_transform().xform(p_local);
	blit_image(p_img, src, Rect2i(dest));
}

void draw_button(Ref<Image> &p_img, const Button *p_button) {
	const Rect2 gr = control_rect(p_button);
	Ref<StyleBox> sb = p_button->get_theme_stylebox(SNAME("normal"));
	if (Object::cast_to<StyleBoxEmpty>(sb.ptr())) {
		return;
	}
	if (const StyleBoxTexture *stex = Object::cast_to<StyleBoxTexture>(sb.ptr())) {
		const Ref<Texture2D> tex = stex->get_texture();
		Ref<Image> src = image_from_texture(tex);

		if (src.is_valid() && !src->is_empty()) {
			blit_image(p_img, src, Rect2i(gr));
			return;
		}
	}
	Color bg;
	if (const StyleBoxFlat *flat = Object::cast_to<StyleBoxFlat>(sb.ptr())) {
		if (flat->get_bg_color().a < 0.01f && p_button->get_text().is_empty()) {
			return;
		}
		bg = flat->get_bg_color();

	} else {
		return;
	}
	blit_clipped(p_img, Rect2i(gr), bg);
	Color fg(0, 0, 0);
	if (p_button->has_theme_color_override("font_color")) {
		fg = p_button->get_theme_color("font_color");
	}
	int font_size = 0;
	if (p_button->has_theme_font_size_override("font_size") || p_button->is_inside_tree()) {
		font_size = p_button->get_theme_font_size(SNAME("font_size"));
	}
	if (font_size > 0) {
		draw_text_at(p_img, p_button->get_text(), gr, fg, font_size, true);
	}
}

bool scene_has_motion(Node *p_node) {
	if (!p_node) {
		return false;
	}
	if (p_node->has_method("inter_dvd_bake_time") || Object::cast_to<AnimationPlayer>(p_node) || Object::cast_to<AnimatedSprite2D>(p_node)) {
		return true;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (scene_has_motion(p_node->get_child(i))) {
			return true;
		}
	}
	return false;
}

void apply_bake_time(Node *p_node, double p_sec) {
	if (!p_node) {
		return;
	}
	if (p_node->has_method("inter_dvd_bake_time")) {
		p_node->call("inter_dvd_bake_time", p_sec);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		apply_bake_time(p_node->get_child(i), p_sec);
	}
}

double bake_hold_sec(Node *p_node) {
	if (!p_node || !p_node->has_method("inter_dvd_bake_hold_sec")) {
		return 0.0;
	}
	return MAX(double(p_node->call("inter_dvd_bake_hold_sec")), 0.0);
}

void draw_polygon2d(Ref<Image> &p_img, Polygon2D *p_poly, const Color &p_mod) {
	const PackedVector2Array pts = p_poly->get_polygon();
	if (pts.is_empty()) {
		return;
	}
	const Transform2D xf = p_poly->get_global_transform();
	Rect2 bounds(xf.xform(pts[0]), Size2());
	for (int i = 1; i < pts.size(); i++) {
		bounds.expand_to(xf.xform(pts[i]));
	}
	const Ref<Image> tex = image_from_texture(p_poly->get_texture());
	if (tex.is_valid() && !tex->is_empty()) {
		blit_image_mod(p_img, tex, Rect2i(bounds), p_mod);
	} else {
		blit_clipped(p_img, Rect2i(bounds), p_poly->get_color() * p_mod);
	}
}

void draw_line2d(Ref<Image> &p_img, const Line2D *p_line, const Color &p_mod) {
	const PackedVector2Array pts = p_line->get_points();
	if (pts.size() < 2) {
		return;
	}
	const Transform2D xf = p_line->get_global_transform();
	const int w = MAX(int(Math::ceil(p_line->get_width())), 1);
	const Color col = p_line->get_default_color() * p_mod;
	for (int i = 0; i + 1 < pts.size(); i++) {
		const Vector2 a = xf.xform(pts[i]);
		const Vector2 b = xf.xform(pts[i + 1]);
		const Vector2 d = b - a;
		const int steps = MAX(int(d.length()), 1);
		for (int s = 0; s <= steps; s++) {
			const Vector2 p = a.lerp(b, float(s) / float(steps));
			blit_clipped(p_img, Rect2i(int(p.x) - w / 2, int(p.y) - w / 2, w, w), col);
		}
	}
}

void draw_mesh2d(Ref<Image> &p_img, MeshInstance2D *p_mesh, const Color &p_mod) {
	const Ref<Image> src = image_from_texture(p_mesh->get_texture());
	if (src.is_null() || src->is_empty()) {
		return;
	}
	const Size2 sz = src->get_size();
	const Rect2 dest = p_mesh->get_global_transform().xform(Rect2(-sz * 0.5, sz));
	blit_image_mod(p_img, src, Rect2i(dest), p_mod);
}

void draw_tilemap_layer(Ref<Image> &p_img, const TileMapLayer *p_layer, const Color &p_mod) {
	const Ref<TileSet> tileset = p_layer->get_tile_set();
	if (tileset.is_null()) {
		return;
	}
	const TypedArray<Vector2i> cells = p_layer->get_used_cells();
	const Transform2D xf = p_layer->get_global_transform();
	const Vector2i tile_size = tileset->get_tile_size();
	for (int i = 0; i < cells.size(); i++) {
		const Vector2i cell = cells[i];
		const int src_id = p_layer->get_cell_source_id(cell);
		if (!tileset->has_source(src_id)) {
			continue;
		}
		const Ref<TileSetAtlasSource> atlas = tileset->get_source(src_id);
		if (atlas.is_null()) {
			continue;
		}
		const Vector2i atlas_coords = p_layer->get_cell_atlas_coords(cell);
		const Ref<Image> atlas_img = image_from_texture(atlas->get_texture());
		if (atlas_img.is_null() || atlas_img->is_empty()) {
			continue;
		}
		const Rect2i region = atlas->get_tile_texture_region(atlas_coords);
		if (!region.has_area()) {
			continue;
		}
		Ref<Image> tile = atlas_img->get_region(region);
		const Vector2 local = p_layer->map_to_local(cell) - Vector2(tile_size) * 0.5;
		const Rect2 dest = xf.xform(Rect2(local, tile_size));
		blit_image_mod(p_img, tile, Rect2i(dest), p_mod);
	}
}

void draw_range_chrome(Ref<Image> &p_img, const Range *p_range, const Color &p_mod) {
	const Control *ctrl = Object::cast_to<Control>(p_range);
	if (!ctrl) {
		return;
	}
	draw_control_style(p_img, ctrl, SNAME("background"));
	const Rect2i gr = control_rect(ctrl);
	const double span = p_range->get_max() - p_range->get_min();
	const double ratio = span > 0.0 ? CLAMP((p_range->get_value() - p_range->get_min()) / span, 0.0, 1.0) : 0.0;
	if (ratio > 0.0) {
		blit_clipped(p_img, Rect2i(gr.position.x, gr.position.y, MAX(int(gr.size.x * ratio), 1), gr.size.y), Color(0.3, 0.7, 0.35) * p_mod);
	}
}

void draw_text_control(Ref<Image> &p_img, const Control *p_ctrl, const String &p_text, const Color &p_mod) {
	draw_control_style(p_img, p_ctrl, SNAME("normal"));
	int font_size = 0;
	if (p_ctrl->has_theme_font_size_override("font_size") || p_ctrl->is_inside_tree()) {
		font_size = p_ctrl->get_theme_font_size(SNAME("font_size"));
	}
	if (font_size <= 0) {
		return;
	}
	Color color(1, 1, 1);
	if (p_ctrl->has_theme_color_override("font_color") || p_ctrl->is_inside_tree()) {
		color = p_ctrl->get_theme_color(SNAME("font_color"));
	}
	draw_text_at(p_img, p_text, control_rect(p_ctrl), color * p_mod, font_size, false, true);
}

void software_draw_node(Node *p_node, Ref<Image> &p_img, const Color &p_mod) {
	if (skip_software_node(p_node)) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			software_draw_node(p_node->get_child(i), p_img, p_mod);
		}
		return;
	}
	Color mod = p_mod;
	if (CanvasItem *ci = Object::cast_to<CanvasItem>(p_node)) {
		if (!ci->is_visible()) {
			return;
		}
		mod = canvas_modulate(ci, p_mod);
	}
	if (const ColorRect *rect = Object::cast_to<ColorRect>(p_node)) {
		blit_clipped(p_img, control_rect(rect), rect->get_color() * mod);
	}
	if (const Panel *panel = Object::cast_to<Panel>(p_node)) {
		draw_control_style(p_img, panel, SNAME("panel"));
	}
	if (const PanelContainer *pc = Object::cast_to<PanelContainer>(p_node)) {
		draw_control_style(p_img, pc, SNAME("panel"));
	}
	if (const NinePatchRect *np = Object::cast_to<NinePatchRect>(p_node)) {
		const Ref<Image> src = image_from_texture(np->get_texture());
		if (src.is_valid() && !src->is_empty()) {
			blit_image_mod(p_img, src, control_rect(np), mod);
		}
	}
	if (const TextureRect *tr = Object::cast_to<TextureRect>(p_node)) {
		draw_texture_rect(p_img, tr);
	}
	if (const TextureButton *tb = Object::cast_to<TextureButton>(p_node)) {
		const Ref<Image> src = image_from_texture(tb->get_texture_normal());
		if (src.is_valid() && !src->is_empty()) {
			blit_image_mod(p_img, src, control_rect(tb), mod);
		}
	}
	if (Sprite2D *sprite = Object::cast_to<Sprite2D>(p_node)) {
		draw_sprite_tex(p_img, sprite, sprite->get_texture(), sprite->get_rect());
	}
	if (AnimatedSprite2D *anim = Object::cast_to<AnimatedSprite2D>(p_node)) {
		const Ref<SpriteFrames> frames = anim->get_sprite_frames();
		if (frames.is_valid() && frames->has_animation(anim->get_animation())) {
			const Ref<Texture2D> tex = frames->get_frame_texture(anim->get_animation(), anim->get_frame());
			const Size2 sz = tex.is_valid() ? tex->get_size() : Size2();
			const Point2 off = anim->is_centered() ? -sz * 0.5 : Point2();
			draw_sprite_tex(p_img, anim, tex, Rect2(off + anim->get_offset(), sz));
		}
	}
	if (Polygon2D *poly = Object::cast_to<Polygon2D>(p_node)) {
		draw_polygon2d(p_img, poly, mod);
	}
	if (const Line2D *line = Object::cast_to<Line2D>(p_node)) {
		draw_line2d(p_img, line, mod);
	}
	if (MeshInstance2D *mesh = Object::cast_to<MeshInstance2D>(p_node)) {
		draw_mesh2d(p_img, mesh, mod);
	}
	if (const TileMapLayer *tml = Object::cast_to<TileMapLayer>(p_node)) {
		draw_tilemap_layer(p_img, tml, mod);
	}
	if (const TileMap *tm = Object::cast_to<TileMap>(p_node)) {
		const int layers = tm->get_layers_count();
		for (int l = 0; l < layers; l++) {
			if (TileMapLayer *layer = Object::cast_to<TileMapLayer>(tm->get_child(l))) {
				draw_tilemap_layer(p_img, layer, mod);
			}
		}
	}
	if (const Label *label = Object::cast_to<Label>(p_node)) {
		draw_label_text(p_img, label);
	}
	if (const RichTextLabel *rtl = Object::cast_to<RichTextLabel>(p_node)) {
		draw_rich_text(p_img, rtl);
	}
	if (const Button *button = Object::cast_to<Button>(p_node)) {
		if (!Object::cast_to<TextureButton>(p_node) && !Object::cast_to<LinkButton>(p_node)) {
			draw_button(p_img, button);
		}
	}
	if (const LinkButton *lb = Object::cast_to<LinkButton>(p_node)) {
		draw_text_control(p_img, lb, lb->get_text(), mod);
	}
	if (const ProgressBar *bar = Object::cast_to<ProgressBar>(p_node)) {
		draw_range_chrome(p_img, bar, mod);
	}
	if (const Slider *slider = Object::cast_to<Slider>(p_node)) {
		draw_range_chrome(p_img, slider, mod);
	}
	if (const LineEdit *le = Object::cast_to<LineEdit>(p_node)) {
		draw_text_control(p_img, le, le->get_text(), mod);
	}
	if (const TextEdit *te = Object::cast_to<TextEdit>(p_node)) {
		draw_text_control(p_img, te, te->get_text(), mod);
	}
	if (const VideoStreamPlayer *vsp = Object::cast_to<VideoStreamPlayer>(p_node)) {
		const Ref<Image> src = image_from_texture(vsp->get_video_texture());
		if (src.is_valid() && !src->is_empty()) {
			blit_image_mod(p_img, src, control_rect(vsp), mod);
		}
	}
	struct DrawOrder {
		bool operator()(const Node *a, const Node *b) const {
			const CanvasItem *ca = Object::cast_to<CanvasItem>(a);
			const CanvasItem *cb = Object::cast_to<CanvasItem>(b);
			const bool ba = ca && ca->is_draw_behind_parent_enabled();
			const bool bb = cb && cb->is_draw_behind_parent_enabled();
			if (ba != bb) {
				return ba;
			}
			const int za = ca ? ca->get_z_index() : 0;
			const int zb = cb ? cb->get_z_index() : 0;
			return za < zb;
		}
	};
	Vector<Node *> kids;
	for (int i = 0; i < p_node->get_child_count(); i++) {
		kids.push_back(p_node->get_child(i));
	}
	kids.sort_custom<DrawOrder>();
	for (int i = 0; i < kids.size(); i++) {
		software_draw_node(kids[i], p_img, mod);
	}
}

Ref<Image> software_raster(Node *p_root) {
	Ref<Image> img = Image::create_empty(BAKE_WIDTH, BAKE_HEIGHT, false, Image::FORMAT_RGBA8);
	img->fill(Color(0, 0, 0, 1));
	software_draw_node(p_root, img, Color(1, 1, 1, 1));
	return img;
}

void cleanup_frames(const String &p_dir, int p_count) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		return;
	}
	for (int i = 0; i < p_count; i++) {
		da->remove(p_dir.path_join(vformat("frame_%06d.png", i)));
	}
}

double probe_black_lead_sec(const String &p_ffmpeg, const String &p_pip) {
	List<String> args;
	args.push_back("-hide_banner");
	args.push_back("-t");
	args.push_back(rtos(InterDVDSettings::pip_blackdetect_sec()));
	args.push_back("-i");
	args.push_back(p_pip);
	args.push_back("-vf");
	args.push_back(vformat("blackdetect=d=0.04:pix_th=%g", InterDVDSettings::pip_blackdetect_pix_th()));
	args.push_back("-an");
	args.push_back("-f");
	args.push_back("null");
	args.push_back("-");
	String pipe;
	OS::get_singleton()->execute(p_ffmpeg, args, &pipe);
	double lead = 0.0;
	int from = 0;
	while (true) {
		const int at = pipe.find("black_end:", from);
		if (at < 0) {
			break;
		}
		const int start_at = pipe.find("black_start:", MAX(from, at - 96));
		const double st = start_at >= 0 ? pipe.substr(start_at + 12).to_float() : 0.0;
		const double en = pipe.substr(at + 10).to_float();
		if (st <= 0.08 && en > lead) {
			lead = en;
		}
		from = at + 10;
	}
	return CLAMP(lead, 0.0, 1.5);
}

void log_first_frame_slot(const String &p_ffmpeg, const String &p_mpg, int p_x, int p_y, int p_w, int p_h, const char *p_tag) {
	const String raw = p_mpg + ".ff0.rgb";
	List<String> args;
	args.push_back("-y");
	args.push_back("-i");
	args.push_back(p_mpg);
	args.push_back("-frames:v");
	args.push_back("1");
	args.push_back("-f");
	args.push_back("rawvideo");
	args.push_back("-pix_fmt");
	args.push_back("rgb24");
	args.push_back(raw);
	String pipe;
	OS::get_singleton()->execute(p_ffmpeg, args, &pipe);
	int avg_r = 0;
	int avg_g = 0;
	int avg_b = 0;
	int n = 0;
	int dark = 0;
	Ref<FileAccess> f = FileAccess::open(raw, FileAccess::READ);
	if (f.is_valid()) {
		const PackedByteArray buf = f->get_buffer(720 * 480 * 3);
		f.unref();
		Ref<DirAccess> rm = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (rm.is_valid()) {
			rm->remove(raw);
		}
		if (buf.size() >= 720 * 480 * 3) {
			const int x0 = CLAMP(p_x, 0, 719);
			const int y0 = CLAMP(p_y, 0, 479);
			const int x1 = CLAMP(p_x + p_w, x0 + 1, 720);
			const int y1 = CLAMP(p_y + p_h, y0 + 1, 480);
			int64_t sr = 0;
			int64_t sg = 0;
			int64_t sb = 0;
			for (int py = y0; py < y1; py += 2) {
				for (int px = x0; px < x1; px += 2) {
					const int i = (py * 720 + px) * 3;
					const int r = buf[i];
					const int g = buf[i + 1];
					const int b = buf[i + 2];
					sr += r;
					sg += g;
					sb += b;
					n++;
					if (r + g + b < 40) {
						dark++;
					}
				}
			}
			if (n > 0) {
				avg_r = int(sr / n);
				avg_g = int(sg / n);
				avg_b = int(sb / n);
			}
		}
	}

}

void push_pip_inputs(List<String> &p_args, const String &p_bg, const String &p_pip, double p_lead, bool p_bg_sequence = false) {
	p_args.push_back("-y");
	p_args.push_back("-framerate");
	p_args.push_back("30000/1001");
	if (p_bg_sequence) {
		p_args.push_back("-stream_loop");
		p_args.push_back("-1");
	} else {
		p_args.push_back("-loop");
		p_args.push_back("1");
	}
	p_args.push_back("-i");
	p_args.push_back(p_bg);
	if (p_lead > 0.001) {
		p_args.push_back("-ss");
		p_args.push_back(rtos(p_lead));
	}
	p_args.push_back("-i");
	p_args.push_back(p_pip);
}

String ffprobe_next_to(const String &p_ffmpeg) {
	if (p_ffmpeg.is_empty()) {
		return String();
	}
	const String dir = p_ffmpeg.get_base_dir();
	const String stem = p_ffmpeg.get_file().get_basename();
	String probe = stem.to_lower().ends_with("ffmpeg") ? stem.substr(0, stem.length() - 6) + "ffprobe" : String("ffprobe");
	if (p_ffmpeg.get_extension().to_lower() == "exe" && !probe.ends_with(".exe")) {
		probe += ".exe";
	}
	const String sibling = dir.path_join(probe);
	if (FileAccess::exists(sibling)) {
		return sibling;
	}
	const String bare = dir.path_join(OS::get_singleton()->get_name() == "Windows" ? "ffprobe.exe" : "ffprobe");
	return FileAccess::exists(bare) ? bare : String();
}

bool probe_has_audio(const String &p_ffmpeg, const String &p_path) {
	if (p_ffmpeg.is_empty() || p_path.is_empty() || !FileAccess::exists(p_path)) {
		return false;
	}
	const String ffprobe = ffprobe_next_to(p_ffmpeg);
	if (!ffprobe.is_empty()) {
		List<String> args;
		args.push_back("-v");
		args.push_back("error");
		args.push_back("-select_streams");
		args.push_back("a");
		args.push_back("-show_entries");
		args.push_back("stream=index");
		args.push_back("-of");
		args.push_back("csv=p=0");
		args.push_back(p_path);
		String pipe;
		int exitcode = 1;
		OS::get_singleton()->execute(ffprobe, args, &pipe, &exitcode, false);
		if (!pipe.strip_edges().is_empty()) {
			return true;
		}
	}
	List<String> args;
	args.push_back("-hide_banner");
	args.push_back("-i");
	args.push_back(p_path);
	String pipe;
	int exitcode = 0;

	OS::get_singleton()->execute(p_ffmpeg, args, &pipe, &exitcode, true);
	return pipe.findn("Audio:") >= 0;
}

void push_legal_ac3(List<String> &p_args) {
	p_args.push_back("-c:a");
	p_args.push_back("ac3");
	p_args.push_back("-ac");
	p_args.push_back(itos(InterDVDSettings::ac3_channels()));
	p_args.push_back("-ar");
	p_args.push_back("48000");
	p_args.push_back("-b:a");
	p_args.push_back(vformat("%dk", InterDVDSettings::ac3_bitrate_k()));
}

Error composite_pip_mpg(const String &p_ffmpeg, const String &p_bg_png, const String &p_pip, const Rect2i &p_slot, const String &p_audio, double p_pip_sec, double p_total, const String &p_mpg, String *r_error, bool p_bg_sequence = false, double p_trim_sec = 0.40) {
	const String bg_check = p_bg_sequence ? p_bg_png.replace("%06d", "000000") : p_bg_png;
	if (p_pip_sec <= 0.0 || p_total <= 0.0 || !FileAccess::exists(bg_check) || !FileAccess::exists(p_pip)) {
		if (r_error) {
			*r_error = "PiP bake is missing a background still or source video.";
		}
		return ERR_INVALID_PARAMETER;
	}
	const int x = p_slot.position.x & ~1;
	const int y = p_slot.position.y & ~1;
	const int w = MAX(p_slot.size.x & ~1, 2);
	const int h = MAX(p_slot.size.y & ~1, 2);
	const double lead = probe_black_lead_sec(p_ffmpeg, p_pip);
	const bool pip_audio = probe_has_audio(p_ffmpeg, p_pip);
	const bool menu_audio = !pip_audio && !p_audio.is_empty() && FileAccess::exists(p_audio);
	const double trim = MAX(p_trim_sec, 0.0);
	String filter = vformat("[1:v]setpts=PTS-STARTPTS,fps=30000/1001,scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1[pip];[0:v]fps=30000/1001,setpts=PTS-STARTPTS[bg];[bg][pip]overlay=%d:%d:eof_action=pass:repeatlast=0[ov];[ov]trim=start=%.3f,setpts=PTS-STARTPTS[v]", w, h, w, h, x, y, trim);
	if (pip_audio) {
		if (p_total > p_pip_sec + 0.01) {
			filter += vformat(";[1:a]atrim=start=%.3f,asetpts=PTS-STARTPTS,aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,apad,atrim=duration=%.3f[a]", trim, p_total);
		} else {
			filter += vformat(";[1:a]atrim=start=%.3f,asetpts=PTS-STARTPTS,aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo[a]", trim);
		}
	} else if (menu_audio) {
		filter += vformat(";[2:a]aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,apad,atrim=duration=%.3f,asetpts=PTS-STARTPTS[a]", p_total);
	}
	List<String> args;
	push_pip_inputs(args, p_bg_png, p_pip, lead, p_bg_sequence);
	if (menu_audio) {
		args.push_back("-i");
		args.push_back(p_audio);
	}
	args.push_back("-filter_complex");
	args.push_back(filter);
	args.push_back("-map");
	args.push_back("[v]");
	if (pip_audio || menu_audio) {
		args.push_back("-map");
		args.push_back("[a]");
	}
	args.push_back("-t");
	args.push_back(rtos(p_total));
	args.push_back("-target");
	args.push_back("ntsc-dvd");
	if (pip_audio || menu_audio) {
		push_legal_ac3(args);
	} else {
		args.push_back("-an");
	}
	args.push_back("-bf");
	args.push_back("0");
	args.push_back("-g");
	args.push_back(itos(InterDVDSettings::gop_size()));
	args.push_back(p_mpg);
	String pipe;
	int code = OS::get_singleton()->execute(p_ffmpeg, args, &pipe);
	if (code != 0 || !FileAccess::exists(p_mpg)) {
		if (r_error) {
			*r_error = vformat("ffmpeg PiP overlay failed (%d): %s", code, pipe);
		}
		return FAILED;
	}



	log_first_frame_slot(p_ffmpeg, p_mpg, x, y, w, h, "post-bake");


	return OK;
}

Error probe_or_fail(const String &p_mpg, double p_duration, String *r_error) {
	const double probed = InterDVDVobMux::estimate_duration_sec(p_mpg);
	if (probed + 0.001 < p_duration - DURATION_SLACK_SEC) {
		if (r_error) {
			*r_error = vformat("Scene bake was truncated: probed %.2fs but duration_sec is %.2fs.", probed, p_duration);
		}
		return FAILED;
	}
	return OK;
}

} //namespace

void InterDVDSceneBaker::_bind_methods() {
	ClassDB::bind_static_method("InterDVDSceneBaker", D_METHOD("raster_root", "root"), &InterDVDSceneBaker::raster_root);
	ClassDB::bind_static_method("InterDVDSceneBaker", D_METHOD("bake_cell", "cell", "ffmpeg", "auto_find_ffmpeg"), &InterDVDSceneBaker::bake_cell_bind, DEFVAL(String()), DEFVAL(true));
}

Ref<Image> InterDVDSceneBaker::raster_root(Node *p_root) {
	ERR_FAIL_NULL_V(p_root, Ref<Image>());
	return software_raster(p_root);
}

Error InterDVDSceneBaker::bake_cell_bind(const Ref<InterDVDCell> &p_cell, const String &p_ffmpeg, bool p_auto_find_ffmpeg) {
	String err;
	const Error code = bake_cell(p_cell, p_ffmpeg, p_auto_find_ffmpeg, &err);
	if (code != OK && !err.is_empty()) {
		ERR_PRINT(err);
	}
	return code;
}

Error InterDVDSceneBaker::bake_cell(const Ref<InterDVDCell> &p_cell, const String &p_ffmpeg, bool p_auto_find_ffmpeg, String *r_error) {
	if (p_cell.is_null() || p_cell->get_packed_scene().is_null()) {
		return OK;
	}
	if (dummy_renderer() && packed_has_node3d(p_cell->get_packed_scene())) {
		if (r_error) {
			*r_error = "3D scenes need a GPU bake / non-dummy export. Headless dummy cannot raster Node3D.";
		}
		return ERR_UNAVAILABLE;
	}

	double duration = p_cell->get_duration_sec();
	const double authored_sec = duration;
	const String pip_path = p_cell->get_pip_source_path();
	double pip_sec = 0.0;
	String ffmpeg = InterDVDVobMux::resolve_ffmpeg(p_ffmpeg, p_auto_find_ffmpeg);
	if (!pip_path.is_empty() && FileAccess::exists(pip_path)) {
		if (ffmpeg.is_empty()) {
			if (r_error) {
				*r_error = "Cannot bake InterDVD scene: ffmpeg was not found. Install ffmpeg or enable Auto-find ffmpeg. Discovered paths are not stored in the project.";
			}
			return ERR_UNCONFIGURED;
		}
		pip_sec = InterDVDVobMux::probe_media_sec(pip_path, ffmpeg);
		if (pip_sec > 0.0) {
			duration = pip_sec + MAX(p_cell->get_loop_pad_sec(), 0.0);
			p_cell->set_duration_sec(duration);
		}
	}


	if (duration <= 0.0) {
		if (r_error) {
			*r_error = "InterDVDCell.duration_sec must be greater than 0 when packed_scene is set.";
		}
		return ERR_INVALID_PARAMETER;
	}

	const String key = scene_key(p_cell);
	const String id = key.md5_text().substr(0, 16);
	const String cache_dir = cache_root();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(cache_dir);
	const String mpg = cache_dir.path_join(id + ".mpg");
	const String sidecar = cache_dir.path_join(id + ".meta");

	const String existing = p_cell->get_encoded_path();
	if (!existing.is_empty() && existing != mpg && FileAccess::exists(existing)) {
		return OK;
	}

	if (FileAccess::exists(mpg) && FileAccess::exists(sidecar)) {
		const String stored = FileAccess::get_file_as_string(sidecar).strip_edges();
		if (stored == key && probe_or_fail(mpg, duration, nullptr) == OK) {
			p_cell->set_encoded_path(mpg);
			if (!ffmpeg.is_empty()) {
				p_cell->set_include_audio(probe_has_audio(ffmpeg, mpg));
			}
			return OK;
		}
	}

	if (ffmpeg.is_empty()) {
		ffmpeg = InterDVDVobMux::resolve_ffmpeg(p_ffmpeg, p_auto_find_ffmpeg);
	}
	if (ffmpeg.is_empty()) {
		if (r_error) {
			*r_error = "Cannot bake InterDVD scene: ffmpeg was not found. Install ffmpeg or enable Auto-find ffmpeg. Discovered paths are not stored in the project.";
		}
		return ERR_UNCONFIGURED;
	}

	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || !tree->get_root()) {
		if (r_error) {
			*r_error = "Cannot bake InterDVD scene: no SceneTree is available.";
		}
		return ERR_UNAVAILABLE;
	}

	Node *inst = p_cell->get_packed_scene()->instantiate();
	if (!inst) {
		if (r_error) {
			*r_error = "InterDVD packed_scene failed to instantiate.";
		}
		return ERR_CANT_CREATE;
	}

	const bool has_3d = node_or_packed_has_3d(inst);
	if (dummy_renderer() && has_3d) {
		inst->queue_free();
		if (r_error) {
			*r_error = "3D scenes need a GPU bake / non-dummy export. Headless dummy cannot raster Node3D.";
		}
		return ERR_UNAVAILABLE;
	}

	SubViewport *vp = memnew(SubViewport);
	vp->set_size(Size2i(BAKE_WIDTH, BAKE_HEIGHT));
	vp->set_update_mode(SubViewport::UPDATE_ALWAYS);
	vp->set_disable_3d(!has_3d);
	vp->set_transparent_background(false);
	tree->get_root()->add_child(vp);
	vp->add_child(inst);
	const NodePath cam_path = p_cell->get_bake_camera_path();
	if (!cam_path.is_empty()) {
		if (Camera2D *cam2 = Object::cast_to<Camera2D>(inst->get_node_or_null(cam_path))) {
			cam2->make_current();
#ifndef _3D_DISABLED
		} else if (Camera3D *cam3 = Object::cast_to<Camera3D>(inst->get_node_or_null(cam_path))) {
			cam3->make_current();
#endif
		}
#ifndef _3D_DISABLED
	} else if (has_3d) {
		Camera3D *current = vp->get_camera_3d();
		if (!current) {
			vp->remove_child(inst);
			inst->queue_free();
			tree->get_root()->remove_child(vp);
			vp->queue_free();
			if (r_error) {
				*r_error = "3D bake needs bake_camera_path or a current Camera3D in the packed scene.";
			}
			return ERR_UNCONFIGURED;
		}
#endif
	}
	if (Control *ctrl = Object::cast_to<Control>(inst)) {
		if (ctrl->get_size().x < 2 || ctrl->get_size().y < 2) {
			ctrl->set_size(Size2(BAKE_WIDTH, BAKE_HEIGHT));
		}
	}

	play_animations(inst);
	String menu_audio = p_cell->get_audio_path();
	if (!menu_audio.is_empty() && !FileAccess::exists(menu_audio) && ProjectSettings::get_singleton()) {
		menu_audio = ProjectSettings::get_singleton()->globalize_path(menu_audio);
	}
	if (menu_audio.is_empty() || !FileAccess::exists(menu_audio)) {
		menu_audio = String();
		const String wav_tmp = cache_dir.path_join(id + ".wav");
		if (p_cell->get_include_audio()) {
			collect_audio_path(inst, menu_audio, wav_tmp);
		}
	}
	const bool use_pip = pip_sec > 0.0;
	const bool motion = scene_has_motion(inst);
	const bool pip_has_audio = use_pip && probe_has_audio(ffmpeg, pip_path);
	String mux_audio_path;
	const char *audio_src = "none";
	if (pip_has_audio) {
		audio_src = "pip";
	} else if (p_cell->get_include_audio() && !menu_audio.is_empty() && FileAccess::exists(menu_audio)) {
		mux_audio_path = menu_audio;
		audio_src = "menu";
	}
	double hold_sec = 0.0;
	if (!use_pip) {
		hold_sec = p_cell->get_bake_hold_sec() > 0.0 ? p_cell->get_bake_hold_sec() : bake_hold_sec(inst);
	}
	const double chrome_sec = (use_pip && motion) ? MIN(duration, authored_sec > 0.05 ? authored_sec : 4.0) : duration;
	const int frames = (hold_sec > 0.05) ? MAX(int(Math::ceil(duration / hold_sec)), 1) : ((use_pip && !motion) ? 1 : MAX(int(Math::ceil(chrome_sec * BAKE_FPS)), 1));
	const bool pip_sequence = use_pip && motion;
	const double dt = (hold_sec > 0.05) ? hold_sec : (1.0 / BAKE_FPS);
	const int warmup = InterDVDSettings::bake_warmup_frames();
	Error capture_err = OK;
	Rect2i pip_slot;
	bool have_pip_slot = false;
	if (p_cell->get_pip_rect().size.x > 0 && p_cell->get_pip_rect().size.y > 0) {
		pip_slot = Rect2i(p_cell->get_pip_rect());
		have_pip_slot = true;
	}
	const NodePath slot_path = p_cell->get_pip_slot_path();
	for (int i = 0; i < frames + warmup; i++) {
		const double bake_t = (hold_sec > 0.05) ? (MAX(i - warmup, 0) * hold_sec + hold_sec * 0.5) : (MAX(i - warmup, 0) * dt);
		apply_bake_time(inst, bake_t);
		tree->process(dt);
		tree->physics_process(dt);
		if (RenderingServer::get_singleton()) {
			RenderingServer::get_singleton()->draw(false, dt);
		}
		if (i < warmup) {
			continue;
		}
		if (!slot_path.is_empty()) {
			if (Control *slot = Object::cast_to<Control>(inst->get_node_or_null(slot_path))) {
				pip_slot = Rect2i(slot->get_global_rect());
				have_pip_slot = true;
			} else if (CanvasItem *ci = Object::cast_to<CanvasItem>(inst->get_node_or_null(slot_path))) {
				if (ci->has_method("get_rect")) {
					pip_slot = Rect2i(ci->get_global_transform().xform(Rect2(ci->call("get_rect"))));
					have_pip_slot = true;
				}
			}
		}
		Ref<Image> img;
		if (!dummy_renderer()) {
			Ref<ViewportTexture> tex = vp->get_texture();
			img = tex.is_valid() ? tex->get_image() : Ref<Image>();
		}
		if (img.is_null() || img->is_empty()) {
			img = software_raster(inst);
		}
		if (img->get_size() != Size2i(BAKE_WIDTH, BAKE_HEIGHT)) {
			img->resize(BAKE_WIDTH, BAKE_HEIGHT, Image::INTERPOLATE_BILINEAR);
		}
		if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGB8);
		}
		const Error png_err = img->save_png(cache_dir.path_join((use_pip && !pip_sequence) ? "pip_bg.png" : vformat("frame_%06d.png", i - warmup)));
		if (png_err != OK) {
			capture_err = png_err;
			if (r_error) {
				*r_error = "Scene bake could not write a PNG frame.";
			}
			break;
		}
	}

	vp->remove_child(inst);
	inst->queue_free();
	tree->get_root()->remove_child(vp);
	vp->queue_free();

	if (capture_err != OK) {
		cleanup_frames(cache_dir, frames);
		return capture_err;
	}

	if (use_pip && have_pip_slot) {
		const String bg = pip_sequence ? cache_dir.path_join("frame_%06d.png") : cache_dir.path_join("pip_bg.png");
		const Error pip_err = composite_pip_mpg(ffmpeg, bg, pip_path, pip_slot, mux_audio_path, pip_sec, duration, mpg, r_error, pip_sequence, p_cell->get_pip_lead_sec());
		if (pip_sequence) {
			cleanup_frames(cache_dir, frames);
		}
		if (pip_err != OK) {
			return pip_err;
		}
	} else {
		List<String> args;
		args.push_back("-y");
		args.push_back("-framerate");
		args.push_back(hold_sec > 0.05 ? rtos(1.0 / hold_sec) : String("30"));
		args.push_back("-i");
		args.push_back(cache_dir.path_join("frame_%06d.png"));
		const bool mux_audio = !mux_audio_path.is_empty() && FileAccess::exists(mux_audio_path);
		if (mux_audio) {
			args.push_back("-stream_loop");
			args.push_back("-1");
			args.push_back("-i");
			args.push_back(mux_audio_path);
		}
		args.push_back("-t");
		args.push_back(rtos(duration));
		if (mux_audio) {
			push_legal_ac3(args);
		} else {
			args.push_back("-an");
		}
		args.push_back("-target");
		args.push_back("ntsc-dvd");
		args.push_back("-g");
		args.push_back(itos(InterDVDSettings::gop_size()));
		args.push_back(mpg);

		{
		}

		String pipe;
		const int code = OS::get_singleton()->execute(ffmpeg, args, &pipe);
		cleanup_frames(cache_dir, frames);
		if (code != 0 || !FileAccess::exists(mpg)) {
			if (r_error) {
				*r_error = vformat("ffmpeg scene bake failed (%d): %s", code, pipe);
			}
			return FAILED;
		}
	}

	const Error probe_err = probe_or_fail(mpg, duration, r_error);
	if (probe_err != OK) {
		return probe_err;
	}

	Ref<FileAccess> meta = FileAccess::open(sidecar, FileAccess::WRITE);
	if (meta.is_valid()) {
		meta->store_string(key);
	}
	p_cell->set_encoded_path(mpg);
	p_cell->set_include_audio(String(audio_src) != "none");
	return OK;
}

#endif
