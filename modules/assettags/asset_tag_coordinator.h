/**************************************************************************/
/*  asset_tag_coordinator.h                                               */
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

#pragma once

#include "core/object/object.h"

class AssetTagCoordinator : public Object {
	GDCLASS(AssetTagCoordinator, Object);

	static AssetTagCoordinator *singleton;

protected:
	static void _bind_methods();

public:
	static AssetTagCoordinator *get_singleton();

	Error rename_tag(const String &p_old_name, const String &p_new_name);
	Error remove_tag(const String &p_tag_name);
	Error add_tag(const String &p_tag_name, const String &p_comment = String());
	Error update_tag_comment(const String &p_tag_name, const String &p_comment);
	Dictionary rename_tag_result(const String &p_old_name, const String &p_new_name);
	Dictionary remove_tag_result(const String &p_tag_name);
	void schedule_prune_removed_paths();

	static bool is_in_transaction();
	Error begin_transaction();
	Error commit_transaction();
	void abort_transaction();
	bool can_undo() const;
	Error undo_last_change();

	AssetTagCoordinator();
	~AssetTagCoordinator();
};

#ifdef TESTS_ENABLED
void assettags_reset_coordinator_test_state();
#endif

class AssetTagCoordinatorScope {
	bool active = false;

public:
	AssetTagCoordinatorScope() {
		if (AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton()) {
			active = coordinator->begin_transaction() == OK;
		}
	}

	~AssetTagCoordinatorScope() {
		if (active) {
			if (AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton()) {
				coordinator->abort_transaction();
			}
		}
	}

	bool is_active() const {
		return active;
	}

	Error commit() {
		if (!active) {
			return ERR_UNCONFIGURED;
		}
		if (AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton()) {
			const Error err = coordinator->commit_transaction();
			active = false;
			return err;
		}
		return ERR_UNAVAILABLE;
	}
};
