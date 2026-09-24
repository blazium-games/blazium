/**************************************************************************/
/*  obfuscation_export_plugin.h                                           */
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

#ifdef TOOLS_ENABLED

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/export/editor_export_plugin.h"

class ObfuscationExportPlugin : public EditorExportPlugin {
	GDCLASS(ObfuscationExportPlugin, EditorExportPlugin);

	bool enabled = true;
	String saved_copyright_text;
	String saved_copyright_author;
	String saved_copyright_license;
	bool stripped_copyright = false;
	int scene_stamp_remaining = 4;
	bool scramble_pack = false;
	HashMap<String, String> pack_idents;
	HashSet<String> pack_funcs;
	HashSet<String> pack_scene_names;
	HashMap<String, String> pack_scripts;
	HashMap<StringName, Variant> saved_settings;
	Vector<uint8_t> saved_uid_cache;
	String uid_cache_path;
	bool uid_cache_patched = false;

protected:
	static void _bind_methods() {}
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_end() override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual bool _begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) override;
	virtual Node *_customize_scene(Node *p_root, const String &p_path) override;
	virtual uint64_t _get_customization_configuration_hash() const override;

public:
	virtual String get_name() const override { return "Obfuscation"; }
};

#endif
