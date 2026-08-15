/**************************************************************************/
/*  justamcp_export_tools.h                                               */
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

#include "core/object/object.h"
#include "core/templates/list.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/main/node.h"

class JustAMCPExportTools : public Object {
	GDCLASS(JustAMCPExportTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;

	Dictionary _list_export_presets(const Dictionary &p_params);
	Dictionary _export_project(const Dictionary &p_params);
	Dictionary _get_export_info(const Dictionary &p_params);
	Dictionary _list_android_devices(const Dictionary &p_params);
	Dictionary _get_android_preset_info(const Dictionary &p_params);
	Dictionary _deploy_to_android(const Dictionary &p_params);
	String _resolve_adb_path() const;
	Dictionary _run_blocking(const String &p_command, const List<String> &p_args) const;
	Dictionary _find_android_preset(const String &p_preset_name, int p_preset_index) const;
	bool _try_schedule_blocking_tool(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_pending) const;

protected:
	static void _bind_methods();

public:
	Dictionary _execute_blocking_tool_sync(const String &p_tool_name, const Dictionary &p_args);
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPExportTools();
	~JustAMCPExportTools();
};
