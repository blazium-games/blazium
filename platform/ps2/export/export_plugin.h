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

class EditorExportPlatformPS2 : public EditorExportPlatform {
	// The PS2 script API object, registered as the "PS2" Engine singleton.
	// Owned by the exporter so it is freed when the editor shuts down.
	Object *api_singleton = nullptr;

	GDCLASS(EditorExportPlatformPS2, EditorExportPlatform);

	Ref<ImageTexture> logo;

	static bool _host_supported();
	String _find_toolchain() const;
	String _optional_runtime_src() const;
	String _optional_runtime_overlay() const;
	Dictionary _toolchain_status() const;
	Error _ensure_toolchain(const String &p_profile, String &r_error);
	bool _scan_forbidden(const String &p_dir, String &r_hit) const;
	Error _spawn_toolchain(const List<String> &p_args, String &r_output, int &r_exit) const;
	Error _try_make_iso(const String &p_elf_path, const String &p_product_code, const String &p_boot_logo, String &r_warning);
	String _write_boot_logo(const Ref<EditorExportPreset> &p_preset, const String &p_dir);
	static bool _is_product_code(const String &p_code);
	void _emit_cook_messages(const ps2_cook::CookResult &p_cooked);

public:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual String get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const override;
	virtual String get_os_name() const override { return "PlayStation 2"; }
	virtual String get_name() const override { return "PS2"; }
	virtual Ref<Texture2D> get_logo() const override { return logo; }

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual void get_platform_features(List<String> *r_features) const override;

	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override { return "PCSX2 (-elf)"; }
	virtual String get_option_tooltip(int p_index) const override { return "Spawn blazium-toolchain ps2 run"; }
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;

	// Registers p_api as the "PS2" Engine singleton and takes ownership of it.
	void set_api_singleton(Object *p_api);

	EditorExportPlatformPS2();
	~EditorExportPlatformPS2();
};
