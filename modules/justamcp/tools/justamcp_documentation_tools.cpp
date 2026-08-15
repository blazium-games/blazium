/**************************************************************************/
/*  justamcp_documentation_tools.cpp                                      */
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

#ifdef TOOLS_ENABLED

#include "justamcp_documentation_tools.h"

#include "editor/doc/doc_tools.h"
#include "editor/doc/editor_help.h"

void JustAMCPDocumentationTools::_bind_methods() {
}

JustAMCPDocumentationTools::JustAMCPDocumentationTools() {
}

JustAMCPDocumentationTools::~JustAMCPDocumentationTools() {
}

DocTools *JustAMCPDocumentationTools::_get_doc_tools() const {
	return EditorHelp::get_doc_data();
}

bool JustAMCPDocumentationTools::_matches_query(const String &p_value, const String &p_query) const {
	return p_query.is_empty() || p_value.findn(p_query) != -1;
}

bool JustAMCPDocumentationTools::_resolve_class_name(const String &p_class_name, String &r_class_name) const {
	DocTools *docs = _get_doc_tools();

	if (!docs) {
		return false;
	}

	if (docs->class_list.has(p_class_name)) {
		r_class_name = p_class_name;

		return true;
	}

	for (const KeyValue<String, DocData::ClassDoc> &E : docs->class_list) {
		if (E.key.nocasecmp_to(p_class_name) == 0) {
			r_class_name = E.key;

			return true;
		}
	}

	return false;
}

Dictionary JustAMCPDocumentationTools::_class_summary(const DocData::ClassDoc &p_doc) const {
	Dictionary summary;

	summary["name"] = p_doc.name;

	summary["inherits"] = p_doc.inherits;

	summary["brief_description"] = p_doc.brief_description;

	summary["is_deprecated"] = p_doc.is_deprecated;

	summary["is_experimental"] = p_doc.is_experimental;

	summary["is_script_doc"] = p_doc.is_script_doc;

	if (!p_doc.script_path.is_empty()) {
		summary["script_path"] = p_doc.script_path;
	}

	summary["method_count"] = p_doc.methods.size();

	summary["property_count"] = p_doc.properties.size();

	summary["signal_count"] = p_doc.signals.size();

	summary["constant_count"] = p_doc.constants.size();

	return summary;
}

Dictionary JustAMCPDocumentationTools::_class_details(const DocData::ClassDoc &p_doc, bool p_include_members) const {
	if (p_include_members) {
		return DocData::ClassDoc::to_dict(p_doc);
	}

	Dictionary details = _class_summary(p_doc);

	details["description"] = p_doc.description;

	details["keywords"] = p_doc.keywords;

	if (p_doc.is_deprecated) {
		details["deprecated"] = p_doc.deprecated_message;
	}

	if (p_doc.is_experimental) {
		details["experimental"] = p_doc.experimental_message;
	}

	return details;
}

Dictionary JustAMCPDocumentationTools::_find_member(const DocData::ClassDoc &p_doc, const String &p_member_type, const String &p_member_name) const {
	String type = p_member_type.to_lower();

	if (type == "method" || type == "methods") {
		for (int i = 0; i < p_doc.methods.size(); i++) {
			if (p_doc.methods[i].name == p_member_name) {
				Dictionary member = DocData::MethodDoc::to_dict(p_doc.methods[i]);

				member["member_type"] = "method";

				return member;
			}
		}

	}

	else if (type == "constructor" || type == "constructors") {
		for (int i = 0; i < p_doc.constructors.size(); i++) {
			if (p_doc.constructors[i].name == p_member_name) {
				Dictionary member = DocData::MethodDoc::to_dict(p_doc.constructors[i]);

				member["member_type"] = "constructor";

				return member;
			}
		}

	}

	else if (type == "operator" || type == "operators") {
		for (int i = 0; i < p_doc.operators.size(); i++) {
			if (p_doc.operators[i].name == p_member_name) {
				Dictionary member = DocData::MethodDoc::to_dict(p_doc.operators[i]);

				member["member_type"] = "operator";

				return member;
			}
		}

	}

	else if (type == "signal" || type == "signals") {
		for (int i = 0; i < p_doc.signals.size(); i++) {
			if (p_doc.signals[i].name == p_member_name) {
				Dictionary member = DocData::MethodDoc::to_dict(p_doc.signals[i]);

				member["member_type"] = "signal";

				return member;
			}
		}

	}

	else if (type == "property" || type == "properties") {
		for (int i = 0; i < p_doc.properties.size(); i++) {
			if (p_doc.properties[i].name == p_member_name) {
				Dictionary member = DocData::PropertyDoc::to_dict(p_doc.properties[i]);

				member["member_type"] = "property";

				return member;
			}
		}

	}

	else if (type == "constant" || type == "constants") {
		for (int i = 0; i < p_doc.constants.size(); i++) {
			if (p_doc.constants[i].name == p_member_name) {
				Dictionary member = DocData::ConstantDoc::to_dict(p_doc.constants[i]);

				member["member_type"] = "constant";

				return member;
			}
		}

	}

	else if (type == "annotation" || type == "annotations") {
		for (int i = 0; i < p_doc.annotations.size(); i++) {
			if (p_doc.annotations[i].name == p_member_name) {
				Dictionary member = DocData::MethodDoc::to_dict(p_doc.annotations[i]);

				member["member_type"] = "annotation";

				return member;
			}
		}

	}

	else if (type == "theme_property" || type == "theme_properties") {
		for (int i = 0; i < p_doc.theme_properties.size(); i++) {
			if (p_doc.theme_properties[i].name == p_member_name) {
				Dictionary member = DocData::ThemeItemDoc::to_dict(p_doc.theme_properties[i]);

				member["member_type"] = "theme_property";

				return member;
			}
		}

	}

	else if (type == "enum" || type == "enums") {
		if (p_doc.enums.has(p_member_name)) {
			Dictionary member = DocData::EnumDoc::to_dict(p_doc.enums[p_member_name]);

			member["name"] = p_member_name;

			member["member_type"] = "enum";

			return member;
		}
	}

	return Dictionary();
}

Dictionary JustAMCPDocumentationTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "docs_list_classes") {
		return list_classes(p_args);
	}

	if (p_tool_name == "docs_search") {
		return search_documentation(p_args);
	}

	if (p_tool_name == "docs_get_class") {
		return get_class_documentation(p_args);
	}

	if (p_tool_name == "docs_get_member") {
		return get_member_documentation(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPDocumentationTools::list_classes(const Dictionary &p_args) {
	DocTools *docs = _get_doc_tools();

	if (!docs) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "Editor documentation database is unavailable.";

		return ret;
	}

	String query = p_args.get("query", "");

	int limit = p_args.get("limit", 200);

	if (limit <= 0) {
		limit = 200;
	}

	Array classes;

	for (const KeyValue<String, DocData::ClassDoc> &E : docs->class_list) {
		const DocData::ClassDoc &class_doc = E.value;

		if (!_matches_query(class_doc.name, query) && !_matches_query(class_doc.inherits, query) && !_matches_query(class_doc.brief_description, query)) {
			continue;
		}

		classes.push_back(_class_summary(class_doc));

		if (classes.size() >= limit) {
			break;
		}
	}

	Dictionary ret;

	ret["ok"] = true;

	ret["classes"] = classes;

	ret["count"] = classes.size();

	ret["total_available"] = docs->class_list.size();

	return ret;
}

Dictionary JustAMCPDocumentationTools::search_documentation(const Dictionary &p_args) {
	DocTools *docs = _get_doc_tools();

	if (!docs) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "Editor documentation database is unavailable.";

		return ret;
	}

	String query = p_args.get("query", "");

	String member_type = String(p_args.get("member_type", "")).to_lower();

	bool include_members = p_args.get("include_members", true);

	int limit = p_args.get("limit", 50);

	if (query.is_empty()) {
		return list_classes(p_args);
	}

	if (limit <= 0) {
		limit = 50;
	}

	Array matches;

	for (const KeyValue<String, DocData::ClassDoc> &E : docs->class_list) {
		const DocData::ClassDoc &class_doc = E.value;

		if (_matches_query(class_doc.name, query) || _matches_query(class_doc.inherits, query) || _matches_query(class_doc.brief_description, query) || _matches_query(class_doc.description, query)) {
			Dictionary match = _class_summary(class_doc);

			match["match_type"] = "class";

			matches.push_back(match);

			if (matches.size() >= limit) {
				break;
			}
		}

		if (!include_members) {
			continue;
		}

		auto add_member_match = [&](const String &p_type, const String &p_name, const String &p_description) {
			if (matches.size() >= limit) {
				return;
			}

			if (!member_type.is_empty() && member_type != p_type && member_type != p_type + "s") {
				return;
			}

			if (!_matches_query(p_name, query) && !_matches_query(p_description, query)) {
				return;
			}

			Dictionary match;

			match["match_type"] = p_type;

			match["class_name"] = class_doc.name;

			match["name"] = p_name;

			match["description"] = p_description;

			matches.push_back(match);
		}

		;

		for (int i = 0; i < class_doc.methods.size(); i++) {
			add_member_match("method", class_doc.methods[i].name, class_doc.methods[i].description);
		}

		for (int i = 0; i < class_doc.properties.size(); i++) {
			add_member_match("property", class_doc.properties[i].name, class_doc.properties[i].description);
		}

		for (int i = 0; i < class_doc.signals.size(); i++) {
			add_member_match("signal", class_doc.signals[i].name, class_doc.signals[i].description);
		}

		for (int i = 0; i < class_doc.constants.size(); i++) {
			add_member_match("constant", class_doc.constants[i].name, class_doc.constants[i].description);
		}

		if (matches.size() >= limit) {
			break;
		}
	}

	Dictionary ret;

	ret["ok"] = true;

	ret["query"] = query;

	ret["matches"] = matches;

	ret["count"] = matches.size();

	return ret;
}

Dictionary JustAMCPDocumentationTools::get_class_documentation(const Dictionary &p_args) {
	String class_name = p_args.get("class_name", p_args.get("class", ""));

	if (class_name.is_empty()) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "class_name is required.";

		return ret;
	}

	DocTools *docs = _get_doc_tools();

	String resolved_class;

	if (!docs || !_resolve_class_name(class_name, resolved_class)) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "Class documentation not found: " + class_name;

		return ret;
	}

	bool include_members = p_args.get("include_members", true);

	Dictionary ret;

	ret["ok"] = true;

	ret["class"] = _class_details(docs->class_list[resolved_class], include_members);

	return ret;
}

Dictionary JustAMCPDocumentationTools::get_member_documentation(const Dictionary &p_args) {
	String class_name = p_args.get("class_name", p_args.get("class", ""));

	String member_type = p_args.get("member_type", p_args.get("type", ""));

	String member_name = p_args.get("member_name", p_args.get("name", ""));

	if (class_name.is_empty() || member_type.is_empty() || member_name.is_empty()) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "class_name, member_type, and member_name are required.";

		return ret;
	}

	DocTools *docs = _get_doc_tools();

	String resolved_class;

	if (!docs || !_resolve_class_name(class_name, resolved_class)) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "Class documentation not found: " + class_name;

		return ret;
	}

	Dictionary member = _find_member(docs->class_list[resolved_class], member_type, member_name);

	if (member.is_empty()) {
		Dictionary ret;

		ret["ok"] = false;

		ret["error"] = "Member documentation not found: " + resolved_class + "." + member_name;

		return ret;
	}

	Dictionary ret;

	ret["ok"] = true;

	ret["class_name"] = resolved_class;

	ret["member"] = member;

	return ret;
}

#endif
