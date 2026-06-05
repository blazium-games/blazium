/**************************************************************************/
/*  xbox_export_platform.h                                                */
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

#include "editor/export/editor_export_platform_pc.h"
#include "modules/xbox_module/xbox_module_constants.h"

class GDKToolchain;

class EditorExportPlatformXbox : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformXbox, EditorExportPlatformPC);

	Ref<GDKToolchain> toolchain;
	XboxExportTarget export_target = XBOX_EXPORT_TARGET_GDK_DESKTOP;

	String get_template_file_name_for_target(XboxExportTarget p_target, bool p_debug) const;
	String get_target_device_family(XboxExportTarget p_target) const;
	String get_windows_template_path(bool p_debug) const;

	Error _stage_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_abs_output_path, BitField<DebugFlags> p_flags, String &r_staging_dir);
	Error _wdapp_register(const String &p_staging_dir);
	Error _makepkg_pack(const Ref<EditorExportPreset> &p_preset, const String &p_staging_dir, const String &p_output_path);
	void _remove_dir_recursive(const String &p_path);

protected:
	static void _bind_methods() {}

public:
	EditorExportPlatformXbox();

	void set_export_target(XboxExportTarget p_target);
	XboxExportTarget get_export_target() const;

	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;
	virtual void get_platform_features(List<String> *r_features) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<DebugFlags> p_flags = 0) override;
};
