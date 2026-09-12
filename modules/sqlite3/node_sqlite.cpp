/**************************************************************************/
/*  node_sqlite.cpp                                                       */
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

#include "node_sqlite.h"

void SQLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_database"), &SQLite::get_database);
	ClassDB::bind_method(D_METHOD("set_database", "database"), &SQLite::set_database);

	ClassDB::bind_method(D_METHOD("create_table", "table_name", "columns"), &SQLite::create_table);
	ClassDB::bind_method(D_METHOD("drop_table", "table_name"), &SQLite::drop_table);

	ClassDB::bind_method(D_METHOD("backup_async", "path"), &SQLite::backup_async);
	ClassDB::bind_method(D_METHOD("restore_async", "path"), &SQLite::restore_async);

	ClassDB::bind_method(D_METHOD("open_blob", "db_name", "table_name", "column_name", "rowid", "read_write"), &SQLite::open_blob, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_authorizer", "callable"), &SQLite::set_authorizer);

	ClassDB::bind_method(D_METHOD("create_index", "index_name", "table_name", "columns", "unique"), &SQLite::create_index, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("drop_index", "index_name"), &SQLite::drop_index);

	ClassDB::bind_method(D_METHOD("begin_transaction"), &SQLite::begin_transaction);
	ClassDB::bind_method(D_METHOD("commit_transaction"), &SQLite::commit_transaction);
	ClassDB::bind_method(D_METHOD("rollback_transaction"), &SQLite::rollback_transaction);

	ClassDB::bind_method(D_METHOD("create_savepoint", "name"), &SQLite::create_savepoint);
	ClassDB::bind_method(D_METHOD("release_savepoint", "name"), &SQLite::release_savepoint);
	ClassDB::bind_method(D_METHOD("rollback_to_savepoint", "name"), &SQLite::rollback_to_savepoint);

	ClassDB::bind_method(D_METHOD("insert_row", "table_name", "value"), &SQLite::insert_row);
	ClassDB::bind_method(D_METHOD("insert_rows", "table_name", "values"), &SQLite::insert_rows);

	ClassDB::bind_method(D_METHOD("update_rows", "table_name", "condition", "value"), &SQLite::update_rows, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("delete_rows", "table_name", "condition"), &SQLite::delete_rows, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("select_rows", "table_name", "condition"), &SQLite::select_rows, DEFVAL(String()));

	ClassDB::bind_method(D_METHOD("get_columns", "table_name"), &SQLite::get_columns);
	ClassDB::bind_method(D_METHOD("get_tables"), &SQLite::get_tables);

	ClassDB::bind_method(D_METHOD("create_query", "query", "arguments"), &SQLite::create_query, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("execute_query", "query", "arguments"), &SQLite::execute_query, DEFVAL(Array()));

	ClassDB::bind_method(D_METHOD("export_to_json", "table_name"), &SQLite::export_to_json);
	ClassDB::bind_method(D_METHOD("import_from_json", "table_name", "json_string"), &SQLite::import_from_json);
	ClassDB::bind_method(D_METHOD("attach_database", "path", "alias"), &SQLite::attach_database);
	ClassDB::bind_method(D_METHOD("detach_database", "alias"), &SQLite::detach_database);

	ClassDB::bind_method(D_METHOD("serialize_object", "table_name", "key", "object"), &SQLite::serialize_object);
	ClassDB::bind_method(D_METHOD("deserialize_object", "table_name", "key", "object"), &SQLite::deserialize_object);
	ClassDB::bind_method(D_METHOD("instantiate_object", "table_name", "key"), &SQLite::instantiate_object);

	ClassDB::bind_method(D_METHOD("backup_to", "path"), &SQLite::backup_to);
	ClassDB::bind_method(D_METHOD("restore_from", "path"), &SQLite::restore_from);
	ClassDB::bind_method(D_METHOD("load_from", "path"), &SQLite::load_from);
	ClassDB::bind_method(D_METHOD("close"), &SQLite::close);

	ClassDB::bind_method(D_METHOD("get_last_insert_rowid"), &SQLite::get_last_insert_rowid);
	ClassDB::bind_method(D_METHOD("get_changes"), &SQLite::get_changes);
	ClassDB::bind_method(D_METHOD("get_total_changes"), &SQLite::get_total_changes);
	ClassDB::bind_method(D_METHOD("set_busy_timeout", "ms"), &SQLite::set_busy_timeout);
	ClassDB::bind_method(D_METHOD("vacuum"), &SQLite::vacuum);

	ClassDB::bind_method(D_METHOD("interrupt"), &SQLite::interrupt);
	ClassDB::bind_method(D_METHOD("set_trace", "enabled"), &SQLite::set_trace);
	ClassDB::bind_method(D_METHOD("enable_load_extension", "enabled"), &SQLite::enable_load_extension);
	ClassDB::bind_method(D_METHOD("execute_script", "path"), &SQLite::execute_script);

	ClassDB::bind_method(D_METHOD("is_autocommit"), &SQLite::is_autocommit);
	ClassDB::bind_method(D_METHOD("is_readonly", "db_name"), &SQLite::is_readonly, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("get_database_filename", "db_name"), &SQLite::get_database_filename, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("wal_checkpoint", "db_name"), &SQLite::wal_checkpoint, DEFVAL("main"));

	ClassDB::bind_method(D_METHOD("create_function", "name", "argc", "callable"), &SQLite::create_function);
	ClassDB::bind_method(D_METHOD("create_aggregate", "name", "argc", "step_callable", "final_callable"), &SQLite::create_aggregate);

	ClassDB::bind_method(D_METHOD("create_collation", "name", "callable"), &SQLite::create_collation);
	ClassDB::bind_method(D_METHOD("set_progress_handler", "instructions"), &SQLite::set_progress_handler);

	ClassDB::bind_method(D_METHOD("set_limit", "id", "new_val"), &SQLite::set_limit);
	ClassDB::bind_method(D_METHOD("get_limit", "id"), &SQLite::get_limit);

	ClassDB::bind_method(D_METHOD("set_db_config", "op", "val"), &SQLite::set_db_config);
	ClassDB::bind_method(D_METHOD("get_db_config", "op"), &SQLite::get_db_config);
	ClassDB::bind_method(D_METHOD("set_foreign_keys_enabled", "enabled"), &SQLite::set_foreign_keys_enabled);

	ClassDB::bind_method(D_METHOD("get_db_status", "op", "reset"), &SQLite::get_db_status, DEFVAL(false));
	ClassDB::bind_static_method("SQLite", D_METHOD("get_global_status", "op", "reset"), &SQLite::get_global_status);

	ClassDB::bind_static_method("SQLite", D_METHOD("release_memory", "bytes"), &SQLite::release_memory);
	ClassDB::bind_static_method("SQLite", D_METHOD("set_soft_heap_limit", "bytes"), &SQLite::set_soft_heap_limit);

	ClassDB::bind_method(D_METHOD("get_last_error_message"), &SQLite::get_last_error_message);
	ClassDB::bind_method(D_METHOD("get_last_error_code"), &SQLite::get_last_error_code);

	ClassDB::bind_method(D_METHOD("_on_row_updated", "op", "db_name", "table_name", "rowid"), &SQLite::_on_row_updated);
	ClassDB::bind_method(D_METHOD("_on_transaction_committed"), &SQLite::_on_transaction_committed);
	ClassDB::bind_method(D_METHOD("_on_transaction_rolled_back"), &SQLite::_on_transaction_rolled_back);
	ClassDB::bind_method(D_METHOD("_on_query_progress"), &SQLite::_on_query_progress);
	ClassDB::bind_method(D_METHOD("_on_wal_updated", "db_name", "pages"), &SQLite::_on_wal_updated);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "SQLiteDatabase"), "set_database", "get_database");

	ADD_SIGNAL(MethodInfo("row_updated", PropertyInfo(Variant::INT, "update_operation"), PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::STRING, "table_name"), PropertyInfo(Variant::INT, "rowid")));
	ADD_SIGNAL(MethodInfo("transaction_committed"));
	ADD_SIGNAL(MethodInfo("transaction_rolled_back"));
	ADD_SIGNAL(MethodInfo("query_progress"));
	ADD_SIGNAL(MethodInfo("wal_updated", PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::INT, "pages_in_wal")));
}

Ref<SQLiteQuery> SQLite::create_table(const String &p_table_name, const TypedArray<SQLiteColumnSchema> &p_columns) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->create_table(p_table_name, p_columns);
}

Ref<SQLiteQuery> SQLite::drop_table(const String &p_table_name) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->drop_table(p_table_name);
}

Ref<SQLiteQuery> SQLite::create_index(const String &p_index_name, const String &p_table_name, const TypedArray<String> &p_columns, bool p_unique) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->create_index(p_index_name, p_table_name, p_columns, p_unique);
}

Ref<SQLiteQuery> SQLite::drop_index(const String &p_index_name) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->drop_index(p_index_name);
}

Ref<SQLiteBackup> SQLite::backup_async(const String &path) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteBackup>());
	return database->backup_async(path);
}

Ref<SQLiteBackup> SQLite::restore_async(const String &path) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteBackup>());
	return database->restore_async(path);
}

Ref<SQLiteBlob> SQLite::open_blob(const String &p_db_name, const String &p_table_name, const String &p_column_name, int64_t p_rowid, bool p_read_write) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteBlob>());
	return database->open_blob(p_db_name, p_table_name, p_column_name, p_rowid, p_read_write);
}

Ref<SQLiteQuery> SQLite::begin_transaction() {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->begin_transaction();
}

Ref<SQLiteQuery> SQLite::commit_transaction() {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->commit_transaction();
}

Ref<SQLiteQuery> SQLite::rollback_transaction() {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->rollback_transaction();
}

Ref<SQLiteQuery> SQLite::create_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->create_savepoint(p_name);
}

Ref<SQLiteQuery> SQLite::release_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->release_savepoint(p_name);
}

Ref<SQLiteQuery> SQLite::rollback_to_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->rollback_to_savepoint(p_name);
}

Ref<SQLiteQuery> SQLite::insert_row(const String &p_name, const Dictionary &p_row_dict) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->insert_row(p_name, p_row_dict);
}

Ref<SQLiteQuery> SQLite::insert_rows(const String &p_name, const TypedArray<Dictionary> &p_row_array) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->insert_rows(p_name, p_row_array);
}

Ref<SQLiteQuery> SQLite::update_rows(const String &p_name, const String &p_conditions, const Dictionary &p_row_dict) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->update_rows(p_name, p_conditions, p_row_dict);
}

Ref<SQLiteQuery> SQLite::delete_rows(const String &p_name, const String &p_conditions) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->delete_rows(p_name, p_conditions);
}

Ref<SQLiteQuery> SQLite::select_rows(const String &p_name, const String &p_conditions) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->select_rows(p_name, p_conditions);
}

TypedArray<SQLiteColumnSchema> SQLite::get_columns(const String &p_name) const {
	ERR_FAIL_COND_V(database.is_null(), TypedArray<SQLiteColumnSchema>());
	return database->get_columns(p_name);
}

Dictionary SQLite::get_tables() const {
	ERR_FAIL_COND_V(database.is_null(), Dictionary());
	return database->get_tables();
}

Ref<SQLiteQuery> SQLite::create_query(const String &p_query, const Array &p_args) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQuery>());
	return database->create_query(p_query, p_args);
}

Ref<SQLiteQueryResult> SQLite::execute_query(const String &p_query, const Array &p_args) {
	ERR_FAIL_COND_V(database.is_null(), Ref<SQLiteQueryResult>());
	return database->execute_query(p_query, p_args);
}

String SQLite::export_to_json(const String &p_table_name) {
	ERR_FAIL_COND_V(database.is_null(), String());
	return database->export_to_json(p_table_name);
}

bool SQLite::import_from_json(const String &p_table_name, const String &p_json_string) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->import_from_json(p_table_name, p_json_string);
}

bool SQLite::attach_database(const String &p_path, const String &p_alias) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->attach_database(p_path, p_alias);
}

bool SQLite::detach_database(const String &p_alias) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->detach_database(p_alias);
}

bool SQLite::serialize_object(const String &p_table_name, const String &p_key, Object *p_object) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->serialize_object(p_table_name, p_key, p_object);
}

bool SQLite::deserialize_object(const String &p_table_name, const String &p_key, Object *p_object) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->deserialize_object(p_table_name, p_key, p_object);
}

Variant SQLite::instantiate_object(const String &p_table_name, const String &p_key) {
	ERR_FAIL_COND_V(database.is_null(), Variant());
	return database->instantiate_object(p_table_name, p_key);
}

bool SQLite::backup_to(const String &p_path) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->backup_to(p_path);
}

bool SQLite::restore_from(const String &p_path) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->restore_from(p_path);
}

bool SQLite::load_from(const String &p_path) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->load_from(p_path);
}

void SQLite::close() {
	if (database.is_valid()) {
		database->close();
	}
}

int64_t SQLite::get_last_insert_rowid() const {
	ERR_FAIL_COND_V(database.is_null(), 0);
	return database->get_last_insert_rowid();
}

int64_t SQLite::get_changes() const {
	ERR_FAIL_COND_V(database.is_null(), 0);
	return database->get_changes();
}

int64_t SQLite::get_total_changes() const {
	ERR_FAIL_COND_V(database.is_null(), 0);
	return database->get_total_changes();
}

bool SQLite::set_busy_timeout(int p_ms) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->set_busy_timeout(p_ms);
}

bool SQLite::vacuum() {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->vacuum();
}

void SQLite::interrupt() {
	if (database.is_valid()) {
		database->interrupt();
	}
}

void SQLite::set_trace(bool p_enabled) {
	if (database.is_valid()) {
		database->set_trace(p_enabled);
	}
}

bool SQLite::enable_load_extension(bool p_enabled) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->enable_load_extension(p_enabled);
}

bool SQLite::execute_script(const String &p_path) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->execute_script(p_path);
}

bool SQLite::is_autocommit() const {
	if (database.is_valid()) {
		return database->is_autocommit();
	}
	return false;
}

bool SQLite::is_readonly(const String &p_db_name) const {
	if (database.is_valid()) {
		return database->is_readonly(p_db_name);
	}
	return true;
}

String SQLite::get_database_filename(const String &p_db_name) const {
	if (database.is_valid()) {
		return database->get_database_filename(p_db_name);
	}
	return "";
}

bool SQLite::wal_checkpoint(const String &p_db_name) {
	if (database.is_valid()) {
		return database->wal_checkpoint(p_db_name);
	}
	return false;
}

bool SQLite::create_function(const String &p_name, int p_argc, const Callable &p_callable) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->create_function(p_name, p_argc, p_callable);
}

String SQLite::get_last_error_message() const {
	ERR_FAIL_COND_V(database.is_null(), String());
	return database->get_last_error_message();
}

int SQLite::get_last_error_code() const {
	ERR_FAIL_COND_V(database.is_null(), 0);
	return database->get_last_error_code();
}

void SQLite::set_database(const Ref<SQLiteDatabase> &p_database) {
	if (database.is_valid()) {
		database->disconnect("row_updated", callable_mp(this, &SQLite::_on_row_updated));
		database->disconnect("transaction_committed", callable_mp(this, &SQLite::_on_transaction_committed));
		database->disconnect("transaction_rolled_back", callable_mp(this, &SQLite::_on_transaction_rolled_back));
		database->disconnect("query_progress", callable_mp(this, &SQLite::_on_query_progress));
		database->disconnect("wal_updated", callable_mp(this, &SQLite::_on_wal_updated));
	}
	database = p_database;
	if (database.is_valid()) {
		database->connect("row_updated", callable_mp(this, &SQLite::_on_row_updated));
		database->connect("transaction_committed", callable_mp(this, &SQLite::_on_transaction_committed));
		database->connect("transaction_rolled_back", callable_mp(this, &SQLite::_on_transaction_rolled_back));
		database->connect("query_progress", callable_mp(this, &SQLite::_on_query_progress));
		database->connect("wal_updated", callable_mp(this, &SQLite::_on_wal_updated));
	}
}

Ref<SQLiteDatabase> SQLite::get_database() const {
	return database;
}

void SQLite::_on_row_updated(int p_op, const String &p_db_name, const String &p_table_name, int64_t p_rowid) {
	emit_signal("row_updated", p_op, p_db_name, p_table_name, p_rowid);
}

void SQLite::_on_transaction_committed() {
	emit_signal("transaction_committed");
}

void SQLite::_on_transaction_rolled_back() {
	emit_signal("transaction_rolled_back");
}

void SQLite::_on_query_progress() {
	emit_signal("query_progress");
}

void SQLite::_on_wal_updated(const String &p_db_name, int p_pages) {
	emit_signal("wal_updated", p_db_name, p_pages);
}

bool SQLite::create_aggregate(const String &p_name, int p_argc, const Callable &p_step_callable, const Callable &p_final_callable) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->create_aggregate(p_name, p_argc, p_step_callable, p_final_callable);
}

bool SQLite::create_collation(const String &p_name, const Callable &p_callable) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->create_collation(p_name, p_callable);
}

void SQLite::set_progress_handler(int p_instructions) {
	if (database.is_valid()) {
		database->set_progress_handler(p_instructions);
	}
}

void SQLite::set_authorizer(const Callable &p_callable) {
	if (database.is_valid()) {
		database->set_authorizer(p_callable);
	}
}

int SQLite::set_limit(int p_id, int p_new_val) {
	ERR_FAIL_COND_V(database.is_null(), -1);
	return database->set_limit(p_id, p_new_val);
}

int SQLite::get_limit(int p_id) const {
	ERR_FAIL_COND_V(database.is_null(), -1);
	return database->get_limit(p_id);
}

bool SQLite::set_db_config(int p_op, int p_val) {
	ERR_FAIL_COND_V(database.is_null(), false);
	return database->set_db_config(p_op, p_val);
}

int SQLite::get_db_config(int p_op) const {
	ERR_FAIL_COND_V(database.is_null(), -1);
	return database->get_db_config(p_op);
}

void SQLite::set_foreign_keys_enabled(bool p_enabled) {
	if (database.is_valid()) {
		database->set_foreign_keys_enabled(p_enabled);
	}
}

int SQLite::get_db_status(int p_op, bool p_reset) const {
	ERR_FAIL_COND_V(database.is_null(), -1);
	return database->get_db_status(p_op, p_reset);
}

int SQLite::get_global_status(int p_op, bool p_reset) {
	return SQLiteDatabase::get_global_status(p_op, p_reset);
}

int SQLite::release_memory(int p_bytes) {
	return SQLiteDatabase::release_memory(p_bytes);
}

void SQLite::set_soft_heap_limit(int64_t p_bytes) {
	SQLiteDatabase::set_soft_heap_limit(p_bytes);
}
