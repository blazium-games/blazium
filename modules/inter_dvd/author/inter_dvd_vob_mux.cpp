/**************************************************************************/
/*  inter_dvd_vob_mux.cpp                                                 */
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

#include "inter_dvd_vob_mux.h"

#include "inter_dvd_project.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"

namespace {

void write_be16(Vector<uint8_t> &p_buf, int p_off, uint16_t p_value) {
	p_buf.write[p_off] = uint8_t((p_value >> 8) & 0xFF);
	p_buf.write[p_off + 1] = uint8_t(p_value & 0xFF);
}

void write_be32(Vector<uint8_t> &p_buf, int p_off, uint32_t p_value) {
	p_buf.write[p_off] = uint8_t((p_value >> 24) & 0xFF);
	p_buf.write[p_off + 1] = uint8_t((p_value >> 16) & 0xFF);
	p_buf.write[p_off + 2] = uint8_t((p_value >> 8) & 0xFF);
	p_buf.write[p_off + 3] = uint8_t(p_value & 0xFF);
}

uint8_t to_bcd(int p_value) {
	const int v = CLAMP(p_value, 0, 99);
	return uint8_t(((v / 10) << 4) | (v % 10));
}

void write_dvd_time(Vector<uint8_t> &p_buf, int p_off, double p_seconds) {
	const int total = MAX(int(Math::round(MAX(p_seconds, 0.0) * 30.0)), 0);
	const int hh = total / (30 * 3600);
	const int mm = (total / (30 * 60)) % 60;
	const int ss = (total / 30) % 60;
	const int ff = total % 30;
	p_buf.write[p_off] = to_bcd(hh);
	p_buf.write[p_off + 1] = to_bcd(mm);
	p_buf.write[p_off + 2] = to_bcd(ss);
	p_buf.write[p_off + 3] = uint8_t(0xC0 | to_bcd(ff));
}

void write_pack_scr(uint8_t *p_d, uint64_t p_scr) {
	p_d[0] = uint8_t(0x40 | ((p_scr >> 27) & 0x38) | 0x04 | ((p_scr >> 28) & 0x03));
	p_d[1] = uint8_t((p_scr >> 20) & 0xFF);
	p_d[2] = uint8_t(0x04 | ((p_scr >> 12) & 0xF8) | ((p_scr >> 13) & 0x03));
	p_d[3] = uint8_t((p_scr >> 5) & 0xFF);
	p_d[4] = uint8_t(((p_scr & 0x1F) << 3) | 0x04);
	p_d[5] = 0x01;
}

bool is_pack_start(const uint8_t *p_sec) {
	return p_sec[0] == 0x00 && p_sec[1] == 0x00 && p_sec[2] == 0x01 && p_sec[3] == 0xBA;
}

bool is_nav_sector(const uint8_t *p_sec) {
	return is_pack_start(p_sec) && p_sec[38] == 0x00 && p_sec[39] == 0x00 && p_sec[40] == 0x01 && p_sec[41] == 0xBF;
}

bool is_ac3_sector(const uint8_t *p_sec) {
	if (!is_pack_start(p_sec)) {
		return false;
	}
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 10; i++) {
		if (p_sec[i] != 0x00 || p_sec[i + 1] != 0x00 || p_sec[i + 2] != 0x01 || p_sec[i + 3] != 0xBD) {
			continue;
		}
		const int sub = i + 9 + p_sec[i + 8];
		if (sub < InterDVDVobMux::SECTOR_SIZE && (p_sec[sub] & 0xF8) == 0x80) {
			return true;
		}
	}
	return false;
}

bool is_spu_sector(const uint8_t *p_sec) {
	if (!is_pack_start(p_sec)) {
		return false;
	}
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 10; i++) {
		if (p_sec[i] != 0x00 || p_sec[i + 1] != 0x00 || p_sec[i + 2] != 0x01 || p_sec[i + 3] != 0xBD) {
			continue;
		}
		const int sub = i + 9 + p_sec[i + 8];
		if (sub < InterDVDVobMux::SECTOR_SIZE && p_sec[sub] >= 0x20 && p_sec[sub] <= 0x3F) {
			return true;
		}
	}
	return false;
}

void collect_private_ids(const uint8_t *p_sec, uint8_t p_min, uint8_t p_max, uint8_t *p_seen, int p_seen_n) {
	if (!is_pack_start(p_sec)) {
		return;
	}
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 10; i++) {
		if (p_sec[i] != 0x00 || p_sec[i + 1] != 0x00 || p_sec[i + 2] != 0x01 || p_sec[i + 3] != 0xBD) {
			continue;
		}
		const int sub = i + 9 + p_sec[i + 8];
		if (sub >= InterDVDVobMux::SECTOR_SIZE) {
			continue;
		}
		const uint8_t id = p_sec[sub];
		if (id < p_min || id > p_max) {
			continue;
		}
		const int idx = int(id - p_min);
		if (idx >= 0 && idx < p_seen_n) {
			p_seen[idx] = 1;
		}
	}
}

int count_private_range(const String &p_path, uint8_t p_min, uint8_t p_max) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return 0;
	}
	const int span = int(p_max) - int(p_min) + 1;
	uint8_t seen[64];
	for (int i = 0; i < 64; i++) {
		seen[i] = 0;
	}
	uint8_t sec[InterDVDVobMux::SECTOR_SIZE];
	int checked = 0;
	while (f->get_position() < f->get_length() && checked < 512) {
		const int n = int(f->get_buffer(sec, InterDVDVobMux::SECTOR_SIZE));
		if (n < InterDVDVobMux::SECTOR_SIZE) {
			break;
		}
		collect_private_ids(sec, p_min, p_max, seen, MIN(span, 64));
		checked++;
	}
	int count = 0;
	for (int i = 0; i < span && i < 64; i++) {
		count += seen[i] ? 1 : 0;
	}
	return count;
}

bool is_mpeg_video_sector(const uint8_t *p_sec) {
	if (!is_pack_start(p_sec)) {
		return false;
	}
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 4; i++) {
		if (p_sec[i] == 0x00 && p_sec[i + 1] == 0x00 && p_sec[i + 2] == 0x01 && p_sec[i + 3] >= 0xE0 && p_sec[i + 3] <= 0xEF) {
			return true;
		}
	}
	return false;
}

Error copy_file(const String &p_src, const String &p_dst);

bool source_path_needs_safe_copy(const String &p_path) {
	return p_path.contains("#") || p_path.contains("!") || p_path.contains("?") || p_path.contains(String::chr(0xFF1F)) || p_path.contains(" ");
}

String cached_ascii_source(const String &p_src, const String &p_out_vob, String *r_error) {
	if (!source_path_needs_safe_copy(p_src)) {
		return p_src;
	}
	String ext = p_src.get_extension().to_lower();
	if (ext.is_empty()) {
		ext = "mp4";
	}
	const String cache = p_out_vob.get_base_dir().path_join(p_src.md5_text() + "." + ext);
	const Error err = copy_file(p_src, cache);
	if (err != OK) {
		if (r_error) {
			*r_error = vformat("Could not cache source with special characters: %s", p_src);
		}
		return String();
	}
	return cache;
}

void collect_cell_sidecars(const Ref<InterDVDCell> &p_cell, Vector<String> &r_audio, Vector<String> &r_subs) {
	if (p_cell.is_null()) {
		return;
	}
	if (p_cell->get_include_audio() && !p_cell->get_audio_path().is_empty() && FileAccess::exists(p_cell->get_audio_path())) {
		r_audio.push_back(p_cell->get_audio_path());
	}
	const TypedArray<InterDVDStream> streams = p_cell->get_streams();
	for (int i = 0; i < streams.size(); i++) {
		const Ref<InterDVDStream> stream = streams[i];
		if (stream.is_null() || !stream->is_enabled() || stream->get_source_path().is_empty() || !FileAccess::exists(stream->get_source_path())) {
			continue;
		}
		if (stream->get_kind() == InterDVDStream::KIND_SUBTITLE) {
			r_subs.push_back(stream->get_source_path());
		} else {
			r_audio.push_back(stream->get_source_path());
		}
	}
}

Error run_dvd_ffmpeg(const String &p_ffmpeg, const String &p_video, const Vector<String> &p_audio, const Vector<String> &p_subs, bool p_include_audio, const String &p_out, String *r_error) {
	List<String> args;
	args.push_back("-y");
	args.push_back("-i");
	args.push_back(p_video);
	for (int i = 0; i < p_audio.size(); i++) {
		args.push_back("-i");
		args.push_back(p_audio[i]);
	}
	for (int i = 0; i < p_subs.size(); i++) {
		args.push_back("-i");
		args.push_back(p_subs[i]);
	}
	args.push_back("-target");
	args.push_back("ntsc-dvd");
	const bool need_map = !p_audio.is_empty() || !p_subs.is_empty() || !p_include_audio;
	if (need_map) {
		args.push_back("-map");
		args.push_back("0:v:0");
		if (p_include_audio && p_audio.is_empty()) {
			args.push_back("-map");
			args.push_back("0:a:0?");
		}
		for (int i = 0; i < p_audio.size(); i++) {
			args.push_back("-map");
			args.push_back(vformat("%d:a:0?", i + 1));
		}
		const int sub_base = 1 + p_audio.size();
		for (int i = 0; i < p_subs.size(); i++) {
			args.push_back("-map");
			args.push_back(vformat("%d:s:0?", sub_base + i));
		}
		if (!p_subs.is_empty()) {
			args.push_back("-c:s");
			args.push_back("dvdsub");
		}
	}
	args.push_back(p_out);
	String pipe;
	const int code = OS::get_singleton()->execute(p_ffmpeg, args, &pipe);
	if (code == 0 && FileAccess::exists(p_out)) {
		return OK;
	}
	if (r_error) {
		*r_error = vformat("ffmpeg failed (%d): %s", code, pipe);
	}
	return FAILED;
}

Error reject_unplayable_cell(const String &p_path, bool p_allow_dummy, String *r_error) {
	if (p_allow_dummy) {
		return OK;
	}
	const double dur = InterDVDVobMux::estimate_duration_sec(p_path);
	if (dur < 2.0) {
		if (r_error) {
			*r_error = vformat("Muxed cell is %.2fs (need >= 2s): %s", dur, p_path);
		}
		return FAILED;
	}
	if (!InterDVDVobMux::contains_mpeg_video(p_path)) {
		if (r_error) {
			*r_error = vformat("Muxed cell has no MPEG video PES: %s", p_path);
		}
		return FAILED;
	}
	return OK;
}

uint16_t first_ac3_rel(const Vector<Vector<uint8_t>> &p_secs, int p_nav, int p_end) {
	for (int i = p_nav + 1; i < p_end && i < p_secs.size(); i++) {
		if (is_ac3_sector(p_secs[i].ptr())) {
			return uint16_t(CLAMP(i - p_nav, 1, 0x3FFE));
		}
	}
	return 0x3FFF;
}

bool read_pes_pts(const uint8_t *p_sec, uint8_t p_stream, uint32_t &r_pts) {
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 14; i++) {
		if (p_sec[i] != 0x00 || p_sec[i + 1] != 0x00 || p_sec[i + 2] != 0x01 || p_sec[i + 3] != p_stream) {
			continue;
		}
		if ((p_sec[i + 7] & 0x80) == 0 || p_sec[i + 8] < 5) {
			continue;
		}
		const uint8_t *b = p_sec + i + 9;
		if ((b[0] & 0xC1) != 0x01 || ((b[0] & 0x30) != 0x20 && (b[0] & 0x30) != 0x30)) {
			continue;
		}
		r_pts = (uint32_t((b[0] >> 1) & 0x07) << 30) | (uint32_t(b[1]) << 22) | (uint32_t((b[2] >> 1) & 0x7F) << 15) | (uint32_t(b[3]) << 7) | (uint32_t((b[4] >> 1) & 0x7F));
		return true;
	}
	return false;
}

uint64_t read_pack_scr(const uint8_t *p_sec) {
	const uint8_t b0 = p_sec[4];
	const uint8_t b1 = p_sec[5];
	const uint8_t b2 = p_sec[6];
	const uint8_t b3 = p_sec[7];
	const uint8_t b4 = p_sec[8];
	return (uint64_t((b0 >> 3) & 0x07) << 30) | (uint64_t(b0 & 0x03) << 28) | (uint64_t(b1) << 20) | (uint64_t((b2 >> 3) & 0x1F) << 15) | (uint64_t(b2 & 0x03) << 13) | (uint64_t(b3) << 5) | (uint64_t((b4 >> 3) & 0x1F));
}

bool first_video_clock(const Vector<Vector<uint8_t>> &p_sectors, int p_from, int p_to, uint32_t &r_pts, uint64_t &r_scr) {
	for (int i = p_from; i < p_to && i < p_sectors.size(); i++) {
		if (p_sectors[i].size() < InterDVDVobMux::SECTOR_SIZE) {
			continue;
		}
		const uint8_t *p = p_sectors[i].ptr();
		uint32_t pts = 0;
		if (!is_pack_start(p) || (p[4] & 0xC4) != 0x44 || !read_pes_pts(p, 0xE0, pts)) {
			continue;
		}
		r_pts = pts;
		r_scr = read_pack_scr(p);
		return true;
	}
	return false;
}

bool sector_has_start_code(const uint8_t *p_sec, uint8_t p_id) {
	for (int i = 0; i <= InterDVDVobMux::SECTOR_SIZE - 4; i++) {
		if (p_sec[i] == 0x00 && p_sec[i + 1] == 0x00 && p_sec[i + 2] == 0x01 && p_sec[i + 3] == p_id) {
			return true;
		}
	}
	return false;
}

bool is_mpeg_program_stream(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	if (ext == "vob" || ext == "mpg" || ext == "mpeg" || ext == "m2p") {
		return true;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null() || f->get_length() < 4) {
		return false;
	}
	uint8_t h[4] = {};
	f->get_buffer(h, 4);
	return h[0] == 0x00 && h[1] == 0x00 && h[2] == 0x01 && h[3] == 0xBA;
}

Error copy_file(const String &p_src, const String &p_dst) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		return ERR_CANT_OPEN;
	}
	return da->copy(p_src, p_dst);
}

Vector<uint8_t> make_nav_sector(uint32_t p_lbn, uint32_t p_vobu_ea, uint32_t p_next_vobu, uint32_t p_prev_vobu, uint32_t p_s_ptm, uint32_t p_e_ptm, uint64_t p_pack_scr, double p_eltm_sec) {
	Vector<uint8_t> sec;
	sec.resize(InterDVDVobMux::SECTOR_SIZE);
	sec.fill(0);

	sec.write[0] = 0x00;
	sec.write[1] = 0x00;
	sec.write[2] = 0x01;
	sec.write[3] = 0xBA;
	write_pack_scr(&sec.write[4], p_pack_scr);
	sec.write[10] = 0x01;
	sec.write[11] = 0x89;
	sec.write[12] = 0xC3;
	sec.write[13] = 0xF8;

	const uint8_t sys[] = { 0x00, 0x00, 0x01, 0xBB, 0x00, 0x12, 0x80, 0x9F, 0xF3, 0x04, 0xE1, 0x7F, 0xFE, 0xE0, 0xE0, 0xC0, 0xC0, 0xBD, 0xE0, 0xBB, 0x00, 0x00, 0x00, 0x00 };
	for (int i = 0; i < 24; i++) {
		sec.write[14 + i] = sys[i];
	}

	sec.write[38] = 0x00;
	sec.write[39] = 0x00;
	sec.write[40] = 0x01;
	sec.write[41] = 0xBF;
	sec.write[42] = 0x03;
	sec.write[43] = 0xD4;
	sec.write[44] = 0x00;
	write_be32(sec, 45, p_lbn);
	write_be32(sec, 45 + 12, p_s_ptm);
	write_be32(sec, 45 + 16, p_e_ptm);
	write_be32(sec, 45 + 20, p_e_ptm);
	write_dvd_time(sec, 45 + 24, p_eltm_sec);

	sec.write[1024] = 0x00;
	sec.write[1025] = 0x00;
	sec.write[1026] = 0x01;
	sec.write[1027] = 0xBF;
	sec.write[1028] = 0x03;
	sec.write[1029] = 0xFA;
	sec.write[1030] = 0x01;
	write_be32(sec, 1031, uint32_t(p_pack_scr));
	write_be32(sec, 1031 + 4, p_lbn);
	write_dvd_time(sec, 1031 + 28, p_eltm_sec);
	write_be32(sec, 1031 + 8, p_vobu_ea);
	if (p_vobu_ea > 0) {
		write_be32(sec, 1031 + 12, p_vobu_ea);
	}
	write_be16(sec, 1031 + 24, 1);
	sec.write[1031 + 27] = 1;

	const int sri = 1031 + 0xEA;
	write_be32(sec, sri, p_next_vobu);
	write_be32(sec, sri + 80, p_next_vobu);
	write_be32(sec, sri + 84, p_prev_vobu);
	write_be32(sec, sri + 164, p_prev_vobu);
	write_be16(sec, sri + 168, 0x3FFF);
	return sec;
}

constexpr uint32_t SRI_NONE = 0x3FFFFFFF;
constexpr uint32_t SRI_OK = 0x40000000;

void put_bits(Vector<uint8_t> &p_buf, int &p_bit, int p_count, uint32_t p_value) {
	for (int i = p_count - 1; i >= 0; i--) {
		const int byte = p_bit / 8;
		const int bit = 7 - (p_bit % 8);
		if (byte < p_buf.size() && ((p_value >> i) & 1u)) {
			p_buf.write[byte] = uint8_t(p_buf[byte] | (1u << bit));
		}
		p_bit++;
	}
}

PackedByteArray hardware_button_command(const Ref<InterDVDButton> &p_btn, bool p_title_domain) {
	if (p_btn.is_valid()) {
		return p_btn->resolve_command(p_title_domain ? InterDVDButton::DOMAIN_VTST : InterDVDButton::DOMAIN_VMGM);
	}
	PackedByteArray jump;
	jump.resize(8);
	jump.fill(0);
	if (p_title_domain) {
		jump.write[0] = 0x20;
		jump.write[1] = 0x04;
		jump.write[7] = 1;
	} else {
		jump.write[0] = 0x30;
		jump.write[1] = 0x02;
		jump.write[5] = 1;
	}
	return jump;
}

Rect2 button_rect(const Ref<InterDVDButton> &p_btn) {
	return p_btn.is_valid() ? p_btn->get_highlight() : Rect2();
}

uint8_t nearest_neighbor(const TypedArray<InterDVDButton> &p_buttons, int p_self, int p_axis, int p_dir) {
	const int n = MIN(p_buttons.size(), 36);
	const Vector2 self = button_rect(p_buttons[p_self]).get_center();
	int best = p_self;
	double best_score = 1e18;
	for (int i = 0; i < n; i++) {
		if (i == p_self) {
			continue;
		}
		const Vector2 c = button_rect(p_buttons[i]).get_center();
		const double along = p_axis == 0 ? (c.x - self.x) : (c.y - self.y);
		const double across = p_axis == 0 ? Math::abs(c.y - self.y) : Math::abs(c.x - self.x);
		if (along * p_dir <= 4.0) {
			continue;
		}
		const double score = along * p_dir + across * 0.35;
		if (score < best_score) {
			best_score = score;
			best = i;
		}
	}
	return uint8_t(best + 1);
}

int nearest_clut_index(const Color &p_color) {
	const Color slots[3] = { Color(1, 0.92, 0.2), Color(1, 0.35, 0.1), Color(1, 1, 1) };
	int best = 1;
	float best_d = 1e9f;
	for (int i = 0; i < 3; i++) {
		const float dr = p_color.r - slots[i].r;
		const float dg = p_color.g - slots[i].g;
		const float db = p_color.b - slots[i].b;
		const float d = dr * dr + dg * dg + db * db;
		if (d < best_d) {
			best_d = d;
			best = i + 1;
		}
	}
	return best;
}

uint32_t pack_coli(int p_index) {
	const uint32_t idx = uint32_t(CLAMP(p_index, 1, 15) & 0xF);
	const uint16_t half = uint16_t((idx << 12) | 0x0F70);
	return (uint32_t(half) << 16) | half;
}

void write_hli_buttons(Vector<uint8_t> &p_sec, const TypedArray<InterDVDButton> &p_buttons, uint32_t p_s_ptm, uint32_t p_e_ptm, bool p_title_domain, uint16_t p_hli_ss, int p_forced_select, int p_forced_activate, uint16_t p_btn_colnfo, int p_title_safe_bottom) {
	const int n = MIN(p_buttons.size(), 36);
	if (n <= 0) {
		return;
	}
	int fosl = p_forced_select;
	int foac = p_forced_activate;
	int numeric_n = 0;
	for (int i = 0; i < n; i++) {
		const Ref<InterDVDButton> btn = p_buttons[i];
		if (btn.is_null()) {
			continue;
		}
		if (btn->get_numeric_select()) {
			numeric_n++;
		}
		if (fosl <= 0 && btn->is_forced_selected()) {
			fosl = i + 1;
		}
		if (foac <= 0 && btn->is_forced_activated()) {
			foac = i + 1;
		}
	}
	if (fosl <= 0 && p_hli_ss == 1) {
		fosl = 1;
	}
	const int hli = 45 + 96;
	write_be16(p_sec, hli, p_hli_ss);
	write_be32(p_sec, hli + 2, p_s_ptm);
	write_be32(p_sec, hli + 6, p_e_ptm);
	write_be32(p_sec, hli + 10, p_e_ptm);

	uint16_t colnfo = p_btn_colnfo;
	if (colnfo == 0 || colnfo == 0x4000) {
		colnfo = 0x1000;
	}
	write_be16(p_sec, hli + 14, colnfo);

	p_sec.write[hli + 16] = uint8_t(n);
	p_sec.write[hli + 17] = uint8_t(numeric_n > 0 ? numeric_n : n);
	p_sec.write[hli + 18] = 0;
	p_sec.write[hli + 19] = 0;
	p_sec.write[hli + 20] = uint8_t(CLAMP(fosl, 0, 36));
	p_sec.write[hli + 21] = uint8_t(CLAMP(foac, 0, 36));
	const Ref<InterDVDButton> first = p_buttons[0];
	const uint32_t sl0 = pack_coli(first.is_valid() ? nearest_clut_index(first->get_select_color()) : 1);
	const uint32_t ac0 = pack_coli(first.is_valid() ? nearest_clut_index(first->get_action_color()) : 2);
	uint32_t sl[3] = { sl0, sl0, sl0 };
	uint32_t ac[3] = { ac0, ac0, ac0 };
	bool have_group[3] = { false, false, false };
	for (int i = 0; i < n; i++) {
		const Ref<InterDVDButton> btn = p_buttons[i];
		if (btn.is_null()) {
			continue;
		}
		const int g = CLAMP(btn->get_color_group(), 1, 3) - 1;
		if (have_group[g]) {
			continue;
		}
		sl[g] = pack_coli(nearest_clut_index(btn->get_select_color()));
		ac[g] = pack_coli(nearest_clut_index(btn->get_action_color()));
		have_group[g] = true;
	}
	for (int g = 0; g < 3; g++) {
		write_be32(p_sec, hli + 22 + g * 8, sl[g]);
		write_be32(p_sec, hli + 26 + g * 8, ac[g]);
	}
	int bit = (hli + 46) * 8;
	for (int i = 0; i < n; i++) {
		const Ref<InterDVDButton> btn = p_buttons[i];
		const bool hidden = btn.is_valid() && btn->is_hidden();
		const Rect2 r = hidden ? Rect2() : button_rect(btn);
		int x0 = hidden ? 0 : CLAMP(int(r.position.x), 0, 719);
		int y0 = hidden ? 0 : CLAMP(int(r.position.y), 0, 479);
		int x1 = hidden ? 0 : CLAMP(int(r.position.x + r.size.x), x0, 719);
		int y1 = hidden ? 0 : CLAMP(int(r.position.y + r.size.y), y0, 479);
		const int safe_y = CLAMP(p_title_safe_bottom > 0 ? p_title_safe_bottom : InterDVDSettings::title_safe_bottom(), 2, 480);
		if (!hidden && y1 > safe_y) {
			const int bh = MAX(y1 - y0, 2);
			y1 = safe_y;
			y0 = MAX(0, y1 - bh);
		}

		x0 &= ~1;
		y0 &= ~1;
		x1 &= ~1;
		y1 &= ~1;
		if (!hidden && x1 <= x0) {
			x1 = MIN(x0 + 2, 718);
		}
		if (!hidden && y1 <= y0) {
			y1 = MIN(y0 + 2, 478);
		}
		const int up = (btn.is_valid() && btn->get_adjacent_up() > 0) ? btn->get_adjacent_up() : nearest_neighbor(p_buttons, i, 1, -1);
		const int down = (btn.is_valid() && btn->get_adjacent_down() > 0) ? btn->get_adjacent_down() : nearest_neighbor(p_buttons, i, 1, 1);
		const int left = (btn.is_valid() && btn->get_adjacent_left() > 0) ? btn->get_adjacent_left() : nearest_neighbor(p_buttons, i, 0, -1);
		const int right = (btn.is_valid() && btn->get_adjacent_right() > 0) ? btn->get_adjacent_right() : nearest_neighbor(p_buttons, i, 0, 1);
		put_bits(p_sec, bit, 2, uint32_t(btn.is_valid() ? CLAMP(btn->get_color_group(), 1, 3) : 1));
		put_bits(p_sec, bit, 10, uint32_t(x0));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 10, uint32_t(x1));
		put_bits(p_sec, bit, 2, (btn.is_valid() && btn->get_auto_action()) ? 1 : 0);
		put_bits(p_sec, bit, 10, uint32_t(y0));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 10, uint32_t(y1));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 6, uint32_t(CLAMP(up, 1, 36)));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 6, uint32_t(CLAMP(down, 1, 36)));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 6, uint32_t(CLAMP(left, 1, 36)));
		put_bits(p_sec, bit, 2, 0);
		put_bits(p_sec, bit, 6, uint32_t(CLAMP(right, 1, 36)));
		const PackedByteArray cmd = hardware_button_command(btn, p_title_domain);

		{
		}

		for (int b = 0; b < 8; b++) {
			put_bits(p_sec, bit, 8, cmd.size() > b ? cmd[b] : 0);
		}
	}
}

} //namespace

String InterDVDVobMux::find_ffmpeg_on_path() {
#ifdef WINDOWS_ENABLED
	const String exe = "ffmpeg.exe";
	const String sep = ";";
#else
	const String exe = "ffmpeg";
	const String sep = ":";
#endif
	const Vector<String> parts = OS::get_singleton()->get_environment("PATH").split(sep, false);
	for (int i = 0; i < parts.size(); i++) {
		const String candidate = parts[i].strip_edges().path_join(exe);
		if (FileAccess::exists(candidate)) {
			return candidate;
		}
	}
	const String env_ffmpeg = OS::get_singleton()->get_environment("FFMPEG_EXE");
	if (!env_ffmpeg.is_empty() && FileAccess::exists(env_ffmpeg)) {
		return env_ffmpeg;
	}
#ifdef WINDOWS_ENABLED
	const String chocolatey = "C:/ProgramData/chocolatey/bin/ffmpeg.exe";
	if (FileAccess::exists(chocolatey)) {
		return chocolatey;
	}
#endif
	return String();
}

String InterDVDVobMux::resolve_ffmpeg(const String &p_configured, bool p_auto_find) {
	if (!p_configured.is_empty() && FileAccess::exists(p_configured)) {
		return p_configured;
	}
	if (p_auto_find) {
		return find_ffmpeg_on_path();
	}
	return String();
}

String InterDVDVobMux::find_ffmpeg(const String &p_configured) {
	return resolve_ffmpeg(p_configured, true);
}

Error InterDVDVobMux::write_dummy_vob(const String &p_path) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (err != OK || f.is_null()) {
		return err == OK ? ERR_CANT_CREATE : err;
	}
	const Vector<uint8_t> nav = make_nav_sector(0, 0, SRI_NONE, SRI_NONE, 0, 90000, 0, 0.0);
	f->store_buffer(nav.ptr(), nav.size());
	return OK;
}

uint32_t InterDVDVobMux::sector_count(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return 0;
	}
	return uint32_t((f->get_length() + SECTOR_SIZE - 1) / SECTOR_SIZE);
}

bool InterDVDVobMux::contains_ac3(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	uint8_t sec[SECTOR_SIZE];
	int checked = 0;
	while (f->get_position() < f->get_length() && checked < 256) {
		const int n = int(f->get_buffer(sec, SECTOR_SIZE));
		if (n < SECTOR_SIZE) {
			break;
		}
		if (is_ac3_sector(sec)) {
			return true;
		}
		checked++;
	}
	return false;
}

bool InterDVDVobMux::contains_mpeg_video(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	uint8_t sec[SECTOR_SIZE];
	int checked = 0;
	while (f->get_position() < f->get_length() && checked < 512) {
		const int n = int(f->get_buffer(sec, SECTOR_SIZE));
		if (n < SECTOR_SIZE) {
			break;
		}
		if (is_mpeg_video_sector(sec)) {
			return true;
		}
		checked++;
	}
	return false;
}

bool InterDVDVobMux::contains_spu(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	uint8_t sec[SECTOR_SIZE];
	int checked = 0;
	while (f->get_position() < f->get_length() && checked < 512) {
		const int n = int(f->get_buffer(sec, SECTOR_SIZE));
		if (n < SECTOR_SIZE) {
			break;
		}
		if (is_spu_sector(sec)) {
			return true;
		}
		checked++;
	}
	return false;
}

int InterDVDVobMux::count_ac3_streams(const String &p_path) {
	const int n = count_private_range(p_path, 0x80, 0x87);
	return n > 0 ? n : (contains_ac3(p_path) ? 1 : 0);
}

int InterDVDVobMux::count_spu_streams(const String &p_path) {
	return count_private_range(p_path, 0x20, 0x3F);
}

Vector<uint32_t> InterDVDVobMux::scan_vobu_sectors(const String &p_vob) {
	Vector<uint32_t> starts;
	Ref<FileAccess> f = FileAccess::open(p_vob, FileAccess::READ);
	if (f.is_null()) {
		return starts;
	}
	uint8_t sec[SECTOR_SIZE];
	uint32_t idx = 0;
	while (f->get_position() < f->get_length()) {
		const int n = int(f->get_buffer(sec, SECTOR_SIZE));
		if (n <= 0) {
			break;
		}
		if (n < SECTOR_SIZE) {
			for (int i = n; i < SECTOR_SIZE; i++) {
				sec[i] = 0;
			}
		}
		if (is_nav_sector(sec) || starts.is_empty()) {
			starts.push_back(idx);
		}
		idx++;
	}
	if (starts.is_empty()) {
		starts.push_back(0);
	}
	return starts;
}

Error InterDVDVobMux::finalize_title_vob(const String &p_path) {
	Ref<FileAccess> in = FileAccess::open(p_path, FileAccess::READ);
	if (in.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	Vector<Vector<uint8_t>> sectors;
	while (in->get_position() < in->get_length()) {
		Vector<uint8_t> sec;
		sec.resize(SECTOR_SIZE);
		sec.fill(0);
		const int n = int(in->get_buffer(sec.ptrw(), SECTOR_SIZE));
		if (n <= 0) {
			break;
		}
		sectors.push_back(sec);
	}
	in.unref();

	if (sectors.is_empty()) {
		return write_dummy_vob(p_path);
	}

	const bool already_nav = is_nav_sector(sectors[0].ptr());
	Vector<int> vobu_first;
	if (already_nav) {
		for (int i = 0; i < sectors.size(); i++) {
			if (is_nav_sector(sectors[i].ptr())) {
				vobu_first.push_back(i);
			}
		}
	} else {
		vobu_first.push_back(0);
		int since = 0;
		for (int i = 1; i < sectors.size(); i++) {
			since++;
			const uint8_t *p = sectors[i].ptr();
			if (since >= 15 || sector_has_start_code(p, 0xB8) || sector_has_start_code(p, 0xB3)) {
				vobu_first.push_back(i);
				since = 0;
			}
		}
	}

	Vector<Vector<uint8_t>> out_sectors;
	Vector<int> nav_index;
	if (already_nav) {
		out_sectors = sectors;
		nav_index = vobu_first;
	} else {
		for (int v = 0; v < vobu_first.size(); v++) {
			const int start = vobu_first[v];
			const int end = (v + 1 < vobu_first.size()) ? vobu_first[v + 1] : sectors.size();
			nav_index.push_back(out_sectors.size());
			out_sectors.push_back(Vector<uint8_t>());
			for (int i = start; i < end; i++) {
				out_sectors.push_back(sectors[i]);
			}
		}
	}

	const int nout = out_sectors.size();
	Vector<uint32_t> vo_pts;
	vo_pts.resize(nav_index.size());
	vo_pts.fill(0);
	for (int v = 0; v < nav_index.size(); v++) {
		const int nav_at = nav_index[v];
		const int next_nav = (v + 1 < nav_index.size()) ? nav_index[v + 1] : nout;
		const int vid_from = (already_nav && next_nav > nav_at + 1) ? (nav_at + 1) : nav_at;
		uint32_t pts = 0;
		uint64_t scr = 0;
		if (first_video_clock(out_sectors, vid_from, next_nav, pts, scr)) {
			vo_pts.write[v] = pts;
		}
	}
	for (int v = 0; v < nav_index.size(); v++) {
		const int nav_at = nav_index[v];
		const int next_nav = (v + 1 < nav_index.size()) ? nav_index[v + 1] : nout;
		const uint32_t ea = uint32_t(MAX(next_nav - nav_at - 1, 0));
		const uint32_t next = (v + 1 < nav_index.size()) ? (SRI_OK | uint32_t(nav_index[v + 1] - nav_at)) : SRI_NONE;
		const uint32_t prev = (v > 0) ? (SRI_OK | uint32_t(nav_at - nav_index[v - 1])) : SRI_NONE;
		uint32_t s_ptm = uint32_t(v) * 45045;
		uint32_t e_ptm = uint32_t(v + 1) * 45045;
		if (already_nav) {
			const uint8_t *p = out_sectors[nav_at].ptr();
			const uint32_t src_s = (uint32_t(p[57]) << 24) | (uint32_t(p[58]) << 16) | (uint32_t(p[59]) << 8) | uint32_t(p[60]);
			const uint32_t src_e = (uint32_t(p[61]) << 24) | (uint32_t(p[62]) << 16) | (uint32_t(p[63]) << 8) | uint32_t(p[64]);
			if (src_e > src_s) {
				s_ptm = src_s;
				e_ptm = src_e;
			} else if (vo_pts[v] > 0) {
				s_ptm = vo_pts[v];
				if (v + 1 < vo_pts.size() && vo_pts[v + 1] > vo_pts[v]) {
					e_ptm = vo_pts[v + 1];
				} else {
					e_ptm = s_ptm + 45045;
				}
			}

			Vector<uint8_t> nav = out_sectors[nav_at];
			write_be32(nav, 45, uint32_t(nav_at));
			if (src_e <= src_s) {
				write_be32(nav, 45 + 12, s_ptm);
				write_be32(nav, 45 + 16, e_ptm);
				write_be32(nav, 45 + 20, e_ptm);
				write_dvd_time(nav, 45 + 24, double(s_ptm) / 90000.0);
				write_dvd_time(nav, 1031 + 28, double(s_ptm) / 90000.0);
			}
			if ((nav[4] & 0xC4) != 0x44) {
				write_pack_scr(&nav.write[4], uint64_t(s_ptm));
				write_dvd_time(nav, 45 + 24, double(s_ptm) / 90000.0);
				write_dvd_time(nav, 1031 + 28, double(s_ptm) / 90000.0);
			}
			write_be32(nav, 1031 + 4, uint32_t(nav_at));
			write_be32(nav, 1031 + 8, ea);
			if (ea > 0) {
				write_be32(nav, 1031 + 12, ea);
			}
			write_be16(nav, 1031 + 24, 1);
			nav.write[1031 + 27] = 1;
			const int sri = 1031 + 0xEA;
			write_be32(nav, sri, next);
			write_be32(nav, sri + 80, next);
			write_be32(nav, sri + 84, prev);
			write_be32(nav, sri + 164, prev);
			write_be16(nav, sri + 168, first_ac3_rel(out_sectors, nav_at, next_nav));
			out_sectors.write[nav_at] = nav;

		} else {
			out_sectors.write[nav_at] = make_nav_sector(uint32_t(nav_at), ea, next, prev, s_ptm, e_ptm, uint64_t(s_ptm), double(s_ptm) / 90000.0);
			write_be16(out_sectors.write[nav_at], 1031 + 0xEA + 168, first_ac3_rel(out_sectors, nav_at, next_nav));
		}
	}

	Error err = OK;
	Ref<FileAccess> out = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (err != OK || out.is_null()) {
		return err == OK ? ERR_CANT_CREATE : err;
	}
	for (int i = 0; i < out_sectors.size(); i++) {
		out->store_buffer(out_sectors[i].ptr(), SECTOR_SIZE);
	}
	return OK;
}

Error InterDVDVobMux::apply_menu_buttons(const String &p_vob, const TypedArray<InterDVDButton> &p_buttons, bool p_title_domain, int p_forced_select, int p_forced_activate, uint16_t p_btn_colnfo, uint32_t p_hli_e_ptm, int p_title_safe_bottom) {
	if (p_buttons.is_empty()) {
		return OK;
	}
	Ref<FileAccess> in = FileAccess::open(p_vob, FileAccess::READ);
	if (in.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}
	Vector<Vector<uint8_t>> sectors;
	while (in->get_position() < in->get_length()) {
		Vector<uint8_t> sec;
		sec.resize(SECTOR_SIZE);
		sec.fill(0);
		const int n = int(in->get_buffer(sec.ptrw(), SECTOR_SIZE));
		if (n <= 0) {
			break;
		}
		sectors.push_back(sec);
	}
	in.unref();
	if (sectors.is_empty()) {
		return ERR_FILE_CANT_OPEN;
	}
	Vector<int> navs;
	uint32_t first_s_ptm = 0;
	uint32_t last_e_ptm = 0;
	for (int i = 0; i < sectors.size(); i++) {
		if (!is_nav_sector(sectors[i].ptr())) {
			continue;
		}
		navs.push_back(i);
		const uint32_t s_ptm = (uint32_t(sectors[i][57]) << 24) | (uint32_t(sectors[i][58]) << 16) | (uint32_t(sectors[i][59]) << 8) | uint32_t(sectors[i][60]);
		const uint32_t e_ptm = (uint32_t(sectors[i][61]) << 24) | (uint32_t(sectors[i][62]) << 16) | (uint32_t(sectors[i][63]) << 8) | uint32_t(sectors[i][64]);
		if (navs.size() == 1) {
			first_s_ptm = s_ptm;
		}
		if (e_ptm) {
			last_e_ptm = e_ptm;
		}
	}

	const uint32_t hl_e_ptm = p_hli_e_ptm ? p_hli_e_ptm : (last_e_ptm ? last_e_ptm : 90000);

	if (!navs.is_empty()) {
		write_hli_buttons(sectors.write[navs[0]], p_buttons, first_s_ptm, hl_e_ptm, p_title_domain, 1, p_forced_select, p_forced_activate, p_btn_colnfo, p_title_safe_bottom);
		for (int i = 1; i < navs.size(); i++) {
			write_hli_buttons(sectors.write[navs[i]], p_buttons, first_s_ptm, hl_e_ptm, p_title_domain, 2, 0, 0, p_btn_colnfo, p_title_safe_bottom);
		}
	}
	Error err = OK;
	Ref<FileAccess> out = FileAccess::open(p_vob, FileAccess::WRITE, &err);
	if (err != OK || out.is_null()) {
		return err == OK ? ERR_CANT_CREATE : err;
	}
	for (int i = 0; i < sectors.size(); i++) {
		out->store_buffer(sectors[i].ptr(), SECTOR_SIZE);
	}
	return OK;
}

Error InterDVDVobMux::append_vob(const String &p_dst, const String &p_src) {
	Ref<FileAccess> src = FileAccess::open(p_src, FileAccess::READ);
	if (src.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}
	Error err = OK;
	Ref<FileAccess> dst = FileAccess::open(p_dst, FileAccess::READ_WRITE, &err);
	if (err != OK || dst.is_null()) {
		return err == OK ? ERR_CANT_OPEN : err;
	}
	dst->seek_end();
	uint8_t sec[SECTOR_SIZE];
	while (src->get_position() < src->get_length()) {
		const int n = int(src->get_buffer(sec, SECTOR_SIZE));
		if (n <= 0) {
			break;
		}
		if (n < SECTOR_SIZE) {
			for (int i = n; i < SECTOR_SIZE; i++) {
				sec[i] = 0;
			}
		}
		dst->store_buffer(sec, SECTOR_SIZE);
	}
	return OK;
}

Error InterDVDVobMux::split_vts_vob(const String &p_vob_1) {
	constexpr uint32_t CAP = 524288;
	const uint32_t secs = sector_count(p_vob_1);
	int files = 1;
	if (secs > CAP) {
		Ref<FileAccess> in = FileAccess::open(p_vob_1, FileAccess::READ);
		if (in.is_null()) {
			return ERR_FILE_CANT_OPEN;
		}
		Vector<String> tmps;
		int part = 1;
		uint8_t buf[SECTOR_SIZE];
		while (in->get_position() < in->get_length()) {
			const String dest = p_vob_1.get_base_dir().path_join(vformat("VTS_01_%d.VOB", part));
			const String tmp = dest + ".split";
			Error err = OK;
			Ref<FileAccess> out = FileAccess::open(tmp, FileAccess::WRITE, &err);
			if (err != OK || out.is_null()) {
				return err == OK ? ERR_CANT_CREATE : err;
			}
			uint32_t n = 0;
			while (n < CAP && in->get_position() < in->get_length()) {
				const int got = int(in->get_buffer(buf, SECTOR_SIZE));
				if (got <= 0) {
					break;
				}
				if (got < SECTOR_SIZE) {
					for (int i = got; i < SECTOR_SIZE; i++) {
						buf[i] = 0;
					}
				}
				out->store_buffer(buf, SECTOR_SIZE);
				n++;
			}
			tmps.push_back(tmp);
			part++;
		}
		in.unref();
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_null()) {
			return FAILED;
		}
		da->remove(p_vob_1);
		for (int i = 0; i < tmps.size(); i++) {
			const String dest = p_vob_1.get_base_dir().path_join(vformat("VTS_01_%d.VOB", i + 1));
			da->remove(dest);
			const Error copied = copy_file(tmps[i], dest);
			da->remove(tmps[i]);
			if (copied != OK) {
				return copied;
			}
		}
		files = tmps.size();
	}


	return OK;
}

Error InterDVDVobMux::reindex_nav_lbns(const String &p_vob, uint32_t p_from_sector, uint8_t p_cell_id) {
	Ref<FileAccess> in = FileAccess::open(p_vob, FileAccess::READ);
	if (in.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}
	Vector<Vector<uint8_t>> sectors;
	while (in->get_position() < in->get_length()) {
		Vector<uint8_t> sec;
		sec.resize(SECTOR_SIZE);
		sec.fill(0);
		const int n = int(in->get_buffer(sec.ptrw(), SECTOR_SIZE));
		if (n <= 0) {
			break;
		}
		sectors.push_back(sec);
	}
	in.unref();
	for (int i = int(p_from_sector); i < sectors.size(); i++) {
		if (!is_nav_sector(sectors[i].ptr())) {
			continue;
		}
		write_be32(sectors.write[i], 45, uint32_t(i));
		write_be32(sectors.write[i], 1031 + 4, uint32_t(i));
		sectors.write[i].write[1031 + 27] = p_cell_id;
	}
	Error err = OK;
	Ref<FileAccess> out = FileAccess::open(p_vob, FileAccess::WRITE, &err);
	if (err != OK || out.is_null()) {
		return err == OK ? ERR_CANT_CREATE : err;
	}
	for (int i = 0; i < sectors.size(); i++) {
		out->store_buffer(sectors[i].ptr(), SECTOR_SIZE);
	}
	return OK;
}

double InterDVDVobMux::probe_media_sec(const String &p_path, const String &p_ffmpeg) {
	if (p_path.is_empty() || !FileAccess::exists(p_path)) {
		return 0.0;
	}
	String ffmpeg = p_ffmpeg;
	if (ffmpeg.is_empty()) {
		ffmpeg = resolve_ffmpeg(String(), true);
	}
	if (ffmpeg.is_empty()) {
		return 0.0;
	}
	const String dir = ffmpeg.get_base_dir();
	const String ff_name = ffmpeg.get_file();
	String probe = dir.path_join(ff_name.replacen("ffmpeg", "ffprobe"));
	if (!FileAccess::exists(probe)) {
		probe = dir.path_join(OS::get_singleton()->get_name() == "Windows" ? "ffprobe.exe" : "ffprobe");
	}
	String pipe;
	if (FileAccess::exists(probe)) {
		List<String> args;
		args.push_back("-v");
		args.push_back("error");
		args.push_back("-show_entries");
		args.push_back("format=duration");
		args.push_back("-of");
		args.push_back("default=nokey=1:noprint_wrappers=1");
		args.push_back(p_path);
		OS::get_singleton()->execute(probe, args, &pipe);
		return pipe.strip_edges().to_float();
	}
	List<String> args;
	args.push_back("-i");
	args.push_back(p_path);
	OS::get_singleton()->execute(ffmpeg, args, &pipe);
	const int idx = pipe.find("Duration:");
	if (idx < 0) {
		return 0.0;
	}
	const String token = pipe.substr(idx + 9).get_slice(",", 0).strip_edges();
	const PackedStringArray parts = token.split(":");
	if (parts.size() < 3) {
		return token.to_float();
	}
	return parts[0].to_float() * 3600.0 + parts[1].to_float() * 60.0 + parts[2].to_float();
}

double InterDVDVobMux::estimate_duration_sec(const String &p_vob) {
	Ref<FileAccess> f = FileAccess::open(p_vob, FileAccess::READ);
	if (f.is_null() || f->get_length() < 65) {
		return 0.0;
	}
	uint32_t last_e_ptm = 0;
	uint32_t first_pts = 0;
	uint32_t last_pts = 0;
	uint8_t sec[SECTOR_SIZE];
	while (f->get_position() < f->get_length()) {
		const int n = int(f->get_buffer(sec, SECTOR_SIZE));
		if (n < 65) {
			break;
		}
		if (is_nav_sector(sec)) {
			last_e_ptm = (uint32_t(sec[61]) << 24) | (uint32_t(sec[62]) << 16) | (uint32_t(sec[63]) << 8) | uint32_t(sec[64]);
		}
		uint32_t pts = 0;
		if (is_pack_start(sec) && read_pes_pts(sec, 0xE0, pts)) {
			if (first_pts == 0) {
				first_pts = pts;
			}
			last_pts = pts;
		}
	}
	double used = 0.0;
	const char *src = "none";
	if (last_e_ptm > 0) {
		used = double(last_e_ptm) / 90000.0;
		src = "pci";
	} else if (last_pts > first_pts) {
		used = double(last_pts - first_pts) / 90000.0;
		src = "pts";
	}


	return used;
}

Error InterDVDVobMux::mux_cell(const Ref<InterDVDCell> &p_cell, const String &p_out_vob, const String &p_ffmpeg, bool p_allow_dummy, String *r_error, bool p_auto_find_ffmpeg) {
	Vector<String> extra_audio;
	Vector<String> extra_subs;
	collect_cell_sidecars(p_cell, extra_audio, extra_subs);
	const bool include_audio = p_cell.is_null() || p_cell->get_include_audio();
	const bool has_sidecars = !extra_audio.is_empty() || !extra_subs.is_empty();

	if (p_cell.is_valid() && !p_cell->get_encoded_path().is_empty()) {
		const String src = p_cell->get_encoded_path();
		if (!FileAccess::exists(src)) {
			if (r_error) {
				*r_error = vformat("Pre-encoded cell not found: %s", src);
			}
			return ERR_FILE_NOT_FOUND;
		}
		if (!has_sidecars) {
			const Error copy_err = copy_file(src, p_out_vob);
			if (copy_err != OK) {
				return copy_err;
			}
			const Error fin = finalize_title_vob(p_out_vob);
			if (fin != OK) {
				return fin;
			}
			return reject_unplayable_cell(p_out_vob, p_allow_dummy, r_error);
		}
	}

	if (p_cell.is_valid() && (!p_cell->get_source_path().is_empty() || !p_cell->get_encoded_path().is_empty())) {
		const String src = !p_cell->get_encoded_path().is_empty() && has_sidecars ? p_cell->get_encoded_path() : p_cell->get_source_path();
		if (src.is_empty() || !FileAccess::exists(src)) {
			if (r_error) {
				*r_error = vformat("Source media not found: %s", src);
			}
			return ERR_FILE_NOT_FOUND;
		}
		if (is_mpeg_program_stream(src) && !has_sidecars) {
			const Error copy_err = copy_file(src, p_out_vob);
			if (copy_err != OK) {
				return copy_err;
			}
			const Error fin = finalize_title_vob(p_out_vob);
			if (fin != OK) {
				return fin;
			}
			return reject_unplayable_cell(p_out_vob, p_allow_dummy, r_error);
		}

		const String ffmpeg = resolve_ffmpeg(p_ffmpeg, p_auto_find_ffmpeg);
		if (ffmpeg.is_empty()) {
			if (p_allow_dummy) {
				return write_dummy_vob(p_out_vob);
			}
			if (r_error) {
				*r_error = "Cannot transcode cell: ffmpeg was not found on PATH and no ffmpeg path is set. Install ffmpeg, enable Auto-find ffmpeg, or set export/inter_dvd/ffmpeg in Editor Settings (that path is not stored in the project).";
			}
			return ERR_UNCONFIGURED;
		}
		const String safe_src = cached_ascii_source(src, p_out_vob, r_error);
		if (safe_src.is_empty()) {
			if (p_allow_dummy) {
				return write_dummy_vob(p_out_vob);
			}
			return FAILED;
		}
		Vector<String> safe_audio;
		Vector<String> safe_subs;
		for (int i = 0; i < extra_audio.size(); i++) {
			const String safe = cached_ascii_source(extra_audio[i], p_out_vob + vformat(".a%d", i), r_error);
			if (!safe.is_empty()) {
				safe_audio.push_back(safe);
			}
		}
		for (int i = 0; i < extra_subs.size(); i++) {
			const String safe = cached_ascii_source(extra_subs[i], p_out_vob + vformat(".s%d", i), r_error);
			if (!safe.is_empty()) {
				safe_subs.push_back(safe);
			}
		}
		const Error mux_err = run_dvd_ffmpeg(ffmpeg, safe_src, safe_audio, safe_subs, include_audio, p_out_vob, r_error);
		if (mux_err == OK) {
			const Error fin = finalize_title_vob(p_out_vob);
			if (fin != OK) {
				return fin;
			}
			return reject_unplayable_cell(p_out_vob, p_allow_dummy, r_error);
		}
		if (p_allow_dummy) {
			return write_dummy_vob(p_out_vob);
		}
		return mux_err;
	}

	if (p_allow_dummy) {
		return write_dummy_vob(p_out_vob);
	}
	if (r_error) {
		*r_error = "Cell has no pre-encoded MPEG-2/VOB and no source to transcode.";
	}
	return ERR_UNCONFIGURED;
}

Error InterDVDVobMux::loop_extend_vob(const String &p_vob, double p_seconds, const String &p_ffmpeg, String *r_error, bool p_auto_find_ffmpeg) {
	const double have = estimate_duration_sec(p_vob);
	if (have < 2.0 || have + 0.25 >= p_seconds) {
		return OK;
	}
	const String ffmpeg = resolve_ffmpeg(p_ffmpeg, p_auto_find_ffmpeg);
	if (ffmpeg.is_empty()) {
		return OK;
	}
	const String tmp = p_vob + ".loop.tmp.vob";
	List<String> args;
	args.push_back("-y");
	args.push_back("-fflags");
	args.push_back("+genpts");
	args.push_back("-stream_loop");
	args.push_back("-1");
	args.push_back("-i");
	args.push_back(p_vob);
	args.push_back("-t");
	args.push_back(String::num(p_seconds, 1));
	args.push_back("-target");
	args.push_back("ntsc-dvd");
	args.push_back("-bf");
	args.push_back("0");
	args.push_back("-g");
	args.push_back(itos(InterDVDSettings::gop_size()));
	args.push_back(tmp);
	String pipe;
	const int code = OS::get_singleton()->execute(ffmpeg, args, &pipe);


	if (code != 0 || !FileAccess::exists(tmp)) {
		Ref<DirAccess> rm = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (rm.is_valid()) {
			rm->remove(tmp);
		}
		return OK;
	}
	if (finalize_title_vob(tmp) != OK) {
		Ref<DirAccess> rm = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (rm.is_valid()) {
			rm->remove(tmp);
		}
		return OK;
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->remove(p_vob);
	}
	const Error copied = copy_file(tmp, p_vob);
	if (da.is_valid()) {
		da->remove(tmp);
	}


	return copied;
}
