/**************************************************************************/
/*  luau_typecheck.cpp                                                    */
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

#include "analysis/luau_typecheck.h"

#include "core/error/error_macros.h"

#ifdef LUAU_MODULE_ANALYSIS_ENABLED
#include "Luau/Config.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/LinterConfig.h"
#endif

using namespace luau_module;

#ifdef LUAU_MODULE_ANALYSIS_ENABLED

namespace {

class SingleFileResolver : public Luau::FileResolver {
public:
	explicit SingleFileResolver(std::string p_name, std::string p_source) :
			module_name(std::move(p_name)), source(std::move(p_source)) {
	}

	std::optional<Luau::SourceCode> readSource(const Luau::ModuleName &name) override {
		if (name != module_name) {
			return std::nullopt;
		}
		return Luau::SourceCode{ source, Luau::SourceCode::Module };
	}

private:
	std::string module_name;
	std::string source;
};

class SingleConfigResolver : public Luau::ConfigResolver {
public:
	const Luau::Config &getConfig(const Luau::ModuleName &name) const override {
		(void)name;
		return default_config;
	}

private:
	Luau::Config default_config;
};

static void push_type_error(const Luau::TypeError &p_error, const String &p_path, List<ScriptLanguage::ScriptError> *r_errors) {
	if (!r_errors) {
		return;
	}
	ScriptLanguage::ScriptError err;
	err.path = p_path;
	err.line = p_error.location.begin.line + 1;
	err.column = p_error.location.begin.column + 1;
	err.message = String(Luau::toString(p_error).c_str());
	r_errors->push_back(err);
}

static void push_lint_warning(const Luau::LintWarning &p_warning, List<ScriptLanguage::Warning> *r_warnings) {
	if (!r_warnings) {
		return;
	}
	ScriptLanguage::Warning warning;
	warning.start_line = p_warning.location.begin.line + 1;
	warning.end_line = p_warning.location.end.line + 1;
	warning.code = static_cast<int>(p_warning.code);
	warning.string_code = String(Luau::LintWarning::getName(p_warning.code));
	warning.message = String(p_warning.text.c_str());
	r_warnings->push_back(warning);
}

} //namespace

#endif

bool LuauTypecheck::analyze(const String &p_source, const String &p_path, List<ScriptLanguage::ScriptError> *r_errors, List<ScriptLanguage::Warning> *r_warnings) {
#ifdef LUAU_MODULE_ANALYSIS_ENABLED
	CharString utf8 = p_source.utf8();
	const std::string module_name = p_path.is_empty() ? "luau_script" : p_path.utf8().get_data();
	const std::string source(utf8.get_data(), utf8.length());

	SingleFileResolver file_resolver(module_name, source);
	SingleConfigResolver config_resolver;

	Luau::FrontendOptions options;
	options.runLintChecks = true;
	Luau::Frontend frontend(&file_resolver, &config_resolver, options);

	const Luau::CheckResult result = frontend.check(module_name);

	for (const Luau::TypeError &error : result.errors) {
		push_type_error(error, p_path, r_errors);
	}

	for (const Luau::LintWarning &warning : result.lintResult.warnings) {
		push_lint_warning(warning, r_warnings);
	}

	return result.errors.empty();
#else
	(void)p_source;
	(void)p_path;
	(void)r_errors;
	(void)r_warnings;
	return true;
#endif
}
