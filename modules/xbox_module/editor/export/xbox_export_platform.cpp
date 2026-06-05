/**************************************************************************/
/*  xbox_export_platform.cpp                                              */
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

#include "xbox_export_platform.h"

#include "../gdk_toolchain.h"
#include "../microsoft_game_config.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/templates/list.h"
#include "scene/resources/image_texture.h"

EditorExportPlatformXbox::EditorExportPlatformXbox() {
	set_name("Xbox on PC");
	set_os_name("Windows");

	Ref<Image> logo_image = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	logo_image->fill(Color(0.0, 0.5, 0.0));
	Ref<ImageTexture> logo_tex = ImageTexture::create_from_image(logo_image);
	set_logo(logo_tex);

	toolchain = GDKToolchain::create();
}

void EditorExportPlatformXbox::set_export_target(XboxExportTarget p_target) {
	export_target = p_target;
}

XboxExportTarget EditorExportPlatformXbox::get_export_target() const {
	return export_target;
}

String EditorExportPlatformXbox::get_target_device_family(XboxExportTarget p_target) const {
	switch (p_target) {
		case XBOX_EXPORT_TARGET_XBOX_ONE:
			return "XboxOne";
		case XBOX_EXPORT_TARGET_XBOX_SERIES:
			return "XboxSeries";
		case XBOX_EXPORT_TARGET_GDK_DESKTOP:
		default:
			return "PC";
	}
}

String EditorExportPlatformXbox::get_template_file_name_for_target(XboxExportTarget p_target, bool p_debug) const {
	switch (p_target) {
		case XBOX_EXPORT_TARGET_XBOX_ONE:
			return vformat("xbox_durango_%s.exe", p_debug ? "debug" : "release");
		case XBOX_EXPORT_TARGET_XBOX_SERIES:
			return vformat("xbox_scarlett_%s.exe", p_debug ? "debug" : "release");
		case XBOX_EXPORT_TARGET_GDK_DESKTOP:
		default:
			return vformat("windows_%s_x86_64.exe", p_debug ? "debug" : "release");
	}
}

String EditorExportPlatformXbox::get_template_file_name(const String &p_target, const String &p_arch) const {
	bool debug = p_target.contains("debug");
	return get_template_file_name_for_target(export_target, debug);
}

List<String> EditorExportPlatformXbox::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> extensions;
	extensions.push_back("msixvc");
	return extensions;
}

void EditorExportPlatformXbox::get_platform_features(List<String> *r_features) const {
	r_features->push_back("pc");
	r_features->push_back("windows");
	r_features->push_back("gdk");
	r_features->push_back("xbox");
	r_features->push_back("d3d12");
	r_features->push_back("x86_64");
}

void EditorExportPlatformXbox::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_64"), "x86_64"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "packaging/ekb_file", PROPERTY_HINT_GLOBAL_FILE, "*.ekb"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "dev/register_loose"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "dev/sandbox_id"), "RETAIL"));
}

bool EditorExportPlatformXbox::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_option == "packaging/ekb_file") {
		return !p_preset->get("dev/register_loose");
	}
	if (p_option == "dev/sandbox_id") {
		return p_preset->get("dev/register_loose");
	}
	return true;
}

String EditorExportPlatformXbox::get_windows_template_path(bool p_debug) const {
	String template_name = get_template_file_name_for_target(XBOX_EXPORT_TARGET_GDK_DESKTOP, p_debug);
	String found = find_export_template(template_name);
	if (!found.is_empty()) {
		return found;
	}
	return OS::get_singleton()->get_executable_path();
}

bool EditorExportPlatformXbox::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	if (!toolchain.is_valid() || !toolchain->is_gdk_available()) {
		r_error = "Microsoft GDK not found. Install via: winget install Microsoft.Gaming.GDK";
		return false;
	}
	r_error = String();
	return true;
}

bool EditorExportPlatformXbox::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	r_missing_templates = false;

	if (export_target == XBOX_EXPORT_TARGET_XBOX_ONE || export_target == XBOX_EXPORT_TARGET_XBOX_SERIES) {
		r_error = "Xbox console export requires GDKX and console export templates (not available in public GDK).";
		return false;
	}

	if (!has_valid_project_configuration(p_preset, r_error)) {
		return false;
	}

	String config_error;
	if (MicrosoftGameConfig::validate_project_config(&config_error).is_empty()) {
		r_error = config_error;
		return false;
	}

	String template_path = get_windows_template_path(p_debug);
	if (template_path.is_empty()) {
		r_error = "Windows export template not found.";
		r_missing_templates = true;
		return false;
	}

	r_error = String();
	return true;
}

void EditorExportPlatformXbox::_remove_dir_recursive(const String &p_path) {
	Ref<DirAccess> da = DirAccess::open(p_path);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String entry = da->get_next();
	while (!entry.is_empty()) {
		if (da->current_is_dir()) {
			_remove_dir_recursive(p_path.path_join(entry));
		} else {
			da->remove(entry);
		}
		entry = da->get_next();
	}
	da->list_dir_end();
	DirAccess::remove_absolute(p_path);
}

Error EditorExportPlatformXbox::_stage_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_abs_output_path, BitField<DebugFlags> p_flags, String &r_staging_dir) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, ERR_UNAVAILABLE);

	String out_dir = p_abs_output_path.get_base_dir();
	Error mk_err = DirAccess::make_dir_recursive_absolute(out_dir);
	ERR_FAIL_COND_V(mk_err != OK, mk_err);

	r_staging_dir = out_dir.path_join("_xbox_staging");
	if (DirAccess::exists(r_staging_dir)) {
		_remove_dir_recursive(r_staging_dir);
	}
	mk_err = DirAccess::make_dir_recursive_absolute(r_staging_dir);
	ERR_FAIL_COND_V(mk_err != OK, mk_err);

	Ref<FileAccess> gdignore = FileAccess::open(r_staging_dir.path_join(".gdignore"), FileAccess::WRITE);
	if (gdignore.is_valid()) {
		gdignore->close();
	}

	const String exe_name = MicrosoftGameConfig::validate_project_config();
	ERR_FAIL_COND_V(exe_name.is_empty(), ERR_FILE_NOT_FOUND);

	const String exe_path = r_staging_dir.path_join(exe_name);
	const String pck_path = r_staging_dir.path_join(exe_name.get_basename() + ".pck");

	const String template_path = get_windows_template_path(p_debug);
	Error copy_err = DirAccess::copy_absolute(template_path, exe_path);
	ERR_FAIL_COND_V(copy_err != OK, copy_err);

	Error pck_err = export_pack(p_preset, p_debug, pck_path, p_flags);
	ERR_FAIL_COND_V(pck_err != OK, pck_err);

	if (toolchain.is_valid()) {
		copy_err = toolchain->copy_runtime_dlls(r_staging_dir, p_debug);
		ERR_FAIL_COND_V(copy_err != OK, copy_err);
	}

	copy_err = MicrosoftGameConfig::copy_to_staging(r_staging_dir, get_target_device_family(export_target));
	ERR_FAIL_COND_V(copy_err != OK, copy_err);

	MicrosoftGameConfig::stage_logos(r_staging_dir);
	return OK;
}

Error EditorExportPlatformXbox::_wdapp_register(const String &p_staging_dir) {
	if (!toolchain.is_valid() || toolchain->get_wdapp_path().is_empty()) {
		return ERR_UNAVAILABLE;
	}

	List<String> args;
	args.push_back("register");
	args.push_back(ProjectSettings::get_singleton()->globalize_path(p_staging_dir));

	int exit_code = 0;
	String output;
	Error err = OS::get_singleton()->execute(toolchain->get_wdapp_path(), args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "XboxExport", vformat("wdapp register failed (exit %d): %s", exit_code, output));
		return ERR_BUG;
	}
	add_message(EXPORT_MESSAGE_INFO, "XboxExport", "Registered loose package via wdapp.");
	return OK;
}

Error EditorExportPlatformXbox::_makepkg_pack(const Ref<EditorExportPreset> &p_preset, const String &p_staging_dir, const String &p_output_path) {
	if (!toolchain.is_valid() || toolchain->get_makepkg_path().is_empty()) {
		return ERR_UNAVAILABLE;
	}

	const String global_staging = ProjectSettings::get_singleton()->globalize_path(p_staging_dir);
	const String global_output = ProjectSettings::get_singleton()->globalize_path(p_output_path);
	const String layout_path = global_staging + "\\layout.xml";

	List<String> genmap_args;
	genmap_args.push_back("genmap");
	genmap_args.push_back("/f");
	genmap_args.push_back(layout_path);
	genmap_args.push_back("/d");
	genmap_args.push_back(global_staging);

	int exit_code = 0;
	String output;
	Error err = OS::get_singleton()->execute(toolchain->get_makepkg_path(), genmap_args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "XboxExport", vformat("makepkg genmap failed: %s", output));
		return ERR_BUG;
	}

	List<String> pack_args;
	pack_args.push_back("pack");
	pack_args.push_back("/f");
	pack_args.push_back(layout_path);
	pack_args.push_back("/d");
	pack_args.push_back(global_staging);
	pack_args.push_back("/pd");
	pack_args.push_back(global_output.get_base_dir());

	String ekb = p_preset->get("packaging/ekb_file");
	if (!ekb.is_empty()) {
		pack_args.push_back("/lk");
		pack_args.push_back(ProjectSettings::get_singleton()->globalize_path(ekb));
	}

	err = OS::get_singleton()->execute(toolchain->get_makepkg_path(), pack_args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		add_message(EXPORT_MESSAGE_ERROR, "XboxExport", vformat("makepkg pack failed: %s", output));
		return ERR_BUG;
	}

	add_message(EXPORT_MESSAGE_INFO, "XboxExport", vformat("Package created: %s", global_output));
	return OK;
}

Error EditorExportPlatformXbox::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<DebugFlags> p_flags) {
	String abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
	if (!abs_path.is_absolute_path()) {
		abs_path = ProjectSettings::get_singleton()->globalize_path("res://").path_join(abs_path);
	}
	abs_path = abs_path.simplify_path();

	String staging_dir;
	Error err = _stage_project(p_preset, p_debug, abs_path, p_flags, staging_dir);
	if (err != OK) {
		return err;
	}

	if (p_preset->get("dev/register_loose")) {
		return _wdapp_register(staging_dir);
	}

	return _makepkg_pack(p_preset, staging_dir, abs_path);
}
