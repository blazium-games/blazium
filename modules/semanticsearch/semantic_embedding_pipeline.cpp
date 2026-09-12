/**************************************************************************/
/*  semantic_embedding_pipeline.cpp                                       */
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

#include "semantic_embedding_pipeline.h"

#include "embedding_provider.h"
#include "hash_vector_embedding.h"
#include "lexical_search_engine.h"
#include "semantic_search_backend_factory.h"

#include "modules/modules_enabled.gen.h"

#ifdef TOOLS_ENABLED
#include "core/os/thread.h"
#include "editor/file_system/editor_file_system.h"
#endif

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#endif

namespace {

void _fill_text_fields_from_path(const String &p_path, SemanticAssetEntry &r_entry) {
	r_entry.path = p_path;
	r_entry.caption = p_path.get_file();
	r_entry.asset_class = String();
	r_entry.path_segments = String();
#ifdef TOOLS_ENABLED
	if (Thread::is_main_thread() && EditorFileSystem::get_singleton()) {
		r_entry.asset_class = EditorFileSystem::get_singleton()->get_file_type(p_path);
		if (!r_entry.asset_class.is_empty()) {
			r_entry.caption += " " + r_entry.asset_class;
		}
	}
#endif
	const String folder_path = p_path.get_base_dir();
	if (folder_path != "res://") {
		r_entry.path_segments = folder_path.replace("res://", "").replace("/", " ");
		r_entry.caption += " " + r_entry.path_segments;
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (registry) {
		const PackedStringArray tags = registry->get_tags_for_asset(p_path);
		for (int i = 0; i < tags.size(); i++) {
			r_entry.caption += " " + tags[i];
		}
		if (manager) {
			for (int i = 0; i < tags.size(); i++) {
				const Dictionary info = manager->get_tag_info(tags[i]);
				if (info.get("ok", false)) {
					r_entry.caption += " " + String(info.get("comment", ""));
				}
			}
		}
	}
#endif
}

} //namespace

String SemanticEmbeddingPipeline::build_caption_from_path(const String &p_path) {
	SemanticAssetEntry entry;
	_fill_text_fields_from_path(p_path, entry);
	return entry.caption;
}

void SemanticEmbeddingPipeline::build_entry_from_path(const String &p_path, SemanticAssetEntry &r_entry) {
	_fill_text_fields_from_path(p_path, r_entry);
	r_entry.tokens = LexicalSearchEngine::tokenize(r_entry.caption);
	const String configured_backend = SemanticSearchBackendFactory::get_active_backend_name();
	if (configured_backend == "embedding" || configured_backend == "hybrid") {
		const Ref<EmbeddingProvider> provider = SemanticSearchBackendFactory::create_embedding_provider();
		const EmbeddingResult embed_result = provider->embed_tokens_result(r_entry.tokens);
		r_entry.embedding_provider = embed_result.effective_provider;
		r_entry.embedding_vector = embed_result.vector;
		if (!r_entry.embedding_vector.is_empty() && !HashVectorEmbedding::is_valid_embedding_dim(r_entry.embedding_vector.size())) {
			r_entry.embedding_vector.clear();
			r_entry.embedding_provider = String();
		}
	}
}
