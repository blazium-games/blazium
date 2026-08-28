/**************************************************************************/
/*  script_cook.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "script_cook.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace ps1_cook {

static String strip_line_comments(const String &p_source) {
	PackedStringArray lines = p_source.split("\n");
	String out;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		const int gd = line.find("#");
		const int lua = line.find("--");
		int cut = -1;
		if (gd >= 0 && lua >= 0) {
			cut = MIN(gd, lua);
		} else if (gd >= 0) {
			cut = gd;
		} else if (lua >= 0) {
			cut = lua;
		}
		if (cut >= 0) {
			line = line.substr(0, cut);
		}
		out += line + "\n";
	}
	return out;
}

bool source_is_rejected(const String &p_source, String &r_why) {
	const String src = strip_line_comments(p_source);
	if (src.contains("eval(")) {
		r_why = "eval(";
		return true;
	}
	if (src.contains("load(")) {
		r_why = "load(";
		return true;
	}
	if (src.contains("ClassDB.get_class_list")) {
		r_why = "ClassDB.get_class_list";
		return true;
	}
	return false;
}

static void walk_scripts(const String &p_dir, ScriptIR &r_out) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
		if (n == "." || n == ".." || n.begins_with(".")) {
			continue;
		}
		const String path = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (n == ".godot" || n == "export") {
				continue;
			}
			walk_scripts(path, r_out);
			continue;
		}
		if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
			continue;
		}
		const String src = FileAccess::get_file_as_string(path);
		String why;
		if (source_is_rejected(src, why)) {
			r_out.rejected.push_back(path + " : " + why);
		} else {
			r_out.accepted.push_back(path);
			if (src.contains("0.4") && (src.contains("rotate_y") || src.contains("delta * 0.4"))) {
				r_out.rotate_y = 0.4f;
			}
		}
	}
}

Error cook_scripts(const String &p_export_dir, ScriptIR &r_out) {
	walk_scripts("res://", r_out);
	r_out.map_text = vformat("IR rotate_y=%f (GDScript+Luau)\n", r_out.rotate_y);
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.map_text += "OK " + r_out.accepted[i] + "\n";
	}
	for (int i = 0; i < r_out.rejected.size(); i++) {
		r_out.map_text += "REJECT " + r_out.rejected[i] + "\n";
	}
	Ref<FileAccess> f = FileAccess::open(p_export_dir.path_join("SCRIPT_IR.txt"), FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	f->store_string(r_out.map_text);
	return OK;
}

Error write_script_ir(const String &p_export_dir, const ScriptIR &p_ir) {
	Vector<uint8_t> bytes;
	union {
		float f;
		uint32_t u;
	} rot;
	rot.f = p_ir.rotate_y;
	bytes.push_back(uint8_t(rot.u & 0xff));
	bytes.push_back(uint8_t((rot.u >> 8) & 0xff));
	bytes.push_back(uint8_t((rot.u >> 16) & 0xff));
	bytes.push_back(uint8_t((rot.u >> 24) & 0xff));
	auto i16 = [&](int16_t v) {
		bytes.push_back(uint8_t(uint16_t(v) & 0xff));
		bytes.push_back(uint8_t(uint16_t(v) >> 8));
	};
	i16(p_ir.pos_x);
	i16(p_ir.pos_y);
	i16(p_ir.pos_z);
	i16(p_ir.geom_screen);
	i16(int16_t(p_ir.flags));
	i16(int16_t(p_ir.light_count));
	for (int i = 0; i < 3; i++) {
		i16(p_ir.lights[i].dx);
		i16(p_ir.lights[i].dy);
		i16(p_ir.lights[i].dz);
		bytes.push_back(p_ir.lights[i].r);
		bytes.push_back(p_ir.lights[i].g);
		bytes.push_back(p_ir.lights[i].b);
		bytes.push_back(0);
	}
	Ref<FileAccess> ir = FileAccess::open(p_export_dir.path_join("SCRIPT.IR"), FileAccess::WRITE);
	ERR_FAIL_COND_V(ir.is_null(), ERR_CANT_CREATE);
	ir->store_buffer(bytes);
	return OK;
}

} // namespace ps1_cook
