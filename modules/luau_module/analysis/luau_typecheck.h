/**************************************************************************/
/*  luau_typecheck.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/list.h"

namespace luau_module {

struct LuauScriptError {
	String path;
	int start_line = -1;
	int start_column = -1;
	int end_line = -1;
	int end_column = -1;
	String message;
};

struct LuauWarning {
	int start_line = 0;
	int start_column = -1;
	int end_line = 0;
	int end_column = -1;
	String string_code;
	String message;
};

class LuauTypecheck {
public:
	static bool analyze(const String &p_source, const String &p_path, List<LuauScriptError> *r_errors, List<LuauWarning> *r_warnings);
};

} //namespace luau_module
