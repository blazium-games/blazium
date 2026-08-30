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
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "modules/svg/image_loader_svg.h"

// Must match blazium-toolchain/guest/ps2 CookABI. Cook writers land in P3.
static const int kGuestABI = 1;

bool EditorExportPlatformPS2::_host_supported() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	return true;
#else
	return false;
#endif
}

String EditorExportPlatformPS2::_find_toolchain() const {
	if (EditorSettings::get_singleton()) {
		const String from_settings = EditorSettings::get_singleton()->get("export/ps2/toolchain");
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

String EditorExportPlatformPS2::_optional_runtime_src() const {
	if (!EditorSettings::get_singleton()) {
		return String();
	}
	const String from_settings = EditorSettings::get_singleton()->get("export/ps2/runtime_src");
	if (from_settings.is_empty()) {
		return String();
	}
	if (FileAccess::exists(from_settings.path_join("CMakeLists.txt")) || FileAccess::exists(from_settings.path_join("Makefile"))) {
		return from_settings;
	}
	return String();
}

String EditorExportPlatformPS2::_optional_runtime_overlay() const {
	if (OS::get_singleton() && OS::get_singleton()->has_environment("BLAZIUM_PS2_OVERLAY")) {
		const String from_env = OS::get_singleton()->get_environment("BLAZIUM_PS2_OVERLAY");
		if (!from_env.is_empty() && DirAccess::dir_exists_absolute(from_env)) {
			return from_env;
		}
	}
	if (EditorSettings::get_singleton()) {
		const String from_settings = EditorSettings::get_singleton()->get("export/ps2/runtime_overlay");
		if (!from_settings.is_empty() && DirAccess::dir_exists_absolute(from_settings)) {
			return from_settings;
		}
	}
	if (ProjectSettings::get_singleton()) {
		const String project_guest = ProjectSettings::get_singleton()->globalize_path("res://ps2_guest");
		if (!project_guest.is_empty() && DirAccess::dir_exists_absolute(project_guest)) {
			return project_guest;
		}
	}
	return String();
}

Dictionary EditorExportPlatformPS2::_toolchain_status() const {
	List<String> args;
	args.push_back("--json");
	args.push_back("ps2");
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

Error EditorExportPlatformPS2::_ensure_toolchain(const String &p_profile, String &r_error) {
	Dictionary st = _toolchain_status();
	const bool compile_ready = st.get("compile_ready", false);
	const bool dest_ready = st.get("dev_ready", false);
	const bool need_setup = (p_profile == "dev") ? (!compile_ready || !dest_ready) : !compile_ready;
	if (need_setup) {
		add_message(EXPORT_MESSAGE_INFO, "PS2", "Running blazium-toolchain ps2 setup --profile " + p_profile + " (first export may download gcc/SDK).");
		List<String> args;
		args.push_back("ps2");
		args.push_back("setup");
		args.push_back("--profile");
		args.push_back(p_profile);
		String output;
		int exitcode = 0;
		if (_spawn_toolchain(args, output, exitcode) != OK || exitcode != 0) {
			r_error = "blazium-toolchain ps2 setup failed:\n" + output;
			return FAILED;
		}
		st = _toolchain_status();
	}
	const int abi = int(st.get("guest_abi", 0));
	if (abi != kGuestABI) {
		r_error = vformat("blazium-toolchain guest_abi is %d; this editor expects ABI %d. Update the toolchain.", abi, kGuestABI);
		return ERR_UNCONFIGURED;
	}
	return OK;
}

bool EditorExportPlatformPS2::_scan_forbidden(const String &p_dir, String &r_hit) const {
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

Error EditorExportPlatformPS2::_spawn_toolchain(const List<String> &p_args, String &r_output, int &r_exit) const {
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_output = "blazium-toolchain not found (set export/ps2/toolchain or BLAZIUM_TOOLCHAIN)";
		return ERR_FILE_NOT_FOUND;
	}
	return OS::get_singleton()->execute(tc, p_args, &r_output, &r_exit, true);
}

void EditorExportPlatformPS2::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("ps2");
}

void EditorExportPlatformPS2::get_platform_features(List<String> *r_features) const {
	r_features->push_back("ps2");
}

void EditorExportPlatformPS2::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/output_kind", PROPERTY_HINT_ENUM, "EE ELF"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/region", PROPERTY_HINT_ENUM, "NTSC,PAL"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/width", PROPERTY_HINT_ENUM, "640,512,320"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ps2/make_iso"), false));
}

List<String> EditorExportPlatformPS2::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("elf");
	if (p_preset.is_valid() && bool(p_preset->get("ps2/make_iso"))) {
		list.push_back("iso");
	}
	return list;
}

int EditorExportPlatformPS2::get_options_count() const {
	return _host_supported() ? 1 : 0;
}

bool EditorExportPlatformPS2::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	r_missing_templates = false;
	if (!_host_supported()) {
		r_error += "PS2 export is available on Windows and Linux hosts only.\n";
		return false;
	}
	bool valid = true;
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_error += "blazium-toolchain is missing. Install the official CLI and set Editor Settings → Export → PS2 → Toolchain (or BLAZIUM_TOOLCHAIN).\n";
		valid = false;
	}
	return valid;
}

bool EditorExportPlatformPS2::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String hit;
	if (_scan_forbidden("res://", hit)) {
		r_error += "PS2 export forbids GDExtension and C# (" + hit + ").\n";
		return false;
	}
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_error += "PS2 export requires application/run/main_scene.\n";
		return false;
	}
	r_error += "Note: Export cooks ABI 1 NODE/MESH/GTEX slices then spawns blazium-toolchain. BIOS is only required for One-click / ps2 run.\n";
	return true;
}

Error EditorExportPlatformPS2::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "PS2 export is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}

	String hit;
	if (_scan_forbidden("res://", hit)) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "GDExtension and C# are not allowed (" + hit + ").");
		return ERR_UNCONFIGURED;
	}

	String setup_err;
	if (_ensure_toolchain("compile", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", setup_err);
		return FAILED;
	}

	const String src = _optional_runtime_src();
	const String overlay = _optional_runtime_overlay();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(p_path.get_base_dir());

	ps2_cook::CookOptions opts;
	static const int widths[] = { 640, 512, 320 };
	const int wsel = int(p_preset->get("ps2/width"));
	opts.width = widths[CLAMP(wsel, 0, 2)];
	opts.region = int(p_preset->get("ps2/region")) == 1 ? 1 : 0;
	ps2_cook::CookResult cooked;
	const Error cook_err = ps2_cook::cook_project(p_path.get_base_dir(), cooked, opts);
	_emit_cook_messages(cooked);
	if (cook_err != OK || !cooked.errors.is_empty()) {
		if (cooked.errors.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, "PS2", cooked.report.is_empty() ? String("Cook failed.") : cooked.report);
		}
		return cook_err != OK ? cook_err : FAILED;
	}

	if (int(p_preset->get("ps2/output_kind")) != 0) {
		add_message(EXPORT_MESSAGE_WARNING, "PS2", "ps2/output_kind is unread; output is an EE ELF. Use ps2/make_iso for a disc image.");
	}

	List<String> args;
	args.push_back("ps2");
	args.push_back("build");
	if (!src.is_empty()) {
		args.push_back("--src");
		args.push_back(src);
	}
	if (!overlay.is_empty()) {
		args.push_back("--overlay");
		args.push_back(overlay);
	}
	args.push_back("--export-src");
	args.push_back(p_path.get_base_dir().path_join("guest"));
	args.push_back("--out");
	args.push_back(p_path);
	if (!cooked.node_path.is_empty()) {
		args.push_back("--node");
		args.push_back(cooked.node_path);
	}
	if (!cooked.mesh_path.is_empty()) {
		args.push_back("--mesh");
		args.push_back(cooked.mesh_path);
	}
	if (!cooked.gtex_path.is_empty()) {
		args.push_back("--gtex");
		args.push_back(cooked.gtex_path);
	}
	if (!cooked.script_path.is_empty()) {
		args.push_back("--script");
		args.push_back(cooked.script_path);
	}

	String output;
	int exitcode = 0;
	const Error err = _spawn_toolchain(args, output, exitcode);
	if (err != OK || exitcode != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "blazium-toolchain ps2 build failed:\n" + output);
		return FAILED;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "Export path was not written: " + p_path);
		return ERR_CANT_CREATE;
	}
	const Vector<uint8_t> magic_bytes = f->get_buffer(4);
	if (magic_bytes.size() < 4 || magic_bytes[0] != 0x7f || magic_bytes[1] != 'E' || magic_bytes[2] != 'L' || magic_bytes[3] != 'F') {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "Output is not a MIPS ELF (refusing PE or other formats).");
		return FAILED;
	}
	add_message(EXPORT_MESSAGE_INFO, "PS2", "Wrote EE ELF " + p_path);

	if (bool(p_preset->get("ps2/make_iso"))) {
		String iso_warn;
		if (_try_make_iso(p_path, iso_warn) != OK) {
			add_message(EXPORT_MESSAGE_WARNING, "PS2", iso_warn.is_empty() ? String("ISO skipped.") : iso_warn);
		}
	}
	return OK;
}

void EditorExportPlatformPS2::_emit_cook_messages(const ps2_cook::CookResult &p_cooked) {
	for (int i = 0; i < p_cooked.errors.size(); i++) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", p_cooked.errors[i]);
	}
	for (int i = 0; i < p_cooked.warnings.size(); i++) {
		add_message(EXPORT_MESSAGE_WARNING, "PS2", p_cooked.warnings[i]);
	}
	for (int i = 0; i < p_cooked.infos.size(); i++) {
		add_message(EXPORT_MESSAGE_INFO, "PS2", p_cooked.infos[i]);
	}
	if (!p_cooked.report.is_empty()) {
		add_message(EXPORT_MESSAGE_INFO, "PS2", p_cooked.report);
	}
}

Error EditorExportPlatformPS2::_try_make_iso(const String &p_elf_path, String &r_warning) {
	const String dir = p_elf_path.get_base_dir();
	const String iso_out = dir.path_join(p_elf_path.get_file().get_basename() + ".iso");
	List<String> args;
	args.push_back("ps2");
	args.push_back("iso");
	args.push_back("--dir");
	args.push_back(dir);
	args.push_back("--out");
	args.push_back(iso_out);
	String output;
	int exitcode = 0;
	const Error err = _spawn_toolchain(args, output, exitcode);
	if (err != OK || exitcode != 0) {
		r_warning = "blazium-toolchain ps2 iso failed:\n" + output;
		return FAILED;
	}
	add_message(EXPORT_MESSAGE_INFO, "PS2", "Wrote ISO " + iso_out);
	return OK;
}

Error EditorExportPlatformPS2::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "PS2 One-click is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}
	String path = p_preset->get_export_path();
	if (path.is_empty()) {
		path = ProjectSettings::get_singleton()->globalize_path("res://export/Game.elf");
	}
	if (!FileAccess::exists(path)) {
		const Error exp = export_project(p_preset, true, path, p_debug_flags);
		if (exp != OK) {
			return exp;
		}
	}
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "blazium-toolchain not found for One-click.");
		return ERR_FILE_NOT_FOUND;
	}
	String setup_err;
	if (_ensure_toolchain("dev", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", setup_err);
		return FAILED;
	}
	Dictionary st = _toolchain_status();
	if (!bool(st.get("bios_ready", false))) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "PS2 BIOS not found. Set PS2_BIOS_DIR to a folder with a legal dump. Export still works without it.");
		return ERR_UNCONFIGURED;
	}
	List<String> args;
	args.push_back("ps2");
	args.push_back("run");
	args.push_back("--timeout");
	args.push_back("30s");
	args.push_back(path);
	return OS::get_singleton()->create_process(tc, args);
}

EditorExportPlatformPS2::EditorExportPlatformPS2() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);
		ImageLoaderSVG::create_image_from_string(img, _ps2_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);
	}
}
