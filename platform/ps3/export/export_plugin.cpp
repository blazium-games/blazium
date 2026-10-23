/**************************************************************************/
/*  export_plugin.cpp                                                     */
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

#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "core/config/project_settings.h"
#include "editor/export/editor_export.h"
#include "editor/editor_paths.h"
#include "scene/resources/image_texture.h"

#include "modules/svg/image_loader_svg.h"
#include "editor/themes/editor_scale.h"

String EditorExportPlatformPS3::get_name() const {
	return "PS3";
}

String EditorExportPlatformPS3::get_os_name() const {
	return "PS3";
}

void EditorExportPlatformPS3::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("ppc64");
	r_features->push_back("gl_compatibility");
}

bool EditorExportPlatformPS3::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	const String rendering_method = GLOBAL_GET("rendering/renderer/rendering_method");
	if (rendering_method == "forward_plus" || rendering_method == "mobile") {
		r_error = TTR("PS3 export requires Compatibility rendering. Set rendering/renderer/rendering_method to \"gl_compatibility\" in Project Settings.");
		return false;
	}
	return true;
}

void EditorExportPlatformPS3::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/title_id", PROPERTY_HINT_PLACEHOLDER_TEXT, "BLAZ00001"), "BLAZ00001"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/content_id", PROPERTY_HINT_PLACEHOLDER_TEXT, "UP0001-BLAZ00001_00-BLAZIUM000"), "UP0001-BLAZ00001_00-BLAZIUM000"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/version", PROPERTY_HINT_PLACEHOLDER_TEXT, "01.00"), "01.00"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/generate_pkg"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/emulator_root", PROPERTY_HINT_GLOBAL_DIR), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_FILE, "*.ppc64;*.self;*.elf"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_FILE, "*.ppc64;*.self;*.elf"), ""));
}

bool EditorExportPlatformPS3::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_preset == nullptr) {
		return true;
	}

	const bool advanced = p_preset->are_advanced_options_enabled();
	if (p_option == "custom_template/debug" || p_option == "custom_template/release") {
		return advanced;
	}
	return true;
}

bool EditorExportPlatformPS3::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	bool dvalid = exists_export_template(_get_template_name(true), &err);
	bool rvalid = exists_export_template(_get_template_name(false), &err);

	if (p_preset->get("custom_template/debug") != "") {
		dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
	}
	if (p_preset->get("custom_template/release") != "") {
		rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
	}

	bool valid = dvalid || rvalid;
	r_missing_templates = !valid;
	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
}

List<String> EditorExportPlatformPS3::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("self");
	list.push_back("elf");
	return list;
}

String EditorExportPlatformPS3::_get_script_path(const String &p_script_name) const {
	const String exe_root = OS::get_singleton()->get_executable_path().get_base_dir().path_join("..");
	const String candidate = exe_root.path_join("platform").path_join("ps3").path_join("scripts").path_join(p_script_name);
	if (FileAccess::exists(candidate)) {
		return candidate;
	}

	const String env_root = OS::get_singleton()->get_environment("BLAZIUM_ROOT");
	if (!env_root.is_empty()) {
		const String env_candidate = env_root.path_join("platform").path_join("ps3").path_join("scripts").path_join(p_script_name);
		if (FileAccess::exists(env_candidate)) {
			return env_candidate;
		}
	}

	return candidate;
}

Error EditorExportPlatformPS3::_run_python_script(const String &p_script, const List<String> &p_args, String *r_output) const {
	List<String> py_args;
	py_args.push_back(p_script);
	for (const String &arg : p_args) {
		py_args.push_back(arg);
	}

	int exit_code = 0;
	String output;
	Error err = OS::get_singleton()->execute("py", py_args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		List<String> wsl_args;
		wsl_args.push_back("python3");
		wsl_args.push_back(p_script);
		for (const String &arg : p_args) {
			wsl_args.push_back(arg);
		}
		err = OS::get_singleton()->execute("wsl", wsl_args, &output, &exit_code, true);
	}

	if (r_output) {
		*r_output = output;
	}
	ERR_FAIL_COND_V_MSG(err != OK || exit_code != 0, ERR_CANT_CREATE, vformat("Script failed: %s\n%s", p_script, output));
	return OK;
}

Error EditorExportPlatformPS3::_copy_template(const String &p_template, const String &p_dest) const {
	Ref<FileAccess> src = FileAccess::open(p_template, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(src.is_null(), ERR_FILE_CANT_OPEN, vformat("Cannot open template: %s", p_template));

	Ref<FileAccess> dst = FileAccess::open(p_dest, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(dst.is_null(), ERR_CANT_CREATE, vformat("Cannot write: %s", p_dest));

	const uint64_t len = src->get_length();
	for (uint64_t i = 0; i < len; i++) {
		dst->store_8(src->get_8());
	}
	return OK;
}

Error EditorExportPlatformPS3::_write_param_sfo(const String &p_path, const String &p_title, const String &p_title_id, const String &p_version, const String &p_content_id) const {
	const String script_path = _get_script_path("generate_param_sfo.py");
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(script_path), ERR_FILE_NOT_FOUND, vformat("Missing script: %s", script_path));

	List<String> args;
	args.push_back("--output");
	args.push_back(p_path);
	args.push_back("--title");
	args.push_back(p_title);
	args.push_back("--title-id");
	args.push_back(p_title_id);
	args.push_back("--version");
	args.push_back(p_version);
	args.push_back("--content-id");
	args.push_back(p_content_id);

	return _run_python_script(script_path, args);
}

Error EditorExportPlatformPS3::_write_icon0(const String &p_game_dir) const {
	const String icon_path = GLOBAL_GET("application/config/icon");
	if (icon_path.is_empty()) {
		return OK;
	}

	Ref<Image> icon;
	if (icon_path.ends_with(".svg")) {
		Ref<FileAccess> f = FileAccess::open(icon_path, FileAccess::READ);
		ERR_FAIL_COND_V(f.is_null(), ERR_FILE_NOT_FOUND);
		icon.instantiate();
		bool upsample = false;
		Error svg_err = ImageLoaderSVG::create_image_from_string(icon, f->get_as_text(), 1.0, upsample, false);
		ERR_FAIL_COND_V(svg_err != OK, svg_err);
	} else {
		icon = Image::load_from_file(icon_path);
	}
	ERR_FAIL_COND_V(icon.is_null(), ERR_FILE_NOT_FOUND);

	icon->resize(320, 176, Image::INTERPOLATE_LANCZOS);
	const String dst = p_game_dir.path_join("ICON0.PNG");
	return icon->save_png(dst);
}

Error EditorExportPlatformPS3::_deploy_to_emulator(const Ref<EditorExportPreset> &p_preset, const String &p_game_dir, const String &p_title_id) const {
	String emulator_root = p_preset->get("application/emulator_root");
	if (emulator_root.is_empty()) {
		return OK;
	}

	const String script_path = _get_script_path("deploy_rpcs3.py");
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(script_path), ERR_FILE_NOT_FOUND, vformat("Missing script: %s", script_path));

	List<String> args;
	args.push_back("--export-dir");
	args.push_back(p_game_dir.get_base_dir());
	args.push_back("--title-id");
	args.push_back(p_title_id);
	args.push_back("--emulator-root");
	args.push_back(emulator_root);

	String output;
	return _run_python_script(script_path, args, &output);
}

Error EditorExportPlatformPS3::_generate_pkg(const String &p_game_dir, const String &p_title_id, const String &p_content_id, const String &p_version, const String &p_title, String *r_pkg_path) const {
	const String pkg_path = p_game_dir.get_base_dir().path_join(p_title_id + ".pkg");
	List<String> args;
	args.push_back("-lc");
	args.push_back(vformat(
			"export PS3DEV=${PS3DEV:-/usr/local/ps3dev}; export PATH=$PATH:$PS3DEV/bin; "
			"make_pkg --contentid '%s' --pkgdir '%s' '%s'",
			p_content_id,
			pkg_path,
			p_game_dir));

	String output;
	int exit_code = 0;
	Error err = OS::get_singleton()->execute("wsl", args, &output, &exit_code, true);
	ERR_FAIL_COND_V_MSG(err != OK || exit_code != 0, ERR_CANT_CREATE, vformat("make_pkg failed:\n%s", output));

	if (r_pkg_path) {
		*r_pkg_path = pkg_path;
	}
	return OK;
}

Error EditorExportPlatformPS3::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	String template_file = find_export_template(_get_template_name(p_debug));
	if (p_preset->get(p_debug ? "custom_template/debug" : "custom_template/release") != "") {
		template_file = p_preset->get(p_debug ? "custom_template/debug" : "custom_template/release");
	}
	ERR_FAIL_COND_V_MSG(template_file.is_empty(), ERR_FILE_NOT_FOUND, "PS3 export template not found.");

	String title_id = p_preset->get("application/title_id");
	if (title_id.is_empty()) {
		title_id = "BLAZ00001";
	}

	String content_id = p_preset->get("application/content_id");
	if (content_id.is_empty()) {
		content_id = "UP0001-BLAZ00001_00-BLAZIUM000";
	}

	String base_path = p_path.get_basename();
	String game_dir = base_path.path_join(title_id);
	String usrdir = game_dir.path_join("USRDIR");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da->make_dir_recursive(usrdir) != OK, ERR_CANT_CREATE);

	String eboot_path = usrdir.path_join("EBOOT.BIN");
	Error err = _copy_template(template_file, eboot_path);
	ERR_FAIL_COND_V(err != OK, err);

	String pck_path = usrdir.path_join("game.pck");
	err = save_pack(p_preset, p_debug, pck_path);
	ERR_FAIL_COND_V(err != OK, err);

	err = da->copy(pck_path, game_dir.path_join("EBOOT.pck"));
	ERR_FAIL_COND_V(err != OK, err);

	String title = GLOBAL_GET("application/config/name");
	if (title.is_empty()) {
		title = "Blazium Game";
	}
	String version = p_preset->get("application/version");
	err = _write_param_sfo(game_dir.path_join("PARAM.SFO"), title, title_id, version, content_id);
	ERR_FAIL_COND_V(err != OK, err);

	err = _write_icon0(game_dir);
	ERR_FAIL_COND_V(err != OK, err);

	if (p_preset->get("application/generate_pkg")) {
		String pkg_path;
		err = _generate_pkg(game_dir, title_id, content_id, version, title, &pkg_path);
		ERR_FAIL_COND_V(err != OK, err);
		add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("PKG written to %s"), pkg_path));
	}

	String emulator_root = p_preset->get("application/emulator_root");
	if (!emulator_root.is_empty()) {
		err = _deploy_to_emulator(p_preset, game_dir, title_id);
		ERR_FAIL_COND_V(err != OK, err);
	}

	add_message(EXPORT_MESSAGE_INFO, TTR("Export"), vformat(TTR("PS3 export written to %s"), game_dir));
	add_message(EXPORT_MESSAGE_INFO, TTR("Export"), TTR("Launch from the RPCS3 game list (dev_hdd0/game/) — not Boot Game on the export folder."));
	return OK;
}

Ref<Texture2D> EditorExportPlatformPS3::get_logo() const {
	return logo;
}

Ref<Texture2D> EditorExportPlatformPS3::get_run_icon() const {
	return run_icon;
}

int EditorExportPlatformPS3::get_options_count() const {
	return 1;
}

String EditorExportPlatformPS3::get_option_label(int p_index) const {
	return TTR("Deploy to RPCS3");
}

String EditorExportPlatformPS3::get_option_tooltip(int p_index) const {
	return TTR("Export the project and copy it to the configured RPCS3 emulator folder.");
}

Error EditorExportPlatformPS3::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	ERR_FAIL_COND_V(p_device != 0, ERR_UNAVAILABLE);

	const String export_path = p_preset->get_export_path();
	ERR_FAIL_COND_V_MSG(export_path.is_empty(), ERR_UNAVAILABLE, "Export path is not configured.");

	Error err = export_project(p_preset, false, export_path, p_debug_flags);
	ERR_FAIL_COND_V(err != OK, err);

	String title_id = p_preset->get("application/title_id");
	if (title_id.is_empty()) {
		title_id = "BLAZ00001";
	}

	const String game_dir = export_path.get_basename().path_join(title_id);
	err = _deploy_to_emulator(p_preset, game_dir, title_id);
	ERR_FAIL_COND_V(err != OK, err);

	const String launch_script = _get_script_path("launch_rpcs3.py");
	if (FileAccess::exists(launch_script)) {
		String emulator_root = p_preset->get("application/emulator_root");
		if (!emulator_root.is_empty()) {
			List<String> launch_args;
			launch_args.push_back("--emulator-root");
			launch_args.push_back(emulator_root);
			launch_args.push_back("--title-id");
			launch_args.push_back(title_id);
			String launch_output;
			Error launch_err = _run_python_script(launch_script, launch_args, &launch_output);
			if (launch_err == OK) {
				add_message(EXPORT_MESSAGE_INFO, TTR("Run"), TTR("RPCS3 launch requested for installed game."));
			} else {
				add_message(EXPORT_MESSAGE_WARNING, TTR("Run"), vformat(TTR("Export deployed but RPCS3 auto-launch failed. Launch %s from the RPCS3 game list."), title_id));
			}
		}
	}

	return OK;
}

EditorExportPlatformPS3::EditorExportPlatformPS3() {
	Ref<Image> img;
	img.instantiate();
	bool upsample = false;
	ImageLoaderSVG::create_image_from_string(img, _ps3_logo_svg, EDSCALE, upsample, false);
	logo = ImageTexture::create_from_image(img);

	Ref<Image> run_img;
	run_img.instantiate();
	ImageLoaderSVG::create_image_from_string(run_img, _ps3_run_icon_svg, EDSCALE, upsample, false);
	run_icon = ImageTexture::create_from_image(run_img);
}
