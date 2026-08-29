/**************************************************************************/
/*  export_plugin.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "export_plugin.h"

#include "cook/cooker.h"
#include "logo_svg.gen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"
#include "core/templates/pair.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "modules/svg/image_loader_svg.h"

bool EditorExportPlatformPS1::_host_supported() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	return true;
#else
	return false;
#endif
}

String EditorExportPlatformPS1::_find_toolchain() const {
	if (EditorSettings::get_singleton()) {
		const String from_settings = EditorSettings::get_singleton()->get("export/ps1/toolchain");
		if (!from_settings.is_empty() && FileAccess::exists(from_settings)) {
			return from_settings;
		}
	}
	const String from_env = OS::get_singleton()->get_environment("BLAZIUM_TOOLCHAIN");
	if (!from_env.is_empty() && FileAccess::exists(from_env)) {
		return from_env;
	}

	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
#ifdef WINDOWS_ENABLED
	const String names[] = { "blazium-toolchain.exe", "blazium-toolchain" };
#else
	const String names[] = { "blazium-toolchain", "blazium-toolchain.exe" };
#endif
	PackedStringArray roots;
	roots.push_back(exe_dir);
	roots.push_back(exe_dir.path_join("..").simplify_path());
	roots.push_back(exe_dir.path_join("../..").simplify_path());
	roots.push_back(exe_dir.path_join("../../blazium-toolchain").simplify_path());
	roots.push_back(exe_dir.path_join("../../../blazium-toolchain").simplify_path());

	for (int i = 0; i < roots.size(); i++) {
		for (const String &name : names) {
			const String cand = roots[i].path_join(name);
			if (FileAccess::exists(cand)) {
				return cand;
			}
		}
	}
	return OS::get_singleton()->get_executable_path().get_base_dir().path_join(names[0]);
}

String EditorExportPlatformPS1::_optional_runtime_src() const {
	if (!EditorSettings::get_singleton()) {
		return String();
	}
	const String from_settings = EditorSettings::get_singleton()->get("export/ps1/runtime_src");
	if (!from_settings.is_empty() && FileAccess::exists(from_settings.path_join("CMakeLists.txt"))) {
		return from_settings;
	}
	return String();
}

Dictionary EditorExportPlatformPS1::_toolchain_status() const {
	List<String> args;
	args.push_back("--json");
	args.push_back("ps1");
	args.push_back("status");
	String output;
	int exitcode = 0;
	if (_spawn_toolchain(args, output, exitcode) != OK || exitcode != 0) {
		return Dictionary();
	}
	const Variant parsed = JSON::parse_string(output.strip_edges());
	if (parsed.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return parsed;
}

Error EditorExportPlatformPS1::_ensure_toolchain(const String &p_profile, String &r_error) {
	Dictionary st = _toolchain_status();
	const bool compile_ready = st.get("compile_ready", false);
	const bool dest_ready = st.get("dev_ready", false);
	const bool need_setup = (p_profile == "dev") ? (!compile_ready || !dest_ready) : !compile_ready;
	if (need_setup) {
		add_message(EXPORT_MESSAGE_INFO, "PS1", "Running blazium-toolchain ps1 setup --profile " + p_profile + " (first export may download gcc/SDK).");
		List<String> args;
		args.push_back("ps1");
		args.push_back("setup");
		args.push_back("--profile");
		args.push_back(p_profile);
		String output;
		int exitcode = 0;
		if (_spawn_toolchain(args, output, exitcode) != OK || exitcode != 0) {
			r_error = "blazium-toolchain ps1 setup failed:\n" + output;
			return FAILED;
		}
		st = _toolchain_status();
	}
	const int abi = int(st.get("guest_abi", 0));
	if (abi != ps1_cook::kCookABI) {
		r_error = vformat("blazium-toolchain guest_abi is %d; this editor cooks ABI %d. Update the toolchain.", abi, ps1_cook::kCookABI);
		return ERR_UNCONFIGURED;
	}
	return OK;
}

bool EditorExportPlatformPS1::_scan_forbidden(const String &p_dir, String &r_hit) const {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return false;
	}
	da->list_dir_begin();
	for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
		if (n == "." || n == ".." || n.begins_with(".")) {
			continue;
		}
		const String path = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (_scan_forbidden(path, r_hit)) {
				return true;
			}
			continue;
		}
		if (n.ends_with(".gdextension") || n.ends_with(".cs") || n.ends_with(".csproj")) {
			r_hit = path;
			return true;
		}
	}
	return false;
}

Error EditorExportPlatformPS1::_spawn_toolchain(const List<String> &p_args, String &r_output, int &r_exit) const {
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_output = "blazium-toolchain not found (set export/ps1/toolchain or BLAZIUM_TOOLCHAIN)";
		return ERR_FILE_NOT_FOUND;
	}
	return OS::get_singleton()->execute(tc, p_args, &r_output, &r_exit, true);
}

void EditorExportPlatformPS1::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("ps1");
}

void EditorExportPlatformPS1::get_platform_features(List<String> *r_features) const {
	r_features->push_back("ps1");
}

void EditorExportPlatformPS1::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps1/output_kind", PROPERTY_HINT_ENUM, "PS-X EXE"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps1/region", PROPERTY_HINT_ENUM, "NTSC,PAL"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps1/width", PROPERTY_HINT_ENUM, "320,256,512,640"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ps1/make_iso"), false));
}

List<String> EditorExportPlatformPS1::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("exe");
	return list;
}

int EditorExportPlatformPS1::get_options_count() const {
	return _host_supported() ? 1 : 0;
}

bool EditorExportPlatformPS1::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	r_missing_templates = false;
	if (!_host_supported()) {
		r_error += "PS1 export is available on Windows and Linux hosts only.\n";
		return false;
	}
	bool valid = true;
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_error += "blazium-toolchain is missing. Install the official CLI and set Editor Settings → Export → PS1 → Toolchain (or BLAZIUM_TOOLCHAIN).\n";
		valid = false;
	}
	return valid;
}

bool EditorExportPlatformPS1::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String hit;
	if (_scan_forbidden("res://", hit)) {
		r_error += "PS1 export forbids GDExtension and C# (" + hit + ").\n";
		return false;
	}
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_error += "PS1 export requires application/run/main_scene.\n";
		return false;
	}
	r_error += "Note: audio is WAV-only (else a beep). Display is NTSC/PAL at 256/320/512/640.\n";
	r_error += "Note: cook keeps first 8192 triangles and first 16 albedo TIMs per pack. Guest VM: first-slice opcodes (24 ISO packs, PS1.* RAM/load APIs, budgeted instantiate, InputMap pad, 16-slot timeout/pressed/anim connect, PAL 50 Hz) + Luau tape (official Luau is not interpreted).\n";
	return true;
}

Error EditorExportPlatformPS1::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "PS1 export is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}

	String hit;
	if (_scan_forbidden("res://", hit)) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "GDExtension and C# are not allowed (" + hit + ").");
		return ERR_UNCONFIGURED;
	}

	String setup_err;
	if (_ensure_toolchain("compile", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", setup_err);
		return FAILED;
	}

	const String src = _optional_runtime_src();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(p_path.get_base_dir());

	ps1_cook::CookOptions opts;
	static const int widths[] = { 320, 256, 512, 640 };
	const int wsel = int(p_preset->get("ps1/width"));
	opts.width = widths[CLAMP(wsel, 0, 3)];
	opts.region = int(p_preset->get("ps1/region")) == 1 ? 1 : 0;
	ps1_cook::CookResult cooked;
	const Error cook_err = ps1_cook::cook_project(p_path.get_base_dir(), cooked, opts);
	_emit_cook_messages(cooked);
	if (cook_err != OK || !cooked.errors.is_empty()) {
		if (cooked.errors.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, "PS1", cooked.report.is_empty() ? String("Cook failed.") : cooked.report);
		}
		return cook_err != OK ? cook_err : FAILED;
	}

	List<String> args;
	args.push_back("ps1");
	args.push_back("build");
	if (!src.is_empty()) {
		args.push_back("--src");
		args.push_back(src);
	}
	args.push_back("--out");
	args.push_back(p_path);
	if (!cooked.tim_path.is_empty()) {
		args.push_back("--tim");
		args.push_back(cooked.tim_path);
	}
	if (!cooked.mesh_path.is_empty()) {
		args.push_back("--mesh");
		args.push_back(cooked.mesh_path);
	}
	if (!cooked.vag_path.is_empty()) {
		args.push_back("--vag");
		args.push_back(cooked.vag_path);
	}
	if (!cooked.sprite_path.is_empty()) {
		args.push_back("--sprite");
		args.push_back(cooked.sprite_path);
	}
	if (!cooked.script_path.is_empty()) {
		args.push_back("--script");
		args.push_back(cooked.script_path);
	}
	if (!cooked.gdbc_path.is_empty()) {
		args.push_back("--gdbc");
		args.push_back(cooked.gdbc_path);
	}
	if (!cooked.luau_path.is_empty()) {
		args.push_back("--luau");
		args.push_back(cooked.luau_path);
	}
	if (!cooked.str_path.is_empty()) {
		args.push_back("--str");
		args.push_back(cooked.str_path);
	}
	if (!cooked.xa_path.is_empty()) {
		args.push_back("--xa");
		args.push_back(cooked.xa_path);
	}
	if (!cooked.node_path.is_empty()) {
		args.push_back("--node");
		args.push_back(cooked.node_path);
	}
	if (!cooked.hud_path.is_empty()) {
		args.push_back("--hud");
		args.push_back(cooked.hud_path);
	}
	if (!cooked.tile_path.is_empty()) {
		args.push_back("--tile");
		args.push_back(cooked.tile_path);
	}
	if (!cooked.scene_path.is_empty()) {
		args.push_back("--scene");
		args.push_back(cooked.scene_path);
	}
	if (!cooked.anim_path.is_empty()) {
		args.push_back("--anim");
		args.push_back(cooked.anim_path);
	}
	if (int(p_preset->get("ps1/output_kind")) != 0) {
		add_message(EXPORT_MESSAGE_WARNING, "PS1", "ps1/output_kind is unread; output is PS-X EXE. Use ps1/make_iso for a disc image.");
	}

	String output;
	int exitcode = 0;
	const Error err = _spawn_toolchain(args, output, exitcode);
	if (err != OK || exitcode != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "blazium-toolchain ps1 build failed:\n" + output);
		return FAILED;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "Export path was not written: " + p_path);
		return ERR_CANT_CREATE;
	}
	const Vector<uint8_t> magic_bytes = f->get_buffer(8);
	String magic;
	magic.parse_utf8((const char *)magic_bytes.ptr(), magic_bytes.size());
	if (magic != "PS-X EXE") {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "Output is not a PS-X EXE (got '" + magic + "').");
		return FAILED;
	}
	add_message(EXPORT_MESSAGE_INFO, "PS1", "Wrote PS-X EXE " + p_path);

	if (bool(p_preset->get("ps1/make_iso"))) {
		add_message(EXPORT_MESSAGE_WARNING, "PS1", "ISO is SYSTEM.CNF + GAME.EXE + optional FMV/XA + export/packs; pack 0 TEX/MESH/SCRIPT/VAG/SPRITE/HUD/TILE stay embedded in the EXE.");
		String iso_warn;
		if (_try_make_iso(p_path, cooked, iso_warn) != OK) {
			add_message(EXPORT_MESSAGE_WARNING, "PS1", iso_warn.is_empty() ? String("ISO skipped because mkpsxiso is missing.") : iso_warn);
		}
	}
	return OK;
}

void EditorExportPlatformPS1::_emit_cook_messages(const ps1_cook::CookResult &p_cooked) {
	for (int i = 0; i < p_cooked.errors.size(); i++) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", p_cooked.errors[i]);
	}
	for (int i = 0; i < p_cooked.warnings.size(); i++) {
		add_message(EXPORT_MESSAGE_WARNING, "PS1", p_cooked.warnings[i]);
	}
	for (int i = 0; i < p_cooked.infos.size(); i++) {
		add_message(EXPORT_MESSAGE_INFO, "PS1", p_cooked.infos[i]);
	}
	if (!p_cooked.report.is_empty()) {
		add_message(EXPORT_MESSAGE_INFO, "PS1", p_cooked.report);
	}
}

Error EditorExportPlatformPS1::_try_make_iso(const String &p_exe_path, const ps1_cook::CookResult &p_cooked, String &r_warning) {
	const String dir = p_exe_path.get_base_dir();
	const String cnf = dir.path_join("SYSTEM.CNF");
	const String xml = dir.path_join("iso.xml");
	const String iso_out = dir.path_join(p_exe_path.get_file().get_basename() + ".bin");

	Ref<FileAccess> cf = FileAccess::open(cnf, FileAccess::WRITE);
	if (cf.is_null()) {
		r_warning = "Could not write SYSTEM.CNF for ISO.";
		return FAILED;
	}
	cf->store_string("BOOT = cdrom:\\GAME.EXE;1\nTCB = 4\nEVENT = 10\nSTACK = 801FFFF0\n");

	Ref<FileAccess> xf = FileAccess::open(xml, FileAccess::WRITE);
	if (xf.is_null()) {
		r_warning = "Could not write iso.xml.";
		return FAILED;
	}
	xf->store_string(
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<iso_project>\n"
			"\t<track type=\"data\">\n"
			"\t\t<identifiers system=\"PLAYSTATION\" application=\"PLAYSTATION\" volume=\"BLAZIUM\" volume_set=\"BLAZIUM\" publisher=\"BLAZIUM\" data_preparer=\"BLAZIUM\" />\n"
			"\t\t<directory_tree>\n"
			"\t\t\t<file name=\"SYSTEM.CNF\" source=\"" +
			cnf.xml_escape(true) + "\" type=\"data\" />\n"
			"\t\t\t<file name=\"GAME.EXE\" source=\"" +
			p_exe_path.xml_escape(true) + "\" type=\"data\" />\n");
	auto add_file = [&](const String &name, const String &src) {
		if (!src.is_empty() && FileAccess::exists(src)) {
			xf->store_string("\t\t\t<file name=\"" + name + "\" source=\"" + src.xml_escape(true) + "\" type=\"data\" />\n");
		}
	};
	add_file("FMV00.STR", p_cooked.str_path);
	add_file("FMV00.XA", p_cooked.xa_path);
	add_file("NODE00.bin", p_cooked.node_path);
	HashMap<String, Vector<String>> pack_dirs;
	for (int i = 0; i < p_cooked.extra_pack_files.size(); i++) {
		const String src = p_cooked.extra_pack_files[i];
		if (src.is_empty() || !FileAccess::exists(src)) {
			continue;
		}
		const String nn = src.get_base_dir().get_file();
		if (!pack_dirs.has(nn)) {
			pack_dirs[nn] = Vector<String>();
		}
		pack_dirs[nn].push_back(src);
	}
	if (!pack_dirs.is_empty()) {
		xf->store_string("\t\t\t<dir name=\"PACKS\">\n");
		for (const KeyValue<String, Vector<String>> &E : pack_dirs) {
			xf->store_string("\t\t\t\t<dir name=\"" + E.key + "\">\n");
			for (int f = 0; f < E.value.size(); f++) {
				const String src = E.value[f];
				xf->store_string("\t\t\t\t\t<file name=\"" + src.get_file().to_upper() + "\" source=\"" + src.xml_escape(true) + "\" type=\"data\" />\n");
			}
			xf->store_string("\t\t\t\t</dir>\n");
		}
		xf->store_string("\t\t\t</dir>\n");
	}
	xf->store_string(
			"\t\t</directory_tree>\n"
									  "\t</track>\n"
									  "</iso_project>\n");

	List<String> args;
	args.push_back("ps1");
	args.push_back("iso");
	args.push_back("--xml");
	args.push_back(xml);
	args.push_back("--out");
	args.push_back(iso_out);
	String output;
	int exitcode = 0;
	const Error err = _spawn_toolchain(args, output, exitcode);
	if (err != OK || exitcode != 0) {
		r_warning = "ISO skipped because mkpsxiso is missing or failed.\n" + output;
		return FAILED;
	}
	add_message(EXPORT_MESSAGE_INFO, "PS1", "Wrote ISO " + iso_out);
	return OK;
}

Error EditorExportPlatformPS1::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "PS1 One-click is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}
	String path = p_preset->get_export_path();
	if (path.is_empty()) {
		path = ProjectSettings::get_singleton()->globalize_path("res://export/Game.exe");
	}
	if (!FileAccess::exists(path)) {
		const Error exp = export_project(p_preset, true, path, p_debug_flags);
		if (exp != OK) {
			return exp;
		}
	}
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", "blazium-toolchain not found for One-click.");
		return ERR_FILE_NOT_FOUND;
	}
	String setup_err;
	if (_ensure_toolchain("dev", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "PS1", setup_err);
		return FAILED;
	}
	List<String> args;
	args.push_back("ps1");
	args.push_back("run");
	args.push_back("--timeout");
	args.push_back("120s");
	args.push_back(path);
	return OS::get_singleton()->create_process(tc, args);
}

EditorExportPlatformPS1::EditorExportPlatformPS1() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);
		ImageLoaderSVG::create_image_from_string(img, _ps1_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);
	}
}
