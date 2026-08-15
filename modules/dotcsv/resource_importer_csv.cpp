/**************************************************************************/
/*  resource_importer_csv.cpp                                             */
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

#include "resource_importer_csv.h"

#include "dotcsv.h"

#include "core/io/resource_saver.h"

namespace {

PackedStringArray _split_option_values(const Variant &p_value) {
	PackedStringArray values;
	String text = String(p_value);
	if (text.is_empty()) {
		return values;
	}
	Vector<String> parts = text.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		values.push_back(parts[i].strip_edges());
	}
	return values;
}

} // namespace

String ResourceImporterCSV::get_importer_name() const {
	return "dotcsv";
}

String ResourceImporterCSV::get_visible_name() const {
	return "CSV";
}

void ResourceImporterCSV::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("csv");
	p_extensions->push_back("tsv");
	p_extensions->push_back("psv");
}

String ResourceImporterCSV::get_save_extension() const {
	return "res";
}

String ResourceImporterCSV::get_resource_type() const {
	return "CSV";
}

int ResourceImporterCSV::get_preset_count() const {
	return 0;
}

String ResourceImporterCSV::get_preset_name(int p_idx) const {
	return "";
}

void ResourceImporterCSV::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	Delimiter default_delimiter = DELIMITER_COMMA;
	if (p_path.get_extension().to_lower() == "tsv") {
		default_delimiter = DELIMITER_TAB;
	} else if (p_path.get_extension().to_lower() == "psv") {
		default_delimiter = DELIMITER_PIPE;
	}

	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "delimiter", PROPERTY_HINT_ENUM, "Comma,Semicolon,Tab,Pipe"), default_delimiter));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "headers"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "trim_fields"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "skip_empty_rows"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "row_length_mode", PROPERTY_HINT_ENUM, "Strict,Pad Short,Pad or Truncate,Jagged"), DotCSVOptions::ROW_LENGTH_STRICT));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "comment_prefixes", PROPERTY_HINT_PLACEHOLDER_TEXT, "#,//"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "null_values", PROPERTY_HINT_PLACEHOLDER_TEXT, "NULL,nil"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "true_values", PROPERTY_HINT_PLACEHOLDER_TEXT, "yes,on,1"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "false_values", PROPERTY_HINT_PLACEHOLDER_TEXT, "no,off,0"), ""));
}

bool ResourceImporterCSV::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterCSV::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	String delimiter = ",";
	switch ((int)p_options["delimiter"]) {
		case DELIMITER_COMMA:
			delimiter = ",";
			break;
		case DELIMITER_SEMICOLON:
			delimiter = ";";
			break;
		case DELIMITER_TAB:
			delimiter = "\t";
			break;
		case DELIMITER_PIPE:
			delimiter = "|";
			break;
	}

	Ref<CSV> csv;
	csv.instantiate();
	csv->set_headers((bool)p_options["headers"]);
	csv->set_delimiter(delimiter);
	csv->set_trim_fields((bool)p_options["trim_fields"]);
	csv->set_skip_empty_rows((bool)p_options["skip_empty_rows"]);
	csv->set_row_length_mode((int)p_options["row_length_mode"]);

	PackedStringArray comment_prefixes = _split_option_values(p_options["comment_prefixes"]);
	if (!comment_prefixes.is_empty()) {
		csv->set_comment_prefixes(comment_prefixes);
	}
	PackedStringArray null_values = _split_option_values(p_options["null_values"]);
	if (!null_values.is_empty()) {
		csv->set_null_values(null_values);
	}
	PackedStringArray true_values = _split_option_values(p_options["true_values"]);
	if (!true_values.is_empty()) {
		csv->set_true_values(true_values);
	}
	PackedStringArray false_values = _split_option_values(p_options["false_values"]);
	if (!false_values.is_empty()) {
		csv->set_false_values(false_values);
	}

	Error err = csv->load_file(p_source_file);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to load CSV from path '%s'.", p_source_file));
		return err;
	}

	const String save_path = p_save_path + ".res";
	err = ResourceSaver::save(csv, save_path);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to save CSV to path '%s'.", save_path));
		return err;
	}

	if (r_gen_files) {
		r_gen_files->push_back(save_path);
	}
	return OK;
}
