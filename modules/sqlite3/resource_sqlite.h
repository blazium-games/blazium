/**************************************************************************/
/*  resource_sqlite.h                                                     */
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

#include "core/io/resource.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"
#include "godot_sqlite.h"

class SQLiteDatabase : public Resource {
	GDCLASS(SQLiteDatabase, Resource);
	Ref<SQLiteAccess> db;

private:
	void _on_row_updated(int p_op, const String &p_db_name, const String &p_table_name, int64_t p_rowid);
	void _on_transaction_committed();
	void _on_transaction_rolled_back();
	void _on_query_progress();
	void _on_query_profiled(const String &p_query, int64_t p_execution_time_ns);
	void _on_wal_updated(const String &p_db_name, int p_pages);

protected:
	static void _bind_methods();

public:
	void set_resource(const String &p_path);
	void set_data(const PackedByteArray &p_data);
	Ref<SQLiteQuery> create_table(const String &p_table_name, const TypedArray<SQLiteColumnSchema> &p_columns);
	Ref<SQLiteQuery> drop_table(const String &p_table_name);
	Ref<SQLiteQuery> create_index(const String &p_index_name, const String &p_table_name, const TypedArray<String> &p_columns, bool p_unique = false);
	Ref<SQLiteQuery> drop_index(const String &p_index_name);

	Ref<SQLiteBackup> backup_async(const String &path);
	Ref<SQLiteBackup> restore_async(const String &path);

	Ref<SQLiteBlob> open_blob(const String &p_db_name, const String &p_table_name, const String &p_column_name, int64_t p_rowid, bool p_read_write = false);

	Ref<SQLiteQuery> begin_transaction();
	Ref<SQLiteQuery> commit_transaction();
	Ref<SQLiteQuery> rollback_transaction();

	Ref<SQLiteQuery> create_savepoint(const String &p_name);
	Ref<SQLiteQuery> release_savepoint(const String &p_name);
	Ref<SQLiteQuery> rollback_to_savepoint(const String &p_name);

	Ref<SQLiteQuery> insert_row(const String &p_name, const Dictionary &p_row_dict);
	Ref<SQLiteQuery> insert_rows(const String &p_name, const TypedArray<Dictionary> &p_row_array);

	Ref<SQLiteQuery> select_rows(const String &p_name, const String &p_conditions);
	Ref<SQLiteQuery> update_rows(const String &p_name, const String &p_conditions, const Dictionary &p_row_dict);
	Ref<SQLiteQuery> delete_rows(const String &p_name, const String &p_conditions);
	Dictionary get_tables() const;
	TypedArray<SQLiteColumnSchema> get_columns(const String &p_name) const;
	Ref<SQLiteQuery> create_query(const String &p_query_string, const Array &p_args = Array());
	Ref<SQLiteQueryResult> execute_query(const String &p_query_string, const Array &p_args = Array());
	String export_to_json(const String &p_table_name);
	bool import_from_json(const String &p_table_name, const String &p_json_string);
	bool attach_database(const String &p_path, const String &p_alias);
	bool detach_database(const String &p_alias);

	bool serialize_object(const String &p_table_name, const String &p_key, Object *p_object);
	bool deserialize_object(const String &p_table_name, const String &p_key, Object *p_object);
	Variant instantiate_object(const String &p_table_name, const String &p_key);

	bool backup_to(const String &p_path);
	bool restore_from(const String &p_path);
	bool load_from(const String &p_path);
	void close();

	String get_last_error_message() const;
	int get_last_error_code() const;
	int64_t get_last_insert_rowid() const;
	int64_t get_changes() const;
	int64_t get_total_changes() const;
	bool set_busy_timeout(int p_ms);
	bool vacuum();

	void interrupt();
	void set_trace(bool p_enabled);
	void set_authorizer(const Callable &p_callable);
	bool enable_load_extension(bool p_enabled);
	bool execute_script(const String &p_path);

	bool is_autocommit() const;
	bool is_readonly(const String &p_db_name = "main") const;
	String get_database_filename(const String &p_db_name = "main") const;
	bool wal_checkpoint(const String &p_db_name = "main");

	bool create_collation(const String &p_name, const Callable &p_callable);
	bool create_aggregate(const String &p_name, int p_argc, const Callable &p_step_callable, const Callable &p_final_callable);
	void set_progress_handler(int p_instructions);

	int set_limit(int p_id, int p_new_val);
	int get_limit(int p_id) const;

	bool set_db_config(int p_op, int p_val);
	int get_db_config(int p_op) const;
	void set_foreign_keys_enabled(bool p_enabled);

	int get_db_status(int p_op, bool p_reset = false) const;
	static int get_global_status(int p_op, bool p_reset = false);

	static int release_memory(int p_bytes);
	static void set_soft_heap_limit(int64_t p_bytes);

	bool create_function(const String &p_name, int p_argc, const Callable &p_callable);

	Ref<SQLiteAccess> get_sqlite();

	SQLiteDatabase();
	~SQLiteDatabase();
};
