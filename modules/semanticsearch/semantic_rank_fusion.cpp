/**************************************************************************/
/*  semantic_rank_fusion.cpp                                              */
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

#include "semantic_rank_fusion.h"

#include "semantic_top_k.h"

#include "core/templates/hash_map.h"

Array SemanticRankFusion::reciprocal_rank_fusion(const Array &p_lexical_results, const Array &p_embedding_results, int p_limit, double p_k) {
	HashMap<String, double> fused_scores;
	HashMap<String, Dictionary> fused_items;
	const double denom_k = MAX(p_k, 1.0);

	auto accumulate = [&](const Array &p_results) {
		for (int i = 0; i < p_results.size(); i++) {
			const Dictionary item = p_results[i];
			const String path = item.get("path", "");
			if (path.is_empty()) {
				continue;
			}
			const double contribution = 1.0 / (denom_k + double(i + 1));
			fused_scores[path] = fused_scores.has(path) ? fused_scores[path] + contribution : contribution;
			if (!fused_items.has(path)) {
				fused_items[path] = item;
			}
		}
	};

	accumulate(p_lexical_results);
	accumulate(p_embedding_results);

	struct RankedItem {
		String path;
		double score = 0.0;
		Dictionary item;
	};

	Vector<RankedItem> ranked;
	ranked.resize(fused_scores.size());
	int write_index = 0;
	for (const KeyValue<String, double> &kv : fused_scores) {
		RankedItem entry;
		entry.path = kv.key;
		entry.score = kv.value;
		entry.item = fused_items[kv.key];
		ranked.write[write_index++] = entry;
	}
	ranked.resize(write_index);

	semantic_partial_sort_top_k(ranked, MAX(p_limit, 1), [](const RankedItem &a, const RankedItem &b) {
		return a.score > b.score;
	});

	Array results;
	for (int i = 0; i < ranked.size(); i++) {
		Dictionary item = ranked[i].item;
		item["score"] = ranked[i].score;
		results.push_back(item);
	}
	return results;
}
