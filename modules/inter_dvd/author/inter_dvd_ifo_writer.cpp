/**************************************************************************/
/*  inter_dvd_ifo_writer.cpp                                              */
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

#include "inter_dvd_ifo_writer.h"

#include "inter_dvd_vob_mux.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "modules/inter_dvd/editor/inter_dvd_scene_baker.h"
#endif

#include "modules/inter_dvd/machine/inter_dvd_instruction.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "scene/main/node.h"

void InterDVDExportProgress::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin", "total"), &InterDVDExportProgress::begin);
	ClassDB::bind_method(D_METHOD("report", "label"), &InterDVDExportProgress::report);
	ClassDB::bind_method(D_METHOD("set_notify", "callable"), &InterDVDExportProgress::set_notify);
	ClassDB::bind_method(D_METHOD("get_notify"), &InterDVDExportProgress::get_notify);
	ClassDB::bind_method(D_METHOD("get_step"), &InterDVDExportProgress::get_step);
	ClassDB::bind_method(D_METHOD("get_total"), &InterDVDExportProgress::get_total);
	ClassDB::bind_method(D_METHOD("get_label"), &InterDVDExportProgress::get_label);
	ClassDB::bind_method(D_METHOD("get_started_usec"), &InterDVDExportProgress::get_started_usec);
	ClassDB::bind_method(D_METHOD("format_line"), &InterDVDExportProgress::format_line);
	ClassDB::bind_static_method("InterDVDExportProgress", D_METHOD("format_clock", "usec"), &InterDVDExportProgress::format_clock);
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "notify"), "set_notify", "get_notify");
}

String InterDVDExportProgress::format_clock(int64_t p_usec) {
	const int64_t sec = MAX(p_usec, 0) / 1000000;
	const int hours = int(sec / 3600);
	const int minutes = int((sec % 3600) / 60);
	const int seconds = int(sec % 60);
	if (hours > 0) {
		return vformat("%02d:%02d:%02d", hours, minutes, seconds);
	}
	return vformat("%02d:%02d", minutes, seconds);
}

void InterDVDExportProgress::begin(int p_total) {
	if (started_usec == 0) {
		started_usec = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;
	}
	if (total <= 0) {
		total = MAX(p_total, 1);
	}
	step = 0;
}

String InterDVDExportProgress::format_line() const {
	const uint64_t now = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : started_usec;
	const int64_t elapsed = int64_t(now - started_usec);
	String eta = "--:--";
	if (step > 0 && total > step) {
		eta = format_clock(int64_t(double(elapsed) * (double(total) / double(step) - 1.0)));
	} else if (step >= total && total > 0) {
		eta = "00:00";
	}
	return vformat("[%d/%d] %s  elapsed %s  eta %s", step, total, label, format_clock(elapsed), eta);
}

void InterDVDExportProgress::report(const String &p_label) {
	if (started_usec == 0) {
		started_usec = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;
	}
	if (total <= 0) {
		total = 1;
	}
	step++;
	label = p_label;
	const String line = format_line();
	print_line(vformat("export_dvd: %s", line));
#ifdef TOOLS_ENABLED
	if (EditorNode::get_singleton()) {
		EditorNode::progress_task_step("inter_dvd", line, MAX(step - 1, 0));
	}
#endif
	if (notify.is_valid()) {
		notify.call(step, total, label, line);
	}
}

void InterDVDIfoWriter::_bind_methods() {
	ClassDB::bind_static_method("InterDVDIfoWriter", D_METHOD("write_video_ts", "root", "project", "allow_dummy_vob", "ffmpeg", "auto_find_ffmpeg", "progress"), &InterDVDIfoWriter::write_video_ts_bind, DEFVAL(false), DEFVAL(String()), DEFVAL(true), DEFVAL(Ref<InterDVDExportProgress>()));
	ClassDB::bind_static_method("InterDVDIfoWriter", D_METHOD("iso_tool_args", "tool", "volume_id", "work_dir", "out_path"), &InterDVDIfoWriter::iso_tool_args);
}

Vector<String> InterDVDIfoWriter::iso_tool_args(const String &p_tool, const String &p_volume_id, const String &p_work_dir, const String &p_out) {
	Vector<String> args;
	const String volume = InterDVDProject::sanitize_volume_id(p_volume_id);
	const String tool_name = p_tool.get_file().to_lower();
	if (tool_name.contains("mkisofs") || tool_name.contains("genisoimage")) {
		args.push_back("-dvd-video");
		args.push_back("-V");
		args.push_back(volume);
		args.push_back("-o");
		args.push_back(p_out);
		args.push_back(p_work_dir);
	} else {
		args.push_back("-u2");
		args.push_back("-l" + volume);
		args.push_back(p_work_dir);
		args.push_back(p_out);
	}
	return args;
}

Error InterDVDIfoWriter::write_video_ts_bind(const String &p_root, const Ref<InterDVDProject> &p_project, bool p_allow_dummy_vob, const String &p_ffmpeg, bool p_auto_find_ffmpeg, const Ref<InterDVDExportProgress> &p_progress) {
	String err;
	const Error code = write_video_ts(p_root, p_project, p_allow_dummy_vob, p_ffmpeg, &err, p_auto_find_ffmpeg, p_progress);
	if (code != OK && !err.is_empty()) {
		ERR_PRINT(err);
	}
	return code;
}

namespace {

constexpr int SECTOR = InterDVDIfoWriter::SECTOR_SIZE;
constexpr int IFO_SECTORS = 16;

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

void write_ident(Vector<uint8_t> &p_buf, const char *p_id) {
	for (int i = 0; p_id[i] && i < 12; i++) {
		p_buf.write[i] = uint8_t(p_id[i]);
	}
}

Error write_file(const String &p_path, const Vector<uint8_t> &p_data) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (err != OK || f.is_null()) {
		return err == OK ? ERR_CANT_CREATE : err;
	}
	f->store_buffer(p_data.ptr(), p_data.size());
	return OK;
}

void append_commands(Vector<uint8_t> &p_cmds, const TypedArray<PackedByteArray> &p_list) {
	for (int i = 0; i < p_list.size(); i++) {
		const PackedByteArray cmd = p_list[i];
		if (cmd.size() != 8) {
			continue;
		}
		const int off = p_cmds.size();
		p_cmds.resize(off + 8);
		for (int b = 0; b < 8; b++) {
			p_cmds.write[off + b] = cmd[b];
		}
	}
}

void append_cmd8(Vector<uint8_t> &p_pre, const uint8_t p_bytes[8]) {
	const int off = p_pre.size();
	p_pre.resize(off + 8);
	for (int i = 0; i < 8; i++) {
		p_pre.write[off + i] = p_bytes[i];
	}
}

void append_default_jump_tt(Vector<uint8_t> &p_pre) {
	const uint8_t jump_tt[8] = { 0x30, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
	append_cmd8(p_pre, jump_tt);
}

void append_default_jump_ss_title_menu(Vector<uint8_t> &p_pre) {
	const uint8_t jump_ss[8] = { 0x30, 0x06, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00 };
	append_cmd8(p_pre, jump_ss);
}

void write_ascii(Vector<uint8_t> &p_buf, int p_off, const String &p_text, int p_max) {
	const CharString utf = p_text.utf8();
	const char *ptr = utf.get_data();
	for (int i = 0; ptr && ptr[i] && i < p_max; i++) {
		p_buf.write[p_off + i] = uint8_t(ptr[i]);
	}
}

void write_yuv_clut_entry(Vector<uint8_t> &p_buf, int p_off, const Color &p_color) {
	const int y = CLAMP(int(16.0 + 65.481 * p_color.r + 128.553 * p_color.g + 24.966 * p_color.b), 16, 235);
	const int cb = CLAMP(int(128.0 - 37.797 * p_color.r - 74.203 * p_color.g + 112.0 * p_color.b), 16, 240);
	const int cr = CLAMP(int(128.0 + 112.0 * p_color.r - 93.786 * p_color.g - 18.214 * p_color.b), 16, 240);
	p_buf.write[p_off] = 0;
	p_buf.write[p_off + 1] = uint8_t(y);
	p_buf.write[p_off + 2] = uint8_t(cr);
	p_buf.write[p_off + 3] = uint8_t(cb);
}

void write_pgc_clut(Vector<uint8_t> &p_pgc, const PackedColorArray &p_clut) {
	const uint8_t fallback[4][4] = {
		{ 0, 16, 128, 128 },
		{ 0, 210, 146, 16 },
		{ 0, 90, 240, 110 },
		{ 0, 235, 128, 128 },
	};
	for (int i = 0; i < 16; i++) {
		if (i < p_clut.size()) {
			write_yuv_clut_entry(p_pgc, 0xA4 + i * 4, p_clut[i]);
		} else {
			const int src = MIN(i, 3);
			p_pgc.write[0xA4 + i * 4] = fallback[src][0];
			p_pgc.write[0xA4 + i * 4 + 1] = fallback[src][1];
			p_pgc.write[0xA4 + i * 4 + 2] = fallback[src][2];
			p_pgc.write[0xA4 + i * 4 + 3] = fallback[src][3];
		}
	}
}

void write_audio_attr(Vector<uint8_t> &p_buf, int p_off, uint16_t p_lang) {
	p_buf.write[p_off] = 0x00;
	p_buf.write[p_off + 1] = 0x01;
	write_be16(p_buf, p_off + 2, p_lang);
	p_buf.write[p_off + 4] = 0;
	p_buf.write[p_off + 5] = 1;
}

void write_subp_attr(Vector<uint8_t> &p_buf, int p_off, uint16_t p_lang) {
	p_buf.write[p_off] = 0x01;
	p_buf.write[p_off] = 0x40;
	p_buf.write[p_off + 1] = 0;
	write_be16(p_buf, p_off + 2, p_lang);
	p_buf.write[p_off + 4] = 0;
	p_buf.write[p_off + 5] = 1;
}

Vector<uint8_t> make_txtdt_mgi(const String &p_disc_title, const String &p_serial, uint16_t p_lang, const Vector<String> &p_titles) {
	Vector<uint8_t> buf;
	buf.resize(28);
	buf.fill(0);
	const String name = p_disc_title.is_empty() ? p_serial : p_disc_title;
	write_ascii(buf, 0, InterDVDProject::sanitize_volume_id(name.is_empty() ? String("BLAZIUM_DVD") : name), 12);
	write_be16(buf, 14, 1);
	write_be32(buf, 16, 27);
	write_be16(buf, 20, p_lang);
	buf.write[23] = 0x01;
	write_be32(buf, 24, 28);
	(void)p_titles;
	return buf;
}

uint8_t to_bcd(int p_value) {
	const int v = CLAMP(p_value, 0, 99);
	return uint8_t(((v / 10) << 4) | (v % 10));
}

void write_ntsc_playback(Vector<uint8_t> &p_buf, int p_off, double p_seconds) {
	const int total = MAX(int(Math::round(MAX(p_seconds, 0.0) * 30.0)), 1);
	const int hh = total / (30 * 3600);
	const int mm = (total / (30 * 60)) % 60;
	const int ss = (total / 30) % 60;
	const int ff = total % 30;
	p_buf.write[p_off] = to_bcd(hh);
	p_buf.write[p_off + 1] = to_bcd(mm);
	p_buf.write[p_off + 2] = to_bcd(ss);
	p_buf.write[p_off + 3] = uint8_t(0xC0 | to_bcd(ff));
}

double resolve_playback_sec(const Ref<InterDVDCell> &p_cell, const String &p_vob, const String &p_ffmpeg = String()) {
	const double authored = (p_cell.is_valid() && p_cell->get_duration_sec() > 0.0) ? p_cell->get_duration_sec() : 0.0;
	const double probed = FileAccess::exists(p_vob) ? InterDVDVobMux::estimate_duration_sec(p_vob) : 0.0;
	double pip = 0.0;
	double src = 0.0;
	if (p_cell.is_valid()) {
		const String pip_path = p_cell->get_pip_source_path();
		if (!pip_path.is_empty() && FileAccess::exists(pip_path)) {
			pip = InterDVDVobMux::probe_media_sec(pip_path, p_ffmpeg) + MAX(p_cell->get_loop_pad_sec(), 0.0);
		}
		const String src_path = p_cell->get_source_path();
		if (!src_path.is_empty() && FileAccess::exists(src_path)) {
			src = InterDVDVobMux::probe_media_sec(src_path, p_ffmpeg);
		}
		const String enc = p_cell->get_encoded_path();
		if (src <= 0.0 && !enc.is_empty() && enc != p_vob && FileAccess::exists(enc)) {
			src = InterDVDVobMux::estimate_duration_sec(enc);
		}
	}
	double sec = MAX(authored, MAX(probed, MAX(pip, src)));
	const uint32_t sectors = FileAccess::exists(p_vob) ? InterDVDVobMux::sector_count(p_vob) : 0;
	if (sec <= 2.25 && sectors > 400) {
		sec = MAX(sec, (double(sectors) * 2048.0 * 8.0) / 5500000.0);
	}
	if (sec <= 0.0) {
		sec = 2.0;
	}
	if (p_cell.is_valid() && sec > p_cell->get_duration_sec() + 0.05) {
		p_cell->set_duration_sec(sec);
	}


	return sec;
}

struct CellPlayback {
	uint32_t first_sector = 0;
	uint32_t last_vobu = 0;
	uint32_t last_sector = 0;
	uint8_t still_time = 0;
	uint8_t cell_cmd = 0;
	uint8_t cell_id = 1;
	double playback_sec = 2.0;
};

Vector<uint8_t> build_pgc(const Vector<uint8_t> &p_pre, const Vector<uint8_t> &p_post, const Vector<uint8_t> &p_cell_cmds, const Vector<CellPlayback> &p_cells, uint16_t p_next_pgc = 0, uint16_t p_prev_pgc = 0, double p_playback_sec = 2.0, bool p_has_audio = true, const PackedColorArray &p_clut = PackedColorArray(), int p_audio_streams = -1, int p_subp_streams = 0) {
	const int programs = p_cells.size();
	const int cells = p_cells.size();
	const int pre_n = p_pre.size() / 8;
	const int post_n = p_post.size() / 8;
	const int cell_n = p_cell_cmds.size() / 8;
	const int cmd_tbl = 0xEC;
	const int cmd_bytes = 8 + p_pre.size() + p_post.size() + p_cell_cmds.size();
	const int prog_off = (programs > 0) ? (cmd_tbl + cmd_bytes) : 0;
	const int cell_pb_off = (cells > 0) ? (prog_off + programs) : 0;
	const int cell_pos_off = (cells > 0) ? (cell_pb_off + cells * 24) : 0;
	const int total = (cells > 0) ? (cell_pos_off + cells * 4) : (cmd_tbl + cmd_bytes);

	Vector<uint8_t> pgc;
	pgc.resize(total);
	pgc.fill(0);
	pgc.write[2] = uint8_t(programs);
	pgc.write[3] = uint8_t(cells);
	write_ntsc_playback(pgc, 4, p_playback_sec);
	if (programs > 0 || cells > 0 || pre_n + post_n + cell_n > 0) {
		write_be16(pgc, 0xE4, uint16_t(cmd_tbl));
	}
	if (prog_off > 0) {
		write_be16(pgc, 0xE6, uint16_t(prog_off));
	}
	if (cell_pb_off > 0) {
		write_be16(pgc, 0xE8, uint16_t(cell_pb_off));
		write_be16(pgc, 0xEA, uint16_t(cell_pos_off));
	}

	pgc.write[cmd_tbl] = uint8_t((pre_n >> 8) & 0xFF);
	pgc.write[cmd_tbl + 1] = uint8_t(pre_n & 0xFF);
	pgc.write[cmd_tbl + 2] = uint8_t((post_n >> 8) & 0xFF);
	pgc.write[cmd_tbl + 3] = uint8_t(post_n & 0xFF);
	pgc.write[cmd_tbl + 4] = uint8_t((cell_n >> 8) & 0xFF);
	pgc.write[cmd_tbl + 5] = uint8_t(cell_n & 0xFF);

	write_be16(pgc, cmd_tbl + 6, uint16_t(cmd_bytes - 1));
	int c = cmd_tbl + 8;
	for (int i = 0; i < p_pre.size(); i++) {
		pgc.write[c++] = p_pre[i];
	}
	for (int i = 0; i < p_post.size(); i++) {
		pgc.write[c++] = p_post[i];
	}
	for (int i = 0; i < p_cell_cmds.size(); i++) {
		pgc.write[c++] = p_cell_cmds[i];
	}
	if (programs > 0) {
		for (int i = 0; i < programs; i++) {
			pgc.write[prog_off + i] = uint8_t(i + 1);
		}
	}
	if (p_next_pgc != 0) {
		write_be16(pgc, 0x9C, p_next_pgc);
	}
	if (p_prev_pgc != 0) {
		write_be16(pgc, 0x9E, p_prev_pgc);
	} else if (p_next_pgc != 0) {
		write_be16(pgc, 0x9E, p_next_pgc);
	}
	const int audio_n = p_audio_streams >= 0 ? CLAMP(p_audio_streams, 0, 8) : (p_has_audio ? 1 : 0);
	if (cells > 0) {
		for (int i = 0; i < audio_n; i++) {
			pgc.write[0x0C + i] = 0x80;
		}
		const int subp_n = CLAMP(p_subp_streams, 0, 32);
		for (int i = 0; i < subp_n; i++) {
			pgc.write[0x1C + i * 4] = 0x80;
		}
		write_pgc_clut(pgc, p_clut);
		for (int i = 0; i < cells; i++) {
			const CellPlayback &cell = p_cells[i];
			const int off = cell_pb_off + i * 24;

			pgc.write[off] = 0x02;
			pgc.write[off + 2] = cell.still_time;
			pgc.write[off + 3] = cell.cell_cmd;
			write_ntsc_playback(pgc, off + 4, cell.playback_sec);
			write_be32(pgc, off + 8, cell.first_sector);
			write_be32(pgc, off + 16, cell.last_vobu);
			write_be32(pgc, off + 20, cell.last_sector);
			write_be16(pgc, cell_pos_off + i * 4, 1);
			pgc.write[cell_pos_off + i * 4 + 3] = cell.cell_id;
		}
	}
	return pgc;
}

int table_sectors(int p_bytes) {
	return MAX((p_bytes + SECTOR - 1) / SECTOR, 1);
}

void copy_table(Vector<uint8_t> &p_buf, int p_off, const Vector<uint8_t> &p_src) {
	for (int i = 0; i < p_src.size(); i++) {
		const int dest = p_off + i;
		if (dest >= 0 && dest < p_buf.size()) {
			p_buf.write[dest] = p_src[i];
		}
	}
}

struct DiscIdent {
	String provider = "BLAZIUM INTER-DVD";
	String disc_title;
	String serial;
	uint16_t menu_lang = 0x656E;
	uint16_t audio_lang = 0x656E;
	uint16_t subp_lang = 0x656E;
};

struct MenuPgcSpec {
	uint8_t menu_id = 2;
	uint8_t still_time = 0;
	uint16_t next_pgc = 1;
	uint16_t prev_pgc = 1;
	double playback_sec = 2.0;
	bool has_audio = false;
	int audio_streams = 0;
	int subp_streams = 0;
	PackedColorArray clut;
	Vector<CellPlayback> cells;
};

Vector<uint8_t> build_menu_pgc(const MenuPgcSpec &p_spec, const Vector<uint8_t> &p_fallback_pre) {
	if (!p_spec.cells.is_empty()) {
		return build_pgc(Vector<uint8_t>(), Vector<uint8_t>(), Vector<uint8_t>(), p_spec.cells, p_spec.next_pgc, p_spec.prev_pgc, p_spec.playback_sec, p_spec.has_audio, p_spec.clut, p_spec.audio_streams, p_spec.subp_streams);
	}
	return build_pgc(p_fallback_pre, Vector<uint8_t>(), Vector<uint8_t>(), Vector<CellPlayback>());
}

Vector<uint8_t> make_pgci_ut(const Vector<MenuPgcSpec> &p_menus, uint16_t p_lang, const Vector<uint8_t> &p_fallback_pre) {
	Vector<MenuPgcSpec> menus = p_menus;
	if (menus.is_empty()) {
		MenuPgcSpec dummy;
		dummy.menu_id = 2;
		menus.push_back(dummy);
	}
	Vector<Vector<uint8_t>> pgcs;
	for (int i = 0; i < menus.size(); i++) {
		pgcs.push_back(build_menu_pgc(menus[i], p_fallback_pre));
	}
	const int n = pgcs.size();
	const int srp_bytes = 8 + 8 * n;
	int pgc_bytes = 0;
	for (int i = 0; i < n; i++) {
		pgc_bytes += pgcs[i].size();
	}
	Vector<uint8_t> buf;
	buf.resize(16 + srp_bytes + pgc_bytes);
	buf.fill(0);
	write_be16(buf, 0, 1);
	write_be32(buf, 4, uint32_t(buf.size() - 1));
	write_be16(buf, 8, p_lang);
	uint8_t exists = 0x80;
	for (int i = 0; i < menus.size(); i++) {
		const uint8_t menu_id = uint8_t(CLAMP(int(menus[i].menu_id), 2, 7));
		if (menu_id != 2) {
			exists = uint8_t(0x80 | menu_id);
		}
	}
	buf.write[11] = exists;
	write_be32(buf, 12, 16);
	write_be16(buf, 16, uint16_t(n));
	write_be32(buf, 20, uint32_t(srp_bytes + pgc_bytes - 1));
	int cursor = srp_bytes;
	for (int i = 0; i < n; i++) {
		const uint8_t menu_id = uint8_t(CLAMP(int(menus[i].menu_id), 2, 7));
		buf.write[24 + i * 8] = uint8_t(0x80 | menu_id);
		write_be32(buf, 28 + i * 8, uint32_t(cursor));
		for (int b = 0; b < pgcs[i].size(); b++) {
			buf.write[16 + cursor + b] = pgcs[i][b];
		}
		cursor += pgcs[i].size();
	}
	return buf;
}

Vector<uint8_t> make_vmg_ifo(int p_title_count, int p_region_mask, uint32_t p_vts_start_sector, const Vector<uint8_t> &p_fp_pre, const Vector<int> &p_chapters, const Vector<MenuPgcSpec> &p_title_menus, const DiscIdent &p_ident, const Vector<String> &p_title_names = Vector<String>()) {
	Vector<uint8_t> buf;
	buf.resize(IFO_SECTORS * SECTOR);
	buf.fill(0);
	write_ident(buf, "DVDVIDEO-VMG");
	write_be32(buf, 0x0C, uint32_t(IFO_SECTORS * 2));
	write_be32(buf, 0x1C, uint32_t(IFO_SECTORS - 1));
	write_be16(buf, 0x20, 0x0011);
	write_be16(buf, 0x26, 1);
	write_be16(buf, 0x28, 1);
	buf.write[0x2A] = 1;
	write_be16(buf, 0x3E, 1);
	write_ascii(buf, 0x40, p_ident.provider.is_empty() ? String("BLAZIUM INTER-DVD") : p_ident.provider, 32);
	const uint32_t fp_off = 0x0400;
	write_be32(buf, 0x84, fp_off);
	write_be32(buf, 0xC0, uint32_t(IFO_SECTORS));
	write_be32(buf, 0xC4, 1);
	write_be32(buf, 0xC8, 3);
	write_be32(buf, 0xD0, 2);

	buf.write[0x22] = uint8_t((~uint8_t(p_region_mask)) & 0xFF);
	int menu_audio = 0;
	int menu_subp = 0;
	for (int i = 0; i < p_title_menus.size(); i++) {
		menu_audio = MAX(menu_audio, p_title_menus[i].audio_streams);
		if (p_title_menus[i].has_audio) {
			menu_audio = MAX(menu_audio, 1);
		}
		menu_subp = MAX(menu_subp, p_title_menus[i].subp_streams);
	}
	write_be16(buf, 0x100, 0x4000);
	write_be16(buf, 0x102, uint16_t(CLAMP(menu_audio, 0, 8)));
	for (int i = 0; i < menu_audio && i < 8; i++) {
		write_audio_attr(buf, 0x104 + i * 8, p_ident.audio_lang);
	}
	write_be16(buf, 0x254, uint16_t(CLAMP(menu_subp, 0, 32)));
	for (int i = 0; i < menu_subp && i < 32; i++) {
		write_subp_attr(buf, 0x256 + i * 6, p_ident.subp_lang);
	}

	const Vector<uint8_t> fp = build_pgc(p_fp_pre, Vector<uint8_t>(), Vector<uint8_t>(), Vector<CellPlayback>());
	for (int i = 0; i < fp.size() && (int(fp_off) + i) < buf.size(); i++) {
		buf.write[int(fp_off) + i] = fp[i];
	}

	write_be32(buf, 0x80, uint32_t(fp_off + MAX(fp.size(), 1) - 1));

	const int tt = SECTOR;
	const int title_n = MAX(p_title_count, 1);
	write_be16(buf, tt, uint16_t(title_n));
	write_be32(buf, tt + 4, uint32_t(8 + 12 * title_n - 1));
	for (int i = 0; i < title_n; i++) {
		const int off = tt + 8 + i * 12;
		buf.write[off] = 0x3C;
		buf.write[off + 1] = 1;
		const int chapters = (i < p_chapters.size() && p_chapters[i] > 0) ? p_chapters[i] : 1;
		write_be16(buf, off + 2, uint16_t(chapters));
		write_be16(buf, off + 4, 1);
		buf.write[off + 6] = 1;
		buf.write[off + 7] = uint8_t(i + 1);
		write_be32(buf, off + 8, p_vts_start_sector);
	}

	const int atrt = SECTOR * 2;
	write_be16(buf, atrt, 1);
	write_be32(buf, atrt + 4, 8 + 4 + 0x300 - 1);
	write_be32(buf, atrt + 8, 12);
	write_be32(buf, atrt + 12, 0x300);

	const int ut = SECTOR * 3;
	const Vector<uint8_t> ut_buf = make_pgci_ut(p_title_menus, p_ident.menu_lang, p_fp_pre);
	copy_table(buf, ut, ut_buf);

	if (!p_ident.disc_title.is_empty()) {
		write_be32(buf, 0xD4, 4);
		copy_table(buf, SECTOR * 4, make_txtdt_mgi(p_ident.disc_title, p_ident.serial, p_ident.menu_lang, p_title_names));
	}
	return buf;
}

struct TitlePart {
	uint8_t still_time = 0;
	uint16_t next_pgc = 0;
	uint16_t prev_pgc = 0;
	double playback_sec = 2.0;
	bool has_audio = true;
	int audio_streams = 0;
	int subp_streams = 0;
	PackedColorArray clut;
	Vector<uint16_t> audio_langs;
	Vector<uint8_t> pre;
	Vector<uint8_t> post;
	Vector<uint8_t> cell_cmds;
	Vector<CellPlayback> cells;
};

Vector<uint8_t> make_vts_ifo(uint32_t p_title_sectors, const Vector<uint32_t> &p_vobus, const Vector<TitlePart> &p_parts, const DiscIdent &p_ident, const Vector<MenuPgcSpec> &p_vtsm, uint32_t p_vtsm_sectors, const Vector<uint32_t> &p_vtsm_vobus) {
	const int part_n = MAX(p_parts.size(), 1);

	int total_ptt = 0;
	int total_cells = 0;
	for (int i = 0; i < part_n; i++) {
		const int chapters = (i < p_parts.size() && p_parts[i].cells.size() > 0) ? p_parts[i].cells.size() : 1;
		total_ptt += chapters;
		total_cells += chapters;
	}

	Vector<uint8_t> ptt_buf;
	ptt_buf.resize(8 + 4 * part_n + 4 * total_ptt);
	ptt_buf.fill(0);
	write_be16(ptt_buf, 0, uint16_t(part_n));
	write_be32(ptt_buf, 4, uint32_t(ptt_buf.size() - 1));
	int ptt_cursor = 8 + 4 * part_n;
	for (int i = 0; i < part_n; i++) {
		write_be32(ptt_buf, 8 + i * 4, uint32_t(ptt_cursor));
		const int chapters = (i < p_parts.size() && p_parts[i].cells.size() > 0) ? p_parts[i].cells.size() : 1;
		for (int c = 0; c < chapters; c++) {
			write_be16(ptt_buf, ptt_cursor, uint16_t(i + 1));
			write_be16(ptt_buf, ptt_cursor + 2, uint16_t(c + 1));
			ptt_cursor += 4;
		}
	}

	Vector<Vector<uint8_t>> pgcs;
	for (int i = 0; i < part_n; i++) {
		if (i < p_parts.size() && !p_parts[i].cells.is_empty()) {
			const TitlePart &p = p_parts[i];
			pgcs.push_back(build_pgc(p.pre, p.post, p.cell_cmds, p.cells, p.next_pgc, p.prev_pgc, p.playback_sec, p.has_audio, p.clut, p.audio_streams, p.subp_streams));
		} else {
			Vector<CellPlayback> dummy;
			CellPlayback cell;
			cell.last_sector = p_title_sectors ? (p_title_sectors - 1) : 0;
			dummy.push_back(cell);
			pgcs.push_back(build_pgc(Vector<uint8_t>(), Vector<uint8_t>(), Vector<uint8_t>(), dummy));
		}
	}
	int pgc_rel = 8 + 8 * part_n;
	int pgc_bytes = 0;
	for (int i = 0; i < pgcs.size(); i++) {
		pgc_bytes += pgcs[i].size();
	}
	Vector<uint8_t> pgcit_buf;
	pgcit_buf.resize(pgc_rel + pgc_bytes);
	pgcit_buf.fill(0);
	write_be16(pgcit_buf, 0, uint16_t(part_n));
	write_be32(pgcit_buf, 4, uint32_t(pgcit_buf.size() - 1));
	int cursor = pgc_rel;
	for (int i = 0; i < pgcs.size(); i++) {
		pgcit_buf.write[8 + i * 8] = uint8_t(0x80 | (i + 1));
		write_be32(pgcit_buf, 12 + i * 8, uint32_t(cursor));
		for (int b = 0; b < pgcs[i].size(); b++) {
			pgcit_buf.write[cursor + b] = pgcs[i][b];
		}
		cursor += pgcs[i].size();
	}

	Vector<uint8_t> cadt_buf;
	cadt_buf.resize(8 + 12 * total_cells);
	cadt_buf.fill(0);
	write_be16(cadt_buf, 0, uint16_t(total_cells));
	write_be32(cadt_buf, 4, uint32_t(cadt_buf.size() - 1));
	int cadt_i = 0;
	for (int i = 0; i < part_n; i++) {
		if (i < p_parts.size() && !p_parts[i].cells.is_empty()) {
			for (int c = 0; c < p_parts[i].cells.size(); c++) {
				const int off = 8 + cadt_i * 12;
				write_be16(cadt_buf, off, 1);
				cadt_buf.write[off + 2] = p_parts[i].cells[c].cell_id;
				cadt_buf.write[off + 3] = 0;
				write_be32(cadt_buf, off + 4, p_parts[i].cells[c].first_sector);
				write_be32(cadt_buf, off + 8, p_parts[i].cells[c].last_sector);
				cadt_i++;
			}
		} else {
			const int off = 8 + cadt_i * 12;
			write_be16(cadt_buf, off, 1);
			cadt_buf.write[off + 2] = uint8_t(cadt_i + 1);
			write_be32(cadt_buf, off + 4, 0);
			write_be32(cadt_buf, off + 8, p_title_sectors ? (p_title_sectors - 1) : 0);
			cadt_i++;
		}
	}

	const int map_bytes = 4 + MAX(p_vobus.size(), 1) * 4;
	Vector<uint8_t> admap_buf;
	admap_buf.resize(map_bytes);
	admap_buf.fill(0);
	write_be32(admap_buf, 0, uint32_t(MAX(map_bytes - 1, 3)));
	if (p_vobus.is_empty()) {
		write_be32(admap_buf, 4, 0);
	} else {
		for (int i = 0; i < p_vobus.size(); i++) {
			write_be32(admap_buf, 4 + i * 4, p_vobus[i]);
		}
	}

	const Vector<uint8_t> vtsm_buf = make_pgci_ut(p_vtsm, p_ident.menu_lang, Vector<uint8_t>());

	int vtsm_cells = 0;
	for (int i = 0; i < p_vtsm.size(); i++) {
		vtsm_cells += p_vtsm[i].cells.size();
	}
	Vector<uint8_t> vtsm_cadt;
	if (vtsm_cells > 0) {
		vtsm_cadt.resize(8 + 12 * vtsm_cells);
		vtsm_cadt.fill(0);
		write_be16(vtsm_cadt, 0, uint16_t(vtsm_cells));
		write_be32(vtsm_cadt, 4, uint32_t(vtsm_cadt.size() - 1));
		int vi = 0;
		for (int i = 0; i < p_vtsm.size(); i++) {
			for (int c = 0; c < p_vtsm[i].cells.size(); c++) {
				const int off = 8 + vi * 12;
				write_be16(vtsm_cadt, off, 1);
				vtsm_cadt.write[off + 2] = p_vtsm[i].cells[c].cell_id;
				write_be32(vtsm_cadt, off + 4, p_vtsm[i].cells[c].first_sector);
				write_be32(vtsm_cadt, off + 8, p_vtsm[i].cells[c].last_sector);
				vi++;
			}
		}
	}
	Vector<uint8_t> vtsm_admap;
	if (!p_vtsm_vobus.is_empty() && vtsm_cells > 0) {
		const int vtsm_map = 4 + p_vtsm_vobus.size() * 4;
		vtsm_admap.resize(vtsm_map);
		vtsm_admap.fill(0);
		write_be32(vtsm_admap, 0, uint32_t(MAX(vtsm_map - 1, 3)));
		for (int i = 0; i < p_vtsm_vobus.size(); i++) {
			write_be32(vtsm_admap, 4 + i * 4, p_vtsm_vobus[i]);
		}
	}

	int sec = 1;
	const int ptt_sec = sec;
	sec += table_sectors(ptt_buf.size());
	const int pgcit_sec = sec;
	sec += table_sectors(pgcit_buf.size());
	const int cadt_sec = sec;
	sec += table_sectors(cadt_buf.size());
	const int admap_sec = sec;
	sec += table_sectors(admap_buf.size());
	const int vtsm_sec = sec;
	sec += table_sectors(vtsm_buf.size());
	int vtsm_cadt_sec = 0;
	int vtsm_admap_sec = 0;
	if (!vtsm_cadt.is_empty()) {
		vtsm_cadt_sec = sec;
		sec += table_sectors(vtsm_cadt.size());
	}
	if (!vtsm_admap.is_empty()) {
		vtsm_admap_sec = sec;
		sec += table_sectors(vtsm_admap.size());
	}
	const int ifo_secs = MAX(IFO_SECTORS, sec);

	Vector<uint8_t> buf;
	buf.resize(ifo_secs * SECTOR);
	buf.fill(0);
	write_ident(buf, "DVDVIDEO-VTS");
	const uint32_t menu_secs = MAX(p_vtsm_sectors, uint32_t(1));
	const uint32_t last = uint32_t(ifo_secs) + menu_secs + p_title_sectors + uint32_t(ifo_secs) - 1;
	write_be32(buf, 0x0C, last);
	write_be32(buf, 0x1C, uint32_t(ifo_secs - 1));
	write_be16(buf, 0x20, 0x0011);
	write_be32(buf, 0x80, 0x03FF);
	write_be16(buf, 0x100, 0x4000);
	int titles_audio = 0;
	int titles_subp = 0;
	Vector<uint16_t> audio_langs;
	for (int i = 0; i < p_parts.size(); i++) {
		int n = p_parts[i].audio_streams;
		if (n <= 0 && p_parts[i].has_audio) {
			n = 1;
		}
		titles_audio = MAX(titles_audio, n);
		titles_subp = MAX(titles_subp, p_parts[i].subp_streams);
		for (int a = 0; a < p_parts[i].audio_langs.size(); a++) {
			if (audio_langs.size() < 8) {
				audio_langs.push_back(p_parts[i].audio_langs[a]);
			}
		}
	}
	write_be16(buf, 0x102, uint16_t(CLAMP(titles_audio, 0, 8)));
	for (int i = 0; i < titles_audio && i < 8; i++) {
		const uint16_t lang = i < audio_langs.size() ? audio_langs[i] : p_ident.audio_lang;
		write_audio_attr(buf, 0x104 + i * 8, lang);
	}
	write_be16(buf, 0x254, uint16_t(CLAMP(titles_subp, 0, 32)));
	for (int i = 0; i < titles_subp && i < 32; i++) {
		write_subp_attr(buf, 0x256 + i * 6, p_ident.subp_lang);
	}
	write_be32(buf, 0xC0, uint32_t(ifo_secs));
	write_be32(buf, 0xC4, uint32_t(ifo_secs + menu_secs));
	write_be32(buf, 0xC8, uint32_t(ptt_sec));
	write_be32(buf, 0xCC, uint32_t(pgcit_sec));
	write_be32(buf, 0xD0, uint32_t(vtsm_sec));
	if (vtsm_cadt_sec > 0) {
		write_be32(buf, 0xD8, uint32_t(vtsm_cadt_sec));
	}
	if (vtsm_admap_sec > 0) {
		write_be32(buf, 0xDC, uint32_t(vtsm_admap_sec));
	}
	write_be32(buf, 0xE0, uint32_t(cadt_sec));
	write_be32(buf, 0xE4, uint32_t(admap_sec));

	copy_table(buf, ptt_sec * SECTOR, ptt_buf);
	copy_table(buf, pgcit_sec * SECTOR, pgcit_buf);
	copy_table(buf, cadt_sec * SECTOR, cadt_buf);
	copy_table(buf, admap_sec * SECTOR, admap_buf);
	copy_table(buf, vtsm_sec * SECTOR, vtsm_buf);
	if (vtsm_cadt_sec > 0) {
		copy_table(buf, vtsm_cadt_sec * SECTOR, vtsm_cadt);
	}
	if (vtsm_admap_sec > 0) {
		copy_table(buf, vtsm_admap_sec * SECTOR, vtsm_admap);
	}
	return buf;
}

Error mux_menu_chain(const Vector<Ref<InterDVDMenu>> &p_menus, const String &p_out_vob, bool p_title_domain, const String &p_ffmpeg, bool p_allow_dummy, bool p_auto_find, String *r_error, Vector<MenuPgcSpec> &r_specs, const Ref<InterDVDProject> &p_project, const Ref<InterDVDExportProgress> &p_progress) {
	r_specs.clear();
	if (p_menus.is_empty()) {
		return InterDVDVobMux::write_dummy_vob(p_out_vob);
	}
	uint8_t next_cell_id = 1;
	int part_n = 0;
	for (int i = 0; i < p_menus.size(); i++) {
		const Ref<InterDVDMenu> menu = p_menus[i];
		if (menu.is_null()) {
			continue;
		}
		const bool first_file = r_specs.is_empty();
		const String part_path = first_file ? p_out_vob : p_out_vob + vformat(".part%d", part_n++);
#ifdef TOOLS_ENABLED
		if (menu->get_cell().is_valid() && menu->get_cell()->get_packed_scene().is_valid()) {
			if (p_progress.is_valid()) {
				String name = menu->get_name();
				if (name.is_empty() && menu->get_cell().is_valid()) {
					name = menu->get_cell()->get_display_name();
				}
				if (name.is_empty()) {
					name = "menu";
				}
				p_progress->report(vformat("Baking %s", name));
			}
			const Error bake = InterDVDSceneBaker::bake_cell(menu->get_cell(), p_ffmpeg, p_auto_find, r_error);
			if (bake != OK) {
				return bake;
			}
		} else if (p_progress.is_valid()) {
			String name = menu->get_name();
			if (name.is_empty()) {
				name = "menu";
			}
			p_progress->report(vformat("Muxing %s", name));
		}
#endif
		Error err = OK;
		if (menu->get_cell().is_valid()) {
			err = InterDVDVobMux::mux_cell(menu->get_cell(), part_path, p_ffmpeg, p_allow_dummy, r_error, p_auto_find);
		} else {
			err = InterDVDVobMux::write_dummy_vob(part_path);
		}
		if (err != OK) {
			return err;
		}
		if (menu->get_buttons().size() > 0) {
#ifdef TOOLS_ENABLED
			if (menu->get_cell().is_valid() && menu->get_cell()->get_packed_scene().is_valid()) {
				Node *inst = menu->get_cell()->get_packed_scene()->instantiate();
				if (inst) {
					TypedArray<InterDVDButton> btns = menu->get_buttons();
					for (int b = 0; b < btns.size(); b++) {
						Ref<InterDVDButton> btn = btns[b];
						if (btn.is_valid()) {
							const Rect2 fallback = menu->get_cell().is_valid() ? menu->get_cell()->get_default_highlight() : Rect2();
							btn->sync_highlight_from_scene(inst, fallback, InterDVDSettings::title_safe_bottom(p_project));
						}
					}
					inst->queue_free();
				}
			}
#endif
			int fosl = menu->get_forced_selected_button();
			if (fosl <= 0) {
				fosl = menu->get_default_button();
			}
			const double dur = resolve_playback_sec(menu->get_cell(), part_path, p_ffmpeg);
			const uint32_t hli_e = (menu->get_still_time() == 255) ? 0x3FFFFFFFu : (dur >= 2.0 ? uint32_t(MIN(dur * 90000.0, 1073741822.0)) : 0);
			err = InterDVDVobMux::apply_menu_buttons(part_path, menu->get_buttons(), p_title_domain, fosl, menu->get_forced_activated_button(), uint16_t(menu->get_button_group_mask()), hli_e, InterDVDSettings::title_safe_bottom(p_project));
			if (err != OK) {
				if (r_error) {
					*r_error = "Could not write menu highlight buttons.";
				}
				return err;
			}
		}
		const uint32_t offset = first_file ? 0 : InterDVDVobMux::sector_count(p_out_vob);
		const uint32_t secs = InterDVDVobMux::sector_count(part_path);
		Vector<uint32_t> local_vobus = InterDVDVobMux::scan_vobu_sectors(part_path);
		if (local_vobus.is_empty()) {
			local_vobus.push_back(0);
		}
		const int part_ac3 = InterDVDVobMux::count_ac3_streams(part_path);
		const int part_spu = InterDVDVobMux::count_spu_streams(part_path);
		const bool part_has_audio = InterDVDVobMux::contains_ac3(part_path);
		const double part_sec = resolve_playback_sec(menu->get_cell(), part_path, p_ffmpeg);
		if (!first_file) {
			err = InterDVDVobMux::append_vob(p_out_vob, part_path);
			if (err != OK) {
				return err;
			}
			err = InterDVDVobMux::reindex_nav_lbns(p_out_vob, offset, next_cell_id);
			if (err != OK) {
				return err;
			}
			Ref<DirAccess> rm = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			if (rm.is_valid()) {
				rm->remove(part_path);
			}
		}
		MenuPgcSpec spec;
		spec.menu_id = uint8_t(menu->get_menu_type());
		spec.still_time = uint8_t(CLAMP(menu->get_still_time(), 0, 255));
		spec.next_pgc = uint16_t(menu->get_next_pgc() > 0 ? menu->get_next_pgc() : 1);
		spec.prev_pgc = uint16_t(menu->get_prev_pgc() > 0 ? menu->get_prev_pgc() : spec.next_pgc);
		spec.playback_sec = part_sec;
		spec.has_audio = part_has_audio;
		spec.audio_streams = part_ac3;
		spec.subp_streams = part_spu;
		spec.clut = menu->get_clut();
		CellPlayback playback;
		playback.first_sector = offset;
		playback.last_sector = offset + (secs ? secs - 1 : 0);
		playback.last_vobu = offset + local_vobus[local_vobus.size() - 1];
		playback.cell_id = next_cell_id;
		playback.still_time = spec.still_time;
		playback.playback_sec = spec.playback_sec;
		spec.cells.push_back(playback);
		r_specs.push_back(spec);
		next_cell_id++;
	}
	if (r_specs.is_empty()) {
		return InterDVDVobMux::write_dummy_vob(p_out_vob);
	}
	return OK;
}

void patch_vmg_last_sector(Vector<uint8_t> &p_vmg, uint32_t p_vmg_last) {
	write_be32(p_vmg, 0x0C, p_vmg_last);
}

} //namespace

Error InterDVDIfoWriter::write_video_ts(const String &p_root, const Ref<InterDVDProject> &p_project, bool p_allow_dummy_vob, const String &p_ffmpeg, String *r_error, bool p_auto_find_ffmpeg, const Ref<InterDVDExportProgress> &p_progress) {
	Ref<InterDVDExportProgress> progress = p_progress;
	if (progress.is_null()) {
		progress.instantiate();
	}
	int step_total = 2;
	if (p_project.is_valid()) {
		const TypedArray<InterDVDMenu> menus = p_project->get_menus();
		for (int i = 0; i < menus.size(); i++) {
			if (Ref<InterDVDMenu>(menus[i]).is_valid()) {
				step_total++;
			}
		}
		const TypedArray<InterDVDPGC> titles = p_project->get_titles();
		for (int i = 0; i < titles.size(); i++) {
			const Ref<InterDVDPGC> title = titles[i];
			if (title.is_null()) {
				step_total++;
				continue;
			}
			const int cells = title->get_cells().size();
			step_total += cells > 0 ? cells : 1;
		}
	} else {
		step_total++;
	}
	step_total++;
	progress->begin(step_total);
	progress->report("Loading project");

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		return ERR_CANT_OPEN;
	}
	Error err = da->make_dir_recursive(p_root.path_join("VIDEO_TS"));
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		if (r_error) {
			*r_error = "Could not create VIDEO_TS.";
		}
		return err;
	}
	err = da->make_dir_recursive(p_root.path_join("AUDIO_TS"));
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		if (r_error) {
			*r_error = "Could not create AUDIO_TS.";
		}
		return err;
	}

	const String video_ts = p_root.path_join("VIDEO_TS");
	const String title_vob = video_ts.path_join("VTS_01_1.VOB");
	TypedArray<InterDVDPGC> title_pgcs;
	if (p_project.is_valid()) {
		title_pgcs = p_project->get_titles();
	}
	if (title_pgcs.is_empty()) {
		title_pgcs.push_back(Ref<InterDVDPGC>());
	}

	Vector<TitlePart> parts;
	int part_file_n = 0;
	uint8_t next_cell_id = 1;
	for (int i = 0; i < title_pgcs.size(); i++) {
		const Ref<InterDVDPGC> title_pgc = title_pgcs[i];
		TypedArray<InterDVDCell> title_cells;
		if (title_pgc.is_valid() && title_pgc->get_cells().size() > 0) {
			title_cells = title_pgc->get_cells();
		} else {
			title_cells.push_back(Ref<InterDVDCell>());
		}
		TitlePart part;
		part.has_audio = false;
		if (title_pgc.is_valid()) {
			append_commands(part.pre, title_pgc->get_pre_commands());
			append_commands(part.post, title_pgc->get_post_commands());
			part.still_time = uint8_t(CLAMP(title_pgc->get_still_time(), 0, 255));
			part.next_pgc = uint16_t(title_pgc->get_next_pgc());
			part.prev_pgc = uint16_t(title_pgc->get_prev_pgc());
			part.clut = title_pgc->get_clut();
		}
		if (title_pgc.is_valid() && title_pgc->get_buttons().size() > 0 && part.next_pgc == 0) {
			part.next_pgc = uint16_t(i + 1);
			if (part.prev_pgc == 0) {
				part.prev_pgc = uint16_t(i + 1);
			}
		} else if (part.post.is_empty() && !(title_pgc.is_valid() && title_pgc->get_buttons().size() > 0)) {
			const PackedByteArray call = InterDVDInstruction::encode_link(InterDVDInstruction::CALL_SS, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VMG, 2, nullptr);
			if (call.size() == 8) {
				const uint8_t bytes[8] = { call[0], call[1], call[2], call[3], call[4], call[5], call[6], call[7] };
				append_cmd8(part.post, bytes);
			}
		}

		{
		}

		int cell_cmd_index = 1;
		for (int c = 0; c < title_cells.size(); c++) {
			Ref<InterDVDCell> cell = title_cells[c];
			const bool first_file = parts.is_empty() && part.cells.is_empty();
			const String part_path = first_file ? title_vob : video_ts.path_join(vformat("VTS_01_1.part%d.vob", part_file_n++));
#ifdef TOOLS_ENABLED
			if (cell.is_valid() && cell->get_packed_scene().is_valid()) {
				if (progress.is_valid()) {
					String name = cell->get_display_name();
					if (name.is_empty() && title_pgc.is_valid()) {
						name = title_pgc->get_name();
					}
					if (name.is_empty()) {
						name = "title";
					}
					progress->report(vformat("Baking %s", name));
				}
				err = InterDVDSceneBaker::bake_cell(cell, p_ffmpeg, p_auto_find_ffmpeg, r_error);
				if (err != OK) {
					return err;
				}
			} else if (progress.is_valid()) {
				String name = cell.is_valid() ? cell->get_display_name() : String();
				if (name.is_empty() && title_pgc.is_valid()) {
					name = title_pgc->get_name();
				}
				if (name.is_empty()) {
					name = "title";
				}
				progress->report(vformat("Muxing %s", name));
			}
#endif
			err = InterDVDVobMux::mux_cell(cell, part_path, p_ffmpeg, p_allow_dummy_vob, r_error, p_auto_find_ffmpeg);
			if (err != OK) {
				return err;
			}
			const int ac3_n = InterDVDVobMux::count_ac3_streams(part_path);
			const int spu_n = InterDVDVobMux::count_spu_streams(part_path);
			if (ac3_n > 0) {
				part.has_audio = true;
				part.audio_streams = MAX(part.audio_streams, ac3_n);
			}
			part.subp_streams = MAX(part.subp_streams, spu_n);
			if (cell.is_valid()) {
				const uint16_t def_lang = p_project.is_valid() ? InterDVDProject::language_be16(p_project->get_audio_language()) : uint16_t(0x656E);
				if (part.audio_langs.is_empty() && (cell->get_include_audio() || !cell->get_audio_path().is_empty())) {
					part.audio_langs.push_back(def_lang);
				}
				const TypedArray<InterDVDStream> extra = cell->get_streams();
				for (int s = 0; s < extra.size(); s++) {
					const Ref<InterDVDStream> stream = extra[s];
					if (stream.is_valid() && stream->is_enabled() && stream->get_kind() == InterDVDStream::KIND_AUDIO) {
						part.audio_langs.push_back(InterDVDProject::language_be16(stream->get_language()));
					}
				}
			}

#ifdef TOOLS_ENABLED
			if (cell.is_valid() && cell->get_packed_scene().is_valid() && title_pgc.is_valid() && c == 0) {
				Node *inst = cell->get_packed_scene()->instantiate();
				if (inst) {
					TypedArray<InterDVDButton> btns = title_pgc->get_buttons();
					for (int b = 0; b < btns.size(); b++) {
						Ref<InterDVDButton> btn = btns[b];
						if (btn.is_valid()) {
							const Rect2 fallback = cell.is_valid() ? cell->get_default_highlight() : Rect2();
							btn->sync_highlight_from_scene(inst, fallback, InterDVDSettings::title_safe_bottom(p_project));
						}
					}
					inst->queue_free();
				}
			}
#endif
			if (title_pgc.is_valid() && title_pgc->get_buttons().size() > 0 && c == 0) {
				const double dur = resolve_playback_sec(cell, part_path, p_ffmpeg);
				const uint32_t hli_e = part.still_time == 255 ? 0x3FFFFFFFu : (dur >= 2.0 ? uint32_t(MIN(dur * 90000.0, 1073741822.0)) : 0);
				err = InterDVDVobMux::apply_menu_buttons(part_path, title_pgc->get_buttons(), true, title_pgc->get_default_button(), 0, 0x1000, hli_e, InterDVDSettings::title_safe_bottom(p_project));
				if (err != OK) {
					if (r_error) {
						*r_error = "Could not write title highlight buttons into the title VOB.";
					}
					return err;
				}
			}

			const uint32_t offset = first_file ? 0 : InterDVDVobMux::sector_count(title_vob);
			const uint32_t secs = InterDVDVobMux::sector_count(part_path);
			Vector<uint32_t> local_vobus = InterDVDVobMux::scan_vobu_sectors(part_path);
			if (local_vobus.is_empty()) {
				local_vobus.push_back(0);
			}
			const double cell_sec = resolve_playback_sec(cell, part_path, p_ffmpeg);
			if (!first_file) {
				err = InterDVDVobMux::append_vob(title_vob, part_path);
				if (err != OK) {
					return err;
				}
				err = InterDVDVobMux::reindex_nav_lbns(title_vob, offset, next_cell_id);
				if (err != OK) {
					return err;
				}
				Ref<DirAccess> rm = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
				if (rm.is_valid()) {
					rm->remove(part_path);
				}
			}
			CellPlayback playback;
			playback.first_sector = offset;
			playback.last_sector = offset + (secs ? secs - 1 : 0);
			playback.last_vobu = offset + local_vobus[local_vobus.size() - 1];
			playback.cell_id = next_cell_id;

			const bool has_menu_buttons = title_pgc.is_valid() && title_pgc->get_buttons().size() > 0;
			if (has_menu_buttons) {
				playback.still_time = (c == 0) ? part.still_time : 0;
			} else {
				playback.still_time = (c == title_cells.size() - 1) ? part.still_time : 0;
			}
			playback.playback_sec = cell_sec;
			if (cell.is_valid() && cell->get_post_commands().size() > 0) {
				append_commands(part.cell_cmds, cell->get_post_commands());
				playback.cell_cmd = uint8_t(cell_cmd_index);
				cell_cmd_index += cell->get_post_commands().size();
			} else if (c + 1 < title_cells.size()) {
				const PackedByteArray link = InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PGN, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, c + 2, nullptr);
				if (link.size() == 8) {
					const uint8_t bytes[8] = { link[0], link[1], link[2], link[3], link[4], link[5], link[6], link[7] };
					append_cmd8(part.cell_cmds, bytes);
					playback.cell_cmd = uint8_t(cell_cmd_index);
					cell_cmd_index++;
				}
			}
			if (c == 0) {
				part.playback_sec = playback.playback_sec;
			} else {
				part.playback_sec += playback.playback_sec;
			}
			part.cells.push_back(playback);
			next_cell_id++;
		}
		parts.push_back(part);
	}


	Vector<Ref<InterDVDMenu>> title_menus;
	Vector<Ref<InterDVDMenu>> vtsm_menus;
	if (p_project.is_valid()) {
		const TypedArray<InterDVDMenu> menus = p_project->get_menus();
		for (int i = 0; i < menus.size(); i++) {
			const Ref<InterDVDMenu> one = menus[i];
			if (one.is_null()) {
				continue;
			}
			if (one->get_menu_type() == InterDVDMenu::MENU_TITLE) {
				title_menus.push_back(one);
			} else {
				vtsm_menus.push_back(one);
			}
		}
	}
	bool has_buttons = false;
	for (int i = 0; i < title_menus.size(); i++) {
		if (title_menus[i].is_valid() && title_menus[i]->get_buttons().size() > 0) {
			has_buttons = true;
			break;
		}
	}
	const String menu_vob = video_ts.path_join("VIDEO_TS.VOB");
	Vector<MenuPgcSpec> title_menu_specs;
	err = mux_menu_chain(title_menus, menu_vob, false, p_ffmpeg, p_allow_dummy_vob, p_auto_find_ffmpeg, r_error, title_menu_specs, p_project, progress);
	if (err != OK) {
		return err;
	}
	const String vtsm_vob = video_ts.path_join("VTS_01_0.VOB");
	Vector<MenuPgcSpec> vtsm_specs;
	err = mux_menu_chain(vtsm_menus, vtsm_vob, true, p_ffmpeg, p_allow_dummy_vob, p_auto_find_ffmpeg, r_error, vtsm_specs, p_project, progress);
	if (err != OK) {
		return err;
	}

	const uint32_t title_secs = InterDVDVobMux::sector_count(video_ts.path_join("VTS_01_1.VOB"));
	Vector<uint32_t> vobus = InterDVDVobMux::scan_vobu_sectors(video_ts.path_join("VTS_01_1.VOB"));
	if (vobus.is_empty()) {
		vobus.push_back(0);
	}

	Vector<uint8_t> fp_pre;
	if (p_project.is_valid() && p_project->get_first_play().is_valid()) {
		append_commands(fp_pre, p_project->get_first_play()->get_pre_commands());
	}
	if (fp_pre.is_empty()) {
		if (has_buttons) {
			append_default_jump_ss_title_menu(fp_pre);
		} else {
			append_default_jump_tt(fp_pre);
		}
	}

	DiscIdent ident;
	Vector<String> title_names;
	if (p_project.is_valid()) {
		ident.provider = p_project->get_provider_id();
		ident.disc_title = p_project->get_disc_title();
		ident.serial = p_project->get_serial().substr(0, 15);
		ident.menu_lang = InterDVDProject::language_be16(p_project->get_menu_language());
		ident.audio_lang = InterDVDProject::language_be16(p_project->get_audio_language());
		ident.subp_lang = InterDVDProject::language_be16(p_project->get_subtitle_language());
		for (int i = 0; i < title_pgcs.size(); i++) {
			const Ref<InterDVDPGC> named = title_pgcs[i];
			title_names.push_back(named.is_valid() ? named->get_name() : String());
		}
	}

	const int titles = MAX(parts.size(), 1);
	const int region = p_project.is_valid() ? p_project->get_region_mask() : 1;
	const uint32_t vmg_ifo = uint32_t(IFO_SECTORS);
	const uint32_t vmg_menu = MAX(InterDVDVobMux::sector_count(menu_vob), uint32_t(1));
	const uint32_t vts_start = vmg_ifo + vmg_menu + vmg_ifo;
	Vector<int> chapter_counts;
	for (int i = 0; i < parts.size(); i++) {
		chapter_counts.push_back(MAX(parts[i].cells.size(), 1));
	}
	const uint32_t vtsm_secs = MAX(InterDVDVobMux::sector_count(vtsm_vob), uint32_t(1));
	Vector<uint32_t> vtsm_vobus = InterDVDVobMux::scan_vobu_sectors(vtsm_vob);
	Vector<uint8_t> vmg = make_vmg_ifo(titles, region, vts_start, fp_pre, chapter_counts, title_menu_specs, ident, title_names);
	patch_vmg_last_sector(vmg, vmg_ifo + vmg_menu + vmg_ifo - 1);
	const Vector<uint8_t> vts = make_vts_ifo(title_secs, vobus, parts, ident, vtsm_specs, vtsm_secs, vtsm_vobus);

	progress->report("Writing IFO");
	err = write_file(video_ts.path_join("VIDEO_TS.IFO"), vmg);
	if (err != OK) {
		return err;
	}
	err = write_file(video_ts.path_join("VIDEO_TS.BUP"), vmg);
	if (err != OK) {
		return err;
	}
	err = write_file(video_ts.path_join("VTS_01_0.IFO"), vts);
	if (err != OK) {
		return err;
	}
	err = write_file(video_ts.path_join("VTS_01_0.BUP"), vts);
	if (err != OK) {
		return err;
	}
	progress->report("Finalizing VOB");
	return InterDVDVobMux::split_vts_vob(video_ts.path_join("VTS_01_1.VOB"));
}
