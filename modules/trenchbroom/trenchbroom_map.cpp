/**************************************************************************/
/*  trenchbroom_map.cpp                                                   */
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

#include "trenchbroom_map.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/variant/variant_utility.h"
#include "modules/trenchbroom/core/data.h"
#include "modules/trenchbroom/core/entity_assembler.h"
#include "modules/trenchbroom/core/geometry_generator.h"
#include "modules/trenchbroom/core/map_parser.h"
#include "modules/trenchbroom/fgd/blazium_fgd_entity_class.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#endif

static const String SIGNATURE = "[MAP]";

void TrenchbroomMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_local_map_file", "local_map_file"), &TrenchbroomMap::set_local_map_file);
	ClassDB::bind_method(D_METHOD("get_local_map_file"), &TrenchbroomMap::get_local_map_file);
	ClassDB::bind_method(D_METHOD("set_global_map_file", "global_map_file"), &TrenchbroomMap::set_global_map_file);
	ClassDB::bind_method(D_METHOD("get_global_map_file"), &TrenchbroomMap::get_global_map_file);
	ClassDB::bind_method(D_METHOD("set_map_settings", "map_settings"), &TrenchbroomMap::set_map_settings);
	ClassDB::bind_method(D_METHOD("get_map_settings"), &TrenchbroomMap::get_map_settings);
	ClassDB::bind_method(D_METHOD("set_build_flags", "build_flags"), &TrenchbroomMap::set_build_flags);
	ClassDB::bind_method(D_METHOD("get_build_flags"), &TrenchbroomMap::get_build_flags);
	ClassDB::bind_method(D_METHOD("set_hyperplane_size", "hyperplane_size"), &TrenchbroomMap::set_hyperplane_size);
	ClassDB::bind_method(D_METHOD("get_hyperplane_size"), &TrenchbroomMap::get_hyperplane_size);
	ClassDB::bind_method(D_METHOD("clear_children"), &TrenchbroomMap::clear_children);
	ClassDB::bind_method(D_METHOD("build"), &TrenchbroomMap::build);
	ClassDB::bind_method(D_METHOD("parse_map_data", "map_file"), &TrenchbroomMap::parse_map_data);
	ClassDB::bind_method(D_METHOD("verify"), &TrenchbroomMap::verify);
	ClassDB::bind_method(D_METHOD("_get_build_func"), &TrenchbroomMap::_get_build_func);
	ClassDB::bind_method(D_METHOD("_get_clear_func"), &TrenchbroomMap::_get_clear_func);

	ADD_SIGNAL(MethodInfo("build_failed"));
	ADD_SIGNAL(MethodInfo("build_complete"));
	ADD_SIGNAL(MethodInfo("build_progress", PropertyInfo(Variant::STRING, "step"), PropertyInfo(Variant::FLOAT, "progress")));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "local_map_file", PROPERTY_HINT_FILE, "*.map,*.vmf"), "set_local_map_file", "get_local_map_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "global_map_file", PROPERTY_HINT_GLOBAL_FILE, "*.map,*.vmf"), "set_global_map_file", "get_global_map_file");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "map_settings", PROPERTY_HINT_RESOURCE_TYPE, "TrenchbroomMapSettings"), "set_map_settings", "get_map_settings");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "build_flags", PROPERTY_HINT_FLAGS, "Unwrap UV2:1,Show Profiling Info:2,Disable Smooth Shading:4,Include Cordon Volumes:8"), "set_build_flags", "get_build_flags");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hyperplane_size", PROPERTY_HINT_RANGE, "256,2048,128"), "set_hyperplane_size", "get_hyperplane_size");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_build_func", PROPERTY_HINT_TOOL_BUTTON, "Build Map,CollisionShape3D", PROPERTY_USAGE_EDITOR), "", "_get_build_func");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_clear_func", PROPERTY_HINT_TOOL_BUTTON, "Clear Map,Skeleton3D", PROPERTY_USAGE_EDITOR), "", "_get_clear_func");

	BIND_BITFIELD_FLAG(UNWRAP_UV2);
	BIND_BITFIELD_FLAG(SHOW_PROFILE_INFO);
	BIND_BITFIELD_FLAG(DISABLE_SMOOTHING);
	BIND_BITFIELD_FLAG(INCLUDE_CORDON_VOLUMES);
}

Callable TrenchbroomMap::_get_build_func() const {
	return callable_mp(const_cast<TrenchbroomMap *>(this), &TrenchbroomMap::build);
}

Callable TrenchbroomMap::_get_clear_func() const {
	return callable_mp(const_cast<TrenchbroomMap *>(this), &TrenchbroomMap::clear_children);
}

void TrenchbroomMap::fail_build(const String &p_reason, bool p_notify) {
	ERR_PRINT(SIGNATURE + " " + p_reason);
	if (p_notify) {
		emit_signal("build_failed");
	}
}

Error TrenchbroomMap::verify() {
	map_file_internal = !global_map_file.is_empty() ? global_map_file : local_map_file;
	if (map_file_internal.is_empty()) {
		fail_build("Cannot build empty map file.");
		return ERR_INVALID_PARAMETER;
	}

	if (map_file_internal.begins_with("uid://")) {
		const ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(map_file_internal);
		if (!ResourceUID::get_singleton()->has_id(uid)) {
			fail_build(vformat("Error: failed to retrieve path for UID (%s)", map_file_internal));
			return ERR_DOES_NOT_EXIST;
		}
		map_file_internal = ResourceUID::get_singleton()->get_id_path(uid);
	}

	if (!FileAccess::exists(map_file_internal) && !FileAccess::exists(map_file_internal + ".import")) {
		fail_build(vformat("Map file %s does not exist.", map_file_internal));
		return ERR_DOES_NOT_EXIST;
	}

	return OK;
}

void TrenchbroomMap::clear_children() {
	while (get_child_count() > 0) {
		Node *child = get_child(0);
		remove_child(child);
		memdelete(child);
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		EditorInterface::get_singleton()->mark_scene_as_unsaved();
	}
#endif
}

void TrenchbroomMap::build() {
	uint64_t time_elapsed = Time::get_singleton()->get_ticks_msec();

	if (build_flags & SHOW_PROFILE_INFO) {
		TrenchbroomUtil::print_profile_info("Building...", SIGNATURE);
	}

	clear_children();

	const Error verify_err = verify();
	if (verify_err != OK) {
		fail_build(vformat("Verification failed: %s. Aborting map build", VariantUtilityFunctions::error_string(verify_err)), true);
		return;
	}

	if (map_settings.is_null()) {
		WARN_PRINT("Map assembler does not have a map settings provided and will use default map settings.");
		const String default_path = GLOBAL_GET("blazium/trenchbroom/default_map_settings");
		if (!default_path.is_empty() && ResourceLoader::exists(default_path)) {
			map_settings = ResourceLoader::load(default_path);
		}
		if (map_settings.is_null()) {
			map_settings = TrenchbroomDefaults::create_default_map_settings();
		}
	}

	emit_signal("build_progress", "Verifying map", 0.1);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	if (build_flags & SHOW_PROFILE_INFO) {
		print_line("\nPARSER");
	}

	const ParseData parse_data = parser->parse_map_data(map_file_internal, map_settings.ptr(), build_flags & SHOW_PROFILE_INFO);
	if (parse_data.entities.is_empty()) {
		fail_build("Map parsing produced no entities.", true);
		return;
	}
	emit_signal("build_progress", "Parsing complete", 0.3);

	LocalVector<EntityData> entities = parse_data.entities;
	LocalVector<::GroupData> groups = parse_data.groups;

	Ref<TrenchbroomGeometryGenerator> generator = memnew(TrenchbroomGeometryGenerator(map_settings.ptr(), hyperplane_size));
	if (build_flags & SHOW_PROFILE_INFO) {
		print_line("\nGEOMETRY GENERATOR");
	}

	const Error generate_error = generator->build(build_flags, entities);
	if (generate_error != OK) {
		fail_build(vformat("Geometry generation failed: %s", VariantUtilityFunctions::error_string(generate_error)), true);
		return;
	}
	emit_signal("build_progress", "Geometry complete", 0.7);

	Ref<TrenchbroomEntityAssembler> assembler = memnew(TrenchbroomEntityAssembler(map_settings.ptr()));
	if (build_flags & SHOW_PROFILE_INFO) {
		print_line("\nENTITY ASSEMBLER");
	}
	assembler->build(this, entities, groups);
	emit_signal("build_progress", "Assembly complete", 0.95);

	time_elapsed = Time::get_singleton()->get_ticks_msec() - time_elapsed;
	if (build_flags & SHOW_PROFILE_INFO) {
		print_line(vformat("\nCompleted in %s seconds", time_elapsed / 1000.0));
		print_line("");
		TrenchbroomUtil::print_profile_info("Build complete", SIGNATURE);
	}

	emit_signal("build_progress", "Build complete", 1.0);
	emit_signal("build_complete");
}

Dictionary TrenchbroomMap::parse_map_data(const String &p_map_file) {
	Ref<TrenchbroomMapSettings> settings = map_settings;
	if (settings.is_null()) {
		settings = TrenchbroomDefaults::create_default_map_settings();
	}

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	return parser->parse_map_data_dict(p_map_file, settings);
}
