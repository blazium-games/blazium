/**************************************************************************/
/*  dotcsv.h                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Portions inspired by CSV-Parser, Copyright (c) 2026 Shadow9876.        */
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

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class CSVDialect;

class DotCSVOptions {
public:
	enum RowLengthMode {
		ROW_LENGTH_STRICT,
		ROW_LENGTH_PAD_SHORT,
		ROW_LENGTH_PAD_OR_TRUNCATE,
		ROW_LENGTH_JAGGED,
	};
};

class DSVImporter : public RefCounted {
	GDCLASS(DSVImporter, RefCounted);

protected:
	static void _bind_methods();

public:
	static Array import_file(const String &p_file_path, const String &p_delimiter = ",", const Dictionary &p_options = Dictionary());
	static Array import_string(const String &p_string, const String &p_delimiter = ",", const Dictionary &p_options = Dictionary());
};

class DSVExporter : public RefCounted {
	GDCLASS(DSVExporter, RefCounted);

protected:
	static void _bind_methods();

public:
	static bool export_file(const Array &p_data, const String &p_file_path, const String &p_delimiter = ",");
	static String export_string(const Array &p_data, const String &p_delimiter = ",");
};

class CSVDialect : public RefCounted {
	GDCLASS(CSVDialect, RefCounted);

	String delimiter = ",";
	bool headers = false;
	bool cast_fields = true;
	Dictionary options;

protected:
	static void _bind_methods();

public:
	static Ref<CSVDialect> from_profile(const String &p_name);
	static Ref<CSVDialect> from_dictionary(const Dictionary &p_data);
	static Ref<CSVDialect> sniff(const String &p_sample, const Dictionary &p_options = Dictionary());

	void set_delimiter(const String &p_delimiter);
	String get_delimiter() const;
	void set_headers(bool p_headers);
	bool get_headers() const;
	void set_cast_fields(bool p_cast_fields);
	bool get_cast_fields() const;
	void set_options(const Dictionary &p_options);
	Dictionary get_options() const;
	Dictionary to_dictionary() const;
	Array import_string(const String &p_string) const;
	Array import_file(const String &p_path) const;
};

class CSVImporter : public RefCounted {
	GDCLASS(CSVImporter, RefCounted);

protected:
	static void _bind_methods();

public:
	static Array import_file(const String &p_file_path, const String &p_delimiter = ",", bool p_cast_fields_to_same_type = true, const Dictionary &p_options = Dictionary());
	static Array import_string(const String &p_string, const String &p_delimiter = ",", bool p_cast_fields_to_same_type = true, const Dictionary &p_options = Dictionary());
	static Array import_with_headers(const String &p_file_path, const String &p_delimiter = ",", bool p_cast_fields_to_same_type = true, const Dictionary &p_options = Dictionary());
	static Array import_string_with_headers(const String &p_string, const String &p_delimiter = ",", bool p_cast_fields_to_same_type = true, const Dictionary &p_options = Dictionary());
	static Array import_with_dialect(const String &p_file_path, const Ref<CSVDialect> &p_dialect);
	static Array import_string_with_dialect(const String &p_string, const Ref<CSVDialect> &p_dialect);
	static Variant parse_record(const String &p_record, const Dictionary &p_options = Dictionary());
};

class CSVExporter : public RefCounted {
	GDCLASS(CSVExporter, RefCounted);

protected:
	static void _bind_methods();

public:
	static bool export_file(const Array &p_data, const String &p_file_path, const String &p_delimiter = ",", bool p_color_as_hex = false, bool p_hex_include_alpha = false);
	static String export_string(const Array &p_data, const String &p_delimiter = ",", bool p_color_as_hex = false, bool p_hex_include_alpha = false);
	static bool export_with_headers(const Array &p_data, const String &p_file_path, const String &p_delimiter = ",", bool p_color_as_hex = false, bool p_hex_include_alpha = false);
	static String export_string_with_headers(const Array &p_data, const String &p_delimiter = ",", bool p_color_as_hex = false, bool p_hex_include_alpha = false);
	static String stringify(const Variant &p_value, bool p_color_as_hex = false, bool p_hex_include_alpha = false);
};

class DSVReader : public RefCounted {
	GDCLASS(DSVReader, RefCounted);

	Ref<FileAccess> file;
	void *parser = nullptr;
	String delimiter = ",";
	Dictionary options;
	Error last_error = OK;
	bool finished = true;

protected:
	static void _bind_methods();

public:
	Error open(const String &p_file_path, const String &p_delimiter = ",", const Dictionary &p_options = Dictionary());
	PackedStringArray read_row();
	Array read_rows(int p_max_rows);
	bool is_eof() const;
	Error get_error() const;
	int get_line() const;
	uint64_t get_position() const;
	uint64_t get_length() const;
	void close();

	~DSVReader();
};

class CSVReader : public RefCounted {
	GDCLASS(CSVReader, RefCounted);

	Ref<DSVReader> reader;
	bool headers = false;
	bool cast_fields = true;
	PackedStringArray header_names;
	Dictionary options;
	bool header_loaded = false;

	Array _convert_row(const PackedStringArray &p_row) const;

protected:
	static void _bind_methods();

public:
	void set_headers(bool p_headers);
	bool get_headers() const;
	void set_cast_fields(bool p_cast_fields);
	bool get_cast_fields() const;
	PackedStringArray get_header_names() const;

	Error open(const String &p_file_path, const String &p_delimiter = ",", bool p_headers = false, bool p_cast_fields = true, const Dictionary &p_options = Dictionary());
	Error open_with_dialect(const String &p_file_path, const Ref<CSVDialect> &p_dialect);
	Variant read_row();
	Array read_rows(int p_max_rows);
	bool is_eof() const;
	Error get_error() const;
	int get_line() const;
	uint64_t get_position() const;
	uint64_t get_length() const;
	void close();
};

class DSVWriter : public RefCounted {
	GDCLASS(DSVWriter, RefCounted);

	Ref<FileAccess> file;
	String delimiter = ",";
	Error last_error = OK;

protected:
	static void _bind_methods();

public:
	Error open(const String &p_file_path, const String &p_delimiter = ",", bool p_append = false);
	bool write_row(const Variant &p_row);
	bool write_rows(const Array &p_rows);
	Error get_error() const;
	void close();
};

class CSVWriter : public RefCounted {
	GDCLASS(CSVWriter, RefCounted);

	Ref<DSVWriter> writer;
	bool headers = false;
	bool color_as_hex = false;
	bool hex_include_alpha = false;
	bool wrote_headers = false;
	PackedStringArray header_names;

	PackedStringArray _row_to_strings(const Variant &p_row);

protected:
	static void _bind_methods();

public:
	void set_headers(bool p_headers);
	bool get_headers() const;
	void set_header_names(const PackedStringArray &p_header_names);
	PackedStringArray get_header_names() const;
	void set_color_as_hex(bool p_color_as_hex);
	bool get_color_as_hex() const;
	void set_hex_include_alpha(bool p_hex_include_alpha);
	bool get_hex_include_alpha() const;

	Error open(const String &p_file_path, const String &p_delimiter = ",", bool p_append = false);
	bool write_row(const Variant &p_row);
	bool write_rows(const Array &p_rows);
	Error get_error() const;
	void close();
};

class CSV : public Resource {
	GDCLASS(CSV, Resource);

	TypedArray<Dictionary> rows;
	bool headers = false;
	String delimiter = ",";
	bool trim_fields = false;
	bool skip_empty_rows = false;
	PackedStringArray comment_prefixes;
	int row_length_mode = DotCSVOptions::ROW_LENGTH_STRICT;
	PackedStringArray null_values;
	PackedStringArray true_values;
	PackedStringArray false_values;

protected:
	static void _bind_methods();

public:
	void set_headers(bool p_headers);
	bool get_headers() const;

	void set_delimiter(const String &p_delimiter);
	String get_delimiter() const;

	void set_trim_fields(bool p_trim_fields);
	bool get_trim_fields() const;

	void set_skip_empty_rows(bool p_skip_empty_rows);
	bool get_skip_empty_rows() const;

	void set_comment_prefixes(const PackedStringArray &p_comment_prefixes);
	PackedStringArray get_comment_prefixes() const;

	void set_row_length_mode(int p_row_length_mode);
	int get_row_length_mode() const;

	void set_null_values(const PackedStringArray &p_null_values);
	PackedStringArray get_null_values() const;

	void set_true_values(const PackedStringArray &p_true_values);
	PackedStringArray get_true_values() const;

	void set_false_values(const PackedStringArray &p_false_values);
	PackedStringArray get_false_values() const;

	void set_rows(const TypedArray<Dictionary> &p_rows);
	TypedArray<Dictionary> get_rows() const;

	Dictionary get_parser_options() const;
	Variant convert_to_variant(const String &p_text) const;
	Error load_file(const String &p_path);
	Error load_string(const String &p_string);
	String save_to_string() const;

	CSV();
};

class CSVTable;

class CSVRowModel : public RefCounted {
	GDCLASS(CSVRowModel, RefCounted);

	Dictionary schema;

protected:
	static void _bind_methods();

public:
	static Ref<CSVRowModel> from_schema(const Dictionary &p_schema);
	static Ref<CSVRowModel> from_file(const String &p_path);
	static Ref<CSVRowModel> infer_from_rows(const Array &p_rows, const Dictionary &p_options = Dictionary());
	static Ref<CSVRowModel> infer_from_table(const Ref<CSVTable> &p_table, const Dictionary &p_options = Dictionary());

	void set_schema(const Dictionary &p_schema);
	Dictionary get_schema() const;
	Error save_schema(const String &p_path) const;
	Error load_schema(const String &p_path);
	PackedStringArray get_field_names() const;
	Dictionary cast_row(const Variant &p_row) const;
	Array cast_rows(const Array &p_rows) const;
	Dictionary validate_row(const Variant &p_row) const;
	Array validate_rows(const Array &p_rows) const;
	Dictionary serialize_row(const Variant &p_row, bool p_include_defaults = true) const;
	Array serialize_rows(const Array &p_rows, bool p_include_defaults = true) const;
};

class CSVChunkProcessor : public RefCounted {
	GDCLASS(CSVChunkProcessor, RefCounted);

protected:
	static void _bind_methods();

public:
	static Error process_file(const String &p_path, const Callable &p_callable, int p_chunk_size = 1000, const Ref<CSVDialect> &p_dialect = Ref<CSVDialect>());
	static Array sample_file(const String &p_path, int p_count = 10, const Ref<CSVDialect> &p_dialect = Ref<CSVDialect>());
	static PackedStringArray split_file(const String &p_path, const String &p_output_dir, int p_rows_per_file, const Ref<CSVDialect> &p_dialect = Ref<CSVDialect>(), bool p_include_headers = true);
	static Error merge_files(const PackedStringArray &p_paths, const String &p_output_path, const Ref<CSVDialect> &p_dialect = Ref<CSVDialect>(), bool p_include_headers = true);
	static int count_rows(const String &p_path, const Ref<CSVDialect> &p_dialect = Ref<CSVDialect>());
};

class CSVAsyncTask : public RefCounted {
	GDCLASS(CSVAsyncTask, RefCounted);

public:
	enum Operation {
		OP_LOAD_CSV,
		OP_LOAD_DSV,
		OP_SAVE_CSV,
		OP_SAVE_DSV,
	};

private:
	Thread thread;
	mutable Mutex mutex;
	bool thread_started = false;
	bool running = false;
	bool done = false;
	bool cancel_requested = false;
	bool cancelled = false;
	double progress = 0.0;
	String error;
	Array result_rows;

	Operation operation = OP_LOAD_CSV;
	String path;
	String delimiter = ",";
	bool headers = false;
	bool cast_fields = true;
	Dictionary options;
	Array input_rows;

	static void _thread_func(void *p_userdata);
	void _run();
	void _set_progress(double p_progress);
	void _finish_success(const Array &p_rows = Array());
	void _finish_failed(const String &p_error);
	void _finish_cancelled();
	bool _is_cancel_requested() const;

protected:
	static void _bind_methods();

public:
	static Ref<CSVAsyncTask> load_csv(const String &p_path, const String &p_delimiter = ",", bool p_headers = false, bool p_cast_fields = true, const Dictionary &p_options = Dictionary());
	static Ref<CSVAsyncTask> load_dsv(const String &p_path, const String &p_delimiter = ",", const Dictionary &p_options = Dictionary());
	static Ref<CSVAsyncTask> save_csv(const String &p_path, const Array &p_rows, const String &p_delimiter = ",", bool p_headers = false, const Dictionary &p_options = Dictionary());
	static Ref<CSVAsyncTask> save_dsv(const String &p_path, const Array &p_rows, const String &p_delimiter = ",");

	Error start();
	void cancel();
	bool is_running() const;
	bool is_done() const;
	bool is_cancelled() const;
	String get_error() const;
	double get_progress() const;
	Array get_rows() const;
	Ref<CSVTable> get_table() const;
	void wait_to_finish();

	~CSVAsyncTask();
};

class CSVIndex : public RefCounted {
	GDCLASS(CSVIndex, RefCounted);

	StringName column;
	bool unique = false;
	Dictionary index;
	PackedStringArray duplicate_key_names;

protected:
	static void _bind_methods();

public:
	void build(const Array &p_rows, const StringName &p_column, bool p_unique = false);
	Variant get_row(const Variant &p_value) const;
	Array get_all(const Variant &p_value) const;
	bool has(const Variant &p_value) const;
	bool is_unique() const;
	PackedStringArray duplicate_keys() const;
	StringName get_column() const;
};

class CSVTable : public RefCounted {
	GDCLASS(CSVTable, RefCounted);

	Array rows;

	static Ref<CSVTable> _from_array(const Array &p_rows);

protected:
	static void _bind_methods();

public:
	static Ref<CSVTable> from_rows(const Array &p_rows);
	static Ref<CSVTable> from_csv(const Ref<CSV> &p_csv);
	static Ref<CSVTable> from_file(const String &p_path, const String &p_delimiter = ",", const Dictionary &p_options = Dictionary());
	static Ref<CSVTable> from_file_with_dialect(const String &p_path, const Ref<CSVDialect> &p_dialect);

	void set_rows(const Array &p_rows);
	Array get_rows() const;
	int row_count() const;
	PackedStringArray column_names() const;
	Ref<CSVTable> where_equals(const StringName &p_column, const Variant &p_value) const;
	Ref<CSVTable> where_in(const StringName &p_column, const Array &p_values) const;
	Ref<CSVTable> filter_rows(const Callable &p_predicate) const;
	Ref<CSVTable> map_rows(const Callable &p_transform) const;
	Ref<CSVTable> sort_by(const StringName &p_column, bool p_ascending = true) const;
	Ref<CSVTable> sort_by_columns(const PackedStringArray &p_columns, bool p_ascending = true) const;
	Ref<CSVTable> limit(int p_count, int p_offset = 0) const;
	Ref<CSVTable> distinct(const PackedStringArray &p_columns = PackedStringArray()) const;
	Ref<CSVTable> with_column(const StringName &p_column, const Variant &p_value_or_callable) const;
	Ref<CSVTable> drop_columns(const PackedStringArray &p_columns) const;
	Ref<CSVTable> select_columns(const PackedStringArray &p_columns) const;
	Ref<CSVTable> rename_columns(const Dictionary &p_map) const;
	Ref<CSVTable> apply_model(const Ref<CSVRowModel> &p_model) const;
	Array validate_model(const Ref<CSVRowModel> &p_model) const;
	Ref<CSVIndex> build_index(const StringName &p_column, bool p_unique = false) const;
	Dictionary group_by(const StringName &p_column) const;
	Ref<CSVTable> inner_join(const Ref<CSVTable> &p_other, const StringName &p_left_column, const StringName &p_right_column) const;
	Ref<CSVTable> left_join(const Ref<CSVTable> &p_other, const StringName &p_left_column, const StringName &p_right_column) const;
};
