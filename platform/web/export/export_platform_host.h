/**************************************************************************/
/*  export_platform_host.h                                                */
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

#include "export_plugin.h"

class EditorExportPlatformHost : public EditorExportPlatformWeb {
	GDCLASS(EditorExportPlatformHost, EditorExportPlatformWeb);

	String platform_name = "Web";
	String host_id = "web";
	bool folder = false;
	bool index_html = false;
	bool reddit_layout = false;
	bool boot_before_play = false;

	Error write_html_glue(const Ref<EditorExportPreset> &p_preset, const String &p_dir);
	Error patch_starter(const String &p_html_path);
	Error export_folder(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags);

protected:
	virtual bool _export_extensions_enabled(const Ref<EditorExportPreset> &p_preset) const override;
	virtual bool _export_threads_enabled(const Ref<EditorExportPreset> &p_preset) const override;
	virtual bool _export_pwa_enabled(const Ref<EditorExportPreset> &p_preset) const override;
	virtual String _export_custom_html_shell(const Ref<EditorExportPreset> &p_preset) const override;
	virtual String _export_third_party_scripts(const Ref<EditorExportPreset> &p_preset, const String &p_name) const override;

public:
	void configure(const String &p_name, const String &p_id, bool p_folder, bool p_index_html, bool p_reddit, bool p_boot);

	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_platform_features(List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	virtual String get_name() const override;
	virtual String get_os_name() const override;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual bool poll_export() override;
	virtual int get_options_count() const override;
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_option, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;
};
