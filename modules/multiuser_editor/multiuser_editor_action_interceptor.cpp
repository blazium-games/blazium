/**************************************************************************/
/*  multiuser_editor_action_interceptor.cpp                               */
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

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"
#include "multiuser_editor_action_interceptor.h"

#include "multiuser_editor_constants.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/script_language.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/settings/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"

static const char *MULTIUSER_SAFE_NODE_BASES[] = {
	"Node2D",
	"Node3D",
	"Control",
	"Camera2D",
	"Camera3D",
	"Light2D",
	"Light3D",
	"MeshInstance3D",
	"MeshInstance2D",
	"CollisionShape2D",
	"CollisionShape3D",
	"CollisionObject2D",
	"CollisionObject3D",
	"StaticBody2D",
	"StaticBody3D",
	"RigidBody2D",
	"RigidBody3D",
	"CharacterBody2D",
	"CharacterBody3D",
	"Area2D",
	"Area3D",
	"Sprite2D",
	"Sprite3D",
	"AnimatedSprite2D",
	"AnimatedSprite3D",
	"AnimationPlayer",
	"AnimationTree",
	"Timer",
	"AudioStreamPlayer",
	"AudioStreamPlayer2D",
	"AudioStreamPlayer3D",
	"GPUParticles2D",
	"GPUParticles3D",
	"CPUParticles2D",
	"CPUParticles3D",
	"RayCast2D",
	"RayCast3D",
	"Path2D",
	"Path3D",
	"PathFollow2D",
	"PathFollow3D",
	"CanvasLayer",
	"SubViewport",
	"SubViewportContainer",
	"Label",
	"Button",
	"LineEdit",
	"TextEdit",
	"Panel",
	"HBoxContainer",
	"VBoxContainer",
	"GridContainer",
	"MarginContainer",
	"CenterContainer",
	"ScrollContainer",
	"HSplitContainer",
	"VSplitContainer",
	"TabContainer",
	"ColorRect",
	"TextureRect",
	"TextureButton",
	"RichTextLabel",
	"CSGBox3D",
	"CSGSphere3D",
	"CSGCylinder3D",
	"CSGMesh3D",
	"CSGCombiner3D",
	"DirectionalLight3D",
	"OmniLight3D",
	"SpotLight3D",
	"WorldEnvironment",
	"NavigationRegion2D",
	"NavigationRegion3D",
	"Marker2D",
	"Marker3D",
	"VisibleOnScreenNotifier2D",
	"VisibleOnScreenNotifier3D",
};

void MultiuserEditorActionInterceptor::_collect_scene_nodes(Node *p_node, Vector<Node *> &r_nodes) const {
	if (!p_node) {
		return;
	}
	if (p_node == scene_root || p_node->get_owner() == scene_root) {
		r_nodes.push_back(p_node);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_scene_nodes(p_node->get_child(i), r_nodes);
	}
}

Vector<Node *> MultiuserEditorActionInterceptor::_get_all_scene_nodes() const {
	Vector<Node *> nodes;
	_collect_scene_nodes(scene_root, nodes);
	return nodes;
}

Array MultiuserEditorActionInterceptor::_get_sync_properties(Node *p_node) const {
	Array properties;
	if (!p_node) {
		return properties;
	}

	List<PropertyInfo> property_list;
	p_node->get_property_list(&property_list);

	for (const PropertyInfo &pi : property_list) {
		String name = pi.name;

		if (name.begins_with("_") || name == "metadata" || name == "script" || name == "multiplayer") {
			continue;
		}

		if (!(pi.usage & PROPERTY_USAGE_EDITOR) && !(pi.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		if (name == "owner" || name == "unique_name_in_owner" || name == "name" || name == "process_mode" || name == "editor_description") {
			continue;
		}

		Dictionary entry;
		entry["name"] = name;
		entry["value"] = p_node->get(name);
		properties.append(entry);
	}
	return properties;
}

Node *MultiuserEditorActionInterceptor::_resolve_node(const String &p_path) const {
	if (!scene_root || p_path.is_empty()) {
		return nullptr;
	}
	String clean_path = lock_manager ? lock_manager->clean_path(p_path) : p_path;
	if (!is_safe_node_path(clean_path)) {
		const String msg = "Multiuser editor: Dropped unsafe node path resolution: " + clean_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
		return nullptr;
	}
	if (clean_path == "." || clean_path == String(scene_root->get_name())) {
		return scene_root;
	}
	return scene_root->has_node(NodePath(clean_path)) ? scene_root->get_node(NodePath(clean_path)) : nullptr;
}

bool MultiuserEditorActionInterceptor::is_safe_node_path(const String &p_path) {
	if (p_path.is_empty()) {
		return true;
	}
	if (p_path.length() > multiuser_editor::kPathLengthMax) {
		return false;
	}
	if (p_path.begins_with("/")) {
		return false;
	}
	if (p_path.contains("..")) {
		return false;
	}
	for (int i = 0; i < p_path.length(); i++) {
		const char32_t c = p_path[i];
		if (c == 0 || c < 0x20) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_file_path(const String &p_path) {
	String dummy;
	return canonicalize_res_path(p_path, dummy);
}

bool MultiuserEditorActionInterceptor::canonicalize_res_path(const String &p_in, String &r_out) {
	r_out = String();
	String input = p_in.strip_edges();
	if (input.is_empty()) {
		return false;
	}
	if (input.length() > multiuser_editor::kPathLengthMax) {
		return false;
	}
	for (int i = 0; i < input.length(); i++) {
		const char32_t c = input[i];
		if (c == 0 || c < 0x20) {
			return false;
		}
	}

	if (input.length() < 6) {
		return false;
	}
	String scheme = input.substr(0, 6).to_lower();
	if (scheme != "res://") {
		return false;
	}
	String body = input.substr(6, input.length() - 6);
	body = body.replace("\\", "/");

	while (body.contains("//")) {
		body = body.replace("//", "/");
	}
	if (body.is_empty()) {
		return false;
	}

	Vector<String> segments = body.split("/", true);
	Vector<String> clean_segments;
	for (int i = 0; i < segments.size(); i++) {
		const String &seg = segments[i];
		if (seg.is_empty()) {
			return false;
		}
		if (seg == "." || seg == "..") {
			return false;
		}
		if (seg.begins_with(".~lock")) {
			return false;
		}
		clean_segments.push_back(seg);
	}
	if (clean_segments.is_empty()) {
		return false;
	}

	String canonical = "res://";
	for (int i = 0; i < clean_segments.size(); i++) {
		if (i > 0) {
			canonical += "/";
		}
		canonical += clean_segments[i];
	}

	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		String resource_root = ps->get_resource_path();
		if (!resource_root.is_empty()) {
			String absolute = ps->globalize_path(canonical);
			absolute = absolute.simplify_path();
			String root_norm = resource_root.simplify_path();
			if (!root_norm.ends_with("/")) {
				root_norm += "/";
			}
			if (!absolute.begins_with(root_norm)) {
				return false;
			}
		}
	}

	r_out = canonical;
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_property_name(const String &p_name) {
	if (p_name.is_empty() || p_name.length() > multiuser_editor::kPropertyNameMax) {
		return false;
	}
	const char32_t first = p_name[0];
	const bool first_ok = (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first == '_';
	if (!first_ok) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '/';
		if (!ok) {
			return false;
		}
	}
	if (p_name == "script" || p_name == "script_class") {
		return false;
	}
	if (p_name.begins_with("metadata/") || p_name.begins_with("__") || p_name.begins_with("editor_")) {
		return false;
	}
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_branch_name(const String &p_name) {
	if (p_name.is_empty() || p_name.length() > multiuser_editor::kBranchNameMax) {
		return false;
	}
	const char32_t first = p_name[0];
	if (first == '-' || first == '.' || first == '/') {
		return false;
	}
	const char32_t last = p_name[p_name.length() - 1];
	if (last == '/' || last == '.') {
		return false;
	}
	if (p_name.contains("..") || p_name.contains("@{") || p_name.contains("//")) {
		return false;
	}
	if (p_name.ends_with(".lock")) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				c == '_' || c == '.' || c == '/' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_remote_name(const String &p_name) {
	if (p_name.is_empty() || p_name.length() > multiuser_editor::kRemoteNameMax) {
		return false;
	}

	if (p_name[0] == '-') {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				c == '_' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_remote_value(const Variant &p_value, int64_t p_byte_cap) {
	if (p_byte_cap <= 0) {
		p_byte_cap = multiuser_editor::kRemoteValueDefaultByteCap;
	}
	const Variant::Type t = p_value.get_type();
	switch (t) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::RECT2:
		case Variant::RECT2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::TRANSFORM2D:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
		case Variant::PLANE:
		case Variant::QUATERNION:
		case Variant::AABB:
		case Variant::BASIS:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
		case Variant::COLOR:
		case Variant::RID:
			return true;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH: {
			const String s = String(p_value);
			if (int64_t(s.length()) > MIN(int64_t(multiuser_editor::kRemoteStringMaxChars), p_byte_cap)) {
				return false;
			}
			return true;
		}
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray a = p_value;
			return int64_t(a.size()) <= p_byte_cap;
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray a = p_value;
			if (a.size() > multiuser_editor::kPackedArrayMaxElems) {
				return false;
			}
			int64_t total = 0;
			for (int i = 0; i < a.size(); i++) {
				total += a[i].length();
				if (total > p_byte_cap) {
					return false;
				}
			}
			return true;
		}
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array a = p_value;
			return int64_t(a.size()) * 4 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array a = p_value;
			return int64_t(a.size()) * 8 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array a = p_value;
			return int64_t(a.size()) * 4 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array a = p_value;
			return int64_t(a.size()) * 8 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array a = p_value;
			return int64_t(a.size()) * 8 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array a = p_value;
			return int64_t(a.size()) * 12 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray a = p_value;
			return int64_t(a.size()) * 16 <= p_byte_cap && a.size() <= multiuser_editor::kPackedArrayMaxElems;
		}
		case Variant::ARRAY: {
			const Array a = p_value;
			return a.size() <= multiuser_editor::kArrayMaxLen;
		}
		case Variant::DICTIONARY: {
			const Dictionary d = p_value;
			return d.size() <= multiuser_editor::kDictionaryMaxKeys;
		}
		default:
			return false;
	}
}

bool MultiuserEditorActionInterceptor::is_safe_commit_message(const String &p_message) {
	if (p_message.is_empty() || p_message.length() > multiuser_editor::kCommitMessageMax) {
		return false;
	}
	for (int i = 0; i < p_message.length(); i++) {
		const char32_t c = p_message[i];
		if (c == 0 || c == '\r') {
			return false;
		}
		if (c < 0x20 && c != '\n' && c != '\t') {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorActionInterceptor::is_safe_node_name(const String &p_name) {
	if (p_name.is_empty() || p_name.length() > multiuser_editor::kNodeNameMax) {
		return false;
	}
	if (p_name[0] == '@') {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if (c == 0 || c < 0x20) {
			return false;
		}
		if (c == '/' || c == ':') {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorActionInterceptor::_is_safe_node_type(const String &p_type) const {
	if (!ClassDB::class_exists(p_type)) {
		return false;
	}

	static const char *DENY_PREFIXES[] = { "Editor", "ScriptEditor" };
	for (const char *prefix : DENY_PREFIXES) {
		if (p_type.begins_with(prefix)) {
			return false;
		}
	}
	for (const char *safe_base : MULTIUSER_SAFE_NODE_BASES) {
		if (p_type == safe_base || ClassDB::is_parent_class(p_type, safe_base)) {
			return true;
		}
	}
	return false;
}

bool MultiuserEditorActionInterceptor::_is_edited_scene_node(Node *p_node) const {
	return p_node && scene_root && scene_root->is_inside_tree() && (p_node == scene_root || p_node->get_owner() == scene_root);
}

Dictionary MultiuserEditorActionInterceptor::_make_action(const String &p_type, const Dictionary &p_data) const {
	Dictionary action;
	Dictionary data = p_data;
	action["type"] = p_type;
	if (data.has("node_path")) {
		action["node_path"] = data["node_path"];
		data.erase("node_path");
	}
	action["data"] = data;
	return action;
}

String MultiuserEditorActionInterceptor::_get_script_path(Node *p_node) const {
	if (!p_node) {
		return String();
	}
	Variant script_variant = p_node->get("script");
	if (script_variant.get_type() != Variant::OBJECT) {
		return String();
	}
	Object *script_obj = script_variant;
	Resource *res = Object::cast_to<Resource>(script_obj);
	return res ? res->get_path() : String();
}

String MultiuserEditorActionInterceptor::_get_script_content(Node *p_node) const {
	if (!p_node) {
		return String();
	}
	Variant script_variant = p_node->get("script");
	if (script_variant.get_type() != Variant::OBJECT) {
		return String();
	}
	Object *script_obj = script_variant;
	Script *script = Object::cast_to<Script>(script_obj);
	return script ? script->get_source_code() : String();
}

void MultiuserEditorActionInterceptor::set_context(Node *p_scene_root, EditorSelection *p_selection, EditorUndoRedoManager *p_undo_redo, MultiuserEditorLockManager *p_lock_manager) {
	scene_root = p_scene_root;
	selection = p_selection;
	undo_redo = p_undo_redo;
	lock_manager = p_lock_manager;
}

Dictionary MultiuserEditorActionInterceptor::capture_selection() const {
	Dictionary data;
	Array paths;
	if (!selection || !scene_root) {
		return _make_action(multiuser_editor::kActionSelect, data);
	}
	TypedArray<Node> selected = selection->get_selected_nodes();
	for (int i = 0; i < selected.size(); i++) {
		Node *node = Object::cast_to<Node>(selected[i]);
		if (_is_edited_scene_node(node)) {
			String path = String(scene_root->get_path_to(node));
			paths.append(lock_manager ? lock_manager->clean_path(path) : path);
		}
	}
	data["paths"] = paths;
	return _make_action(multiuser_editor::kActionSelect, data);
}

Dictionary MultiuserEditorActionInterceptor::capture_node_add(Node *p_node) const {
	Dictionary data;
	if (!_is_edited_scene_node(p_node) || p_node == scene_root || !p_node->get_parent()) {
		return data;
	}
	String path = String(scene_root->get_path_to(p_node));
	String parent_path = String(scene_root->get_path_to(p_node->get_parent()));
	path = lock_manager ? lock_manager->clean_path(path) : path;
	parent_path = lock_manager ? lock_manager->clean_path(parent_path) : parent_path;
	if (lock_manager && lock_manager->is_locked(parent_path)) {
		const String msg = vformat("Multiuser editor: add blocked by lock from %s", lock_manager->get_lock_owner(parent_path));
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
		locked_add_blocked = true;
		return data;
	}
	data["node_path"] = path;
	data["parent_path"] = parent_path;
	data["node_name"] = lock_manager ? lock_manager->clean_path(String(p_node->get_name())) : String(p_node->get_name());
	data["node_type"] = p_node->get_class();
	return _make_action(multiuser_editor::kActionNodeAdd, data);
}

Dictionary MultiuserEditorActionInterceptor::capture_node_delete(Node *p_node) const {
	Dictionary data;
	if (!_is_edited_scene_node(p_node) || p_node == scene_root) {
		return data;
	}
	String path = String(scene_root->get_path_to(p_node));
	path = lock_manager ? lock_manager->clean_path(path) : path;
	if (lock_manager && lock_manager->is_locked(path)) {
		const String msg = vformat("Multiuser editor: delete blocked by lock from %s", lock_manager->get_lock_owner(path));
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
		locked_delete_blocked = true;
		return data;
	}
	data["node_path"] = path;
	data["node_name"] = lock_manager ? lock_manager->clean_path(String(p_node->get_name())) : String(p_node->get_name());
	data["parent_path"] = lock_manager ? lock_manager->clean_path(String(scene_root->get_path_to(p_node->get_parent()))) : String(scene_root->get_path_to(p_node->get_parent()));
	data["node_type"] = p_node->get_class();
	return _make_action(multiuser_editor::kActionNodeDelete, data);
}

bool MultiuserEditorActionInterceptor::consume_locked_add_blocked() {
	bool was_blocked = locked_add_blocked;
	locked_add_blocked = false;
	return was_blocked;
}

bool MultiuserEditorActionInterceptor::consume_locked_delete_blocked() {
	bool was_blocked = locked_delete_blocked;
	locked_delete_blocked = false;
	return was_blocked;
}

void MultiuserEditorActionInterceptor::undo_last_scene_action() {
	if (!undo_redo || !scene_root) {
		return;
	}
	int history_id = undo_redo->get_history_id_for_object(scene_root);
	if (undo_redo->has_history(history_id)) {
		undo_redo->undo_history(history_id);
	} else {
		undo_redo->undo();
	}
}

void MultiuserEditorActionInterceptor::build_initial_state_actions(Vector<Dictionary> &r_actions) const {
	if (!scene_root) {
		return;
	}
	Vector<Node *> nodes = _get_all_scene_nodes();
	for (Node *node : nodes) {
		if (node == scene_root || !node->get_parent()) {
			continue;
		}
		Dictionary add_action = capture_node_add(node);
		if (!add_action.is_empty()) {
			r_actions.push_back(add_action);
		}
		String path = String(scene_root->get_path_to(node));
		path = lock_manager ? lock_manager->clean_path(path) : path;
		Array props = _get_sync_properties(node);
		for (int i = 0; i < props.size(); i++) {
			Dictionary entry = props[i];
			Dictionary data;
			data["node_path"] = path;
			data["property"] = entry.get("name", "");
			data["value"] = entry.get("value", Variant());
			r_actions.push_back(_make_action(multiuser_editor::kActionProperty, data));
		}
		String script_path = _get_script_path(node);
		if (!script_path.is_empty()) {
			Dictionary data;
			data["node_path"] = path;
			data["script_path"] = script_path;
			data["script_content"] = _get_script_content(node);
			r_actions.push_back(_make_action(multiuser_editor::kActionScriptAttach, data));
		}
	}
}

uint64_t MultiuserEditorActionInterceptor::generate_scene_hash(Node *p_root) const {
	if (!p_root) {
		return 0;
	}
	uint64_t h = hash_djb2_one_64((uint64_t)p_root->get_class_name().hash());
	h = hash_djb2_one_64((uint64_t)p_root->get_name().hash(), h);
	for (int i = 0; i < p_root->get_child_count(); i++) {
		h = hash_djb2_one_64(generate_scene_hash(p_root->get_child(i)), h);
	}
	return h;
}

void MultiuserEditorActionInterceptor::poll_scene_changes(Vector<Dictionary> &r_actions) {
	if (!scene_root) {
		return;
	}
	for (Node *node : _get_all_scene_nodes()) {
		String path = String(scene_root->get_path_to(node));
		path = lock_manager ? lock_manager->clean_path(path) : path;
		Variant current_transform;
		if (Node2D *node_2d = Object::cast_to<Node2D>(node)) {
			current_transform = node_2d->get_transform();
		} else if (Node3D *node_3d = Object::cast_to<Node3D>(node)) {
			current_transform = node_3d->get_transform();
		}
		if (current_transform.get_type() != Variant::NIL) {
			bool significant = true;
			if (transform_cache.has(path)) {
				if (current_transform.get_type() == Variant::TRANSFORM3D) {
					Transform3D t1 = transform_cache[path];
					Transform3D t2 = current_transform;
					significant = t1.origin.distance_to(t2.origin) > 0.001 || t1.basis.get_rotation_quaternion().angle_to(t2.basis.get_rotation_quaternion()) > 0.001;
				} else if (current_transform.get_type() == Variant::TRANSFORM2D) {
					Transform2D t1 = transform_cache[path];
					Transform2D t2 = current_transform;
					significant = t1.get_origin().distance_to(t2.get_origin()) > 0.001 || Math::abs(t1.get_rotation() - t2.get_rotation()) > 0.001;
				}
			}
			if (significant && !remote_changed_paths.has(path) && (!lock_manager || !lock_manager->is_locked(path))) {
				Dictionary data;
				data["node_path"] = path;
				data["property"] = "transform";
				data["value"] = current_transform;
				data["old_value"] = transform_cache.has(path) ? transform_cache[path] : Variant();
				r_actions.push_back(_make_action(multiuser_editor::kActionProperty, data));
			}
			transform_cache[path] = current_transform;
		}

		Dictionary current_properties;
		Array props = _get_sync_properties(node);
		for (int i = 0; i < props.size(); i++) {
			Dictionary entry = props[i];
			current_properties[String(entry["name"])] = entry["value"];
		}
		if (property_cache.has(path) && !remote_changed_paths.has(path) && (!lock_manager || !lock_manager->is_locked(path))) {
			Dictionary previous = property_cache[path];
			Array keys = current_properties.keys();
			for (int i = 0; i < keys.size(); i++) {
				String key = String(keys[i]);
				if (!previous.has(key) || previous[key] != current_properties[key]) {
					Dictionary data;
					data["node_path"] = path;
					data["property"] = key;
					data["value"] = current_properties[key];
					data["old_value"] = previous.has(key) ? previous[key] : Variant();
					r_actions.push_back(_make_action(multiuser_editor::kActionProperty, data));
				}
			}
		}
		property_cache[path] = current_properties;

		String script_path = _get_script_path(node);
		String old_script = script_cache.has(path) ? script_cache[path] : String();
		if (script_cache.has(path) && old_script != script_path && !remote_changed_paths.has(path)) {
			Dictionary data;
			data["node_path"] = path;
			if (script_path.is_empty()) {
				data["script_path"] = old_script;
				r_actions.push_back(_make_action(multiuser_editor::kActionScriptDetach, data));
			} else {
				data["script_path"] = script_path;
				data["script_content"] = _get_script_content(node);
				r_actions.push_back(_make_action(multiuser_editor::kActionScriptAttach, data));
			}
		}
		script_cache[path] = script_path;
	}
	remote_changed_paths.clear();
}

void MultiuserEditorActionInterceptor::apply_inspector_property_lock(Object *p_undo_redo, Object *p_modified_object, const String &p_property, const Variant &p_new_value) {
	Node *node = Object::cast_to<Node>(p_modified_object);
	if (!_is_edited_scene_node(node) || !lock_manager) {
		return;
	}
	String path = String(scene_root->get_path_to(node));
	path = lock_manager->clean_path(path);
	if (!lock_manager->is_locked(path)) {
		return;
	}
	EditorUndoRedoManager *manager = Object::cast_to<EditorUndoRedoManager>(p_undo_redo);
	if (!manager) {
		manager = undo_redo;
	}
	{
		const String msg = vformat("Multiuser editor: property edit blocked by lock from %s", lock_manager->get_lock_owner(path));
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
	}
	if (manager) {
		Variant current_value = node->get(p_property);
		manager->add_do_property(node, p_property, current_value);
		manager->add_undo_property(node, p_property, current_value);
	}
}

void MultiuserEditorActionInterceptor::apply_remote_action(const Dictionary &p_action) {
	String type = String(p_action.get("type", ""));
	Dictionary data = p_action.get("data", Dictionary());
	if (type == multiuser_editor::kActionSelect) {
		return;
	}
	if (type == multiuser_editor::kActionProperty) {
		String path = String(p_action.get("node_path", data.get("path", "")));
		Node *node = _resolve_node(path);
		if (!node) {
			return;
		}
		String property = String(data.get("property", ""));
		if (!is_safe_property_name(property)) {
			const String msg = "Multiuser editor: Dropped unsafe property name: " + property;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
			return;
		}
		Variant value = data.get("value", Variant());
		int64_t prop_cap = multiuser_editor::kRemoteValueDefaultByteCap;
		EditorSettings *editor_settings = EditorSettings::get_singleton();
		if (editor_settings && editor_settings->has_setting("blazium/multiuser_editor/limits/property_value_max_bytes")) {
			prop_cap = int64_t(editor_settings->get("blazium/multiuser_editor/limits/property_value_max_bytes"));
		}
		if (!is_safe_remote_value(value, prop_cap)) {
			const String msg = vformat("Multiuser editor: Dropped oversized remote property value (type=%d) on %s::%s", int(value.get_type()), path, property);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
			return;
		}
		if (undo_redo) {
			undo_redo->create_action(TTR("Apply Remote Multiuser Property"), UndoRedo::MERGE_DISABLE, node);
			undo_redo->add_do_property(node, property, value);
			undo_redo->add_undo_property(node, property, node->get(property));
			undo_redo->commit_action();
		} else {
			node->set(property, value);
		}
		remote_changed_paths.insert(lock_manager ? lock_manager->clean_path(path) : path);
	} else if (type == multiuser_editor::kActionNodeAdd) {
		String parent_path = String(data.get("parent_path", data.get("parent", "")));
		String name = String(data.get("node_name", data.get("name", "")));
		String node_type = String(data.get("node_type", data.get("type", "")));
		Node *parent = _resolve_node(parent_path);
		if (!parent || name.is_empty() || !is_safe_node_name(name) || !_is_safe_node_type(node_type) || parent->has_node(NodePath(name))) {
			if (!name.is_empty() && !is_safe_node_name(name)) {
				const String msg = "Multiuser editor: Dropped unsafe node name: " + name;
				print_verbose(msg);
				_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
			}
			return;
		}
		Object *obj = ClassDB::instantiate(node_type);
		Node *node = Object::cast_to<Node>(obj);
		if (!node) {
			if (obj) {
				memdelete(obj);
			}
			return;
		}
		node->set_name(name);
		if (undo_redo) {
			undo_redo->create_action(TTR("Apply Remote Multiuser Add"), UndoRedo::MERGE_DISABLE, parent);
			undo_redo->add_do_method(parent, "add_child", node, true);
			undo_redo->add_do_method(node, "set_owner", scene_root);
			undo_redo->add_do_reference(node);
			undo_redo->add_undo_method(parent, "remove_child", node);
			undo_redo->commit_action(true);
		} else {
			parent->add_child(node, true);
			node->set_owner(scene_root);
		}
		remote_changed_paths.insert(lock_manager ? lock_manager->clean_path(String(scene_root->get_path_to(node))) : String(scene_root->get_path_to(node)));
	} else if (type == multiuser_editor::kActionNodeDelete) {
		String path = String(p_action.get("node_path", data.get("path", "")));
		Node *node = _resolve_node(path);
		if (!node || node == scene_root || !node->get_parent()) {
			return;
		}
		remote_changed_paths.insert(lock_manager ? lock_manager->clean_path(path) : path);
		Node *parent = node->get_parent();
		if (undo_redo) {
			undo_redo->create_action(TTR("Apply Remote Multiuser Delete"), UndoRedo::MERGE_DISABLE, node);
			undo_redo->add_do_method(parent, "remove_child", node);
			undo_redo->add_undo_method(parent, "add_child", node, true);
			undo_redo->add_undo_method(node, "set_owner", scene_root);
			undo_redo->add_undo_reference(node);
			undo_redo->commit_action(true);
		} else {
			parent->remove_child(node);
			node->queue_free();
		}
	} else if (type == multiuser_editor::kActionScriptAttach) {
		String path = String(p_action.get("node_path", data.get("path", "")));
		Node *node = _resolve_node(path);
		String raw_script_path = String(data.get("script_path", data.get("script", "")));
		String script_path;
		String script_content = String(data.get("script_content", ""));
		if (!node || raw_script_path.is_empty() || !canonicalize_res_path(raw_script_path, script_path)) {
			const String msg = "Multiuser editor: Dropped unsafe script attach to path: " + raw_script_path;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
			return;
		}
		if (script_content.length() > multiuser_editor::kRemoteValueDefaultByteCap) {
			const String msg = vformat("Multiuser editor: Dropped oversized script_attach content (>%d bytes).", int(multiuser_editor::kRemoteValueDefaultByteCap));
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
			return;
		}
		Ref<FileAccess> file = FileAccess::open(script_path, FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(script_content);
		}
		EditorFileSystem *efs = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_resource_filesystem() : nullptr;
		if (efs) {
			efs->update_file(script_path);
		}
		Ref<Script> script = ResourceLoader::load(script_path, "Script", ResourceFormatLoader::CACHE_MODE_REPLACE);
		if (script.is_valid()) {
			node->set("script", script);
			node->notify_property_list_changed();
			String clean_path = lock_manager ? lock_manager->clean_path(path) : path;
			remote_changed_paths.insert(clean_path);
			script_cache[clean_path] = script_path;
		}
	} else if (type == multiuser_editor::kActionScriptDetach) {
		String path = String(p_action.get("node_path", data.get("path", "")));
		Node *node = _resolve_node(path);
		if (node) {
			node->set("script", Variant());
			node->notify_property_list_changed();
			String clean_path = lock_manager ? lock_manager->clean_path(path) : path;
			remote_changed_paths.insert(clean_path);
			script_cache[clean_path] = String();
		}
	} else {
		const String msg = vformat("Multiuser editor: apply_remote_action dropped unknown action type '%s' (default-deny).", type);
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
	}
}

void MultiuserEditorActionInterceptor::clear_caches() {
	transform_cache.clear();
	property_cache.clear();
	script_cache.clear();
	remote_changed_paths.clear();
	locked_add_blocked = false;
	locked_delete_blocked = false;
}

#endif
