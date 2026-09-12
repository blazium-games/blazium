/**************************************************************************/
/*  dotcsv.cpp                                                            */
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

#include "dotcsv.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/color.h"
#include "core/object/class_db.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant_parser.h"
#include "core/variant/variant_utility.h"

namespace {

enum class LineEnd {
	CRLF,
	LF,
	CR,
	UNKNOWN,
};

bool _validate_delimiter(const String &p_delimiter) {
	ERR_FAIL_COND_V_MSG(p_delimiter.length() != 1, false, vformat("'%s' is not a valid DSV delimiter. DSV delimiters must be one character long.", p_delimiter));
	const char32_t delimiter = p_delimiter[0];
	ERR_FAIL_COND_V_MSG(delimiter == '"' || delimiter == '\r' || delimiter == '\n', false, "DSV delimiters must not be quotes or line breaks.");
	return true;
}

Array _read_text_file(const String &p_file_path, String &r_text) {
	Array empty;
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), empty, vformat("Opening file '%s' failed. Reason: %s.", p_file_path, error_names[err]));
	r_text = file->get_as_text();
	return empty;
}

bool _is_all_spaces(const String &p_text) {
	for (int i = 0; i < p_text.length(); i++) {
		if (p_text[i] != ' ') {
			return false;
		}
	}
	return true;
}

struct DotCSVParseOptions {
	bool trim_fields = false;
	bool skip_empty_rows = false;
	int row_length_mode = DotCSVOptions::ROW_LENGTH_STRICT;
	PackedStringArray comment_prefixes;
	PackedStringArray null_values;
	PackedStringArray true_values;
	PackedStringArray false_values;

	DotCSVParseOptions() {
		null_values.push_back("");
		true_values.push_back("true");
		true_values.push_back("True");
		true_values.push_back("TRUE");
		false_values.push_back("false");
		false_values.push_back("False");
		false_values.push_back("FALSE");
	}
};

PackedStringArray _variant_to_string_array(const Variant &p_value, const PackedStringArray &p_default = PackedStringArray()) {
	if (p_value.get_type() == Variant::NIL) {
		return p_default;
	}
	if (p_value.get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_value;
	}

	PackedStringArray out;
	if (p_value.get_type() == Variant::ARRAY) {
		Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			out.push_back(String(array[i]));
		}
		return out;
	}
	if (p_value.get_type() == Variant::STRING) {
		String value = p_value;
		if (value.is_empty()) {
			return out;
		}
		Vector<String> parts = value.split(",", false);
		for (int i = 0; i < parts.size(); i++) {
			out.push_back(parts[i].strip_edges());
		}
		return out;
	}

	out.push_back(String(p_value));
	return out;
}

int _row_length_mode_from_variant(const Variant &p_value, int p_default) {
	if (p_value.get_type() == Variant::STRING) {
		String value = String(p_value).to_snake_case().to_lower();
		if (value == "strict") {
			return DotCSVOptions::ROW_LENGTH_STRICT;
		}
		if (value == "pad_short") {
			return DotCSVOptions::ROW_LENGTH_PAD_SHORT;
		}
		if (value == "pad_or_truncate") {
			return DotCSVOptions::ROW_LENGTH_PAD_OR_TRUNCATE;
		}
		if (value == "jagged") {
			return DotCSVOptions::ROW_LENGTH_JAGGED;
		}
		return p_default;
	}
	return CLAMP((int)p_value, DotCSVOptions::ROW_LENGTH_STRICT, DotCSVOptions::ROW_LENGTH_JAGGED);
}

DotCSVParseOptions _parse_options(const Dictionary &p_options) {
	DotCSVParseOptions options;
	if (p_options.has("trim_fields")) {
		options.trim_fields = (bool)p_options["trim_fields"];
	}
	if (p_options.has("skip_empty_rows")) {
		options.skip_empty_rows = (bool)p_options["skip_empty_rows"];
	}
	if (p_options.has("comment_prefixes")) {
		options.comment_prefixes = _variant_to_string_array(p_options["comment_prefixes"]);
	}
	if (p_options.has("row_length_mode")) {
		options.row_length_mode = _row_length_mode_from_variant(p_options["row_length_mode"], options.row_length_mode);
	}
	if (p_options.has("null_values")) {
		options.null_values = _variant_to_string_array(p_options["null_values"]);
	}
	if (p_options.has("true_values")) {
		options.true_values = _variant_to_string_array(p_options["true_values"]);
	}
	if (p_options.has("false_values")) {
		options.false_values = _variant_to_string_array(p_options["false_values"]);
	}
	return options;
}

bool _dialect_value_looks_typed(const String &p_value) {
	Variant parsed = CSVImporter::parse_record(p_value);
	return parsed.get_type() != Variant::STRING && parsed.get_type() != Variant::NIL;
}

bool _dialect_first_row_looks_like_headers(const Array &p_rows) {
	if (p_rows.size() < 2) {
		return false;
	}
	PackedStringArray first = p_rows[0];
	if (first.is_empty()) {
		return false;
	}
	for (int i = 0; i < first.size(); i++) {
		String value = first[i].strip_edges();
		if (value.is_empty() || _dialect_value_looks_typed(value)) {
			return false;
		}
	}
	for (int row_idx = 1; row_idx < p_rows.size(); row_idx++) {
		PackedStringArray row = p_rows[row_idx];
		for (int col_idx = 0; col_idx < row.size(); col_idx++) {
			if (_dialect_value_looks_typed(row[col_idx].strip_edges())) {
				return true;
			}
		}
	}
	return false;
}

int _dialect_score_rows(const Array &p_rows) {
	Dictionary counts;
	int rows_with_delimiter = 0;
	for (int i = 0; i < p_rows.size(); i++) {
		PackedStringArray row = p_rows[i];
		if (row.size() < 2) {
			continue;
		}
		rows_with_delimiter++;
		int count = counts.has(row.size()) ? int(counts[row.size()]) : 0;
		counts[row.size()] = count + 1;
	}
	int best_count = 0;
	Array keys = counts.keys();
	for (int i = 0; i < keys.size(); i++) {
		best_count = MAX(best_count, int(counts[keys[i]]));
	}
	return best_count * 100 + rows_with_delimiter;
}

Ref<CSVDialect> _chunk_get_dialect(const Ref<CSVDialect> &p_dialect) {
	if (p_dialect.is_valid()) {
		return p_dialect;
	}
	return CSVDialect::from_profile("csv");
}

Error _chunk_open_reader(Ref<CSVReader> &r_reader, const String &p_path, const Ref<CSVDialect> &p_dialect) {
	r_reader.instantiate();
	Ref<CSVDialect> dialect = _chunk_get_dialect(p_dialect);
	return r_reader->open_with_dialect(p_path, dialect);
}

void _chunk_configure_writer(const Ref<CSVWriter> &p_writer, const Ref<CSVDialect> &p_dialect, bool p_include_headers, const PackedStringArray &p_header_names) {
	Ref<CSVDialect> dialect = _chunk_get_dialect(p_dialect);
	p_writer->set_headers(p_include_headers && dialect->get_headers());
	if (!p_header_names.is_empty()) {
		p_writer->set_header_names(p_header_names);
	}
}

String _chunk_output_path(const String &p_source_path, const String &p_output_dir, int p_part_index) {
	String source_name = p_source_path.get_file();
	String base = source_name.get_basename();
	String extension = source_name.get_extension();
	String file_name = extension.is_empty() ? vformat("%s_part_%03d", base, p_part_index) : vformat("%s_part_%03d.%s", base, p_part_index, extension);
	return p_output_dir.path_join(file_name);
}

bool _string_array_has(const PackedStringArray &p_values, const String &p_value) {
	for (int i = 0; i < p_values.size(); i++) {
		if (p_values[i] == p_value) {
			return true;
		}
	}
	return false;
}

void _push_completed_record(PackedStringArray &r_records, String &r_buffer, bool &r_ignore_spaces, bool &r_record_quoted, const DotCSVParseOptions &p_options) {
	r_records.push_back((p_options.trim_fields && !r_record_quoted) ? r_buffer.strip_edges() : r_buffer);
	r_ignore_spaces = false;
	r_record_quoted = false;
	r_buffer = "";
}

bool _row_is_empty(const PackedStringArray &p_records) {
	for (int i = 0; i < p_records.size(); i++) {
		if (!p_records[i].is_empty()) {
			return false;
		}
	}
	return true;
}

bool _row_is_comment(const PackedStringArray &p_records, const DotCSVParseOptions &p_options) {
	if (p_records.is_empty() || p_options.comment_prefixes.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_options.comment_prefixes.size(); i++) {
		const String prefix = p_options.comment_prefixes[i];
		if (!prefix.is_empty() && p_records[0].begins_with(prefix)) {
			return true;
		}
	}
	return false;
}

bool _push_completed_line(Array &r_out, PackedStringArray &r_records, String &r_buffer, bool &r_ignore_spaces, bool &r_record_quoted, int &r_record_len, const DotCSVParseOptions &p_options) {
	_push_completed_record(r_records, r_buffer, r_ignore_spaces, r_record_quoted, p_options);

	if ((p_options.skip_empty_rows && _row_is_empty(r_records)) || _row_is_comment(r_records, p_options)) {
		r_records = PackedStringArray();
		return true;
	}

	if (p_options.row_length_mode != DotCSVOptions::ROW_LENGTH_JAGGED) {
		if (r_record_len < 0) {
			r_record_len = r_records.size();
		}
		if (r_records.size() < r_record_len) {
			if (p_options.row_length_mode == DotCSVOptions::ROW_LENGTH_STRICT) {
				ERR_PRINT("The number of DSV records in a line does not match previous lines.");
				return false;
			}
			while (r_records.size() < r_record_len) {
				r_records.push_back("");
			}
		} else if (r_records.size() > r_record_len) {
			if (p_options.row_length_mode == DotCSVOptions::ROW_LENGTH_PAD_OR_TRUNCATE) {
				r_records.resize(r_record_len);
			} else {
				ERR_PRINT("The number of DSV records in a line does not match previous lines.");
				return false;
			}
		}
	}

	r_out.push_back(r_records);
	r_records = PackedStringArray();
	return true;
}

class DotCSVStreamParser {
	DotCSVParseOptions options;
	char32_t delimiter = ',';
	String buffer;
	PackedStringArray records;
	Array rows;
	int record_len = -1;
	int line = 1;
	bool inside_quotes = false;
	bool ignore_spaces = false;
	bool record_quoted = false;
	bool pending_cr = false;
	bool pending_quote = false;
	bool finished = false;
	bool error = false;

	bool _complete_line() {
		if (!_push_completed_line(rows, records, buffer, ignore_spaces, record_quoted, record_len, options)) {
			error = true;
			return false;
		}
		line++;
		return true;
	}

public:
	DotCSVStreamParser(const String &p_delimiter, const Dictionary &p_options) {
		options = _parse_options(p_options);
		delimiter = p_delimiter[0];
	}

	bool push_char(char32_t p_char) {
		if (finished || error) {
			return !error;
		}

		if (pending_cr) {
			pending_cr = false;
			if (p_char == '\n') {
				return true;
			}
		}

		if (pending_quote) {
			pending_quote = false;
			if (p_char == '"') {
				buffer += String::chr('"');
				return true;
			}
			inside_quotes = false;
			ignore_spaces = true;
		}

		if (p_char == '"') {
			if (!inside_quotes) {
				if (buffer.is_empty()) {
					inside_quotes = true;
					record_quoted = true;
					return true;
				}
				if (_is_all_spaces(buffer)) {
					buffer = "";
					inside_quotes = true;
					record_quoted = true;
					return true;
				}
			} else {
				pending_quote = true;
				return true;
			}
		}

		if (p_char == delimiter && !inside_quotes) {
			_push_completed_record(records, buffer, ignore_spaces, record_quoted, options);
			return true;
		}

		if ((p_char == '\r' || p_char == '\n') && !inside_quotes) {
			if (!_complete_line()) {
				return false;
			}
			if (p_char == '\r') {
				pending_cr = true;
			}
			return true;
		}

		if (ignore_spaces && p_char == ' ') {
			return true;
		}

		buffer += String::chr(p_char);
		return true;
	}

	bool finish() {
		if (finished || error) {
			return !error;
		}
		finished = true;
		pending_cr = false;
		if (pending_quote) {
			pending_quote = false;
			inside_quotes = false;
			ignore_spaces = true;
		}
		if (inside_quotes) {
			ERR_PRINT("Unclosed quoted DSV record.");
			error = true;
			return false;
		}
		if (!buffer.is_empty() || !records.is_empty()) {
			return _complete_line();
		}
		return true;
	}

	bool has_rows() const { return !rows.is_empty(); }
	bool has_error() const { return error; }
	bool is_finished() const { return finished; }
	int get_line() const { return line; }

	PackedStringArray pop_row() {
		PackedStringArray row;
		if (rows.is_empty()) {
			return row;
		}
		row = rows[0];
		rows.remove_at(0);
		return row;
	}

	Array take_rows() {
		Array out = rows.duplicate();
		rows.clear();
		return out;
	}
};

PackedStringArray _variant_to_dsv_row(const Variant &p_row, bool p_stringify_variants, bool p_color_as_hex, bool p_hex_include_alpha) {
	PackedStringArray out;
	if (p_row.get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_row;
	}
	if (p_row.get_type() == Variant::ARRAY) {
		Array row = p_row;
		for (int i = 0; i < row.size(); i++) {
			out.push_back(p_stringify_variants ? CSVExporter::stringify(row[i], p_color_as_hex, p_hex_include_alpha) : String(row[i]));
		}
		return out;
	}
	if (p_row.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_row;
		Array keys = dict.keys();
		keys.sort();
		for (int i = 0; i < keys.size(); i++) {
			out.push_back(p_stringify_variants ? CSVExporter::stringify(dict[keys[i]], p_color_as_hex, p_hex_include_alpha) : String(dict[keys[i]]));
		}
		return out;
	}
	out.push_back(p_stringify_variants ? CSVExporter::stringify(p_row, p_color_as_hex, p_hex_include_alpha) : String(p_row));
	return out;
}

Array _to_string_rows(const Array &p_data, bool p_stringify_variants, bool p_color_as_hex, bool p_hex_include_alpha) {
	Array out;
	for (int i = 0; i < p_data.size(); i++) {
		PackedStringArray line_out;
		Variant line_variant = p_data[i];
		if (line_variant.get_type() == Variant::PACKED_STRING_ARRAY) {
			line_out = line_variant;
		} else {
			Array line = line_variant;
			for (int j = 0; j < line.size(); j++) {
				line_out.push_back(p_stringify_variants ? CSVExporter::stringify(line[j], p_color_as_hex, p_hex_include_alpha) : String(line[j]));
			}
		}
		out.push_back(line_out);
	}
	return out;
}

Variant::Type _compare_types(Variant::Type p_a, Variant::Type p_b) {
	if (p_a == p_b) {
		return p_a;
	}

	if ((p_a == Variant::INT && p_b == Variant::FLOAT) || (p_a == Variant::FLOAT && p_b == Variant::INT)) {
		return Variant::FLOAT;
	}
	if ((p_a == Variant::VECTOR2 && p_b == Variant::VECTOR2I) || (p_a == Variant::VECTOR2I && p_b == Variant::VECTOR2)) {
		return Variant::VECTOR2;
	}
	if ((p_a == Variant::VECTOR3 && p_b == Variant::VECTOR3I) || (p_a == Variant::VECTOR3I && p_b == Variant::VECTOR3)) {
		return Variant::VECTOR3;
	}
	if ((p_a == Variant::VECTOR4 && p_b == Variant::VECTOR4I) || (p_a == Variant::VECTOR4I && p_b == Variant::VECTOR4)) {
		return Variant::VECTOR4;
	}
	if (p_a == Variant::NIL) {
		return p_b;
	}
	if (p_b == Variant::NIL) {
		return p_a;
	}

	return Variant::STRING;
}

Variant _cast_variant(Variant::Type p_old_type, Variant::Type p_type, const Variant &p_value, const String &p_record) {
	if (p_old_type == p_type) {
		return p_value;
	}
	if (p_type == Variant::STRING) {
		return p_record;
	}
	if (p_value.get_type() == Variant::NIL) {
		return p_value;
	}

	switch (p_type) {
		case Variant::FLOAT:
			return (double)p_value;
		case Variant::VECTOR2:
			return (Vector2)p_value;
		case Variant::VECTOR3:
			return (Vector3)p_value;
		case Variant::VECTOR4:
			return (Vector4)p_value;
		default:
			return p_value;
	}
}

Variant _parse_engine_variant(const String &p_record, bool &r_ok) {
	Variant ret;
	String err_str;
	int err_line = 0;
	VariantParser::StreamString stream;
	stream.s = p_record;
	r_ok = VariantParser::parse(&stream, ret, err_str, err_line) == OK;
	return ret;
}

} // namespace

Array DSVImporter::import_file(const String &p_file_path, const String &p_delimiter, const Dictionary &p_options) {
	String text;
	Array empty = _read_text_file(p_file_path, text);
	if (text.is_empty() && !FileAccess::exists(p_file_path)) {
		return empty;
	}
	return import_string(text, p_delimiter, p_options);
}

Array DSVImporter::import_string(const String &p_string, const String &p_delimiter, const Dictionary &p_options) {
	Array out;
	if (!_validate_delimiter(p_delimiter)) {
		return out;
	}

	DotCSVStreamParser parser(p_delimiter, p_options);
	for (int i = 0; i < p_string.length(); i++) {
		if (!parser.push_char(p_string[i])) {
			return Array();
		}
	}
	if (!parser.finish()) {
		return Array();
	}
	return parser.take_rows();
}

void DSVImporter::_bind_methods() {
	ClassDB::bind_static_method("DSVImporter", D_METHOD("import", "file_path", "delimiter", "options"), &DSVImporter::import_file, DEFVAL(","), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("DSVImporter", D_METHOD("import_string", "string", "delimiter", "options"), &DSVImporter::import_string, DEFVAL(","), DEFVAL(Dictionary()));
}

bool DSVExporter::export_file(const Array &p_data, const String &p_file_path, const String &p_delimiter) {
	if (!_validate_delimiter(p_delimiter)) {
		return false;
	}

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), false, vformat("Opening file '%s' failed. Reason: %s.", p_file_path, error_names[err]));
	file->store_string(export_string(p_data, p_delimiter));
	return true;
}

String DSVExporter::export_string(const Array &p_data, const String &p_delimiter) {
	if (!_validate_delimiter(p_delimiter)) {
		return "";
	}

	Array out = _to_string_rows(p_data, false, false, false);
	int record_len = 0;
	for (int i = 0; i < out.size(); i++) {
		PackedStringArray line = out[i];
		record_len = MAX(record_len, line.size());
	}

	String buffer;
	for (int i = 0; i < out.size(); i++) {
		PackedStringArray line = out[i];
		while (line.size() < record_len) {
			line.push_back("");
		}
		for (int j = 0; j < line.size(); j++) {
			String record = line[j].replace("\"", "\"\"");
			if (record.contains(p_delimiter) || record.contains("\"") || record.contains("\r") || record.contains("\n")) {
				record = "\"" + record + "\"";
			}
			line.set(j, record);
		}
		buffer += String(p_delimiter).join(line) + "\r\n";
	}
	return buffer;
}

void DSVExporter::_bind_methods() {
	ClassDB::bind_static_method("DSVExporter", D_METHOD("export", "data", "file_path", "delimiter"), &DSVExporter::export_file, DEFVAL(","));
	ClassDB::bind_static_method("DSVExporter", D_METHOD("export_string", "data", "delimiter"), &DSVExporter::export_string, DEFVAL(","));
}

Ref<CSVDialect> CSVDialect::from_profile(const String &p_name) {
	Ref<CSVDialect> dialect;
	dialect.instantiate();
	String profile = p_name.to_snake_case().to_lower();
	if (profile == "tsv") {
		dialect->set_delimiter("\t");
	} else if (profile == "excel") {
		dialect->set_headers(true);
		Dictionary options;
		options["trim_fields"] = true;
		options["skip_empty_rows"] = true;
		dialect->set_options(options);
	} else if (profile == "pipes" || profile == "pipe") {
		dialect->set_delimiter("|");
	} else {
		dialect->set_delimiter(",");
	}
	return dialect;
}

Ref<CSVDialect> CSVDialect::from_dictionary(const Dictionary &p_data) {
	Ref<CSVDialect> dialect;
	dialect.instantiate();
	if (p_data.has("delimiter")) {
		dialect->set_delimiter(p_data["delimiter"]);
	}
	if (p_data.has("headers")) {
		dialect->set_headers(p_data["headers"]);
	}
	if (p_data.has("cast_fields")) {
		dialect->set_cast_fields(p_data["cast_fields"]);
	}
	if (p_data.has("options") && p_data["options"].get_type() == Variant::DICTIONARY) {
		dialect->set_options(p_data["options"]);
	}
	return dialect;
}

Ref<CSVDialect> CSVDialect::sniff(const String &p_sample, const Dictionary &p_options) {
	PackedStringArray candidates;
	if (p_options.has("delimiters")) {
		candidates = _variant_to_string_array(p_options["delimiters"]);
	} else {
		candidates.push_back(",");
		candidates.push_back("\t");
		candidates.push_back(";");
		candidates.push_back("|");
	}
	Dictionary parser_options;
	parser_options["skip_empty_rows"] = true;
	parser_options["row_length_mode"] = DotCSVOptions::ROW_LENGTH_JAGGED;

	String best_delimiter = ",";
	Array best_rows;
	int best_score = -1;
	for (int i = 0; i < candidates.size(); i++) {
		String delimiter = candidates[i];
		if (!_validate_delimiter(delimiter)) {
			continue;
		}
		Array rows = DSVImporter::import_string(p_sample, delimiter, parser_options);
		int score = _dialect_score_rows(rows);
		if (score > best_score) {
			best_score = score;
			best_delimiter = delimiter;
			best_rows = rows;
		}
	}

	Ref<CSVDialect> dialect;
	dialect.instantiate();
	dialect->set_delimiter(best_delimiter);
	bool infer_headers = !p_options.has("infer_headers") || bool(p_options["infer_headers"]);
	dialect->set_headers(infer_headers && _dialect_first_row_looks_like_headers(best_rows));
	return dialect;
}

void CSVDialect::set_delimiter(const String &p_delimiter) {
	if (_validate_delimiter(p_delimiter)) {
		delimiter = p_delimiter;
	}
}

String CSVDialect::get_delimiter() const {
	return delimiter;
}

void CSVDialect::set_headers(bool p_headers) {
	headers = p_headers;
}

bool CSVDialect::get_headers() const {
	return headers;
}

void CSVDialect::set_cast_fields(bool p_cast_fields) {
	cast_fields = p_cast_fields;
}

bool CSVDialect::get_cast_fields() const {
	return cast_fields;
}

void CSVDialect::set_options(const Dictionary &p_options) {
	options = p_options.duplicate(true);
}

Dictionary CSVDialect::get_options() const {
	return options.duplicate(true);
}

Dictionary CSVDialect::to_dictionary() const {
	Dictionary data;
	data["delimiter"] = delimiter;
	data["headers"] = headers;
	data["cast_fields"] = cast_fields;
	data["options"] = get_options();
	return data;
}

Array CSVDialect::import_string(const String &p_string) const {
	if (headers) {
		return CSVImporter::import_string_with_headers(p_string, delimiter, cast_fields, options);
	}
	return CSVImporter::import_string(p_string, delimiter, cast_fields, options);
}

Array CSVDialect::import_file(const String &p_path) const {
	if (headers) {
		return CSVImporter::import_with_headers(p_path, delimiter, cast_fields, options);
	}
	return CSVImporter::import_file(p_path, delimiter, cast_fields, options);
}

void CSVDialect::_bind_methods() {
	ClassDB::bind_static_method("CSVDialect", D_METHOD("from_profile", "name"), &CSVDialect::from_profile);
	ClassDB::bind_static_method("CSVDialect", D_METHOD("from_dictionary", "data"), &CSVDialect::from_dictionary);
	ClassDB::bind_static_method("CSVDialect", D_METHOD("sniff", "sample", "options"), &CSVDialect::sniff, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("set_delimiter", "delimiter"), &CSVDialect::set_delimiter);
	ClassDB::bind_method(D_METHOD("get_delimiter"), &CSVDialect::get_delimiter);
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &CSVDialect::set_headers);
	ClassDB::bind_method(D_METHOD("get_headers"), &CSVDialect::get_headers);
	ClassDB::bind_method(D_METHOD("set_cast_fields", "cast_fields"), &CSVDialect::set_cast_fields);
	ClassDB::bind_method(D_METHOD("get_cast_fields"), &CSVDialect::get_cast_fields);
	ClassDB::bind_method(D_METHOD("set_options", "options"), &CSVDialect::set_options);
	ClassDB::bind_method(D_METHOD("get_options"), &CSVDialect::get_options);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &CSVDialect::to_dictionary);
	ClassDB::bind_method(D_METHOD("import_string", "string"), &CSVDialect::import_string);
	ClassDB::bind_method(D_METHOD("import_file", "path"), &CSVDialect::import_file);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter"), "set_delimiter", "get_delimiter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cast_fields"), "set_cast_fields", "get_cast_fields");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "options"), "set_options", "get_options");
}

Array CSVImporter::import_file(const String &p_file_path, const String &p_delimiter, bool p_cast_fields_to_same_type, const Dictionary &p_options) {
	String text;
	Array empty = _read_text_file(p_file_path, text);
	if (text.is_empty() && !FileAccess::exists(p_file_path)) {
		return empty;
	}
	return import_string(text, p_delimiter, p_cast_fields_to_same_type, p_options);
}

Array CSVImporter::import_string(const String &p_string, const String &p_delimiter, bool p_cast_fields_to_same_type, const Dictionary &p_options) {
	Array out;
	Array data = DSVImporter::import_string(p_string, p_delimiter, p_options);
	LocalVector<Variant::Type> types;

	for (int row_idx = 0; row_idx < data.size(); row_idx++) {
		PackedStringArray records = data[row_idx];
		Array line;
		for (int i = 0; i < records.size(); i++) {
			const String record = records[i];
			if (i < (int)types.size() && types[i] == Variant::STRING) {
				line.push_back(record);
				continue;
			}

			Variant value = parse_record(record, p_options);
			if (p_cast_fields_to_same_type) {
				if (i >= (int)types.size()) {
					types.push_back(value.get_type());
				}
				Variant::Type new_type = _compare_types(value.get_type(), types[i]);
				Variant::Type old_type = types[i];
				if (new_type != old_type) {
					types[i] = new_type;
					for (int j = 0; j < out.size(); j++) {
						Array old_line = out[j];
						if (i < old_line.size()) {
							PackedStringArray old_records = data[j];
							old_line[i] = _cast_variant(old_type, new_type, old_line[i], old_records[i]);
							out[j] = old_line;
						}
					}
				}
				line.push_back(_cast_variant(value.get_type(), new_type, value, record));
			} else {
				line.push_back(value);
			}
		}
		out.push_back(line);
	}

	return out;
}

Array CSVImporter::import_with_headers(const String &p_file_path, const String &p_delimiter, bool p_cast_fields_to_same_type, const Dictionary &p_options) {
	String text;
	Array empty = _read_text_file(p_file_path, text);
	if (text.is_empty() && !FileAccess::exists(p_file_path)) {
		return empty;
	}
	return import_string_with_headers(text, p_delimiter, p_cast_fields_to_same_type, p_options);
}

Array CSVImporter::import_string_with_headers(const String &p_string, const String &p_delimiter, bool p_cast_fields_to_same_type, const Dictionary &p_options) {
	Array out;
	Array data = DSVImporter::import_string(p_string, p_delimiter, p_options);
	if (data.is_empty()) {
		return out;
	}

	PackedStringArray headers = data[0];
	LocalVector<Variant::Type> types;
	for (int row_idx = 1; row_idx < data.size(); row_idx++) {
		PackedStringArray records = data[row_idx];
		Dictionary dict;
		for (int i = 0; i < records.size() && i < headers.size(); i++) {
			const String record = records[i];
			const String key = headers[i];
			if (i < (int)types.size() && types[i] == Variant::STRING) {
				dict[key] = record;
				continue;
			}

			Variant value = parse_record(record, p_options);
			if (p_cast_fields_to_same_type) {
				if (i >= (int)types.size()) {
					types.push_back(value.get_type());
				}
				Variant::Type new_type = _compare_types(value.get_type(), types[i]);
				Variant::Type old_type = types[i];
				if (new_type != old_type) {
					types[i] = new_type;
					for (int j = 0; j < out.size(); j++) {
						Dictionary old_dict = out[j];
						if (old_dict.has(key)) {
							PackedStringArray old_records = data[j + 1];
							old_dict[key] = _cast_variant(old_type, new_type, old_dict[key], old_records[i]);
							out[j] = old_dict;
						}
					}
				}
				dict[key] = _cast_variant(value.get_type(), new_type, value, record);
			} else {
				dict[key] = value;
			}
		}
		out.push_back(dict);
	}

	return out;
}

Variant CSVImporter::parse_record(const String &p_record, const Dictionary &p_options) {
	const DotCSVParseOptions options = _parse_options(p_options);
	if (_string_array_has(options.null_values, p_record)) {
		return Variant();
	}
	if (_string_array_has(options.true_values, p_record)) {
		return true;
	}
	if (_string_array_has(options.false_values, p_record)) {
		return false;
	}
	if (p_record.is_valid_int()) {
		return p_record.to_int();
	}
	if (p_record.is_valid_float()) {
		return p_record.to_float();
	}
	if (p_record.begins_with("#") && p_record.is_valid_html_color()) {
		return Color::html(p_record);
	}

	bool parsed = false;
	Variant value = _parse_engine_variant(p_record, parsed);
	if (parsed) {
		return value;
	}

	return p_record;
}

Array CSVImporter::import_with_dialect(const String &p_file_path, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_dialect.is_null(), Array(), "import_with_dialect requires a valid CSVDialect.");
	return p_dialect->import_file(p_file_path);
}

Array CSVImporter::import_string_with_dialect(const String &p_string, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_dialect.is_null(), Array(), "import_string_with_dialect requires a valid CSVDialect.");
	return p_dialect->import_string(p_string);
}

void CSVImporter::_bind_methods() {
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import", "file_path", "delimiter", "cast_fields_to_same_type", "options"), &CSVImporter::import_file, DEFVAL(","), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import_string", "string", "delimiter", "cast_fields_to_same_type", "options"), &CSVImporter::import_string, DEFVAL(","), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import_with_headers", "file_path", "delimiter", "cast_fields_to_same_type", "options"), &CSVImporter::import_with_headers, DEFVAL(","), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import_string_with_headers", "string", "delimiter", "cast_fields_to_same_type", "options"), &CSVImporter::import_string_with_headers, DEFVAL(","), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import_with_dialect", "file_path", "dialect"), &CSVImporter::import_with_dialect);
	ClassDB::bind_static_method("CSVImporter", D_METHOD("import_string_with_dialect", "string", "dialect"), &CSVImporter::import_string_with_dialect);
	ClassDB::bind_static_method("CSVImporter", D_METHOD("parse_record", "record", "options"), &CSVImporter::parse_record, DEFVAL(Dictionary()));
}

bool CSVExporter::export_file(const Array &p_data, const String &p_file_path, const String &p_delimiter, bool p_color_as_hex, bool p_hex_include_alpha) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), false, vformat("Opening file '%s' failed. Reason: %s.", p_file_path, error_names[err]));
	file->store_string(export_string(p_data, p_delimiter, p_color_as_hex, p_hex_include_alpha));
	return true;
}

String CSVExporter::export_string(const Array &p_data, const String &p_delimiter, bool p_color_as_hex, bool p_hex_include_alpha) {
	return DSVExporter::export_string(_to_string_rows(p_data, true, p_color_as_hex, p_hex_include_alpha), p_delimiter);
}

bool CSVExporter::export_with_headers(const Array &p_data, const String &p_file_path, const String &p_delimiter, bool p_color_as_hex, bool p_hex_include_alpha) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), false, vformat("Opening file '%s' failed. Reason: %s.", p_file_path, error_names[err]));
	file->store_string(export_string_with_headers(p_data, p_delimiter, p_color_as_hex, p_hex_include_alpha));
	return true;
}

String CSVExporter::export_string_with_headers(const Array &p_data, const String &p_delimiter, bool p_color_as_hex, bool p_hex_include_alpha) {
	Array out;
	PackedStringArray headers;
	out.push_back(headers);

	for (int i = 0; i < p_data.size(); i++) {
		Dictionary dict = p_data[i];
		PackedStringArray row;
		while (row.size() < headers.size()) {
			row.push_back("");
		}

		Array keys = dict.keys();
		for (int j = 0; j < keys.size(); j++) {
			String key = stringify(keys[j], p_color_as_hex, p_hex_include_alpha);
			String value = stringify(dict[keys[j]], p_color_as_hex, p_hex_include_alpha);
			int index = headers.find(key);
			if (index >= 0) {
				row.set(index, value);
			} else {
				headers.push_back(key);
				row.push_back(value);
			}
		}
		out.push_back(row);
	}

	out[0] = headers;
	return DSVExporter::export_string(out, p_delimiter);
}

String CSVExporter::stringify(const Variant &p_value, bool p_color_as_hex, bool p_hex_include_alpha) {
	if (p_value.get_type() == Variant::NIL) {
		return "";
	}
	if (p_value.get_type() == Variant::BOOL) {
		return (bool)p_value ? "true" : "false";
	}
	if (p_value.get_type() == Variant::STRING) {
		return p_value;
	}
	if (p_value.get_type() == Variant::COLOR && p_color_as_hex) {
		Color color = p_value;
		return "#" + color.to_html(p_hex_include_alpha);
	}
	if (p_value.get_type() == Variant::PACKED_COLOR_ARRAY && p_color_as_hex) {
		PackedColorArray colors = p_value;
		PackedStringArray entries;
		for (int i = 0; i < colors.size(); i++) {
			entries.push_back("#" + colors[i].to_html(p_hex_include_alpha));
		}
		return "PackedColorArray(" + String(",").join(entries) + ")";
	}

	String out;
	VariantWriter::write_to_string(p_value, out);
	return out;
}

void CSVExporter::_bind_methods() {
	ClassDB::bind_static_method("CSVExporter", D_METHOD("export", "data", "file_path", "delimiter", "color_as_hex", "hex_include_alpha"), &CSVExporter::export_file, DEFVAL(","), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_static_method("CSVExporter", D_METHOD("export_string", "data", "delimiter", "color_as_hex", "hex_include_alpha"), &CSVExporter::export_string, DEFVAL(","), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_static_method("CSVExporter", D_METHOD("export_with_headers", "data", "file_path", "delimiter", "color_as_hex", "hex_include_alpha"), &CSVExporter::export_with_headers, DEFVAL(","), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_static_method("CSVExporter", D_METHOD("export_string_with_headers", "data", "delimiter", "color_as_hex", "hex_include_alpha"), &CSVExporter::export_string_with_headers, DEFVAL(","), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_static_method("CSVExporter", D_METHOD("stringify", "value", "color_as_hex", "hex_include_alpha"), &CSVExporter::stringify, DEFVAL(false), DEFVAL(false));
}

Error DSVReader::open(const String &p_file_path, const String &p_delimiter, const Dictionary &p_options) {
	close();
	last_error = OK;
	finished = true;
	ERR_FAIL_COND_V(p_file_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!_validate_delimiter(p_delimiter)) {
		last_error = ERR_INVALID_PARAMETER;
		return last_error;
	}

	Error err;
	file = FileAccess::open(p_file_path, FileAccess::READ, &err);
	if (file.is_null()) {
		last_error = err;
		return last_error;
	}

	delimiter = p_delimiter;
	options = p_options;
	parser = memnew(DotCSVStreamParser(delimiter, options));
	finished = false;
	return OK;
}

PackedStringArray DSVReader::read_row() {
	PackedStringArray empty;
	if (file.is_null() || parser == nullptr) {
		return empty;
	}

	DotCSVStreamParser *state = static_cast<DotCSVStreamParser *>(parser);
	while (!state->has_rows() && !state->has_error() && file->get_position() < file->get_length()) {
		state->push_char((char32_t)file->get_8());
	}

	if (!state->has_rows() && !state->has_error() && file->get_position() >= file->get_length()) {
		state->finish();
		finished = true;
	}

	if (state->has_error()) {
		last_error = ERR_PARSE_ERROR;
		finished = true;
		return empty;
	}

	if (state->has_rows()) {
		return state->pop_row();
	}
	return empty;
}

Array DSVReader::read_rows(int p_max_rows) {
	Array out;
	ERR_FAIL_COND_V_MSG(p_max_rows < 0, out, "max_rows must be greater than or equal to zero.");
	while (p_max_rows == 0 || out.size() < p_max_rows) {
		PackedStringArray row = read_row();
		if (row.is_empty() && is_eof()) {
			break;
		}
		if (row.is_empty() && last_error != OK) {
			break;
		}
		out.push_back(row);
	}
	return out;
}

bool DSVReader::is_eof() const {
	if (file.is_null() || parser == nullptr) {
		return true;
	}
	const DotCSVStreamParser *state = static_cast<const DotCSVStreamParser *>(parser);
	return finished && !state->has_rows();
}

Error DSVReader::get_error() const {
	return last_error;
}

int DSVReader::get_line() const {
	if (parser == nullptr) {
		return 0;
	}
	const DotCSVStreamParser *state = static_cast<const DotCSVStreamParser *>(parser);
	return state->get_line();
}

uint64_t DSVReader::get_position() const {
	return file.is_valid() ? file->get_position() : 0;
}

uint64_t DSVReader::get_length() const {
	return file.is_valid() ? file->get_length() : 0;
}

void DSVReader::close() {
	if (file.is_valid()) {
		file->close();
		file.unref();
	}
	if (parser != nullptr) {
		DotCSVStreamParser *state = static_cast<DotCSVStreamParser *>(parser);
		memdelete(state);
		parser = nullptr;
	}
	finished = true;
}

DSVReader::~DSVReader() {
	close();
}

void DSVReader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "file_path", "delimiter", "options"), &DSVReader::open, DEFVAL(","), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("read_row"), &DSVReader::read_row);
	ClassDB::bind_method(D_METHOD("read_rows", "max_rows"), &DSVReader::read_rows, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_eof"), &DSVReader::is_eof);
	ClassDB::bind_method(D_METHOD("get_error"), &DSVReader::get_error);
	ClassDB::bind_method(D_METHOD("get_line"), &DSVReader::get_line);
	ClassDB::bind_method(D_METHOD("get_position"), &DSVReader::get_position);
	ClassDB::bind_method(D_METHOD("get_length"), &DSVReader::get_length);
	ClassDB::bind_method(D_METHOD("close"), &DSVReader::close);
}

Array CSVReader::_convert_row(const PackedStringArray &p_row) const {
	Array out;
	for (int i = 0; i < p_row.size(); i++) {
		out.push_back(cast_fields ? CSVImporter::parse_record(p_row[i], options) : Variant(p_row[i]));
	}
	return out;
}

void CSVReader::set_headers(bool p_headers) {
	headers = p_headers;
}

bool CSVReader::get_headers() const {
	return headers;
}

void CSVReader::set_cast_fields(bool p_cast_fields) {
	cast_fields = p_cast_fields;
}

bool CSVReader::get_cast_fields() const {
	return cast_fields;
}

PackedStringArray CSVReader::get_header_names() const {
	return header_names;
}

Error CSVReader::open(const String &p_file_path, const String &p_delimiter, bool p_headers, bool p_cast_fields, const Dictionary &p_options) {
	close();
	headers = p_headers;
	cast_fields = p_cast_fields;
	options = p_options;
	header_names = PackedStringArray();
	header_loaded = false;
	reader.instantiate();
	return reader->open(p_file_path, p_delimiter, p_options);
}

Error CSVReader::open_with_dialect(const String &p_file_path, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_dialect.is_null(), ERR_INVALID_PARAMETER, "open_with_dialect requires a valid CSVDialect.");
	return open(p_file_path, p_dialect->get_delimiter(), p_dialect->get_headers(), p_dialect->get_cast_fields(), p_dialect->get_options());
}

Variant CSVReader::read_row() {
	if (reader.is_null()) {
		return Variant();
	}
	if (headers && !header_loaded) {
		header_names = reader->read_row();
		header_loaded = true;
		if (header_names.is_empty() && reader->is_eof()) {
			return Variant();
		}
	}

	PackedStringArray raw = reader->read_row();
	if (raw.is_empty() && reader->is_eof()) {
		return Variant();
	}

	Array converted = _convert_row(raw);
	if (!headers) {
		return converted;
	}

	Dictionary row;
	for (int i = 0; i < header_names.size(); i++) {
		row[header_names[i]] = i < converted.size() ? converted[i] : Variant();
	}
	return row;
}

Array CSVReader::read_rows(int p_max_rows) {
	Array out;
	ERR_FAIL_COND_V_MSG(p_max_rows < 0, out, "max_rows must be greater than or equal to zero.");
	while (p_max_rows == 0 || out.size() < p_max_rows) {
		Variant row = read_row();
		if (row.get_type() == Variant::NIL && is_eof()) {
			break;
		}
		if (row.get_type() == Variant::NIL && get_error() != OK) {
			break;
		}
		out.push_back(row);
	}
	return out;
}

bool CSVReader::is_eof() const {
	return reader.is_null() || reader->is_eof();
}

Error CSVReader::get_error() const {
	return reader.is_null() ? ERR_UNCONFIGURED : reader->get_error();
}

int CSVReader::get_line() const {
	return reader.is_null() ? 0 : reader->get_line();
}

uint64_t CSVReader::get_position() const {
	return reader.is_valid() ? reader->get_position() : 0;
}

uint64_t CSVReader::get_length() const {
	return reader.is_valid() ? reader->get_length() : 0;
}

void CSVReader::close() {
	if (reader.is_valid()) {
		reader->close();
		reader.unref();
	}
	header_names = PackedStringArray();
	header_loaded = false;
}

void CSVReader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &CSVReader::set_headers);
	ClassDB::bind_method(D_METHOD("get_headers"), &CSVReader::get_headers);
	ClassDB::bind_method(D_METHOD("set_cast_fields", "cast_fields"), &CSVReader::set_cast_fields);
	ClassDB::bind_method(D_METHOD("get_cast_fields"), &CSVReader::get_cast_fields);
	ClassDB::bind_method(D_METHOD("get_header_names"), &CSVReader::get_header_names);
	ClassDB::bind_method(D_METHOD("open", "file_path", "delimiter", "headers", "cast_fields", "options"), &CSVReader::open, DEFVAL(","), DEFVAL(false), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("open_with_dialect", "file_path", "dialect"), &CSVReader::open_with_dialect);
	ClassDB::bind_method(D_METHOD("read_row"), &CSVReader::read_row);
	ClassDB::bind_method(D_METHOD("read_rows", "max_rows"), &CSVReader::read_rows, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_eof"), &CSVReader::is_eof);
	ClassDB::bind_method(D_METHOD("get_error"), &CSVReader::get_error);
	ClassDB::bind_method(D_METHOD("get_line"), &CSVReader::get_line);
	ClassDB::bind_method(D_METHOD("get_position"), &CSVReader::get_position);
	ClassDB::bind_method(D_METHOD("get_length"), &CSVReader::get_length);
	ClassDB::bind_method(D_METHOD("close"), &CSVReader::close);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cast_fields"), "set_cast_fields", "get_cast_fields");
}

Error DSVWriter::open(const String &p_file_path, const String &p_delimiter, bool p_append) {
	close();
	last_error = OK;
	ERR_FAIL_COND_V(p_file_path.is_empty(), ERR_INVALID_PARAMETER);
	if (!_validate_delimiter(p_delimiter)) {
		last_error = ERR_INVALID_PARAMETER;
		return last_error;
	}

	Error err;
	if (p_append && FileAccess::exists(p_file_path)) {
		file = FileAccess::open(p_file_path, FileAccess::READ_WRITE, &err);
		if (file.is_valid()) {
			file->seek_end();
		}
	} else {
		file = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	}
	if (file.is_null()) {
		last_error = err;
		return last_error;
	}
	delimiter = p_delimiter;
	return OK;
}

bool DSVWriter::write_row(const Variant &p_row) {
	ERR_FAIL_COND_V_MSG(file.is_null(), false, "DSVWriter is not open.");
	Array rows;
	rows.push_back(_variant_to_dsv_row(p_row, false, false, false));
	file->store_string(DSVExporter::export_string(rows, delimiter));
	return true;
}

bool DSVWriter::write_rows(const Array &p_rows) {
	for (int i = 0; i < p_rows.size(); i++) {
		if (!write_row(p_rows[i])) {
			return false;
		}
	}
	return true;
}

Error DSVWriter::get_error() const {
	return last_error;
}

void DSVWriter::close() {
	if (file.is_valid()) {
		file->close();
		file.unref();
	}
}

void DSVWriter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "file_path", "delimiter", "append"), &DSVWriter::open, DEFVAL(","), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("write_row", "row"), &DSVWriter::write_row);
	ClassDB::bind_method(D_METHOD("write_rows", "rows"), &DSVWriter::write_rows);
	ClassDB::bind_method(D_METHOD("get_error"), &DSVWriter::get_error);
	ClassDB::bind_method(D_METHOD("close"), &DSVWriter::close);
}

PackedStringArray CSVWriter::_row_to_strings(const Variant &p_row) {
	if (p_row.get_type() != Variant::DICTIONARY || !headers || header_names.is_empty()) {
		return _variant_to_dsv_row(p_row, true, color_as_hex, hex_include_alpha);
	}

	Dictionary dict = p_row;
	PackedStringArray out;
	for (int i = 0; i < header_names.size(); i++) {
		out.push_back(CSVExporter::stringify(dict.has(header_names[i]) ? dict[header_names[i]] : Variant(), color_as_hex, hex_include_alpha));
	}
	return out;
}

void CSVWriter::set_headers(bool p_headers) {
	headers = p_headers;
}

bool CSVWriter::get_headers() const {
	return headers;
}

void CSVWriter::set_header_names(const PackedStringArray &p_header_names) {
	header_names = p_header_names;
}

PackedStringArray CSVWriter::get_header_names() const {
	return header_names;
}

void CSVWriter::set_color_as_hex(bool p_color_as_hex) {
	color_as_hex = p_color_as_hex;
}

bool CSVWriter::get_color_as_hex() const {
	return color_as_hex;
}

void CSVWriter::set_hex_include_alpha(bool p_hex_include_alpha) {
	hex_include_alpha = p_hex_include_alpha;
}

bool CSVWriter::get_hex_include_alpha() const {
	return hex_include_alpha;
}

Error CSVWriter::open(const String &p_file_path, const String &p_delimiter, bool p_append) {
	writer.instantiate();
	wrote_headers = p_append;
	return writer->open(p_file_path, p_delimiter, p_append);
}

bool CSVWriter::write_row(const Variant &p_row) {
	ERR_FAIL_COND_V_MSG(writer.is_null(), false, "CSVWriter is not open.");
	if (headers && !wrote_headers) {
		if (header_names.is_empty() && p_row.get_type() == Variant::DICTIONARY) {
			Dictionary dict = p_row;
			Array keys = dict.keys();
			keys.sort();
			for (int i = 0; i < keys.size(); i++) {
				header_names.push_back(String(keys[i]));
			}
		}
		if (!header_names.is_empty() && !writer->write_row(header_names)) {
			return false;
		}
		wrote_headers = true;
	}
	return writer->write_row(_row_to_strings(p_row));
}

bool CSVWriter::write_rows(const Array &p_rows) {
	for (int i = 0; i < p_rows.size(); i++) {
		if (!write_row(p_rows[i])) {
			return false;
		}
	}
	return true;
}

Error CSVWriter::get_error() const {
	return writer.is_null() ? ERR_UNCONFIGURED : writer->get_error();
}

void CSVWriter::close() {
	if (writer.is_valid()) {
		writer->close();
		writer.unref();
	}
	wrote_headers = false;
}

void CSVWriter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &CSVWriter::set_headers);
	ClassDB::bind_method(D_METHOD("get_headers"), &CSVWriter::get_headers);
	ClassDB::bind_method(D_METHOD("set_header_names", "header_names"), &CSVWriter::set_header_names);
	ClassDB::bind_method(D_METHOD("get_header_names"), &CSVWriter::get_header_names);
	ClassDB::bind_method(D_METHOD("set_color_as_hex", "color_as_hex"), &CSVWriter::set_color_as_hex);
	ClassDB::bind_method(D_METHOD("get_color_as_hex"), &CSVWriter::get_color_as_hex);
	ClassDB::bind_method(D_METHOD("set_hex_include_alpha", "hex_include_alpha"), &CSVWriter::set_hex_include_alpha);
	ClassDB::bind_method(D_METHOD("get_hex_include_alpha"), &CSVWriter::get_hex_include_alpha);
	ClassDB::bind_method(D_METHOD("open", "file_path", "delimiter", "append"), &CSVWriter::open, DEFVAL(","), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("write_row", "row"), &CSVWriter::write_row);
	ClassDB::bind_method(D_METHOD("write_rows", "rows"), &CSVWriter::write_rows);
	ClassDB::bind_method(D_METHOD("get_error"), &CSVWriter::get_error);
	ClassDB::bind_method(D_METHOD("close"), &CSVWriter::close);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "header_names"), "set_header_names", "get_header_names");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "color_as_hex"), "set_color_as_hex", "get_color_as_hex");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hex_include_alpha"), "set_hex_include_alpha", "get_hex_include_alpha");
}

void CSV::set_headers(bool p_headers) {
	headers = p_headers;
	emit_changed();
}

bool CSV::get_headers() const {
	return headers;
}

void CSV::set_delimiter(const String &p_delimiter) {
	delimiter = p_delimiter;
	emit_changed();
}

String CSV::get_delimiter() const {
	return delimiter;
}

void CSV::set_trim_fields(bool p_trim_fields) {
	trim_fields = p_trim_fields;
	emit_changed();
}

bool CSV::get_trim_fields() const {
	return trim_fields;
}

void CSV::set_skip_empty_rows(bool p_skip_empty_rows) {
	skip_empty_rows = p_skip_empty_rows;
	emit_changed();
}

bool CSV::get_skip_empty_rows() const {
	return skip_empty_rows;
}

void CSV::set_comment_prefixes(const PackedStringArray &p_comment_prefixes) {
	comment_prefixes = p_comment_prefixes;
	emit_changed();
}

PackedStringArray CSV::get_comment_prefixes() const {
	return comment_prefixes;
}

void CSV::set_row_length_mode(int p_row_length_mode) {
	row_length_mode = CLAMP(p_row_length_mode, DotCSVOptions::ROW_LENGTH_STRICT, DotCSVOptions::ROW_LENGTH_JAGGED);
	emit_changed();
}

int CSV::get_row_length_mode() const {
	return row_length_mode;
}

void CSV::set_null_values(const PackedStringArray &p_null_values) {
	null_values = p_null_values;
	emit_changed();
}

PackedStringArray CSV::get_null_values() const {
	return null_values;
}

void CSV::set_true_values(const PackedStringArray &p_true_values) {
	true_values = p_true_values;
	emit_changed();
}

PackedStringArray CSV::get_true_values() const {
	return true_values;
}

void CSV::set_false_values(const PackedStringArray &p_false_values) {
	false_values = p_false_values;
	emit_changed();
}

PackedStringArray CSV::get_false_values() const {
	return false_values;
}

void CSV::set_rows(const TypedArray<Dictionary> &p_rows) {
	rows = p_rows;
	emit_changed();
}

TypedArray<Dictionary> CSV::get_rows() const {
	return rows;
}

Dictionary CSV::get_parser_options() const {
	Dictionary options;
	options["trim_fields"] = trim_fields;
	options["skip_empty_rows"] = skip_empty_rows;
	options["comment_prefixes"] = comment_prefixes;
	options["row_length_mode"] = row_length_mode;
	options["null_values"] = null_values;
	options["true_values"] = true_values;
	options["false_values"] = false_values;
	return options;
}

Variant CSV::convert_to_variant(const String &p_text) const {
	return CSVImporter::parse_record(p_text, get_parser_options());
}

Error CSV::load_file(const String &p_path) {
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, vformat("Cannot open CSV file '%s'.", p_path));
	return load_string(file->get_as_text());
}

Error CSV::load_string(const String &p_string) {
	TypedArray<Dictionary> loaded_rows;
	Dictionary parser_options = get_parser_options();
	if (headers) {
		Array imported = CSVImporter::import_string_with_headers(p_string, delimiter, true, parser_options);
		for (int i = 0; i < imported.size(); i++) {
			loaded_rows.push_back(imported[i]);
		}
	} else {
		Array imported = CSVImporter::import_string(p_string, delimiter, true, parser_options);
		for (int i = 0; i < imported.size(); i++) {
			Array line = imported[i];
			Dictionary row;
			for (int j = 0; j < line.size(); j++) {
				row[j] = line[j];
			}
			loaded_rows.push_back(row);
		}
	}
	set_rows(loaded_rows);
	return OK;
}

String CSV::save_to_string() const {
	Array export_rows;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		export_rows.push_back(row);
	}
	if (headers) {
		return CSVExporter::export_string_with_headers(export_rows, delimiter);
	}

	Array indexed_rows;
	for (int i = 0; i < export_rows.size(); i++) {
		Dictionary row = export_rows[i];
		Array line;
		Array keys = row.keys();
		keys.sort();
		for (int j = 0; j < keys.size(); j++) {
			line.push_back(row[keys[j]]);
		}
		indexed_rows.push_back(line);
	}
	return CSVExporter::export_string(indexed_rows, delimiter);
}

void CSV::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &CSV::set_headers);
	ClassDB::bind_method(D_METHOD("get_headers"), &CSV::get_headers);
	ClassDB::bind_method(D_METHOD("set_delimiter", "delimiter"), &CSV::set_delimiter);
	ClassDB::bind_method(D_METHOD("get_delimiter"), &CSV::get_delimiter);
	ClassDB::bind_method(D_METHOD("set_trim_fields", "trim_fields"), &CSV::set_trim_fields);
	ClassDB::bind_method(D_METHOD("get_trim_fields"), &CSV::get_trim_fields);
	ClassDB::bind_method(D_METHOD("set_skip_empty_rows", "skip_empty_rows"), &CSV::set_skip_empty_rows);
	ClassDB::bind_method(D_METHOD("get_skip_empty_rows"), &CSV::get_skip_empty_rows);
	ClassDB::bind_method(D_METHOD("set_comment_prefixes", "comment_prefixes"), &CSV::set_comment_prefixes);
	ClassDB::bind_method(D_METHOD("get_comment_prefixes"), &CSV::get_comment_prefixes);
	ClassDB::bind_method(D_METHOD("set_row_length_mode", "row_length_mode"), &CSV::set_row_length_mode);
	ClassDB::bind_method(D_METHOD("get_row_length_mode"), &CSV::get_row_length_mode);
	ClassDB::bind_method(D_METHOD("set_null_values", "null_values"), &CSV::set_null_values);
	ClassDB::bind_method(D_METHOD("get_null_values"), &CSV::get_null_values);
	ClassDB::bind_method(D_METHOD("set_true_values", "true_values"), &CSV::set_true_values);
	ClassDB::bind_method(D_METHOD("get_true_values"), &CSV::get_true_values);
	ClassDB::bind_method(D_METHOD("set_false_values", "false_values"), &CSV::set_false_values);
	ClassDB::bind_method(D_METHOD("get_false_values"), &CSV::get_false_values);
	ClassDB::bind_method(D_METHOD("set_rows", "rows"), &CSV::set_rows);
	ClassDB::bind_method(D_METHOD("get_rows"), &CSV::get_rows);
	ClassDB::bind_method(D_METHOD("get_parser_options"), &CSV::get_parser_options);
	ClassDB::bind_method(D_METHOD("convert_to_variant", "text"), &CSV::convert_to_variant);
	ClassDB::bind_method(D_METHOD("load_file", "path"), &CSV::load_file);
	ClassDB::bind_method(D_METHOD("load_string", "string"), &CSV::load_string);
	ClassDB::bind_method(D_METHOD("save_to_string"), &CSV::save_to_string);

	ClassDB::bind_integer_constant(get_class_static(), "RowLengthMode", "ROW_LENGTH_STRICT", DotCSVOptions::ROW_LENGTH_STRICT);
	ClassDB::bind_integer_constant(get_class_static(), "RowLengthMode", "ROW_LENGTH_PAD_SHORT", DotCSVOptions::ROW_LENGTH_PAD_SHORT);
	ClassDB::bind_integer_constant(get_class_static(), "RowLengthMode", "ROW_LENGTH_PAD_OR_TRUNCATE", DotCSVOptions::ROW_LENGTH_PAD_OR_TRUNCATE);
	ClassDB::bind_integer_constant(get_class_static(), "RowLengthMode", "ROW_LENGTH_JAGGED", DotCSVOptions::ROW_LENGTH_JAGGED);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter"), "set_delimiter", "get_delimiter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trim_fields"), "set_trim_fields", "get_trim_fields");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "skip_empty_rows"), "set_skip_empty_rows", "get_skip_empty_rows");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "comment_prefixes"), "set_comment_prefixes", "get_comment_prefixes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "row_length_mode", PROPERTY_HINT_ENUM, "Strict,Pad Short,Pad or Truncate,Jagged"), "set_row_length_mode", "get_row_length_mode");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "null_values"), "set_null_values", "get_null_values");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "true_values"), "set_true_values", "get_true_values");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "false_values"), "set_false_values", "get_false_values");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rows", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "set_rows", "get_rows");
}

CSV::CSV() {
	null_values.push_back("");
	true_values.push_back("true");
	true_values.push_back("True");
	true_values.push_back("TRUE");
	false_values.push_back("false");
	false_values.push_back("False");
	false_values.push_back("FALSE");
}

namespace {

String _csv_query_key(const Variant &p_value) {
	return itos((int)p_value.get_type()) + ":" + CSVExporter::stringify(p_value, true, true);
}

String _csv_display_key(const Variant &p_value) {
	return CSVExporter::stringify(p_value, true, true);
}

Dictionary _row_to_dictionary(const Variant &p_row) {
	Dictionary out;
	if (p_row.get_type() == Variant::DICTIONARY) {
		Dictionary row = p_row;
		Array keys = row.keys();
		for (int i = 0; i < keys.size(); i++) {
			out[keys[i]] = row[keys[i]];
		}
		return out;
	}
	if (p_row.get_type() == Variant::ARRAY) {
		Array row = p_row;
		for (int i = 0; i < row.size(); i++) {
			out[i] = row[i];
		}
		return out;
	}
	return out;
}

Array _normalize_rows(const Array &p_rows) {
	Array out;
	for (int i = 0; i < p_rows.size(); i++) {
		out.push_back(_row_to_dictionary(p_rows[i]));
	}
	return out;
}

bool _array_has_variant(const Array &p_values, const Variant &p_value) {
	for (int i = 0; i < p_values.size(); i++) {
		if (p_values[i] == p_value) {
			return true;
		}
	}
	return false;
}

String _schema_type_from_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		return String();
	}
	Variant value = p_value;
	if (p_value.get_type() == Variant::STRING) {
		value = CSVImporter::parse_record(p_value);
	}
	switch (value.get_type()) {
		case Variant::BOOL:
			return "bool";
		case Variant::INT:
			return "int";
		case Variant::FLOAT:
			return "float";
		case Variant::COLOR:
			return "color";
		case Variant::STRING:
			return "string";
		default:
			return "variant";
	}
}

String _schema_promote_type(const String &p_existing, const String &p_observed) {
	if (p_observed.is_empty()) {
		return p_existing;
	}
	if (p_existing.is_empty()) {
		return p_observed;
	}
	if (p_existing == p_observed) {
		return p_existing;
	}
	if ((p_existing == "int" && p_observed == "float") || (p_existing == "float" && p_observed == "int")) {
		return "float";
	}
	return "variant";
}

Dictionary _infer_schema_from_rows(const Array &p_rows, const Dictionary &p_options) {
	const bool required_by_default = p_options.has("required_by_default") && bool(p_options["required_by_default"]);
	Array field_keys;
	Dictionary field_types;
	Dictionary present_counts;
	Dictionary non_null_counts;

	for (int i = 0; i < p_rows.size(); i++) {
		Dictionary row = _row_to_dictionary(p_rows[i]);
		Array keys = row.keys();
		for (int j = 0; j < keys.size(); j++) {
			Variant key = keys[j];
			if (!present_counts.has(key)) {
				field_keys.push_back(key);
				present_counts[key] = 0;
				non_null_counts[key] = 0;
				field_types[key] = String();
			}
			present_counts[key] = int(present_counts[key]) + 1;
			Variant value = row[key];
			if (value.get_type() == Variant::NIL) {
				continue;
			}
			non_null_counts[key] = int(non_null_counts[key]) + 1;
			field_types[key] = _schema_promote_type(field_types[key], _schema_type_from_value(value));
		}
	}

	Dictionary schema;
	for (int i = 0; i < field_keys.size(); i++) {
		Variant key = field_keys[i];
		Dictionary definition;
		String type = field_types[key];
		definition["type"] = type.is_empty() ? String("variant") : type;
		definition["required"] = required_by_default || (int(present_counts[key]) == p_rows.size() && int(non_null_counts[key]) == p_rows.size());
		schema[key] = definition;
	}
	return schema;
}

String _model_field_type(const Dictionary &p_definition) {
	if (!p_definition.has("type")) {
		return "variant";
	}
	return String(p_definition["type"]).to_lower();
}

bool _model_is_missing(const Variant &p_value) {
	return p_value.get_type() == Variant::NIL;
}

bool _model_array_has_variant(const Array &p_values, const Variant &p_value) {
	for (int i = 0; i < p_values.size(); i++) {
		if (p_values[i] == p_value) {
			return true;
		}
	}
	return false;
}

Variant _model_normalize_input_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		return CSVImporter::parse_record(p_value);
	}
	return p_value;
}

Variant _model_cast_value(const Variant &p_value, const String &p_type, bool &r_ok) {
	r_ok = true;
	if (p_type == "variant" || p_type.is_empty()) {
		return p_value;
	}
	if (_model_is_missing(p_value)) {
		return Variant();
	}

	Variant value = _model_normalize_input_value(p_value);
	if (p_type == "string") {
		if (value.get_type() == Variant::STRING) {
			return value;
		}
		return CSVExporter::stringify(value, true, true);
	}
	if (p_type == "int") {
		if (value.get_type() == Variant::INT) {
			return value;
		}
		if (value.get_type() == Variant::FLOAT) {
			return int64_t(double(value));
		}
		if (p_value.get_type() == Variant::STRING && String(p_value).is_valid_int()) {
			return String(p_value).to_int();
		}
		r_ok = false;
		return p_value;
	}
	if (p_type == "float") {
		if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
			return double(value);
		}
		if (p_value.get_type() == Variant::STRING && String(p_value).is_valid_float()) {
			return String(p_value).to_float();
		}
		r_ok = false;
		return p_value;
	}
	if (p_type == "bool") {
		if (value.get_type() == Variant::BOOL) {
			return value;
		}
		if (value.get_type() == Variant::INT) {
			return int64_t(value) != 0;
		}
		if (p_value.get_type() == Variant::STRING) {
			String lower = String(p_value).strip_edges().to_lower();
			if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
				return true;
			}
			if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
				return false;
			}
		}
		r_ok = false;
		return p_value;
	}
	if (p_type == "color") {
		if (value.get_type() == Variant::COLOR) {
			return value;
		}
		if (p_value.get_type() == Variant::STRING && String(p_value).is_valid_html_color()) {
			return Color::html(p_value);
		}
		r_ok = false;
		return p_value;
	}

	r_ok = false;
	return p_value;
}

Dictionary _model_error(const String &p_field, const String &p_message) {
	Dictionary error;
	error["field"] = p_field;
	error["message"] = p_message;
	return error;
}

Variant _model_get_raw_field(const Dictionary &p_row, const Variant &p_key, int p_index, bool &r_found) {
	if (p_row.has(p_key)) {
		r_found = true;
		return p_row[p_key];
	}
	String key_string = String(p_key);
	if (p_row.has(key_string)) {
		r_found = true;
		return p_row[key_string];
	}
	StringName key_name = key_string;
	if (p_row.has(key_name)) {
		r_found = true;
		return p_row[key_name];
	}
	if (p_row.has(p_index)) {
		r_found = true;
		return p_row[p_index];
	}
	r_found = false;
	return Variant();
}

String _variant_transform_key(const Variant &p_value) {
	return String::num_int64(p_value.get_type()) + ":" + CSVExporter::stringify(p_value, true, true);
}

String _row_transform_key(const Dictionary &p_row, const PackedStringArray &p_columns) {
	String key;
	if (p_columns.is_empty()) {
		Array keys = p_row.keys();
		keys.sort();
		for (int i = 0; i < keys.size(); i++) {
			Variant row_key = keys[i];
			key += _variant_transform_key(row_key) + "=" + _variant_transform_key(p_row[row_key]) + ";";
		}
		return key;
	}

	for (int i = 0; i < p_columns.size(); i++) {
		StringName column = p_columns[i];
		key += String(column) + "=" + (p_row.has(column) ? _variant_transform_key(p_row[column]) : "nil") + ";";
	}
	return key;
}

int _variant_transform_compare(const Variant &p_left, const Variant &p_right) {
	Variant::Type left_type = p_left.get_type();
	Variant::Type right_type = p_right.get_type();
	if ((left_type == Variant::INT || left_type == Variant::FLOAT) && (right_type == Variant::INT || right_type == Variant::FLOAT)) {
		double left = double(p_left);
		double right = double(p_right);
		if (left < right) {
			return -1;
		}
		if (left > right) {
			return 1;
		}
		return 0;
	}
	if (left_type == Variant::BOOL && right_type == Variant::BOOL) {
		bool left = bool(p_left);
		bool right = bool(p_right);
		return left == right ? 0 : (left ? 1 : -1);
	}
	if (left_type != right_type) {
		return left_type < right_type ? -1 : 1;
	}
	String left_key = CSVExporter::stringify(p_left, true, true);
	String right_key = CSVExporter::stringify(p_right, true, true);
	return left_key.naturalnocasecmp_to(right_key);
}

int _row_transform_compare(const Dictionary &p_left, const Dictionary &p_right, const PackedStringArray &p_columns) {
	for (int i = 0; i < p_columns.size(); i++) {
		StringName column = p_columns[i];
		Variant left_value = p_left.has(column) ? p_left[column] : Variant();
		Variant right_value = p_right.has(column) ? p_right[column] : Variant();
		int result = _variant_transform_compare(left_value, right_value);
		if (result != 0) {
			return result;
		}
	}
	return 0;
}

bool _array_has_string_name(const PackedStringArray &p_values, const StringName &p_value) {
	for (int i = 0; i < p_values.size(); i++) {
		if (StringName(p_values[i]) == p_value) {
			return true;
		}
	}
	return false;
}

Dictionary _merge_join_rows(const Dictionary &p_left, const Dictionary &p_right, const StringName &p_left_column, const StringName &p_right_column) {
	Dictionary out;
	Array left_keys = p_left.keys();
	for (int i = 0; i < left_keys.size(); i++) {
		out[left_keys[i]] = p_left[left_keys[i]];
	}

	Array right_keys = p_right.keys();
	for (int i = 0; i < right_keys.size(); i++) {
		Variant key = right_keys[i];
		if (StringName(key) == p_right_column && p_left_column == p_right_column) {
			continue;
		}
		Variant out_key = key;
		if (out.has(out_key)) {
			out_key = String("right.") + String(key);
		}
		out[out_key] = p_right[key];
	}
	return out;
}

} // namespace

Ref<CSVRowModel> CSVRowModel::from_schema(const Dictionary &p_schema) {
	Ref<CSVRowModel> model;
	model.instantiate();
	model->set_schema(p_schema);
	return model;
}

Ref<CSVRowModel> CSVRowModel::from_file(const String &p_path) {
	Ref<CSVRowModel> model;
	model.instantiate();
	Error err = model->load_schema(p_path);
	ERR_FAIL_COND_V(err != OK, Ref<CSVRowModel>());
	return model;
}

Ref<CSVRowModel> CSVRowModel::infer_from_rows(const Array &p_rows, const Dictionary &p_options) {
	Ref<CSVRowModel> model;
	model.instantiate();
	model->set_schema(_infer_schema_from_rows(p_rows, p_options));
	return model;
}

void CSVRowModel::set_schema(const Dictionary &p_schema) {
	schema = p_schema.duplicate(true);
}

Dictionary CSVRowModel::get_schema() const {
	return schema.duplicate(true);
}

Error CSVRowModel::save_schema(const String &p_path) const {
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, vformat("Opening file '%s' failed. Reason: %s.", p_path, error_names[err]));
	String text;
	VariantWriter::write_to_string(schema, text);
	file->store_string(text);
	return OK;
}

Error CSVRowModel::load_schema(const String &p_path) {
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, vformat("Opening file '%s' failed. Reason: %s.", p_path, error_names[err]));
	Variant value;
	String err_str;
	int err_line = 0;
	VariantParser::StreamString stream;
	stream.s = file->get_as_text();
	err = VariantParser::parse(&stream, value, err_str, err_line);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_PARSE_ERROR, vformat("Parsing schema file '%s' failed at line %d: %s", p_path, err_line, err_str));
	ERR_FAIL_COND_V_MSG(value.get_type() != Variant::DICTIONARY, ERR_INVALID_DATA, "CSVRowModel schema files must contain a Dictionary.");
	set_schema(value);
	return OK;
}

PackedStringArray CSVRowModel::get_field_names() const {
	PackedStringArray names;
	Array keys = schema.keys();
	for (int i = 0; i < keys.size(); i++) {
		names.push_back(String(keys[i]));
	}
	return names;
}

Dictionary CSVRowModel::cast_row(const Variant &p_row) const {
	Dictionary raw = _row_to_dictionary(p_row);
	Dictionary out;
	Array keys = schema.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		String field = String(key);
		Dictionary definition;
		if (schema[key].get_type() == Variant::DICTIONARY) {
			definition = schema[key];
		}
		bool found = false;
		Variant value = _model_get_raw_field(raw, key, i, found);
		if (!found && definition.has("default")) {
			value = definition["default"];
		}
		bool cast_ok = true;
		out[field] = _model_cast_value(value, _model_field_type(definition), cast_ok);
	}
	return out;
}

Array CSVRowModel::cast_rows(const Array &p_rows) const {
	Array out;
	for (int i = 0; i < p_rows.size(); i++) {
		out.push_back(cast_row(p_rows[i]));
	}
	return out;
}

Dictionary CSVRowModel::validate_row(const Variant &p_row) const {
	Dictionary raw = _row_to_dictionary(p_row);
	Dictionary casted;
	Array errors;
	Array keys = schema.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		String field = String(key);
		Dictionary definition;
		if (schema[key].get_type() == Variant::DICTIONARY) {
			definition = schema[key];
		}
		bool found = false;
		Variant value = _model_get_raw_field(raw, key, i, found);
		if (!found && definition.has("default")) {
			value = definition["default"];
		}
		if ((!found || _model_is_missing(value)) && definition.has("required") && bool(definition["required"])) {
			errors.push_back(_model_error(field, "Missing required field."));
		}
		bool cast_ok = true;
		Variant casted_value = _model_cast_value(value, _model_field_type(definition), cast_ok);
		casted[field] = casted_value;
		if (!_model_is_missing(value) && !cast_ok) {
			errors.push_back(_model_error(field, "Value cannot be converted to " + _model_field_type(definition) + "."));
		}
		if (definition.has("enum")) {
			Array enum_values = definition["enum"];
			if (!_model_is_missing(casted_value) && !_model_array_has_variant(enum_values, casted_value)) {
				errors.push_back(_model_error(field, "Value is not in enum."));
			}
		}
	}
	Dictionary report;
	report["ok"] = errors.is_empty();
	report["errors"] = errors;
	report["row"] = casted;
	return report;
}

Array CSVRowModel::validate_rows(const Array &p_rows) const {
	Array out;
	for (int i = 0; i < p_rows.size(); i++) {
		Dictionary report = validate_row(p_rows[i]);
		report["index"] = i;
		out.push_back(report);
	}
	return out;
}

Dictionary CSVRowModel::serialize_row(const Variant &p_row, bool p_include_defaults) const {
	Dictionary raw = _row_to_dictionary(p_row);
	Dictionary casted = cast_row(p_row);
	Dictionary out;
	Array keys = schema.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		String field = String(key);
		Dictionary definition;
		if (schema[key].get_type() == Variant::DICTIONARY) {
			definition = schema[key];
		}
		if (!p_include_defaults && !raw.has(key) && !raw.has(field) && !raw.has(StringName(field)) && !raw.has(i) && definition.has("default")) {
			continue;
		}
		out[field] = casted[field];
	}
	return out;
}

Array CSVRowModel::serialize_rows(const Array &p_rows, bool p_include_defaults) const {
	Array out;
	for (int i = 0; i < p_rows.size(); i++) {
		out.push_back(serialize_row(p_rows[i], p_include_defaults));
	}
	return out;
}

void CSVRowModel::_bind_methods() {
	ClassDB::bind_static_method("CSVRowModel", D_METHOD("from_schema", "schema"), &CSVRowModel::from_schema);
	ClassDB::bind_static_method("CSVRowModel", D_METHOD("from_file", "path"), &CSVRowModel::from_file);
	ClassDB::bind_static_method("CSVRowModel", D_METHOD("infer_from_rows", "rows", "options"), &CSVRowModel::infer_from_rows, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVRowModel", D_METHOD("infer_from_table", "table", "options"), &CSVRowModel::infer_from_table, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("set_schema", "schema"), &CSVRowModel::set_schema);
	ClassDB::bind_method(D_METHOD("get_schema"), &CSVRowModel::get_schema);
	ClassDB::bind_method(D_METHOD("save_schema", "path"), &CSVRowModel::save_schema);
	ClassDB::bind_method(D_METHOD("load_schema", "path"), &CSVRowModel::load_schema);
	ClassDB::bind_method(D_METHOD("get_field_names"), &CSVRowModel::get_field_names);
	ClassDB::bind_method(D_METHOD("cast_row", "row"), &CSVRowModel::cast_row);
	ClassDB::bind_method(D_METHOD("cast_rows", "rows"), &CSVRowModel::cast_rows);
	ClassDB::bind_method(D_METHOD("validate_row", "row"), &CSVRowModel::validate_row);
	ClassDB::bind_method(D_METHOD("validate_rows", "rows"), &CSVRowModel::validate_rows);
	ClassDB::bind_method(D_METHOD("serialize_row", "row", "include_defaults"), &CSVRowModel::serialize_row, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("serialize_rows", "rows", "include_defaults"), &CSVRowModel::serialize_rows, DEFVAL(true));
}

static double _dotcsv_clamp_progress(double p_value) {
	if (p_value < 0.0) {
		return 0.0;
	}
	if (p_value > 1.0) {
		return 1.0;
	}
	return p_value;
}

void CSVAsyncTask::_thread_func(void *p_userdata) {
	CSVAsyncTask *task = static_cast<CSVAsyncTask *>(p_userdata);
	task->_run();
}

Error CSVChunkProcessor::process_file(const String &p_path, const Callable &p_callable, int p_chunk_size, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), ERR_INVALID_PARAMETER, "process_file requires a file path.");
	ERR_FAIL_COND_V_MSG(!p_callable.is_valid(), ERR_INVALID_PARAMETER, "process_file requires a valid Callable.");
	ERR_FAIL_COND_V_MSG(p_chunk_size <= 0, ERR_INVALID_PARAMETER, "chunk_size must be greater than zero.");

	Ref<CSVReader> reader;
	Error err = _chunk_open_reader(reader, p_path, p_dialect);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Unable to open CSV file '%s'.", p_path));

	int chunk_index = 0;
	while (true) {
		Array chunk = reader->read_rows(p_chunk_size);
		if (chunk.is_empty()) {
			break;
		}
		Variant chunk_arg = chunk;
		Variant index_arg = chunk_index;
		const Variant *args[2] = { &chunk_arg, &index_arg };
		Callable::CallError call_error;
		Variant result;
		p_callable.callp(args, 2, result, call_error);
		if (call_error.error != Callable::CallError::CALL_OK) {
			reader->close();
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "process_file callable call failed.");
		}
		chunk_index++;
	}
	Error read_error = reader->get_error();
	reader->close();
	return read_error;
}

Array CSVChunkProcessor::sample_file(const String &p_path, int p_count, const Ref<CSVDialect> &p_dialect) {
	Array out;
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), out, "sample_file requires a file path.");
	ERR_FAIL_COND_V_MSG(p_count < 0, out, "count must be greater than or equal to zero.");
	if (p_count == 0) {
		return out;
	}
	Ref<CSVReader> reader;
	Error err = _chunk_open_reader(reader, p_path, p_dialect);
	ERR_FAIL_COND_V_MSG(err != OK, out, vformat("Unable to open CSV file '%s'.", p_path));
	out = reader->read_rows(p_count);
	reader->close();
	return out;
}

PackedStringArray CSVChunkProcessor::split_file(const String &p_path, const String &p_output_dir, int p_rows_per_file, const Ref<CSVDialect> &p_dialect, bool p_include_headers) {
	PackedStringArray paths;
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), paths, "split_file requires a file path.");
	ERR_FAIL_COND_V_MSG(p_output_dir.is_empty(), paths, "split_file requires an output directory.");
	ERR_FAIL_COND_V_MSG(p_rows_per_file <= 0, paths, "rows_per_file must be greater than zero.");

	if (!DirAccess::dir_exists_absolute(p_output_dir)) {
		Error dir_err = DirAccess::make_dir_recursive_absolute(p_output_dir);
		ERR_FAIL_COND_V_MSG(dir_err != OK, paths, vformat("Unable to create output directory '%s'.", p_output_dir));
	}

	Ref<CSVDialect> dialect = _chunk_get_dialect(p_dialect);
	Ref<CSVReader> reader;
	Error err = _chunk_open_reader(reader, p_path, dialect);
	ERR_FAIL_COND_V_MSG(err != OK, paths, vformat("Unable to open CSV file '%s'.", p_path));

	Ref<CSVWriter> writer;
	int part_index = 0;
	int rows_in_part = 0;
	while (true) {
		Variant row = reader->read_row();
		if (row.get_type() == Variant::NIL) {
			break;
		}
		if (writer.is_null() || rows_in_part >= p_rows_per_file) {
			if (writer.is_valid()) {
				writer->close();
				writer.unref();
			}
			String output_path = _chunk_output_path(p_path, p_output_dir, part_index);
			writer.instantiate();
			_chunk_configure_writer(writer, dialect, p_include_headers, reader->get_header_names());
			Error open_error = writer->open(output_path, dialect->get_delimiter(), false);
			if (open_error != OK) {
				reader->close();
				ERR_FAIL_V_MSG(PackedStringArray(), vformat("Unable to open CSV split output '%s'.", output_path));
			}
			paths.push_back(output_path);
			part_index++;
			rows_in_part = 0;
		}
		if (!writer->write_row(row)) {
			reader->close();
			writer->close();
			ERR_FAIL_V_MSG(PackedStringArray(), "CSV split write failed.");
		}
		rows_in_part++;
	}
	Error read_error = reader->get_error();
	reader->close();
	if (writer.is_valid()) {
		writer->close();
	}
	ERR_FAIL_COND_V_MSG(read_error != OK, PackedStringArray(), "CSV split read failed.");
	return paths;
}

Error CSVChunkProcessor::merge_files(const PackedStringArray &p_paths, const String &p_output_path, const Ref<CSVDialect> &p_dialect, bool p_include_headers) {
	ERR_FAIL_COND_V_MSG(p_paths.is_empty(), ERR_INVALID_PARAMETER, "merge_files requires at least one input path.");
	ERR_FAIL_COND_V_MSG(p_output_path.is_empty(), ERR_INVALID_PARAMETER, "merge_files requires an output path.");

	Ref<CSVDialect> dialect = _chunk_get_dialect(p_dialect);
	Ref<CSVWriter> writer;
	writer.instantiate();
	Error err = writer->open(p_output_path, dialect->get_delimiter(), false);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Unable to open CSV merge output '%s'.", p_output_path));

	bool configured_headers = false;
	for (int path_index = 0; path_index < p_paths.size(); path_index++) {
		Ref<CSVReader> reader;
		err = _chunk_open_reader(reader, p_paths[path_index], dialect);
		if (err != OK) {
			writer->close();
			ERR_FAIL_V_MSG(err, vformat("Unable to open CSV merge input '%s'.", p_paths[path_index]));
		}
		while (true) {
			Variant row = reader->read_row();
			if (row.get_type() == Variant::NIL) {
				break;
			}
			if (!configured_headers) {
				_chunk_configure_writer(writer, dialect, p_include_headers, reader->get_header_names());
				configured_headers = true;
			}
			if (!writer->write_row(row)) {
				reader->close();
				writer->close();
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CSV merge write failed.");
			}
		}
		Error read_error = reader->get_error();
		reader->close();
		if (read_error != OK) {
			writer->close();
			ERR_FAIL_V_MSG(read_error, vformat("CSV merge read failed for '%s'.", p_paths[path_index]));
		}
	}
	writer->close();
	return OK;
}

int CSVChunkProcessor::count_rows(const String &p_path, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), -1, "count_rows requires a file path.");
	Ref<CSVReader> reader;
	Error err = _chunk_open_reader(reader, p_path, p_dialect);
	ERR_FAIL_COND_V_MSG(err != OK, -1, vformat("Unable to open CSV file '%s'.", p_path));
	int count = 0;
	while (true) {
		Variant row = reader->read_row();
		if (row.get_type() == Variant::NIL) {
			break;
		}
		count++;
	}
	Error read_error = reader->get_error();
	reader->close();
	ERR_FAIL_COND_V_MSG(read_error != OK, -1, "CSV row counting failed.");
	return count;
}

void CSVChunkProcessor::_bind_methods() {
	ClassDB::bind_static_method("CSVChunkProcessor", D_METHOD("process_file", "path", "callable", "chunk_size", "dialect"), &CSVChunkProcessor::process_file, DEFVAL(1000), DEFVAL(Ref<CSVDialect>()));
	ClassDB::bind_static_method("CSVChunkProcessor", D_METHOD("sample_file", "path", "count", "dialect"), &CSVChunkProcessor::sample_file, DEFVAL(10), DEFVAL(Ref<CSVDialect>()));
	ClassDB::bind_static_method("CSVChunkProcessor", D_METHOD("split_file", "path", "output_dir", "rows_per_file", "dialect", "include_headers"), &CSVChunkProcessor::split_file, DEFVAL(Ref<CSVDialect>()), DEFVAL(true));
	ClassDB::bind_static_method("CSVChunkProcessor", D_METHOD("merge_files", "paths", "output_path", "dialect", "include_headers"), &CSVChunkProcessor::merge_files, DEFVAL(Ref<CSVDialect>()), DEFVAL(true));
	ClassDB::bind_static_method("CSVChunkProcessor", D_METHOD("count_rows", "path", "dialect"), &CSVChunkProcessor::count_rows, DEFVAL(Ref<CSVDialect>()));
}

Ref<CSVAsyncTask> CSVAsyncTask::load_csv(const String &p_path, const String &p_delimiter, bool p_headers, bool p_cast_fields, const Dictionary &p_options) {
	Ref<CSVAsyncTask> task;
	task.instantiate();
	task->operation = OP_LOAD_CSV;
	task->path = p_path;
	task->delimiter = p_delimiter;
	task->headers = p_headers;
	task->cast_fields = p_cast_fields;
	task->options = p_options;
	return task;
}

Ref<CSVAsyncTask> CSVAsyncTask::load_dsv(const String &p_path, const String &p_delimiter, const Dictionary &p_options) {
	Ref<CSVAsyncTask> task;
	task.instantiate();
	task->operation = OP_LOAD_DSV;
	task->path = p_path;
	task->delimiter = p_delimiter;
	task->options = p_options;
	return task;
}

Ref<CSVAsyncTask> CSVAsyncTask::save_csv(const String &p_path, const Array &p_rows, const String &p_delimiter, bool p_headers, const Dictionary &p_options) {
	Ref<CSVAsyncTask> task;
	task.instantiate();
	task->operation = OP_SAVE_CSV;
	task->path = p_path;
	task->input_rows = p_rows.duplicate(true);
	task->delimiter = p_delimiter;
	task->headers = p_headers;
	task->options = p_options;
	return task;
}

Ref<CSVAsyncTask> CSVAsyncTask::save_dsv(const String &p_path, const Array &p_rows, const String &p_delimiter) {
	Ref<CSVAsyncTask> task;
	task.instantiate();
	task->operation = OP_SAVE_DSV;
	task->path = p_path;
	task->input_rows = p_rows.duplicate(true);
	task->delimiter = p_delimiter;
	return task;
}

Error CSVAsyncTask::start() {
	{
		MutexLock lock(mutex);
		ERR_FAIL_COND_V_MSG(thread_started || running, ERR_ALREADY_IN_USE, "CSVAsyncTask has already started.");
		running = true;
		done = false;
		cancelled = false;
		error = String();
		result_rows.clear();
		progress = 0.0;
		thread_started = true;
	}
	thread.start(_thread_func, this);
	return OK;
}

void CSVAsyncTask::cancel() {
	MutexLock lock(mutex);
	cancel_requested = true;
}

bool CSVAsyncTask::is_running() const {
	MutexLock lock(mutex);
	return running;
}

bool CSVAsyncTask::is_done() const {
	MutexLock lock(mutex);
	return done;
}

bool CSVAsyncTask::is_cancelled() const {
	MutexLock lock(mutex);
	return cancelled;
}

String CSVAsyncTask::get_error() const {
	MutexLock lock(mutex);
	return error;
}

double CSVAsyncTask::get_progress() const {
	MutexLock lock(mutex);
	return progress;
}

Array CSVAsyncTask::get_rows() const {
	MutexLock lock(mutex);
	return result_rows.duplicate(true);
}

Ref<CSVTable> CSVAsyncTask::get_table() const {
	return CSVTable::from_rows(get_rows());
}

void CSVAsyncTask::wait_to_finish() {
	bool should_wait = false;
	{
		MutexLock lock(mutex);
		should_wait = thread_started;
	}
	if (should_wait) {
		thread.wait_to_finish();
		MutexLock lock(mutex);
		thread_started = false;
	}
}

CSVAsyncTask::~CSVAsyncTask() {
	cancel();
	wait_to_finish();
}

void CSVAsyncTask::_set_progress(double p_progress) {
	const double clamped = _dotcsv_clamp_progress(p_progress);
	{
		MutexLock lock(mutex);
		if (clamped <= progress && clamped < 1.0) {
			return;
		}
		progress = clamped;
	}
	emit_signal("progress_changed", clamped);
}

void CSVAsyncTask::_finish_success(const Array &p_rows) {
	{
		MutexLock lock(mutex);
		result_rows = p_rows.duplicate(true);
		progress = 1.0;
		running = false;
		done = true;
		cancelled = false;
		error = String();
	}
	emit_signal("progress_changed", 1.0);
	emit_signal("completed");
}

void CSVAsyncTask::_finish_failed(const String &p_error) {
	{
		MutexLock lock(mutex);
		running = false;
		done = true;
		cancelled = false;
		error = p_error;
	}
	emit_signal("failed", p_error);
}

void CSVAsyncTask::_finish_cancelled() {
	{
		MutexLock lock(mutex);
		running = false;
		done = true;
		cancelled = true;
		error = String();
	}
	emit_signal("cancelled");
}

bool CSVAsyncTask::_is_cancel_requested() const {
	MutexLock lock(mutex);
	return cancel_requested;
}

void CSVAsyncTask::_run() {
	Operation local_operation;
	String local_path;
	String local_delimiter;
	bool local_headers;
	bool local_cast_fields;
	Dictionary local_options;
	Array local_rows;
	{
		MutexLock lock(mutex);
		local_operation = operation;
		local_path = path;
		local_delimiter = delimiter;
		local_headers = headers;
		local_cast_fields = cast_fields;
		local_options = options;
		local_rows = input_rows.duplicate(true);
	}

	if (_is_cancel_requested()) {
		_finish_cancelled();
		return;
	}

	switch (local_operation) {
		case OP_LOAD_CSV: {
			Ref<CSVReader> reader;
			reader.instantiate();
			Error err = reader->open(local_path, local_delimiter, local_headers, local_cast_fields, local_options);
			if (err != OK) {
				_finish_failed("Unable to open CSV file: " + local_path);
				return;
			}
			Array out;
			const uint64_t length = reader->get_length();
			while (true) {
				if (_is_cancel_requested()) {
					reader->close();
					_finish_cancelled();
					return;
				}
				Variant row = reader->read_row();
				if (row.get_type() == Variant::NIL) {
					if (reader->get_error() != OK) {
						reader->close();
						_finish_failed("CSV read failed at line " + itos(reader->get_line()) + ".");
						return;
					}
					if (reader->is_eof()) {
						break;
					}
				}
				out.push_back(row);
				if (length > 0) {
					_set_progress(double(reader->get_position()) / double(length));
				}
			}
			reader->close();
			_finish_success(out);
		} break;
		case OP_LOAD_DSV: {
			Ref<DSVReader> reader;
			reader.instantiate();
			Error err = reader->open(local_path, local_delimiter, local_options);
			if (err != OK) {
				_finish_failed("Unable to open DSV file: " + local_path);
				return;
			}
			Array out;
			const uint64_t length = reader->get_length();
			while (true) {
				if (_is_cancel_requested()) {
					reader->close();
					_finish_cancelled();
					return;
				}
				PackedStringArray row = reader->read_row();
				if (row.is_empty()) {
					if (reader->get_error() != OK) {
						reader->close();
						_finish_failed("DSV read failed at line " + itos(reader->get_line()) + ".");
						return;
					}
					if (reader->is_eof()) {
						break;
					}
				}
				out.push_back(row);
				if (length > 0) {
					_set_progress(double(reader->get_position()) / double(length));
				}
			}
			reader->close();
			_finish_success(out);
		} break;
		case OP_SAVE_CSV: {
			Ref<CSVWriter> writer;
			writer.instantiate();
			writer->set_headers(local_headers);
			if (local_options.has("header_names")) {
				writer->set_header_names(local_options["header_names"]);
			}
			if (local_options.has("color_as_hex")) {
				writer->set_color_as_hex(local_options["color_as_hex"]);
			}
			if (local_options.has("hex_include_alpha")) {
				writer->set_hex_include_alpha(local_options["hex_include_alpha"]);
			}
			Error err = writer->open(local_path, local_delimiter, false);
			if (err != OK) {
				_finish_failed("Unable to open CSV file for writing: " + local_path);
				return;
			}
			const int total = local_rows.size();
			for (int i = 0; i < total; i++) {
				if (_is_cancel_requested()) {
					writer->close();
					_finish_cancelled();
					return;
				}
				if (!writer->write_row(local_rows[i])) {
					writer->close();
					_finish_failed("CSV write failed at row " + itos(i) + ".");
					return;
				}
				_set_progress(total == 0 ? 1.0 : double(i + 1) / double(total));
			}
			writer->close();
			_finish_success();
		} break;
		case OP_SAVE_DSV: {
			Ref<DSVWriter> writer;
			writer.instantiate();
			Error err = writer->open(local_path, local_delimiter, false);
			if (err != OK) {
				_finish_failed("Unable to open DSV file for writing: " + local_path);
				return;
			}
			const int total = local_rows.size();
			for (int i = 0; i < total; i++) {
				if (_is_cancel_requested()) {
					writer->close();
					_finish_cancelled();
					return;
				}
				if (!writer->write_row(local_rows[i])) {
					writer->close();
					_finish_failed("DSV write failed at row " + itos(i) + ".");
					return;
				}
				_set_progress(total == 0 ? 1.0 : double(i + 1) / double(total));
			}
			writer->close();
			_finish_success();
		} break;
	}
}

void CSVAsyncTask::_bind_methods() {
	ClassDB::bind_static_method("CSVAsyncTask", D_METHOD("load_csv", "path", "delimiter", "headers", "cast_fields", "options"), &CSVAsyncTask::load_csv, DEFVAL(","), DEFVAL(false), DEFVAL(true), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVAsyncTask", D_METHOD("load_dsv", "path", "delimiter", "options"), &CSVAsyncTask::load_dsv, DEFVAL(","), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVAsyncTask", D_METHOD("save_csv", "path", "rows", "delimiter", "headers", "options"), &CSVAsyncTask::save_csv, DEFVAL(","), DEFVAL(false), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVAsyncTask", D_METHOD("save_dsv", "path", "rows", "delimiter"), &CSVAsyncTask::save_dsv, DEFVAL(","));
	ClassDB::bind_method(D_METHOD("start"), &CSVAsyncTask::start);
	ClassDB::bind_method(D_METHOD("cancel"), &CSVAsyncTask::cancel);
	ClassDB::bind_method(D_METHOD("is_running"), &CSVAsyncTask::is_running);
	ClassDB::bind_method(D_METHOD("is_done"), &CSVAsyncTask::is_done);
	ClassDB::bind_method(D_METHOD("is_cancelled"), &CSVAsyncTask::is_cancelled);
	ClassDB::bind_method(D_METHOD("get_error"), &CSVAsyncTask::get_error);
	ClassDB::bind_method(D_METHOD("get_progress"), &CSVAsyncTask::get_progress);
	ClassDB::bind_method(D_METHOD("get_rows"), &CSVAsyncTask::get_rows);
	ClassDB::bind_method(D_METHOD("get_table"), &CSVAsyncTask::get_table);
	ClassDB::bind_method(D_METHOD("wait_to_finish"), &CSVAsyncTask::wait_to_finish);

	ADD_SIGNAL(MethodInfo("completed"));
	ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("cancelled"));
	ADD_SIGNAL(MethodInfo("progress_changed", PropertyInfo(Variant::FLOAT, "progress")));
}

void CSVIndex::build(const Array &p_rows, const StringName &p_column, bool p_unique) {
	column = p_column;
	unique = p_unique;
	index.clear();
	duplicate_key_names = PackedStringArray();

	for (int i = 0; i < p_rows.size(); i++) {
		Dictionary row = _row_to_dictionary(p_rows[i]);
		Variant value = row.has(column) ? row[column] : Variant();
		String key = _csv_query_key(value);
		Array bucket;
		if (index.has(key)) {
			bucket = index[key];
			if (unique && !bucket.is_empty()) {
				String display_key = _csv_display_key(value);
				if (!duplicate_key_names.has(display_key)) {
					duplicate_key_names.push_back(display_key);
				}
			}
		}
		bucket.push_back(row);
		index[key] = bucket;
	}
}

Variant CSVIndex::get_row(const Variant &p_value) const {
	Array rows = get_all(p_value);
	if (rows.is_empty()) {
		return Variant();
	}
	return rows[0];
}

Array CSVIndex::get_all(const Variant &p_value) const {
	String key = _csv_query_key(p_value);
	if (!index.has(key)) {
		return Array();
	}
	Array rows = index[key];
	return rows.duplicate(true);
}

bool CSVIndex::has(const Variant &p_value) const {
	return index.has(_csv_query_key(p_value));
}

bool CSVIndex::is_unique() const {
	return unique;
}

PackedStringArray CSVIndex::duplicate_keys() const {
	return duplicate_key_names;
}

StringName CSVIndex::get_column() const {
	return column;
}

void CSVIndex::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_row", "value"), &CSVIndex::get_row);
	ClassDB::bind_method(D_METHOD("get_all", "value"), &CSVIndex::get_all);
	ClassDB::bind_method(D_METHOD("has", "value"), &CSVIndex::has);
	ClassDB::bind_method(D_METHOD("is_unique"), &CSVIndex::is_unique);
	ClassDB::bind_method(D_METHOD("duplicate_keys"), &CSVIndex::duplicate_keys);
	ClassDB::bind_method(D_METHOD("get_column"), &CSVIndex::get_column);
}

Ref<CSVTable> CSVTable::_from_array(const Array &p_rows) {
	Ref<CSVTable> table;
	table.instantiate();
	table->set_rows(p_rows);
	return table;
}

Ref<CSVTable> CSVTable::from_rows(const Array &p_rows) {
	return _from_array(p_rows);
}

Ref<CSVRowModel> CSVRowModel::infer_from_table(const Ref<CSVTable> &p_table, const Dictionary &p_options) {
	ERR_FAIL_COND_V(p_table.is_null(), Ref<CSVRowModel>());
	return infer_from_rows(p_table->get_rows(), p_options);
}

Ref<CSVTable> CSVTable::from_csv(const Ref<CSV> &p_csv) {
	ERR_FAIL_COND_V(p_csv.is_null(), Ref<CSVTable>());
	return _from_array(p_csv->get_rows());
}

Ref<CSVTable> CSVTable::from_file(const String &p_path, const String &p_delimiter, const Dictionary &p_options) {
	return _from_array(CSVImporter::import_with_headers(p_path, p_delimiter, true, p_options));
}

Ref<CSVTable> CSVTable::from_file_with_dialect(const String &p_path, const Ref<CSVDialect> &p_dialect) {
	ERR_FAIL_COND_V_MSG(p_dialect.is_null(), Ref<CSVTable>(), "from_file_with_dialect requires a valid CSVDialect.");
	return _from_array(CSVImporter::import_with_dialect(p_path, p_dialect));
}

void CSVTable::set_rows(const Array &p_rows) {
	rows = _normalize_rows(p_rows);
}

Array CSVTable::get_rows() const {
	return rows.duplicate(true);
}

int CSVTable::row_count() const {
	return rows.size();
}

PackedStringArray CSVTable::column_names() const {
	PackedStringArray names;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Array keys = row.keys();
		for (int j = 0; j < keys.size(); j++) {
			String key = String(keys[j]);
			if (!names.has(key)) {
				names.push_back(key);
			}
		}
	}
	return names;
}

Ref<CSVTable> CSVTable::where_equals(const StringName &p_column, const Variant &p_value) const {
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		if (row.has(p_column) && row[p_column] == p_value) {
			out.push_back(row);
		}
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::where_in(const StringName &p_column, const Array &p_values) const {
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		if (row.has(p_column) && _array_has_variant(p_values, row[p_column])) {
			out.push_back(row);
		}
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::filter_rows(const Callable &p_predicate) const {
	ERR_FAIL_COND_V_MSG(!p_predicate.is_valid(), Ref<CSVTable>(), "filter_rows requires a valid Callable.");
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Variant row_arg = row;
		Variant index_arg = i;
		const Variant *args[2] = { &row_arg, &index_arg };
		Callable::CallError call_error;
		Variant result;
		p_predicate.callp(args, 2, result, call_error);
		ERR_FAIL_COND_V_MSG(call_error.error != Callable::CallError::CALL_OK, Ref<CSVTable>(), "filter_rows predicate call failed.");
		if (bool(result)) {
			out.push_back(row);
		}
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::map_rows(const Callable &p_transform) const {
	ERR_FAIL_COND_V_MSG(!p_transform.is_valid(), Ref<CSVTable>(), "map_rows requires a valid Callable.");
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Variant row_arg = row;
		Variant index_arg = i;
		const Variant *args[2] = { &row_arg, &index_arg };
		Callable::CallError call_error;
		Variant result;
		p_transform.callp(args, 2, result, call_error);
		ERR_FAIL_COND_V_MSG(call_error.error != Callable::CallError::CALL_OK, Ref<CSVTable>(), "map_rows transform call failed.");
		out.push_back(_row_to_dictionary(result));
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::sort_by(const StringName &p_column, bool p_ascending) const {
	PackedStringArray columns;
	columns.push_back(String(p_column));
	return sort_by_columns(columns, p_ascending);
}

Ref<CSVTable> CSVTable::sort_by_columns(const PackedStringArray &p_columns, bool p_ascending) const {
	if (p_columns.is_empty() || rows.size() < 2) {
		return _from_array(rows);
	}
	Array out = rows.duplicate(true);
	for (int i = 1; i < out.size(); i++) {
		Dictionary current = out[i];
		int j = i - 1;
		while (j >= 0) {
			Dictionary previous = out[j];
			int compare = _row_transform_compare(previous, current, p_columns);
			if (p_ascending ? compare <= 0 : compare >= 0) {
				break;
			}
			out[j + 1] = previous;
			j--;
		}
		out[j + 1] = current;
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::limit(int p_count, int p_offset) const {
	ERR_FAIL_COND_V_MSG(p_count < 0, Ref<CSVTable>(), "limit count must be greater than or equal to zero.");
	ERR_FAIL_COND_V_MSG(p_offset < 0, Ref<CSVTable>(), "limit offset must be greater than or equal to zero.");
	Array out;
	for (int i = p_offset; i < rows.size() && out.size() < p_count; i++) {
		out.push_back(rows[i]);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::distinct(const PackedStringArray &p_columns) const {
	Array out;
	HashSet<String> seen;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		String key = _row_transform_key(row, p_columns);
		if (seen.has(key)) {
			continue;
		}
		seen.insert(key);
		out.push_back(row);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::with_column(const StringName &p_column, const Variant &p_value_or_callable) const {
	Array out;
	bool is_callable = p_value_or_callable.get_type() == Variant::CALLABLE;
	Callable callable;
	if (is_callable) {
		callable = p_value_or_callable;
		ERR_FAIL_COND_V_MSG(!callable.is_valid(), Ref<CSVTable>(), "with_column requires a valid Callable.");
	}
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Dictionary transformed = row.duplicate(true);
		if (is_callable) {
			Variant row_arg = row;
			Variant index_arg = i;
			const Variant *args[2] = { &row_arg, &index_arg };
			Callable::CallError call_error;
			Variant result;
			callable.callp(args, 2, result, call_error);
			ERR_FAIL_COND_V_MSG(call_error.error != Callable::CallError::CALL_OK, Ref<CSVTable>(), "with_column callable call failed.");
			transformed[p_column] = result;
		} else {
			transformed[p_column] = p_value_or_callable;
		}
		out.push_back(transformed);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::drop_columns(const PackedStringArray &p_columns) const {
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Dictionary transformed;
		Array keys = row.keys();
		for (int j = 0; j < keys.size(); j++) {
			Variant key = keys[j];
			if (_array_has_string_name(p_columns, StringName(key))) {
				continue;
			}
			transformed[key] = row[key];
		}
		out.push_back(transformed);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::select_columns(const PackedStringArray &p_columns) const {
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Dictionary selected;
		for (int j = 0; j < p_columns.size(); j++) {
			StringName column = p_columns[j];
			if (row.has(column)) {
				selected[column] = row[column];
			}
		}
		out.push_back(selected);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::rename_columns(const Dictionary &p_map) const {
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Dictionary renamed;
		Array keys = row.keys();
		for (int j = 0; j < keys.size(); j++) {
			Variant key = keys[j];
			Variant out_key = p_map.has(key) ? p_map[key] : key;
			renamed[out_key] = row[key];
		}
		out.push_back(renamed);
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::apply_model(const Ref<CSVRowModel> &p_model) const {
	ERR_FAIL_COND_V(p_model.is_null(), Ref<CSVTable>());
	return _from_array(p_model->cast_rows(rows));
}

Array CSVTable::validate_model(const Ref<CSVRowModel> &p_model) const {
	ERR_FAIL_COND_V(p_model.is_null(), Array());
	return p_model->validate_rows(rows);
}

Ref<CSVIndex> CSVTable::build_index(const StringName &p_column, bool p_unique) const {
	Ref<CSVIndex> csv_index;
	csv_index.instantiate();
	csv_index->build(rows, p_column, p_unique);
	return csv_index;
}

Dictionary CSVTable::group_by(const StringName &p_column) const {
	Dictionary groups;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary row = rows[i];
		Variant value = row.has(p_column) ? row[p_column] : Variant();
		String key = _csv_display_key(value);
		Array group;
		if (groups.has(key)) {
			group = groups[key];
		}
		group.push_back(row);
		groups[key] = group;
	}
	return groups;
}

Ref<CSVTable> CSVTable::inner_join(const Ref<CSVTable> &p_other, const StringName &p_left_column, const StringName &p_right_column) const {
	ERR_FAIL_COND_V(p_other.is_null(), Ref<CSVTable>());
	Ref<CSVIndex> right_index = p_other->build_index(p_right_column, false);
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary left = rows[i];
		Variant value = left.has(p_left_column) ? left[p_left_column] : Variant();
		Array matches = right_index->get_all(value);
		for (int j = 0; j < matches.size(); j++) {
			out.push_back(_merge_join_rows(left, matches[j], p_left_column, p_right_column));
		}
	}
	return _from_array(out);
}

Ref<CSVTable> CSVTable::left_join(const Ref<CSVTable> &p_other, const StringName &p_left_column, const StringName &p_right_column) const {
	ERR_FAIL_COND_V(p_other.is_null(), Ref<CSVTable>());
	Ref<CSVIndex> right_index = p_other->build_index(p_right_column, false);
	Array out;
	for (int i = 0; i < rows.size(); i++) {
		Dictionary left = rows[i];
		Variant value = left.has(p_left_column) ? left[p_left_column] : Variant();
		Array matches = right_index->get_all(value);
		if (matches.is_empty()) {
			out.push_back(left);
			continue;
		}
		for (int j = 0; j < matches.size(); j++) {
			out.push_back(_merge_join_rows(left, matches[j], p_left_column, p_right_column));
		}
	}
	return _from_array(out);
}

void CSVTable::_bind_methods() {
	ClassDB::bind_static_method("CSVTable", D_METHOD("from_rows", "rows"), &CSVTable::from_rows);
	ClassDB::bind_static_method("CSVTable", D_METHOD("from_csv", "csv"), &CSVTable::from_csv);
	ClassDB::bind_static_method("CSVTable", D_METHOD("from_file", "path", "delimiter", "options"), &CSVTable::from_file, DEFVAL(","), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("CSVTable", D_METHOD("from_file_with_dialect", "path", "dialect"), &CSVTable::from_file_with_dialect);
	ClassDB::bind_method(D_METHOD("set_rows", "rows"), &CSVTable::set_rows);
	ClassDB::bind_method(D_METHOD("get_rows"), &CSVTable::get_rows);
	ClassDB::bind_method(D_METHOD("row_count"), &CSVTable::row_count);
	ClassDB::bind_method(D_METHOD("column_names"), &CSVTable::column_names);
	ClassDB::bind_method(D_METHOD("where_equals", "column", "value"), &CSVTable::where_equals);
	ClassDB::bind_method(D_METHOD("where_in", "column", "values"), &CSVTable::where_in);
	ClassDB::bind_method(D_METHOD("filter_rows", "predicate"), &CSVTable::filter_rows);
	ClassDB::bind_method(D_METHOD("map_rows", "transform"), &CSVTable::map_rows);
	ClassDB::bind_method(D_METHOD("sort_by", "column", "ascending"), &CSVTable::sort_by, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("sort_by_columns", "columns", "ascending"), &CSVTable::sort_by_columns, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("limit", "count", "offset"), &CSVTable::limit, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("distinct", "columns"), &CSVTable::distinct, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("with_column", "column", "value_or_callable"), &CSVTable::with_column);
	ClassDB::bind_method(D_METHOD("drop_columns", "columns"), &CSVTable::drop_columns);
	ClassDB::bind_method(D_METHOD("select_columns", "columns"), &CSVTable::select_columns);
	ClassDB::bind_method(D_METHOD("rename_columns", "map"), &CSVTable::rename_columns);
	ClassDB::bind_method(D_METHOD("apply_model", "model"), &CSVTable::apply_model);
	ClassDB::bind_method(D_METHOD("validate_model", "model"), &CSVTable::validate_model);
	ClassDB::bind_method(D_METHOD("build_index", "column", "unique"), &CSVTable::build_index, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("group_by", "column"), &CSVTable::group_by);
	ClassDB::bind_method(D_METHOD("inner_join", "other", "left_column", "right_column"), &CSVTable::inner_join);
	ClassDB::bind_method(D_METHOD("left_join", "other", "left_column", "right_column"), &CSVTable::left_join);
}
