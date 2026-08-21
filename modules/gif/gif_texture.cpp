/**************************************************************************/
/*  gif_texture.cpp                                                       */
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

#include "gif_texture.h"

#include "gif_decode.h"
#include "gif_encode.h"

#include "core/io/file_access.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"
#include "servers/rendering_server.h"

Mutex GIFTexture::_active_mutex;
HashSet<GIFTexture *> GIFTexture::_active_textures;

static int _gif_meta_int(const PackedInt32Array &p_arr, int p_index, int p_def) {
	if (p_arr.is_empty()) {
		return p_def;
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)];
}

static Vector2 _gif_meta_vec2(const PackedVector2Array &p_arr, int p_index) {
	if (p_arr.is_empty()) {
		return Vector2();
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)];
}

static bool _gif_meta_bool(const PackedByteArray &p_arr, int p_index) {
	if (p_arr.is_empty()) {
		return false;
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)] != 0;
}

static Ref<Texture2D> _gif_make_texture(const Ref<Image> &p_image) {
	if (p_image.is_null() || p_image->is_empty() || RenderingServer::get_singleton() == nullptr) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(p_image);
}

void GIFTexture::_clear_frames() {
	source_frames.clear();
	baked_frames.clear();
	baked_textures.clear();
	source_textures.clear();
	delays.clear();
	disposals.clear();
	positions.clear();
	transparent_colors.clear();
	has_transparency.clear();
}

void GIFTexture::_compact_trailing_duplicates() {
	auto compact_int = [](PackedInt32Array &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	auto compact_byte = [](PackedByteArray &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	auto compact_vec = [](PackedVector2Array &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	compact_int(delays);
	compact_int(disposals);
	compact_int(transparent_colors);
	compact_byte(has_transparency);
	compact_vec(positions);
}

void GIFTexture::_rebuild_textures() {
	source_textures.clear();
	baked_textures.clear();
}

bool GIFTexture::_source_needs_composition() const {
	for (int i = 0; i < source_frames.size(); i++) {
		if (_gif_meta_bool(has_transparency, i) || _gif_meta_vec2(positions, i) != Vector2i()) {
			return true;
		}
		const Ref<Image> img = source_frames[i];
		if (img.is_null() || img->get_size() != canvas_size) {
			return true;
		}
	}
	return false;
}

void GIFTexture::_ensure_baked() {
	if (source_frames.is_empty()) {
		return;
	}
	if (baked_frames.size() == source_frames.size()) {
		return;
	}
	if (display_mode == DISPLAY_BAKED || _source_needs_composition()) {
		_bake_frames_locked();
	}
}

void GIFTexture::_reset_playback() {
	current_frame = 0;
	time = 0;
	loops_done = 0;
	prev_ticks = 0;
	finished = false;
}

void GIFTexture::_emit_data_changed() {
	_reset_playback();
	if (Thread::is_main_thread()) {
		_update_proxy();
	}
	_update_activity();
	emit_changed();
	emit_signal(SNAME("frames_changed"));
}

void GIFTexture::_apply_decoded(const GIFDecoded &p_decoded) {
	_clear_frames();
	canvas_size = p_decoded.canvas_size;
	netscape_loop_count = p_decoded.loop_count;
	bool needs_bake = false;
	for (int i = 0; i < p_decoded.frames.size(); i++) {
		const GIFDecodedFrame &f = p_decoded.frames[i];
		source_frames.push_back(f.image);
		delays.push_back(f.delay_cs);
		disposals.push_back(f.disposal);
		positions.push_back(f.position);
		transparent_colors.push_back(f.transparent_color);
		has_transparency.push_back(f.has_transparency ? 1 : 0);
		if (f.has_transparency || f.position != Vector2i() || f.image.is_null() || f.image->get_size() != canvas_size) {
			needs_bake = true;
		}
	}
	_compact_trailing_duplicates();
	if (needs_bake) {
		display_mode = DISPLAY_BAKED;
		_bake_frames_locked();
	} else {
		display_mode = DISPLAY_SOURCE;
		_rebuild_textures();
	}
}

void GIFTexture::apply_decoded(const GIFDecoded &p_decoded) {
	{
		RWLockWrite w(rw_lock);
		_apply_decoded(p_decoded);
	}
	_emit_data_changed();
}

Error GIFTexture::load_from_path(const String &p_path) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK || f.is_null(), err == OK ? ERR_CANT_OPEN : err, vformat("Cannot open GIF: '%s'.", p_path));
	const uint64_t len = f->get_length();
	ERR_FAIL_COND_V(len == 0 || len > 0x7FFFFFFF, ERR_FILE_CORRUPT);
	PackedByteArray buf;
	buf.resize(int(len));
	f->get_buffer(buf.ptrw(), len);
	return load_from_buffer(buf);
}

Error GIFTexture::load_from_buffer(const PackedByteArray &p_buffer) {
	ERR_FAIL_COND_V(p_buffer.is_empty(), ERR_INVALID_PARAMETER);
	GIFDecoded decoded;
	const Error err = gif_decode_buffer(p_buffer.ptr(), p_buffer.size(), decoded, false);
	if (err != OK) {
		return err;
	}
	apply_decoded(decoded);
	return OK;
}

Error GIFTexture::save_to_path(const String &p_path) const {
	return gif_encode_write_file(p_path, save_to_buffer());
}

PackedByteArray GIFTexture::save_to_buffer() const {
	Vector<GIFEncodeFrame> frames;
	const_cast<GIFTexture *>(this)->_ensure_baked();
	const bool use_baked = display_mode == DISPLAY_BAKED && !baked_frames.is_empty();
	const int count = get_frame_count();
	frames.resize(count);
	for (int i = 0; i < count; i++) {
		Ref<Image> img = use_baked ? get_baked_image(i) : get_source_image(i);
		if (img.is_null()) {
			return PackedByteArray();
		}
		if (!use_baked && (img->get_size() != canvas_size || get_frame_position(i) != Vector2i())) {
			Ref<Image> canvas = Image::create_empty(canvas_size.x, canvas_size.y, false, Image::FORMAT_RGBA8);
			canvas->fill(Color(0, 0, 0, 0));
			canvas->blit_rect(img, Rect2i(Vector2i(), img->get_size()), get_frame_position(i));
			img = canvas;
		}
		frames.write[i].image = img;
		frames.write[i].delay_cs = get_frame_delay(i);
	}
	PackedByteArray out;
	if (gif_encode_frames(frames, netscape_loop_count, dither, true, out) != OK) {
		return PackedByteArray();
	}
	return out;
}

Error GIFTexture::_bake_frames_locked() {
	baked_frames.clear();
	baked_textures.clear();
	const int count = source_frames.size();
	if (count == 0 || canvas_size.x <= 0 || canvas_size.y <= 0) {
		return ERR_UNCONFIGURED;
	}

	Ref<Image> canvas = Image::create_empty(canvas_size.x, canvas_size.y, false, Image::FORMAT_RGBA8);
	canvas->fill(Color(0, 0, 0, 0));
	Ref<Image> previous = canvas->duplicate();

	for (int i = 0; i < count; i++) {
		const Disposal disposal = Disposal(CLAMP(_gif_meta_int(disposals, i, 0), 0, 3));
		if (disposal == DISPOSAL_RESTORE_PREVIOUS) {
			previous = canvas->duplicate();
		}

		const Ref<Image> src = source_frames[i];
		const Vector2i pos = Vector2i(_gif_meta_vec2(positions, i));
		if (src.is_valid()) {
			Ref<Image> src_rgba = src->duplicate();
			if (src_rgba->is_compressed()) {
				src_rgba->decompress();
			}
			if (src_rgba->get_format() != Image::FORMAT_RGBA8) {
				src_rgba->convert(Image::FORMAT_RGBA8);
			}
			const Rect2i src_rect(Vector2i(), src_rgba->get_size());
			if (_gif_meta_bool(has_transparency, i)) {
				canvas->blend_rect(src_rgba, src_rect, pos);
			} else {
				canvas->blit_rect(src_rgba, src_rect, pos);
			}
		}

		Ref<Image> baked = canvas->duplicate();
		if (bake_compress) {
			baked->compress(Image::COMPRESS_S3TC);
		}
		baked_frames.push_back(baked);

		if (disposal == DISPOSAL_RESTORE_BACKGROUND) {
			const Vector2i fsize = src.is_valid() ? src->get_size() : canvas_size;
			canvas->fill_rect(Rect2i(pos, fsize), Color(0, 0, 0, 0));
		} else if (disposal == DISPOSAL_RESTORE_PREVIOUS) {
			canvas = previous->duplicate();
		}
	}

	_rebuild_textures();
	return OK;
}

Error GIFTexture::bake_frames() {
	Error err = ERR_UNCONFIGURED;
	bool did_bake = false;
	{
		RWLockWrite w(rw_lock);
		err = _bake_frames_locked();
		did_bake = err == OK;
	}
	if (did_bake) {
		emit_signal(SNAME("rebaked"));
	}
	return err;
}

int GIFTexture::get_frame_count() const {
	RWLockRead r(rw_lock);
	return source_frames.size();
}

int GIFTexture::_wrap_frame_locked(int p_frame) const {
	const int count = source_frames.size();
	if (count <= 0) {
		return 0;
	}
	int f = p_frame % count;
	if (f < 0) {
		f += count;
	}
	return f;
}

int GIFTexture::wrap_frame(int p_frame) const {
	RWLockRead r(rw_lock);
	return _wrap_frame_locked(p_frame);
}

Ref<Texture2D> GIFTexture::_get_active_texture_locked(int p_frame) {
	_ensure_baked();
	const int i = _wrap_frame_locked(p_frame);
	const bool use_baked = (display_mode == DISPLAY_BAKED || _source_needs_composition()) && !baked_frames.is_empty();
	if (use_baked) {
		if (i < 0 || i >= baked_frames.size()) {
			return Ref<Texture2D>();
		}
		if (baked_textures.size() != baked_frames.size()) {
			baked_textures.resize(baked_frames.size());
		}
		if (baked_textures[i].is_null()) {
			baked_textures.write[i] = _gif_make_texture(baked_frames[i]);
		}
		return baked_textures[i];
	}
	if (i < 0 || i >= source_frames.size()) {
		return Ref<Texture2D>();
	}
	if (source_textures.size() != source_frames.size()) {
		source_textures.resize(source_frames.size());
	}
	if (source_textures[i].is_null()) {
		source_textures.write[i] = _gif_make_texture(source_frames[i]);
	}
	return source_textures[i];
}

Ref<Texture2D> GIFTexture::get_active_texture(int p_frame) {
	RWLockWrite w(rw_lock);
	return _get_active_texture_locked(p_frame);
}

Ref<Image> GIFTexture::get_source_image(int p_frame) const {
	RWLockRead r(rw_lock);
	ERR_FAIL_INDEX_V(p_frame, source_frames.size(), Ref<Image>());
	return source_frames[p_frame];
}

Ref<Image> GIFTexture::get_baked_image(int p_frame) const {
	RWLockRead r(rw_lock);
	if (p_frame < 0 || p_frame >= baked_frames.size()) {
		return Ref<Image>();
	}
	return baked_frames[p_frame];
}

int GIFTexture::get_frame_delay(int p_frame) const {
	RWLockRead r(rw_lock);
	return MAX(1, _gif_meta_int(delays, p_frame, 10));
}

double GIFTexture::get_frame_delay_sec(int p_frame) const {
	RWLockRead r(rw_lock);
	return MAX(0.01, MAX(1, _gif_meta_int(delays, p_frame, 10)) / 100.0);
}

GIFTexture::Disposal GIFTexture::get_frame_disposal(int p_frame) const {
	RWLockRead r(rw_lock);
	return Disposal(CLAMP(_gif_meta_int(disposals, p_frame, 0), 0, 3));
}

Vector2i GIFTexture::get_frame_position(int p_frame) const {
	RWLockRead r(rw_lock);
	return Vector2i(_gif_meta_vec2(positions, p_frame));
}

int GIFTexture::get_frame_transparent_color(int p_frame) const {
	RWLockRead r(rw_lock);
	return _gif_meta_int(transparent_colors, p_frame, -1);
}

bool GIFTexture::get_frame_has_transparency(int p_frame) const {
	RWLockRead r(rw_lock);
	return _gif_meta_bool(has_transparency, p_frame);
}

void GIFTexture::add_source_frame(const Ref<Image> &p_image, int p_delay_cs, Disposal p_disposal, const Vector2i &p_position) {
	ERR_FAIL_COND(p_image.is_null());
	{
		RWLockWrite w(rw_lock);
		if (canvas_size == Vector2i()) {
			canvas_size = p_image->get_size();
		}
		source_frames.push_back(p_image);
		delays.push_back(MAX(1, p_delay_cs));
		disposals.push_back(int(p_disposal));
		positions.push_back(p_position);
		transparent_colors.push_back(-1);
		has_transparency.push_back(p_image->detect_alpha() != Image::ALPHA_NONE ? 1 : 0);
		_rebuild_textures();
	}
	_emit_data_changed();
}

Ref<SpriteFrames> GIFTexture::to_sprite_frames(const StringName &p_animation_name) {
	RWLockWrite w(rw_lock);
	_ensure_baked();
	Ref<SpriteFrames> frames;
	frames.instantiate();
	if (!frames->has_animation(p_animation_name)) {
		frames->add_animation(p_animation_name);
	} else {
		frames->clear(p_animation_name);
	}
	frames->set_animation_loop(p_animation_name, netscape_loop_count == 0);
	frames->set_animation_speed(p_animation_name, 1.0);
	const int count = source_frames.size();
	for (int i = 0; i < count; i++) {
		Ref<Texture2D> tex = _get_active_texture_locked(i);
		if (tex.is_null()) {
			Ref<Image> img = display_mode == DISPLAY_BAKED ? baked_frames[i] : source_frames[i];
			tex = _gif_make_texture(img);
		}
		const double delay_sec = MAX(0.01, MAX(1, _gif_meta_int(delays, i, 10)) / 100.0);
		frames->add_frame(p_animation_name, tex, float(delay_sec));
	}
	return frames;
}

Ref<GIFTexture> GIFTexture::from_sprite_frames(const Ref<SpriteFrames> &p_frames, const StringName &p_animation_name) {
	Ref<GIFTexture> tex;
	tex.instantiate();
	ERR_FAIL_COND_V(p_frames.is_null() || !p_frames->has_animation(p_animation_name), tex);
	const int count = p_frames->get_frame_count(p_animation_name);
	tex->set_netscape_loop_count(p_frames->get_animation_loop(p_animation_name) ? 0 : 1);
	for (int i = 0; i < count; i++) {
		Ref<Texture2D> frame_tex = p_frames->get_frame_texture(p_animation_name, i);
		ERR_CONTINUE(frame_tex.is_null());
		Ref<Image> img = frame_tex->get_image();
		ERR_CONTINUE(img.is_null());
		const int delay_cs = MAX(1, int(Math::round(p_frames->get_frame_duration(p_animation_name, i) * 100.0)));
		tex->add_source_frame(img, delay_cs);
	}
	return tex;
}

TypedArray<Image> GIFTexture::_get_source_frames() const {
	RWLockRead r(rw_lock);
	TypedArray<Image> arr;
	arr.resize(source_frames.size());
	for (int i = 0; i < source_frames.size(); i++) {
		arr[i] = source_frames[i];
	}
	return arr;
}

void GIFTexture::_set_source_frames(const TypedArray<Image> &p_frames) {
	{
		RWLockWrite w(rw_lock);
		source_frames.clear();
		for (int i = 0; i < p_frames.size(); i++) {
			source_frames.push_back(p_frames[i]);
		}
		if (bake_storage == BAKE_GENERATE_ON_LOAD) {
			baked_frames.clear();
		}
		_rebuild_textures();
	}
	_emit_data_changed();
}

TypedArray<Image> GIFTexture::_get_baked_frames() const {
	RWLockRead r(rw_lock);
	TypedArray<Image> arr;
	if (bake_storage != BAKE_STORE) {
		return arr;
	}
	arr.resize(baked_frames.size());
	for (int i = 0; i < baked_frames.size(); i++) {
		arr[i] = baked_frames[i];
	}
	return arr;
}

void GIFTexture::_set_baked_frames(const TypedArray<Image> &p_frames) {
	{
		RWLockWrite w(rw_lock);
		baked_frames.clear();
		for (int i = 0; i < p_frames.size(); i++) {
			baked_frames.push_back(p_frames[i]);
		}
		_rebuild_textures();
	}
	_emit_data_changed();
}

void GIFTexture::_update_proxy() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());

	if (!proxy.is_valid()) {
		return;
	}
	bool just_finished = false;
	{
		RWLockWrite w(rw_lock);
		if (source_frames.is_empty()) {
			return;
		}

		float delta = 0.0f;
		if (prev_ticks == 0) {
			prev_ticks = OS::get_singleton()->get_ticks_usec();
		} else {
			const uint64_t ticks = OS::get_singleton()->get_ticks_usec();
			delta = float(double(ticks - prev_ticks) / 1000000.0);
			prev_ticks = ticks;
		}

		const int count = source_frames.size();
		const bool honor_netscape = netscape_loop_count != 0;
		const int max_loops = honor_netscape ? netscape_loop_count : 0;
		const bool can_loop = loop && (!honor_netscape || loops_done < max_loops || max_loops == 0);

		if (play && !finished && speed_scale != 0.0f) {
			time += delta * Math::abs(speed_scale);
			int guard = count;
			while (guard-- > 0) {
				const float limit = float(MAX(0.01, MAX(1, _gif_meta_int(delays, current_frame, 10)) / 100.0));
				if (time < limit) {
					break;
				}
				time -= limit;
				if (speed_scale > 0.0f) {
					current_frame++;
					if (current_frame >= count) {
						if (can_loop) {
							current_frame = 0;
							loops_done++;
						} else {
							current_frame = count - 1;
							time = 0;
							finished = true;
							play = false;
							just_finished = true;
							break;
						}
					}
				} else {
					current_frame--;
					if (current_frame < 0) {
						if (can_loop) {
							current_frame = count - 1;
							loops_done++;
						} else {
							current_frame = 0;
							time = 0;
							finished = true;
							play = false;
							just_finished = true;
							break;
						}
					}
				}
			}
		}

		Ref<Texture2D> tex = _get_active_texture_locked(current_frame);
		if (tex.is_valid()) {
			RenderingServer::get_singleton()->texture_proxy_update(proxy, tex->get_rid());
		}
	}
	if (just_finished) {
		_update_activity();
		emit_changed();
	}
}

void GIFTexture::_finish_non_thread_safe_setup() {
	_update_proxy();
	_update_activity();
}

bool GIFTexture::_is_playing_state() const {
	RWLockRead r(rw_lock);
	return play && !finished && speed_scale != 0.0f && !source_frames.is_empty();
}

void GIFTexture::_update_activity() {
	const bool active = _is_playing_state();
	MutexLock m(_active_mutex);
	if (active) {
		if (!_active_textures.has(this)) {
			_active_textures.insert(this);
		}
		_connect_heartbeat();
	} else if (_active_textures.has(this)) {
		{
			RWLockWrite w(rw_lock);
			prev_ticks = 0;
			time = 0;
		}
		_active_textures.erase(this);
		if (_active_textures.is_empty()) {
			_disconnect_heartbeat();
		}
	}
}

void GIFTexture::_connect_heartbeat() {
	if (!Thread::is_main_thread()) {
		return;
	}
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!st) {
		return;
	}
	Callable cb = callable_mp_static(&GIFTexture::_process_active);
	if (!st->is_connected("process_frame", cb)) {
		st->connect("process_frame", cb);
	}
}

void GIFTexture::_disconnect_heartbeat() {
	if (!Thread::is_main_thread()) {
		return;
	}
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!st) {
		return;
	}
	Callable cb = callable_mp_static(&GIFTexture::_process_active);
	if (st->is_connected("process_frame", cb)) {
		st->disconnect("process_frame", cb);
	}
}

void GIFTexture::_process_active() {
	MutexLock m(_active_mutex);
	LocalVector<GIFTexture *> retired;
	for (GIFTexture *tex : _active_textures) {
		tex->_update_proxy();
		if (!tex->_is_playing_state()) {
			retired.push_back(tex);
		}
	}
	for (GIFTexture *tex : retired) {
		_active_textures.erase(tex);
	}
	if (_active_textures.is_empty()) {
		_disconnect_heartbeat();
	}
}

void GIFTexture::set_play(bool p_play) {
	{
		RWLockWrite w(rw_lock);
		if (p_play && !play) {
			finished = false;
			time = 0;
			prev_ticks = 0;
			if (!loop) {
				current_frame = 0;
				loops_done = 0;
			}
		}
		play = p_play;
	}
	if (Thread::is_main_thread()) {
		_update_proxy();
	}
	_update_activity();
	emit_changed();
}

bool GIFTexture::get_play() const {
	RWLockRead r(rw_lock);
	return play;
}

void GIFTexture::set_loop(bool p_loop) {
	{
		RWLockWrite w(rw_lock);
		loop = p_loop;
		if (loop) {
			finished = false;
		}
	}
	_update_activity();
}

bool GIFTexture::get_loop() const {
	RWLockRead r(rw_lock);
	return loop;
}

void GIFTexture::set_speed_scale(float p_scale) {
	ERR_FAIL_COND(p_scale < -1000.0f || p_scale >= 1000.0f);
	{
		RWLockWrite w(rw_lock);
		speed_scale = p_scale;
	}
	_update_activity();
}

float GIFTexture::get_speed_scale() const {
	RWLockRead r(rw_lock);
	return speed_scale;
}

void GIFTexture::set_current_frame(int p_frame) {
	{
		RWLockWrite w(rw_lock);
		current_frame = _wrap_frame_locked(p_frame);
		time = 0;
		finished = false;
	}
	if (Thread::is_main_thread()) {
		_update_proxy();
	}
	_update_activity();
	emit_changed();
}

int GIFTexture::get_current_frame() const {
	RWLockRead r(rw_lock);
	return current_frame;
}

void GIFTexture::set_canvas_size(const Vector2i &p_size) {
	{
		RWLockWrite w(rw_lock);
		canvas_size = p_size;
		_rebuild_textures();
	}
	_emit_data_changed();
}

Vector2i GIFTexture::get_canvas_size() const {
	RWLockRead r(rw_lock);
	return canvas_size;
}

void GIFTexture::set_netscape_loop_count(int p_count) {
	{
		RWLockWrite w(rw_lock);
		netscape_loop_count = MAX(0, p_count);
	}
}

int GIFTexture::get_netscape_loop_count() const {
	RWLockRead r(rw_lock);
	return netscape_loop_count;
}

void GIFTexture::set_display_mode(DisplayMode p_mode) {
	{
		RWLockWrite w(rw_lock);
		display_mode = p_mode;
		if (display_mode == DISPLAY_BAKED) {
			_ensure_baked();
		}
		_rebuild_textures();
	}
	_emit_data_changed();
}

GIFTexture::DisplayMode GIFTexture::get_display_mode() const {
	RWLockRead r(rw_lock);
	return display_mode;
}

void GIFTexture::set_bake_storage(BakeStorage p_storage) {
	RWLockWrite w(rw_lock);
	bake_storage = p_storage;
}

GIFTexture::BakeStorage GIFTexture::get_bake_storage() const {
	RWLockRead r(rw_lock);
	return bake_storage;
}

void GIFTexture::set_bake_compress(bool p_compress) {
	RWLockWrite w(rw_lock);
	bake_compress = p_compress;
}

bool GIFTexture::get_bake_compress() const {
	RWLockRead r(rw_lock);
	return bake_compress;
}

void GIFTexture::set_dither(bool p_dither) {
	RWLockWrite w(rw_lock);
	dither = p_dither;
}

bool GIFTexture::get_dither() const {
	RWLockRead r(rw_lock);
	return dither;
}

int GIFTexture::get_width() const {
	RWLockRead r(rw_lock);
	return canvas_size.x > 0 ? canvas_size.x : 1;
}

int GIFTexture::get_height() const {
	RWLockRead r(rw_lock);
	return canvas_size.y > 0 ? canvas_size.y : 1;
}

RID GIFTexture::get_rid() const {
	return proxy;
}

bool GIFTexture::has_alpha() const {
	return true;
}

Ref<Image> GIFTexture::get_image() const {
	int frame = 0;
	{
		RWLockRead r(rw_lock);
		frame = current_frame;
	}
	Ref<Texture2D> tex = const_cast<GIFTexture *>(this)->get_active_texture(frame);
	if (tex.is_valid()) {
		return tex->get_image();
	}
	return get_source_image(frame);
}

bool GIFTexture::is_pixel_opaque(int p_x, int p_y) const {
	Ref<Image> img = get_image();
	if (img.is_null() || img->is_empty()) {
		return true;
	}
	if (p_x < 0 || p_y < 0 || p_x >= img->get_width() || p_y >= img->get_height()) {
		return false;
	}
	return img->get_pixel(p_x, p_y).a > 0.001f;
}

void GIFTexture::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "delays", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "disposals", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "positions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "transparent_colors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "has_transparency", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
}

bool GIFTexture::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("delays")) {
		RWLockWrite w(rw_lock);
		delays = p_value;
		return true;
	}
	if (p_name == SNAME("disposals")) {
		RWLockWrite w(rw_lock);
		disposals = p_value;
		return true;
	}
	if (p_name == SNAME("positions")) {
		RWLockWrite w(rw_lock);
		positions = p_value;
		return true;
	}
	if (p_name == SNAME("transparent_colors")) {
		RWLockWrite w(rw_lock);
		transparent_colors = p_value;
		return true;
	}
	if (p_name == SNAME("has_transparency")) {
		RWLockWrite w(rw_lock);
		has_transparency = p_value;
		return true;
	}
	return false;
}

bool GIFTexture::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == SNAME("delays")) {
		RWLockRead r(rw_lock);
		r_ret = delays;
		return true;
	}
	if (p_name == SNAME("disposals")) {
		RWLockRead r(rw_lock);
		r_ret = disposals;
		return true;
	}
	if (p_name == SNAME("positions")) {
		RWLockRead r(rw_lock);
		r_ret = positions;
		return true;
	}
	if (p_name == SNAME("transparent_colors")) {
		RWLockRead r(rw_lock);
		r_ret = transparent_colors;
		return true;
	}
	if (p_name == SNAME("has_transparency")) {
		RWLockRead r(rw_lock);
		r_ret = has_transparency;
		return true;
	}
	return false;
}

void GIFTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_path", "path"), &GIFTexture::load_from_path);
	ClassDB::bind_method(D_METHOD("load_from_buffer", "buffer"), &GIFTexture::load_from_buffer);
	ClassDB::bind_method(D_METHOD("save_to_path", "path"), &GIFTexture::save_to_path);
	ClassDB::bind_method(D_METHOD("save_to_buffer"), &GIFTexture::save_to_buffer);
	ClassDB::bind_method(D_METHOD("bake_frames"), &GIFTexture::bake_frames);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &GIFTexture::get_frame_count);
	ClassDB::bind_method(D_METHOD("wrap_frame", "frame"), &GIFTexture::wrap_frame);
	ClassDB::bind_method(D_METHOD("get_active_texture", "frame"), &GIFTexture::get_active_texture);
	ClassDB::bind_method(D_METHOD("get_source_image", "frame"), &GIFTexture::get_source_image);
	ClassDB::bind_method(D_METHOD("get_baked_image", "frame"), &GIFTexture::get_baked_image);
	ClassDB::bind_method(D_METHOD("get_frame_delay", "frame"), &GIFTexture::get_frame_delay);
	ClassDB::bind_method(D_METHOD("get_frame_delay_sec", "frame"), &GIFTexture::get_frame_delay_sec);
	ClassDB::bind_method(D_METHOD("get_frame_disposal", "frame"), &GIFTexture::get_frame_disposal);
	ClassDB::bind_method(D_METHOD("get_frame_position", "frame"), &GIFTexture::get_frame_position);
	ClassDB::bind_method(D_METHOD("get_frame_transparent_color", "frame"), &GIFTexture::get_frame_transparent_color);
	ClassDB::bind_method(D_METHOD("get_frame_has_transparency", "frame"), &GIFTexture::get_frame_has_transparency);
	ClassDB::bind_method(D_METHOD("add_source_frame", "image", "delay_cs", "disposal", "position"), &GIFTexture::add_source_frame, DEFVAL(10), DEFVAL(DISPOSAL_NONE), DEFVAL(Vector2i()));
	ClassDB::bind_method(D_METHOD("to_sprite_frames", "animation_name"), &GIFTexture::to_sprite_frames, DEFVAL(StringName("default")));
	ClassDB::bind_static_method("GIFTexture", D_METHOD("from_sprite_frames", "frames", "animation_name"), &GIFTexture::from_sprite_frames, DEFVAL(StringName("default")));

	ClassDB::bind_method(D_METHOD("set_play", "play"), &GIFTexture::set_play);
	ClassDB::bind_method(D_METHOD("get_play"), &GIFTexture::get_play);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &GIFTexture::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &GIFTexture::get_loop);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &GIFTexture::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &GIFTexture::get_speed_scale);
	ClassDB::bind_method(D_METHOD("set_current_frame", "frame"), &GIFTexture::set_current_frame);
	ClassDB::bind_method(D_METHOD("get_current_frame"), &GIFTexture::get_current_frame);

	ClassDB::bind_method(D_METHOD("set_canvas_size", "size"), &GIFTexture::set_canvas_size);
	ClassDB::bind_method(D_METHOD("get_canvas_size"), &GIFTexture::get_canvas_size);
	ClassDB::bind_method(D_METHOD("set_netscape_loop_count", "count"), &GIFTexture::set_netscape_loop_count);
	ClassDB::bind_method(D_METHOD("get_netscape_loop_count"), &GIFTexture::get_netscape_loop_count);
	ClassDB::bind_method(D_METHOD("set_display_mode", "mode"), &GIFTexture::set_display_mode);
	ClassDB::bind_method(D_METHOD("get_display_mode"), &GIFTexture::get_display_mode);
	ClassDB::bind_method(D_METHOD("set_bake_storage", "storage"), &GIFTexture::set_bake_storage);
	ClassDB::bind_method(D_METHOD("get_bake_storage"), &GIFTexture::get_bake_storage);
	ClassDB::bind_method(D_METHOD("set_bake_compress", "compress"), &GIFTexture::set_bake_compress);
	ClassDB::bind_method(D_METHOD("get_bake_compress"), &GIFTexture::get_bake_compress);
	ClassDB::bind_method(D_METHOD("set_dither", "dither"), &GIFTexture::set_dither);
	ClassDB::bind_method(D_METHOD("get_dither"), &GIFTexture::get_dither);
	ClassDB::bind_method(D_METHOD("_get_source_frames"), &GIFTexture::_get_source_frames);
	ClassDB::bind_method(D_METHOD("_set_source_frames", "frames"), &GIFTexture::_set_source_frames);
	ClassDB::bind_method(D_METHOD("_get_baked_frames"), &GIFTexture::_get_baked_frames);
	ClassDB::bind_method(D_METHOD("_set_baked_frames", "frames"), &GIFTexture::_set_baked_frames);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "play"), "set_play", "get_play");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "-60,60,0.1,or_less,or_greater"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_frame"), "set_current_frame", "get_current_frame");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "canvas_size"), "set_canvas_size", "get_canvas_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "netscape_loop_count", PROPERTY_HINT_RANGE, "0,65535,1"), "set_netscape_loop_count", "get_netscape_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "display_mode", PROPERTY_HINT_ENUM, "Source,Baked"), "set_display_mode", "get_display_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_storage", PROPERTY_HINT_ENUM, "Store,Generate On Load"), "set_bake_storage", "get_bake_storage");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_compress"), "set_bake_compress", "get_bake_compress");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dither"), "set_dither", "get_dither");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "source_frames", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "_set_source_frames", "_get_source_frames");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "baked_frames", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "_set_baked_frames", "_get_baked_frames");

	ADD_SIGNAL(MethodInfo("frames_changed"));
	ADD_SIGNAL(MethodInfo("rebaked"));

	BIND_ENUM_CONSTANT(DISPLAY_SOURCE);
	BIND_ENUM_CONSTANT(DISPLAY_BAKED);
	BIND_ENUM_CONSTANT(BAKE_STORE);
	BIND_ENUM_CONSTANT(BAKE_GENERATE_ON_LOAD);
	BIND_ENUM_CONSTANT(DISPOSAL_NONE);
	BIND_ENUM_CONSTANT(DISPOSAL_DO_NOT_DISPOSE);
	BIND_ENUM_CONSTANT(DISPOSAL_RESTORE_BACKGROUND);
	BIND_ENUM_CONSTANT(DISPOSAL_RESTORE_PREVIOUS);
}

GIFTexture::GIFTexture() {
	set_local_to_scene(true);
	ERR_FAIL_NULL(RenderingServer::get_singleton());

	proxy_ph = RS::get_singleton()->texture_2d_placeholder_create();
	proxy = RS::get_singleton()->texture_proxy_create(proxy_ph);
	callable_mp(this, &GIFTexture::_finish_non_thread_safe_setup).call_deferred();
}

GIFTexture::~GIFTexture() {
	MutexLock m(_active_mutex);
	if (_active_textures.has(this)) {
		_active_textures.erase(this);
	}
	ERR_FAIL_NULL(RenderingServer::get_singleton());

	if (proxy.is_valid()) {
		RS::get_singleton()->free(proxy);
	}
	if (proxy_ph.is_valid()) {
		RS::get_singleton()->free(proxy_ph);
	}
}
