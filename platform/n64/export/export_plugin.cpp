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
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "servers/display_server.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "modules/svg/image_loader_svg.h"

// Must match blazium-toolchain/guest/n64 CookABI. Cook writers land in P3.
static const int kGuestABI = 1;

bool EditorExportPlatformN64::_host_supported() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	return true;
#else
	return false;
#endif
}

String EditorExportPlatformN64::_find_toolchain() const {
	if (EditorSettings::get_singleton()) {
		const String from_settings = EditorSettings::get_singleton()->get("export/n64/toolchain");
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

String EditorExportPlatformN64::_optional_runtime_src() const {
	if (!EditorSettings::get_singleton()) {
		return String();
	}
	const String from_settings = EditorSettings::get_singleton()->get("export/n64/runtime_src");
	if (from_settings.is_empty()) {
		return String();
	}
	if (FileAccess::exists(from_settings.path_join("CMakeLists.txt")) || FileAccess::exists(from_settings.path_join("Makefile"))) {
		return from_settings;
	}
	return String();
}

String EditorExportPlatformN64::_optional_runtime_overlay() const {
	if (OS::get_singleton() && OS::get_singleton()->has_environment("BLAZIUM_N64_OVERLAY")) {
		const String from_env = OS::get_singleton()->get_environment("BLAZIUM_N64_OVERLAY");
		if (!from_env.is_empty() && DirAccess::dir_exists_absolute(from_env)) {
			return from_env;
		}
	}
	if (EditorSettings::get_singleton()) {
		const String from_settings = EditorSettings::get_singleton()->get("export/n64/runtime_overlay");
		if (!from_settings.is_empty() && DirAccess::dir_exists_absolute(from_settings)) {
			return from_settings;
		}
	}
	if (ProjectSettings::get_singleton()) {
		const String project_guest = ProjectSettings::get_singleton()->globalize_path("res://n64_guest");
		if (!project_guest.is_empty() && DirAccess::dir_exists_absolute(project_guest)) {
			return project_guest;
		}
	}
	return String();
}

Dictionary EditorExportPlatformN64::_toolchain_status() const {
	List<String> args;
	args.push_back("--json");
	args.push_back("n64");
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

Error EditorExportPlatformN64::_ensure_toolchain(const String &p_profile, String &r_error) {
	Dictionary st = _toolchain_status();
	const bool compile_ready = st.get("compile_ready", false);
	const bool dest_ready = st.get("dev_ready", false);
	const bool need_setup = (p_profile == "dev") ? (!compile_ready || !dest_ready) : !compile_ready;
	if (need_setup) {
		add_message(EXPORT_MESSAGE_INFO, "N64", "Running blazium-toolchain n64 setup --profile " + p_profile + " (first export may download gcc/SDK/Ares).");
		List<String> args;
		args.push_back("n64");
		args.push_back("setup");
		args.push_back("--profile");
		args.push_back(p_profile);
		String output;
		int exitcode = 0;
		if (_spawn_toolchain(args, output, exitcode) != OK || exitcode != 0) {
			r_error = "blazium-toolchain n64 setup failed:\n" + output;
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

bool EditorExportPlatformN64::_scan_forbidden(const String &p_dir, String &r_hit) const {
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

Error EditorExportPlatformN64::_spawn_toolchain(const List<String> &p_args, String &r_output, int &r_exit) const {
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_output = "blazium-toolchain not found (set export/n64/toolchain or BLAZIUM_TOOLCHAIN)";
		return ERR_FILE_NOT_FOUND;
	}
	return OS::get_singleton()->execute(tc, p_args, &r_output, &r_exit, true);
}

String EditorExportPlatformN64::_one_click_emu() const {
#ifdef WINDOWS_ENABLED
	return "project64";
#else
	return "ares";
#endif
}

void EditorExportPlatformN64::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("n64");
}

void EditorExportPlatformN64::get_platform_features(List<String> *r_features) const {
	r_features->push_back("n64");
}

void EditorExportPlatformN64::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "n64/region", PROPERTY_HINT_ENUM, "Region-free,NTSC,PAL"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "n64/width", PROPERTY_HINT_ENUM, "320"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "n64/display_hint", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), "v1 framebuffer is 320×240. Expansion Pak 8 MiB is always on."));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "n64/emu", PROPERTY_HINT_ENUM, "Host default,Ares,Project64"), 0));
}

List<String> EditorExportPlatformN64::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("z64");
	return list;
}

int EditorExportPlatformN64::get_options_count() const {
	return _host_supported() ? 1 : 0;
}

String EditorExportPlatformN64::get_option_label(int p_index) const {
#ifdef WINDOWS_ENABLED
	return "Project64 (.z64)";
#else
	return "Ares (.z64)";
#endif
}

bool EditorExportPlatformN64::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	r_missing_templates = false;
	if (!_host_supported()) {
		r_error += "N64 export is available on Windows and Linux hosts only.\n";
		return false;
	}
	bool valid = true;
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		r_error += "blazium-toolchain is missing. Install the official CLI and set Editor Settings → Export → N64 → Toolchain (or BLAZIUM_TOOLCHAIN).\n";
		valid = false;
	}
	Dictionary st = _toolchain_status();
	const bool ares_ok = bool(st.get("ares_ready", false));
	const bool pj_ok = bool(st.get("project64_ready", false));
	if (valid && !ares_ok && !pj_ok) {
		r_error += "One-click needs Ares or Project64. Run blazium-toolchain n64 setup --profile dev (export still works without an emu).\n";
	}
	return valid;
}

bool EditorExportPlatformN64::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	if (!ProjectSettings::get_singleton() || !bool(ProjectSettings::get_singleton()->get("n64/enabled"))) {
		r_error += "Turn on Project Settings → n64/enabled to export for Nintendo 64.\n";
		return false;
	}
	String hit;
	if (_scan_forbidden("res://", hit)) {
		r_error += "N64 export forbids GDExtension and C# (" + hit + ").\n";
		return false;
	}
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_error += "N64 export requires application/run/main_scene.\n";
		return false;
	}
	if (!ResourceLoader::exists(main_scene)) {
		r_error += "main_scene does not exist: " + main_scene + "\n";
		return false;
	}
	r_error += "Works: bundled rdpq hello .z64; 8 MiB Expansion Pak; DragonFS rom://; EEPROM user://.\n";
	r_error += "Partial: cook NTEX/MESH/NODE/SCRIPT is P3; rdpq draw of cooked mesh is P4; pad/mixer is P5; GDScript/Luau IR is P6.\n";
	r_error += "Refused: Godot templates/.tpz, ISO/CUE, official Nintendo SDK (C:\\ultra), Tiny3D in v1, GDExtension/C#, official GDScript/Luau VMs.\n";
	r_error += "Validators: Ares and Project64 (n64 run --emu both). No PIF/BIOS dump.\n";
	return true;
}

Error EditorExportPlatformN64::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!ProjectSettings::get_singleton() || !bool(ProjectSettings::get_singleton()->get("n64/enabled"))) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "Turn on Project Settings → n64/enabled to export for Nintendo 64.");
		return ERR_UNCONFIGURED;
	}
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "N64 export is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}
	if (!p_path.to_lower().ends_with(".z64")) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "Export path must end in .z64 (got " + p_path + ").");
		return ERR_INVALID_PARAMETER;
	}

	String hit;
	if (_scan_forbidden("res://", hit)) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "GDExtension and C# are not allowed (" + hit + ").");
		return ERR_UNCONFIGURED;
	}

	String setup_err;
	if (_ensure_toolchain("compile", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", setup_err);
		return FAILED;
	}

	const String src = _optional_runtime_src();
	const String overlay = _optional_runtime_overlay();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(p_path.get_base_dir());

	n64_cook::CookResult cooked;
	const Error cook_err = n64_cook::cook_project(p_path.get_base_dir(), cooked);
	if (cook_err != OK || !cooked.errors.is_empty()) {
		const String msg = cooked.errors.is_empty() ? (cooked.report.is_empty() ? String("N64 cook failed.") : cooked.report) : cooked.errors[0];
		add_message(EXPORT_MESSAGE_ERROR, "N64", msg);
		return cook_err != OK ? cook_err : FAILED;
	}
	add_message(EXPORT_MESSAGE_INFO, "N64", cooked.report);

	List<String> args;
	args.push_back("n64");
	args.push_back("build");
	if (!src.is_empty()) {
		args.push_back("--src");
		args.push_back(src);
	}
	if (!overlay.is_empty()) {
		args.push_back("--overlay");
		args.push_back(overlay);
	}
	if (!cooked.node_path.is_empty()) {
		args.push_back("--node");
		args.push_back(cooked.node_path);
	}
	if (!cooked.mesh_path.is_empty()) {
		args.push_back("--mesh");
		args.push_back(cooked.mesh_path);
	}
	if (!cooked.ntex_path.is_empty()) {
		args.push_back("--ntex");
		args.push_back(cooked.ntex_path);
	}
	if (!cooked.inp_path.is_empty()) {
		args.push_back("--inp");
		args.push_back(cooked.inp_path);
	}
	args.push_back("--export-src");
	args.push_back(p_path.get_base_dir().path_join("guest"));
	args.push_back("--out");
	args.push_back(p_path);

	String output;
	int exitcode = 0;
	const Error err = _spawn_toolchain(args, output, exitcode);
	if (err != OK || exitcode != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "blazium-toolchain n64 build failed:\n" + output);
		return FAILED;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "Export path was not written: " + p_path);
		return ERR_CANT_CREATE;
	}
	const Vector<uint8_t> magic_bytes = f->get_buffer(4);
	if (magic_bytes.size() < 4 || magic_bytes[0] != 0x80 || magic_bytes[1] != 0x37 || magic_bytes[2] != 0x12 || magic_bytes[3] != 0x40) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "Output is not a big-endian .z64 (refusing PE, ELF, or other formats).");
		return FAILED;
	}
	const uint64_t rom_len = f->get_length();
	if (rom_len > uint64_t(n64_cook::kRomBudget)) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", vformat("N64 ROM is %d bytes; cap is %d (no silent oversize).", int64_t(rom_len), n64_cook::kRomBudget));
		return ERR_OUT_OF_MEMORY;
	}
	add_message(EXPORT_MESSAGE_INFO, "N64", "Wrote N64 ROM " + p_path);
	return OK;
}

Error EditorExportPlatformN64::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	if (!_host_supported()) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "N64 One-click is available on Windows and Linux hosts only.");
		return ERR_UNAVAILABLE;
	}
	String path = p_preset.is_valid() ? p_preset->get_export_path() : String();
	if (path.is_empty()) {
		path = ProjectSettings::get_singleton()->globalize_path("res://export/Game.z64");
	} else if (!path.is_absolute_path()) {
		path = ProjectSettings::get_singleton()->globalize_path(path.begins_with("res://") ? path : String("res://") + path);
	}
	if (!FileAccess::exists(path)) {
		const Error exp = export_project(p_preset, true, path, p_debug_flags);
		if (exp != OK) {
			return exp;
		}
	}
	const String tc = _find_toolchain();
	if (tc.is_empty() || !FileAccess::exists(tc)) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", "blazium-toolchain not found for One-click.");
		return ERR_FILE_NOT_FOUND;
	}
	String setup_err;
	if (_ensure_toolchain("dev", setup_err) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, "N64", setup_err);
		return FAILED;
	}
	String emu = _one_click_emu();
	if (p_preset.is_valid()) {
		const int sel = int(p_preset->get("n64/emu"));
		if (sel == 1) {
			emu = "ares";
		} else if (sel == 2) {
			emu = "project64";
		}
		// sel == 0: host default (Windows Project64, Linux Ares). Dual-emu is CI, not One-click.
	}
	List<String> args;
	args.push_back("n64");
	args.push_back("run");
	args.push_back("--emu");
	args.push_back(emu);
	args.push_back("--timeout");
	args.push_back("30s");
	args.push_back(path);
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (headless) {
		String output;
		int exitcode = 0;
		const Error err = OS::get_singleton()->execute(tc, args, &output, &exitcode, true);
		if (err != OK || exitcode != 0) {
			add_message(EXPORT_MESSAGE_ERROR, "N64", "blazium-toolchain n64 run failed:\n" + output);
			return FAILED;
		}
		if (!output.is_empty()) {
			add_message(EXPORT_MESSAGE_INFO, "N64", output);
		}
		return OK;
	}
	return OS::get_singleton()->create_process(tc, args);
}

EditorExportPlatformN64::EditorExportPlatformN64() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);
		ImageLoaderSVG::create_image_from_string(img, _n64_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);
	}
}
