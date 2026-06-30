/**************************************************************************/
/*  bottleneck_debugger.cpp                                               */
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

#include "bottleneck_debugger.h"

#include "core/config/engine.h"
#include "core/debugger/engine_debugger.h"
#include "core/object/script_language.h"
#include "core/templates/hash_map.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace {
struct Agg {
	String klass;
	int process = 0;
	int physics = 0;
	int total = 0;
};

void _aggregate(Node *p_node, HashMap<String, Agg> &r_map, int &r_total_nodes) {
	r_total_nodes++;

	ScriptInstance *si = p_node->get_script_instance();
	if (si) {
		Ref<Script> scr = si->get_script();
		if (scr.is_valid()) {
			const String path = scr->get_path();
			if (!path.is_empty()) {
				Agg &a = r_map[path];
				if (a.klass.is_empty()) {
					a.klass = p_node->get_class();
				}
				a.total++;
				if (p_node->is_processing()) {
					a.process++;
				}
				if (p_node->is_physics_processing()) {
					a.physics++;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_aggregate(p_node->get_child(i), r_map, r_total_nodes);
	}
}

} //namespace

Error BottleneckDebugger::parse_message(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	if (p_msg != "capture") {
		r_captured = false;
		return OK;
	}
	r_captured = true;

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree || !scene_tree->get_root()) {
		EngineDebugger::get_singleton()->send_message("bottleneck:report", Array());
		return OK;
	}

	HashMap<String, Agg> map;
	int total_nodes = 0;
	_aggregate(scene_tree->get_root(), map, total_nodes);

	Array scripts;
	for (const KeyValue<String, Agg> &kv : map) {
		Dictionary d;
		d["path"] = kv.key;
		d["class"] = kv.value.klass;
		d["process"] = kv.value.process;
		d["physics"] = kv.value.physics;
		d["total"] = kv.value.total;
		scripts.push_back(d);
	}

	Dictionary payload;
	payload["frame"] = (int)Engine::get_singleton()->get_physics_frames();
	payload["total_nodes"] = total_nodes;
	payload["scripts"] = scripts;

	Array data;
	data.push_back(payload);
	EngineDebugger::get_singleton()->send_message("bottleneck:report", data);
	return OK;
}
