/**************************************************************************/
/*  resource_sqlite.cpp                                                   */
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

#include "resource_sqlite.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/variant/variant_utility.h"

void SQLiteDatabase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_sqlite"), &SQLiteDatabase::get_sqlite);
	ClassDB::bind_method(D_METHOD("create_table", "table_name", "columns"), &SQLiteDatabase::create_table);
	ClassDB::bind_method(D_METHOD("drop_table", "table_name"), &SQLiteDatabase::drop_table);

	ClassDB::bind_method(D_METHOD("backup_async", "path"), &SQLiteDatabase::backup_async);
	ClassDB::bind_method(D_METHOD("restore_async", "path"), &SQLiteDatabase::restore_async);

	ClassDB::bind_method(D_METHOD("open_blob", "db_name", "table_name", "column_name", "rowid", "read_write"), &SQLiteDatabase::open_blob, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("create_index", "index_name", "table_name", "columns", "unique"), &SQLiteDatabase::create_index, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("drop_index", "index_name"), &SQLiteDatabase::drop_index);

	ClassDB::bind_method(D_METHOD("begin_transaction"), &SQLiteDatabase::begin_transaction);
	ClassDB::bind_method(D_METHOD("commit_transaction"), &SQLiteDatabase::commit_transaction);
	ClassDB::bind_method(D_METHOD("rollback_transaction"), &SQLiteDatabase::rollback_transaction);

	ClassDB::bind_method(D_METHOD("create_savepoint", "name"), &SQLiteDatabase::create_savepoint);
	ClassDB::bind_method(D_METHOD("release_savepoint", "name"), &SQLiteDatabase::release_savepoint);
	ClassDB::bind_method(D_METHOD("rollback_to_savepoint", "name"), &SQLiteDatabase::rollback_to_savepoint);
	ClassDB::bind_method(D_METHOD("create_query", "query", "arguments"), &SQLiteDatabase::create_query, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("execute_query", "query", "arguments"), &SQLiteDatabase::execute_query, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("get_columns", "table_name"), &SQLiteDatabase::get_columns);
	ClassDB::bind_method(D_METHOD("insert_row", "table_name", "value"), &SQLiteDatabase::insert_row);
	ClassDB::bind_method(D_METHOD("insert_rows", "table_name", "values"), &SQLiteDatabase::insert_rows);
	ClassDB::bind_method(D_METHOD("update_rows", "table_name", "condition", "value"), &SQLiteDatabase::update_rows, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("delete_rows", "table_name", "condition"), &SQLiteDatabase::delete_rows, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("select_rows", "table_name", "condition"), &SQLiteDatabase::select_rows, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_tables"), &SQLiteDatabase::get_tables);

	ClassDB::bind_method(D_METHOD("set_data", "data"), &SQLiteDatabase::set_data);
	ClassDB::bind_method(D_METHOD("set_resource", "path"), &SQLiteDatabase::set_resource);

	ClassDB::bind_method(D_METHOD("export_to_json", "table_name"), &SQLiteDatabase::export_to_json);
	ClassDB::bind_method(D_METHOD("import_from_json", "table_name", "json_string"), &SQLiteDatabase::import_from_json);
	ClassDB::bind_method(D_METHOD("attach_database", "path", "alias"), &SQLiteDatabase::attach_database);
	ClassDB::bind_method(D_METHOD("detach_database", "alias"), &SQLiteDatabase::detach_database);

	ClassDB::bind_method(D_METHOD("serialize_object", "table_name", "key", "object"), &SQLiteDatabase::serialize_object);
	ClassDB::bind_method(D_METHOD("deserialize_object", "table_name", "key", "object"), &SQLiteDatabase::deserialize_object);
	ClassDB::bind_method(D_METHOD("instantiate_object", "table_name", "key"), &SQLiteDatabase::instantiate_object);

	ClassDB::bind_method(D_METHOD("backup_to", "path"), &SQLiteDatabase::backup_to);
	ClassDB::bind_method(D_METHOD("restore_from", "path"), &SQLiteDatabase::restore_from);
	ClassDB::bind_method(D_METHOD("load_from", "path"), &SQLiteDatabase::load_from);
	ClassDB::bind_method(D_METHOD("close"), &SQLiteDatabase::close);

	ClassDB::bind_method(D_METHOD("get_last_insert_rowid"), &SQLiteDatabase::get_last_insert_rowid);
	ClassDB::bind_method(D_METHOD("get_changes"), &SQLiteDatabase::get_changes);
	ClassDB::bind_method(D_METHOD("get_total_changes"), &SQLiteDatabase::get_total_changes);
	ClassDB::bind_method(D_METHOD("set_busy_timeout", "ms"), &SQLiteDatabase::set_busy_timeout);
	ClassDB::bind_method(D_METHOD("vacuum"), &SQLiteDatabase::vacuum);

	ClassDB::bind_method(D_METHOD("interrupt"), &SQLiteDatabase::interrupt);
	ClassDB::bind_method(D_METHOD("set_trace", "enabled"), &SQLiteDatabase::set_trace);
	ClassDB::bind_method(D_METHOD("enable_load_extension", "enabled"), &SQLiteDatabase::enable_load_extension);
	ClassDB::bind_method(D_METHOD("execute_script", "path"), &SQLiteDatabase::execute_script);

	ClassDB::bind_method(D_METHOD("is_autocommit"), &SQLiteDatabase::is_autocommit);
	ClassDB::bind_method(D_METHOD("is_readonly", "db_name"), &SQLiteDatabase::is_readonly, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("get_database_filename", "db_name"), &SQLiteDatabase::get_database_filename, DEFVAL("main"));
	ClassDB::bind_method(D_METHOD("wal_checkpoint", "db_name"), &SQLiteDatabase::wal_checkpoint, DEFVAL("main"));

	ClassDB::bind_method(D_METHOD("create_function", "name", "argc", "callable"), &SQLiteDatabase::create_function);
	ClassDB::bind_method(D_METHOD("create_aggregate", "name", "argc", "step_callable", "final_callable"), &SQLiteDatabase::create_aggregate);
	ClassDB::bind_method(D_METHOD("create_collation", "name", "callable"), &SQLiteDatabase::create_collation);
	ClassDB::bind_method(D_METHOD("set_progress_handler", "instructions"), &SQLiteDatabase::set_progress_handler);
	ClassDB::bind_method(D_METHOD("set_authorizer", "callable"), &SQLiteDatabase::set_authorizer);

	ClassDB::bind_method(D_METHOD("set_limit", "id", "new_val"), &SQLiteDatabase::set_limit);
	ClassDB::bind_method(D_METHOD("get_limit", "id"), &SQLiteDatabase::get_limit);

	ClassDB::bind_method(D_METHOD("set_db_config", "op", "val"), &SQLiteDatabase::set_db_config);
	ClassDB::bind_method(D_METHOD("get_db_config", "op"), &SQLiteDatabase::get_db_config);
	ClassDB::bind_method(D_METHOD("set_foreign_keys_enabled", "enabled"), &SQLiteDatabase::set_foreign_keys_enabled);

	ClassDB::bind_method(D_METHOD("get_db_status", "op", "reset"), &SQLiteDatabase::get_db_status, DEFVAL(false));
	ClassDB::bind_static_method("SQLiteDatabase", D_METHOD("get_global_status", "op", "reset"), &SQLiteDatabase::get_global_status);

	ClassDB::bind_static_method("SQLiteDatabase", D_METHOD("release_memory", "bytes"), &SQLiteDatabase::release_memory);
	ClassDB::bind_static_method("SQLiteDatabase", D_METHOD("set_soft_heap_limit", "bytes"), &SQLiteDatabase::set_soft_heap_limit);
	ClassDB::bind_method(D_METHOD("_on_row_updated", "op", "db_name", "table_name", "rowid"), &SQLiteDatabase::_on_row_updated);
	ClassDB::bind_method(D_METHOD("_on_transaction_committed"), &SQLiteDatabase::_on_transaction_committed);
	ClassDB::bind_method(D_METHOD("_on_transaction_rolled_back"), &SQLiteDatabase::_on_transaction_rolled_back);
	ClassDB::bind_method(D_METHOD("_on_query_progress"), &SQLiteDatabase::_on_query_progress);
	ClassDB::bind_method(D_METHOD("_on_query_profiled", "query", "execution_time_ns"), &SQLiteDatabase::_on_query_profiled);
	ClassDB::bind_method(D_METHOD("_on_wal_updated", "db_name", "pages"), &SQLiteDatabase::_on_wal_updated);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "tables"), "", "get_tables");

	ADD_SIGNAL(MethodInfo("row_updated", PropertyInfo(Variant::INT, "update_operation"), PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::STRING, "table_name"), PropertyInfo(Variant::INT, "rowid")));
	ADD_SIGNAL(MethodInfo("transaction_committed"));
	ADD_SIGNAL(MethodInfo("transaction_rolled_back"));
	ADD_SIGNAL(MethodInfo("query_progress"));
	ADD_SIGNAL(MethodInfo("query_profiled", PropertyInfo(Variant::STRING, "query_string"), PropertyInfo(Variant::INT, "execution_time_ns")));
	ADD_SIGNAL(MethodInfo("wal_updated", PropertyInfo(Variant::STRING, "db_name"), PropertyInfo(Variant::INT, "pages_in_wal")));
}

void SQLiteDatabase::set_resource(const String &p_path) {
	db->open(p_path);
	emit_changed();
}

void SQLiteDatabase::set_data(const PackedByteArray &p_data) {
	db->open_buffered(get_name(), p_data, p_data.size());
	emit_changed();
}

Ref<SQLiteAccess> SQLiteDatabase::get_sqlite() {
	return db;
}

Ref<SQLiteBackup> SQLiteDatabase::backup_async(const String &path) {
	return db->backup_async(path);
}

Ref<SQLiteBackup> SQLiteDatabase::restore_async(const String &path) {
	return db->restore_async(path);
}

Ref<SQLiteBlob> SQLiteDatabase::open_blob(const String &p_db_name, const String &p_table_name, const String &p_column_name, int64_t p_rowid, bool p_read_write) {
	return db->open_blob(p_db_name, p_table_name, p_column_name, p_rowid, p_read_write);
}

String SQLiteDatabase::export_to_json(const String &p_table_name) {
	Ref<SQLiteQuery> query = select_rows(p_table_name, "");
	Ref<SQLiteQueryResult> res = query->execute(Array());
	ERR_FAIL_COND_V_MSG(res->get_error_code() != 0, "", "Export failed: " + res->get_error());
	return JSON::stringify(res->get_result());
}

bool SQLiteDatabase::import_from_json(const String &p_table_name, const String &p_json_string) {
	Variant parsed = JSON::parse_string(p_json_string);
	ERR_FAIL_COND_V_MSG(parsed.get_type() != Variant::ARRAY, false, "JSON root must be an array of objects.");

	TypedArray<Dictionary> arr = parsed;
	if (arr.is_empty()) {
		return true;
	}

	Ref<SQLiteQuery> query = insert_rows(p_table_name, arr);
	Ref<SQLiteQueryResult> res = query->execute(Array());
	ERR_FAIL_COND_V_MSG(res->get_error_code() != 0, false, "Import failed: " + res->get_error());
	return true;
}

Ref<SQLiteQuery> SQLiteDatabase::create_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(db.is_null(), Ref<SQLiteQuery>());
	return db->create_savepoint(p_name);
}

Ref<SQLiteQuery> SQLiteDatabase::release_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(db.is_null(), Ref<SQLiteQuery>());
	return db->release_savepoint(p_name);
}

Ref<SQLiteQuery> SQLiteDatabase::rollback_to_savepoint(const String &p_name) {
	ERR_FAIL_COND_V(db.is_null(), Ref<SQLiteQuery>());
	return db->rollback_to_savepoint(p_name);
}

Ref<SQLiteQuery> SQLiteDatabase::create_table(const String &p_table_name, const TypedArray<SQLiteColumnSchema> &p_columns) {
	String query_string, type_string, key_string, primary_string;
	/* Create SQL statement */
	query_string = "CREATE TABLE IF NOT EXISTS " + p_table_name + " (";
	key_string = "";
	primary_string = "";

	Dictionary column_dict;
	int primary_key_columns = 0;
	for (int64_t i = 0; i < p_columns.size(); i++) {
		Ref<SQLiteColumnSchema> schema = p_columns[i];
		if (schema->is_primary_key()) {
			primary_key_columns++;
		}
	}
	for (int64_t i = 0; i < p_columns.size(); i++) {
		Ref<SQLiteColumnSchema> schema = p_columns[i];
		query_string += (const String &)schema->get_name() + String(" ");
		switch (schema->get_type()) {
			case Variant::Type::NIL:
				type_string = "NULL";
				break;
			case Variant::Type::INT:
				type_string = "INTEGER";
				break;
			case Variant::Type::FLOAT:
				type_string = "REAL";
				break;
			case Variant::Type::STRING:
				type_string = "TEXT";
				break;
			case Variant::Type::PACKED_BYTE_ARRAY:
				type_string = "BLOB";
				break;
			default:
				ERR_PRINT(vformat("Invalid column type. %s", schema->get_type()));
				type_string = "NULL";
				break;
		}
		query_string += type_string;

		/* Primary key check */
		if (schema->is_primary_key()) {
			if (primary_key_columns == 1) {
				query_string += String(" PRIMARY KEY");
				/* Autoincrement check */
				if (schema->is_auto_increment()) {
					query_string += String(" AUTOINCREMENT");
				}
			} else {
				if (primary_string.is_empty()) {
					primary_string = schema->get_name();
				} else {
					primary_string += String(", ") + schema->get_name();
				}
			}
		}
		/* Not null check */
		if (schema->is_not_null()) {
			query_string += String(" NOT NULL");
		}
		/* Unique check */
		if (schema->is_unique()) {
			query_string += String(" UNIQUE");
		}
		/* Default check */
		if (!schema->get_default_value().is_null()) {
			query_string += String(" DEFAULT ") + schema->get_default_value().to_json_string();
		}

		if (i != p_columns.size() - 1) {
			query_string += ",";
		}
	}

	if (primary_key_columns > 1) {
		query_string += String(", PRIMARY KEY (") + primary_string + String(")");
	}

	query_string += key_string + ");";
	return db->create_query(query_string);
}

Ref<SQLiteQuery> SQLiteDatabase::drop_table(const String &p_name) {
	String query_string;
	/* Create SQL statement */
	query_string = "DROP TABLE " + p_name + ";";

	return db->create_query(query_string);
}

Ref<SQLiteQuery> SQLiteDatabase::create_index(const String &p_index_name, const String &p_table_name, const TypedArray<String> &p_columns, bool p_unique) {
	String query_string = "CREATE " + String(p_unique ? "UNIQUE " : "") + "INDEX " + p_index_name + " ON " + p_table_name + " (";
	for (int i = 0; i < p_columns.size(); i++) {
		query_string += (const String &)p_columns[i];
		if (i != p_columns.size() - 1) {
			query_string += ", ";
		}
	}
	query_string += ");";
	return db->create_query(query_string);
}

Ref<SQLiteQuery> SQLiteDatabase::drop_index(const String &p_index_name) {
	return db->create_query("DROP INDEX " + p_index_name + ";");
}

Ref<SQLiteQuery> SQLiteDatabase::begin_transaction() {
	return db->create_query("BEGIN TRANSACTION;");
}

Ref<SQLiteQuery> SQLiteDatabase::commit_transaction() {
	return db->create_query("COMMIT;");
}

Ref<SQLiteQuery> SQLiteDatabase::rollback_transaction() {
	return db->create_query("ROLLBACK;");
}

Ref<SQLiteQuery> SQLiteDatabase::insert_row(const String &p_name, const Dictionary &p_row_dict) {
	String query_string, key_string, value_string = "";
	Array keys = p_row_dict.keys();
	Array param_bindings = p_row_dict.values();

	/* Create SQL statement */
	query_string = "INSERT INTO " + p_name;

	int64_t number_of_keys = p_row_dict.size();
	for (int64_t i = 0; i < number_of_keys; i++) {
		key_string += (const String &)keys[i];
		value_string += "?";
		if (i != number_of_keys - 1) {
			key_string += ",";
			value_string += ",";
		}
	}
	query_string += " (" + key_string + ") VALUES (" + value_string + ");";

	return db->create_query(query_string, param_bindings);
}

Ref<SQLiteQuery> SQLiteDatabase::insert_rows(const String &p_name, const TypedArray<Dictionary> &p_row_array) {
	ERR_FAIL_COND_V(p_row_array.is_empty(), Ref<SQLiteQuery>());
	String query_string, key_string, value_string = "", values_string = "";
	Dictionary row0 = p_row_array[0];
	Array keys = row0.keys();
	Array param_bindings;

	/* Create SQL statement */
	query_string = "INSERT INTO " + p_name;

	int64_t number_of_keys = row0.size();
	for (int64_t i = 0; i < number_of_keys; i++) {
		key_string += (const String &)keys[i];
		value_string += "?";
		if (i != number_of_keys - 1) {
			key_string += ",";
			value_string += ",";
		}
	}
	for (int64_t i = 0; i < p_row_array.size(); i++) {
		values_string += "(";
		values_string += value_string;
		values_string += ")";
		if (i != p_row_array.size() - 1) {
			values_string += ",";
		}
		Dictionary row = p_row_array[i];
		Array values = row.values();
		param_bindings.append_array(values);
	}
	query_string += " (" + key_string + ") VALUES " + values_string + ";";

	return db->create_query(query_string, param_bindings);
}

Ref<SQLiteQuery> SQLiteDatabase::select_rows(const String &p_name, const String &p_conditions) {
	String query_string;

	/* Create SQL statement */
	query_string = "SELECT * FROM " + p_name;
	/* If it's empty or * everything is to be selected */
	if (!p_conditions.is_empty() && (p_conditions != (const String &)"*")) {
		query_string += " WHERE " + p_conditions;
	}

	return db->create_query(query_string);
}

Ref<SQLiteQuery> SQLiteDatabase::update_rows(const String &p_name, const String &p_conditions, const Dictionary &p_row_dict) {
	ERR_FAIL_COND_V(p_row_dict.is_empty(), Ref<SQLiteQuery>());
	String query_string, key_string;
	Array keys = p_row_dict.keys();
	Array param_bindings = p_row_dict.values();

	/* Create SQL statement */
	query_string = "UPDATE " + p_name + " SET ";

	int64_t number_of_keys = p_row_dict.size();
	for (int64_t i = 0; i < number_of_keys; i++) {
		key_string += (const String &)keys[i] + "=?";
		if (i != number_of_keys - 1) {
			key_string += ",";
		}
	}
	query_string += key_string;

	/* If it's empty or * everything is to be updated */
	if (!p_conditions.is_empty() && (p_conditions != (const String &)"*")) {
		query_string += " WHERE " + p_conditions;
	}
	query_string += ";";

	return db->create_query(query_string, param_bindings);
}

Ref<SQLiteQuery> SQLiteDatabase::delete_rows(const String &p_name, const String &p_conditions) {
	String query_string;

	/* Create SQL statement */
	query_string = "DELETE FROM " + p_name;
	/* If it's empty or * everything is to be deleted */
	if (!p_conditions.is_empty() && (p_conditions != (const String &)"*")) {
		query_string += " WHERE " + p_conditions;
	}
	query_string += ";";

	return db->create_query(query_string);
}

TypedArray<SQLiteColumnSchema> SQLiteDatabase::get_columns(const String &p_name) const {
	Ref<SQLiteQuery> query = db->create_query("PRAGMA table_info(" + p_name + ")");
	Ref<SQLiteQueryResult> result = query->execute(Array());
	if (result->get_error() != "") {
		ERR_PRINT("Error getting column names: " + result->get_error() + " " + result->get_error_code());
		return TypedArray<String>();
	}
	TypedArray<SQLiteColumnSchema> column_schemas;
	for (int i = 0; i < result->get_result().size(); i++) {
		Dictionary row = result->get_result()[i];
		Ref<SQLiteColumnSchema> schema;
		schema.instantiate();
		schema->set_name(row["name"]);
		String data_type = row["type"];
		// data type
		if (data_type == "INTEGER") {
			schema->set_type(Variant::Type::INT);
		} else if (data_type == "REAL") {
			schema->set_type(Variant::Type::FLOAT);
		} else if (data_type == "TEXT") {
			schema->set_type(Variant::Type::STRING);
		} else if (data_type == "BLOB") {
			schema->set_type(Variant::Type::PACKED_BYTE_ARRAY);
		} else {
			schema->set_type(Variant::Type::NIL);
		}
		int not_null = row["notnull"];
		schema->set_not_null(not_null == 1 ? true : false);
		Variant default_value = row["dflt_value"];
		if (!default_value.is_null()) {
			schema->set_default_value(default_value);
		}
		int primary_key = row["pk"];
		schema->set_primary_key(primary_key == 1 ? true : false);
		column_schemas.append(schema);
	}
	return column_schemas;
}

Ref<SQLiteQuery> SQLiteDatabase::create_query(const String &p_query_string, const Array &p_args) {
	return db->create_query(p_query_string, p_args);
}

Ref<SQLiteQueryResult> SQLiteDatabase::execute_query(const String &p_query_string, const Array &p_args) {
	return db->create_query(p_query_string, p_args)->execute(Array());
}

String SQLiteDatabase::get_last_error_message() const {
	return db->get_last_error_message();
}
int SQLiteDatabase::get_last_error_code() const {
	return db->get_last_error_code();
}

Dictionary SQLiteDatabase::get_tables() const {
	Ref<SQLiteQuery> query = db->create_query("SELECT name FROM sqlite_master WHERE type = \"table\"");
	Ref<SQLiteQueryResult> result = query->execute(Array());
	ERR_FAIL_COND_V_MSG(result->get_error() != "", Dictionary(), "Error getting table names: " + result->get_error() + " " + result->get_error_code());

	Dictionary result_dict;
	for (int i = 0; i < result->get_result().size(); i++) {
		Dictionary row = result->get_result()[i];
		for (Variant key : row.keys()) {
			String table_name = row.get("name", String());
			if (!table_name.begins_with("sqlite_")) {
				TypedArray<SQLiteColumnSchema> columns = get_columns(table_name);
				TypedArray<String> column_names;
				for (int k = 0; k < columns.size(); k++) {
					Ref<SQLiteColumnSchema> column = columns[k];
					column_names.append(column->get_name());
				}
				result_dict[table_name] = column_names;
			}
		}
	}
	return result_dict;
}

bool SQLiteDatabase::attach_database(const String &p_path, const String &p_alias) {
	ERR_FAIL_COND_V(db.is_null(), false);
	ProjectSettings *project_settings_singleton = ProjectSettings::get_singleton();
	if (!project_settings_singleton) {
		ERR_FAIL_V_MSG(false, "Cannot get project settings!");
	}
	String real_path = project_settings_singleton->globalize_path(p_path.strip_edges());
	String query_string = "ATTACH DATABASE '" + real_path + "' AS " + p_alias + ";";
	Ref<SQLiteQueryResult> res = execute_query(query_string);
	return res->get_error_code() == 0;
}

bool SQLiteDatabase::detach_database(const String &p_alias) {
	ERR_FAIL_COND_V(db.is_null(), false);
	String query_string = "DETACH DATABASE " + p_alias + ";";
	Ref<SQLiteQueryResult> res = execute_query(query_string);
	return res->get_error_code() == 0;
}

bool SQLiteDatabase::serialize_object(const String &p_table_name, const String &p_key, Object *p_object) {
	ERR_FAIL_NULL_V(p_object, false);
	ERR_FAIL_COND_V(db.is_null(), false);

	String query_string = "CREATE TABLE IF NOT EXISTS " + p_table_name + " (key TEXT PRIMARY KEY, class_name TEXT, properties BLOB);";
	Ref<SQLiteQueryResult> res = execute_query(query_string);
	if (res->get_error_code() != 0) {
		return false;
	}

	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	Dictionary props;
	for (const PropertyInfo &E : plist) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			props[E.name] = p_object->get(E.name);
		}
	}

	PackedByteArray var_bytes = VariantUtilityFunctions::var_to_bytes_with_objects(props);
	String insert_query = "INSERT OR REPLACE INTO " + p_table_name + " (key, class_name, properties) VALUES (?, ?, ?);";
	Array args;
	args.push_back(p_key);
	args.push_back(p_object->get_class());
	args.push_back(var_bytes);

	res = execute_query(insert_query, args);
	return res->get_error_code() == 0;
}

bool SQLiteDatabase::deserialize_object(const String &p_table_name, const String &p_key, Object *p_object) {
	ERR_FAIL_NULL_V(p_object, false);
	ERR_FAIL_COND_V(db.is_null(), false);

	String query_string = "SELECT properties FROM " + p_table_name + " WHERE key = ?;";
	Array args;
	args.push_back(p_key);
	Ref<SQLiteQueryResult> res = execute_query(query_string, args);

	if (res->get_error_code() != 0 || res->get_result().is_empty()) {
		return false;
	}

	Dictionary row = res->get_result()[0];
	Variant prop_var = row.get("properties", Variant());
	if (prop_var.get_type() != Variant::PACKED_BYTE_ARRAY) {
		return false;
	}

	Variant parsed = VariantUtilityFunctions::bytes_to_var_with_objects(prop_var);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary props = parsed;
	for (int i = 0; i < props.keys().size(); i++) {
		StringName k = props.keys()[i];
		p_object->set(k, props[k]);
	}
	return true;
}

Variant SQLiteDatabase::instantiate_object(const String &p_table_name, const String &p_key) {
	ERR_FAIL_COND_V(db.is_null(), Variant());

	String query_string = "SELECT class_name, properties FROM " + p_table_name + " WHERE key = ?;";
	Array args;
	args.push_back(p_key);
	Ref<SQLiteQueryResult> res = execute_query(query_string, args);

	if (res->get_error_code() != 0 || res->get_result().is_empty()) {
		return Variant();
	}

	Dictionary row = res->get_result()[0];
	String class_name = row.get("class_name", "");
	if (class_name.is_empty()) {
		return Variant();
	}

	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		return Variant();
	}

	Variant prop_var = row.get("properties", Variant());
	if (prop_var.get_type() == Variant::PACKED_BYTE_ARRAY) {
		Variant parsed = VariantUtilityFunctions::bytes_to_var_with_objects(prop_var);
		if (parsed.get_type() == Variant::DICTIONARY) {
			Dictionary props = parsed;
			for (int i = 0; i < props.keys().size(); i++) {
				StringName k = props.keys()[i];
				obj->set(k, props[k]);
			}
		}
	}
	// Return the object safely wrapped in a Variant
	return Variant(obj);
}

bool SQLiteDatabase::backup_to(const String &p_path) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->backup(p_path);
}

bool SQLiteDatabase::restore_from(const String &p_path) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->restore(p_path);
}

bool SQLiteDatabase::load_from(const String &p_path) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->open(p_path);
}

int64_t SQLiteDatabase::get_last_insert_rowid() const {
	ERR_FAIL_COND_V(db.is_null(), 0);
	return db->get_last_insert_rowid();
}

int64_t SQLiteDatabase::get_changes() const {
	ERR_FAIL_COND_V(db.is_null(), 0);
	return db->get_changes();
}

int64_t SQLiteDatabase::get_total_changes() const {
	ERR_FAIL_COND_V(db.is_null(), 0);
	return db->get_total_changes();
}

bool SQLiteDatabase::set_busy_timeout(int p_ms) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->set_busy_timeout(p_ms);
}

bool SQLiteDatabase::vacuum() {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->vacuum();
}

void SQLiteDatabase::interrupt() {
	if (db.is_valid()) {
		db->interrupt();
	}
}

void SQLiteDatabase::set_trace(bool p_enabled) {
	if (db.is_valid()) {
		db->set_trace(p_enabled);
	}
}

bool SQLiteDatabase::enable_load_extension(bool p_enabled) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->enable_load_extension(p_enabled);
}

bool SQLiteDatabase::execute_script(const String &p_path) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->execute_script(p_path);
}

bool SQLiteDatabase::is_autocommit() const {
	if (db.is_valid()) {
		return db->is_autocommit();
	}
	return false;
}

bool SQLiteDatabase::is_readonly(const String &p_db_name) const {
	if (db.is_valid()) {
		return db->is_readonly(p_db_name);
	}
	return true;
}

String SQLiteDatabase::get_database_filename(const String &p_db_name) const {
	if (db.is_valid()) {
		return db->get_database_filename(p_db_name);
	}
	return "";
}

bool SQLiteDatabase::wal_checkpoint(const String &p_db_name) {
	if (db.is_valid()) {
		return db->wal_checkpoint(p_db_name);
	}
	return false;
}

bool SQLiteDatabase::create_function(const String &p_name, int p_argc, const Callable &p_callable) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->create_function(p_name, p_argc, p_callable);
}

void SQLiteDatabase::close() {
	if (db.is_valid()) {
		db->close();
	}
}

void SQLiteDatabase::_on_row_updated(int p_op, const String &p_db_name, const String &p_table_name, int64_t p_rowid) {
	emit_signal("row_updated", p_op, p_db_name, p_table_name, p_rowid);
}

void SQLiteDatabase::_on_transaction_committed() {
	emit_signal("transaction_committed");
}

void SQLiteDatabase::_on_transaction_rolled_back() {
	emit_signal("transaction_rolled_back");
}

void SQLiteDatabase::_on_query_progress() {
	emit_signal("query_progress");
}

void SQLiteDatabase::_on_query_profiled(const String &p_query, int64_t p_execution_time_ns) {
	emit_signal("query_profiled", p_query, p_execution_time_ns);
}

void SQLiteDatabase::_on_wal_updated(const String &p_db_name, int p_pages) {
	emit_signal("wal_updated", p_db_name, p_pages);
}

SQLiteDatabase::SQLiteDatabase() {
	db.instantiate();
	db->connect("row_updated", callable_mp(this, &SQLiteDatabase::_on_row_updated));
	db->connect("transaction_committed", callable_mp(this, &SQLiteDatabase::_on_transaction_committed));
	db->connect("transaction_rolled_back", callable_mp(this, &SQLiteDatabase::_on_transaction_rolled_back));
	db->connect("query_progress", callable_mp(this, &SQLiteDatabase::_on_query_progress));
	db->connect("query_profiled", callable_mp(this, &SQLiteDatabase::_on_query_profiled));
	db->connect("wal_updated", callable_mp(this, &SQLiteDatabase::_on_wal_updated));
	db->open_in_memory();
}

SQLiteDatabase::~SQLiteDatabase() {
	if (db.is_valid()) {
		db->disconnect("row_updated", callable_mp(this, &SQLiteDatabase::_on_row_updated));
		db->disconnect("transaction_committed", callable_mp(this, &SQLiteDatabase::_on_transaction_committed));
		db->disconnect("transaction_rolled_back", callable_mp(this, &SQLiteDatabase::_on_transaction_rolled_back));
		db->disconnect("query_progress", callable_mp(this, &SQLiteDatabase::_on_query_progress));
		db->disconnect("query_profiled", callable_mp(this, &SQLiteDatabase::_on_query_profiled));
		db->disconnect("wal_updated", callable_mp(this, &SQLiteDatabase::_on_wal_updated));
		db->close();
	}
	db.unref();
}

bool SQLiteDatabase::create_collation(const String &p_name, const Callable &p_callable) {
	return db->create_collation(p_name, p_callable);
}

bool SQLiteDatabase::create_aggregate(const String &p_name, int p_argc, const Callable &p_step_callable, const Callable &p_final_callable) {
	ERR_FAIL_COND_V(db.is_null(), false);
	return db->create_aggregate(p_name, p_argc, p_step_callable, p_final_callable);
}

void SQLiteDatabase::set_progress_handler(int p_instructions) {
	db->set_progress_handler(p_instructions);
}

void SQLiteDatabase::set_authorizer(const Callable &p_callable) {
	if (db.is_valid()) {
		db->set_authorizer(p_callable);
	}
}

int SQLiteDatabase::set_limit(int p_id, int p_new_val) {
	return db->set_limit(p_id, p_new_val);
}

int SQLiteDatabase::get_limit(int p_id) const {
	return db->get_limit(p_id);
}

bool SQLiteDatabase::set_db_config(int p_op, int p_val) {
	return db->set_db_config(p_op, p_val);
}

int SQLiteDatabase::get_db_config(int p_op) const {
	return db->get_db_config(p_op);
}

void SQLiteDatabase::set_foreign_keys_enabled(bool p_enabled) {
	db->set_foreign_keys_enabled(p_enabled);
}

int SQLiteDatabase::get_db_status(int p_op, bool p_reset) const {
	return db->get_db_status(p_op, p_reset);
}

int SQLiteDatabase::get_global_status(int p_op, bool p_reset) {
	return SQLiteAccess::get_global_status(p_op, p_reset);
}

int SQLiteDatabase::release_memory(int p_bytes) {
	return SQLiteAccess::release_memory(p_bytes);
}

void SQLiteDatabase::set_soft_heap_limit(int64_t p_bytes) {
	SQLiteAccess::set_soft_heap_limit(p_bytes);
}
