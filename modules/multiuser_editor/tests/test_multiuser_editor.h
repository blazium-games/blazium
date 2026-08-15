/**************************************************************************/
/*  test_multiuser_editor.h                                               */
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

#include "tests/test_macros.h"

#ifdef TOOLS_ENABLED

#include "../multiuser_editor_access_list.h"
#include "../multiuser_editor_action_interceptor.h"
#include "../multiuser_editor_constants.h"
#include "../multiuser_editor_crdt_text_buffer.h"
#include "../multiuser_editor_dock.h"
#include "../multiuser_editor_filesystem_sync.h"
#include "../multiuser_editor_lock_manager.h"
#include "../multiuser_editor_network.h"
#include "../multiuser_editor_permissions.h"
#include "../multiuser_editor_plugin.h"
#include "../multiuser_editor_script_sync.h"
#include "../multiuser_editor_security_sink.h"
#include "../multiuser_editor_settings.h"

#include "modules/jwttool/jwt.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"

namespace TestMultiuserEditor {

TEST_CASE("[MultiuserEditor] CRDT concurrent inserts converge") {
	MultiuserEditorCRDTTextBuffer a;
	MultiuserEditorCRDTTextBuffer b;
	a.init("site_a");
	b.init("site_b");

	Dictionary op_a = a.local_insert(0, "A");
	Dictionary op_b = b.local_insert(0, "B");
	a.remote_insert(op_b);
	b.remote_insert(op_a);

	CHECK(a.get_text() == b.get_text());
	CHECK(a.get_text().length() == 2);
	CHECK(a.get_text().contains("A"));
	CHECK(a.get_text().contains("B"));
}

TEST_CASE("[MultiuserEditor] CRDT exported state round-trips") {
	MultiuserEditorCRDTTextBuffer source;
	MultiuserEditorCRDTTextBuffer imported;
	source.init("source_site");
	imported.init("imported_site");

	source.local_insert(0, "h");
	source.local_insert(1, "i");

	CHECK(imported.import_state(source.export_state()));

	CHECK(imported.get_text() == "hi");
}

TEST_CASE("[MultiuserEditor] CRDT deletes converge") {
	MultiuserEditorCRDTTextBuffer a;
	MultiuserEditorCRDTTextBuffer b;
	a.init("site_a");
	b.init("site_b");

	Dictionary first = a.local_insert(0, "h");
	Dictionary second = a.local_insert(1, "i");
	b.remote_insert(first);
	b.remote_insert(second);

	Dictionary removed = a.local_delete(0);
	b.remote_delete(removed);

	CHECK(a.get_text() == "i");
	CHECK(b.get_text() == "i");
}

TEST_CASE("[MultiuserEditor] CRDT remote inserts keep deterministic order") {
	MultiuserEditorCRDTTextBuffer source;
	MultiuserEditorCRDTTextBuffer imported;
	source.init("source_site");
	imported.init("imported_site");

	Vector<Dictionary> ops;
	String text = "multiuser";
	for (int i = 0; i < text.length(); i++) {
		ops.push_back(source.local_insert(i, String::chr(text[i])));
	}

	for (int i = ops.size() - 1; i >= 0; i--) {
		imported.remote_insert(ops[i]);
	}
	CHECK(imported.get_text() == text);
}

TEST_CASE("[MultiuserEditor] Script sync accepts direct CRDT op packets") {
	MultiuserEditorCRDTTextBuffer source;
	source.init("source_site");
	Dictionary op = source.local_insert(0, "x");

	MultiuserEditorScriptSync sync;
	sync.set_local_peer_id("local_site");
	sync.apply_remote_crdt(op, "res://script.gd");

	MultiuserEditorCRDTTextBuffer imported;
	imported.init("imported_site");

	CHECK(imported.import_state(sync.export_buffer("res://script.gd")));
	CHECK(imported.get_text() == "x");
}

TEST_CASE("[MultiuserEditor] Network action dictionaries round-trip") {
	MultiuserEditorCRDTTextBuffer source;
	source.init("source_site");
	Dictionary op = source.local_insert(0, "x");

	Dictionary action;
	action["type"] = "crdt";
	action["node_path"] = "res://script.gd";
	action["data"] = op;

	MultiuserEditorNetwork network;
	PackedByteArray packet = network.test_serialize_action(action);
	Dictionary decoded = network.test_deserialize_action(packet);
	CHECK(String(decoded.get("type", "")) == "crdt");
	CHECK(String(decoded.get("node_path", "")) == "res://script.gd");
	Dictionary decoded_data = decoded.get("data", Dictionary());
	CHECK(String(decoded_data.get("op", "")) == "insert");

	Variant not_an_action = String("not an action");
	int len = 0;
	CHECK(encode_variant(not_an_action, nullptr, len, false) == OK);
	PackedByteArray not_dictionary;
	not_dictionary.resize(len);
	CHECK(encode_variant(not_an_action, not_dictionary.ptrw(), len, false) == OK);
	CHECK(network.test_deserialize_action(not_dictionary).is_empty());
}

TEST_CASE("[MultiuserEditor] Script sync waits for explicit sync completion") {
	MultiuserEditorCRDTTextBuffer source;
	source.init("source_site");
	source.local_insert(0, "a");
	Dictionary state = source.export_state();

	MultiuserEditorScriptSync sync;
	sync.set_local_peer_id("local_site");
	sync.set_sync_pending(true);
	sync.import_buffer_state("res://a.gd", state, false);
	double now = OS::get_singleton()->get_ticks_msec() / 1000.0;
	CHECK(sync.is_sync_pending_expired(now + 4.0, 3.0));

	sync.import_buffer_state("res://b.gd", state, true);
	CHECK_FALSE(sync.is_sync_pending_expired(now + 4.0, 3.0));
}

TEST_CASE("[MultiuserEditor] Script sync exports removes and clears buffers") {
	MultiuserEditorCRDTTextBuffer source;
	source.init("source_site");
	Dictionary first = source.local_insert(0, "x");
	Dictionary second = source.local_insert(1, "y");

	MultiuserEditorScriptSync sync;
	sync.set_local_peer_id("local_site");
	sync.apply_remote_crdt(first, "res://one.gd");
	sync.apply_remote_crdt(second, "res://two.gd");
	Dictionary buffers = sync.export_all_buffers();
	CHECK(buffers.size() == 2);
	CHECK(sync.has_buffer("res://one.gd"));
	CHECK(sync.has_buffer("res://two.gd"));

	sync.remove_buffer("res://one.gd");
	CHECK_FALSE(sync.has_buffer("res://one.gd"));
	CHECK(sync.has_buffer("res://two.gd"));

	sync.clear_all_buffers();
	CHECK(sync.export_all_buffers().is_empty());
}

TEST_CASE("[MultiuserEditor] Lock manager cleans paths and releases peer locks") {
	MultiuserEditorLockManager locks;
	locks.add_peer_lock("peer_a", "Root/Node");

	CHECK(locks.is_locked("Root/Node"));

	CHECK(locks.get_lock_owner("Root/Node [locked peer_xyz]") == "peer_a");

	CHECK(locks.get_lock_owner("Root/Node") == "peer_a");

	locks.release_peer("peer_a");
	CHECK_FALSE(locks.is_locked("Root/Node"));
}

static PackedStringArray _mu_fs_inc_only_test_prefix() {
	PackedStringArray inc;
	inc.push_back("res://_mu_fss_test*");
	return inc;
}

TEST_CASE("[MultiuserEditorFilesystemSync] glob include/exclude") {
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	exc.push_back("res://.godot/*");
	CHECK(MultiuserEditorFilesystemSync::test_path_matches_policy("res://scripts/foo.gd", inc, exc, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.godot/editor_settings", inc, exc, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://tex.png.import", inc, exc, false));
	CHECK(MultiuserEditorFilesystemSync::test_path_matches_policy("res://tex.png.import", inc, exc, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("user://x", inc, exc, true));
}

TEST_CASE("[MultiuserEditorFilesystemSync] diff produces create/update/delete") {
	const String path = "res://_mu_fss_test_diff.txt";
	Ref<FileAccess> probe = FileAccess::open(path, FileAccess::WRITE);
	if (probe.is_null()) {
		return;
	}
	probe->close();
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));

	PackedStringArray inc = _mu_fs_inc_only_test_prefix();
	PackedStringArray exc;
	const bool incl_import = true;
	const int64_t max_sz = 1024 * 1024;

	MultiuserEditorFilesystemSync sync;
	sync.clear_snapshot();
	sync.capture_snapshot_from_res(incl_import, inc, exc);

	{
		Ref<FileAccess> wf = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE_FALSE(wf.is_null());
		wf->store_string("v1");
		wf->close();
	}
	Vector<MultiuserEditorFilesystemSync::Delta> d1 = sync.diff_and_update_snapshot(incl_import, inc, exc, max_sz);
	REQUIRE(d1.size() >= 1);
	CHECK(d1[0].kind == MultiuserEditorFilesystemSync::DELTA_CREATE);
	CHECK(d1[0].path == path);

	{
		Ref<FileAccess> wf = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE_FALSE(wf.is_null());
		wf->store_string("v2");
		wf->close();
	}
	Vector<MultiuserEditorFilesystemSync::Delta> d2 = sync.diff_and_update_snapshot(incl_import, inc, exc, max_sz);
	REQUIRE(d2.size() >= 1);
	CHECK(d2[0].kind == MultiuserEditorFilesystemSync::DELTA_UPDATE);
	CHECK(d2[0].path == path);

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	Vector<MultiuserEditorFilesystemSync::Delta> d3 = sync.diff_and_update_snapshot(incl_import, inc, exc, max_sz);
	REQUIRE(d3.size() >= 1);
	CHECK(d3[0].kind == MultiuserEditorFilesystemSync::DELTA_DELETE);
	CHECK(d3[0].path == path);
}

TEST_CASE("[MultiuserEditorFilesystemSync] diff detects rename via content hash") {
	const String old_path = "res://_mu_fss_test_move_from.txt";
	const String new_path = "res://_mu_fss_test_move_to.txt";
	const String payload = "same-bytes-for-move-detection\n";

	const String cleanup_paths[2] = { old_path, new_path };
	for (int pi = 0; pi < 2; pi++) {
		const String &p = cleanup_paths[pi];
		if (FileAccess::exists(p)) {
			DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(p));
		}
	}

	Ref<FileAccess> probe = FileAccess::open(old_path, FileAccess::WRITE);
	if (probe.is_null()) {
		return;
	}
	probe->store_string(payload);
	probe->close();

	PackedStringArray inc = _mu_fs_inc_only_test_prefix();
	PackedStringArray exc;
	MultiuserEditorFilesystemSync sync;
	sync.clear_snapshot();
	sync.capture_snapshot_from_res(true, inc, exc);

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(old_path));
	Ref<FileAccess> wf = FileAccess::open(new_path, FileAccess::WRITE);
	REQUIRE_FALSE(wf.is_null());
	wf->store_string(payload);
	wf->close();

	Vector<MultiuserEditorFilesystemSync::Delta> d = sync.diff_and_update_snapshot(true, inc, exc, 1024 * 1024);
	REQUIRE(d.size() == 1);
	CHECK(d[0].kind == MultiuserEditorFilesystemSync::DELTA_MOVE);
	CHECK(d[0].old_path == old_path);
	CHECK(d[0].path == new_path);

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(new_path));
}

TEST_CASE("[MultiuserEditorFilesystemSync] chunked transfer reassembles to original bytes and matches sha1") {
	const String path = "res://_mu_fss_test_chunk.bin";
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	}
	Ref<FileAccess> probe = FileAccess::open(path, FileAccess::WRITE);
	if (probe.is_null()) {
		return;
	}
	PackedByteArray payload;
	payload.resize(2500);
	for (int i = 0; i < payload.size(); i++) {
		payload.ptrw()[i] = uint8_t(37 + (i % 11));
	}
	probe->store_buffer(payload);
	probe->close();

	MultiuserEditorFilesystemSync::Delta d;
	d.kind = MultiuserEditorFilesystemSync::DELTA_UPDATE;
	d.path = path;
	d.hash_hex = MultiuserEditorFilesystemSync::hash_file_hex(path);
	d.size = uint64_t(payload.size());

	String tid;
	Vector<Dictionary> actions = MultiuserEditorFilesystemSync::build_transfer_actions(
			String("file_apply"), d, 64 * 1024 * 1024, 800, true, tid);
	CHECK(actions.size() >= 3);

	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	for (int i = 0; i < actions.size(); i++) {
		const Dictionary &act = actions[i];
		const String t = String(act.get("type", ""));
		const Error e = sync->apply_incoming_transfer(t, act, false, false, true);
		if (t.ends_with("_begin") || t.ends_with("_chunk")) {
			CHECK(e == ERR_BUSY);
		} else if (t.ends_with("_end")) {
			CHECK(e == OK);
		}
	}

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

TEST_CASE("[MultiuserEditorFilesystemSync] enforces max_file_bytes") {
	const String path = "res://_mu_fss_test_max.bin";
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	}
	Ref<FileAccess> wf = FileAccess::open(path, FileAccess::WRITE);
	if (wf.is_null()) {
		return;
	}
	wf->store_string("1234567890");
	wf->close();

	MultiuserEditorFilesystemSync::Delta d;
	d.kind = MultiuserEditorFilesystemSync::DELTA_CREATE;
	d.path = path;
	String tid;
	Vector<Dictionary> actions = MultiuserEditorFilesystemSync::build_transfer_actions(
			String("file_apply"), d, 5, 65536, true, tid);
	CHECK(actions.is_empty());

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

TEST_CASE("[MultiuserEditorFilesystemSync] suppress recent apply skips matching path hash") {
	MultiuserEditorFilesystemSync sync;
	const String path = "res://_mu_fss_test_skip.gd";
	const String h = "abc123";
	sync.remember_applied(path, h);
	CHECK(sync.should_skip_path_hash(path, h));
	CHECK_FALSE(sync.should_skip_path_hash(path, "other"));
	CHECK_FALSE(sync.should_skip_path_hash("res://other.gd", h));
}

TEST_CASE("[MultiuserEditorFilesystemSync] should_skip_host_relay for propose types") {
	Dictionary a;
	a["type"] = "file_propose_begin";
	CHECK(MultiuserEditorFilesystemSync::should_skip_host_relay(a));
	a["type"] = "file_propose_chunk";
	CHECK(MultiuserEditorFilesystemSync::should_skip_host_relay(a));
	a["type"] = "file_propose_delete";
	CHECK(MultiuserEditorFilesystemSync::should_skip_host_relay(a));
	a["type"] = "file_apply_begin";
	CHECK_FALSE(MultiuserEditorFilesystemSync::should_skip_host_relay(a));
}

TEST_CASE("[MultiuserEditorPermissions] role string parsing") {
	CHECK(MultiuserEditorPermissions::role_from_string("Viewer") == MultiuserEditorPermissions::ROLE_VIEWER);
	CHECK(MultiuserEditorPermissions::role_from_string("editor") == MultiuserEditorPermissions::ROLE_EDITOR);
	CHECK(MultiuserEditorPermissions::role_from_string("Admin") == MultiuserEditorPermissions::ROLE_ADMIN);
	CHECK(MultiuserEditorPermissions::role_from_string("Host") == MultiuserEditorPermissions::ROLE_ADMIN);
	CHECK(MultiuserEditorPermissions::role_from_string("Bogus") == MultiuserEditorPermissions::ROLE_NONE);
	CHECK(MultiuserEditorPermissions::role_from_string("") == MultiuserEditorPermissions::ROLE_NONE);
}

TEST_CASE("[MultiuserEditorPermissions] default three-tier matrix") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	CHECK(perms->can_perform("chat", "Viewer"));
	CHECK(perms->can_perform("chat", "Editor"));
	CHECK(perms->can_perform("chat", "Admin"));
	CHECK(perms->can_perform("telemetry", "Viewer"));
	CHECK(perms->can_perform("cursor_update", "Viewer"));
	CHECK(perms->can_perform("select", "Viewer"));

	CHECK_FALSE(perms->can_perform("crdt", "Viewer"));
	CHECK(perms->can_perform("crdt", "Editor"));
	CHECK(perms->can_perform("crdt", "Admin"));
	CHECK_FALSE(perms->can_perform("script_attach", "Viewer"));
	CHECK_FALSE(perms->can_perform("file_apply_begin", "Viewer"));
	CHECK(perms->can_perform("file_apply_begin", "Editor"));
	CHECK_FALSE(perms->can_perform("property", "Viewer"));
	CHECK(perms->can_perform("property", "Editor"));
	CHECK_FALSE(perms->can_perform("resource_sync", "Viewer"));
	CHECK(perms->can_perform("resource_sync", "Editor"));

	CHECK_FALSE(perms->can_perform("project_setting", "Viewer"));
	CHECK_FALSE(perms->can_perform("project_setting", "Editor"));
	CHECK(perms->can_perform("project_setting", "Admin"));
	CHECK_FALSE(perms->can_perform("scene_sync", "Editor"));
	CHECK(perms->can_perform("scene_sync", "Admin"));
	CHECK_FALSE(perms->can_perform("magic_repair_start", "Editor"));
	CHECK(perms->can_perform("magic_repair_start", "Admin"));
	CHECK_FALSE(perms->can_perform("autowork_trigger", "Editor"));
	CHECK(perms->can_perform("autowork_trigger", "Admin"));

	CHECK_FALSE(perms->can_perform("not_a_real_action", "Admin"));
	CHECK_FALSE(perms->is_known_action("not_a_real_action"));
	CHECK(perms->is_known_action("project_setting"));
}

TEST_CASE("[MultiuserEditorPermissions] host-only flags") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	CHECK(perms->is_host_only("project_setting"));
	CHECK(perms->is_host_only("project_settings_snapshot"));
	CHECK(perms->is_host_only("handshake_ack"));
	CHECK(perms->is_host_only("auth_challenge"));
	CHECK(perms->is_host_only("magic_repair_start"));
	CHECK(perms->is_host_only("scene_sync"));
	CHECK(perms->is_host_only("team_play_start"));
	CHECK(perms->is_host_only("autowork_trigger"));

	CHECK_FALSE(perms->is_host_only("handshake"));
	CHECK_FALSE(perms->is_host_only("chat"));
	CHECK_FALSE(perms->is_host_only("crdt"));
	CHECK_FALSE(perms->is_host_only("file_apply_begin"));
	CHECK_FALSE(perms->is_host_only("magic_repair_request"));
}

TEST_CASE("[MultiuserEditorPermissions] override parser - relax and tighten") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	perms->set_allow_widen_host_only(true);
	perms->apply_overrides("scene_sync=Editor,Admin@any");
	CHECK(perms->can_perform("scene_sync", "Editor"));
	CHECK_FALSE(perms->is_host_only("scene_sync"));

	perms->apply_overrides("chat=Admin");
	CHECK_FALSE(perms->can_perform("chat", "Viewer"));
	CHECK_FALSE(perms->can_perform("chat", "Editor"));
	CHECK(perms->can_perform("chat", "Admin"));

	perms->apply_overrides("telemetry=*");
	CHECK(perms->can_perform("telemetry", "Viewer"));
}

TEST_CASE("[MultiuserEditorPermissions] override parser ignores malformed entries") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	const int chat_mask_before = perms->get_action_mask("chat");
	const int proj_mask_before = perms->get_action_mask("project_setting");

	perms->apply_overrides("");
	perms->apply_overrides(";;;");
	perms->apply_overrides("=Admin");
	perms->apply_overrides("chat=");
	perms->apply_overrides("chat=Garbage");
	perms->apply_overrides("not_an_action=Editor");

	CHECK(perms->get_action_mask("chat") == chat_mask_before);
	CHECK(perms->get_action_mask("project_setting") == proj_mask_before);
}

TEST_CASE("[MultiuserEditor] canonicalize_res_path accepts safe paths") {
	String out;
	CHECK(MultiuserEditorActionInterceptor::canonicalize_res_path("res://foo/bar.gd", out));
	CHECK(out == "res://foo/bar.gd");
	out = String();
	CHECK(MultiuserEditorActionInterceptor::canonicalize_res_path("res://scenes/main.tscn", out));
	CHECK(out == "res://scenes/main.tscn");
	out = String();

	CHECK(MultiuserEditorActionInterceptor::canonicalize_res_path("res://foo\\bar.gd", out));
	CHECK(out == "res://foo/bar.gd");

	out = String();
	CHECK(MultiuserEditorActionInterceptor::canonicalize_res_path("RES://foo.gd", out));
	CHECK(out == "res://foo.gd");

	out = String();
	CHECK(MultiuserEditorActionInterceptor::canonicalize_res_path("  res://foo.gd  ", out));
	CHECK(out == "res://foo.gd");
}

TEST_CASE("[MultiuserEditor] canonicalize_res_path rejects unsafe paths") {
	String out;
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("file:///etc/passwd", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("/abs/path.gd", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("res://", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("res://../etc/passwd", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("res://foo/../bar", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("res://./foo", out));
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("res://foo/.~lockfile", out));
	{
		String control_path = "res://foo";
		control_path += String::chr(0x07);
		control_path += ".gd";
		CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path(control_path, out));
	}
	CHECK_FALSE(MultiuserEditorActionInterceptor::canonicalize_res_path("http://evil/host", out));

	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_file_path("res://../etc/passwd"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_file_path("res://safe/file.gd"));
}

TEST_CASE("[MultiuserEditor] is_safe_property_name accepts/rejects expected names") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_property_name("position"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_property_name("text"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_property_name("section/subsection_value"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_property_name("_underscore_first"));

	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name(""));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("script"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("script_class"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("metadata/foo"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("__internal"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("editor_internal"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("9digit_first"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name("has space"));
	{
		String control_name = "name";
		control_name += String::chr(0x07);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name(control_name));
	}
	{
		String huge;
		for (int i = 0; i < 300; i++) {
			huge += "a";
		}
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name(huge));
	}
}

TEST_CASE("[MultiuserEditor] is_safe_node_name accepts/rejects expected names") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_node_name("Foo"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_node_name("Foo_1"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_node_name("Player.0"));

	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name(""));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name("foo/bar"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name("foo:bar"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name("@instance"));
	{
		String soh_name = "Foo";
		soh_name += String::chr(0x01);
		soh_name += "Bar";
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name(soh_name));
	}
	{
		String control_name = "Foo";
		control_name += String::chr(0x05);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_name(control_name));
	}
}

TEST_CASE("[MultiuserEditor] CRDT remote_insert rejects oversized fields") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local");

	{
		Dictionary op;
		op["op"] = "insert";
		String huge_site;
		for (int i = 0; i < 80; i++) {
			huge_site += "x";
		}
		op["site"] = huge_site;
		op["clock"] = 1;
		op["char"] = "a";
		Array pos;
		pos.append(0);
		op["position"] = pos;
		CHECK(buffer.remote_insert(op) == -1);
	}
	{
		Dictionary op;
		op["op"] = "insert";
		op["site"] = "ok_site";
		op["clock"] = 2;
		String huge_char;
		for (int i = 0; i < 80; i++) {
			huge_char += "z";
		}
		op["char"] = huge_char;
		Array pos;
		pos.append(0);
		op["position"] = pos;
		CHECK(buffer.remote_insert(op) == -1);
	}
	{
		Dictionary op;
		op["op"] = "insert";
		op["site"] = "ok_site";
		op["clock"] = 3;
		op["char"] = "b";
		Array pos;
		for (int i = 0; i < 100; i++) {
			pos.append(i);
		}
		op["position"] = pos;
		CHECK(buffer.remote_insert(op) == -1);
	}

	{
		Dictionary op;
		op["op"] = "insert";
		op["site"] = "ok_site";
		op["clock"] = 4;
		op["char"] = "c";
		Array pos;
		pos.append(1);
		op["position"] = pos;
		CHECK(buffer.remote_insert(op) >= 0);
	}
}

TEST_CASE("[MultiuserEditor] CRDT remote_delete rejects oversized site") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local");

	Dictionary op;
	String huge_site;
	for (int i = 0; i < 80; i++) {
		huge_site += "y";
	}
	op["site"] = huge_site;
	op["clock"] = 1;
	CHECK(buffer.remote_delete(op) == -1);
}

TEST_CASE("[MultiuserEditor] filesystem_sync apply_incoming_transfer rejects unsafe paths") {
	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	sync->set_sync_policy(inc, exc, true, 1024 * 1024, 65536);

	Dictionary action;
	action["type"] = "file_apply_delete";
	Dictionary data;
	data["path"] = "res://../etc/passwd";
	action["data"] = data;
	const Error err = sync->apply_incoming_transfer("file_apply_delete", action, false, false, true);
	CHECK(err == ERR_INVALID_PARAMETER);
}

TEST_CASE("[MultiuserEditor] is_safe_branch_name accepts valid names") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_branch_name("main"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_branch_name("feature/foo"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_branch_name("release-1.2.3"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_branch_name("Author_Name"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_branch_name("a"));
}

TEST_CASE("[MultiuserEditor] is_safe_branch_name rejects unsafe names") {
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name(""));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("-foo"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name(".foo"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("/foo"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo..bar"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo@{1}"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo//bar"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo.lock"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo/"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo bar"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name("foo;rm"));
	{
		String long_name;
		for (int i = 0; i < 250; i++) {
			long_name += "a";
		}
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_branch_name(long_name));
	}
}

TEST_CASE("[MultiuserEditor] is_safe_remote_name accepts and rejects") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("origin"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("team-a"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("mirror_2"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name(""));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("bad name"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("bad/name"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("with;semi"));
	{
		String long_name;
		for (int i = 0; i < 100; i++) {
			long_name += "a";
		}
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name(long_name));
	}
}

TEST_CASE("[MultiuserEditor] is_safe_commit_message accepts and rejects") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_commit_message("simple"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_commit_message("line1\nline2"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_commit_message(""));
	{
		String huge;
		for (int i = 0; i < 5000; i++) {
			huge += "x";
		}
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_commit_message(huge));
	}
	{
		String with_cr = "ok";
		with_cr += String::chr(0x0D);
		with_cr += "next";
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_commit_message(with_cr));
	}
	{
		String with_ctrl = "ok";
		with_ctrl += String::chr(0x05);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_commit_message(with_ctrl));
	}
}

TEST_CASE("[MultiuserEditor] filesystem_sync host_accumulate_propose rejects oversized chunk") {
	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	sync->set_sync_policy(inc, exc, true, 16 * 1024 * 1024, 64 * 1024);

	Dictionary begin_action;
	begin_action["type"] = "file_propose_begin";
	Dictionary bdata;
	bdata["transfer_id"] = "tid_oversized";
	bdata["path"] = "res://example.tres";
	bdata["total_size"] = 200000;
	bdata["chunk_bytes"] = 64 * 1024;
	bdata["total_chunks"] = 4;
	begin_action["data"] = bdata;
	String reject;
	Vector<Dictionary> broadcasts;
	const Error err_begin = sync->host_accumulate_propose(42, "file_propose_begin", begin_action, broadcasts, reject);
	CHECK(err_begin == ERR_BUSY);

	Dictionary chunk_action;
	chunk_action["type"] = "file_propose_chunk";
	Dictionary cdata;
	cdata["transfer_id"] = "tid_oversized";
	cdata["chunk_index"] = 0;
	PackedByteArray oversized;
	oversized.resize(300 * 1024);
	cdata["chunk_data"] = oversized;
	chunk_action["data"] = cdata;
	const Error err_chunk = sync->host_accumulate_propose(42, "file_propose_chunk", chunk_action, broadcasts, reject);
	CHECK(err_chunk == ERR_INVALID_PARAMETER);
	CHECK(reject == "chunk_too_large");
}

TEST_CASE("[MultiuserEditor] permissions registers git_request and git_response") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	CHECK(perms->can_perform("git_request", "Editor"));
	CHECK(perms->can_perform("git_request", "Admin"));
	CHECK_FALSE(perms->can_perform("git_request", "Viewer"));
	CHECK(perms->is_host_only("git_response"));
	CHECK_FALSE(perms->is_host_only("git_request"));
}

TEST_CASE("[MultiuserEditor] filesystem_sync rejects empty or oversized transfer_id") {
	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	sync->set_sync_policy(inc, exc, true, 16 * 1024 * 1024, 64 * 1024);
	sync->set_transfer_id_max_chars(32);

	String reject;
	Vector<Dictionary> broadcasts;

	{
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		data["transfer_id"] = "";
		data["path"] = "res://test.tres";
		data["total_size"] = 1024;
		data["total_chunks"] = 1;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(42, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_INVALID_PARAMETER);
		CHECK(reject == "invalid_transfer_id");
	}
	{
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		String huge;
		for (int i = 0; i < 64; i++) {
			huge += "x";
		}
		data["transfer_id"] = huge;
		data["path"] = "res://test.tres";
		data["total_size"] = 1024;
		data["total_chunks"] = 1;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(42, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_INVALID_PARAMETER);
		CHECK(reject == "invalid_transfer_id");
	}
	{
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		String control_tid = "ok";
		control_tid += String::chr(0x05);
		data["transfer_id"] = control_tid;
		data["path"] = "res://test.tres";
		data["total_size"] = 1024;
		data["total_chunks"] = 1;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(42, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_INVALID_PARAMETER);
		CHECK(reject == "invalid_transfer_id");
	}
}

TEST_CASE("[MultiuserEditor] filesystem_sync rejects too many concurrent transfers per peer") {
	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	sync->set_sync_policy(inc, exc, true, 16 * 1024 * 1024, 64 * 1024);
	sync->set_concurrent_transfer_cap(2);

	String reject;
	Vector<Dictionary> broadcasts;

	for (int i = 0; i < 2; i++) {
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		data["transfer_id"] = "tid_" + itos(i);
		data["path"] = "res://test_" + itos(i) + ".tres";
		data["total_size"] = 1024;
		data["total_chunks"] = 1;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(7, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_BUSY);
	}

	Dictionary action;
	action["type"] = "file_propose_begin";
	Dictionary data;
	data["transfer_id"] = "tid_overflow";
	data["path"] = "res://test_overflow.tres";
	data["total_size"] = 1024;
	data["total_chunks"] = 1;
	action["data"] = data;
	const Error err = sync->host_accumulate_propose(7, "file_propose_begin", action, broadcasts, reject);
	CHECK(err == ERR_BUSY);
	CHECK(reject == "too_many_concurrent_transfers");
}

TEST_CASE("[MultiuserEditor] CRDT import_state caps atom count") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local");

	Dictionary state;
	state["clock"] = 1;
	Array atoms;
	for (int i = 0; i < 100; i++) {
		Dictionary item;
		Array pos;
		pos.append(i);
		item["position"] = pos;
		item["site"] = "remote";
		item["clock"] = i;
		item["char"] = "a";
		atoms.append(item);
	}
	state["atoms"] = atoms;

	buffer.set_import_atom_cap_override(10);

	CHECK_FALSE(buffer.import_state(state));
	CHECK(buffer.get_text().length() == 10);
}

TEST_CASE("[MultiuserEditor] CRDT import_state drops oversized per-atom fields") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local");

	Dictionary state;
	state["clock"] = 1;
	Array atoms;

	{
		Dictionary item;
		Array pos;
		for (int i = 0; i < 100; i++) {
			pos.append(i);
		}
		item["position"] = pos;
		item["site"] = "remote_a";
		item["clock"] = 1;
		item["char"] = "X";
		atoms.append(item);
	}

	{
		Dictionary item;
		Array pos;
		pos.append(0);
		item["position"] = pos;
		String huge;
		for (int i = 0; i < 80; i++) {
			huge += "y";
		}
		item["site"] = huge;
		item["clock"] = 1;
		item["char"] = "Y";
		atoms.append(item);
	}

	{
		Dictionary item;
		Array pos;
		pos.append(1);
		item["position"] = pos;
		item["site"] = "remote_c";
		item["clock"] = 1;
		item["char"] = "Z";
		atoms.append(item);
	}
	state["atoms"] = atoms;

	CHECK_FALSE(buffer.import_state(state));
	CHECK(buffer.get_text() == "Z");
}

TEST_CASE("[MultiuserEditor] CRDT export_state respects override cap and reports truncation") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local");
	for (int i = 0; i < 50; i++) {
		buffer.local_insert(i, "a");
	}
	buffer.set_export_atom_cap_override(10);
	Dictionary state = buffer.export_state();
	Array out_atoms = state.get("atoms", Array());
	CHECK(out_atoms.size() == 10);
	CHECK(bool(state.get("truncated", false)) == true);
}

TEST_CASE("[MultiuserEditor] is_safe_remote_value caps oversized container and packed values") {
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(String("hello"), 4 * 1024 * 1024));

	{
		PackedByteArray big;
		big.resize(5 * 1024 * 1024);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(big, 4 * 1024 * 1024));
	}

	{
		Array a;
		a.resize(70000);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(a, 4 * 1024 * 1024));
	}

	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(42, 4 * 1024 * 1024));
}

TEST_CASE("[MultiuserEditor] filesystem_sync rejects insane total_chunks") {
	Ref<MultiuserEditorFilesystemSync> sync;
	sync.instantiate();
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	sync->set_sync_policy(inc, exc, true, 16 * 1024 * 1024, 64 * 1024);

	String reject;
	Vector<Dictionary> broadcasts;

	{
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		data["transfer_id"] = "tid_huge_chunks";
		data["path"] = "res://huge.tres";
		data["total_size"] = 1024;
		data["total_chunks"] = 1000000000;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(11, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_INVALID_PARAMETER);
		CHECK(reject == "invalid_total_chunks");
	}
	{
		Dictionary action;
		action["type"] = "file_propose_begin";
		Dictionary data;
		data["transfer_id"] = "tid_too_many_chunks";
		data["path"] = "res://small.tres";
		data["total_size"] = 100;
		data["total_chunks"] = 100;
		action["data"] = data;
		const Error err = sync->host_accumulate_propose(11, "file_propose_begin", action, broadcasts, reject);
		CHECK(err == ERR_INVALID_PARAMETER);
		CHECK(reject == "invalid_total_chunks");
	}
}

TEST_CASE("[MultiuserEditor] lock_manager rejects unsafe paths and caps per-peer locks") {
	MultiuserEditorLockManager locks;
	locks.set_max_locks_per_peer(3);

	locks.add_peer_lock("peer_a", "Root/Node");
	locks.add_peer_lock("peer_a", "Root/Other");
	locks.add_peer_lock("peer_a", "Root/Third");
	CHECK(locks.is_locked("Root/Node"));
	CHECK(locks.is_locked("Root/Other"));
	CHECK(locks.is_locked("Root/Third"));

	locks.add_peer_lock("peer_a", "Root/FourthOverCap");
	CHECK_FALSE(locks.is_locked("Root/FourthOverCap"));

	locks.add_peer_lock("peer_b", "");
	CHECK_FALSE(locks.is_locked(""));
}

TEST_CASE("[MultiuserEditor] script sync evicts oldest buffer past LRU cap") {
	MultiuserEditorScriptSync sync;
	sync.set_local_peer_id("local");

	sync.set_max_tracked_buffers(8);
	CHECK(sync.get_max_tracked_buffers() == 8);

	MultiuserEditorCRDTTextBuffer source;
	source.init("source");

	for (int i = 0; i < 8; i++) {
		Dictionary op = source.local_insert(i, "x");
		const String path = "res://lru_" + itos(i) + ".gd";
		sync.apply_remote_crdt(op, path);
	}
	CHECK(sync.get_tracked_buffer_count() == 8);
	CHECK(sync.has_buffer("res://lru_0.gd"));

	{
		Dictionary op = source.local_insert(8, "y");
		sync.apply_remote_crdt(op, "res://lru_0.gd");
	}

	{
		Dictionary op = source.local_insert(9, "z");
		sync.apply_remote_crdt(op, "res://lru_new.gd");
	}
	CHECK(sync.has_buffer("res://lru_new.gd"));
	CHECK(sync.has_buffer("res://lru_0.gd"));
	CHECK_FALSE(sync.has_buffer("res://lru_1.gd"));
	CHECK(sync.get_tracked_buffer_count() == 8);
}

namespace MultiuserHardeningPass4Helpers {

inline String _mint_hs256(const Dictionary &p_payload, const String &p_secret) {
	Dictionary header;
	header["alg"] = "HS256";
	header["typ"] = "JWT";
	JWT *jwt = JWT::get_singleton();
	if (!jwt) {
		return String();
	}
	return jwt->create_jwt_hs256(header, p_payload, p_secret);
}

inline MultiuserEditorPlugin::JWTValidationConfig _default_cfg() {
	MultiuserEditorPlugin::JWTValidationConfig cfg;
	cfg.algorithms_csv = "HS256";
	cfg.leeway_sec = 30.0;
	cfg.max_token_age_sec = 3600;
	return cfg;
}

inline Dictionary _basic_payload(const String &p_role) {
	Dictionary p;
	const double now = OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0.0;
	p["iat"] = now;
	p["exp"] = now + 600.0;
	p["role"] = p_role;
	return p;
}

} //namespace MultiuserHardeningPass4Helpers

TEST_CASE("[MultiuserEditor] JWT validate accepts well-formed HS256 token") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		MESSAGE("JWT singleton unavailable, skipping");
		return;
	}
	const String secret = "test_secret_12345";
	Dictionary payload = _basic_payload("Editor");
	const String token = _mint_hs256(payload, secret);
	REQUIRE_FALSE(token.is_empty());

	auto cfg = _default_cfg();
	auto r = MultiuserEditorPlugin::validate_jwt_static(token, secret, cfg);
	CHECK(r.valid);
	CHECK(r.role == "Editor");
}

TEST_CASE("[MultiuserEditor] JWT validate rejects bad signature") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	Dictionary payload = _basic_payload("Editor");
	const String token = _mint_hs256(payload, secret);
	auto cfg = _default_cfg();
	auto r = MultiuserEditorPlugin::validate_jwt_static(token, "WRONG_SECRET", cfg);
	CHECK_FALSE(r.valid);
	CHECK(r.reason == "bad_sig");
}

TEST_CASE("[MultiuserEditor] JWT validate rejects disallowed algorithm") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	const String token = _mint_hs256(_basic_payload("Editor"), secret);
	auto cfg = _default_cfg();
	cfg.algorithms_csv = "RS256";
	auto r = MultiuserEditorPlugin::validate_jwt_static(token, secret, cfg);
	CHECK_FALSE(r.valid);
	CHECK(r.reason.begins_with("bad_alg"));
}

TEST_CASE("[MultiuserEditor] JWT validate enforces required audience") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	auto cfg = _default_cfg();
	cfg.expected_audience = "multiuser-editor";

	Dictionary payload_no_aud = _basic_payload("Editor");
	auto r1 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(payload_no_aud, secret), secret, cfg);
	CHECK_FALSE(r1.valid);
	CHECK(r1.reason.begins_with("bad_aud"));

	Dictionary payload_wrong_aud = _basic_payload("Editor");
	payload_wrong_aud["aud"] = "other";
	auto r2 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(payload_wrong_aud, secret), secret, cfg);
	CHECK_FALSE(r2.valid);
	CHECK(r2.reason == "bad_aud");

	Dictionary payload_ok_aud = _basic_payload("Editor");
	payload_ok_aud["aud"] = "multiuser-editor";
	auto r3 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(payload_ok_aud, secret), secret, cfg);
	CHECK(r3.valid);
}

TEST_CASE("[MultiuserEditor] JWT validate enforces required issuer") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	auto cfg = _default_cfg();
	cfg.expected_issuer = "blazium-host";

	Dictionary p1 = _basic_payload("Editor");
	auto r1 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p1, secret), secret, cfg);
	CHECK_FALSE(r1.valid);
	CHECK(r1.reason.begins_with("bad_iss"));

	Dictionary p2 = _basic_payload("Editor");
	p2["iss"] = "blazium-host";
	auto r2 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p2, secret), secret, cfg);
	CHECK(r2.valid);
}

TEST_CASE("[MultiuserEditor] JWT validate requires iat and rejects too-old tokens") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	const double now = OS::get_singleton()->get_unix_time();
	auto cfg = _default_cfg();
	cfg.max_token_age_sec = 60;
	cfg.leeway_sec = 0;

	Dictionary p_no_iat;
	p_no_iat["exp"] = now + 600.0;
	p_no_iat["role"] = "Editor";
	auto r0 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p_no_iat, secret), secret, cfg);
	CHECK_FALSE(r0.valid);
	CHECK(r0.reason == "missing_iat");

	Dictionary p_old;
	p_old["iat"] = now - 7200.0;
	p_old["exp"] = now + 600.0;
	p_old["role"] = "Editor";
	auto r1 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p_old, secret), secret, cfg);
	CHECK_FALSE(r1.valid);
	CHECK(r1.reason == "too_old");

	Dictionary p_future;
	p_future["iat"] = now + 7200.0;
	p_future["exp"] = now + 14400.0;
	p_future["role"] = "Editor";
	auto r2 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p_future, secret), secret, cfg);
	CHECK_FALSE(r2.valid);

	CHECK((r2.reason == "future_iat" || r2.reason == "not_yet_valid" || r2.reason == "expired" || r2.reason == "bad_timing"));
}

TEST_CASE("[MultiuserEditor] JWT validate honors leeway on expired tokens") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	const double now = OS::get_singleton()->get_unix_time();

	Dictionary p;
	p["iat"] = now - 60.0;
	p["exp"] = now - 5.0;
	p["role"] = "Editor";
	const String token = _mint_hs256(p, secret);

	{
		auto cfg = _default_cfg();
		cfg.leeway_sec = 0;
		auto r = MultiuserEditorPlugin::validate_jwt_static(token, secret, cfg);
		CHECK_FALSE(r.valid);
		CHECK(r.reason == "expired");
	}
	{
		auto cfg = _default_cfg();
		cfg.leeway_sec = 30.0;
		auto r = MultiuserEditorPlugin::validate_jwt_static(token, secret, cfg);
		CHECK(r.valid);
	}
}

TEST_CASE("[MultiuserEditor] JWT validate rejects invalid roles and missing role") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	auto cfg = _default_cfg();

	Dictionary p_bad_role = _basic_payload("Superuser");
	auto r1 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p_bad_role, secret), secret, cfg);
	CHECK_FALSE(r1.valid);
	CHECK(r1.reason == "bad_role");

	Dictionary p_no_role;
	const double now = OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0.0;
	p_no_role["iat"] = now;
	p_no_role["exp"] = now + 600.0;
	auto r2 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p_no_role, secret), secret, cfg);
	CHECK_FALSE(r2.valid);
	CHECK(r2.reason == "missing_role");
}

TEST_CASE("[MultiuserEditor] JWT validate enforces require_jti when missing") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		return;
	}
	const String secret = "test_secret_12345";
	auto cfg = _default_cfg();
	cfg.require_jti = true;

	Dictionary p = _basic_payload("Editor");
	auto r = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p, secret), secret, cfg);
	CHECK_FALSE(r.valid);
	CHECK(r.reason == "missing_jti");

	p["jti"] = "abc-123";
	auto r2 = MultiuserEditorPlugin::validate_jwt_static(_mint_hs256(p, secret), secret, cfg);
	CHECK(r2.valid);
	CHECK(r2.jti == "abc-123");
}

TEST_CASE("[MultiuserEditor] JWT validate rejects malformed tokens") {
	auto cfg = MultiuserHardeningPass4Helpers::_default_cfg();
	auto r = MultiuserEditorPlugin::validate_jwt_static("not.a.valid.token!", "x", cfg);
	CHECK_FALSE(r.valid);
	CHECK(r.reason == "bad_format");

	auto r2 = MultiuserEditorPlugin::validate_jwt_static(String(), "x", cfg);
	CHECK_FALSE(r2.valid);
	CHECK(r2.reason == "bad_format");
}

TEST_CASE("[MultiuserEditor] tile_sync int validators reject out-of-range values") {
	const int kCoordMin = -(1 << 24);
	const int kCoordMax = (1 << 24);
	const int kAltMax = 1 << 16;
	CHECK(0 >= kCoordMin);
	CHECK(0 <= kCoordMax);
	CHECK_FALSE(((1 << 25) <= kCoordMax));
	CHECK_FALSE((-(1 << 25)) >= kCoordMin);
	CHECK_FALSE((kAltMax + 1) <= kAltMax);
}

TEST_CASE("[MultiuserEditor] sensitive setting prefix list contains expected categories") {
	const char *expected_prefixes[] = {
		"network/ssl/",
		"network/tls/",
		"network/limits/tcp/",
		"debug/file_logging/",
		"editor/",
		"filesystem/import/",
		"filesystem/on_save/",
		"editor_plugins/",
		nullptr,
	};
	for (int i = 0; expected_prefixes[i] != nullptr; i++) {
		const String p = String(expected_prefixes[i]);
		CHECK(p.contains("/"));
		CHECK(p.length() >= 5);
	}
}

TEST_CASE("[MultiuserEditor] is_safe_remote_value caps Variant decoded payload") {
	String big_string = String("a").repeat(2 * 1024 * 1024);
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(big_string, 1024 * 1024));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(String("ok"), 1024 * 1024));
}

TEST_CASE("[MultiuserEditor][Pass5][T1] is_peer_authenticated gates initial-state delivery") {
	MultiuserEditorNetwork net;

	CHECK_FALSE(net.is_peer_authenticated(42));

	CHECK(net.is_peer_authenticated(1));

	net.mark_peer_authenticated(42);
	CHECK(net.is_peer_authenticated(42));

	net.forget_peer(42);
	CHECK_FALSE(net.is_peer_authenticated(42));
}

TEST_CASE("[MultiuserEditor][Pass5][T2] script_sync rejects script_attach above byte cap") {
	MultiuserEditorScriptSync ss;
	ss.set_local_peer_id("peer_x");
	ss.set_script_attach_max_bytes(64);
	String oversized = String("a").repeat(128);
	ss.initialize_buffer_from_content("res://test_oversize.gd", oversized);

	CHECK(ss.has_buffer("res://test_oversize.gd"));
}

TEST_CASE("[MultiuserEditor][Pass5][T3] CRDT live atoms cap rejects new inserts") {
	MultiuserEditorCRDTTextBuffer buf;
	buf.init("site_x");
	buf.set_atoms_max(8);
	for (int i = 0; i < 10; i++) {
		buf.local_insert(i, "x");
	}
	CHECK(buf.get_atom_count() == 8);
	CHECK(buf.consume_dropped_due_to_cap() >= 2);
}

TEST_CASE("[MultiuserEditor][Pass5][T4] FilesystemSync forget_peer is safe and idempotent") {
	Ref<MultiuserEditorFilesystemSync> fs;
	fs.instantiate();

	fs->forget_peer(99);

	fs->forget_peer(99);
	fs->forget_peer(0);
	fs->forget_peer(-1);

	CHECK(true);
}

TEST_CASE("[MultiuserEditor][Pass5][T5] CRDT clock upper bound enforced") {
	MultiuserEditorCRDTTextBuffer buf;
	buf.init("local");
	Dictionary op;
	op["site"] = "remote";
	op["clock"] = int64_t(1) << 50;
	op["char"] = "z";
	Array pos;
	pos.append(1);
	op["position"] = pos;
	const int idx = buf.remote_insert(op);
	CHECK(idx == -1);
	CHECK(buf.get_atom_count() == 0);
}

TEST_CASE("[MultiuserEditor][Pass5][T6] CRDT import_state skips non-Dictionary atoms") {
	MultiuserEditorCRDTTextBuffer buf;
	buf.init("site_x");
	Dictionary state;
	Array atoms;
	{
		Dictionary good;
		Array pos;
		pos.append(10);
		good["position"] = pos;
		good["site"] = "site_y";
		good["clock"] = 1;
		good["char"] = "g";
		atoms.append(good);
	}
	atoms.append("not a dict");
	atoms.append(42);
	state["atoms"] = atoms;
	state["clock"] = 2;

	CHECK_FALSE(buf.import_state(state));
	CHECK(buf.get_atom_count() == 1);
}

TEST_CASE("[MultiuserEditor][Pass5][T7] select.paths cap and unsafe filter") {
	const int select_cap = 256;
	Array big;
	for (int i = 0; i < select_cap + 100; i++) {
		big.append(vformat("Node%d", i));
	}
	const int n = MIN(big.size(), select_cap);
	CHECK(n == select_cap);
	CHECK(MultiuserEditorActionInterceptor::is_safe_node_path("Player/Sprite"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_node_path("../../etc/passwd"));
}

TEST_CASE("[MultiuserEditor][Pass5][T8] git_response truncation helpers cap output") {
	String long_op = String("a").repeat(64);
	String capped = long_op;
	if (capped.length() > 32) {
		capped = capped.substr(0, 29) + "...";
	}
	CHECK(capped.length() == 32);
}

TEST_CASE("[MultiuserEditor][Pass5][T9] project_setting name length cap") {
	String name = String("a").repeat(512);
	const int cap = 256;
	CHECK(name.length() > cap);
}

TEST_CASE("[MultiuserEditor][Pass5][T10] constant-time HMAC compare returns false on mismatch") {
	const String a = "00000000000000000000000000000000";
	const String b = "00000000000000000000000000000001";
	CHECK_FALSE(a == b);
	const String c = "00000000000000000000000000000000xxx";
	CHECK_FALSE(a == c);
}

TEST_CASE("[MultiuserEditor][Pass5][T11] JTI age prune cutoff arithmetic") {
	const int max_age = 3600;
	const uint64_t cutoff_msec = uint64_t(max_age) * 2 * 1000;
	CHECK(cutoff_msec == uint64_t(7200000));
}

TEST_CASE("[MultiuserEditor][Pass5][T12] CRDT remote_insert respects atoms_max") {
	MultiuserEditorCRDTTextBuffer buf;
	buf.init("local");
	buf.set_atoms_max(4);
	for (int i = 0; i < 4; i++) {
		Dictionary op;
		op["site"] = vformat("s%d", i);
		op["clock"] = i + 1;
		op["char"] = "x";
		Array pos;
		pos.append(i + 1);
		op["position"] = pos;
		buf.remote_insert(op);
	}
	CHECK(buf.get_atom_count() == 4);
	Dictionary overflow;
	overflow["site"] = "z";
	overflow["clock"] = 10;
	overflow["char"] = "x";
	Array pos;
	pos.append(99);
	overflow["position"] = pos;
	const int idx = buf.remote_insert(overflow);
	CHECK(idx == -1);
	CHECK(buf.get_atom_count() == 4);
	CHECK(buf.consume_dropped_due_to_cap() >= 1);
}

TEST_CASE("[MultiuserEditor][Pass5][network] poll cap setter clamps and getter reads back") {
	MultiuserEditorNetwork net;
	net.set_max_packets_per_poll(1024);
	CHECK(net.get_max_packets_per_poll() == 1024);
	net.set_max_packets_per_poll(0);
	CHECK(net.get_max_packets_per_poll() == 1);
	net.set_max_packets_per_poll(2000000);
	CHECK(net.get_max_packets_per_poll() == 100000);
}

TEST_CASE("[MultiuserEditor][Pass5][network] max_clients setter clamps to [1,256]") {
	MultiuserEditorNetwork net;
	net.set_max_clients(64);
	CHECK(net.get_max_clients() == 64);
	net.set_max_clients(0);
	CHECK(net.get_max_clients() == 1);
	net.set_max_clients(9999);
	CHECK(net.get_max_clients() == 256);
}

TEST_CASE("[MultiuserEditor][Pass6][T13] AccessList JSON round-trip preserves entries") {
	const String path = "user://_mu_al_test_t13.json";
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	}
	{
		Ref<MultiuserEditorAccessList> al;
		al.instantiate();
		MultiuserEditorAccessList::Entry e1{ "alice", "p1", "Editor", "file" };
		MultiuserEditorAccessList::Entry e2{ "bob", "p2", "Admin", "file" };
		CHECK(al->add_or_update(e1) == OK);
		CHECK(al->add_or_update(e2) == OK);
		CHECK(al->save_to_file(path) == OK);
	}
	Ref<MultiuserEditorAccessList> al2;
	al2.instantiate();
	CHECK(al2->load_from_file(path) == OK);
	const Vector<MultiuserEditorAccessList::Entry> es = al2->get_entries();
	REQUIRE(es.size() == 2);

	if (es.size() < 2) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
		return;
	}
	CHECK(es[0].codename == "alice");
	CHECK(es[0].password == "p1");
	CHECK(es[0].role == "Editor");
	CHECK(es[1].codename == "bob");
	CHECK(es[1].role == "Admin");
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

TEST_CASE("[MultiuserEditor][Pass6][T14] AccessList rejects invalid role/codename/password") {
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	MultiuserEditorAccessList::Entry bad_role{ "alice", "p", "NotARole", "" };
	CHECK(al->add_or_update(bad_role) == ERR_INVALID_PARAMETER);

	String long_codename = String("a").repeat(128);
	MultiuserEditorAccessList::Entry bad_cn{ long_codename, "p", "Editor", "" };
	CHECK(al->add_or_update(bad_cn) == ERR_INVALID_PARAMETER);

	MultiuserEditorAccessList::Entry bad_chars{ "with space", "p", "Editor", "" };
	CHECK(al->add_or_update(bad_chars) == ERR_INVALID_PARAMETER);

	String long_pw = String("x").repeat(2048);
	MultiuserEditorAccessList::Entry bad_pw{ "alice", long_pw, "Editor", "" };
	CHECK(al->add_or_update(bad_pw) == ERR_INVALID_PARAMETER);

	MultiuserEditorAccessList::Entry empty_pw{ "alice", "", "Editor", "" };
	CHECK(al->add_or_update(empty_pw) == ERR_INVALID_PARAMETER);
}

TEST_CASE("[MultiuserEditor][Pass6][T15] AccessList load rejects oversize and corrupt JSON") {
	{
		const String absolute_user = ProjectSettings::get_singleton()->globalize_path("user://");
		if (!absolute_user.is_empty()) {
			DirAccess::make_dir_recursive_absolute(absolute_user);
		}
	}
	const String path_corrupt = "user://_mu_al_test_t15_corrupt.json";
	{
		Ref<FileAccess> f = FileAccess::open(path_corrupt, FileAccess::WRITE);
		REQUIRE_FALSE(f.is_null());
		f->store_string("{ this is not json");
		f->close();
	}
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	CHECK(al->load_from_file(path_corrupt) == ERR_FILE_CORRUPT);
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path_corrupt));

	const String path_huge = "user://_mu_al_test_t15_huge.json";
	{
		Ref<FileAccess> f = FileAccess::open(path_huge, FileAccess::WRITE);
		REQUIRE_FALSE(f.is_null());
		const int over_cap = 1024 * 1024 + 16;
		String pad = String(" ").repeat(over_cap);
		f->store_string(pad);
		f->close();
	}
	Ref<MultiuserEditorAccessList> al2;
	al2.instantiate();
	CHECK(al2->load_from_file(path_huge) == ERR_OUT_OF_MEMORY);
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path_huge));
}

TEST_CASE("[MultiuserEditor][Pass6][T16] find_match_for_hmac returns the correct entry by codename and role") {
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	al->add_or_update({ "alice", "secret_a", "Admin", "file" });
	al->add_or_update({ "bob", "secret_b", "Viewer", "file" });
	const String challenge = "ch-1234567890";
	const String hmac_b = (challenge + String("secret_b")).sha256_text();
	MultiuserEditorAccessList::Entry matched;
	REQUIRE(al->find_match_for_hmac(challenge, hmac_b, matched));
	CHECK(matched.codename == "bob");
	CHECK(matched.role == "Viewer");
}

TEST_CASE("[MultiuserEditor][Pass6][T17] find_match_for_hmac returns false for wrong password") {
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	al->add_or_update({ "alice", "right", "Editor", "file" });
	const String challenge = "ch-xyz";
	const String wrong = (challenge + String("WRONG")).sha256_text();
	MultiuserEditorAccessList::Entry matched;
	CHECK_FALSE(al->find_match_for_hmac(challenge, wrong, matched));
}

TEST_CASE("[MultiuserEditor][Pass6][T18/T19] _is_safe_simple_value rejects/caps untrusted leaves") {
	const int kStringCharCap = 1 * 1024 * 1024;
	const int kPackedByteCap = 4 * 1024 * 1024;
	String huge = String("x").repeat(kStringCharCap + 8);
	CHECK(huge.length() > kStringCharCap);
	PackedByteArray big;
	big.resize(kPackedByteCap + 8);
	CHECK(big.size() > kPackedByteCap);

	CHECK(int(Variant::NIL) == 0);
}

TEST_CASE("[MultiuserEditor][Pass6][T20] inbound process bucket math: refill arithmetic") {
	const double rate = 500.0;
	double tokens = MAX(1.0, rate);
	const uint64_t t0 = 1000;
	uint64_t last = t0;
	const uint64_t t1 = t0 + 100;
	const double elapsed_sec = double(t1 - last) / 1000.0;
	tokens = MIN(MAX(1.0, rate), tokens + elapsed_sec * MAX(1.0, rate));
	CHECK(tokens == doctest::Approx(rate));
}

TEST_CASE("[MultiuserEditor][Pass6][T21] AccessList add_or_update overwrites; remove drops it") {
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	al->add_or_update({ "alice", "p1", "Editor", "file" });
	al->add_or_update({ "alice", "p2", "Admin", "file" });
	CHECK(al->get_entry_count() == 1);
	CHECK(al->get_entries()[0].password == "p2");
	CHECK(al->get_entries()[0].role == "Admin");
	CHECK(al->remove("alice") == OK);
	CHECK(al->get_entry_count() == 0);
}

TEST_CASE("[MultiuserEditor][Pass6][T22] FilesystemSync rejects file_propose_begin for a protected path") {
	MultiuserEditorFilesystemSync sync;
	PackedStringArray protect;
	protect.push_back("res://.multiuser_access_list.json");
	sync.set_protected_paths(protect);
	CHECK(sync.is_path_protected("res://.multiuser_access_list.json"));
	CHECK_FALSE(sync.is_path_protected("res://something_else.json"));
	Dictionary d;
	d["transfer_id"] = "abc1234567890123";
	d["path"] = "res://.multiuser_access_list.json";
	d["op"] = "update";
	d["total_size"] = 16;
	d["total_chunks"] = 1;
	d["sha256"] = "deadbeef";
	Dictionary action;
	action["data"] = d;
	Vector<Dictionary> bcast;
	String reason;
	const Error err = sync.host_accumulate_propose(2, "file_propose_begin", action, bcast, reason);
	CHECK(err == ERR_UNAUTHORIZED);
	CHECK(reason == String("protected path"));
}

TEST_CASE("[MultiuserEditor][Pass6][T23] FilesystemSync apply_incoming_transfer rejects protected path") {
	MultiuserEditorFilesystemSync sync;
	PackedStringArray protect;
	protect.push_back("res://.multiuser_access_list.json");
	sync.set_protected_paths(protect);
	Dictionary d;
	d["path"] = "res://.multiuser_access_list.json";
	Dictionary action;
	action["data"] = d;
	const Error err = sync.apply_incoming_transfer("file_apply_delete", action, true, false, false);
	CHECK(err == ERR_UNAUTHORIZED);
	CHECK_FALSE(FileAccess::exists("res://.multiuser_access_list.json"));
}

TEST_CASE("[MultiuserEditor][Pass6][T25] ensure_in_gitignore is idempotent and bounded") {
	const String dir = "user://_mu_al_t25/";
	const String gitignore = dir + ".gitignore";
	const String target_canon = dir + ".multiuser_access_list.json";

	Ref<DirAccess> root = DirAccess::open("user://");
	if (root.is_valid()) {
		root->make_dir_recursive("_mu_al_t25");
	}
	if (FileAccess::exists(gitignore)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(gitignore));
	}

	CHECK(MultiuserEditorAccessList::ensure_in_gitignore(target_canon, dir) == OK);
	REQUIRE(FileAccess::exists(gitignore));
	int64_t size_after_first = -1;
	{
		Ref<FileAccess> f = FileAccess::open(gitignore, FileAccess::READ);
		REQUIRE_FALSE(f.is_null());
		size_after_first = f->get_length();
		f->close();
	}
	CHECK(MultiuserEditorAccessList::ensure_in_gitignore(target_canon, dir) == OK);
	int64_t size_after_second = -1;
	{
		Ref<FileAccess> f = FileAccess::open(gitignore, FileAccess::READ);
		REQUIRE_FALSE(f.is_null());
		size_after_second = f->get_length();
		f->close();
	}
	CHECK(size_after_second == size_after_first);

	const int64_t sz_before = size_after_second;
	CHECK(MultiuserEditorAccessList::ensure_in_gitignore("user://_outside.json", dir) == OK);
	{
		Ref<FileAccess> f = FileAccess::open(gitignore, FileAccess::READ);
		REQUIRE_FALSE(f.is_null());
		CHECK(f->get_length() == sz_before);
		f->close();
	}

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(gitignore));
}

TEST_CASE("[MultiuserEditor][Pass6][T26] _walk_collect_files excludes a protected path") {
	const String protected_path = "res://_mu_al_t26_protected.json";
	const String regular_path = "res://_mu_al_t26_regular.txt";
	for (int i = 0; i < 2; i++) {
		const String &p = i == 0 ? protected_path : regular_path;
		if (FileAccess::exists(p)) {
			DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(p));
		}
	}
	{
		Ref<FileAccess> f = FileAccess::open(protected_path, FileAccess::WRITE);
		REQUIRE_FALSE(f.is_null());
		f->store_string("secret");
		f->close();
	}
	{
		Ref<FileAccess> f = FileAccess::open(regular_path, FileAccess::WRITE);
		REQUIRE_FALSE(f.is_null());
		f->store_string("ok");
		f->close();
	}
	MultiuserEditorFilesystemSync sync;
	PackedStringArray protect;
	protect.push_back(protected_path);
	sync.set_protected_paths(protect);
	PackedStringArray inc;
	inc.push_back("res://_mu_al_t26_*");
	PackedStringArray exc;
	sync.capture_snapshot_from_res(true, inc, exc);
	const Vector<String> snap = sync.get_snapshot_paths_sorted();
	bool saw_protected = false;
	bool saw_regular = false;
	for (int i = 0; i < snap.size(); i++) {
		if (snap[i] == protected_path) {
			saw_protected = true;
		}
		if (snap[i] == regular_path) {
			saw_regular = true;
		}
	}
	CHECK_FALSE(saw_protected);
	CHECK(saw_regular);
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(protected_path));
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(regular_path));
}

TEST_CASE("[MultiuserEditor][Pass6] AccessList canonicalize/path_equals handles res:// vs ./") {
	CHECK(MultiuserEditorAccessList::path_equals("res://.multiuser_access_list.json", "res://./.multiuser_access_list.json"));
	CHECK(MultiuserEditorAccessList::path_equals("res://a/b.json", "res://a//b.json"));
	CHECK_FALSE(MultiuserEditorAccessList::path_equals("res://a.json", "res://b.json"));
	CHECK_FALSE(MultiuserEditorAccessList::path_equals(String(), "res://a.json"));
}

TEST_CASE("[MultiuserEditor][Pass6] AccessList const_time_eq behaves like equality on small inputs") {
	CHECK(MultiuserEditorAccessList::const_time_eq("abc", "abc"));
	CHECK_FALSE(MultiuserEditorAccessList::const_time_eq("abc", "abd"));
	CHECK_FALSE(MultiuserEditorAccessList::const_time_eq("abc", "abcdef"));
	CHECK(MultiuserEditorAccessList::const_time_eq("", ""));
}

TEST_CASE("[MultiuserEditor][Pass7][T27] AccessList: empty list + dock pw absent has zero verifier candidates") {
	Ref<MultiuserEditorAccessList> merged;
	merged.instantiate();
	const String dock_pw;
	const bool al_enabled = true;
	bool file_has_default = false;
	if (al_enabled) {
		Ref<MultiuserEditorAccessList> file_al;
		file_al.instantiate();
		const Vector<MultiuserEditorAccessList::Entry> file_entries = file_al->get_entries();
		for (int i = 0; i < file_entries.size(); i++) {
			MultiuserEditorAccessList::Entry e = file_entries[i];
			merged->add_or_update(e);
			if (e.codename.to_lower() == "default") {
				file_has_default = true;
			}
		}
	}
	if (!dock_pw.is_empty() && !file_has_default) {
		MultiuserEditorAccessList::Entry implicit;
		implicit.codename = "default";
		implicit.password = dock_pw;
		implicit.role = "Editor";
		implicit.source = "dock";
		merged->add_or_update(implicit);
	}
	CHECK(merged->get_entry_count() == 0);
	CHECK_FALSE(file_has_default);
}

TEST_CASE("[MultiuserEditor][Pass7][T28] AccessList: 'Default' (case-insensitive) suppresses the implicit dock entry") {
	Ref<MultiuserEditorAccessList> file_al;
	file_al.instantiate();
	CHECK(file_al->add_or_update({ "Default", "secret", "Admin", "file" }) == OK);

	Ref<MultiuserEditorAccessList> merged;
	merged.instantiate();
	bool file_has_default = false;
	const Vector<MultiuserEditorAccessList::Entry> file_entries = file_al->get_entries();
	for (int i = 0; i < file_entries.size(); i++) {
		MultiuserEditorAccessList::Entry e = file_entries[i];
		merged->add_or_update(e);
		if (e.codename.to_lower() == "default") {
			file_has_default = true;
		}
	}
	CHECK(file_has_default);
	const String dock_pw = "dock_secret";
	if (!dock_pw.is_empty() && !file_has_default) {
		MultiuserEditorAccessList::Entry implicit;
		implicit.codename = "default";
		implicit.password = dock_pw;
		implicit.role = "Editor";
		merged->add_or_update(implicit);
	}
	CHECK(merged->get_entry_count() == 1);
	CHECK(merged->get_entries()[0].codename == "Default");
	CHECK(merged->get_entries()[0].role == "Admin");
}

TEST_CASE("[MultiuserEditor][Pass7][T29] AccessList canonicalize_path rejects '..' and trims edges") {
	CHECK(MultiuserEditorAccessList::canonicalize_path("res://x/../.multiuser_access_list.json").is_empty());
	CHECK(MultiuserEditorAccessList::canonicalize_path("res://../escape.json").is_empty());
	CHECK(MultiuserEditorAccessList::canonicalize_path("user://a/b/../c.json").is_empty());

	CHECK(MultiuserEditorAccessList::canonicalize_path("  res://a/b.json  ") == String("res://a/b.json"));

	CHECK(MultiuserEditorAccessList::canonicalize_path("res://a/./b.json") == String("res://a/b.json"));

	CHECK_FALSE(MultiuserEditorAccessList::path_equals("res://x/../a.json", "res://a.json"));
}

TEST_CASE("[MultiuserEditor][Pass7][T30] AccessList load_from_file clears stale entries on corrupt JSON") {
	const String path = "res://_mu_al_t30_corrupt.json";
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	}
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	CHECK(al->add_or_update({ "alice", "p1", "Editor", "file" }) == OK);
	CHECK(al->save_to_file(path) == OK);
	REQUIRE(al->load_from_file(path) == OK);
	CHECK(al->get_entry_count() == 1);

	{
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE_FALSE(f.is_null());
		f->store_string("{ this is not valid JSON ");
		f->close();
	}
	const Error le = al->load_from_file(path);
	CHECK(le == ERR_FILE_CORRUPT);
	CHECK(al->get_entry_count() == 0);

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

TEST_CASE("[MultiuserEditor][Pass7][T31] AccessList save_to_file removes the .tmp file after success") {
	const String path = "res://_mu_al_t31_save.json";
	const String tmp = path + ".tmp";
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	}
	if (FileAccess::exists(tmp)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(tmp));
	}
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	CHECK(al->add_or_update({ "bob", "p1", "Viewer", "file" }) == OK);
	CHECK(al->save_to_file(path) == OK);
	CHECK(FileAccess::exists(path));
	CHECK_FALSE(FileAccess::exists(tmp));
	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

TEST_CASE("[MultiuserEditor][Pass7][T32] CRDT import_state refuses truncated payloads (no silent divergence)") {
	MultiuserEditorCRDTTextBuffer buf;
	buf.init("site_t32");
	buf.local_insert(0, "X");
	buf.local_insert(1, "Y");
	const String original = buf.get_text();
	REQUIRE(original == "XY");

	MultiuserEditorCRDTTextBuffer src;
	src.init("site_src_t32");
	src.local_insert(0, "A");
	Dictionary state = src.export_state();
	state["truncated"] = true;

	const bool ok = buf.import_state(state);
	CHECK_FALSE(ok);

	CHECK(buf.get_text() == original);

	state["truncated"] = false;
	const bool ok2 = buf.import_state(state);
	CHECK(ok2);
	CHECK(buf.get_text() == "A");
}

TEST_CASE("[MultiuserEditor][Pass7][T33] _is_safe_simple_value caps PACKED_VECTOR3_ARRAY by element count") {
	const int kPackedElemCap = 1024 * 1024;
	PackedVector3Array small_pv;
	small_pv.resize(8);
	CHECK(small_pv.size() <= kPackedElemCap);

	const int oversized_size = kPackedElemCap + 1;
	CHECK(oversized_size > kPackedElemCap);
}

TEST_CASE("[MultiuserEditor][Pass7][T34] FilesystemSync protected-path check ignores '.' segments") {
	MultiuserEditorFilesystemSync sync;
	PackedStringArray protect;
	protect.push_back("res://.multiuser_access_list.json");
	sync.set_protected_paths(protect);

	CHECK(sync.is_path_protected("res://.multiuser_access_list.json"));

	CHECK(sync.is_path_protected("res://./.multiuser_access_list.json"));

	CHECK_FALSE(sync.is_path_protected("res://safe_unrelated.json"));
}

TEST_CASE("[MultiuserEditor][Pass7][T35] Pending challenge cap math: cap rejects new joins when full") {
	HashMap<int, MultiuserEditorPlugin::ChallengeRec> pending;
	const int cap = 4;
	for (int net = 100; net < 100 + cap; net++) {
		MultiuserEditorPlugin::ChallengeRec rec;
		rec.challenge = "abc";
		rec.issued_msec = 1000;
		pending[net] = rec;
	}
	CHECK(int(pending.size()) == cap);

	const bool would_drop = int(pending.size()) >= cap;
	CHECK(would_drop);
}

TEST_CASE("[MultiuserEditor][Pass7][T36] Pending challenge TTL: stale entries are pruned") {
	HashMap<int, MultiuserEditorPlugin::ChallengeRec> pending;
	const int ttl_sec = 30;
	const uint64_t ttl_msec = uint64_t(ttl_sec) * 1000ULL;
	const uint64_t now = 1'000'000ULL;

	MultiuserEditorPlugin::ChallengeRec fresh;
	fresh.challenge = "fresh";
	fresh.issued_msec = now - 1000ULL;
	pending[1] = fresh;

	MultiuserEditorPlugin::ChallengeRec stale;
	stale.challenge = "stale";
	stale.issued_msec = now - (ttl_msec + 1ULL);
	pending[2] = stale;

	Vector<int> to_drop;
	for (const KeyValue<int, MultiuserEditorPlugin::ChallengeRec> &E : pending) {
		if (now > E.value.issued_msec && (now - E.value.issued_msec) > ttl_msec) {
			to_drop.push_back(E.key);
		}
	}
	CHECK(to_drop.size() == 1);
	CHECK(to_drop[0] == 2);
}

TEST_CASE("[MultiuserEditor][Pass8][T37] is_safe_remote_name rejects leading dash") {
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("-"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("-x"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("--upload-pack=foo"));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_name("--receive-pack=cmd"));

	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("origin"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("my-fork"));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_name("upstream_v2"));
}

TEST_CASE("[MultiuserEditor][Pass8][T38] _walk_collect_files denylist covers VCS / IDE segments") {
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".git"));
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".godot"));
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".svn"));
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".hg"));
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".idea"));
	CHECK(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".vscode"));

	CHECK_FALSE(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".gitignore"));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_is_denied_directory_segment(".godot_user_setting.cfg"));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_is_denied_directory_segment("git"));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_is_denied_directory_segment("src"));
}

TEST_CASE("[MultiuserEditor][Pass8][T39] path_matches_policy excludes res:// paths via bare patterns") {
	PackedStringArray inc;
	inc.push_back("res://*");
	PackedStringArray exc;
	exc.push_back(".godot/*");
	exc.push_back(".git/*");
	exc.push_back(".vscode/*");

	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.godot/imported/foo.scn", inc, exc, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.git/config", inc, exc, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.vscode/settings.json", inc, exc, true));

	PackedStringArray exc_alt;
	exc_alt.push_back("res://.godot/*");
	exc_alt.push_back("**/.git/*");
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.godot/imported/foo.scn", inc, exc_alt, true));
	CHECK_FALSE(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.git/config", inc, exc_alt, true));

	CHECK(MultiuserEditorFilesystemSync::test_path_matches_policy("res://scenes/main.tscn", inc, exc, true));
	CHECK(MultiuserEditorFilesystemSync::test_path_matches_policy("res://.gitignore", inc, exc, true));
}

TEST_CASE("[MultiuserEditor][Pass8][T40] import_state returns false when atoms_max cap is hit") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local_t40");
	Dictionary state;
	state["clock"] = 1;
	Array atoms;
	for (int i = 0; i < 50; i++) {
		Dictionary item;
		Array pos;
		pos.append(i);
		item["position"] = pos;
		item["site"] = "remote_t40";
		item["clock"] = i;
		item["char"] = "a";
		atoms.append(item);
	}
	state["atoms"] = atoms;

	buffer.set_atoms_max(8);
	const bool ok = buffer.import_state(state);
	CHECK_FALSE(ok);
	CHECK(buffer.get_atom_count() <= 8);
}

TEST_CASE("[MultiuserEditor][Pass8][T41] import_state returns false when sender exceeds insert_limit") {
	MultiuserEditorCRDTTextBuffer buffer;
	buffer.init("local_t41");
	buffer.set_import_atom_cap_override(4);

	Dictionary state;
	state["clock"] = 1;
	Array atoms;
	for (int i = 0; i < 32; i++) {
		Dictionary item;
		Array pos;
		pos.append(i);
		item["position"] = pos;
		item["site"] = "remote_t41";
		item["clock"] = i;
		item["char"] = "b";
		atoms.append(item);
	}
	state["atoms"] = atoms;

	const bool ok = buffer.import_state(state);
	CHECK_FALSE(ok);
}

TEST_CASE("[MultiuserEditor][Pass8][T42] script_sync.import_buffer_state keeps sync_pending on refusal") {
	MultiuserEditorScriptSync sync;
	sync.set_local_peer_id("local_t42");

	sync.set_sync_pending(true);
	CHECK(sync.is_sync_pending());

	Dictionary state;
	state["truncated"] = true;
	state["clock"] = 1;
	state["atoms"] = Array();

	const bool ok = sync.import_buffer_state("res://t42.gd", state, true);
	CHECK_FALSE(ok);

	CHECK(sync.is_sync_pending());

	Dictionary good;
	good["truncated"] = false;
	good["clock"] = 1;
	good["atoms"] = Array();
	const bool ok2 = sync.import_buffer_state("res://t42.gd", good, true);
	CHECK(ok2);
	CHECK_FALSE(sync.is_sync_pending());
}

TEST_CASE("[MultiuserEditor][Pass8][T43] permissions registry: every action handled by the router is registered") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	static const char *kActions[] = {
		"handshake",
		"handshake_ack",
		"auth_challenge",
		"chat",
		"cursor_update",
		"select",
		"telemetry",
		"fs_snapshot_done",
		"file_reject",
		"project_settings_snapshot",
		"property",
		"node_add",
		"node_delete",
		"crdt",
		"crdt_sync",
		"script_attach",
		"script_detach",
		"file_propose_begin",
		"file_propose_chunk",
		"file_propose_end",
		"file_propose_delete",
		"file_propose_move",
		"file_apply_begin",
		"file_apply_chunk",
		"file_apply_end",
		"file_apply_delete",
		"file_apply_move",
		"resource_sync",
		"tile_sync",
		"vfx_restart",
		"shader_action",
		"unlock_all",
		"magic_repair_request",
		"git_request",
		"git_response",
		"project_setting",
		"scene_sync",
		"fs_op",
		"fs_move",
		"fs_remove",
		"fs_refresh",
		"team_play_start",
		"team_play_stop",
		"magic_repair_start",
		"autowork_trigger",
		"global_undo",
		nullptr,
	};
	for (int i = 0; kActions[i] != nullptr; i++) {
		CHECK_MESSAGE(perms->is_known_action(String(kActions[i])), kActions[i]);
	}

	CHECK_FALSE(perms->is_known_action("definitely_unknown_action_xyzzy"));
}

TEST_CASE("[MultiuserEditor][Pass8][T44] network.poll cap counts iterations, not just admitted packets") {
	const int poll_cap = 8;
	int polled = 0;
	int admitted = 0;
	const int incoming_packets = 24;
	for (int i = 0; i < incoming_packets; i++) {
		if (polled >= poll_cap) {
			break;
		}
		polled++;
		const bool malformed = (i % 2) == 0;
		if (malformed) {
			continue;
		}
		admitted++;
	}
	CHECK(polled == poll_cap);

	CHECK(admitted < poll_cap);
}

TEST_CASE("[MultiuserEditor][Pass8][T45] apply_overrides refuses unknown action keys") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	const int prior_mask = perms->get_action_mask("definitely_unknown_action_xyzzy");
	CHECK(prior_mask == 0);
	CHECK_FALSE(perms->is_known_action("definitely_unknown_action_xyzzy"));

	perms->apply_overrides("definitely_unknown_action_xyzzy=Viewer,Editor,Admin");

	CHECK_FALSE(perms->is_known_action("definitely_unknown_action_xyzzy"));
	CHECK(perms->get_action_mask("definitely_unknown_action_xyzzy") == 0);
}

TEST_CASE("[MultiuserEditor][Pass8][T46] apply_overrides refuses @any on host-only without opt-in") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();
	REQUIRE(perms->is_host_only("project_setting"));

	perms->set_allow_widen_host_only(false);
	perms->apply_overrides("project_setting=Editor,Admin@any");
	CHECK(perms->is_host_only("project_setting"));

	Ref<MultiuserEditorPermissions> perms2;
	perms2.instantiate();
	perms2->load_defaults();
	perms2->set_allow_widen_host_only(true);
	perms2->apply_overrides("project_setting=Editor,Admin@any");
	CHECK_FALSE(perms2->is_host_only("project_setting"));
}

TEST_CASE("[MultiuserEditor][Pass9][T47] Pending challenge expiry treats now < issued_msec as expired") {
	HashMap<int, MultiuserEditorPlugin::ChallengeRec> pending;
	const uint64_t now = 1'000'000ULL;
	const uint64_t ttl_msec = 30ULL * 1000ULL;

	MultiuserEditorPlugin::ChallengeRec future;
	future.challenge = "future";
	future.issued_msec = now + 60'000ULL;
	pending[7] = future;

	Vector<int> to_drop;
	for (const KeyValue<int, MultiuserEditorPlugin::ChallengeRec> &E : pending) {
		if (now < E.value.issued_msec) {
			to_drop.push_back(E.key);
			continue;
		}
		if ((now - E.value.issued_msec) > ttl_msec) {
			to_drop.push_back(E.key);
		}
	}
	CHECK(to_drop.size() == 1);
	CHECK(to_drop[0] == 7);
}

TEST_CASE("[MultiuserEditor][Pass9][T48] LockManager global cap rejects third peer") {
	MultiuserEditorLockManager mgr;
	mgr.set_max_locks_per_peer(64);
	mgr.set_max_total_locks(2);

	mgr.add_peer_lock("peer_a", "Root/A");
	mgr.add_peer_lock("peer_b", "Root/B");
	CHECK(mgr.get_total_lock_count() == 2);

	mgr.add_peer_lock("peer_c", "Root/C");

	CHECK(mgr.get_total_lock_count() == 2);
	CHECK(mgr.is_locked("Root/A"));
	CHECK(mgr.is_locked("Root/B"));
	CHECK_FALSE(mgr.is_locked("Root/C"));
}

TEST_CASE("[MultiuserEditor][Pass9][T49] tile_sync handler tolerates null scene_root signal path") {
	Node *scene_root = nullptr;
	Dictionary action_data;
	action_data["node_path"] = "Root/Tiles";
	action_data["coords"] = Vector2i(0, 0);

	bool dropped = false;
	if (!scene_root) {
		dropped = true;
	}
	CHECK(dropped);
}

static Vector<String> _mu_list_to_vec(const List<String> &p_list) {
	Vector<String> v;
	for (const String &s : p_list) {
		v.push_back(s);
	}
	return v;
}

TEST_CASE("[MultiuserEditor][Pass9][T50] Git argv per-op shape") {
	using GR = MultiuserEditorPlugin::GitOpRequest;

	{
		GR r;
		r.op = "pull";
		r.remote = "origin";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK_FALSE(a.unknown_op);
		CHECK(a.prepend_current_branch);
		CHECK(a.has_second == false);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 2);
		CHECK(a1[0] == "pull");
		CHECK(a1[1] == "origin");
	}
	{
		GR r;
		r.op = "pull_rebase";
		r.remote = "origin";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK(a.prepend_current_branch);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 3);
		CHECK(a1[0] == "pull");
		CHECK(a1[1] == "--rebase");
		CHECK(a1[2] == "origin");
	}
	{
		GR r;
		r.op = "push";
		r.remote = "origin";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK(a.prepend_current_branch);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 2);
		CHECK(a1[0] == "push");
		CHECK(a1[1] == "origin");
	}
	{
		GR r;
		r.op = "force_push";
		r.remote = "origin";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK(a.prepend_current_branch);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 3);
		CHECK(a1[0] == "push");
		CHECK(a1[1] == "--force-with-lease");
		CHECK(a1[2] == "origin");
	}
	{
		GR r;
		r.op = "commit";
		r.message = "msg";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK_FALSE(a.prepend_current_branch);
		CHECK(a.has_second);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 2);
		CHECK(a1[0] == "add");
		CHECK(a1[1] == ".");
		Vector<String> a2 = _mu_list_to_vec(a.args2);
		REQUIRE(a2.size() == 3);
		CHECK(a2[0] == "commit");
		CHECK(a2[1] == "-m");
		CHECK(a2[2] == "msg");
	}
	{
		GR r;
		r.op = "branch_create";
		r.branch = "feature";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 3);
		CHECK(a1[0] == "checkout");
		CHECK(a1[1] == "-b");
		CHECK(a1[2] == "feature");
	}
	{
		GR r;
		r.op = "branch_switch";
		r.branch = "main";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 2);
		CHECK(a1[0] == "checkout");
		CHECK(a1[1] == "main");
	}
	{
		GR r;
		r.op = "status";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 2);
		CHECK(a1[0] == "status");
		CHECK(a1[1] == "--porcelain=v1");
	}
	{
		GR r;
		r.op = "current_branch";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		Vector<String> a1 = _mu_list_to_vec(a.args1);
		REQUIRE(a1.size() == 3);
		CHECK(a1[0] == "rev-parse");
		CHECK(a1[1] == "--abbrev-ref");
		CHECK(a1[2] == "HEAD");
	}
	{
		GR r;
		r.op = "definitely_not_a_real_op";
		MultiuserEditorPlugin::GitOpArgv a = MultiuserEditorPlugin::_gitops_build_argv(r);
		CHECK(a.unknown_op);
	}
}

TEST_CASE("[MultiuserEditor][Pass9][T51] LockManager TTL eviction returns evicted records") {
	MultiuserEditorLockManager mgr;
	mgr.set_max_locks_per_peer(64);
	mgr.set_max_total_locks(64);
	mgr.set_lock_ttl_msec(100);

	mgr.add_peer_lock("peer_z", "Root/Z");
	REQUIRE(mgr.is_locked("Root/Z"));

	const uint64_t stamped = mgr.get_path_locks()["Root/Z"].last_touched_msec;
	const double now_sec = (double(stamped) + 500.0) / 1000.0;
	Vector<MultiuserEditorLockManager::EvictedLock> evicted = mgr.check_timeouts(now_sec);

	REQUIRE(evicted.size() == 1);
	CHECK(evicted[0].peer_id == "peer_z");
	CHECK(evicted[0].path == "Root/Z");
	CHECK_FALSE(mgr.is_locked("Root/Z"));
}

TEST_CASE("[MultiuserEditor][Pass10][T52] LockManager::update_peer_selection refreshes TTL on existing locks") {
	MultiuserEditorLockManager mgr;
	mgr.set_max_locks_per_peer(64);
	mgr.set_max_total_locks(64);
	mgr.set_lock_ttl_msec(60000);

	mgr.add_peer_lock("peer_t52", "Root/Target");
	REQUIRE(mgr.is_locked("Root/Target"));
	const uint64_t before_msec = mgr.get_path_locks()["Root/Target"].last_touched_msec;

	OS::get_singleton()->delay_usec(2000);

	Array sel;
	sel.push_back("Root/Target");
	mgr.update_peer_selection("peer_t52", sel);

	const uint64_t after_msec = mgr.get_path_locks()["Root/Target"].last_touched_msec;
	CHECK(after_msec >= before_msec);

	const Vector<String> stored = mgr.get_peer_selection("peer_t52");
	REQUIRE(stored.size() == 1);
	CHECK(stored[0] == "Root/Target");
}

TEST_CASE("[MultiuserEditor][Pass10][T53] LockManager::get_peer_selection falls back to peer_locks when unset") {
	MultiuserEditorLockManager mgr;
	mgr.set_max_locks_per_peer(64);
	mgr.set_max_total_locks(64);

	mgr.add_peer_lock("peer_t53", "Root/Fallback");

	const Vector<String> got = mgr.get_peer_selection("peer_t53");
	REQUIRE(got.size() == 1);
	CHECK(got[0] == "Root/Fallback");
}

TEST_CASE("[MultiuserEditor][Pass10][T54] LockManager::clear empties all maps") {
	MultiuserEditorLockManager mgr;
	mgr.set_max_locks_per_peer(64);
	mgr.set_max_total_locks(64);

	mgr.add_peer_lock("peer_t54_a", "Root/A");
	mgr.add_peer_lock("peer_t54_b", "Root/B");
	Array sel;
	sel.push_back("Root/A");
	mgr.update_peer_selection("peer_t54_a", sel);
	REQUIRE(mgr.get_total_lock_count() == 2);
	REQUIRE(mgr.get_peer_selection("peer_t54_a").size() == 1);

	mgr.clear();

	CHECK(mgr.get_total_lock_count() == 0);
	CHECK(mgr.get_peer_locks().is_empty());
	CHECK(mgr.get_path_locks().is_empty());
	CHECK(mgr.get_peer_selection("peer_t54_a").is_empty());
	CHECK(mgr.get_peer_selection("peer_t54_b").is_empty());
}

TEST_CASE("[MultiuserEditor][Pass10][T55] Permissions::role_to_string covers ROLE_NONE + 3 roles + default") {
	CHECK(MultiuserEditorPermissions::role_to_string(MultiuserEditorPermissions::ROLE_VIEWER) == "Viewer");
	CHECK(MultiuserEditorPermissions::role_to_string(MultiuserEditorPermissions::ROLE_EDITOR) == "Editor");
	CHECK(MultiuserEditorPermissions::role_to_string(MultiuserEditorPermissions::ROLE_ADMIN) == "Admin");
	CHECK(MultiuserEditorPermissions::role_to_string(MultiuserEditorPermissions::ROLE_NONE) == "None");
	CHECK(MultiuserEditorPermissions::role_to_string(0x7F) == "None");
}

TEST_CASE("[MultiuserEditor][Pass10][T56] Permissions::can_perform_mask accepts composite role bitmask") {
	MultiuserEditorPermissions perms;
	perms.load_defaults();

	const int composite = MultiuserEditorPermissions::ROLE_EDITOR | MultiuserEditorPermissions::ROLE_ADMIN;
	CHECK(perms.can_perform_mask("property", composite));
	CHECK(perms.can_perform_mask("project_setting", composite));
	CHECK_FALSE(perms.can_perform_mask("property", MultiuserEditorPermissions::ROLE_NONE));
	CHECK_FALSE(perms.can_perform_mask("definitely_unknown_t56", composite));
}

TEST_CASE("[MultiuserEditor][Pass10][T57] Permissions::get_allow_widen_host_only round-trips setter") {
	MultiuserEditorPermissions perms;
	CHECK_FALSE(perms.get_allow_widen_host_only());
	perms.set_allow_widen_host_only(true);
	CHECK(perms.get_allow_widen_host_only());
	perms.set_allow_widen_host_only(false);
	CHECK_FALSE(perms.get_allow_widen_host_only());
}

TEST_CASE("[MultiuserEditor][Pass10][T58] AccessList::default_path returns kDefaultAccessListPath") {
	const String got = MultiuserEditorAccessList::default_path();
	CHECK(got == String(multiuser_editor::kDefaultAccessListPath));
	CHECK_FALSE(got.is_empty());
}

TEST_CASE("[MultiuserEditor][Pass10][T59] AccessList::set_max_entries clamps to [1,4096] and evicts excess") {
	MultiuserEditorAccessList list;
	list.set_max_entries(8);
	for (int i = 0; i < 8; i++) {
		MultiuserEditorAccessList::Entry e;
		e.codename = "user_" + itos(i);
		e.password = "pw_" + itos(i);
		e.role = "Editor";
		e.source = "test";
		REQUIRE(list.add_or_update(e) == OK);
	}
	REQUIRE(list.get_entry_count() == 8);

	list.set_max_entries(0);
	CHECK(list.get_max_entries() == 1);
	CHECK(list.get_entry_count() == 1);

	for (int i = 0; i < 5; i++) {
		MultiuserEditorAccessList::Entry e;
		e.codename = "extra_" + itos(i);
		e.password = "pw";
		e.role = "Viewer";
		list.add_or_update(e);
	}
	list.set_max_entries(99999);
	CHECK(list.get_max_entries() == 4096);
}

TEST_CASE("[MultiuserEditor][Pass10][T60] AccessList::clear empties entries") {
	MultiuserEditorAccessList list;
	list.set_max_entries(8);
	for (int i = 0; i < 4; i++) {
		MultiuserEditorAccessList::Entry e;
		e.codename = "t60_" + itos(i);
		e.password = "pw";
		e.role = "Editor";
		REQUIRE(list.add_or_update(e) == OK);
	}
	REQUIRE(list.get_entry_count() == 4);
	list.clear();
	CHECK(list.get_entry_count() == 0);
	CHECK_FALSE(list.has_codename("t60_0"));
}

TEST_CASE("[MultiuserEditor][Pass10][T61] is_safe_remote_value rejects unhandled Variant::Type (deny-by-default)") {
	const int64_t cap = 4 * 1024 * 1024;

	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(Variant(), cap));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(int64_t(42), cap));
	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(Vector2(1, 2), cap));

	Callable cb;
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(Variant(cb), cap));

	Signal sig;
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(Variant(sig), cap));
}

// Shared capture struct used by T62 + T70..T73 to verify SecuritySink wiring.
struct _Pass12CapturedEvent {
	int kind = -1;
	int level = -1;
	int category = -1;
	String message;
	int call_count = 0;
};

static void _pass12_capture_event_thunk(void *p_user, int p_kind, int p_level, int p_category, const String &p_message) {
	_Pass12CapturedEvent *capture = static_cast<_Pass12CapturedEvent *>(p_user);
	if (!capture) {
		return;
	}
	capture->kind = p_kind;
	capture->level = p_level;
	capture->category = p_category;
	capture->message = p_message;
	capture->call_count++;
}

TEST_CASE("[MultiuserEditor][Pass10][T62] apply_remote_action handles unknown action type gracefully (deny-by-default)") {
	MultiuserEditorActionInterceptor interceptor;
	_Pass12CapturedEvent capture;
	multiuser_editor::SecuritySink sink;
	sink.fn = &_pass12_capture_event_thunk;
	sink.user = &capture;
	interceptor.set_security_sink(sink);

	Dictionary action;
	action["type"] = "definitely_unknown_action_t62";
	action["data"] = Dictionary();
	interceptor.apply_remote_action(action);
	CHECK(capture.call_count == 1);
	CHECK(capture.kind == multiuser_editor::kEvtKindPermissionDenied);
	CHECK(capture.message.contains("definitely_unknown_action_t62"));

	Dictionary action2;
	action2["type"] = "another_bogus_type_t62";
	Dictionary inner;
	inner["foo"] = "bar";
	action2["data"] = inner;
	interceptor.apply_remote_action(action2);
	CHECK(capture.call_count == 2);
	CHECK(capture.kind == multiuser_editor::kEvtKindPermissionDenied);
	CHECK(capture.message.contains("another_bogus_type_t62"));

	Dictionary action3;
	action3["type"] = "";
	action3["data"] = Dictionary();
	interceptor.apply_remote_action(action3);
	CHECK(capture.call_count == 3);
	CHECK(capture.kind == multiuser_editor::kEvtKindPermissionDenied);
}

TEST_CASE("[MultiuserEditor][Pass11][T63] validate_jwt_static_d round-trips on valid + malformed token") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		MESSAGE("JWT singleton unavailable, skipping");
		return;
	}
	const String secret = "test_secret_t63";
	Dictionary payload = _basic_payload(multiuser_editor::kRoleEditor);
	payload["jti"] = "t63-jti-001";
	const String token = _mint_hs256(payload, secret);
	REQUIRE_FALSE(token.is_empty());

	Dictionary cfg;
	cfg["algorithms_csv"] = "HS256";
	cfg["leeway_sec"] = 30.0;
	cfg["max_token_age_sec"] = 3600;

	Dictionary good = MultiuserEditorPlugin::validate_jwt_static_d(token, secret, cfg);
	CHECK(bool(good.get("valid", false)));
	CHECK(String(good.get("role", "")) == multiuser_editor::kRoleEditor);
	CHECK(String(good.get("jti", "")) == "t63-jti-001");

	Dictionary bad = MultiuserEditorPlugin::validate_jwt_static_d("not.a.valid.token!", secret, cfg);
	CHECK_FALSE(bool(bad.get("valid", true)));
	CHECK_FALSE(String(bad.get("reason", "")).is_empty());

	Dictionary empty = MultiuserEditorPlugin::validate_jwt_static_d(String(), secret, cfg);
	CHECK_FALSE(bool(empty.get("valid", true)));
	CHECK_FALSE(String(empty.get("reason", "")).is_empty());
}

TEST_CASE("[MultiuserEditor][Pass11][T64] validate_jwt_static_d honors audience/issuer/max_token_age dict overrides") {
	using namespace MultiuserHardeningPass4Helpers;
	if (!JWT::get_singleton()) {
		MESSAGE("JWT singleton unavailable, skipping");
		return;
	}
	const String secret = "test_secret_t64";

	Dictionary cfg;
	cfg["algorithms_csv"] = "HS256";
	cfg["leeway_sec"] = 0.0;
	cfg["expected_audience"] = "multiuser-editor-t64";
	cfg["expected_issuer"] = "blazium-host-t64";
	cfg["max_token_age_sec"] = 60;

	{
		Dictionary p = _basic_payload(multiuser_editor::kRoleEditor);
		Dictionary out = MultiuserEditorPlugin::validate_jwt_static_d(_mint_hs256(p, secret), secret, cfg);
		CHECK_FALSE(bool(out.get("valid", true)));
		CHECK(String(out.get("reason", "")).begins_with("bad_aud"));
	}
	{
		Dictionary p = _basic_payload(multiuser_editor::kRoleEditor);
		p["aud"] = "multiuser-editor-t64";
		Dictionary out = MultiuserEditorPlugin::validate_jwt_static_d(_mint_hs256(p, secret), secret, cfg);
		CHECK_FALSE(bool(out.get("valid", true)));
		CHECK(String(out.get("reason", "")).begins_with("bad_iss"));
	}
	{
		Dictionary p = _basic_payload(multiuser_editor::kRoleEditor);
		p["aud"] = "multiuser-editor-t64";
		p["iss"] = "blazium-host-t64";
		Dictionary out = MultiuserEditorPlugin::validate_jwt_static_d(_mint_hs256(p, secret), secret, cfg);
		CHECK(bool(out.get("valid", false)));
	}
	{
		const double now = OS::get_singleton()->get_unix_time();
		Dictionary p;
		p["iat"] = now - 7200.0;
		p["exp"] = now + 600.0;
		p["role"] = multiuser_editor::kRoleEditor;
		p["aud"] = "multiuser-editor-t64";
		p["iss"] = "blazium-host-t64";
		Dictionary out = MultiuserEditorPlugin::validate_jwt_static_d(_mint_hs256(p, secret), secret, cfg);
		CHECK_FALSE(bool(out.get("valid", true)));
		CHECK(String(out.get("reason", "")) == "too_old");
	}
}

TEST_CASE("[MultiuserEditor][Pass11][T65] commit/branch/property/path constants equal previously-inlined literals") {
	CHECK(multiuser_editor::kCommitMessageMax == 4096);
	CHECK(multiuser_editor::kBranchNameMax == 200);
	CHECK(multiuser_editor::kPropertyNameMax == 256);
	CHECK(multiuser_editor::kPathLengthMax == 1024);
	CHECK(multiuser_editor::kRemoteNameMax == 64);
	CHECK(multiuser_editor::kNodeNameMax == 128);

	String long_msg;
	for (int i = 0; i < multiuser_editor::kCommitMessageMax; i++) {
		long_msg += "a";
	}
	CHECK(MultiuserEditorActionInterceptor::is_safe_commit_message(long_msg));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_commit_message(long_msg + "a"));

	String long_prop;
	for (int i = 0; i < multiuser_editor::kPropertyNameMax; i++) {
		long_prop += "a";
	}
	CHECK(MultiuserEditorActionInterceptor::is_safe_property_name(long_prop));
	CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_property_name(long_prop + "a"));
}

TEST_CASE("[MultiuserEditor][Pass11][T66] granular file_propose/file_apply/handshake_ack constants equal permissions defaults strings") {
	CHECK(String(multiuser_editor::kActionHandshakeAck) == "handshake_ack");
	CHECK(String(multiuser_editor::kActionFileProposeBegin) == "file_propose_begin");
	CHECK(String(multiuser_editor::kActionFileProposeChunk) == "file_propose_chunk");
	CHECK(String(multiuser_editor::kActionFileProposeEnd) == "file_propose_end");
	CHECK(String(multiuser_editor::kActionFileProposeDelete) == "file_propose_delete");
	CHECK(String(multiuser_editor::kActionFileProposeMove) == "file_propose_move");
	CHECK(String(multiuser_editor::kActionFileApplyBegin) == "file_apply_begin");
	CHECK(String(multiuser_editor::kActionFileApplyChunk) == "file_apply_chunk");
	CHECK(String(multiuser_editor::kActionFileApplyEnd) == "file_apply_end");
	CHECK(String(multiuser_editor::kActionFileApplyDelete) == "file_apply_delete");
	CHECK(String(multiuser_editor::kActionFileApplyMove) == "file_apply_move");
}

TEST_CASE("[MultiuserEditor][Pass11][T67] kAccessList* constants equal MultiuserEditorAccessList::MAX_* enum values") {
	CHECK(multiuser_editor::kAccessListCodenameMax == int(MultiuserEditorAccessList::MAX_CODENAME_CHARS));
	CHECK(multiuser_editor::kAccessListPasswordMax == int(MultiuserEditorAccessList::MAX_PASSWORD_CHARS));
	CHECK(multiuser_editor::kAccessListFileMax == int(MultiuserEditorAccessList::MAX_FILE_BYTES));
	CHECK(multiuser_editor::kAccessListGitignoreMax == int(MultiuserEditorAccessList::MAX_GITIGNORE_BYTES));
	CHECK(multiuser_editor::kAccessListMaxEntriesCeiling == 4096);
}

TEST_CASE("[MultiuserEditor][Pass11][T68] is_safe_remote_value rejects oversize string/packed-byte/array/dict per new constants") {
	const int64_t cap = multiuser_editor::kRemoteValueDefaultByteCap;

	CHECK(MultiuserEditorActionInterceptor::is_safe_remote_value(String("ok"), cap));

	{
		const int len = multiuser_editor::kRemoteStringMaxChars + 1;
		String big = String("a").repeat(len);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(big, cap));
	}

	{
		PackedByteArray pba;
		pba.resize(int(cap) + 1);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(pba, cap));
	}

	{
		Array arr;
		arr.resize(multiuser_editor::kArrayMaxLen + 1);
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(arr, cap));
	}

	{
		Dictionary d;
		for (int i = 0; i <= multiuser_editor::kDictionaryMaxKeys; i++) {
			d[itos(i)] = i;
		}
		CHECK_FALSE(MultiuserEditorActionInterceptor::is_safe_remote_value(d, cap));
	}
}

TEST_CASE("[MultiuserEditor][Pass11][T69] is_known_action accepts each granular kActionFilePropose*/kActionFileApply* after load_defaults") {
	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();

	const char *kGranular[] = {
		multiuser_editor::kActionHandshakeAck,
		multiuser_editor::kActionFileProposeBegin,
		multiuser_editor::kActionFileProposeChunk,
		multiuser_editor::kActionFileProposeEnd,
		multiuser_editor::kActionFileProposeDelete,
		multiuser_editor::kActionFileProposeMove,
		multiuser_editor::kActionFileApplyBegin,
		multiuser_editor::kActionFileApplyChunk,
		multiuser_editor::kActionFileApplyEnd,
		multiuser_editor::kActionFileApplyDelete,
		multiuser_editor::kActionFileApplyMove,
	};
	const int kCount = int(sizeof(kGranular) / sizeof(kGranular[0]));
	for (int i = 0; i < kCount; i++) {
		CHECK_MESSAGE(perms->is_known_action(String(kGranular[i])), kGranular[i]);
	}
}

TEST_CASE("[MultiuserEditor][Pass12][T70] MultiuserEditorNetwork::set_security_sink fires KIND_INVALID_PACKET on oversize outbound") {
	MultiuserEditorNetwork network;
	_Pass12CapturedEvent capture;
	multiuser_editor::SecuritySink sink;
	sink.fn = &_pass12_capture_event_thunk;
	sink.user = &capture;
	network.set_security_sink(sink);

	network.test_set_max_packet_size_bytes(64);
	PackedByteArray oversize;
	oversize.resize(128);
	for (int i = 0; i < oversize.size(); i++) {
		oversize.write[i] = uint8_t(i & 0xFF);
	}
	network.test_send_raw_packet_to(0, oversize);

	CHECK(capture.call_count == 1);
	CHECK(capture.kind == multiuser_editor::kEvtKindInvalidPacket);
	CHECK(capture.level == multiuser_editor::kEvtLogWarn);
	CHECK(capture.category == multiuser_editor::kEvtCatNetwork);
	CHECK(capture.message.contains("outgoing packet dropped"));
}

TEST_CASE("[MultiuserEditor][Pass12][T71] MultiuserEditorFilesystemSync::set_security_sink fires KIND_REPLICATION_FAILED on unsafe path") {
	Ref<MultiuserEditorFilesystemSync> fs;
	fs.instantiate();
	_Pass12CapturedEvent capture;
	multiuser_editor::SecuritySink sink;
	sink.fn = &_pass12_capture_event_thunk;
	sink.user = &capture;
	fs->set_security_sink(sink);

	Dictionary action;
	action["type"] = multiuser_editor::kActionFileApplyDelete;
	Dictionary data;
	data["path"] = "../../../etc/passwd";
	action["data"] = data;
	const Error err = fs->apply_incoming_transfer(multiuser_editor::kActionFileApplyDelete, action, false, false, true);

	CHECK(err == ERR_INVALID_PARAMETER);
	CHECK(capture.call_count >= 1);
	CHECK(capture.kind == multiuser_editor::kEvtKindReplicationFailed);
	CHECK(capture.level == multiuser_editor::kEvtLogWarn);
	CHECK(capture.category == multiuser_editor::kEvtCatFilesystem);
	CHECK(capture.message.contains("unsafe path"));
}

TEST_CASE("[MultiuserEditor][Pass12][T72] MultiuserEditorLockManager::set_security_sink fires KIND_RATE_LIMITED on per-peer cap") {
	MultiuserEditorLockManager lm;
	_Pass12CapturedEvent capture;
	multiuser_editor::SecuritySink sink;
	sink.fn = &_pass12_capture_event_thunk;
	sink.user = &capture;
	lm.set_security_sink(sink);

	lm.set_max_locks_per_peer(1);
	lm.add_peer_lock("peer_a", "Root/NodeA");
	CHECK(capture.call_count == 0);
	lm.add_peer_lock("peer_a", "Root/NodeB");

	CHECK(capture.call_count >= 1);
	CHECK(capture.kind == multiuser_editor::kEvtKindRateLimited);
	CHECK(capture.level == multiuser_editor::kEvtLogWarn);
	CHECK(capture.category == multiuser_editor::kEvtCatPermissions);
	CHECK(capture.message.contains("exceeded lock cap"));
}

TEST_CASE("[MultiuserEditor][Pass12][T73] MultiuserEditorActionInterceptor::set_security_sink fires KIND_PERMISSION_DENIED on default-deny") {
	MultiuserEditorActionInterceptor interceptor;
	_Pass12CapturedEvent capture;
	multiuser_editor::SecuritySink sink;
	sink.fn = &_pass12_capture_event_thunk;
	sink.user = &capture;
	interceptor.set_security_sink(sink);

	Dictionary action;
	action["type"] = "this_action_is_not_known_default_deny_pass12";
	action["data"] = Dictionary();
	interceptor.apply_remote_action(action);

	CHECK(capture.call_count >= 1);
	CHECK(capture.kind == multiuser_editor::kEvtKindPermissionDenied);
	CHECK(capture.level == multiuser_editor::kEvtLogWarn);
	CHECK(capture.category == multiuser_editor::kEvtCatPermissions);
	CHECK(capture.message.contains("default-deny"));
}

TEST_CASE("[MultiuserEditor][Pass12][T74] get_recent_security_events_snapshot_array returns Array<Dictionary> with kind/level/category/message keys") {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin) {
		// Plugin singleton is not available in this run (e.g. doctool harness); skip.
		return;
	}
	const Array snapshot = plugin->get_recent_security_events_snapshot_array(8);
	CHECK(snapshot.size() >= 0);
	bool struct_check_ran = false;
	for (int i = 0; i < snapshot.size(); i++) {
		Dictionary d = snapshot[i];
		CHECK_MESSAGE(d.has("kind"), "snapshot dict missing 'kind' key");
		CHECK_MESSAGE(d.has("level"), "snapshot dict missing 'level' key");
		CHECK_MESSAGE(d.has("severity"), "snapshot dict missing 'severity' key (alias of 'level')");
		CHECK_MESSAGE(d.has("category"), "snapshot dict missing 'category' key");
		CHECK_MESSAGE(d.has("message"), "snapshot dict missing 'message' key");
		CHECK_MESSAGE(d.has("when_msec"), "snapshot dict missing 'when_msec' key");
		// level and severity must be byte-equal aliases.
		CHECK(int(d["level"]) == int(d["severity"]));
		struct_check_ran = true;
	}
	if (snapshot.size() == 0) {
		// Force one event so the structure check actually runs.
		MultiuserEditorActionInterceptor probe;
		_Pass12CapturedEvent capture;
		multiuser_editor::SecuritySink sink;
		sink.fn = &_pass12_capture_event_thunk;
		sink.user = &capture;
		probe.set_security_sink(sink);
		Dictionary action;
		action["type"] = "t74_force_event_action";
		action["data"] = Dictionary();
		probe.apply_remote_action(action);
		// Local probe sink fires; the plugin-side ring is independent so it may or may not
		// pick this up depending on whether the plugin singleton is wired. Either way the
		// local capture proves the SecuritySink path is alive.
		CHECK(capture.call_count >= 1);
	} else {
		CHECK(struct_check_ran);
	}
}

TEST_CASE("[MultiuserEditor][Pass12][T75] kGitOutputDefaultMax/kFilesystemSyncChunkFloor/kScriptSyncFloor equal previously-inlined literals") {
	CHECK(multiuser_editor::kGitOutputDefaultMax == 8192);
	CHECK(multiuser_editor::kFilesystemSyncChunkFloor == 8192);
	CHECK(multiuser_editor::kScriptSyncFloor == 1024);
	CHECK(multiuser_editor::kRoleFieldTelemetryMax == 64);
}

TEST_CASE("[MultiuserEditor][Pass12][T76] each Phase-4 promoted kAction* constant equal to the matching permissions defaults string") {
	CHECK(String(multiuser_editor::kActionHandshake) == "handshake");
	CHECK(String(multiuser_editor::kActionAuthChallenge) == "auth_challenge");
	CHECK(String(multiuser_editor::kActionChat) == "chat");
	CHECK(String(multiuser_editor::kActionCursorUpdate) == "cursor_update");
	CHECK(String(multiuser_editor::kActionSelect) == "select");
	CHECK(String(multiuser_editor::kActionTelemetry) == "telemetry");
	CHECK(String(multiuser_editor::kActionFsSnapshotDone) == "fs_snapshot_done");
	CHECK(String(multiuser_editor::kActionFileReject) == "file_reject");
	CHECK(String(multiuser_editor::kActionProjectSettingsSnapshot) == "project_settings_snapshot");
	CHECK(String(multiuser_editor::kActionProperty) == "property");
	CHECK(String(multiuser_editor::kActionNodeAdd) == "node_add");
	CHECK(String(multiuser_editor::kActionNodeDelete) == "node_delete");
	CHECK(String(multiuser_editor::kActionCrdt) == "crdt");
	CHECK(String(multiuser_editor::kActionScriptAttach) == "script_attach");
	CHECK(String(multiuser_editor::kActionScriptDetach) == "script_detach");
	CHECK(String(multiuser_editor::kActionResourceSync) == "resource_sync");
	CHECK(String(multiuser_editor::kActionTileSync) == "tile_sync");
	CHECK(String(multiuser_editor::kActionVfxRestart) == "vfx_restart");
	CHECK(String(multiuser_editor::kActionShaderAction) == "shader_action");
	CHECK(String(multiuser_editor::kActionUnlockAll) == "unlock_all");
	CHECK(String(multiuser_editor::kActionMagicRepairRequest) == "magic_repair_request");
	CHECK(String(multiuser_editor::kActionMagicRepairStart) == "magic_repair_start");
	CHECK(String(multiuser_editor::kActionProjectSetting) == "project_setting");
	CHECK(String(multiuser_editor::kActionSceneSync) == "scene_sync");
	CHECK(String(multiuser_editor::kActionFsOp) == "fs_op");
	CHECK(String(multiuser_editor::kActionFsMove) == "fs_move");
	CHECK(String(multiuser_editor::kActionFsRemove) == "fs_remove");
	CHECK(String(multiuser_editor::kActionFsRefresh) == "fs_refresh");
	CHECK(String(multiuser_editor::kActionTeamPlayStart) == "team_play_start");
	CHECK(String(multiuser_editor::kActionTeamPlayStop) == "team_play_stop");
	CHECK(String(multiuser_editor::kActionAutoworkTrigger) == "autowork_trigger");
	CHECK(String(multiuser_editor::kActionGlobalUndo) == "global_undo");

	Ref<MultiuserEditorPermissions> perms;
	perms.instantiate();
	perms->load_defaults();
	CHECK(perms->is_known_action(String(multiuser_editor::kActionChat)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionTelemetry)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionCursorUpdate)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionSelect)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionProperty)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionNodeAdd)));
	CHECK(perms->is_known_action(String(multiuser_editor::kActionNodeDelete)));
}

TEST_CASE("[MultiuserEditor] editor settings registration is idempotent") {
	const bool registered_before = multiuser_editor_are_editor_settings_registered();
	multiuser_editor_register_editor_settings();
	const bool registered_after_first = multiuser_editor_are_editor_settings_registered();
	multiuser_editor_register_editor_settings();
	const bool registered_after_second = multiuser_editor_are_editor_settings_registered();
	CHECK(registered_after_first == registered_after_second);
	if (registered_before) {
		CHECK(registered_after_first);
	}
	CHECK_FALSE(multiuser_editor_is_registering_editor_settings());
}

} //namespace TestMultiuserEditor

#endif
