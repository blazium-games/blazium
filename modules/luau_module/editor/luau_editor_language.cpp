/**************************************************************************/
/*  luau_editor_language.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "luau_editor_language.h"

#include "luau_script_language.h"

LuauEditorLanguage *LuauEditorLanguage::singleton = nullptr;

Error LuauEditorLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<EditorLanguage::CompletionOption> *r_options, bool &r_force, String &r_call_hint) {
	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL_V(lang, ERR_UNAVAILABLE);
	return lang->complete_code(p_code, p_path, p_owner, r_options, r_force, r_call_hint);
}

Error LuauEditorLanguage::lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) {
	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL_V(lang, ERR_UNAVAILABLE);
	return lang->lookup_code(p_code, p_symbol, p_path, p_owner, r_result);
}

int32_t LuauEditorLanguage::find_function(const String &p_function, const String &p_code) const {
	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL_V(lang, -1);
	return lang->find_function(p_function, p_code);
}

void LuauEditorLanguage::format_code(String &r_code, uint32_t p_from_line, uint32_t p_to_line) const {
	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL(lang);
	lang->format_code(r_code, p_from_line, p_to_line);
}

bool LuauEditorLanguage::validate(const String &p_code, const String &p_path, List<ScriptError> *r_errors, List<Warning> *r_warnings, List<String> *r_functions, HashSet<int> *r_safe_lines) const {
	LuauScriptLanguage *lang = LuauScriptLanguage::get_singleton();
	ERR_FAIL_NULL_V(lang, false);

	List<luau_module::LuauScriptError> errors;
	List<luau_module::LuauWarning> warnings;
	const bool ok = lang->validate(p_code, p_path, r_functions, r_errors ? &errors : nullptr, r_warnings ? &warnings : nullptr, r_safe_lines);

	if (r_errors) {
		for (const luau_module::LuauScriptError &err : errors) {
			ScriptError out;
			out.path = err.path;
			out.start_line = err.start_line;
			out.start_column = err.start_column;
			out.end_line = err.end_line;
			out.end_column = err.end_column;
			out.message = err.message;
			r_errors->push_back(out);
		}
	}
	if (r_warnings) {
		for (const luau_module::LuauWarning &warning : warnings) {
			Warning out;
			out.start_line = warning.start_line;
			out.start_column = warning.start_column;
			out.end_line = warning.end_line;
			out.end_column = warning.end_column;
			out.string_code = warning.string_code;
			out.message = warning.message;
			r_warnings->push_back(out);
		}
	}
	return ok;
}

#endif // TOOLS_ENABLED
