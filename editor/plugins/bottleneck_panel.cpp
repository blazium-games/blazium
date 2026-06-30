/**************************************************************************/
/*  bottleneck_panel.cpp                                                  */
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

#include "bottleneck_panel.h"

#include "core/input/shortcut.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/color.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/script_language.h"
#include "core/string/translation_server.h"
#include "core/templates/hash_set.h"
#include "editor/editor_interface.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/tree.h"

#define BOTTLENECK_SWARM 10

namespace {
struct Pattern {
	const char *id;
	const char *needles[6];
	int weight;
	const char *context;
	const char *note;
	const char *fix;
};

const Pattern PATTERNS[] = {
	{ "move_and_slide", { "move_and_slide(", "move_and_collide(", nullptr }, 8, "",
			"Kinematic solver runs every physics tick, per body.",
			"Only move bodies that need to. Disable physics processing on idle/off-screen bodies (set_physics_process(false)) or LOD distant ones." },
	{ "file_io", { "FileAccess.open", "FileAccess.get_file_as", ".csv", ".json", ".cfg", nullptr }, 12, "",
			"Disk read inside a per-frame / per-instance callback.",
			"Read the file ONCE (in _ready or an autoload) and pass the parsed data to instances. (The CSV-loot case from the issue.)" },
	{ "runtime_load", { nullptr }, 12, "",
			"Resource load at runtime in a hot callback (preload is exempt).",
			"Use preload() at parse time, or load once and cache. Never load() inside a process callback." },
	{ "node_lookup", { "get_node(", "find_child(", "find_children(", "$", nullptr }, 4, "",
			"Node path resolution every frame.",
			"Resolve once in _ready and store it (or use @onready). Do not re-walk the tree each frame." },
	{ "group_query", { "get_nodes_in_group(", "get_node_count_in_group(", nullptr }, 6, "",
			"Group scan every frame allocates and iterates the whole group.",
			"Cache the result, or maintain your own list via add/remove signals." },
	{ "allocation", { ".instantiate(", ".new(", ".duplicate(", nullptr }, 7, "",
			"Allocation / instancing inside a hot callback churns the heap.",
			"Pool objects: create up front, reuse, hide/show instead of free/instantiate." },
	{ "query_alloc", { "PhysicsRayQueryParameters", "PhysicsShapeQueryParameters", "RegEx.new(", nullptr }, 9, "",
			"Building a query / RegEx object every frame.",
			"Create the query/RegEx once and mutate its fields; compile RegEx in _ready." },
	{ "tree_walk", { "get_tree()", "get_viewport(", "get_window(", nullptr }, 3, "",
			"Tree / viewport accessor called every frame.",
			"Cache the reference once; these rarely change during a node's life." },
	{ "per_frame_print", { "print(", "printt(", "print_debug(", "print_rich(", "push_warning(", nullptr }, 10, "",
			"Logging every frame is expensive and floods the output.",
			"Gate it (e.g. every 60th frame) or remove it." },
	{ "string_build", { " str(", "+ str(", "String(", ".format(", nullptr }, 2, "",
			"String allocation / formatting every frame.",
			"Rebuild strings only when the value changes (from a setter or signal)." },
	{ "await_in_physics", { "await ", nullptr }, 5, "_physics_process",
			"await inside _physics_process splits work across ticks unpredictably.",
			"Keep the physics step synchronous; start awaited work from a coroutine elsewhere." },
};

const int PATTERN_COUNT = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

struct Row {
	BottleneckPanel::Finding f;
	int live_count = 0;
	int effective = 0;
};

struct RowSort {
	bool operator()(const Row &a, const Row &b) const {
		if (a.effective != b.effective) {
			return a.effective > b.effective;
		}
		return a.f.path < b.f.path;
	}
};

} //namespace

int BottleneckPanel::_leading_ws(const String &p_line) {
	int n = 0;
	for (int i = 0; i < p_line.length(); i++) {
		char32_t c = p_line[i];
		if (c == ' ' || c == '\t') {
			n++;
		} else {
			break;
		}
	}
	return n;
}

bool BottleneckPanel::_is_func_named(const String &p_after_func, const char *p_name) {
	String name = String(p_name);
	if (!p_after_func.begins_with(name)) {
		return false;
	}
	if (p_after_func.length() == name.length()) {
		return true;
	}
	char32_t next = p_after_func[name.length()];
	return next == '(' || next == ' ' || next == '\t' || next == ':';
}

String BottleneckPanel::_callback_at(const String &p_line) {
	String s = p_line.strip_edges();
	if (!s.begins_with("func ")) {
		return String();
	}
	String after = s.substr(5).strip_edges(true, false);

	if (_is_func_named(after, "_physics_process")) {
		return "_physics_process";
	}
	if (_is_func_named(after, "_process")) {
		return "_process";
	}
	return String();
}

bool BottleneckPanel::_line_has_runtime_load(const String &p_line) {
	if (p_line.find("ResourceLoader.load(") != -1) {
		return true;
	}
	int from = 0;
	while (true) {
		int pos = p_line.find("load(", from);
		if (pos == -1) {
			break;
		}
		if (pos >= 3 && p_line.substr(pos - 3, 3) == "pre") {
			from = pos + 5;
			continue;
		}
		return true;
	}
	return false;
}

void BottleneckPanel::_collect_gd_files(const String &p_dir, Vector<String> &r_files) const {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full = p_dir.path_join(name);
		if (dir->current_is_dir()) {
			_collect_gd_files(full, r_files);
		} else if (name.ends_with(".gd")) {
			r_files.push_back(full);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

void BottleneckPanel::_scan_file(const String &p_path, Vector<Finding> &r_findings) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return;
	}
	const String text = f->get_as_text();
	const Vector<String> lines = text.split("\n");

	for (int i = 0; i < lines.size(); i++) {
		const String cb = _callback_at(lines[i]);
		if (cb.is_empty()) {
			continue;
		}
		const int func_indent = _leading_ws(lines[i]);
		HashSet<String> already;

		int j = i + 1;
		for (; j < lines.size(); j++) {
			const String &raw = lines[j];
			const String bare = raw.strip_edges();
			if (bare.is_empty() || bare.begins_with("#")) {
				continue;
			}
			if (_leading_ws(raw) <= func_indent) {
				break;
			}
			for (int p = 0; p < PATTERN_COUNT; p++) {
				const Pattern &pat = PATTERNS[p];
				if (pat.context[0] != '\0' && cb != String(pat.context)) {
					continue;
				}
				if (already.has(pat.id)) {
					continue;
				}
				bool match = false;
				if (String(pat.id) == "runtime_load") {
					match = _line_has_runtime_load(raw);
				} else {
					for (int n = 0; n < 6 && pat.needles[n] != nullptr; n++) {
						if (raw.find(pat.needles[n]) != -1) {
							match = true;
							break;
						}
					}
				}
				if (match) {
					already.insert(pat.id);
					Finding fd;
					fd.path = p_path;
					fd.line = j + 1;
					fd.callback = cb;
					fd.id = pat.id;
					fd.weight = pat.weight;
					fd.note = pat.note;
					fd.fix = pat.fix;
					r_findings.push_back(fd);
				}
			}
		}
		i = j - 1;
	}
}

void BottleneckPanel::_run_static_scan() {
	static_findings.clear();
	Vector<String> files;
	_collect_gd_files("res://", files);
	scanned_files = files.size();
	for (const String &path : files) {
		_scan_file(path, static_findings);
	}
}

void BottleneckPanel::show_runtime_report(const Dictionary &p_payload) {
	live.clear();
	const Array scripts = p_payload.get("scripts", Array());
	for (int i = 0; i < scripts.size(); i++) {
		const Dictionary d = scripts[i];
		LiveAgg a;
		a.klass = d.get("class", String());
		a.process = (int)(int64_t)d.get("process", 0);
		a.physics = (int)(int64_t)d.get("physics", 0);
		a.total = (int)(int64_t)d.get("total", 0);
		live[String(d.get("path", String()))] = a;
	}
	live_frame = (int)(int64_t)p_payload.get("frame", 0);
	live_total_nodes = (int)(int64_t)p_payload.get("total_nodes", 0);
	have_live = true;

	if (static_findings.is_empty()) {
		_run_static_scan();
	}
	_render();
}

void BottleneckPanel::_render() {
	results->clear();
	TreeItem *root = results->create_item();

	Vector<Row> rows;
	for (const Finding &fd : static_findings) {
		Row r;
		r.f = fd;
		r.live_count = 0;
		if (have_live && live.has(fd.path)) {
			const LiveAgg &a = live[fd.path];
			r.live_count = (fd.callback == "_physics_process") ? a.physics : a.process;
		}
		r.effective = fd.weight * MAX(1, r.live_count);
		rows.push_back(r);
	}
	rows.sort_custom<RowSort>();

	HashMap<String, TreeItem *> file_items;
	for (const Row &r : rows) {
		const Finding &fd = r.f;
		TreeItem *parent;
		if (file_items.has(fd.path)) {
			parent = file_items[fd.path];
		} else {
			parent = results->create_item(root);
			parent->set_text(0, fd.path.trim_prefix("res://"));
			parent->set_selectable(0, false);
			parent->set_selectable(1, false);
			file_items[fd.path] = parent;
		}

		bool swarm = r.live_count >= BOTTLENECK_SWARM;

		String label = fd.id + "  (" + fd.callback + ", line " + itos(fd.line) + ")";
		if (r.live_count > 0) {
			label += "   x" + itos(r.live_count) + " live";
		}
		if (swarm) {
			label += "  [SWARM]";
		}

		TreeItem *it = results->create_item(parent);
		it->set_text(0, label);
		it->set_text(1, fd.fix);
		it->set_tooltip_text(0, fd.note);
		it->set_tooltip_text(1, fd.note);

		Color c;
		if (swarm || fd.weight >= 10) {
			c = Color(0.93, 0.36, 0.36);
		} else if (fd.weight >= 6) {
			c = Color(0.95, 0.66, 0.30);
		} else {
			c = Color(0.92, 0.86, 0.40);
		}
		it->set_custom_color(0, c);

		Dictionary md;
		md["path"] = fd.path;
		md["line"] = fd.line;
		it->set_metadata(0, md);
	}

	if (have_live) {
		summary_label->set_text(TTR("Live: double-click a row to jump.") +
				vformat("   (frame %d, %d nodes, ranked by cost x live instances)", live_frame, live_total_nodes));
	} else {
		summary_label->set_text(TTR("Double-click a row to jump to the line.") +
				vformat("   (%d scripts, %d findings)", scanned_files, static_findings.size()));
	}
}

void BottleneckPanel::_scan_pressed() {
	_run_static_scan();
	have_live = false;
	live.clear();
	_render();
}

void BottleneckPanel::_scan_live_pressed() {
	if (debugger.is_null()) {
		summary_label->set_text(TTR("Live scan is unavailable."));
		return;
	}
	int n = debugger->request_live_scan();
	if (n == 0) {
		summary_label->set_text(TTR("No running project — press Play (F5), then Scan Live Tree."));
	} else {
		summary_label->set_text(TTR("Requested live data from the running project..."));
	}
}

void BottleneckPanel::_item_activated() {
	TreeItem *it = results->get_selected();
	if (!it) {
		return;
	}
	const Variant md = it->get_metadata(0);
	if (md.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Dictionary d = md;
	const String path = d.get("path", String());
	const int line = (int)(int64_t)d.get("line", Variant(1));
	if (path.is_empty()) {
		return;
	}
	Ref<Script> scr = ResourceLoader::load(path, "Script");
	if (scr.is_valid()) {
		EditorInterface::get_singleton()->set_main_screen_editor("Script");
		EditorInterface::get_singleton()->edit_script(scr, line);
	}
}

BottleneckPanel::BottleneckPanel() {
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_custom_minimum_size(Size2(0, 200) * EDSCALE);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	add_child(toolbar);

	scan_button = memnew(Button);
	scan_button->set_text(TTR("Scan Project"));
	scan_button->set_tooltip_text(TTR("Statically scan every .gd for costly calls in _process / _physics_process."));
	scan_button->connect("pressed", callable_mp(this, &BottleneckPanel::_scan_pressed));
	toolbar->add_child(scan_button);

	live_button = memnew(Button);
	live_button->set_text(TTR("Scan Live Tree"));
	live_button->set_tooltip_text(TTR("Ask the running project how many instances are running each callback right now."));
	live_button->connect("pressed", callable_mp(this, &BottleneckPanel::_scan_live_pressed));
	toolbar->add_child(live_button);

	summary_label = memnew(Label);
	summary_label->set_text(TTR("Finds costly calls in _process / _physics_process."));
	toolbar->add_child(summary_label);

	results = memnew(Tree);
	results->set_v_size_flags(SIZE_EXPAND_FILL);
	results->set_columns(2);
	results->set_hide_root(true);
	results->set_column_titles_visible(true);
	results->set_column_title(0, TTR("Issue"));
	results->set_column_title(1, TTR("Suggested fix"));
	results->set_column_expand(0, true);
	results->set_column_expand(1, true);
	results->set_column_custom_minimum_width(0, 320 * EDSCALE);
	results->connect("item_activated", callable_mp(this, &BottleneckPanel::_item_activated));
	add_child(results);
}

int BottleneckDebuggerPlugin::request_live_scan() {
	int messaged = 0;
	int count = get_sessions().size();
	for (int i = 0; i < count; i++) {
		Ref<EditorDebuggerSession> s = get_session(i);
		if (s.is_valid() && s->is_active()) {
			s->send_message("bottleneck:capture", Array());
			messaged++;
		}
	}
	return messaged;
}

bool BottleneckDebuggerPlugin::has_capture(const String &p_capture) const {
	return p_capture == "bottleneck";
}

bool BottleneckDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session) {
	if (p_message == "bottleneck:report") {
		if (panel) {
			Dictionary payload;
			if (p_data.size() > 0) {
				payload = p_data[0];
			}
			panel->show_runtime_report(payload);
		}
		return true;
	}
	return false;
}

BottleneckEditorPlugin::BottleneckEditorPlugin() {
	panel = memnew(BottleneckPanel);
	panel->set_name("Bottlenecks");
	add_control_to_bottom_panel(panel, TTR("Bottlenecks"));

	debugger.instantiate();
	debugger->set_panel(panel);
	panel->set_debugger(debugger);
	add_debugger_plugin(debugger);
}
