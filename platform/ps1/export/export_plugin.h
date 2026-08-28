/**************************************************************************/
/*  export_plugin.h                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "cook/cooker.h"

#include "core/variant/dictionary.h"
#include "editor/export/editor_export_platform.h"
#include "scene/resources/image_texture.h"

class EditorExportPlatformPS1 : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformPS1, EditorExportPlatform);

	Ref<ImageTexture> logo;

	static bool _host_supported();
	String _find_toolchain() const;
	String _optional_runtime_src() const;
	Dictionary _toolchain_status() const;
	Error _ensure_toolchain(const String &p_profile, String &r_error);
	bool _scan_forbidden(const String &p_dir, String &r_hit) const;
	Error _spawn_toolchain(const List<String> &p_args, String &r_output, int &r_exit) const;
	void _emit_cook_messages(const ps1_cook::CookResult &p_cooked);
	Error _try_make_iso(const String &p_exe_path, const ps1_cook::CookResult &p_cooked, String &r_warning);

public:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual String get_os_name() const override { return "PlayStation 1"; }
	virtual String get_name() const override { return "PS1"; }
	virtual Ref<Texture2D> get_logo() const override { return logo; }

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual void get_platform_features(List<String> *r_features) const override;

	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override { return "pcsx-redux (OpenBIOS)"; }
	virtual String get_option_tooltip(int p_index) const override { return "Spawn blazium-toolchain ps1 run"; }
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;

	EditorExportPlatformPS1();
};
