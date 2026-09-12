/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "godot_sqlite.h"
#include "node_sqlite.h"
#include "resource_loader_sqlite.h"
#include "resource_saver_sqlite.h"
#include "resource_sqlite.h"

static Ref<ResourceFormatLoaderSQLite> sqlite_loader;
static Ref<ResourceFormatSaverSQLite> sqlite_saver;

void initialize_sqlite3_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}
	sqlite_loader.instantiate();
	sqlite_saver.instantiate();
	ResourceLoader::add_resource_format_loader(sqlite_loader);
	ResourceSaver::add_resource_format_saver(sqlite_saver);
	ClassDB::register_class<SQLiteDatabase>();
	ClassDB::register_class<SQLiteAccess>();
	ClassDB::register_class<SQLiteQuery>();
	ClassDB::register_class<SQLiteQueryResult>();
	ClassDB::register_class<SQLiteBackup>();
	ClassDB::register_class<SQLiteBlob>();
	ClassDB::register_class<SQLiteColumnSchema>();
	ClassDB::register_class<SQLite>();
}

void uninitialize_sqlite3_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}

	if (sqlite_loader.is_valid()) {
		ResourceLoader::remove_resource_format_loader(sqlite_loader);
		sqlite_loader.unref();
	}
	if (sqlite_saver.is_valid()) {
		ResourceSaver::remove_resource_format_saver(sqlite_saver);
		sqlite_saver.unref();
	}
}
