/**************************************************************************/
/*  luau_export_plugin.h                                                  */
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

#include "editor/export/editor_export_platform.h"
#include "editor/export/editor_export_plugin.h"
#include "editor/export/editor_export_preset.h"

class EditorExportLuau : public EditorExportPlugin {
	GDCLASS(EditorExportLuau, EditorExportPlugin);

	static constexpr int DEFAULT_SCRIPT_MODE = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;

	int script_mode = DEFAULT_SCRIPT_MODE;
	bool compile_bytecode = true;
	bool strip_source = false;
	bool minify_source = false;
	bool tree_shake_source = false;
	bool encrypt_bytecode = false;

protected:
	static void _bind_methods() {}
	void _get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const override;
	void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;

public:
	virtual String get_name() const override { return "Luau"; }
};

#endif
