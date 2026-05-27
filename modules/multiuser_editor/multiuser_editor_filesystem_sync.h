/**************************************************************************/
/*  multiuser_editor_filesystem_sync.h                                    */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/pair.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "multiuser_editor_security_sink.h"

class MultiuserEditorFilesystemSync : public RefCounted {
public:
	enum DeltaKind {
		DELTA_CREATE,
		DELTA_UPDATE,
		DELTA_DELETE,
		DELTA_MOVE,
	};

	struct FileEntry {
		uint64_t size = 0;
		uint64_t modified_usec = 0;
		String hash_hex;
	};

	struct Delta {
		DeltaKind kind = DELTA_CREATE;
		String path;
		String old_path;
		String hash_hex;
		uint64_t size = 0;
	};

private:
	HashMap<String, FileEntry> _snapshot;

	struct ReceiveState {
		String path;
		String old_path;
		String op;
		int64_t total_size = 0;
		int total_chunks = 0;
		String expected_hash_hex;
		HashMap<int, PackedByteArray> chunks_by_index;
		int received_count = 0;
		int64_t accumulated_bytes = 0;
		bool include_import_sidecars = true;
	};
	HashMap<String, ReceiveState> _receive_by_transfer_id;

	struct ProposeState {
		int sender_net_id = 0;
		String path;
		String old_path;
		String op;
		int64_t total_size = 0;
		int total_chunks = 0;
		String expected_hash_hex;
		HashMap<int, PackedByteArray> chunks_by_index;
		int received_count = 0;
		int64_t accumulated_bytes = 0;
	};
	HashMap<String, ProposeState> _propose_by_transfer_id;

	Vector<Pair<String, String>> _recent_apply;

	void _walk_collect_files(const String &p_dir, bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude, Vector<String> &r_out) const;
	static bool _single_glob_match(const String &p_path, const String &p_pattern);

	static bool _is_denied_directory_segment(const String &p_segment);

public:
	static bool path_matches_policy(const String &p_path, const PackedStringArray &p_include, const PackedStringArray &p_exclude, bool p_include_import_sidecars);
	static String hash_file_hex(const String &p_res_path);
	static String hash_bytes_hex(const Vector<uint8_t> &p_bytes);

	void clear_snapshot();
	void clear_pending();

	void forget_peer(int p_net_id);
	void capture_snapshot_from_res(bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude);
	Vector<String> get_snapshot_paths_sorted() const;
	Vector<Delta> diff_and_update_snapshot(bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude, int64_t p_max_file_bytes);

	void remember_applied(const String &p_path, const String &p_hash_hex);
	bool should_skip_path_hash(const String &p_path, const String &p_hash_hex) const;

	static Vector<Dictionary> build_transfer_actions(
			const String &p_type_prefix,
			const Delta &p_delta,
			int64_t p_max_file_bytes,
			int64_t p_chunk_bytes,
			bool p_include_import_sidecars,
			String &r_transfer_id_out);

	Error host_accumulate_propose(int p_sender_net_id, const String &p_type, const Dictionary &p_action, Vector<Dictionary> &r_broadcast_on_success, String &r_reject_reason_out);

	Error apply_incoming_transfer(const String &p_type, const Dictionary &p_action, bool p_write_disk, bool p_trigger_reimport, bool p_include_import_sidecars);

	static void apply_delete(const String &p_res_path);
	static void apply_move(const String &p_old_res, const String &p_new_res);
	static Error write_bytes_to_res_path(const String &p_res_path, const Vector<uint8_t> &p_bytes, bool p_create_dir);

	static bool should_skip_host_relay(const Dictionary &p_action);
	static int get_server_peer_id() { return 1; }

	static bool test_path_matches_policy(const String &p_path, const PackedStringArray &p_inc, const PackedStringArray &p_exc, bool p_imports);
	static String test_hash_bytes_hex(const Vector<uint8_t> &p_bytes) { return hash_bytes_hex(p_bytes); }

	static bool test_is_denied_directory_segment(const String &p_segment) { return _is_denied_directory_segment(p_segment); }
	void test_walk_collect_files(const String &p_dir, bool p_include_import_sidecars, const PackedStringArray &p_include, const PackedStringArray &p_exclude, Vector<String> &r_out) const { _walk_collect_files(p_dir, p_include_import_sidecars, p_include, p_exclude, r_out); }

	void set_sync_policy(const PackedStringArray &p_include, const PackedStringArray &p_exclude, bool p_include_import_sidecars, int64_t p_max_file_bytes, int64_t p_chunk_bytes);
	void set_concurrent_transfer_cap(int p_cap) { _concurrent_transfer_cap = MAX(1, p_cap); }
	int get_concurrent_transfer_cap() const { return _concurrent_transfer_cap; }
	void set_transfer_id_max_chars(int p_cap) { _transfer_id_max_chars = MAX(16, p_cap); }
	int get_transfer_id_max_chars() const { return _transfer_id_max_chars; }

	void set_protected_paths(const PackedStringArray &p_paths);
	bool is_path_protected(const String &p_path) const;
	int get_protected_path_count() const { return _protected_paths.size(); }

	void set_security_sink(const multiuser_editor::SecuritySink &p_sink) { _security = p_sink; }

private:
	PackedStringArray _policy_include;
	PackedStringArray _policy_exclude;
	bool _policy_include_imports = true;
	int64_t _policy_max_file_bytes = 64 * 1024 * 1024;
	int64_t _policy_chunk_bytes = 262144;
	int _concurrent_transfer_cap = 8;
	int _transfer_id_max_chars = 128;

	HashSet<String> _protected_paths;
	multiuser_editor::SecuritySink _security;
};

#endif
