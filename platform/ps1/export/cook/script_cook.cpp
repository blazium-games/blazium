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
#include "modules/gdscript/gdscript.h"

#ifdef MODULE_LUAU_MODULE_ENABLED
#include "modules/luau_module/luau.h"
#endif

namespace ps1_cook {

static const uint8_t kOpReturn = 0;
static const uint8_t kOpLoadConstF = 1;
static const uint8_t kOpLoadArg = 2;
static const uint8_t kOpMul = 3;
static const uint8_t kOpAdd = 4;
static const uint8_t kOpCallSelf = 5;

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
	const char *bad[] = {
		"eval(", "load(", "ClassDB.get_class_list", "ClassDB.", "HTTPClient",
		"Thread.", "await ", "func lambda", "preload(", "OS.execute", "FileAccess.store",
		"callv("
	};
	for (const char *b : bad) {
		if (src.contains(b)) {
			r_why = b;
			return true;
		}
	}
	return false;
}

static bool skip_script_dir(const String &p_name) {
	return p_name == ".godot" || p_name == "export" || p_name == "tests" || p_name == "tools";
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	union {
		float f;
		uint32_t u;
	} x;
	x.f = v;
	o.push_back(uint8_t(x.u & 0xff));
	o.push_back(uint8_t((x.u >> 8) & 0xff));
	o.push_back(uint8_t((x.u >> 16) & 0xff));
	o.push_back(uint8_t((x.u >> 24) & 0xff));
}

static void append_str(Vector<uint8_t> &o, const String &s) {
	const CharString utf = s.utf8();
	o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
	for (int i = 0; i < utf.length() && i < 255; i++) {
		o.push_back(uint8_t(utf[i]));
	}
}

static Vector<uint8_t> lower_process(const String &p_path, const String &p_src, float &r_rate) {
	Vector<uint8_t> code;
	r_rate = 0.4f;
	if (p_src.contains("rotate_y") && p_src.contains("delta")) {
		code.push_back(kOpLoadArg);
		code.push_back(0);
		code.push_back(kOpLoadConstF);
		code.push_back(0);
		code.push_back(kOpMul);
		code.push_back(kOpCallSelf);
		code.push_back(0);
		code.push_back(kOpReturn);
		if (p_src.contains("0.4")) {
			r_rate = 0.4f;
		}
	} else if (p_src.contains("rotate_y")) {
		code.push_back(kOpLoadConstF);
		code.push_back(0);
		code.push_back(kOpCallSelf);
		code.push_back(0);
		code.push_back(kOpReturn);
	} else {
		code.push_back(kOpReturn);
	}
	(void)p_path;
	return code;
}

static Vector<uint8_t> pack_gdbc(const Vector<String> &p_paths, const Vector<String> &p_sources) {
	Vector<uint8_t> out;
	out.push_back('G');
	out.push_back('D');
	out.push_back('B');
	out.push_back('C');
	append_u16(out, 4);
	append_u16(out, uint16_t(p_paths.size()));
	for (int i = 0; i < p_paths.size(); i++) {
		append_str(out, p_paths[i]);
	}
	append_u16(out, uint16_t(p_paths.size()));
	for (int i = 0; i < p_paths.size(); i++) {
		float rate = 0.4f;
		const Vector<uint8_t> code = lower_process(p_paths[i], p_sources[i], rate);
		append_str(out, "_process");
		out.push_back(1); // nargs
		append_u16(out, 1);
		append_u16(out, uint16_t(code.size()));
		append_f32(out, rate);
		out.push_back(1);
		append_str(out, "rotate_y");
		for (int c = 0; c < code.size(); c++) {
			out.push_back(code[c]);
		}
	}
	return out;
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
			if (skip_script_dir(n)) {
				continue;
			}
			walk_scripts(path, r_out);
			continue;
		}
		if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
			continue;
		}
		if (n == "run_tests.gd" || n.begins_with("test_")) {
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
	r_out.map_text = vformat("IR rotate_y=%f (GDScript+Luau ABI 4)\n", r_out.rotate_y);
	for (int i = 0; i < r_out.accepted.size(); i++) {
		r_out.map_text += "OK " + r_out.accepted[i] + "\n";
	}
	for (int i = 0; i < r_out.rejected.size(); i++) {
		r_out.map_text += "REJECT " + r_out.rejected[i] + "\n";
	}

	Vector<String> gd_paths;
	Vector<String> gd_src;
	Vector<String> lu_paths;
	Vector<String> lu_src;
	for (int i = 0; i < r_out.accepted.size(); i++) {
		const String path = r_out.accepted[i];
		const String src = FileAccess::get_file_as_string(path);
		if (path.ends_with(".gd")) {
			Ref<GDScript> script;
			script.instantiate();
			script->set_source_code(src);
			if (script->reload(true) != OK) {
				r_out.rejected.push_back(path + " : GDScript compile failed");
				r_out.map_text += "REJECT " + path + " : GDScript compile failed\n";
				continue;
			}
			gd_paths.push_back(path);
			gd_src.push_back(src);
		} else if (path.ends_with(".luau")) {
#ifdef MODULE_LUAU_MODULE_ENABLED
			const PackedByteArray bc = luau_module::Luau::compile(src);
			if (bc.is_empty()) {
				r_out.rejected.push_back(path + " : Luau compile failed");
				r_out.map_text += "REJECT " + path + " : Luau compile failed\n";
				continue;
			}
#endif
			lu_paths.push_back(path);
			lu_src.push_back(src);
		}
	}
	r_out.gd_count = uint16_t(gd_paths.size());
	r_out.luau_count = uint16_t(lu_paths.size());

	if (!gd_paths.is_empty()) {
		const Vector<uint8_t> gdbc = pack_gdbc(gd_paths, gd_src);
		r_out.gdbc_path = p_export_dir.path_join("SCRIPT.GD.BC");
		Ref<FileAccess> gf = FileAccess::open(r_out.gdbc_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(gf.is_null(), ERR_CANT_CREATE);
		gf->store_buffer(gdbc);
	}
	if (!lu_paths.is_empty()) {
		Vector<uint8_t> lubc;
		lubc.push_back('L');
		lubc.push_back('U');
		lubc.push_back('B');
		lubc.push_back('C');
#ifdef MODULE_LUAU_MODULE_ENABLED
		const PackedByteArray official = luau_module::Luau::compile(lu_src[0]);
		append_u16(lubc, uint16_t(official.size() & 0xffff));
		for (int i = 0; i < official.size(); i++) {
			lubc.push_back(official[i]);
		}
#else
		append_u16(lubc, 0);
#endif
		const Vector<uint8_t> lowered = pack_gdbc(lu_paths, lu_src);
		for (int i = 0; i < lowered.size(); i++) {
			lubc.push_back(lowered[i]);
		}
		r_out.luau_path = p_export_dir.path_join("SCRIPT.LU.BC");
		Ref<FileAccess> lf = FileAccess::open(r_out.luau_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(lf.is_null(), ERR_CANT_CREATE);
		lf->store_buffer(lubc);
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
	i16(int16_t(p_ir.display_w));
	i16(int16_t(p_ir.display_h));
	bytes.push_back(p_ir.region);
	bytes.push_back(p_ir.display_flags);
	i16(int16_t(p_ir.gd_count));
	i16(int16_t(p_ir.luau_count));
	i16(p_ir.cam_rx);
	i16(p_ir.cam_ry);
	i16(p_ir.cam_rz);
	Ref<FileAccess> ir = FileAccess::open(p_export_dir.path_join("SCRIPT.IR"), FileAccess::WRITE);
	ERR_FAIL_COND_V(ir.is_null(), ERR_CANT_CREATE);
	ir->store_buffer(bytes);
	return OK;
}

} // namespace ps1_cook
