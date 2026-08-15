/**************************************************************************/
/*  multiuser_editor_filesystem_sync.cpp                                  */
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

#include "multiuser_editor_filesystem_sync.h"

#include "multiuser_editor_access_list.h"
#include "multiuser_editor_action_interceptor.h"
#include "multiuser_editor_constants.h"

#include "core/config/project_settings.h"
#include "core/crypto/hashing_context.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"

#define RECENT_APPLY_MAX (multiuser_editor::kFilesystemSyncRecentApplyMax)

static void _maybe_reimport(const String &p_path);

static int64_t _res_file_length(const String &p_res_path) {
	Ref<FileAccess> f = FileAccess::open(p_res_path, FileAccess::READ);
	if (f.is_null()) {
		return -1;
	}
	return int64_t(f->get_length());
}

void MultiuserEditorFilesystemSync::set_sync_policy(const PackedStringArray &p_include, const PackedStringArray &p_exclude, bool p_include_import_sidecars, int64_t p_max_file_bytes, int64_t p_chunk_bytes) {
	_policy_include = p_include;
	_policy_exclude = p_exclude;
	_policy_include_imports = p_include_import_sidecars;
	_policy_max_file_bytes = p_max_file_bytes > 0 ? p_max_file_bytes : (64 * 1024 * 1024);
	_policy_chunk_bytes = CLAMP(p_chunk_bytes, 4096, 4 * 1024 * 1024);
}

void MultiuserEditorFilesystemSync::set_protected_paths(const PackedStringArray &p_paths) {
	_protected_paths.clear();
	for (int i = 0; i < p_paths.size(); i++) {
		const String canon = MultiuserEditorAccessList::canonicalize_path(p_paths[i]);
		if (!canon.is_empty()) {
			_protected_paths.insert(canon);
		}
	}
}

bool MultiuserEditorFilesystemSync::is_path_protected(const String &p_path) const {
	if (_protected_paths.is_empty() || p_path.is_empty()) {
		return false;
	}
	const String canon = MultiuserEditorAccessList::canonicalize_path(p_path);
	if (canon.is_empty()) {
		return false;
	}
	return _protected_paths.has(canon);
}

bool MultiuserEditorFilesystemSync::_single_glob_match(const String &p_path, const String &p_pattern) {
	if (p_pattern.is_empty()) {
		return false;
	}

	if (p_path.matchn(p_pattern)) {
		return true;
	}
	String stripped_path = p_path;
	if (stripped_path.begins_with("res://")) {
		stripped_path = stripped_path.substr(6, stripped_path.length() - 6);
	}
	if (stripped_path != p_path && stripped_path.matchn(p_pattern)) {
		return true;
	}
	String stripped_pattern = p_pattern;
	if (stripped_pattern.begins_with("res://")) {
		stripped_pattern = stripped_pattern.substr(6, stripped_pattern.length() - 6);
	} else if (stripped_pattern.begins_with("**/")) {
		stripped_pattern = stripped_pattern.substr(3, stripped_pattern.length() - 3);
	} else if (stripped_pattern.begins_with("/")) {
		stripped_pattern = stripped_pattern.substr(1, stripped_pattern.length() - 1);
	}
	if (stripped_pattern != p_pattern && stripped_path.matchn(stripped_pattern)) {
		return true;
	}
	return false;
}

bool MultiuserEditorFilesystemSync::path_matches_policy(const String &p_path, const PackedStringArray &p_include, const PackedStringArray &p_exclude, bool p_include_import_sidecars) {
	if (!p_path.begins_with("res://")) {
		return false;
	}
	if (!MultiuserEditorActionInterceptor::is_safe_file_path(p_path)) {
		return false;
	}
	if (p_path.contains("..")) {
		return false;
	}
	if (!p_include_import_sidecars && p_path.ends_with(".import")) {
		return false;
	}
	bool matched_include = false;
	if (p_include.is_empty()) {
		matched_include = true;
	} else {
		for (int i = 0; i < p_include.size(); i++) {
			if (_single_glob_match(p_path, p_include[i])) {
				matched_include = true;
				break;
			}
		}
	}
	if (!matched_include) {
		return false;
	}
	for (int i = 0; i < p_exclude.size(); i++) {
		if (_single_glob_match(p_path, p_exclude[i])) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorFilesystemSync::_is_denied_directory_segment(const String &p_segment) {
	if (p_segment == ".git" || p_segment == ".godot" || p_segment == ".svn" ||
			p_segment == ".hg" || p_segment == ".idea" || p_segment == ".vscode") {
		return true;
	}
	return false;
}

void MultiuserEditorFilesystemSync::_walk_collect_files(const String &p_dir, bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude, Vector<String> &r_out) const {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (n == "." || n == "..") {
			n = da->get_next();
			continue;
		}
		const String rel = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (_is_denied_directory_segment(n)) {
				n = da->get_next();
				continue;
			}
			_walk_collect_files(rel, p_include_import_sidecars, p_include, p_exclude, r_out);
		} else {
			if (!is_path_protected(rel) && path_matches_policy(rel, p_include, p_exclude, p_include_import_sidecars)) {
				r_out.push_back(rel);
			}
		}
		n = da->get_next();
	}
	da->list_dir_end();
}

String MultiuserEditorFilesystemSync::hash_bytes_hex(const Vector<uint8_t> &p_bytes) {
	Ref<HashingContext> ctx;
	ctx.instantiate();
	ctx->start(HashingContext::HASH_SHA256);
	if (!p_bytes.is_empty()) {
		PackedByteArray pba;
		pba.resize(p_bytes.size());
		memcpy(pba.ptrw(), p_bytes.ptr(), p_bytes.size());
		ctx->update(pba);
	}
	PackedByteArray out = ctx->finish();
	return String::hex_encode_buffer(out.ptr(), out.size());
}

String MultiuserEditorFilesystemSync::hash_file_hex(const String &p_res_path) {
	Ref<FileAccess> f = FileAccess::open(p_res_path, FileAccess::READ);
	if (f.is_null()) {
		return String();
	}
	Ref<HashingContext> ctx;
	ctx.instantiate();
	ctx->start(HashingContext::HASH_SHA256);
	const int64_t buf_size = multiuser_editor::kFilesystemSyncHashReadChunk;
	while (!f->eof_reached()) {
		PackedByteArray chunk = f->get_buffer(buf_size);
		if (!chunk.is_empty()) {
			ctx->update(chunk);
		}
	}
	f->close();
	PackedByteArray fin = ctx->finish();
	return String::hex_encode_buffer(fin.ptr(), fin.size());
}

void MultiuserEditorFilesystemSync::clear_snapshot() {
	_snapshot.clear();
}

void MultiuserEditorFilesystemSync::clear_pending() {
	_receive_by_transfer_id.clear();
	_propose_by_transfer_id.clear();
}

void MultiuserEditorFilesystemSync::forget_peer(int p_net_id) {
	const String key_prefix = String::num_int64(p_net_id) + ":";
	Vector<String> to_drop;
	for (const KeyValue<String, ProposeState> &E : _propose_by_transfer_id) {
		if (E.value.sender_net_id == p_net_id || E.key.begins_with(key_prefix)) {
			to_drop.push_back(E.key);
		}
	}
	for (const String &k : to_drop) {
		_propose_by_transfer_id.erase(k);
	}
}

void MultiuserEditorFilesystemSync::capture_snapshot_from_res(bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude) {
	_snapshot.clear();
	if (!DirAccess::dir_exists_absolute("res://") && !DirAccess::open("res://").is_valid()) {
		print_verbose("Multiuser file sync: capture_snapshot_from_res aborted - res:// is not accessible.");
		return;
	}
	Vector<String> files;
	_walk_collect_files(String("res://"), p_include_import_sidecars, p_include, p_exclude, files);
	for (const String &path : files) {
		FileEntry fe;
		if (!FileAccess::exists(path)) {
			continue;
		}
		const int64_t flen = _res_file_length(path);
		if (flen < 0) {
			continue;
		}
		if (_policy_max_file_bytes > 0 && flen > _policy_max_file_bytes) {
			continue;
		}
		fe.size = uint64_t(flen);
		fe.modified_usec = FileAccess::get_modified_time(path);
		fe.hash_hex = hash_file_hex(path);
		if (!fe.hash_hex.is_empty()) {
			_snapshot.insert(path, fe);
		}
	}
}

Vector<String> MultiuserEditorFilesystemSync::get_snapshot_paths_sorted() const {
	Vector<String> keys;
	for (const KeyValue<String, FileEntry> &E : _snapshot) {
		keys.push_back(E.key);
	}
	keys.sort();
	return keys;
}

Vector<MultiuserEditorFilesystemSync::Delta> MultiuserEditorFilesystemSync::diff_and_update_snapshot(bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude, int64_t p_max_file_bytes) {
	Vector<Delta> out;
	HashMap<String, FileEntry> current;
	Vector<String> files;
	_walk_collect_files(String("res://"), p_include_import_sidecars, p_include, p_exclude, files);
	for (const String &path : files) {
		if (!FileAccess::exists(path)) {
			continue;
		}
		const int64_t sz = _res_file_length(path);
		if (sz < 0) {
			continue;
		}
		if (sz > p_max_file_bytes) {
			print_verbose("Multiuser file sync: skipping oversized file: " + path);
			continue;
		}
		FileEntry fe;
		fe.size = uint64_t(sz);
		fe.modified_usec = FileAccess::get_modified_time(path);
		fe.hash_hex = hash_file_hex(path);
		if (!fe.hash_hex.is_empty()) {
			current.insert(path, fe);
		}
	}

	Vector<String> deleted_paths;
	for (const KeyValue<String, FileEntry> &E : _snapshot) {
		if (!current.has(E.key)) {
			deleted_paths.push_back(E.key);
		}
	}

	Vector<String> created_paths;
	for (const KeyValue<String, FileEntry> &E : current) {
		if (!_snapshot.has(E.key)) {
			created_paths.push_back(E.key);
		}
	}

	HashMap<String, String> hash_to_deleted_path;
	for (const String &dp : deleted_paths) {
		const FileEntry &old_fe = _snapshot[dp];
		hash_to_deleted_path.insert(old_fe.hash_hex, dp);
	}

	HashSet<String> deleted_consumed;
	HashSet<String> created_consumed;

	for (int ci = 0; ci < created_paths.size(); ci++) {
		const String &cp = created_paths[ci];
		const FileEntry &cfe = current[cp];
		if (!hash_to_deleted_path.has(cfe.hash_hex)) {
			continue;
		}
		const String old_p = hash_to_deleted_path[cfe.hash_hex];
		if (deleted_consumed.has(old_p)) {
			continue;
		}
		Delta d;
		d.kind = DELTA_MOVE;
		d.old_path = old_p;
		d.path = cp;
		d.hash_hex = cfe.hash_hex;
		d.size = cfe.size;
		out.push_back(d);
		deleted_consumed.insert(old_p);
		created_consumed.insert(cp);
		hash_to_deleted_path.erase(cfe.hash_hex);
	}

	for (const String &dp : deleted_paths) {
		if (deleted_consumed.has(dp)) {
			continue;
		}
		const FileEntry &old_fe = _snapshot[dp];
		Delta d;
		d.kind = DELTA_DELETE;
		d.path = dp;
		d.hash_hex = old_fe.hash_hex;
		d.size = old_fe.size;
		out.push_back(d);
	}

	for (int ci = 0; ci < created_paths.size(); ci++) {
		const String &cp = created_paths[ci];
		if (created_consumed.has(cp)) {
			continue;
		}
		const FileEntry &cfe = current[cp];
		Delta d;
		d.kind = DELTA_CREATE;
		d.path = cp;
		d.hash_hex = cfe.hash_hex;
		d.size = cfe.size;
		out.push_back(d);
	}

	for (const KeyValue<String, FileEntry> &E : current) {
		if (!_snapshot.has(E.key)) {
			continue;
		}
		const FileEntry &old_fe = _snapshot[E.key];
		const FileEntry &new_fe = E.value;
		if (old_fe.hash_hex != new_fe.hash_hex || old_fe.size != new_fe.size) {
			Delta d;
			d.kind = DELTA_UPDATE;
			d.path = E.key;
			d.hash_hex = new_fe.hash_hex;
			d.size = new_fe.size;
			out.push_back(d);
		}
	}

	_snapshot = current;
	return out;
}

void MultiuserEditorFilesystemSync::remember_applied(const String &p_path, const String &p_hash_hex) {
	_recent_apply.push_back(Pair<String, String>(p_path, p_hash_hex));
	while (_recent_apply.size() > RECENT_APPLY_MAX) {
		_recent_apply.remove_at(0);
	}
}

bool MultiuserEditorFilesystemSync::should_skip_path_hash(const String &p_path, const String &p_hash_hex) const {
	for (int i = _recent_apply.size() - 1; i >= 0; i--) {
		if (_recent_apply[i].first == p_path && _recent_apply[i].second == p_hash_hex) {
			return true;
		}
	}
	return false;
}

Vector<Dictionary> MultiuserEditorFilesystemSync::build_transfer_actions(
		const String &p_type_prefix,
		const Delta &p_delta,
		int64_t p_max_file_bytes,
		int64_t p_chunk_bytes,
		bool p_include_import_sidecars,
		String &r_transfer_id_out) {
	Vector<Dictionary> actions;
	const int64_t chunk_sz = CLAMP(p_chunk_bytes, 4096, 4 * 1024 * 1024);

	if (p_delta.kind == DELTA_DELETE) {
		Dictionary data;
		data["path"] = p_delta.path;
		Dictionary act = Dictionary();
		act["type"] = p_type_prefix + "_delete";
		act["data"] = data;
		actions.push_back(act);
		return actions;
	}
	if (p_delta.kind == DELTA_MOVE) {
		Dictionary data;
		data["old_path"] = p_delta.old_path;
		data["new_path"] = p_delta.path;
		Dictionary act = Dictionary();
		act["type"] = p_type_prefix + "_move";
		act["data"] = data;
		actions.push_back(act);
		return actions;
	}

	Ref<FileAccess> f = FileAccess::open(p_delta.path, FileAccess::READ);
	if (f.is_null()) {
		return actions;
	}
	int64_t sz = f->get_length();
	if (sz > p_max_file_bytes) {
		print_line("Multiuser file sync: file too large to transfer: " + p_delta.path);
		return actions;
	}
	Vector<uint8_t> bytes;
	bytes.resize(int(sz));
	if (sz > 0) {
		const PackedByteArray readb = f->get_buffer(sz);
		if (readb.size() != sz) {
			return actions;
		}
		memcpy(bytes.ptrw(), readb.ptr(), sz);
	}
	f->close();

	const String computed = hash_bytes_hex(bytes);
	if (!p_delta.hash_hex.is_empty() && computed != p_delta.hash_hex) {
		print_line("Multiuser file sync: hash mismatch before send: " + p_delta.path);
	}

	r_transfer_id_out = String::num_uint64(OS::get_singleton()->get_ticks_usec() ^ (uint64_t)Math::rand(), 16).sha256_text().substr(0, multiuser_editor::kFilesystemSyncTransferIdHexWidth);

	const int total_chunks = sz == 0 ? 0 : int((sz + chunk_sz - 1) / chunk_sz);

	Dictionary begin_data;
	begin_data["path"] = p_delta.path;
	begin_data["op"] = p_delta.kind == DELTA_CREATE ? String("create") : String("update");
	begin_data["old_path"] = String();
	begin_data["total_size"] = sz;
	begin_data["total_chunks"] = total_chunks;
	begin_data["sha256"] = computed;
	begin_data["transfer_id"] = r_transfer_id_out;
	begin_data["mtime_usec"] = FileAccess::get_modified_time(p_delta.path);
	begin_data["include_import_sidecars"] = p_include_import_sidecars;

	Dictionary begin_act;
	begin_act["type"] = p_type_prefix + "_begin";
	begin_act["data"] = begin_data;
	actions.push_back(begin_act);

	for (int i = 0; i < total_chunks; i++) {
		const int64_t from = int64_t(i) * chunk_sz;
		const int64_t to = MIN(from + chunk_sz, sz);
		const int64_t len = to - from;
		PackedByteArray chunk;
		chunk.resize(int(len));
		if (len > 0) {
			memcpy(chunk.ptrw(), bytes.ptr() + from, size_t(len));
		}
		Dictionary ch_data;
		ch_data["transfer_id"] = r_transfer_id_out;
		ch_data["chunk_index"] = i;
		ch_data["chunk_data"] = chunk;
		Dictionary ch_act;
		ch_act["type"] = p_type_prefix + "_chunk";
		ch_act["data"] = ch_data;
		actions.push_back(ch_act);
	}

	Dictionary end_data;
	end_data["transfer_id"] = r_transfer_id_out;
	end_data["sha256"] = computed;
	Dictionary end_act;
	end_act["type"] = p_type_prefix + "_end";
	end_act["data"] = end_data;
	actions.push_back(end_act);

	(void)p_include_import_sidecars;
	return actions;
}

static String _propose_key(int p_sender, const String &p_tid) {
	return String::num_int64(p_sender) + ":" + p_tid;
}

static bool _is_safe_transfer_id(const String &p_tid, int p_max_chars) {
	if (p_tid.is_empty() || p_tid.length() > p_max_chars) {
		return false;
	}
	for (int i = 0; i < p_tid.length(); i++) {
		const char32_t c = p_tid[i];
		if (c == 0 || c < 0x20) {
			return false;
		}
	}
	return true;
}

Error MultiuserEditorFilesystemSync::host_accumulate_propose(int p_sender_net_id, const String &p_type, const Dictionary &p_action, Vector<Dictionary> &r_broadcast_on_success, String &r_reject_reason_out) {
	r_broadcast_on_success.clear();
	r_reject_reason_out = String();
	const Dictionary data = p_action.get("data", Dictionary());
	const String tid = String(data.get("transfer_id", ""));
	if ((p_type == multiuser_editor::kActionFileProposeBegin || p_type == multiuser_editor::kActionFileProposeChunk || p_type == multiuser_editor::kActionFileProposeEnd) && !_is_safe_transfer_id(tid, _transfer_id_max_chars)) {
		r_reject_reason_out = "invalid_transfer_id";
		return ERR_INVALID_PARAMETER;
	}

	if (p_type == multiuser_editor::kActionFileProposeDelete) {
		const String path = String(data.get("path", ""));

		if (is_path_protected(path)) {
			r_reject_reason_out = "protected path";
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(path, _policy_include, _policy_exclude, _policy_include_imports)) {
			r_reject_reason_out = "policy";
			return ERR_UNAUTHORIZED;
		}
		apply_delete(path);
		Dictionary act;
		act["type"] = multiuser_editor::kActionFileApplyDelete;
		act["data"] = data;
		r_broadcast_on_success.push_back(act);
		return OK;
	}
	if (p_type == multiuser_editor::kActionFileProposeMove) {
		const String old_p = String(data.get("old_path", ""));
		const String new_p = String(data.get("new_path", ""));

		if (is_path_protected(old_p) || is_path_protected(new_p)) {
			r_reject_reason_out = "protected path";
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(old_p, _policy_include, _policy_exclude, _policy_include_imports) ||
				!path_matches_policy(new_p, _policy_include, _policy_exclude, _policy_include_imports)) {
			r_reject_reason_out = "policy";
			return ERR_UNAUTHORIZED;
		}
		apply_move(old_p, new_p);
		Dictionary act;
		act["type"] = multiuser_editor::kActionFileApplyMove;
		act["data"] = data;
		r_broadcast_on_success.push_back(act);
		return OK;
	}

	if (p_type == multiuser_editor::kActionFileProposeBegin) {
		ProposeState st;
		st.sender_net_id = p_sender_net_id;
		st.path = String(data.get("path", ""));
		st.old_path = String(data.get("old_path", ""));
		st.op = String(data.get("op", "update"));
		st.total_size = int64_t(data.get("total_size", 0));
		st.total_chunks = int(data.get("total_chunks", 0));
		st.expected_hash_hex = String(data.get("sha256", ""));
		st.received_count = 0;
		if (tid.is_empty() || st.path.is_empty() || !MultiuserEditorActionInterceptor::is_safe_file_path(st.path)) {
			r_reject_reason_out = "invalid_begin";
			return ERR_INVALID_PARAMETER;
		}

		if (is_path_protected(st.path)) {
			r_reject_reason_out = "protected path";
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(st.path, _policy_include, _policy_exclude, _policy_include_imports)) {
			r_reject_reason_out = "policy";
			return ERR_UNAUTHORIZED;
		}
		if (st.total_size < 0 || st.total_size > _policy_max_file_bytes) {
			r_reject_reason_out = "too_large";
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_chunks < 0 || st.total_chunks > multiuser_editor::kFilesystemSyncTotalChunksMax) {
			r_reject_reason_out = "invalid_total_chunks";
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_size == 0 && st.total_chunks != 0) {
			r_reject_reason_out = "invalid_total_chunks";
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_size > 0) {
			const int64_t max_reasonable_chunks = (st.total_size + 1023) / 1024;
			if (int64_t(st.total_chunks) > max_reasonable_chunks) {
				r_reject_reason_out = "invalid_total_chunks";
				return ERR_INVALID_PARAMETER;
			}
		}
		const String key_prefix = String::num_int64(p_sender_net_id) + ":";
		int active_for_peer = 0;
		for (const KeyValue<String, ProposeState> &E : _propose_by_transfer_id) {
			if (E.key.begins_with(key_prefix)) {
				active_for_peer++;
			}
		}
		if (active_for_peer >= _concurrent_transfer_cap) {
			r_reject_reason_out = "too_many_concurrent_transfers";
			return ERR_BUSY;
		}
		_propose_by_transfer_id.insert(_propose_key(p_sender_net_id, tid), st);
		return ERR_BUSY;
	}

	if (p_type == multiuser_editor::kActionFileProposeChunk) {
		const String key = _propose_key(p_sender_net_id, tid);
		if (!_propose_by_transfer_id.has(key)) {
			r_reject_reason_out = "unknown_transfer";
			return ERR_DOES_NOT_EXIST;
		}
		ProposeState &st = _propose_by_transfer_id[key];
		const int idx = int(data.get("chunk_index", -1));
		const PackedByteArray chunk = data.get("chunk_data", PackedByteArray());
		if (idx < 0 || (st.total_chunks > 0 && idx >= st.total_chunks)) {
			r_reject_reason_out = "bad_chunk_index";
			_propose_by_transfer_id.erase(key);
			return ERR_INVALID_PARAMETER;
		}
		const int64_t chunk_cap = MAX(int64_t(_policy_chunk_bytes) * 2, int64_t(multiuser_editor::kFilesystemSyncChunkFloor));
		if (chunk.size() > chunk_cap) {
			r_reject_reason_out = "chunk_too_large";
			_propose_by_transfer_id.erase(key);
			return ERR_INVALID_PARAMETER;
		}
		if (st.accumulated_bytes + chunk.size() > st.total_size) {
			r_reject_reason_out = "exceeds_total_size";
			_propose_by_transfer_id.erase(key);
			return ERR_INVALID_PARAMETER;
		}
		if (st.received_count > st.total_chunks + 4) {
			r_reject_reason_out = "too_many_chunks";
			_propose_by_transfer_id.erase(key);
			return ERR_INVALID_PARAMETER;
		}
		st.chunks_by_index.insert(idx, chunk);
		st.received_count++;
		st.accumulated_bytes += chunk.size();
		return ERR_BUSY;
	}

	if (p_type == multiuser_editor::kActionFileProposeEnd) {
		const String key = _propose_key(p_sender_net_id, tid);
		if (!_propose_by_transfer_id.has(key)) {
			r_reject_reason_out = "unknown_transfer";
			return ERR_DOES_NOT_EXIST;
		}
		ProposeState st = _propose_by_transfer_id[key];
		_propose_by_transfer_id.erase(key);

		if (st.total_chunks > 0 && st.chunks_by_index.size() != (uint32_t)st.total_chunks) {
			r_reject_reason_out = "incomplete_chunks";
			return ERR_INVALID_DATA;
		}
		Vector<uint8_t> all;
		all.resize(int(st.total_size));
		int64_t off = 0;
		for (int i = 0; i < st.total_chunks; i++) {
			if (!st.chunks_by_index.has(i)) {
				r_reject_reason_out = "missing_chunk";
				return ERR_INVALID_DATA;
			}
			const PackedByteArray &c = st.chunks_by_index[i];
			memcpy(all.ptrw() + off, c.ptr(), c.size());
			off += c.size();
		}
		if (off != st.total_size) {
			r_reject_reason_out = "size_mismatch";
			return ERR_INVALID_DATA;
		}
		const String h = hash_bytes_hex(all);
		if (h != st.expected_hash_hex) {
			r_reject_reason_out = "hash_mismatch";
			return ERR_INVALID_DATA;
		}

		const Error we = write_bytes_to_res_path(st.path, all, true);
		if (we != OK) {
			r_reject_reason_out = "host_write_failed";
			return we;
		}
		if (EditorInterface::get_singleton()) {
			if (EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_filesystem()) {
				efs->update_file(st.path);
			}
		}
		_maybe_reimport(st.path);

		Delta d;
		d.kind = (st.op == "create") ? DELTA_CREATE : DELTA_UPDATE;
		d.path = st.path;
		d.hash_hex = h;
		d.size = uint64_t(st.total_size);
		String dummy;
		r_broadcast_on_success = build_transfer_actions(String("file_apply"), d, _policy_max_file_bytes, _policy_chunk_bytes, _policy_include_imports, dummy);
		return OK;
	}

	r_reject_reason_out = "unknown_type";
	return ERR_UNAVAILABLE;
}

Error MultiuserEditorFilesystemSync::write_bytes_to_res_path(const String &p_res_path, const Vector<uint8_t> &p_bytes, bool p_create_dir) {
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_res_path, canonical)) {
		print_line("Multiuser file sync: write rejected unsafe path: " + p_res_path);
		return ERR_FILE_CANT_WRITE;
	}
	const String base_dir = canonical.get_base_dir();
	if (p_create_dir && base_dir != String("res://") && base_dir.begins_with("res://")) {
		Ref<DirAccess> root = DirAccess::open("res://");
		if (root.is_valid()) {
			String rel_dir = base_dir.substr(6, base_dir.length() - 6);
			root->make_dir_recursive(rel_dir);
		}
	}
	Ref<FileAccess> f = FileAccess::open(canonical, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	if (!p_bytes.is_empty()) {
		PackedByteArray pba;
		pba.resize(p_bytes.size());
		memcpy(pba.ptrw(), p_bytes.ptr(), p_bytes.size());
		f->store_buffer(pba);
	}
	f->close();
	return OK;
}

void MultiuserEditorFilesystemSync::apply_delete(const String &p_res_path) {
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_res_path, canonical)) {
		print_line("Multiuser file sync: apply_delete rejected unsafe path: " + p_res_path);
		return;
	}
	if (FileAccess::exists(canonical)) {
		Ref<DirAccess> root = DirAccess::open("res://");
		if (root.is_valid()) {
			String rel_path = canonical.substr(6, canonical.length() - 6);
			root->remove(rel_path);
		}
	}
	if (EditorInterface::get_singleton()) {
		if (EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_filesystem()) {
			efs->update_file(canonical);
		}
	}
}

void MultiuserEditorFilesystemSync::apply_move(const String &p_old_res, const String &p_new_res) {
	String old_canonical;
	String new_canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_old_res, old_canonical) ||
			!MultiuserEditorActionInterceptor::canonicalize_res_path(p_new_res, new_canonical)) {
		print_line("Multiuser file sync: apply_move rejected unsafe path");
		return;
	}
	Ref<DirAccess> root = DirAccess::open("res://");
	if (root.is_valid()) {
		String rel_old = old_canonical.substr(6, old_canonical.length() - 6);
		String rel_new = new_canonical.substr(6, new_canonical.length() - 6);
		root->rename(rel_old, rel_new);
	}
	if (EditorInterface::get_singleton()) {
		if (EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_filesystem()) {
			efs->update_file(old_canonical);
			efs->update_file(new_canonical);
		}
	}
}

static void _maybe_reimport(const String &p_path) {
	if (p_path.ends_with(".import")) {
		return;
	}
	if (!EditorInterface::get_singleton()) {
		return;
	}
	EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_filesystem();
	if (!efs) {
		return;
	}
	Vector<String> vf;
	vf.push_back(p_path);
	efs->reimport_files(vf);
}

Error MultiuserEditorFilesystemSync::apply_incoming_transfer(const String &p_type, const Dictionary &p_action, bool p_write_disk, bool p_trigger_reimport, bool p_include_import_sidecars) {
	(void)p_include_import_sidecars;
	const Dictionary data = p_action.get("data", Dictionary());

	if (p_type == multiuser_editor::kActionFileApplyDelete) {
		String canonical;
		const String raw = String(data.get("path", ""));
		if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw, canonical)) {
			const String msg = "Multiuser file sync: file_apply_delete rejected unsafe path: " + raw;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}

		if (is_path_protected(canonical)) {
			const String msg = "Multiuser file sync: file_apply_delete rejected protected path: " + canonical;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindProtectedPath, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(canonical, _policy_include, _policy_exclude, _policy_include_imports)) {
			const String msg = "Multiuser file sync: file_apply_delete rejected by policy: " + canonical;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (p_write_disk) {
			apply_delete(canonical);
		}
		return OK;
	}
	if (p_type == multiuser_editor::kActionFileApplyMove) {
		String old_canonical;
		String new_canonical;
		const String raw_old = String(data.get("old_path", ""));
		const String raw_new = String(data.get("new_path", ""));
		if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_old, old_canonical) ||
				!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_new, new_canonical)) {
			const String msg = "Multiuser file sync: file_apply_move rejected unsafe path";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}

		if (is_path_protected(old_canonical) || is_path_protected(new_canonical)) {
			const String msg = "Multiuser file sync: file_apply_move rejected protected path";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindProtectedPath, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(old_canonical, _policy_include, _policy_exclude, _policy_include_imports) ||
				!path_matches_policy(new_canonical, _policy_include, _policy_exclude, _policy_include_imports)) {
			const String msg = "Multiuser file sync: file_apply_move rejected by policy";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (p_write_disk) {
			apply_move(old_canonical, new_canonical);
		}
		return OK;
	}

	const String tid = String(data.get("transfer_id", ""));
	if ((p_type == multiuser_editor::kActionFileApplyBegin || p_type == multiuser_editor::kActionFileApplyChunk || p_type == multiuser_editor::kActionFileApplyEnd) && !_is_safe_transfer_id(tid, _transfer_id_max_chars)) {
		const String msg = vformat("Multiuser file sync: %s rejected: invalid transfer_id (len=%d)", p_type, tid.length());
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
		return ERR_INVALID_PARAMETER;
	}

	if (p_type == multiuser_editor::kActionFileApplyBegin) {
		ReceiveState st;
		const String raw_path = String(data.get("path", ""));
		const String raw_old = String(data.get("old_path", ""));
		if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_path, st.path)) {
			const String msg = "Multiuser file sync: file_apply_begin rejected unsafe path: " + raw_path;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (!raw_old.is_empty()) {
			if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_old, st.old_path)) {
				const String msg = "Multiuser file sync: file_apply_begin rejected unsafe old_path: " + raw_old;
				print_verbose(msg);
				_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
				return ERR_INVALID_PARAMETER;
			}
		}
		st.op = String(data.get("op", "update"));
		st.total_size = int64_t(data.get("total_size", 0));
		st.total_chunks = int(data.get("total_chunks", 0));
		st.expected_hash_hex = String(data.get("sha256", ""));
		st.received_count = 0;
		st.include_import_sidecars = bool(data.get("include_import_sidecars", true));
		if (tid.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}

		if (is_path_protected(st.path)) {
			const String msg = "Multiuser file sync: file_apply_begin rejected protected path: " + st.path;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindProtectedPath, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_UNAUTHORIZED;
		}
		if (!path_matches_policy(st.path, _policy_include, _policy_exclude, _policy_include_imports)) {
			const String msg = "Multiuser file sync: file_apply_begin rejected by policy: " + st.path;
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_size < 0 || st.total_size > _policy_max_file_bytes) {
			const String msg = vformat("Multiuser file sync: file_apply_begin rejected: size %d exceeds max %d", int(st.total_size), int(_policy_max_file_bytes));
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_chunks < 0 || st.total_chunks > multiuser_editor::kFilesystemSyncTotalChunksMax) {
			const String msg = vformat("Multiuser file sync: file_apply_begin rejected: total_chunks=%d out of range", st.total_chunks);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_size == 0 && st.total_chunks != 0) {
			const String msg = "Multiuser file sync: file_apply_begin rejected: total_chunks present for empty file";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_INVALID_PARAMETER;
		}
		if (st.total_size > 0) {
			const int64_t max_reasonable_chunks = (st.total_size + 1023) / 1024;
			if (int64_t(st.total_chunks) > max_reasonable_chunks) {
				const String msg = vformat("Multiuser file sync: file_apply_begin rejected: total_chunks=%d exceeds total_size-based ceiling %d", st.total_chunks, int(max_reasonable_chunks));
				print_verbose(msg);
				_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
				return ERR_INVALID_PARAMETER;
			}
		}
		if (int(_receive_by_transfer_id.size()) >= _concurrent_transfer_cap) {
			const String msg = vformat("Multiuser file sync: file_apply_begin rejected: concurrent transfer cap %d reached", _concurrent_transfer_cap);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindRateLimited, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			return ERR_BUSY;
		}
		_receive_by_transfer_id.insert(tid, st);
		return ERR_BUSY;
	}

	if (p_type == multiuser_editor::kActionFileApplyChunk) {
		if (!_receive_by_transfer_id.has(tid)) {
			return ERR_DOES_NOT_EXIST;
		}
		ReceiveState &st = _receive_by_transfer_id[tid];
		const int idx = int(data.get("chunk_index", -1));
		const PackedByteArray chunk = data.get("chunk_data", PackedByteArray());
		if (idx < 0 || (st.total_chunks > 0 && idx >= st.total_chunks)) {
			_receive_by_transfer_id.erase(tid);
			return ERR_INVALID_PARAMETER;
		}
		const int64_t chunk_cap = MAX(int64_t(_policy_chunk_bytes) * 2, int64_t(multiuser_editor::kFilesystemSyncChunkFloor));
		if (chunk.size() > chunk_cap) {
			const String msg = vformat("Multiuser file sync: file_apply_chunk rejected: chunk size %d exceeds cap %d", int(chunk.size()), int(chunk_cap));
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			_receive_by_transfer_id.erase(tid);
			return ERR_INVALID_PARAMETER;
		}
		if (st.accumulated_bytes + chunk.size() > st.total_size) {
			const String msg = "Multiuser file sync: file_apply_chunk rejected: exceeds announced total_size";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			_receive_by_transfer_id.erase(tid);
			return ERR_INVALID_PARAMETER;
		}
		if (st.received_count > st.total_chunks + 4) {
			const String msg = "Multiuser file sync: file_apply_chunk rejected: too many chunks";
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindInvalidPacket, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatFilesystem, msg);
			_receive_by_transfer_id.erase(tid);
			return ERR_INVALID_PARAMETER;
		}
		st.chunks_by_index.insert(idx, chunk);
		st.received_count++;
		st.accumulated_bytes += chunk.size();
		return ERR_BUSY;
	}

	if (p_type == multiuser_editor::kActionFileApplyEnd) {
		if (!_receive_by_transfer_id.has(tid)) {
			return ERR_DOES_NOT_EXIST;
		}
		ReceiveState st = _receive_by_transfer_id[tid];
		_receive_by_transfer_id.erase(tid);

		if (st.total_chunks > 0 && st.chunks_by_index.size() != (uint32_t)st.total_chunks) {
			return ERR_INVALID_DATA;
		}
		Vector<uint8_t> all;
		all.resize(int(st.total_size));
		int64_t off = 0;
		for (int i = 0; i < st.total_chunks; i++) {
			if (!st.chunks_by_index.has(i)) {
				return ERR_INVALID_DATA;
			}
			const PackedByteArray &c = st.chunks_by_index[i];
			memcpy(all.ptrw() + off, c.ptr(), c.size());
			off += c.size();
		}
		if (off != st.total_size) {
			return ERR_INVALID_DATA;
		}
		const String h = hash_bytes_hex(all);
		if (h != st.expected_hash_hex) {
			return ERR_INVALID_DATA;
		}
		if (p_write_disk) {
			const Error e = write_bytes_to_res_path(st.path, all, true);
			if (e != OK) {
				return e;
			}
			if (EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_filesystem()) {
				efs->update_file(st.path);
			}
			if (p_trigger_reimport) {
				_maybe_reimport(st.path);
			}
			remember_applied(st.path, h);
		}
		return OK;
	}

	return ERR_UNAVAILABLE;
}

bool MultiuserEditorFilesystemSync::should_skip_host_relay(const Dictionary &p_action) {
	const String t = String(p_action.get("type", ""));
	return t == multiuser_editor::kActionHandshake ||
			t == multiuser_editor::kActionFileProposeBegin || t == multiuser_editor::kActionFileProposeChunk || t == multiuser_editor::kActionFileProposeEnd ||
			t == multiuser_editor::kActionFileProposeDelete || t == multiuser_editor::kActionFileProposeMove;
}

bool MultiuserEditorFilesystemSync::test_path_matches_policy(const String &p_path, const PackedStringArray &p_inc, const PackedStringArray &p_exc, bool p_imports) {
	return path_matches_policy(p_path, p_inc, p_exc, p_imports);
}

#endif
