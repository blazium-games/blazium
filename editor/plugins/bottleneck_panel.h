/**************************************************************************/
/*  bottleneck_panel.h                                                    */
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

#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "editor/plugins/editor_debugger_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class Button;
class Label;
class Tree;
class BottleneckPanel;

class BottleneckDebuggerPlugin : public EditorDebuggerPlugin {
	GDCLASS(BottleneckDebuggerPlugin, EditorDebuggerPlugin);

	BottleneckPanel *panel = nullptr;

protected:
	static void _bind_methods() {}

public:
	void set_panel(BottleneckPanel *p_panel) { panel = p_panel; }

	int request_live_scan();

	virtual bool has_capture(const String &p_capture) const override;
	virtual bool capture(const String &p_message, const Array &p_data, int p_session) override;
};

class BottleneckPanel : public VBoxContainer {
	GDCLASS(BottleneckPanel, VBoxContainer);

public:
	struct Finding {
		String path;
		int line = 0;
		String callback;
		String id;
		int weight = 0;
		String note;
		String fix;
	};

private:
	struct LiveAgg {
		String klass;
		int process = 0;
		int physics = 0;
		int total = 0;
	};

	Button *scan_button = nullptr;
	Button *live_button = nullptr;
	Label *summary_label = nullptr;
	Tree *results = nullptr;

	Ref<BottleneckDebuggerPlugin> debugger;

	Vector<Finding> static_findings;
	HashMap<String, LiveAgg> live;
	bool have_live = false;
	int live_frame = 0;
	int live_total_nodes = 0;
	int scanned_files = 0;

	void _scan_pressed();
	void _scan_live_pressed();
	void _item_activated();

	void _run_static_scan();
	void _render();

	void _collect_gd_files(const String &p_dir, Vector<String> &r_files) const;
	void _scan_file(const String &p_path, Vector<Finding> &r_findings) const;

	static int _leading_ws(const String &p_line);
	static String _callback_at(const String &p_line);
	static bool _is_func_named(const String &p_after_func, const char *p_name);
	static bool _line_has_runtime_load(const String &p_line);

protected:
	static void _bind_methods() {}

public:
	void set_debugger(const Ref<BottleneckDebuggerPlugin> &p_debugger) { debugger = p_debugger; }
	void show_runtime_report(const Dictionary &p_payload);

	BottleneckPanel();
};

class BottleneckEditorPlugin : public EditorPlugin {
	GDCLASS(BottleneckEditorPlugin, EditorPlugin);

	BottleneckPanel *panel = nullptr;
	Ref<BottleneckDebuggerPlugin> debugger;

public:
	virtual String get_plugin_name() const override { return "Bottlenecks"; }

	BottleneckEditorPlugin();
};
