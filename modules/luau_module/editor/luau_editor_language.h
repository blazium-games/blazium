/**************************************************************************/
/*  luau_editor_language.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#ifdef TOOLS_ENABLED

#include "core/object/editor_language.h"

class LuauEditorLanguage final : public EditorLanguage {
	static LuauEditorLanguage *singleton;

public:
	_FORCE_INLINE_ static LuauEditorLanguage *get_singleton() { return singleton; }

	virtual Error complete_code(const String &p_code, const String &p_path, Object *p_owner, List<EditorLanguage::CompletionOption> *r_options, bool &r_force, String &r_call_hint) override;
	virtual Error lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) override;
	virtual int32_t find_function(const String &p_function, const String &p_code) const override;
	virtual void format_code(String &r_code, uint32_t p_from_line, uint32_t p_to_line) const override;
	virtual bool validate(const String &p_code, const String &p_path, List<ScriptError> *r_errors, List<Warning> *r_warnings, List<String> *r_functions, HashSet<int> *r_safe_lines) const override;

	LuauEditorLanguage() {
		ERR_FAIL_COND(singleton != nullptr);
		singleton = this;
	}
	~LuauEditorLanguage() {
		if (singleton == this) {
			singleton = nullptr;
		}
	}
};

#endif // TOOLS_ENABLED
