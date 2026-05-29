/**************************************************************************/
/*  test_dotcsv.h                                                         */
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

#include "../dotcsv.h"

#include "core/io/file_access.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestDotCSV {

static int _dotcsv_test_chunk_total = 0;
static int _dotcsv_test_chunk_count = 0;

static bool _dotcsv_test_count_chunk(const Array &p_chunk, int p_index) {
	_dotcsv_test_chunk_total += p_chunk.size();
	_dotcsv_test_chunk_count++;
	return p_index >= 0;
}

static bool _dotcsv_test_keep_rpg(const Dictionary &p_row, int p_index) {
	return p_index >= 0 && p_row.has("genre") && String(p_row["genre"]) == "rpg";
}

static Dictionary _dotcsv_test_map_score_label(const Dictionary &p_row, int p_index) {
	Dictionary out;
	out["name"] = p_row["name"];
	out["slot"] = p_index;
	out["label"] = String(p_row["name"]) + ":" + String::num_int64((int64_t)p_row["score"]);
	return out;
}

static String _dotcsv_test_score_tier(const Dictionary &p_row, int p_index) {
	return (int64_t)p_row["score"] >= 9 || p_index == 0 ? "high" : "normal";
}

TEST_CASE("[DotCSV] DSVImporter parses quoted records and line endings") {
	Array rows = DSVImporter::import_string("aaa,\"b\r\nbb\",ccc\r\nxxx,\"y, yy\",zzz\r\n");

	REQUIRE(rows.size() == 2);
	PackedStringArray first = rows[0];
	PackedStringArray second = rows[1];

	CHECK(first.size() == 3);
	CHECK(first[0] == "aaa");
	CHECK(first[1] == "b\r\nbb");
	CHECK(first[2] == "ccc");
	CHECK(second[0] == "xxx");
	CHECK(second[1] == "y, yy");
	CHECK(second[2] == "zzz");
}

TEST_CASE("[DotCSV] DSVImporter rejects uneven rows") {
	ERR_PRINT_OFF;
	Array rows = DSVImporter::import_string("aaa,bbb,ccc\r\n111,222,333,444\r\nxxx,yyy,zzz\r\n");
	ERR_PRINT_ON;

	CHECK(rows.is_empty());
}

TEST_CASE("[DotCSV] DSVExporter quotes and pads records") {
	Array rows;
	rows.push_back(PackedStringArray({ "aaa", "b\r\nbb", "ccc" }));
	rows.push_back(PackedStringArray({ "xxx", "y, yy" }));

	CHECK(DSVExporter::export_string(rows) == "aaa,\"b\r\nbb\",ccc\r\nxxx,\"y, yy\",\r\n");
}

TEST_CASE("[DotCSV] CSVImporter converts variants and unifies column types") {
	Array rows = CSVImporter::import_string("0\r\n1.0\r\n2\r\n");

	REQUIRE(rows.size() == 3);
	Array first = rows[0];
	Array second = rows[1];
	Array third = rows[2];

	CHECK(first[0].get_type() == Variant::FLOAT);
	CHECK((double)first[0] == doctest::Approx(0.0));
	CHECK((double)second[0] == doctest::Approx(1.0));
	CHECK((double)third[0] == doctest::Approx(2.0));
}

TEST_CASE("[DotCSV] CSVImporter parses dictionaries with headers") {
	Array rows = CSVImporter::import_string_with_headers("name,enabled,count,color\r\nBlazium,true,3,#00ff00\r\n");

	REQUIRE(rows.size() == 1);
	Dictionary row = rows[0];

	CHECK(row["name"] == "Blazium");
	CHECK((bool)row["enabled"]);
	CHECK((int64_t)row["count"] == 3);
	CHECK(row["color"].get_type() == Variant::COLOR);
	CHECK(Color(row["color"]) == Color::html("#00ff00"));
}

TEST_CASE("[DotCSV] CSVExporter exports typed rows and dictionaries") {
	Array typed_rows;
	Array row;
	row.push_back(10);
	row.push_back(true);
	row.push_back(Variant());
	row.push_back("hello, world");
	typed_rows.push_back(row);

	CHECK(CSVExporter::export_string(typed_rows) == "10,true,,\"hello, world\"\r\n");

	Array dictionaries;
	Dictionary dict;
	dict["name"] = "Blazium";
	dict["enabled"] = true;
	dictionaries.push_back(dict);

	CHECK(CSVExporter::export_string_with_headers(dictionaries) == "name,enabled\r\nBlazium,true\r\n");
}

TEST_CASE("[DotCSV] CSV resource loads strings and files") {
	Ref<CSV> csv;
	csv.instantiate();
	csv->set_headers(true);
	CHECK(csv->load_string("name,count\r\nBlazium,3\r\n") == OK);

	TypedArray<Dictionary> rows = csv->get_rows();
	REQUIRE(rows.size() == 1);
	Dictionary row = rows[0];
	CHECK(row["name"] == "Blazium");
	CHECK((int64_t)row["count"] == 3);

	const String csv_path = TestUtils::get_temp_path("dotcsv_load_file.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name,count\r\nEngine,4\r\n");
	file.unref();

	CHECK(csv->load_file(csv_path) == OK);
	rows = csv->get_rows();
	REQUIRE(rows.size() == 1);
	row = rows[0];
	CHECK(row["name"] == "Engine");
	CHECK((int64_t)row["count"] == 4);
}

TEST_CASE("[DotCSV] Parser options trim comments and skip empty rows") {
	Dictionary options;
	options["trim_fields"] = true;
	options["skip_empty_rows"] = true;
	options["comment_prefixes"] = PackedStringArray({ "#", "//" });

	Array rows = DSVImporter::import_string("# skip\r\n a , b \r\n\r\n// skip too\r\n", ",", options);

	REQUIRE(rows.size() == 1);
	PackedStringArray row = rows[0];
	CHECK(row == PackedStringArray({ "a", "b" }));
}

TEST_CASE("[DotCSV] Parser options support row length modes") {
	Dictionary pad_short;
	pad_short["row_length_mode"] = DotCSVOptions::ROW_LENGTH_PAD_SHORT;
	Array rows = DSVImporter::import_string("a,b,c\r\n1,2\r\n", ",", pad_short);
	REQUIRE(rows.size() == 2);
	PackedStringArray padded = rows[1];
	CHECK(padded == PackedStringArray({ "1", "2", "" }));

	ERR_PRINT_OFF;
	Array rejected = DSVImporter::import_string("a,b\r\n1,2,3\r\n", ",", pad_short);
	ERR_PRINT_ON;
	CHECK(rejected.is_empty());

	Dictionary truncate;
	truncate["row_length_mode"] = DotCSVOptions::ROW_LENGTH_PAD_OR_TRUNCATE;
	rows = DSVImporter::import_string("a,b\r\n1,2,3\r\n4\r\n", ",", truncate);
	REQUIRE(rows.size() == 3);
	PackedStringArray truncated = rows[1];
	PackedStringArray padded_truncated = rows[2];
	CHECK(truncated == PackedStringArray({ "1", "2" }));
	CHECK(padded_truncated == PackedStringArray({ "4", "" }));

	Dictionary jagged;
	jagged["row_length_mode"] = DotCSVOptions::ROW_LENGTH_JAGGED;
	rows = DSVImporter::import_string("a,b\r\n1,2,3\r\n", ",", jagged);
	REQUIRE(rows.size() == 2);
	PackedStringArray jagged_row = rows[1];
	CHECK(jagged_row == PackedStringArray({ "1", "2", "3" }));
}

TEST_CASE("[DotCSV] CSVImporter supports custom typed values") {
	Dictionary options;
	options["null_values"] = PackedStringArray({ "NULL", "nil" });
	options["true_values"] = PackedStringArray({ "yes", "on" });
	options["false_values"] = PackedStringArray({ "no", "off" });

	Array rows = CSVImporter::import_string("NULL,yes,no,nil,on,off\r\n", ",", false, options);
	REQUIRE(rows.size() == 1);
	Array row = rows[0];
	CHECK(row[0].get_type() == Variant::NIL);
	CHECK((bool)row[1]);
	CHECK(!(bool)row[2]);
	CHECK(row[3].get_type() == Variant::NIL);
	CHECK((bool)row[4]);
	CHECK(!(bool)row[5]);
}

TEST_CASE("[DotCSV] CSV resource applies parser options") {
	Ref<CSV> csv;
	csv.instantiate();
	csv->set_headers(true);
	csv->set_trim_fields(true);
	csv->set_skip_empty_rows(true);
	csv->set_comment_prefixes(PackedStringArray({ "#" }));
	csv->set_row_length_mode(DotCSVOptions::ROW_LENGTH_PAD_SHORT);
	csv->set_null_values(PackedStringArray({ "", "NULL" }));
	csv->set_true_values(PackedStringArray({ "yes" }));
	csv->set_false_values(PackedStringArray({ "no" }));

	CHECK(csv->load_string("# comment\r\n name , enabled , missing \r\n Blazium , yes \r\n\r\n") == OK);
	TypedArray<Dictionary> rows = csv->get_rows();
	REQUIRE(rows.size() == 1);
	Dictionary row = rows[0];
	CHECK(row["name"] == "Blazium");
	CHECK((bool)row["enabled"]);
	CHECK(row["missing"].get_type() == Variant::NIL);
	CHECK(csv->convert_to_variant("NULL").get_type() == Variant::NIL);
}

TEST_CASE("[DotCSV] DSVReader streams rows and parser options") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_stream_reader.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("# skip\r\n name , quote \r\n Blazium , \"Hello, CSV\"\r\n\r\n");
	file.unref();

	Dictionary options;
	options["trim_fields"] = true;
	options["skip_empty_rows"] = true;
	options["comment_prefixes"] = PackedStringArray({ "#" });

	Ref<DSVReader> reader;
	reader.instantiate();
	CHECK(reader->open(csv_path, ",", options) == OK);

	PackedStringArray header = reader->read_row();
	CHECK(header == PackedStringArray({ "name", "quote" }));
	PackedStringArray row = reader->read_row();
	CHECK(row == PackedStringArray({ "Blazium", "Hello, CSV" }));
	CHECK(reader->read_row().is_empty());
	CHECK(reader->is_eof());
	CHECK(reader->get_error() == OK);
}

TEST_CASE("[DotCSV] CSVReader streams typed header dictionaries") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_stream_csv_reader.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name,enabled,count\r\nBlazium,yes,3\r\nEngine,no,4\r\n");
	file.unref();

	Dictionary options;
	options["true_values"] = PackedStringArray({ "yes" });
	options["false_values"] = PackedStringArray({ "no" });

	Ref<CSVReader> reader;
	reader.instantiate();
	CHECK(reader->open(csv_path, ",", true, true, options) == OK);

	Variant first_variant = reader->read_row();
	REQUIRE(first_variant.get_type() == Variant::DICTIONARY);
	Dictionary first = first_variant;
	CHECK(first["name"] == "Blazium");
	CHECK((bool)first["enabled"]);
	CHECK((int64_t)first["count"] == 3);

	Array rest = reader->read_rows(0);
	REQUIRE(rest.size() == 1);
	Dictionary second = rest[0];
	CHECK(second["name"] == "Engine");
	CHECK(!(bool)second["enabled"]);
	CHECK((int64_t)second["count"] == 4);
	CHECK(reader->is_eof());
}

TEST_CASE("[DotCSV] Streaming writers roundtrip through streaming readers") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_stream_writer.csv");

	Ref<CSVWriter> writer;
	writer.instantiate();
	writer->set_headers(true);
	writer->set_header_names(PackedStringArray({ "name", "count", "color" }));
	writer->set_color_as_hex(true);
	CHECK(writer->open(csv_path) == OK);

	Dictionary first;
	first["name"] = "Blazium";
	first["count"] = 3;
	first["color"] = Color::html("#00ff00");
	CHECK(writer->write_row(first));

	Dictionary second;
	second["name"] = "Engine";
	second["count"] = 4;
	second["color"] = Color::html("#0000ff");
	CHECK(writer->write_row(second));
	writer->close();

	Ref<CSVReader> reader;
	reader.instantiate();
	CHECK(reader->open(csv_path, ",", true) == OK);
	Array rows = reader->read_rows(0);
	REQUIRE(rows.size() == 2);
	Dictionary row = rows[0];
	CHECK(row["name"] == "Blazium");
	CHECK((int64_t)row["count"] == 3);
	CHECK(row["color"].get_type() == Variant::COLOR);
	CHECK(Color(row["color"]) == Color::html("#00ff00"));
}

TEST_CASE("[DotCSV] CSVDialect profiles import and roundtrip dictionaries") {
	Ref<CSVDialect> tsv = CSVDialect::from_profile("tsv");
	REQUIRE(tsv.is_valid());
	CHECK(tsv->get_delimiter() == "\t");
	Array tsv_rows = tsv->import_string("name\tcount\r\nBlazium\t3\r\n");
	REQUIRE(tsv_rows.size() == 2);
	CHECK((int64_t)Array(tsv_rows[1])[1] == 3);

	Dictionary data = tsv->to_dictionary();
	data["headers"] = true;
	Ref<CSVDialect> restored = CSVDialect::from_dictionary(data);
	CHECK(restored->get_delimiter() == "\t");
	CHECK(restored->get_headers());
	Array header_rows = restored->import_string("name\tcount\r\nBlazium\t3\r\n");
	REQUIRE(header_rows.size() == 1);
	CHECK(Dictionary(header_rows[0])["name"] == "Blazium");

	Ref<CSVDialect> excel = CSVDialect::from_profile("excel");
	Array excel_rows = excel->import_string(" name , count \r\n Blazium , 3 \r\n\r\n");
	REQUIRE(excel_rows.size() == 1);
	CHECK(Dictionary(excel_rows[0])["name"] == "Blazium");
	CHECK((int64_t)Dictionary(excel_rows[0])["count"] == 3);
}

TEST_CASE("[DotCSV] CSVDialect sniffs delimiters and integrates with importers") {
	Ref<CSVDialect> dialect = CSVDialect::sniff("name|count\r\nBlazium|3\r\nEngine|4\r\n");
	REQUIRE(dialect.is_valid());
	CHECK(dialect->get_delimiter() == "|");
	CHECK(dialect->get_headers());

	Array rows = CSVImporter::import_string_with_dialect("name|count\r\nBlazium|3\r\n", dialect);
	REQUIRE(rows.size() == 1);
	CHECK(Dictionary(rows[0])["name"] == "Blazium");
	CHECK((int64_t)Dictionary(rows[0])["count"] == 3);

	const String csv_path = TestUtils::get_temp_path("dotcsv_dialect_pipe.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name|count\r\nBlazium|3\r\n");
	file.unref();

	Array file_rows = CSVImporter::import_with_dialect(csv_path, dialect);
	REQUIRE(file_rows.size() == 1);
	CHECK(Dictionary(file_rows[0])["name"] == "Blazium");

	Ref<CSVReader> reader;
	reader.instantiate();
	CHECK(reader->open_with_dialect(csv_path, dialect) == OK);
	Variant row = reader->read_row();
	CHECK((int64_t)Dictionary(row)["count"] == 3);
	reader->close();

	Ref<CSVTable> table = CSVTable::from_file_with_dialect(csv_path, dialect);
	REQUIRE(table.is_valid());
	CHECK(table->row_count() == 1);
	CHECK(Dictionary(table->get_rows()[0])["name"] == "Blazium");
}

TEST_CASE("[DotCSV] CSVTable filters selects and renames rows") {
	Array rows;
	Dictionary first;
	first["name"] = "Blazium";
	first["genre"] = "engine";
	first["score"] = 10;
	rows.push_back(first);
	Dictionary second;
	second["name"] = "Game";
	second["genre"] = "rpg";
	second["score"] = 7;
	rows.push_back(second);

	Ref<CSVTable> table = CSVTable::from_rows(rows);
	REQUIRE(table.is_valid());
	CHECK(table->row_count() == 2);
	CHECK(table->column_names() == PackedStringArray({ "name", "genre", "score" }));

	Ref<CSVTable> filtered = table->where_equals("genre", "rpg");
	CHECK(filtered->row_count() == 1);
	Dictionary filtered_row = filtered->get_rows()[0];
	CHECK(filtered_row["name"] == "Game");

	Array scores;
	scores.push_back(7);
	CHECK(table->where_in("score", scores)->row_count() == 1);

	Ref<CSVTable> selected = table->select_columns(PackedStringArray({ "name", "score" }));
	Dictionary selected_row = selected->get_rows()[0];
	CHECK(selected_row.has("name"));
	CHECK(selected_row.has("score"));
	CHECK(!selected_row.has("genre"));

	Dictionary rename_map;
	rename_map["score"] = "rating";
	Ref<CSVTable> renamed = table->rename_columns(rename_map);
	Dictionary renamed_row = renamed->get_rows()[0];
	CHECK(renamed_row.has("rating"));
	CHECK(!renamed_row.has("score"));
}

TEST_CASE("[DotCSV] CSVIndex supports lookup and duplicate reporting") {
	Array rows;
	Dictionary first;
	first["id"] = 1;
	first["name"] = "Blazium";
	rows.push_back(first);
	Dictionary second;
	second["id"] = 1;
	second["name"] = "Duplicate";
	rows.push_back(second);
	Dictionary third;
	third["id"] = 2;
	third["name"] = "Engine";
	rows.push_back(third);

	Ref<CSVTable> table = CSVTable::from_rows(rows);
	Ref<CSVIndex> index = table->build_index("id", true);
	CHECK(index->is_unique());
	CHECK(index->has(1));
	CHECK(index->get_all(1).size() == 2);
	Dictionary row = index->get_row(2);
	CHECK(row["name"] == "Engine");
	CHECK(index->duplicate_keys() == PackedStringArray({ "1" }));
}

TEST_CASE("[DotCSV] CSVTable groups and joins rows") {
	Array people;
	Dictionary alice;
	alice["id"] = 1;
	alice["name"] = "Alice";
	alice["team_id"] = 10;
	people.push_back(alice);
	Dictionary bob;
	bob["id"] = 2;
	bob["name"] = "Bob";
	bob["team_id"] = 20;
	people.push_back(bob);
	Dictionary no_team;
	no_team["id"] = 3;
	no_team["name"] = "No Team";
	no_team["team_id"] = 30;
	people.push_back(no_team);

	Array teams;
	Dictionary red;
	red["team_id"] = 10;
	red["team"] = "Red";
	teams.push_back(red);
	Dictionary blue;
	blue["team_id"] = 20;
	blue["team"] = "Blue";
	teams.push_back(blue);

	Ref<CSVTable> people_table = CSVTable::from_rows(people);
	Dictionary groups = people_table->group_by("team_id");
	CHECK(Array(groups["10"]).size() == 1);
	CHECK(Array(groups["20"]).size() == 1);

	Ref<CSVTable> teams_table = CSVTable::from_rows(teams);
	Ref<CSVTable> joined = people_table->inner_join(teams_table, "team_id", "team_id");
	CHECK(joined->row_count() == 2);
	Dictionary joined_row = joined->get_rows()[0];
	CHECK(joined_row["name"] == "Alice");
	CHECK(joined_row["team"] == "Red");

	Ref<CSVTable> left_joined = people_table->left_join(teams_table, "team_id", "team_id");
	CHECK(left_joined->row_count() == 3);
}

TEST_CASE("[DotCSV] CSVChunkProcessor processes samples and counts large files") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_chunk_source.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name,count\r\nA,1\r\nB,2\r\nC,3\r\nD,4\r\nE,5\r\n");
	file.unref();

	Ref<CSVDialect> dialect = CSVDialect::from_profile("csv");
	dialect->set_headers(true);
	CHECK(CSVChunkProcessor::count_rows(csv_path, dialect) == 5);

	Array sample = CSVChunkProcessor::sample_file(csv_path, 2, dialect);
	REQUIRE(sample.size() == 2);
	CHECK(Dictionary(sample[0])["name"] == "A");
	CHECK((int64_t)Dictionary(sample[1])["count"] == 2);

	_dotcsv_test_chunk_total = 0;
	_dotcsv_test_chunk_count = 0;
	CHECK(CSVChunkProcessor::process_file(csv_path, callable_mp_static(&_dotcsv_test_count_chunk), 2, dialect) == OK);
	CHECK(_dotcsv_test_chunk_total == 5);
	CHECK(_dotcsv_test_chunk_count == 3);

	Ref<CSVDialect> tsv = CSVDialect::from_profile("tsv");
	const String tsv_path = TestUtils::get_temp_path("dotcsv_chunk_source.tsv");
	file = FileAccess::open(tsv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("A\t1\r\nB\t2\r\n");
	file.unref();
	CHECK(CSVChunkProcessor::count_rows(tsv_path, tsv) == 2);
	Array tsv_sample = CSVChunkProcessor::sample_file(tsv_path, 1, tsv);
	REQUIRE(tsv_sample.size() == 1);
	CHECK(PackedStringArray(tsv_sample[0]) == PackedStringArray({ "A", "1" }));
}

TEST_CASE("[DotCSV] CSVChunkProcessor splits merges and reports invalid arguments") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_chunk_split_source.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name,count\r\nA,1\r\nB,2\r\nC,3\r\nD,4\r\nE,5\r\n");
	file.unref();

	Ref<CSVDialect> dialect = CSVDialect::from_profile("csv");
	dialect->set_headers(true);
	const String output_dir = TestUtils::get_temp_path("dotcsv_chunk_split_parts");
	PackedStringArray parts = CSVChunkProcessor::split_file(csv_path, output_dir, 2, dialect, true);
	REQUIRE(parts.size() == 3);
	CHECK(CSVChunkProcessor::count_rows(parts[0], dialect) == 2);
	CHECK(CSVChunkProcessor::count_rows(parts[1], dialect) == 2);
	CHECK(CSVChunkProcessor::count_rows(parts[2], dialect) == 1);

	const String merged_path = TestUtils::get_temp_path("dotcsv_chunk_merged.csv");
	CHECK(CSVChunkProcessor::merge_files(parts, merged_path, dialect, true) == OK);
	CHECK(CSVChunkProcessor::count_rows(merged_path, dialect) == 5);
	Array merged = CSVImporter::import_with_dialect(merged_path, dialect);
	REQUIRE(merged.size() == 5);
	CHECK(Dictionary(merged[4])["name"] == "E");

	ERR_PRINT_OFF;
	CHECK(CSVChunkProcessor::process_file(csv_path, Callable(), 2, dialect) == ERR_INVALID_PARAMETER);
	CHECK(CSVChunkProcessor::sample_file(csv_path, -1, dialect).is_empty());
	CHECK(CSVChunkProcessor::split_file(csv_path, output_dir, 0, dialect, true).is_empty());
	CHECK(CSVChunkProcessor::merge_files(PackedStringArray(), merged_path, dialect, true) == ERR_INVALID_PARAMETER);
	CHECK(CSVChunkProcessor::count_rows("user://missing_chunk.csv", dialect) == -1);
	ERR_PRINT_ON;
}

TEST_CASE("[DotCSV] CSVAsyncTask loads CSV rows and table results") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_async_load.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("name,enabled,count\r\nBlazium,yes,3\r\nEngine,no,4\r\n");
	file.unref();

	Dictionary options;
	options["true_values"] = PackedStringArray({ "yes" });
	options["false_values"] = PackedStringArray({ "no" });
	Ref<CSVAsyncTask> task = CSVAsyncTask::load_csv(csv_path, ",", true, true, options);
	REQUIRE(task.is_valid());
	CHECK(task->start() == OK);
	task->wait_to_finish();

	CHECK(task->is_done());
	CHECK(!task->is_running());
	CHECK(!task->is_cancelled());
	CHECK(task->get_error().is_empty());
	CHECK(task->get_progress() == doctest::Approx(1.0));
	Array rows = task->get_rows();
	REQUIRE(rows.size() == 2);
	Dictionary first = rows[0];
	CHECK(first["name"] == "Blazium");
	CHECK((bool)first["enabled"]);
	CHECK((int64_t)first["count"] == 3);
	Ref<CSVTable> table = task->get_table();
	REQUIRE(table.is_valid());
	CHECK(table->row_count() == 2);
}

TEST_CASE("[DotCSV] CSVAsyncTask loads DSV rows") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_async_load_dsv.csv");
	Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("# skip\r\n a | b \r\n c | d \r\n");
	file.unref();

	Dictionary options;
	options["trim_fields"] = true;
	options["comment_prefixes"] = PackedStringArray({ "#" });
	Ref<CSVAsyncTask> task = CSVAsyncTask::load_dsv(csv_path, "|", options);
	CHECK(task->start() == OK);
	task->wait_to_finish();

	CHECK(task->is_done());
	CHECK(task->get_error().is_empty());
	Array rows = task->get_rows();
	REQUIRE(rows.size() == 2);
	CHECK(PackedStringArray(rows[0]) == PackedStringArray({ "a", "b" }));
	CHECK(PackedStringArray(rows[1]) == PackedStringArray({ "c", "d" }));
}

TEST_CASE("[DotCSV] CSVAsyncTask saves CSV rows") {
	const String csv_path = TestUtils::get_temp_path("dotcsv_async_save.csv");
	Array rows;
	Dictionary first;
	first["name"] = "Blazium";
	first["count"] = 3;
	rows.push_back(first);
	Dictionary second;
	second["name"] = "Engine";
	second["count"] = 4;
	rows.push_back(second);

	Dictionary options;
	options["header_names"] = PackedStringArray({ "name", "count" });
	Ref<CSVAsyncTask> task = CSVAsyncTask::save_csv(csv_path, rows, ",", true, options);
	CHECK(task->start() == OK);
	task->wait_to_finish();

	CHECK(task->is_done());
	CHECK(task->get_error().is_empty());
	CHECK(task->get_progress() == doctest::Approx(1.0));
	Array loaded = CSVImporter::import_with_headers(csv_path);
	REQUIRE(loaded.size() == 2);
	Dictionary loaded_first = loaded[0];
	CHECK(loaded_first["name"] == "Blazium");
	CHECK((int64_t)loaded_first["count"] == 3);
}

TEST_CASE("[DotCSV] CSVAsyncTask reports cancellation and errors") {
	Ref<CSVAsyncTask> cancelled = CSVAsyncTask::load_dsv("user://does_not_need_to_exist.csv");
	cancelled->cancel();
	CHECK(cancelled->start() == OK);
	cancelled->wait_to_finish();
	CHECK(cancelled->is_done());
	CHECK(cancelled->is_cancelled());
	CHECK(cancelled->get_error().is_empty());

	ERR_PRINT_OFF;
	Ref<CSVAsyncTask> failed = CSVAsyncTask::load_csv("user://missing_async.csv", "::");
	ERR_PRINT_ON;
	CHECK(failed->start() == OK);
	failed->wait_to_finish();
	CHECK(failed->is_done());
	CHECK(!failed->is_cancelled());
	CHECK(!failed->get_error().is_empty());
}

TEST_CASE("[DotCSV] CSVTable filters maps and mutates rows") {
	Array rows;
	Dictionary first;
	first["name"] = "A";
	first["genre"] = "rpg";
	first["score"] = 9;
	rows.push_back(first);
	Dictionary second;
	second["name"] = "B";
	second["genre"] = "puzzle";
	second["score"] = 7;
	rows.push_back(second);
	Dictionary third;
	third["name"] = "C";
	third["genre"] = "rpg";
	third["score"] = 8;
	rows.push_back(third);

	Ref<CSVTable> table = CSVTable::from_rows(rows);
	Ref<CSVTable> filtered = table->filter_rows(callable_mp_static(&_dotcsv_test_keep_rpg));
	REQUIRE(filtered.is_valid());
	CHECK(filtered->row_count() == 2);
	CHECK(Dictionary(filtered->get_rows()[1])["name"] == "C");

	Ref<CSVTable> mapped = table->map_rows(callable_mp_static(&_dotcsv_test_map_score_label));
	REQUIRE(mapped.is_valid());
	Array mapped_rows = mapped->get_rows();
	CHECK(Dictionary(mapped_rows[0])["label"] == "A:9");
	CHECK((int64_t)Dictionary(mapped_rows[2])["slot"] == 2);

	Ref<CSVTable> with_constant = table->with_column("source", "manual");
	CHECK(Dictionary(with_constant->get_rows()[0])["source"] == "manual");
	Ref<CSVTable> with_callable = table->with_column("tier", callable_mp_static(&_dotcsv_test_score_tier));
	CHECK(Dictionary(with_callable->get_rows()[0])["tier"] == "high");
	CHECK(Dictionary(with_callable->get_rows()[1])["tier"] == "normal");

	Ref<CSVTable> dropped = with_callable->drop_columns(PackedStringArray({ "genre", "tier" }));
	Dictionary dropped_row = dropped->get_rows()[0];
	CHECK(!dropped_row.has("genre"));
	CHECK(!dropped_row.has("tier"));
	CHECK(dropped_row.has("name"));
}

TEST_CASE("[DotCSV] CSVTable sorts limits and deduplicates rows") {
	Array rows;
	Dictionary first;
	first["name"] = "A";
	first["team"] = "blue";
	first["score"] = 9;
	rows.push_back(first);
	Dictionary second;
	second["name"] = "B";
	second["team"] = "red";
	second["score"] = 7;
	rows.push_back(second);
	Dictionary third;
	third["name"] = "C";
	third["team"] = "blue";
	third["score"] = 8;
	rows.push_back(third);
	Dictionary fourth = third.duplicate(true);
	rows.push_back(fourth);

	Ref<CSVTable> table = CSVTable::from_rows(rows);
	Ref<CSVTable> sorted_score = table->sort_by("score");
	CHECK(Dictionary(sorted_score->get_rows()[0])["name"] == "B");
	CHECK(Dictionary(sorted_score->get_rows()[3])["name"] == "A");

	Ref<CSVTable> sorted_desc = table->sort_by("score", false);
	CHECK(Dictionary(sorted_desc->get_rows()[0])["name"] == "A");

	Ref<CSVTable> sorted_columns = table->sort_by_columns(PackedStringArray({ "team", "score" }));
	CHECK(Dictionary(sorted_columns->get_rows()[0])["name"] == "C");
	CHECK(Dictionary(sorted_columns->get_rows()[2])["name"] == "A");
	CHECK(Dictionary(sorted_columns->get_rows()[3])["name"] == "B");

	Ref<CSVTable> page = sorted_score->limit(2, 1);
	CHECK(page->row_count() == 2);
	CHECK(Dictionary(page->get_rows()[0])["name"] == "C");

	Ref<CSVTable> unique_teams = table->distinct(PackedStringArray({ "team" }));
	CHECK(unique_teams->row_count() == 2);
	CHECK(Dictionary(unique_teams->get_rows()[0])["name"] == "A");
	Ref<CSVTable> unique_rows = table->distinct();
	CHECK(unique_rows->row_count() == 3);
}

TEST_CASE("[DotCSV] CSVRowModel saves and loads schema files") {
	Dictionary schema;
	Dictionary id_def;
	id_def["type"] = "int";
	id_def["required"] = true;
	schema["id"] = id_def;
	Dictionary color_def;
	color_def["type"] = "color";
	schema["tint"] = color_def;

	Ref<CSVRowModel> model = CSVRowModel::from_schema(schema);
	const String schema_path = TestUtils::get_temp_path("dotcsv_row_model_schema.treschema");
	CHECK(model->save_schema(schema_path) == OK);

	Ref<CSVRowModel> loaded = CSVRowModel::from_file(schema_path);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_schema() == schema);

	Ref<CSVRowModel> assigned;
	assigned.instantiate();
	CHECK(assigned->load_schema(schema_path) == OK);
	Dictionary casted = assigned->cast_row(Dictionary({ { "id", "12" }, { "tint", "#ff00ff" } }));
	CHECK((int64_t)casted["id"] == 12);
	CHECK(casted["tint"].get_type() == Variant::COLOR);
}

TEST_CASE("[DotCSV] CSVRowModel infers schemas from rows and tables") {
	Array rows;
	Dictionary first;
	first["id"] = "1";
	first["score"] = "2";
	first["enabled"] = "true";
	first["tint"] = "#ff00ff";
	first["name"] = "Alpha";
	rows.push_back(first);
	Dictionary second;
	second["id"] = "2";
	second["score"] = 3.5;
	second["enabled"] = false;
	second["tint"] = "#00ff00";
	second["name"] = "Beta";
	second["optional"] = Variant();
	rows.push_back(second);

	Ref<CSVRowModel> model = CSVRowModel::infer_from_rows(rows);
	Dictionary schema = model->get_schema();
	CHECK(Dictionary(schema["id"])["type"] == "int");
	CHECK(Dictionary(schema["score"])["type"] == "float");
	CHECK(Dictionary(schema["enabled"])["type"] == "bool");
	CHECK(Dictionary(schema["tint"])["type"] == "color");
	CHECK(Dictionary(schema["name"])["type"] == "string");
	CHECK(Dictionary(schema["optional"])["type"] == "variant");
	CHECK((bool)Dictionary(schema["id"])["required"]);
	CHECK(!(bool)Dictionary(schema["optional"])["required"]);

	Ref<CSVTable> table = CSVTable::from_rows(rows);
	Ref<CSVRowModel> table_model = CSVRowModel::infer_from_table(table, Dictionary({ { "required_by_default", true } }));
	Dictionary table_schema = table_model->get_schema();
	CHECK((bool)Dictionary(table_schema["optional"])["required"]);
}

TEST_CASE("[DotCSV] CSVRowModel casts defaults and validates rows") {
	Dictionary schema;
	Dictionary id_def;
	id_def["type"] = "int";
	id_def["required"] = true;
	schema["id"] = id_def;
	Dictionary name_def;
	name_def["type"] = "string";
	name_def["default"] = "";
	schema["name"] = name_def;
	Dictionary status_def;
	status_def["type"] = "string";
	Array status_enum;
	status_enum.push_back("new");
	status_enum.push_back("active");
	status_enum.push_back("done");
	status_def["enum"] = status_enum;
	schema["status"] = status_def;
	Dictionary enabled_def;
	enabled_def["type"] = "bool";
	enabled_def["default"] = true;
	schema["enabled"] = enabled_def;

	Ref<CSVRowModel> model = CSVRowModel::from_schema(schema);
	REQUIRE(model.is_valid());
	CHECK(model->get_field_names() == PackedStringArray({ "id", "name", "status", "enabled" }));

	Dictionary raw;
	raw["id"] = "7";
	raw["status"] = "active";
	Dictionary casted = model->cast_row(raw);
	CHECK((int64_t)casted["id"] == 7);
	CHECK(casted["name"] == "");
	CHECK(casted["status"] == "active");
	CHECK((bool)casted["enabled"]);

	Dictionary ok_report = model->validate_row(raw);
	CHECK((bool)ok_report["ok"]);
	CHECK(Array(ok_report["errors"]).is_empty());

	Dictionary invalid;
	invalid["status"] = "archived";
	Dictionary bad_report = model->validate_row(invalid);
	CHECK(!(bool)bad_report["ok"]);
	CHECK(Array(bad_report["errors"]).size() == 2);
}

TEST_CASE("[DotCSV] CSVRowModel casts array rows and serializes rows") {
	Dictionary schema;
	Dictionary id_def;
	id_def["type"] = "int";
	schema["id"] = id_def;
	Dictionary name_def;
	name_def["type"] = "string";
	name_def["default"] = "Unknown";
	schema["name"] = name_def;
	Dictionary enabled_def;
	enabled_def["type"] = "bool";
	enabled_def["default"] = false;
	schema["enabled"] = enabled_def;

	Ref<CSVRowModel> model = CSVRowModel::from_schema(schema);
	Array row;
	row.push_back("12");
	row.push_back("Blazium");
	row.push_back("yes");
	Dictionary casted = model->cast_row(row);
	CHECK((int64_t)casted["id"] == 12);
	CHECK(casted["name"] == "Blazium");
	CHECK((bool)casted["enabled"]);

	Dictionary partial;
	partial["id"] = 13;
	Dictionary serialized = model->serialize_row(partial, true);
	CHECK(serialized["name"] == "Unknown");
	CHECK(!(bool)serialized["enabled"]);
	Dictionary sparse = model->serialize_row(partial, false);
	CHECK(!sparse.has("name"));
	CHECK(!sparse.has("enabled"));
}

TEST_CASE("[DotCSV] CSVTable applies and validates row models") {
	Dictionary schema;
	Dictionary id_def;
	id_def["type"] = "int";
	id_def["required"] = true;
	schema["id"] = id_def;
	Dictionary enabled_def;
	enabled_def["type"] = "bool";
	enabled_def["default"] = true;
	schema["enabled"] = enabled_def;

	Array rows;
	Dictionary first;
	first["id"] = "1";
	first["enabled"] = "false";
	rows.push_back(first);
	Dictionary second;
	second["enabled"] = "true";
	rows.push_back(second);

	Ref<CSVRowModel> model = CSVRowModel::from_schema(schema);
	Ref<CSVTable> table = CSVTable::from_rows(rows);
	Ref<CSVTable> typed = table->apply_model(model);
	REQUIRE(typed.is_valid());
	Array typed_rows = typed->get_rows();
	REQUIRE(typed_rows.size() == 2);
	Dictionary typed_first = typed_rows[0];
	CHECK((int64_t)typed_first["id"] == 1);
	CHECK(!(bool)typed_first["enabled"]);

	Array reports = table->validate_model(model);
	REQUIRE(reports.size() == 2);
	Dictionary first_report = reports[0];
	Dictionary second_report = reports[1];
	CHECK((bool)first_report["ok"]);
	CHECK(!(bool)second_report["ok"]);
	CHECK((int64_t)second_report["index"] == 1);
}

} // namespace TestDotCSV
