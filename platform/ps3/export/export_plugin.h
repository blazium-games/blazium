/**************************************************************************/
/*  export_plugin.h                                                       */
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

#include "editor/export/editor_export_platform.h"
#include "scene/resources/image_texture.h"

class EditorExportPlatformPS3 : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformPS3, EditorExportPlatform);

	Ref<ImageTexture> logo;
	Ref<ImageTexture> run_icon;

	String _get_template_name(bool p_debug) const {
		return String("ps3_") + (p_debug ? "debug" : "release") + ".ppc64";
	}

	Error _write_param_sfo(const String &p_path, const String &p_title, const String &p_title_id, const String &p_version, const String &p_content_id) const;
	Error _copy_template(const String &p_template, const String &p_dest) const;
	String _get_script_path(const String &p_script_name) const;
	Error _run_python_script(const String &p_script, const List<String> &p_args, String *r_output = nullptr) const;
	Error _generate_pkg(const String &p_game_dir, const String &p_title_id, const String &p_content_id, const String &p_version, const String &p_title, String *r_pkg_path) const;
	Error _write_icon0(const String &p_game_dir) const;
	Error _deploy_to_emulator(const Ref<EditorExportPreset> &p_preset, const String &p_game_dir, const String &p_title_id) const;

public:
	virtual String get_name() const override;
	virtual String get_os_name() const override;

	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual Ref<Texture2D> get_logo() const override;
	virtual Ref<Texture2D> get_run_icon() const override;

	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override;
	virtual String get_option_tooltip(int p_index) const override;
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;

	virtual void get_platform_features(List<String> *r_features) const override {
		r_features->push_back("ps3");
		r_features->push_back(get_os_name().to_lower());
	}

	EditorExportPlatformPS3();
};
