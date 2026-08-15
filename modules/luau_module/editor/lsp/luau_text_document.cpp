/**************************************************************************/
/*  luau_text_document.cpp                                                */
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

#include "core/object/class_db.h"
#include "editor/lsp/luau_text_document.h"

#include "analysis/luau_analysis.h"
#include "analysis/luau_typecheck.h"
#include "editor/luau_completion.h"
#include "editor/luau_formatter.h"
#include "luau.h"
#include "luau_class_info.h"
#include "luau_compile_result.h"
#include "luau_script.h"
#include "luau_script_language.h"

#ifndef LUAU_NO_LSP
#include "editor/lsp/luau_language_protocol.h"
#endif

#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "core/string/char_utils.h"

namespace {

String uri_to_path(const String &p_uri) {
	if (p_uri.begins_with("file:///")) {
		return "res://" + p_uri.substr(8).replace("\\", "/");
	}
	return p_uri;
}

String symbol_at_position(const String &p_text, int p_line, int p_character) {
	const PackedStringArray lines = p_text.split("\n");
	if (p_line < 0 || p_line >= lines.size()) {
		return String();
	}

	const String line = lines[p_line];
	if (p_character < 0 || p_character > line.length()) {
		return String();
	}

	int start = p_character;
	while (start > 0 && (is_ascii_alphanumeric_char(line[start - 1]) || line[start - 1] == '_')) {
		start--;
	}

	int end = p_character;
	while (end < line.length() && (is_ascii_alphanumeric_char(line[end]) || line[end] == '_')) {
		end++;
	}

	if (start >= end) {
		return String();
	}

	return line.substr(start, end - start);
}

Dictionary make_range(int p_line, int p_character, int p_end_line = -1, int p_end_character = -1) {
	if (p_end_line < 0) {
		p_end_line = p_line;
	}
	if (p_end_character < 0) {
		p_end_character = p_character + 1;
	}

	return Dictionary({
			{ "start", Dictionary({ { "line", p_line }, { "character", p_character } }) },
			{ "end", Dictionary({ { "line", p_end_line }, { "character", p_end_character } }) },
	});
}

} //namespace

void LuauTextDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("didOpen", "params"), &LuauTextDocument::didOpen);
	ClassDB::bind_method(D_METHOD("didClose", "params"), &LuauTextDocument::didClose);
	ClassDB::bind_method(D_METHOD("didChange", "params"), &LuauTextDocument::didChange);
	ClassDB::bind_method(D_METHOD("didSave", "params"), &LuauTextDocument::didSave);
	ClassDB::bind_method(D_METHOD("completion", "params"), &LuauTextDocument::completion);
	ClassDB::bind_method(D_METHOD("definition", "params"), &LuauTextDocument::definition);
	ClassDB::bind_method(D_METHOD("hover", "params"), &LuauTextDocument::hover);
	ClassDB::bind_method(D_METHOD("documentSymbol", "params"), &LuauTextDocument::documentSymbol);
	ClassDB::bind_method(D_METHOD("formatting", "params"), &LuauTextDocument::formatting);
}

void LuauTextDocument::didOpen(const Dictionary &p_params) {
	const Dictionary doc = p_params.get("textDocument", Dictionary());
	const String uri = String(doc.get("uri", ""));
	did_open(uri, String(doc.get("text", "")));
	push_diagnostics(uri);
}

void LuauTextDocument::didClose(const Dictionary &p_params) {
	const Dictionary doc = p_params.get("textDocument", Dictionary());
	did_close(String(doc.get("uri", "")));
}

void LuauTextDocument::didChange(const Dictionary &p_params) {
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));
	const Array content_changes = p_params.get("contentChanges", Array());
	if (!content_changes.is_empty()) {
		const Dictionary change = content_changes[0];
		did_change(uri, String(change.get("text", "")));
	}
	push_diagnostics(uri);
}

void LuauTextDocument::didSave(const Dictionary &p_params) {
	const Dictionary doc = p_params.get("textDocument", Dictionary());
	const String uri = String(doc.get("uri", ""));
	did_save(uri, open_documents.has(uri) ? open_documents[uri] : String());
	push_diagnostics(uri);
}

Array LuauTextDocument::completion(const Dictionary &p_params) {
	const Dictionary pos = p_params.get("position", Dictionary());
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));
	return complete_at(uri, int(pos.get("line", 0)), int(pos.get("character", 0)));
}

Array LuauTextDocument::definition(const Dictionary &p_params) {
	const Dictionary pos = p_params.get("position", Dictionary());
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));

	Array locations;
	if (!open_documents.has(uri)) {
		return locations;
	}

	const String symbol = symbol_at_position(open_documents[uri], int(pos.get("line", 0)), int(pos.get("character", 0)));
	Dictionary loc = lookup_definition(uri, symbol);
	if (!loc.is_empty()) {
		locations.push_back(loc);
	}
	return locations;
}

Variant LuauTextDocument::hover(const Dictionary &p_params) {
	const Dictionary pos = p_params.get("position", Dictionary());
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));

	if (!open_documents.has(uri)) {
		return Variant();
	}

	const String symbol = symbol_at_position(open_documents[uri], int(pos.get("line", 0)), int(pos.get("character", 0)));
	if (symbol.is_empty()) {
		return Variant();
	}

	LuauClassInfo info;
	luau_module::LuauAnalysis::parse_annotations(open_documents[uri], &info);

	String hover_text;
	if (info.properties.has(StringName(symbol))) {
		const LuauClassProperty &prop = info.properties[StringName(symbol)];
		hover_text = "```luau\n" + String(Variant::get_type_name(prop.info.type)) + " " + symbol + "\n```";
		if (!prop.description.is_empty()) {
			hover_text += "\n\n" + prop.description;
		}
	} else if (info.methods.has(StringName(symbol))) {
		const LuauClassMethod &method = info.methods[StringName(symbol)];
		String signature = "function " + symbol + "(";
		for (int i = 0; i < method.info.arguments.size(); i++) {
			if (i > 0) {
				signature += ", ";
			}
			signature += method.info.arguments[i].name;
		}
		signature += ")";
		hover_text = "```luau\n" + signature + "\n```";
		if (!method.description.is_empty()) {
			hover_text += "\n\n" + method.description;
		}
	} else if (info.class_name == StringName(symbol)) {
		hover_text = "```luau\nclass " + symbol;
		if (!info.extends.is_empty()) {
			hover_text += " extends " + info.extends;
		}
		hover_text += "\n```";
		if (!info.class_description.is_empty()) {
			hover_text += "\n\n" + info.class_description;
		}
	} else if (ScriptServer::is_global_class(symbol)) {
		hover_text = "global class `" + symbol + "`";
	} else {
		return Variant();
	}

	Dictionary result;
	Dictionary contents;
	contents["kind"] = "markdown";
	contents["value"] = hover_text;
	result["contents"] = contents;
	return result;
}

Array LuauTextDocument::documentSymbol(const Dictionary &p_params) {
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));
	Array symbols;
	if (!open_documents.has(uri)) {
		return symbols;
	}
	LuauClassInfo info;
	luau_module::LuauAnalysis::parse_annotations(open_documents[uri], &info);
	if (!info.class_name.is_empty()) {
		Dictionary sym;
		sym["name"] = info.class_name;
		sym["kind"] = 5;
		Array children;
		for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
			Dictionary child;
			child["name"] = pair.key;
			child["kind"] = 8;
			children.push_back(child);
		}
		for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
			Dictionary child;
			child["name"] = pair.key;
			child["kind"] = 6;
			children.push_back(child);
		}
		for (const KeyValue<StringName, LuauClassSignal> &pair : info.signals) {
			Dictionary child;
			child["name"] = pair.key;
			child["kind"] = 13;
			children.push_back(child);
		}
		for (const KeyValue<StringName, Variant> &pair : info.constants) {
			Dictionary child;
			child["name"] = pair.key;
			child["kind"] = 14;
			children.push_back(child);
		}
		if (!children.is_empty()) {
			sym["children"] = children;
		}
		symbols.push_back(sym);
	}
	return symbols;
}

Array LuauTextDocument::formatting(const Dictionary &p_params) {
	Array edits;
	const Dictionary text_doc = p_params.get("textDocument", Dictionary());
	const String uri = String(text_doc.get("uri", ""));
	if (!open_documents.has(uri)) {
		return edits;
	}

	const String source = open_documents[uri];
	const String formatted = LuauFormatter::format_source(source);
	if (formatted == source) {
		return edits;
	}

	const PackedStringArray lines = source.split("\n");
	Dictionary range;
	Dictionary start;
	start["line"] = 0;
	start["character"] = 0;
	Dictionary end;
	end["line"] = MAX(lines.size() - 1, 0);
	end["character"] = lines.is_empty() ? 0 : lines[lines.size() - 1].length();
	range["start"] = start;
	range["end"] = end;

	Dictionary edit;
	edit["range"] = range;
	edit["newText"] = formatted;
	edits.push_back(edit);
	return edits;
}

void LuauTextDocument::initialize() {
	open_documents.clear();
}

void LuauTextDocument::did_open(const String &p_uri, const String &p_text) {
	open_documents[p_uri] = p_text;
}

void LuauTextDocument::did_change(const String &p_uri, const String &p_text) {
	open_documents[p_uri] = p_text;
}

void LuauTextDocument::did_save(const String &p_uri, const String &p_text) {
	open_documents[p_uri] = p_text;
	if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
		Ref<Script> scr = ResourceLoader::load(uri_to_path(p_uri));
		if (scr.is_valid()) {
			lang->reload_tool_script(scr, false);
		}
	}
}

void LuauTextDocument::did_close(const String &p_uri) {
	open_documents.erase(p_uri);
}

void LuauTextDocument::push_diagnostics(const String &p_uri) {
#ifndef LUAU_NO_LSP
	if (LuauLanguageProtocol *protocol = LuauLanguageProtocol::get_singleton()) {
		Dictionary params;
		params["uri"] = p_uri;
		params["diagnostics"] = publish_diagnostics(p_uri);
		protocol->notify_client("textDocument/publishDiagnostics", params);
	}
#else
	(void)p_uri;
#endif
}

String LuauTextDocument::get_text(const String &p_uri) const {
	if (open_documents.has(p_uri)) {
		return open_documents[p_uri];
	}
	return String();
}

Array LuauTextDocument::publish_diagnostics(const String &p_uri) const {
	Array diagnostics;
	if (!open_documents.has(p_uri)) {
		return diagnostics;
	}

	const String source = open_documents[p_uri];
	const String path = uri_to_path(p_uri);
	const luau_module::LuauCompileResult compile_result = luau_module::Luau::compile_with_diagnostics(source);
	if (compile_result.is_error() || compile_result.bytecode.is_empty()) {
		Dictionary diag;
		const int line = MAX((compile_result.error_line > 0 ? compile_result.error_line : 1) - 1, 0);
		diag["range"] = make_range(line, MAX(compile_result.error_column - 1, 0));
		diag["severity"] = 1;
		diag["message"] = compile_result.error_message.is_empty() ? "Luau compilation failed" : compile_result.error_message;
		diag["source"] = "luau";
		diagnostics.push_back(diag);
	}

#ifdef LUAU_MODULE_ANALYSIS_ENABLED
	List<ScriptLanguage::ScriptError> errors;
	List<ScriptLanguage::Warning> warnings;
	luau_module::LuauTypecheck::analyze(source, path, &errors, &warnings);
	for (const ScriptLanguage::ScriptError &err : errors) {
		Dictionary diag;
		const int line = MAX(err.line - 1, 0);
		diag["range"] = make_range(line, MAX(err.column - 1, 0));
		diag["severity"] = 1;
		diag["message"] = err.message;
		diag["source"] = "luau";
		diagnostics.push_back(diag);
	}
	for (const ScriptLanguage::Warning &warning : warnings) {
		Dictionary diag;
		const int line = MAX(warning.start_line - 1, 0);
		diag["range"] = make_range(line, 0, MAX(warning.end_line - 1, 0), 1);
		diag["severity"] = 2;
		diag["message"] = warning.message;
		diag["source"] = "luau";
		diagnostics.push_back(diag);
	}
#endif

	return diagnostics;
}

Array LuauTextDocument::complete_at(const String &p_uri, int p_line, int p_character) const {
	Array items;

	static const char *keywords[] = {
		"and",
		"break",
		"do",
		"else",
		"elseif",
		"end",
		"false",
		"for",
		"function",
		"if",
		"in",
		"local",
		"nil",
		"not",
		"or",
		"repeat",
		"return",
		"then",
		"true",
		"until",
		"while",
		"continue",
		"type",
		"export",
		"await",
		"wait",
		"wait_signal",
		nullptr,
	};

	LuauCompletionContext ctx;
	if (open_documents.has(p_uri)) {
		ctx = LuauCompletionHelper::extract_context(open_documents[p_uri], p_line, p_character);
	}

	auto add_item = [&](const String &p_label, int p_kind) {
		if (!LuauCompletionHelper::matches_prefix(p_label, ctx.prefix)) {
			return;
		}
		Dictionary item;
		item["label"] = p_label;
		item["kind"] = p_kind;
		items.push_back(item);
	};

	if (!ctx.wants_member) {
		for (const char **word = keywords; *word; word++) {
			add_item(*word, 14);
		}

		if (LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton()) {
			for (const String &w : lang->get_reserved_words()) {
				add_item(w, 14);
			}
		}

		LocalVector<StringName> global_classes;
		ScriptServer::get_global_class_list(global_classes);
		for (const StringName &class_name : global_classes) {
			add_item(class_name, 7);
		}
	}

	if (open_documents.has(p_uri)) {
		const String source = open_documents[p_uri];
		LuauClassInfo info;
		luau_module::LuauAnalysis::parse_annotations(source, &info);

		if (ctx.wants_member && !ctx.base.is_empty()) {
			if (ctx.base == "self" || ctx.base == info.class_name) {
				for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
					add_item(pair.key, 5);
				}
				for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
					add_item(pair.key, 3);
				}
			}
		} else {
			for (const KeyValue<StringName, LuauClassProperty> &pair : info.properties) {
				add_item(pair.key, 5);
			}
			for (const KeyValue<StringName, LuauClassMethod> &pair : info.methods) {
				add_item(pair.key, 3);
			}
		}
	}

	return items;
}

Dictionary LuauTextDocument::lookup_definition(const String &p_uri, const String &p_symbol) const {
	Dictionary result;
	if (p_symbol.is_empty() || !open_documents.has(p_uri)) {
		return result;
	}

	const String source = open_documents[p_uri];
	const String path = uri_to_path(p_uri);
	const int line = LuauScript::find_member_line_in_source(source, StringName(p_symbol));

	LuauClassInfo info;
	luau_module::LuauAnalysis::parse_annotations(source, &info);
	if (line >= 0 || info.properties.has(StringName(p_symbol)) || info.methods.has(StringName(p_symbol))) {
		result["uri"] = p_uri;
		result["range"] = make_range(line >= 0 ? line : 0, 0);
		return result;
	}

	if (ScriptServer::is_global_class(p_symbol)) {
		const String class_path = ScriptServer::get_global_class_path(p_symbol);
		if (!class_path.is_empty()) {
			result["uri"] = "file:///" + class_path.replace("res://", "").replace("\\", "/");
			result["range"] = make_range(0, 0);
		}
	}
	return result;
}

#endif
