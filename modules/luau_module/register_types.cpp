/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "lua_compileoptions.h"
#include "lua_debug.h"
#include "lua_state.h"
#include "luau.h"
#include "luau_script.h"
#include "luau_script_language.h"
#include "require/luau_package_path.h"
#include "resource_loader_luau.h"
#include "resource_saver_luau.h"
#include "scheduler/luau_wait_signal_task.h"
#include "static_strings.h"
#include "string_cache.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"
#include "editor/lsp/luau_language_server.h"
#ifndef LUAU_NO_LSP
#include "editor/lsp/luau_language_protocol.h"
#include "editor/lsp/luau_text_document.h"
#include "editor/lsp/luau_workspace.h"
#endif
#include "editor/luau_editor_plugin.h"
#include "editor/luau_export_plugin.h"
#include "editor/luau_formatter.h"
#include "editor/luau_highlighter.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#endif

using namespace luau_module;

static LuauScriptLanguage *script_language_luau = nullptr;
static Ref<ResourceFormatLoaderLuau> resource_loader_luau;
static Ref<ResourceFormatSaverLuau> resource_saver_luau;

#ifdef TOOLS_ENABLED
static void _luau_editor_init() {
	Ref<LuauSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);

	Ref<EditorExportLuau> export_plugin;
	export_plugin.instantiate();
	EditorExport::get_singleton()->add_export_plugin(export_plugin);

	EDITOR_DEF("luau/editor/stylua_path", "");
	EDITOR_DEF("luau/editor/bytecode_compress_on_save", false);
	EDITOR_DEF("luau/editor/bytecode_encrypt_on_save", false);
	EDITOR_DEF("luau/editor/bytecode_minify_on_save", false);

	EditorPlugins::add_by_type<LuauEditorPlugin>();
	EditorPlugins::add_by_type<LuauLanguageServerPlugin>();
}
#endif

void initialize_luau_module_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		initialize_static_strings();
		initialize_string_cache();

		GDREGISTER_CLASS(Luau);
		GDREGISTER_CLASS(LuaCompileOptions);
		GDREGISTER_CLASS(LuaDebug);
		GDREGISTER_CLASS(LuaState);
		GDREGISTER_CLASS(LuauScript);
		GDREGISTER_CLASS(LuauScriptLanguage);
		GDREGISTER_CLASS(LuauSignalWaiter);

		script_language_luau = memnew(LuauScriptLanguage);
		ScriptServer::register_language(script_language_luau);

		GDREGISTER_CLASS(ResourceFormatLoaderLuau);
		resource_loader_luau.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_luau);

		GDREGISTER_CLASS(ResourceFormatSaverLuau);
		resource_saver_luau.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_luau);

#ifdef TOOLS_ENABLED
		EditorNode::add_init_callback(_luau_editor_init);
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(LuauSyntaxHighlighter);
		GDREGISTER_CLASS(LuauFormatter);
		GDREGISTER_CLASS(EditorExportLuau);
		GDREGISTER_CLASS(LuauEditorPlugin);
		GDREGISTER_CLASS(LuauLanguageServerPlugin);
#ifndef LUAU_NO_LSP
		GDREGISTER_CLASS(LuauLanguageProtocol);
		GDREGISTER_CLASS(LuauTextDocument);
		GDREGISTER_CLASS(LuauWorkspace);
#endif
	}
#endif
}

void uninitialize_luau_module_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ResourceSaver::remove_resource_format_saver(resource_saver_luau);
		resource_saver_luau.unref();

		ResourceLoader::remove_resource_format_loader(resource_loader_luau);
		resource_loader_luau.unref();

		if (script_language_luau) {
			ScriptServer::unregister_language(script_language_luau);
			memdelete(script_language_luau);
			script_language_luau = nullptr;
		}

		uninitialize_string_cache();
		uninitialize_static_strings();
	}
}
