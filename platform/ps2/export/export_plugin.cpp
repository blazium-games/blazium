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

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
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
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/output_kind", PROPERTY_HINT_ENUM, "EE ELF,EE ELF + ISO"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/region", PROPERTY_HINT_ENUM, "NTSC,PAL"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "ps2/width", PROPERTY_HINT_ENUM, "640,512,320"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ps2/display_hint", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), "Height from width: 640/512 -> 448 NTSC / 512 PAL; 320 -> 224. NTSC 60 Hz, PAL 50 Hz."));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ps2/make_iso"), false));
	// The disc's boot ELF name. The PS2 BIOS checks it against the disc's key
	// and only boots product codes; PCSX2's fast boot skips the BIOS, so a
	// GAME.ELF disc boots fast but not with a full boot.
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ps2/product_code", PROPERTY_HINT_PLACEHOLDER_TEXT, "SLUS_000.01"), "SLUS_000.01"));
	// The picture PS2LOGO shows during a full boot, from the disc's first 12
	// sectors. A disc without one stops at a gray box. Empty: the project's
	// name in the guest's HUD font.
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ps2/boot_logo", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg,*.webp,*.svg"), ""));
}

bool EditorExportPlatformPS2::_is_product_code(const String &p_code) {
	// four letters, underscore, three digits, dot, two digits: SLUS_000.01
	if (p_code.length() != 11 || p_code[4] != '_' || p_code[8] != '.') {
		return false;
	}
	for (int i = 0; i < 11; i++) {
		const char32_t c = p_code[i];
		if (i < 4) {
			if (!is_ascii_alphabet_char(c)) {
				return false;
			}
		} else if (i != 4 && i != 8 && !is_digit(c)) {
			return false;
		}
	}
	return true;
}

String EditorExportPlatformPS2::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset && p_name == StringName("ps2/product_code")) {
		const String code = String(p_preset->get("ps2/product_code")).strip_edges();
		if (!code.is_empty() && !_is_product_code(code)) {
			return "Use a product code like SLUS_000.01 (four letters, _, three digits, ., two digits). The PS2 BIOS only boots a disc whose boot ELF has one.";
		}
	}
	return String();
}

List<String> EditorExportPlatformPS2::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("elf");
	if (p_preset.is_valid() && (bool(p_preset->get("ps2/make_iso")) || int(p_preset->get("ps2/output_kind")) == 1)) {
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
	if (!ProjectSettings::get_singleton() || !bool(ProjectSettings::get_singleton()->get("ps2/enabled"))) {
		r_error += "Turn on Project Settings → ps2/enabled to export for PS2.\n";
		return false;
	}
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
	if (!ResourceLoader::exists(main_scene)) {
		r_error += "main_scene does not exist: " + main_scene + "\n";
		return false;
	}
	r_error += "Works: MESH/GTEX/NODE pack 0 in the ELF; STREAM extras; CAM/HIT/HUD/TILE/NAV/ANIM per pack; Input; move_and_slide; overlaps/raycast/tile_solid_at; look/orbit/shake; linear tweens; IMA SFX/MUSIC via libsdr; set_fade; DISP NTSC/PAL 640/512/320 (heights 448/512 or 224).\n";
	r_error += "Partial: change_scene/load/instantiate resolve STREAM paths; AnimationPlayer Node3D TRS keys (no AnimationTree); Button HUD slots; custom IR (not the official GDScript/Luau VM).\n";
	r_error += "Refused: play_xa/stop_xa (no CD-XA; use play_sfx/load_music IMA). play_fmv (no license-clean IPU decoder). spawn_shot, memcard_wrap. Official GDScript/Luau VMs. GDExtension/C#. AnimationTree/RigidBody/GPUParticles.\n";
	r_error += "Audio is WAV-only (first res:// .wav = SFX00; path containing music = MUSIC00). FileAccess is user:// only (host:SAVE.BIN / mc0:SAVE.BIN). STREAM/SFX/DISP/CAM.. are not ELF-embedded (host: or ISO).\n";
	r_error += "BIOS is required only for One-click / ps2 run, not for export. Guest framebuffer follows ps2/width and ps2/region (DISP.bin).\n";
	r_error += "EE 24 MiB and GS 4 MiB are shared across every resident pack (not per-scene). Catalog up to 99 extras; at most 16 extras + pack 0 in RAM. Unload rooms/NPCs you are not using.\n";
	return true;
}

Error EditorExportPlatformPS2::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!ProjectSettings::get_singleton() || !bool(ProjectSettings::get_singleton()->get("ps2/enabled"))) {
		add_message(EXPORT_MESSAGE_ERROR, "PS2", "Turn on Project Settings → ps2/enabled to export for PS2.");
		return ERR_UNCONFIGURED;
	}
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

	const int output_kind = int(p_preset->get("ps2/output_kind"));
	const bool want_iso = bool(p_preset->get("ps2/make_iso")) || output_kind == 1;
	if (output_kind > 1) {
		add_message(EXPORT_MESSAGE_WARNING, "PS2", "ps2/output_kind is unknown; writing an EE ELF. Use 0 (ELF) or 1 (ELF + ISO), or ps2/make_iso.");
	}
	if (output_kind == 1 && !bool(p_preset->get("ps2/make_iso"))) {
		add_message(EXPORT_MESSAGE_INFO, "PS2", "ps2/output_kind is EE ELF + ISO; will write Game.iso next to the ELF.");
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

	if (want_iso) {
		String iso_warn;
		const String boot_logo = _write_boot_logo(p_preset, p_path.get_base_dir());
		if (_try_make_iso(p_path, String(p_preset->get("ps2/product_code")).strip_edges(), boot_logo, iso_warn) != OK) {
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

static const uint8_t kLogoFont5x7[95][7] = {
	{ 0, 0, 0, 0, 0, 0, 0 }, /* space */
	{ 4, 4, 4, 4, 4, 0, 4 },
	{ 10, 10, 0, 0, 0, 0, 0 },
	{ 10, 31, 10, 31, 10, 0, 0 },
	{ 4, 14, 20, 14, 5, 14, 4 },
	{ 17, 18, 4, 8, 19, 0, 0 },
	{ 8, 20, 8, 21, 18, 13, 0 },
	{ 4, 4, 0, 0, 0, 0, 0 },
	{ 4, 8, 8, 8, 8, 8, 4 },
	{ 4, 2, 2, 2, 2, 2, 4 },
	{ 0, 10, 4, 31, 4, 10, 0 },
	{ 0, 4, 4, 31, 4, 4, 0 },
	{ 0, 0, 0, 0, 4, 4, 8 },
	{ 0, 0, 0, 31, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 4, 4 },
	{ 1, 2, 4, 8, 16, 0, 0 },
	{ 14, 17, 19, 21, 25, 17, 14 }, /* 0 */
	{ 4, 12, 4, 4, 4, 4, 14 },
	{ 14, 17, 1, 6, 8, 16, 31 },
	{ 14, 17, 1, 6, 1, 17, 14 },
	{ 2, 6, 10, 18, 31, 2, 2 },
	{ 31, 16, 30, 1, 1, 17, 14 },
	{ 6, 8, 16, 30, 17, 17, 14 },
	{ 31, 1, 2, 4, 8, 8, 8 },
	{ 14, 17, 17, 14, 17, 17, 14 },
	{ 14, 17, 17, 15, 1, 2, 12 },
	{ 0, 4, 4, 0, 4, 4, 0 },
	{ 0, 4, 4, 0, 4, 4, 8 },
	{ 2, 4, 8, 16, 8, 4, 2 },
	{ 0, 0, 31, 0, 31, 0, 0 },
	{ 8, 4, 2, 1, 2, 4, 8 },
	{ 14, 17, 1, 6, 4, 0, 4 },
	{ 14, 17, 19, 21, 23, 16, 14 },
	{ 14, 17, 17, 31, 17, 17, 17 }, /* A */
	{ 30, 17, 17, 30, 17, 17, 30 },
	{ 14, 17, 16, 16, 16, 17, 14 },
	{ 30, 17, 17, 17, 17, 17, 30 },
	{ 31, 16, 16, 30, 16, 16, 31 },
	{ 31, 16, 16, 30, 16, 16, 16 },
	{ 14, 17, 16, 23, 17, 17, 15 },
	{ 17, 17, 17, 31, 17, 17, 17 },
	{ 14, 4, 4, 4, 4, 4, 14 },
	{ 1, 1, 1, 1, 17, 17, 14 },
	{ 17, 18, 20, 24, 20, 18, 17 },
	{ 16, 16, 16, 16, 16, 16, 31 },
	{ 17, 27, 21, 21, 17, 17, 17 },
	{ 17, 25, 21, 19, 17, 17, 17 },
	{ 14, 17, 17, 17, 17, 17, 14 },
	{ 30, 17, 17, 30, 16, 16, 16 },
	{ 14, 17, 17, 17, 21, 18, 13 },
	{ 30, 17, 17, 30, 20, 18, 17 },
	{ 14, 17, 16, 14, 1, 17, 14 },
	{ 31, 4, 4, 4, 4, 4, 4 },
	{ 17, 17, 17, 17, 17, 17, 14 },
	{ 17, 17, 17, 17, 17, 10, 4 },
	{ 17, 17, 17, 21, 21, 21, 10 },
	{ 17, 17, 10, 4, 10, 17, 17 },
	{ 17, 17, 10, 4, 4, 4, 4 },
	{ 31, 1, 2, 4, 8, 16, 31 },
	{ 14, 8, 8, 8, 8, 8, 14 }, /* [ */
	{ 16, 8, 4, 2, 1, 0, 0 },
	{ 14, 2, 2, 2, 2, 2, 14 },
	{ 4, 10, 17, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 31 },
	{ 8, 4, 0, 0, 0, 0, 0 },
	{ 0, 0, 14, 1, 15, 17, 15 }, /* a */
	{ 16, 16, 22, 25, 17, 17, 30 },
	{ 0, 0, 14, 16, 16, 17, 14 },
	{ 1, 1, 13, 19, 17, 17, 15 },
	{ 0, 0, 14, 17, 31, 16, 14 },
	{ 6, 9, 8, 28, 8, 8, 8 },
	{ 0, 15, 17, 17, 15, 1, 14 },
	{ 16, 16, 22, 25, 17, 17, 17 },
	{ 4, 0, 12, 4, 4, 4, 14 },
	{ 2, 0, 6, 2, 2, 18, 12 },
	{ 16, 16, 18, 20, 24, 20, 18 },
	{ 12, 4, 4, 4, 4, 4, 14 },
	{ 0, 0, 26, 21, 21, 17, 17 },
	{ 0, 0, 22, 25, 17, 17, 17 },
	{ 0, 0, 14, 17, 17, 17, 14 },
	{ 0, 0, 30, 17, 30, 16, 16 },
	{ 0, 0, 13, 19, 15, 1, 1 },
	{ 0, 0, 22, 25, 16, 16, 16 },
	{ 0, 0, 14, 16, 14, 1, 30 },
	{ 8, 8, 28, 8, 8, 9, 6 },
	{ 0, 0, 17, 17, 17, 19, 13 },
	{ 0, 0, 17, 17, 17, 10, 4 },
	{ 0, 0, 17, 17, 21, 21, 10 },
	{ 0, 0, 17, 10, 4, 10, 17 },
	{ 0, 0, 17, 17, 15, 1, 14 },
	{ 0, 0, 31, 2, 4, 8, 31 },
	{ 2, 4, 4, 8, 4, 4, 2 }, /* { */
	{ 4, 4, 4, 4, 4, 4, 4 },
	{ 8, 4, 4, 2, 4, 4, 8 },
	{ 0, 0, 8, 21, 2, 0, 0 },
};

// BOOTLOGO.RAW for the toolchain's `ps2 iso --boot-logo`: the boot logo as
// raw 8-bit gray at the size PS2LOGO shows it (384x64 NTSC, 344x71 PAL),
// from ps2/boot_logo (fitted and centered on black) or, without one, the
// project's name in the guest's 5x7 HUD font. The first pixel is always
// black, as on Sony's discs. Returns the file's path, or "" on failure.
String EditorExportPlatformPS2::_write_boot_logo(const Ref<EditorExportPreset> &p_preset, const String &p_dir) {
	const bool pal = int(p_preset->get("ps2/region")) == 1;
	const int w = pal ? 344 : 384;
	const int h = pal ? 71 : 64;
	Vector<uint8_t> px;
	px.resize(w * h);
	px.fill(0);
	const String path = String(p_preset->get("ps2/boot_logo")).strip_edges();
	bool drawn = false;
	if (!path.is_empty()) {
		Ref<Image> img;
		Ref<Texture2D> tex = ResourceLoader::exists(path) ? Ref<Texture2D>(ResourceLoader::load(path)) : Ref<Texture2D>();
		if (tex.is_valid()) {
			img = tex->get_image();
		}
		if (img.is_null()) {
			img.instantiate();
			if (img->load(ProjectSettings::get_singleton()->globalize_path(path)) != OK) {
				img.unref();
			}
		}
		if (img.is_valid() && !img->is_empty()) {
			img = img->duplicate();
			if (img->is_compressed()) {
				img->decompress();
			}
			img->convert(Image::FORMAT_RGBA8);
			const float sc = MIN(float(w) / img->get_width(), float(h) / img->get_height());
			const int iw = MAX(1, int(img->get_width() * sc));
			const int ih = MAX(1, int(img->get_height() * sc));
			img->resize(iw, ih, Image::INTERPOLATE_BILINEAR);
			const int ox = (w - iw) / 2;
			const int oy = (h - ih) / 2;
			for (int y = 0; y < ih; y++) {
				for (int x = 0; x < iw; x++) {
					const Color c = img->get_pixel(x, y);
					px.write[(oy + y) * w + ox + x] = uint8_t(CLAMP(c.get_luminance() * c.a * 255.0f + 0.5f, 0.0f, 255.0f));
				}
			}
			drawn = true;
		} else {
			add_message(EXPORT_MESSAGE_WARNING, "PS2", "ps2/boot_logo " + path + " could not be loaded; using the project name.");
		}
	}
	if (!drawn) {
		String text = String(GLOBAL_GET("application/config/name")).strip_edges().to_upper();
		if (text.is_empty()) {
			text = "BLAZIUM";
		}
		int scale = 3;
		while (scale > 1 && text.length() * 6 * scale - scale > w - 8) {
			scale--;
		}
		const int tw = text.length() * 6 * scale - scale;
		const int x0 = MAX(0, (w - tw) / 2);
		const int y0 = (h - 7 * scale) / 2;
		for (int i = 0; i < text.length(); i++) {
			const char32_t ch = text[i];
			if (ch < 32 || ch > 126) {
				continue;
			}
			for (int ry = 0; ry < 7; ry++) {
				const uint8_t bits = kLogoFont5x7[ch - 32][ry];
				for (int cx = 0; cx < 5; cx++) {
					if (!(bits & (16 >> cx))) {
						continue;
					}
					for (int dy = 0; dy < scale; dy++) {
						for (int dx = 0; dx < scale; dx++) {
							const int x = x0 + (i * 6 + cx) * scale + dx;
							const int y = y0 + ry * scale + dy;
							if (x >= 0 && x < w && y >= 0 && y < h) {
								px.write[y * w + x] = 0xe0;
							}
						}
					}
				}
			}
		}
	}
	px.write[0] = 0;
	const String out = p_dir.path_join("BOOTLOGO.RAW");
	Ref<FileAccess> f = FileAccess::open(out, FileAccess::WRITE);
	if (f.is_null()) {
		return String();
	}
	f->store_buffer(px);
	return out;
}

Error EditorExportPlatformPS2::_try_make_iso(const String &p_elf_path, const String &p_product_code, const String &p_boot_logo, String &r_warning) {
	const String dir = p_elf_path.get_base_dir();
	const String iso_out = dir.path_join(p_elf_path.get_file().get_basename() + ".iso");
	List<String> args;
	args.push_back("ps2");
	args.push_back("iso");
	args.push_back("--dir");
	args.push_back(dir);
	args.push_back("--out");
	args.push_back(iso_out);
	// the boot ELF goes on the disc under this name (ps2/product_code)
	if (_is_product_code(p_product_code)) {
		args.push_back("--boot-elf");
		args.push_back(p_product_code.to_upper());
		// the logo is scrambled with a key from the product code
		if (!p_boot_logo.is_empty()) {
			args.push_back("--boot-logo");
			args.push_back(p_boot_logo);
		}
	} else if (!p_product_code.is_empty()) {
		add_message(EXPORT_MESSAGE_WARNING, "PS2", "ps2/product_code \"" + p_product_code + "\" is not a product code like SLUS_000.01; the ISO uses the toolchain's default.");
	}
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

void EditorExportPlatformPS2::set_api_singleton(Object *p_api) {
	ERR_FAIL_NULL(p_api);
	ERR_FAIL_COND(api_singleton != nullptr);
	ERR_FAIL_COND(Engine::get_singleton()->has_singleton("PS2"));
	api_singleton = p_api;
	Engine::get_singleton()->add_singleton(Engine::Singleton("PS2", api_singleton));
}

EditorExportPlatformPS2::EditorExportPlatformPS2() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);
		ImageLoaderSVG::create_image_from_string(img, _ps2_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);
	}
}

EditorExportPlatformPS2::~EditorExportPlatformPS2() {
	if (!api_singleton) {
		return;
	}
	Engine *engine = Engine::get_singleton();
	if (engine && engine->has_singleton("PS2") && engine->get_singleton_object("PS2") == api_singleton) {
		engine->remove_singleton("PS2");
	}
	memdelete(api_singleton);
	api_singleton = nullptr;
}
