/**************************************************************************/
/*  godot_sqlite.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/callable.h"
#include "core/variant/typed_array.h"
#include "thirdparty/spmemvfs/spmemvfs.h"
#include "thirdparty/sqlite/sqlite3.h"

class SQLiteColumnSchema : public RefCounted {
	GDCLASS(SQLiteColumnSchema, RefCounted);
	String name;
	Variant::Type type = Variant::NIL;
	Variant default_value;
	bool primary_key = false;
	bool auto_increment = false;
	bool not_null = false;
	bool unique = false;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_name"), &SQLiteColumnSchema::get_name);
		ClassDB::bind_method(D_METHOD("get_type"), &SQLiteColumnSchema::get_type);
		ClassDB::bind_method(D_METHOD("get_default_value"), &SQLiteColumnSchema::get_default_value);
		ClassDB::bind_method(D_METHOD("is_primary_key"), &SQLiteColumnSchema::is_primary_key);
		ClassDB::bind_method(D_METHOD("is_auto_increment"), &SQLiteColumnSchema::is_auto_increment);
		ClassDB::bind_method(D_METHOD("is_not_null"), &SQLiteColumnSchema::is_not_null);
		ClassDB::bind_method(D_METHOD("is_unique"), &SQLiteColumnSchema::is_unique);

		ClassDB::bind_method(D_METHOD("set_name", "name"), &SQLiteColumnSchema::set_name);
		ClassDB::bind_method(D_METHOD("set_type", "type"), &SQLiteColumnSchema::set_type);
		ClassDB::bind_method(D_METHOD("set_default_value", "default_value"), &SQLiteColumnSchema::set_default_value);
		ClassDB::bind_method(D_METHOD("set_primary_key", "primary_key"), &SQLiteColumnSchema::set_primary_key);
		ClassDB::bind_method(D_METHOD("set_auto_increment", "auto_increment"), &SQLiteColumnSchema::set_auto_increment);
		ClassDB::bind_method(D_METHOD("set_not_null", "not_null"), &SQLiteColumnSchema::set_not_null);
		ClassDB::bind_method(D_METHOD("set_unique", "unique"), &SQLiteColumnSchema::set_unique);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "type"), "set_type", "get_type");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_value"), "set_default_value", "get_default_value");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "primary_key"), "set_primary_key", "is_primary_key");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_increment"), "set_auto_increment", "is_auto_increment");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "not_null"), "set_not_null", "is_not_null");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unique"), "set_unique", "is_unique");

		ClassDB::bind_static_method("SQLiteColumnSchema", D_METHOD("create", "name", "type", "default_value", "primary_key", "auto_increment", "not_null", "unique"), &SQLiteColumnSchema::create, DEFVAL(Variant::Type::STRING), DEFVAL(Variant()), DEFVAL(false), DEFVAL(false), DEFVAL(false), DEFVAL(false));
	}

public:
	static Ref<SQLiteColumnSchema> create(const String &p_name, Variant::Type p_type, const Variant &p_default_value, bool p_primary_key, bool p_auto_increment, bool p_not_null, bool p_unique) {
		Ref<SQLiteColumnSchema> schema;
		schema.instantiate();
		schema->set_name(p_name);
		schema->set_type(p_type);
		schema->set_default_value(p_default_value);
		schema->set_primary_key(p_primary_key);
		schema->set_auto_increment(p_auto_increment);
		schema->set_not_null(p_not_null);
		schema->set_unique(p_unique);
		return schema;
	}
	String get_name() const { return name; }
	Variant::Type get_type() const { return type; }
	Variant get_default_value() const { return default_value; }
	bool is_primary_key() const { return primary_key; }
	bool is_auto_increment() const { return auto_increment; }
	bool is_not_null() const { return not_null; }
	bool is_unique() const { return unique; }

	void set_name(const String &p_name) { name = p_name; }
	void set_type(Variant::Type p_type) { type = p_type; }
	void set_default_value(const Variant &p_default_value) { default_value = p_default_value; }
	void set_primary_key(bool p_primary_key) { primary_key = p_primary_key; }
	void set_auto_increment(bool p_auto_increment) { auto_increment = p_auto_increment; }
	void set_not_null(bool p_not_null) { not_null = p_not_null; }
	void set_unique(bool p_unique) { unique = p_unique; }
};

class SQLiteAccess;

class SQLiteQueryResult : public RefCounted {
	GDCLASS(SQLiteQueryResult, RefCounted);
	TypedArray<Dictionary> result;
	Array arguments;
	String query;
	String error;
	int error_code = 0;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_result"), &SQLiteQueryResult::get_result);
		ClassDB::bind_method(D_METHOD("get_error"), &SQLiteQueryResult::get_error);
		ClassDB::bind_method(D_METHOD("get_error_code"), &SQLiteQueryResult::get_error_code);
		ClassDB::bind_method(D_METHOD("get_query"), &SQLiteQueryResult::get_query);
		ClassDB::bind_method(D_METHOD("get_arguments"), &SQLiteQueryResult::get_arguments);

		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "result", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "", "get_result");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "error"), "", "get_error");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "error_code"), "", "get_error_code");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "query"), "", "get_query");
		ADD_PROPERTY(PropertyInfo(Variant::NIL, "arguments", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "", "get_arguments");
	}

public:
	Variant get_arguments() const { return arguments; }
	TypedArray<Dictionary> get_result() const { return result; }
	String get_error() const { return error; }
	int get_error_code() const { return error_code; }
	String get_query() const { return query; }

	void set_result(TypedArray<Dictionary> p_result) { result = p_result; }
	void set_error(String p_error) { error = p_error; }
	void set_error_code(int p_error_code) { error_code = p_error_code; }
	void set_query(String p_query) { query = p_query; }
	void set_arguments(Variant p_arguments) { arguments = p_arguments; }
};

class SQLiteBackup : public RefCounted {
	GDCLASS(SQLiteBackup, RefCounted);

	sqlite3_backup *backup = nullptr;
	sqlite3 *side_db = nullptr;

protected:
	static void _bind_methods();

public:
	~SQLiteBackup();
	void init(sqlite3_backup *p_backup, sqlite3 *p_side_db) {
		backup = p_backup;
		side_db = p_side_db;
	}
	int step(int pages = 100);
	int get_remaining() const;
	int get_page_count() const;
	void finish();
};

class SQLiteBlob : public RefCounted {
	GDCLASS(SQLiteBlob, RefCounted);

	sqlite3_blob *blob = nullptr;
	int size = 0;

protected:
	static void _bind_methods();

public:
	~SQLiteBlob();
	void init(sqlite3_blob *p_blob) {
		blob = p_blob;
		if (blob) {
			size = sqlite3_blob_bytes(blob);
		}
	}
	int get_size() const { return size; }
	PackedByteArray read_chunk(int p_offset, int p_length);
	bool write_chunk(int p_offset, const PackedByteArray &p_buffer);
	void close();
};

class SQLiteQuery : public RefCounted {
	GDCLASS(SQLiteQuery, RefCounted);

	Variant arguments;
	SQLiteAccess *db = nullptr;
	sqlite3_stmt *stmt = nullptr;
	String query;

protected:
	static void _bind_methods();

public:
	SQLiteQuery();
	~SQLiteQuery();
	void init(SQLiteAccess *p_db, const String &p_query, Variant p_args);
	bool is_ready() const;
	String get_query() const { return query; }
	String get_last_error_message() const;
	Variant get_arguments() const { return arguments; }
	void set_arguments(Variant p_arguments) { arguments = p_arguments; }
	TypedArray<SQLiteColumnSchema> get_columns();
	void finalize();
	Ref<SQLiteQueryResult> execute(const Variant p_args);
	TypedArray<SQLiteQueryResult> batch_execute(Array p_rows);
	int get_stmt_status(int p_op, bool p_reset = false);

private:
	bool prepare();
};

class SQLiteAccess : public RefCounted {
	GDCLASS(SQLiteAccess, RefCounted);

	friend SQLiteQuery;

private:
	sqlite3 *db = nullptr;
	spmemvfs_db_t spmemvfs_db{};
	bool memory_read = false;

	::LocalVector<Ref<WeakRef>> queries;

	struct SQLiteCallableContext {
		Callable func;
	};
	struct SQLiteAggregateContext {
		Callable step;
		Callable final;
	};
	HashMap<String, SQLiteCallableContext *> custom_functions;
	HashMap<String, SQLiteCallableContext *> custom_collations;
	HashMap<String, SQLiteAggregateContext *> custom_aggregates;

	static void _sqlite_udf_static_trampoline(sqlite3_context *context, int argc, sqlite3_value **argv);
	static void _sqlite_udf_step_trampoline(sqlite3_context *context, int argc, sqlite3_value **argv);
	static void _sqlite_udf_final_trampoline(sqlite3_context *context);
	static int _sqlite_collation_trampoline(void *ctx, int len1, const void *str1, int len2, const void *str2);

	sqlite3_stmt *prepare(const char *statement);
	sqlite3 *get_handler() const { return memory_read ? spmemvfs_db.handle : db; }

public:
	static String bind_args(sqlite3_stmt *stmt, const Array &args);
	static String bind_args_dict(sqlite3_stmt *stmt, const Dictionary &args);

	Callable authorizer_callable;
	static int _sqlite_trace_callback(unsigned int mask, void *ctx, void *p, void *x);
	static int _sqlite_authorizer_callback(void *p_user, int p_action, const char *p_arg1, const char *p_arg2, const char *p_db_name, const char *p_trigger_name);
	static void _sqlite_update_hook_callback(void *p_arg, int p_op, char const *p_db_name, char const *p_table_name, sqlite3_int64 p_rowid);
	static int _sqlite_commit_hook_callback(void *p_arg);
	static void _sqlite_rollback_hook_callback(void *p_arg);
	static int _sqlite_progress_handler_callback(void *p_arg);
	static int _sqlite_wal_hook_callback(void *p_arg, sqlite3 *db, const char *db_name, int pages_in_wal);

protected:
	void _configure_hooks();
	static void _bind_methods();

public:
	enum { RESULT_BOTH = 0,
		RESULT_NUM,
		RESULT_ASSOC };

	SQLiteAccess();
	~SQLiteAccess();

	bool open(const String &path);
	bool open_in_memory();
	bool open_buffered(const String &name, const PackedByteArray &buffers, int64_t size);
	bool backup(const String &path);
	bool restore(const String &path);

	Ref<SQLiteBackup> backup_async(const String &path);
	Ref<SQLiteBackup> restore_async(const String &path);
	Ref<SQLiteBlob> open_blob(const String &p_db_name, const String &p_table_name, const String &p_column_name, int64_t p_rowid, bool p_read_write);

	bool close();

	Ref<SQLiteQuery> create_query(String p_query, Array p_args = Array());

	Ref<SQLiteQuery> create_savepoint(const String &p_name);
	Ref<SQLiteQuery> release_savepoint(const String &p_name);
	Ref<SQLiteQuery> rollback_to_savepoint(const String &p_name);

	bool create_function(const String &p_name, int p_argc, const Callable &p_callable);
	bool create_aggregate(const String &p_name, int p_argc, const Callable &p_step_callable, const Callable &p_final_callable);

	int64_t get_last_insert_rowid() const;
	int64_t get_changes() const;
	int64_t get_total_changes() const;
	bool set_busy_timeout(int p_ms);
	bool vacuum();

	void interrupt();
	void set_trace(bool p_enabled);
	bool enable_load_extension(bool p_enabled);
	bool execute_script(const String &p_path);

	String get_last_error_message() const;
	int get_last_error_code() const;

	bool is_autocommit() const;
	bool is_readonly(const String &p_db_name = "main") const;
	String get_database_filename(const String &p_db_name = "main") const;
	bool wal_checkpoint(const String &p_db_name = "main");

	bool create_collation(const String &p_name, const Callable &p_callable);
	void set_progress_handler(int p_instructions);
	void set_authorizer(const Callable &p_callable);

	int set_limit(int p_id, int p_new_val);
	int get_limit(int p_id) const;

	bool set_db_config(int p_op, int p_val);
	int get_db_config(int p_op) const;
	void set_foreign_keys_enabled(bool p_enabled);

	int get_db_status(int p_op, bool p_reset = false) const;
	static int get_global_status(int p_op, bool p_reset = false);

	static int release_memory(int p_bytes);
	static void set_soft_heap_limit(int64_t p_bytes);
};
