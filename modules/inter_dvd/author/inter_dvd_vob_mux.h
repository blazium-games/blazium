/**************************************************************************/
/*  inter_dvd_vob_mux.h                                                   */
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

#include "core/error/error_list.h"
#include "core/io/resource.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/typed_array.h"

class InterDVDButton;
class InterDVDCell;

class InterDVDVobMux {
public:
	static constexpr int SECTOR_SIZE = 2048;

	static String find_ffmpeg_on_path();
	static String resolve_ffmpeg(const String &p_configured, bool p_auto_find);
	static String find_ffmpeg(const String &p_configured);
	static Error write_dummy_vob(const String &p_path);
	static Error mux_cell(const Ref<InterDVDCell> &p_cell, const String &p_out_vob, const String &p_ffmpeg, bool p_allow_dummy, String *r_error = nullptr, bool p_auto_find_ffmpeg = true);
	static Error loop_extend_vob(const String &p_vob, double p_seconds, const String &p_ffmpeg, String *r_error = nullptr, bool p_auto_find_ffmpeg = true);
	static Error finalize_title_vob(const String &p_path);
	static Error apply_menu_buttons(const String &p_vob, const TypedArray<InterDVDButton> &p_buttons, bool p_title_domain = false, int p_forced_select = 0, int p_forced_activate = 0, uint16_t p_btn_colnfo = 0x1000, uint32_t p_hli_e_ptm = 0, int p_title_safe_bottom = -1);
	static Error append_vob(const String &p_dst, const String &p_src);
	static Error split_vts_vob(const String &p_vob_1);
	static Error reindex_nav_lbns(const String &p_vob, uint32_t p_from_sector, uint8_t p_cell_id);
	static Vector<uint32_t> scan_vobu_sectors(const String &p_vob);
	static uint32_t sector_count(const String &p_path);
	static bool contains_ac3(const String &p_path);
	static bool contains_mpeg_video(const String &p_path);
	static bool contains_spu(const String &p_path);
	static int count_ac3_streams(const String &p_path);
	static int count_spu_streams(const String &p_path);
	static double estimate_duration_sec(const String &p_vob);
	static double probe_media_sec(const String &p_path, const String &p_ffmpeg = String());
};
