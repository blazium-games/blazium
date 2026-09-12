/**************************************************************************/
/*  godot_sqlite.cpp                                                      */
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

#include "core/config/project_settings.h"
#include "core/core_bind.h"
#include "core/error/error_macros.h"
#include "core/variant/variant.h"
#include "thirdparty/sqlite/sqlite3.h"

#include "godot_sqlite.h"

Array fast_parse_row(sqlite3_stmt *stmt) {
	Array result;

	const int column_count = sqlite3_column_count(stmt);

	for (int i = 0; i < column_count; i++) {
		const int column_type = sqlite3_column_type(stmt, i);
		Variant value;
		switch (column_type) {
			case SQLITE_INTEGER:
				value = Variant(sqlite3_column_int(stmt, i));
				break;

			case SQLITE_FLOAT:
				value = Variant(sqlite3_column_double(stmt, i));
				break;

			case SQLITE_TEXT: {
				int size = sqlite3_column_bytes(stmt, i);
				String str =
						String::utf8((const char *)sqlite3_column_text(stmt, i), size);
				value = Variant(str);
				break;
			}
			case SQLITE_BLOB: {
				PackedByteArray arr;
				int size = sqlite3_column_bytes(stmt, i);
				arr.resize(size);
				memcpy(arr.ptrw(), sqlite3_column_blob(stmt, i), size);
				value = Variant(arr);
				break;
			}
			case SQLITE_NULL: {
			} break;
			default:
				ERR_PRINT("This kind of data is not yet supported: " + itos(column_type));
				break;
		}

		result.push_back(value);
	}

	return result;
}

SQLiteQuery::SQLiteQuery() {}

SQLiteQuery::~SQLiteQuery() {
	finalize();
}

void SQLiteQuery::init(SQLiteAccess *p_db, const String &p_query, Variant p_args) {
	db = p_db;
	query = p_query;
	arguments = p_args;
	stmt = nullptr;
}

bool SQLiteQuery::is_ready() const {
	return stmt != nullptr;
}

String SQLiteQuery::get_last_error_message() const {
	ERR_FAIL_COND_V(db == nullptr, "Database is undefined.");
	return db->get_last_error_message();
}

TypedArray<SQLiteColumnSchema> SQLiteQuery::get_columns() {
	ERR_FAIL_NULL_V(stmt, TypedArray<SQLiteColumnSchema>());

	if (is_ready() == false) {
		ERR_FAIL_COND_V(prepare() == false, Array());
	}

	TypedArray<SQLiteColumnSchema> res;
	const int col_count = sqlite3_column_count(stmt);
	res.resize(col_count);

	// Fetch all column
	for (int i = 0; i < col_count; i++) {
		Ref<SQLiteColumnSchema> schema;
		schema.instantiate();
		// Key name
		const char *col_name = sqlite3_column_name(stmt, i);
		schema->set_name(String(col_name));
		switch (sqlite3_column_type(stmt, i)) {
			case SQLITE_INTEGER:
				schema->set_type(Variant::Type::INT);
				break;
			case SQLITE_FLOAT:
				schema->set_type(Variant::Type::FLOAT);
				break;
			case SQLITE_TEXT:
				schema->set_type(Variant::Type::STRING);
				break;
			case SQLITE_BLOB:
				schema->set_type(Variant::Type::PACKED_BYTE_ARRAY);
				break;
			case SQLITE_NULL:
				schema->set_type(Variant::Type::NIL);
				break;
			default:
				ERR_PRINT("This kind of data is not yet supported: " + itos(sqlite3_column_type(stmt, i)));
				schema->set_type(Variant::Type::NIL);
				break;
		}
		res[i] = schema;
	}

	return res;
}

bool SQLiteQuery::prepare() {
	ERR_FAIL_COND_V(stmt != nullptr, SQLITE_ERROR);
	ERR_FAIL_COND_V(db == nullptr, SQLITE_ERROR);
	ERR_FAIL_COND_V(db->get_handler() == nullptr, SQLITE_ERROR);
	ERR_FAIL_COND_V(query == "", SQLITE_ERROR);
	// Prepare the statement
	int result = sqlite3_prepare_v2(db->get_handler(), query.utf8().ptr(), -1,
			&stmt, nullptr);

	// Cannot prepare query!
	ERR_FAIL_COND_V_MSG(result != SQLITE_OK, false,
			"SQL Error: " + db->get_last_error_message());

	return true;
}

void SQLiteQuery::finalize() {
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = nullptr;
	}
}

void SQLiteQuery::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_last_error_message"), &SQLiteQuery::get_last_error_message);
	ClassDB::bind_method(D_METHOD("execute", "arguments"), &SQLiteQuery::execute, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("batch_execute", "rows"), &SQLiteQuery::batch_execute);
	ClassDB::bind_method(D_METHOD("get_columns"), &SQLiteQuery::get_columns);
	ClassDB::bind_method(D_METHOD("get_query"), &SQLiteQuery::get_query);
	ClassDB::bind_method(D_METHOD("get_arguments"), &SQLiteQuery::get_arguments);
	ClassDB::bind_method(D_METHOD("set_arguments", "arguments"), &SQLiteQuery::set_arguments);
	ClassDB::bind_method(D_METHOD("is_ready"), &SQLiteQuery::is_ready);
	ClassDB::bind_method(D_METHOD("finalize_query"), &SQLiteQuery::finalize);
	ClassDB::bind_method(D_METHOD("get_stmt_status", "op", "reset"), &SQLiteQuery::get_stmt_status, DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "arguments"), "set_arguments", "get_arguments");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "query"), "", "get_query");
}

void SQLiteBackup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("step", "pages"), &SQLiteBackup::step, DEFVAL(100));
	ClassDB::bind_method(D_METHOD("get_remaining"), &SQLiteBackup::get_remaining);
	ClassDB::bind_method(D_METHOD("get_page_count"), &SQLiteBackup::get_page_count);
	ClassDB::bind_method(D_METHOD("finish"), &SQLiteBackup::finish);
}

SQLiteBackup::~SQLiteBackup() {
	finish();
}

int SQLiteBackup::step(int pages) {
	if (backup) {
		return sqlite3_backup_step(backup, pages);
	}
	return -1; // -1 loosely evaluates misaligned pointers natively.
}

int SQLiteBackup::get_remaining() const {
	if (backup) {
		return sqlite3_backup_remaining(backup);
	}
	return -1;
}

int SQLiteBackup::get_page_count() const {
	if (backup) {
		return sqlite3_backup_pagecount(backup);
	}
	return -1;
}

void SQLiteBackup::finish() {
	if (backup) {
		sqlite3_backup_finish(backup);
		backup = nullptr;
	}
	if (side_db) {
		sqlite3_close_v2(side_db);
		side_db = nullptr;
	}
}

void SQLiteBlob::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_size"), &SQLiteBlob::get_size);
	ClassDB::bind_method(D_METHOD("read_chunk", "offset", "length"), &SQLiteBlob::read_chunk);
	ClassDB::bind_method(D_METHOD("write_chunk", "offset", "buffer"), &SQLiteBlob::write_chunk);
	ClassDB::bind_method(D_METHOD("close"), &SQLiteBlob::close);
}

SQLiteBlob::~SQLiteBlob() {
	close();
}

PackedByteArray SQLiteBlob::read_chunk(int p_offset, int p_length) {
	PackedByteArray output;
	if (blob && p_offset >= 0 && p_length > 0 && p_offset + p_length <= size) {
		output.resize(p_length);
		int res = sqlite3_blob_read(blob, output.ptrw(), p_length, p_offset);
		if (res != SQLITE_OK) {
			output.clear();
			ERR_PRINT("SQLite BLOB Read failed with error code: " + itos(res));
		}
	} else if (blob == nullptr) {
		ERR_PRINT("SQLite BLOB Read failed: BLOB pointer is void.");
	} else {
		ERR_PRINT("SQLite BLOB Read failed: Bounds exceeded!");
	}
	return output;
}

bool SQLiteBlob::write_chunk(int p_offset, const PackedByteArray &p_buffer) {
	if (blob && p_offset >= 0 && p_buffer.size() > 0 && p_offset + p_buffer.size() <= size) {
		int res = sqlite3_blob_write(blob, p_buffer.ptr(), p_buffer.size(), p_offset);
		if (res != SQLITE_OK) {
			ERR_PRINT("SQLite BLOB Write failed with error code: " + itos(res));
			return false;
		}
		return true;
	}
	return false;
}

void SQLiteBlob::close() {
	if (blob) {
		sqlite3_blob_close(blob);
		blob = nullptr;
		size = 0;
	}
}

SQLiteAccess::SQLiteAccess() {
}

bool SQLiteAccess::open_in_memory() {
	if (sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
		return false;
	}
	_configure_hooks();
	return true;
}

bool SQLiteAccess::close() {
	// Finalize all queries before close the DB.
	// Reverse order because I need to remove the not available queries.
	for (uint32_t i = queries.size(); i > 0; i -= 1) {
		SQLiteQuery *query =
				Object::cast_to<SQLiteQuery>(queries[i - 1]->get_ref());
		if (query != nullptr) {
			query->finalize();
		} else {
			queries.remove_at(i - 1);
		}
	}

	if (db) {
		// Cannot close database!
		int result = sqlite3_close_v2(db);
		db = nullptr;
		return result;
	}

	if (memory_read) {
		// Close virtual filesystem database
		int result = spmemvfs_close_db(&spmemvfs_db);
		spmemvfs_env_fini();
		memory_read = false;
		return result;
	}

	for (KeyValue<String, SQLiteCallableContext *> &E : custom_functions) {
		memdelete(E.value);
	}
	custom_functions.clear();
	for (KeyValue<String, SQLiteCallableContext *> &E : custom_collations) {
		memdelete(E.value);
	}
	custom_collations.clear();
	for (KeyValue<String, SQLiteAggregateContext *> &E : custom_aggregates) {
		memdelete(E.value);
	}
	custom_aggregates.clear();

	return true;
}

sqlite3_stmt *SQLiteAccess::prepare(const char *query) {
	// Get database pointer
	sqlite3 *dbs = get_handler();

	ERR_FAIL_COND_V_MSG(dbs == nullptr, nullptr,
			"Cannot prepare query. The database was not opened.");

	// Prepare the statement
	sqlite3_stmt *stmt = nullptr;
	sqlite3_prepare_v2(dbs, query, -1, &stmt, nullptr);
	return stmt;
}

Dictionary parse_row(sqlite3_stmt *stmt) {
	Dictionary result;

	// Get column count
	int col_count = sqlite3_column_count(stmt);

	// Fetch all column
	for (int i = 0; i < col_count; i++) {
		// Key name
		const char *col_name = sqlite3_column_name(stmt, i);
		String key = String(col_name);

		// Value
		int col_type = sqlite3_column_type(stmt, i);
		Variant value;

		// Get column value
		switch (col_type) {
			case SQLITE_INTEGER:
				value = Variant(sqlite3_column_int(stmt, i));
				break;

			case SQLITE_FLOAT:
				value = Variant(sqlite3_column_double(stmt, i));
				break;

			case SQLITE_TEXT: {
				int size = sqlite3_column_bytes(stmt, i);
				String str =
						String::utf8((const char *)sqlite3_column_text(stmt, i), size);
				value = Variant(str);
				break;
			}
			case SQLITE_BLOB: {
				PackedByteArray arr;
				int size = sqlite3_column_bytes(stmt, i);
				arr.resize(size);
				memcpy((void *)arr.ptr(), sqlite3_column_blob(stmt, i), size);
				value = Variant(arr);
				break;
			}

			default:
				break;
		}
		result[key] = value;
	}

	return result;
}

String SQLiteAccess::get_last_error_message() const {
	return sqlite3_errmsg(get_handler());
}

int SQLiteAccess::get_last_error_code() const {
	return sqlite3_errcode(get_handler());
}

SQLiteAccess::~SQLiteAccess() {
	close();
	for (uint32_t i = 0; i < queries.size(); i += 1) {
		SQLiteQuery *query = Object::cast_to<SQLiteQuery>(queries[i]->get_ref());
		if (query != nullptr) {
			query->init(nullptr, "", Array());
		}
	}
	for (const KeyValue<String, SQLiteCallableContext *> &E : custom_functions) {
		memdelete(E.value);
	}
	custom_functions.clear();
	for (const KeyValue<String, SQLiteCallableContext *> &E : custom_collations) {
		memdelete(E.value);
	}
	custom_collations.clear();
}

void SQLiteAccess::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "database"), &SQLiteAccess::open);
	ClassDB::bind_method(D_METHOD("open_in_memory"), &SQLiteAccess::open_in_memory);
	ClassDB::bind_method(D_METHOD("open_buffered", "path", "buffers", "size"), &SQLiteAccess::open_buffered);
	ClassDB::bind_method(D_METHOD("backup", "path"), &SQLiteAccess::backup);
	ClassDB::bind_method(D_METHOD("restore", "path"), &SQLiteAccess::restore);
	ClassDB::bind_method(D_METHOD("backup_async", "path"), &SQLiteAccess::backup_async);
	ClassDB::bind_method(D_METHOD("restore_async", "path"), &SQLiteAccess::restore_async);
	ClassDB::bind_method(D_METHOD("open_blob", "db_name", "table_name", "column_name", "rowid", "read_write"), &SQLiteAccess::open_blob, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_last_error_message"), &SQLiteAccess::get_last_error_message);
	ClassDB::bind_method(D_METHOD("get_last_error_code"), &SQLiteAccess::get_last_error_code);
	ClassDB::bind_method(D_METHOD("get_last_insert_rowid"), &SQLiteAccess::get_last_insert_rowid);
	ClassDB::bind_method(D_METHOD("get_changes"), &SQLiteAccess::get_changes);
	ClassDB::bind_method(D_METHOD("get_total_changes"), &SQLiteAccess::get_total_changes);
	ClassDB::bind_method(D_METHOD("set_busy_timeout", "ms"), &SQLiteAccess::set_busy_timeout);
	ClassDB::bind_method(D_METHOD("interrupt"), &SQLiteAccess::interrupt);
	ClassDB::bind_method(D_METHOD("set_trace", "enabled"), &SQLiteAccess::set_trace);
	ClassDB::bind_method(D_METHOD("enable_load_extension", "enabled"), &SQLiteAccess::enable_load_extension);
	ClassDB::bind_method(D_METHOD("execute_script", "path"), &SQLiteAccess::execute_script);
	ClassDB::bind_method(D_METHOD("vacuum"), &SQLiteAccess::vacuum);

	ClassDB::bind_method(D_METHOD("is_autocommit"), &SQLiteAccess::is_autocommit);
	ClassDB::bind_method(D_METHOD("is_readonly", "db_name"), &SQLiteAccess::is_readonly, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("get_database_filename", "db_name"), &SQLiteAccess::get_database_filename, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("wal_checkpoint", "db_name"), &SQLiteAccess::wal_checkpoint, DEFVAL("main"));

	ClassDB::bind_method(D_METHOD("create_function", "name", "argc", "callable"), &SQLiteAccess::create_function);
	ClassDB::bind_method(D_METHOD("create_collation", "name", "callable"), &SQLiteAccess::create_collation);
	ClassDB::bind_method(D_METHOD("set_progress_handler", "instructions"), &SQLiteAccess::set_progress_handler);
	ClassDB::bind_method(D_METHOD("set_authorizer", "callable"), &SQLiteAccess::set_authorizer);

	ClassDB::bind_method(D_METHOD("set_limit", "id", "new_val"), &SQLiteAccess::set_limit);
	ClassDB::bind_method(D_METHOD("get_limit", "id"), &SQLiteAccess::get_limit);

	ClassDB::bind_method(D_METHOD("set_db_config", "op", "val"), &SQLiteAccess::set_db_config);
	ClassDB::bind_method(D_METHOD("get_db_config", "op"), &SQLiteAccess::get_db_config);
	ClassDB::bind_method(D_METHOD("set_foreign_keys_enabled", "enabled"), &SQLiteAccess::set_foreign_keys_enabled);

	ClassDB::bind_method(D_METHOD("get_db_status", "op", "reset"), &SQLiteAccess::get_db_status, DEFVAL(false));
	ClassDB::bind_static_method("SQLiteAccess", D_METHOD("get_global_status", "op", "reset"), &SQLiteAccess::get_global_status);

	ClassDB::bind_static_method("SQLiteAccess", D_METHOD("release_memory", "bytes"), &SQLiteAccess::release_memory);
	ClassDB::bind_static_method("SQLiteAccess", D_METHOD("set_soft_heap_limit", "bytes"), &SQLiteAccess::set_soft_heap_limit);

	ClassDB::bind_method(D_METHOD("close"), &SQLiteAccess::close);
	ClassDB::bind_method(D_METHOD("create_query", "statement", "arguments"), &SQLiteAccess::create_query, DEFVAL(Array()));

	ADD_SIGNAL(MethodInfo("row_updated", PropertyInfo(Variant::INT, "update_operation"), PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::STRING, "table_name"), PropertyInfo(Variant::INT, "rowid")));
	ADD_SIGNAL(MethodInfo("transaction_committed"));
	ADD_SIGNAL(MethodInfo("transaction_rolled_back"));
	ADD_SIGNAL(MethodInfo("query_progress"));
	ADD_SIGNAL(MethodInfo("query_profiled", PropertyInfo(Variant::STRING, "query_string"), PropertyInfo(Variant::INT, "execution_time_ns")));
	ADD_SIGNAL(MethodInfo("wal_updated", PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::INT, "pages_in_wal")));
}

bool SQLiteAccess::open(const String &path) {
	if (!path.strip_edges().length()) {
		print_error("Path is wrong!");
		return false;
	}
	Engine *engine_singleton = Engine::get_singleton();
	if (!engine_singleton) {
		print_error("Cannot get engine singleton!");
		return false;
	}
	if (!engine_singleton->is_editor_hint() && path.begins_with("res://")) {
		Ref<FileAccess> dbfile = FileAccess::open(path, FileAccess::READ);
		if (dbfile.is_null()) {
			print_error("Cannot open packed database!");
			return false;
		}
		int64_t size = dbfile->get_length();
		PackedByteArray buffer;
		buffer.resize(size);
		buffer.fill(0);
		dbfile->get_buffer(buffer.ptrw(), size);
		return open_buffered(path, buffer, size);
	}
	ProjectSettings *project_settings_singleton = ProjectSettings::get_singleton();
	if (!project_settings_singleton) {
		print_error("Cannot get project settings!");
		return SQLITE_ERROR;
	}
	String real_path = project_settings_singleton->globalize_path(path.strip_edges());

	if (sqlite3_open(real_path.utf8().get_data(), &db) == SQLITE_OK) {
		_configure_hooks();
		return true;
	}
	return false;
}

String SQLiteAccess::bind_args(sqlite3_stmt *stmt, const Array &args) {
	int param_count = sqlite3_bind_parameter_count(stmt);
	if (param_count != args.size()) {
		return "SQLiteQuery failed; expected " + itos(param_count) + " arguments, got " + itos(args.size());
	}

	/**
	 * SQLite data types:
	 * - NULL
	 * - INTEGER (signed, max 8 bytes)
	 * - REAL (stored as a double-precision float)
	 * - TEXT (stored in database encoding of UTF-8, UTF-16BE or UTF-16LE)
	 * - BLOB (1:1 storage)
	 */

	for (int i = 0; i < param_count; i++) {
		int retcode;
		switch (args[i].get_type()) {
			case Variant::Type::NIL:
				retcode = sqlite3_bind_null(stmt, i + 1);
				break;
			case Variant::Type::BOOL:
			case Variant::Type::INT:
				retcode = sqlite3_bind_int(stmt, i + 1, (int)args[i]);
				break;
			case Variant::Type::FLOAT:
				retcode = sqlite3_bind_double(stmt, i + 1, (double)args[i]);
				break;
			case Variant::Type::STRING:
				retcode = sqlite3_bind_text(
						stmt, i + 1, String(args[i]).utf8().get_data(), -1, SQLITE_TRANSIENT);
				break;
			case Variant::Type::PACKED_BYTE_ARRAY:
				retcode =
						sqlite3_bind_blob(stmt, i + 1, PackedByteArray(args[i]).ptr(),
								PackedByteArray(args[i]).size(), SQLITE_TRANSIENT);
				break;
			default:
				return "SQLite was passed unhandled Variant with TYPE_* enum " +
						itos(args[i].get_type()) +
						". Please serialize your object into a String or a PoolByteArray.";
		}

		if (retcode != SQLITE_OK) {
			return "SQLiteQuery failed, an error occurred while binding argument" +
					itos(i + 1) + " of " + itos(args.size()) + " (SQLite errcode " +
					itos(retcode) + ")";
		}
	}

	return "";
}

String SQLiteAccess::bind_args_dict(sqlite3_stmt *stmt, const Dictionary &args) {
	int param_count = sqlite3_bind_parameter_count(stmt);
	if (param_count != args.size()) {
		return "SQLiteQuery failed, parameter count mismatch. Query requires " +
				itos(param_count) + " arguments, but " + itos(args.size()) + " were mapped.";
	}

	Array keys = args.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Variant val = args[key];

		int bind_idx = sqlite3_bind_parameter_index(stmt, key.utf8().get_data());
		if (bind_idx == 0) {
			return "SQLiteQuery failed, could not find named parameter " + key + " in query.";
		}

		int retcode;
		switch (val.get_type()) {
			case Variant::Type::NIL:
				retcode = sqlite3_bind_null(stmt, bind_idx);
				break;
			case Variant::Type::BOOL:
			case Variant::Type::INT:
				retcode = sqlite3_bind_int(stmt, bind_idx, (int)val);
				break;
			case Variant::Type::FLOAT:
				retcode = sqlite3_bind_double(stmt, bind_idx, (double)val);
				break;
			case Variant::Type::STRING:
				retcode = sqlite3_bind_text(
						stmt, bind_idx, String(val).utf8().get_data(), -1, SQLITE_TRANSIENT);
				break;
			case Variant::Type::PACKED_BYTE_ARRAY:
				retcode = sqlite3_bind_blob(stmt, bind_idx, PackedByteArray(val).ptr(), PackedByteArray(val).size(), SQLITE_TRANSIENT);
				break;
			default:
				return "SQLite was passed unhandled Variant for param " + key + ". Please serialize your object into a String or a PoolByteArray.";
		}

		if (retcode != SQLITE_OK) {
			return "SQLiteQuery failed, an error occurred while binding argument " + key;
		}
	}
	return "";
}

bool SQLiteAccess::open_buffered(const String &name, const PackedByteArray &buffers, int64_t size) {
	if (!name.strip_edges().length()) {
		return false;
	}

	if (!buffers.size() || !size) {
		return false;
	}

	spmembuffer_t *p_mem = (spmembuffer_t *)calloc(1, sizeof(spmembuffer_t));
	p_mem->total = p_mem->used = size;
	p_mem->data = (char *)malloc(size + 1);
	memcpy(p_mem->data, buffers.ptr(), size);
	p_mem->data[size] = '\0';

	spmemvfs_env_init();
	int err = spmemvfs_open_db(&spmemvfs_db, name.utf8().get_data(), p_mem);

	if (err != SQLITE_OK || spmemvfs_db.mem != p_mem) {
		print_error("Cannot open buffered database!");
		return false;
	}

	memory_read = true;
	_configure_hooks();
	return true;
}

bool SQLiteAccess::backup(const String &path) {
	String destination_path = ProjectSettings::get_singleton()->globalize_path(path.strip_edges());
	sqlite3 *destination_db;
	int result = sqlite3_open_v2(destination_path.utf8().get_data(), &destination_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, NULL);

	if (result != SQLITE_OK) {
		ERR_PRINT("Cannot create backup database, error:" + itos(result));
		sqlite3_close_v2(destination_db);
		return false;
	}

	// Get database pointer
	sqlite3 *dbs = get_handler();

	if (dbs == nullptr) {
		ERR_PRINT("Cannot backup. The database was not opened.");
		sqlite3_close_v2(destination_db);
		return false;
	}

	sqlite3_backup *backup = sqlite3_backup_init(destination_db, "main", dbs, "main");
	if (backup) {
		sqlite3_backup_step(backup, -1);
		sqlite3_backup_finish(backup);
	}

	sqlite3_close_v2(destination_db);
	return true;
}

bool SQLiteAccess::restore(const String &path) {
	String source_path = ProjectSettings::get_singleton()->globalize_path(path.strip_edges());
	sqlite3 *source_db;
	int result = sqlite3_open_v2(source_path.utf8().get_data(), &source_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL);

	if (result != SQLITE_OK) {
		ERR_PRINT("Cannot open source database for restore, error:" + itos(result));
		sqlite3_close_v2(source_db);
		return false;
	}

	sqlite3 *dbs = get_handler();
	if (dbs == nullptr) {
		ERR_PRINT("Cannot restore. The active database was not opened.");
		sqlite3_close_v2(source_db);
		return false;
	}

	sqlite3_backup *backup = sqlite3_backup_init(dbs, "main", source_db, "main");
	if (backup) {
		sqlite3_backup_step(backup, -1);
		sqlite3_backup_finish(backup);
	} else {
		ERR_PRINT("Failed to initialize sqlite3_backup for restore.");
		sqlite3_close_v2(source_db);
		return false;
	}

	sqlite3_close_v2(source_db);
	return true;
}

Ref<SQLiteBackup> SQLiteAccess::backup_async(const String &path) {
	Ref<SQLiteBackup> backup_obj;
	backup_obj.instantiate();

	sqlite3 *dbs = get_handler();
	if (dbs == nullptr) {
		ERR_PRINT("Cannot backup. The active database was not opened.");
		return backup_obj;
	}

	String destination_path = ProjectSettings::get_singleton()->globalize_path(path.strip_edges());
	sqlite3 *destination_db;

	int result = sqlite3_open(destination_path.utf8().get_data(), &destination_db);
	if (result != SQLITE_OK) {
		ERR_PRINT("Cannot open asynchronous backup destination db, error:" + itos(result));
		sqlite3_close_v2(destination_db);
		return backup_obj;
	}

	sqlite3_backup *b = sqlite3_backup_init(destination_db, "main", dbs, "main");
	if (b) {
		backup_obj->init(b, destination_db);
	} else {
		ERR_PRINT("Failed to initialize asynchronous sqlite3_backup!");
		sqlite3_close_v2(destination_db);
	}
	return backup_obj;
}

Ref<SQLiteBackup> SQLiteAccess::restore_async(const String &path) {
	Ref<SQLiteBackup> restore_obj;
	restore_obj.instantiate();

	sqlite3 *dbs = get_handler();
	if (dbs == nullptr) {
		ERR_PRINT("Cannot restore. The active database was not opened.");
		return restore_obj;
	}

	String source_path = ProjectSettings::get_singleton()->globalize_path(path.strip_edges());
	sqlite3 *source_db;

	int result = sqlite3_open_v2(source_path.utf8().get_data(), &source_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL);
	if (result != SQLITE_OK) {
		ERR_PRINT("Cannot open source database for restore, error:" + itos(result));
		sqlite3_close_v2(source_db);
		return restore_obj;
	}

	sqlite3_backup *b = sqlite3_backup_init(dbs, "main", source_db, "main");
	if (b) {
		restore_obj->init(b, source_db);
	} else {
		ERR_PRINT("Failed to initialize asynchronous sqlite3_restore!");
		sqlite3_close_v2(source_db);
	}
	return restore_obj;
}

Ref<SQLiteBlob> SQLiteAccess::open_blob(const String &p_db_name, const String &p_table_name, const String &p_column_name, int64_t p_rowid, bool p_read_write) {
	Ref<SQLiteBlob> blob_obj;
	blob_obj.instantiate();

	sqlite3 *dbs = get_handler();
	if (dbs == nullptr) {
		ERR_PRINT("Cannot open BLOB. The active database was not dynamically opened.");
		return blob_obj;
	}

	sqlite3_blob *b = nullptr;
	int flags = p_read_write ? 1 : 0;

	int result = sqlite3_blob_open(dbs, p_db_name.utf8().get_data(), p_table_name.utf8().get_data(), p_column_name.utf8().get_data(), p_rowid, flags, &b);

	if (result == SQLITE_OK && b) {
		blob_obj->init(b);
	} else {
		ERR_PRINT("SQLite failed to initialize BLOB incremental stream, error: " + itos(result));
	}

	return blob_obj;
}

Ref<SQLiteQueryResult> SQLiteQuery::execute(const Variant p_args) {
	Ref<SQLiteQueryResult> result;
	result.instantiate();
	result->set_query(query);

	Variant args = p_args;
	if (args.get_type() == Variant::NIL || (args.get_type() == Variant::ARRAY && Array(args).is_empty())) {
		args = arguments;
	}
	result->set_arguments(args);

	ERR_FAIL_COND_V_MSG(db == nullptr, result, "SQLiteQuery database is null!");
	ERR_FAIL_COND_V_MSG(db->get_handler() == nullptr, result, "SQLiteQuery database handler is null!");

	if (!is_ready()) {
		if (!prepare()) {
			result->set_error_code(db->get_last_error_code());
			result->set_error(db->get_last_error_message());
			return result;
		}
	} else {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}

	String bind_err_msg = "";
	if (args.get_type() == Variant::ARRAY) {
		bind_err_msg = SQLiteAccess::bind_args(stmt, args);
	} else if (args.get_type() == Variant::DICTIONARY) {
		bind_err_msg = SQLiteAccess::bind_args_dict(stmt, args);
	} else {
		bind_err_msg = "Arguments must be Array or Dictionary.";
	}

	if (bind_err_msg != "") {
		result->set_error_code(SQLITE_MISMATCH);
		result->set_error(bind_err_msg);
		return result;
	}

	TypedArray<Dictionary> results;
	while (true) {
		const int res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			results.append(parse_row(stmt));
		} else if (res == SQLITE_DONE) {
			break;
		} else {
			result->set_error_code(res);
			result->set_error(get_last_error_message());
			break; // Stop parsing rows but don't crash engine natively
		}
	}
	result->set_result(results);

	if (SQLITE_OK != sqlite3_reset(stmt)) {
		finalize();
		result->set_error_code(db->get_last_error_code());
		result->set_error(get_last_error_message());
	}
	return result;
}

TypedArray<SQLiteQueryResult> SQLiteQuery::batch_execute(Array p_rows) {
	TypedArray<SQLiteQueryResult> res;
	Array rows = p_rows;
	if (rows.is_empty()) {
		if (arguments.get_type() == Variant::ARRAY) {
			Array arr = arguments;
			for (int i = 0; i < arr.size(); i++) {
				rows.push_back(arr[i]);
			}
		}
	}
	for (int i = 0; i < rows.size(); i += 1) {
		Ref<SQLiteQueryResult> r = execute(rows[i]);
		res.push_back(r);
	}
	return res;
}

Ref<SQLiteQuery> SQLiteAccess::create_query(String p_query, Array p_args) {
	Ref<SQLiteQuery> query;
	query.instantiate();
	query->init(this, p_query, p_args);

	Ref<WeakRef> wr;
	wr.instantiate();
	wr->set_obj(query.ptr());
	queries.push_back(wr);

	return query;
}

Ref<SQLiteQuery> SQLiteAccess::create_savepoint(const String &p_name) {
	return create_query("SAVEPOINT " + p_name + ";");
}

Ref<SQLiteQuery> SQLiteAccess::release_savepoint(const String &p_name) {
	return create_query("RELEASE " + p_name + ";");
}

Ref<SQLiteQuery> SQLiteAccess::rollback_to_savepoint(const String &p_name) {
	return create_query("ROLLBACK TO " + p_name + ";");
}

int64_t SQLiteAccess::get_last_insert_rowid() const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_last_insert_rowid(dbs);
	}
	return 0;
}

int64_t SQLiteAccess::get_changes() const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_changes(dbs);
	}
	return 0;
}

int64_t SQLiteAccess::get_total_changes() const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_total_changes(dbs);
	}
	return 0;
}

bool SQLiteAccess::set_busy_timeout(int p_ms) {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		ERR_PRINT("Cannot set busy timeout. The database was not opened.");
		return false;
	}
	return sqlite3_busy_timeout(dbs, p_ms) == SQLITE_OK;
}

bool SQLiteAccess::vacuum() {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		ERR_PRINT("Cannot vacuum. The database was not opened.");
		return false;
	}
	int result = sqlite3_exec(dbs, "VACUUM;", nullptr, nullptr, nullptr);
	if (result != SQLITE_OK) {
		ERR_PRINT("Vacuum failed, error: " + String(sqlite3_errmsg(dbs)));
		return false;
	}
	return true;
}

void SQLiteAccess::_sqlite_udf_static_trampoline(sqlite3_context *context, int argc, sqlite3_value **argv) {
	SQLiteCallableContext *data = (SQLiteCallableContext *)sqlite3_user_data(context);
	if (!data) {
		return;
	}

	Array g_args;
	for (int i = 0; i < argc; i++) {
		int col_type = sqlite3_value_type(argv[i]);
		Variant value;
		switch (col_type) {
			case SQLITE_INTEGER:
				value = Variant((int64_t)sqlite3_value_int64(argv[i]));
				break;
			case SQLITE_FLOAT:
				value = Variant(sqlite3_value_double(argv[i]));
				break;
			case SQLITE_TEXT: {
				int size = sqlite3_value_bytes(argv[i]);
				String str = String::utf8((const char *)sqlite3_value_text(argv[i]), size);
				value = Variant(str);
				break;
			}
			case SQLITE_BLOB: {
				PackedByteArray arr;
				int size = sqlite3_value_bytes(argv[i]);
				arr.resize(size);
				memcpy((void *)arr.ptr(), sqlite3_value_blob(argv[i]), size);
				value = Variant(arr);
				break;
			}
			default:
				value = Variant(); // NULL
		}
		g_args.push_back(value);
	}

	Callable::CallError err;
	Variant result;
	if (data->func.is_valid()) {
		const Variant **argptrs = nullptr;
		if (g_args.size() > 0) {
			argptrs = (const Variant **)alloca(sizeof(Variant *) * g_args.size());
			for (int i = 0; i < g_args.size(); i++) {
				argptrs[i] = &g_args[i];
			}
		}
		data->func.callp(argptrs, g_args.size(), result, err);
	}

	if (result.get_type() == Variant::Type::INT) {
		sqlite3_result_int64(context, result.operator int64_t());
	} else if (result.get_type() == Variant::Type::FLOAT) {
		sqlite3_result_double(context, result.operator double());
	} else if (result.get_type() == Variant::Type::STRING) {
		String s = result.operator String();
		sqlite3_result_text(context, s.utf8().get_data(), -1, SQLITE_TRANSIENT);
	} else if (result.get_type() == Variant::Type::PACKED_BYTE_ARRAY) {
		PackedByteArray arr = result.operator PackedByteArray();
		sqlite3_result_blob(context, arr.ptr(), arr.size(), SQLITE_TRANSIENT);
	} else {
		sqlite3_result_null(context);
	}
}

void SQLiteAccess::_sqlite_udf_step_trampoline(sqlite3_context *context, int argc, sqlite3_value **argv) {
	SQLiteAggregateContext *ctx = (SQLiteAggregateContext *)sqlite3_user_data(context);
	if (!ctx || !ctx->step.is_valid()) {
		return;
	}

	Variant **variant_ctx = (Variant **)sqlite3_aggregate_context(context, sizeof(Variant *));
	if (!variant_ctx) {
		sqlite3_result_error_nomem(context);
		return;
	}

	if (!*variant_ctx) {
		*variant_ctx = memnew(Variant);
	}

	Vector<Variant> g_args;
	g_args.push_back(**variant_ctx);

	for (int i = 0; i < argc; i++) {
		Variant value;
		int type = sqlite3_value_type(argv[i]);
		switch (type) {
			case SQLITE_INTEGER:
				value = Variant((int64_t)sqlite3_value_int64(argv[i]));
				break;
			case SQLITE_FLOAT:
				value = Variant(sqlite3_value_double(argv[i]));
				break;
			case SQLITE_TEXT: {
				int size = sqlite3_value_bytes(argv[i]);
				String str = String::utf8((const char *)sqlite3_value_text(argv[i]), size);
				value = Variant(str);
				break;
			}
			case SQLITE_BLOB: {
				PackedByteArray arr;
				int size = sqlite3_value_bytes(argv[i]);
				arr.resize(size);
				memcpy((void *)arr.ptr(), sqlite3_value_blob(argv[i]), size);
				value = Variant(arr);
				break;
			}
			default:
				value = Variant(); // NULL
		}
		g_args.push_back(value);
	}

	Callable::CallError err;
	Variant result;
	const Variant **argptrs = nullptr;
	if (g_args.size() > 0) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * g_args.size());
		for (int i = 0; i < g_args.size(); i++) {
			argptrs[i] = &g_args[i];
		}
	}
	ctx->step.callp(argptrs, g_args.size(), result, err);

	if (err.error == Callable::CallError::CALL_OK) {
		**variant_ctx = result;
	}
}

void SQLiteAccess::_sqlite_udf_final_trampoline(sqlite3_context *context) {
	SQLiteAggregateContext *ctx = (SQLiteAggregateContext *)sqlite3_user_data(context);
	if (!ctx || !ctx->final.is_valid()) {
		return;
	}

	Variant **variant_ctx = (Variant **)sqlite3_aggregate_context(context, 0);

	Variant accum;
	if (variant_ctx && *variant_ctx) {
		accum = **variant_ctx;
		memdelete(*variant_ctx);
		*variant_ctx = nullptr;
	}

	const Variant *arg_ptr = &accum;

	Variant result;
	Callable::CallError err;
	ctx->final.callp(&arg_ptr, 1, result, err);

	if (err.error != Callable::CallError::CALL_OK) {
		sqlite3_result_error(context, "Godot SQLite UDF aggregate final execution failed", -1);
	} else {
		if (result.get_type() == Variant::Type::INT) {
			sqlite3_result_int64(context, result.operator int64_t());
		} else if (result.get_type() == Variant::Type::FLOAT) {
			sqlite3_result_double(context, result.operator double());
		} else if (result.get_type() == Variant::Type::STRING) {
			String s = result.operator String();
			sqlite3_result_text(context, s.utf8().get_data(), -1, SQLITE_TRANSIENT);
		} else if (result.get_type() == Variant::Type::PACKED_BYTE_ARRAY) {
			PackedByteArray arr = result.operator PackedByteArray();
			sqlite3_result_blob(context, arr.ptr(), arr.size(), SQLITE_TRANSIENT);
		} else {
			sqlite3_result_null(context);
		}
	}
}

bool SQLiteAccess::create_function(const String &p_name, int p_argc, const Callable &p_callable) {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		ERR_PRINT("Cannot create function. Database is not opened.");
		return false;
	}
	if (!p_callable.is_valid()) {
		ERR_PRINT("Cannot create function with invalid Callable.");
		return false;
	}

	SQLiteCallableContext *ctx = nullptr;
	if (custom_functions.has(p_name)) {
		ctx = custom_functions[p_name];
	} else {
		ctx = memnew(SQLiteCallableContext);
		custom_functions.insert(p_name, ctx);
	}
	ctx->func = p_callable;

	int err = sqlite3_create_function(
			dbs,
			p_name.utf8().get_data(),
			p_argc,
			SQLITE_UTF8 | SQLITE_DETERMINISTIC,
			ctx,
			SQLiteAccess::_sqlite_udf_static_trampoline,
			nullptr,
			nullptr);

	if (err != SQLITE_OK) {
		ERR_PRINT("Failed to create function in SQLite engine.");
		return false;
	}
	return true;
}

bool SQLiteAccess::create_aggregate(const String &p_name, int p_argc, const Callable &p_step_callable, const Callable &p_final_callable) {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		ERR_PRINT("Cannot create aggregate. Database is not opened.");
		return false;
	}
	if (!p_step_callable.is_valid() || !p_final_callable.is_valid()) {
		ERR_PRINT("Cannot create aggregate with invalid Callables.");
		return false;
	}

	SQLiteAggregateContext *ctx = nullptr;
	if (custom_aggregates.has(p_name)) {
		ctx = custom_aggregates[p_name];
	} else {
		ctx = memnew(SQLiteAggregateContext);
		custom_aggregates.insert(p_name, ctx);
	}
	ctx->step = p_step_callable;
	ctx->final = p_final_callable;

	int err = sqlite3_create_function(
			dbs,
			p_name.utf8().get_data(),
			p_argc,
			SQLITE_UTF8 | SQLITE_DETERMINISTIC,
			ctx,
			nullptr,
			SQLiteAccess::_sqlite_udf_step_trampoline,
			SQLiteAccess::_sqlite_udf_final_trampoline);

	if (err != SQLITE_OK) {
		ERR_PRINT("Failed to create aggregate in SQLite engine.");
		return false;
	}
	return true;
}

int SQLiteAccess::_sqlite_trace_callback(unsigned int mask, void *ctx, void *p, void *x) {
	if (mask == SQLITE_TRACE_PROFILE) {
		SQLiteAccess *instance = (SQLiteAccess *)ctx;
		sqlite3_stmt *stmt = (sqlite3_stmt *)p;
		sqlite3_int64 execution_time_ns = *(sqlite3_int64 *)x;

		char *sql = sqlite3_expanded_sql(stmt);
		if (sql && instance) {
			instance->emit_signal("query_profiled", String::utf8(sql), (int64_t)execution_time_ns);
			sqlite3_free(sql);
		}
	}
	return 0;
}

int SQLiteAccess::_sqlite_authorizer_callback(void *p_user, int p_action, const char *p_arg1, const char *p_arg2, const char *p_db_name, const char *p_trigger_name) {
	SQLiteAccess *instance = (SQLiteAccess *)p_user;
	if (instance && instance->authorizer_callable.is_valid()) {
		if (instance->authorizer_callable.is_valid()) {
			Variant ret = instance->authorizer_callable.call(
					p_action,
					String::utf8(p_arg1 ? p_arg1 : ""),
					String::utf8(p_arg2 ? p_arg2 : ""),
					String::utf8(p_db_name ? p_db_name : ""),
					String::utf8(p_trigger_name ? p_trigger_name : ""));
			if (ret.get_type() == Variant::INT) {
				return ret.operator int();
			}
		}
	}
	return SQLITE_OK;
}

void SQLiteAccess::_configure_hooks() {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		sqlite3_update_hook(dbs, _sqlite_update_hook_callback, this);
		sqlite3_commit_hook(dbs, _sqlite_commit_hook_callback, this);
		sqlite3_rollback_hook(dbs, _sqlite_rollback_hook_callback, this);
		sqlite3_wal_hook(dbs, _sqlite_wal_hook_callback, this);
	}
}

void SQLiteAccess::_sqlite_update_hook_callback(void *p_arg, int p_op, char const *p_db_name, char const *p_table_name, sqlite3_int64 p_rowid) {
	SQLiteAccess *instance = (SQLiteAccess *)p_arg;
	if (instance) {
		instance->emit_signal("row_updated", p_op, String::utf8(p_db_name), String::utf8(p_table_name), (int64_t)p_rowid);
	}
}

int SQLiteAccess::_sqlite_commit_hook_callback(void *p_arg) {
	SQLiteAccess *instance = (SQLiteAccess *)p_arg;
	if (instance) {
		instance->emit_signal("transaction_committed");
	}
	return 0;
}

void SQLiteAccess::_sqlite_rollback_hook_callback(void *p_arg) {
	SQLiteAccess *instance = (SQLiteAccess *)p_arg;
	if (instance) {
		instance->emit_signal("transaction_rolled_back");
	}
}

int SQLiteAccess::_sqlite_wal_hook_callback(void *p_arg, sqlite3 *db, const char *db_name, int pages_in_wal) {
	SQLiteAccess *instance = (SQLiteAccess *)p_arg;
	if (instance) {
		instance->emit_signal("wal_updated", String::utf8(db_name), pages_in_wal);
	}
	return SQLITE_OK;
}

void SQLiteAccess::set_trace(bool p_enabled) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		if (p_enabled) {
			sqlite3_trace_v2(dbs, SQLITE_TRACE_PROFILE, SQLiteAccess::_sqlite_trace_callback, this);
		} else {
			sqlite3_trace_v2(dbs, 0, nullptr, nullptr);
		}
	}
}

void SQLiteAccess::set_authorizer(const Callable &p_callable) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		authorizer_callable = p_callable;
		if (p_callable.is_valid()) {
			sqlite3_set_authorizer(dbs, SQLiteAccess::_sqlite_authorizer_callback, this);
		} else {
			sqlite3_set_authorizer(dbs, nullptr, nullptr);
		}
	}
}

bool SQLiteAccess::enable_load_extension(bool p_enabled) {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		return false;
	}

	int res = sqlite3_enable_load_extension(dbs, p_enabled ? 1 : 0);
	if (res != SQLITE_OK) {
		ERR_PRINT("Failed to enable SQLite extension loading: " + String(sqlite3_errmsg(dbs)));
		return false;
	}
	return true;
}

void SQLiteAccess::interrupt() {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		sqlite3_interrupt(dbs);
	}
}

bool SQLiteAccess::execute_script(const String &p_path) {
	sqlite3 *dbs = get_handler();
	ERR_FAIL_COND_V_MSG(!dbs, false, "Cannot execute script: database not opened.");

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), false, "Cannot execute script: failed to open file " + p_path);

	String sql_script = file->get_as_utf8_string();

	char *errmsg = nullptr;
	int res = sqlite3_exec(dbs, sql_script.utf8().get_data(), nullptr, nullptr, &errmsg);

	if (res != SQLITE_OK) {
		String godot_err = "";
		if (errmsg != nullptr) {
			godot_err = String::utf8(errmsg);
			sqlite3_free(errmsg);
		}
		ERR_PRINT("SQLite execute_script failed: " + godot_err);
		return false;
	}
	return true;
}

bool SQLiteAccess::is_autocommit() const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_get_autocommit(dbs) != 0;
	}
	return false;
}

bool SQLiteAccess::is_readonly(const String &p_db_name) const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		int res = sqlite3_db_readonly(dbs, p_db_name.utf8().get_data());
		return res == 1; // 1: readonly, 0: r/w, -1: invalid name
	}
	return true;
}

String SQLiteAccess::get_database_filename(const String &p_db_name) const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		const char *filename = sqlite3_db_filename(dbs, p_db_name.utf8().get_data());
		if (filename) {
			return String::utf8(filename);
		}
	}
	return "";
}

bool SQLiteAccess::wal_checkpoint(const String &p_db_name) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		// Attempt to run a basic WAL checkpoint explicitly
		int res = sqlite3_wal_checkpoint(dbs, p_db_name.utf8().get_data());
		return res == SQLITE_OK;
	}
	return false;
}

int SQLiteAccess::_sqlite_collation_trampoline(void *ctx, int len1, const void *str1, int len2, const void *str2) {
	SQLiteCallableContext *context = (SQLiteCallableContext *)ctx;
	if (context && context->func.is_valid()) {
		String s1 = String::utf8((const char *)str1, len1);
		String s2 = String::utf8((const char *)str2, len2);

		Array args;
		args.push_back(s1);
		args.push_back(s2);

		Variant ret = context->func.callv(args);
		if (ret.get_type() == Variant::Type::INT) {
			return (int)ret;
		}
	}
	return 0;
}

bool SQLiteAccess::create_collation(const String &p_name, const Callable &p_callable) {
	sqlite3 *dbs = get_handler();
	if (!dbs) {
		return false;
	}

	if (custom_collations.has(p_name)) {
		memdelete(custom_collations[p_name]);
		custom_collations.erase(p_name);
	}

	SQLiteCallableContext *ctx = memnew(SQLiteCallableContext);
	ctx->func = p_callable;
	custom_collations[p_name] = ctx;

	int res = sqlite3_create_collation(dbs, p_name.utf8().get_data(), SQLITE_UTF8, ctx, _sqlite_collation_trampoline);
	return res == SQLITE_OK;
}

int SQLiteAccess::_sqlite_progress_handler_callback(void *p_arg) {
	SQLiteAccess *instance = (SQLiteAccess *)p_arg;
	if (instance) {
		instance->emit_signal("query_progress");
	}
	return 0; // return 0 to continue execution, 1 to interrupt
}

void SQLiteAccess::set_progress_handler(int p_instructions) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		sqlite3_progress_handler(dbs, p_instructions, p_instructions > 0 ? _sqlite_progress_handler_callback : nullptr, this);
	}
}

int SQLiteAccess::set_limit(int p_id, int p_new_val) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_limit(dbs, p_id, p_new_val);
	}
	return -1;
}

int SQLiteAccess::get_limit(int p_id) const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		return sqlite3_limit(dbs, p_id, -1);
	}
	return -1;
}

bool SQLiteAccess::set_db_config(int p_op, int p_val) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		int res = sqlite3_db_config(dbs, p_op, p_val, nullptr);
		return res == SQLITE_OK;
	}
	return false;
}

int SQLiteAccess::get_db_config(int p_op) const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		int current = 0;
		if (sqlite3_db_config(dbs, p_op, -1, &current) == SQLITE_OK) {
			return current;
		}
	}
	return -1;
}

void SQLiteAccess::set_foreign_keys_enabled(bool p_enabled) {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		if (p_enabled) {
			sqlite3_exec(dbs, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
		} else {
			sqlite3_exec(dbs, "PRAGMA foreign_keys = OFF;", nullptr, nullptr, nullptr);
		}
	}
}

int SQLiteAccess::get_db_status(int p_op, bool p_reset) const {
	sqlite3 *dbs = get_handler();
	if (dbs) {
		int current = 0;
		int highwater = 0;
		if (sqlite3_db_status(dbs, p_op, &current, &highwater, p_reset) == SQLITE_OK) {
			return current; // Returns current evaluation explicitly; highwater exists as a separate evaluation standard
		}
	}
	return -1; // -1 generally represents unsupported op
}

int SQLiteAccess::get_global_status(int p_op, bool p_reset) {
	int current = 0;
	int highwater = 0;
	if (sqlite3_status(p_op, &current, &highwater, p_reset) == SQLITE_OK) {
		return current;
	}
	return -1;
}

int SQLiteAccess::release_memory(int p_bytes) {
	return sqlite3_release_memory(p_bytes);
}

void SQLiteAccess::set_soft_heap_limit(int64_t p_bytes) {
	sqlite3_soft_heap_limit64(p_bytes);
}

int SQLiteQuery::get_stmt_status(int p_op, bool p_reset) {
	if (stmt) {
		return sqlite3_stmt_status(stmt, p_op, p_reset);
	}
	return -1;
}
