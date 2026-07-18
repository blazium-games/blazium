/**************************************************************************/
/*  test_semantic_filter_snapshot.cpp                                     */
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

#include "test_semantic_filter_snapshot.h"

#include "../semantic_asset_index.h"
#include "../semantic_search_filters.h"

void test_semantic_filter_snapshot_metadata_and_tags() {
	SemanticAssetIndex index;
	index.upsert_entry("res://characters/hero.tscn");
	index.upsert_entry("res://props/tree.tscn");

	const SemanticFilterSnapshot class_snapshot = index.build_filter_snapshot(
			PackedStringArray(), false, String(), "PackedScene");
	CHECK(class_snapshot.filter_error.is_empty());

	const SemanticFilterSnapshot path_snapshot = index.build_filter_snapshot(
			PackedStringArray(), false, "characters/.*", String());
	CHECK(path_snapshot.filter_error.is_empty());
	CHECK(path_snapshot.has_metadata_filter);
	CHECK(path_snapshot.allowed_paths.has("res://characters/hero.tscn"));
	CHECK(!path_snapshot.allowed_paths.has("res://props/tree.tscn"));

	HashMap<String, SemanticEntryMetadata> metadata;
	SemanticEntryMetadata hero_meta;
	hero_meta.asset_class = "PackedScene";
	metadata.insert("res://characters/hero.tscn", hero_meta);
	SemanticEntryMetadata tree_meta;
	tree_meta.asset_class = "Mesh";
	metadata.insert("res://props/tree.tscn", tree_meta);

	const SemanticFilterSnapshot built = SemanticSearchFilters::build_filter_snapshot(
			metadata, PackedStringArray(), false, String(), "PackedScene");
	CHECK(built.has_metadata_filter);
	CHECK(built.allowed_paths.has("res://characters/hero.tscn"));
	CHECK(!built.allowed_paths.has("res://props/tree.tscn"));

	const SemanticFilterSnapshot with_generation = index.build_filter_snapshot(
			PackedStringArray(), false, String(), String());
	CHECK(with_generation.mutation_generation >= 0);
}
