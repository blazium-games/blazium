/**************************************************************************/
/*  multiuser_editor_permissions.cpp                                      */
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

#include "multiuser_editor_permissions.h"

#include "multiuser_editor_constants.h"

void MultiuserEditorPermissions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_defaults"), &MultiuserEditorPermissions::load_defaults);
	ClassDB::bind_method(D_METHOD("apply_overrides", "overrides"), &MultiuserEditorPermissions::apply_overrides);
	ClassDB::bind_method(D_METHOD("can_perform", "action", "role"), &MultiuserEditorPermissions::can_perform);
	ClassDB::bind_method(D_METHOD("is_host_only", "action"), &MultiuserEditorPermissions::is_host_only);
	ClassDB::bind_method(D_METHOD("is_known_action", "action"), &MultiuserEditorPermissions::is_known_action);

	ClassDB::bind_static_method("MultiuserEditorPermissions", D_METHOD("role_from_string", "role"), &MultiuserEditorPermissions::role_from_string);
	ClassDB::bind_static_method("MultiuserEditorPermissions", D_METHOD("role_to_string", "role_bit"), &MultiuserEditorPermissions::role_to_string);
	ClassDB::bind_method(D_METHOD("can_perform_mask", "action", "role_mask"), &MultiuserEditorPermissions::can_perform_mask);
	ClassDB::bind_method(D_METHOD("get_action_mask", "action"), &MultiuserEditorPermissions::get_action_mask);
	ClassDB::bind_method(D_METHOD("set_allow_widen_host_only", "allow"), &MultiuserEditorPermissions::set_allow_widen_host_only);
	ClassDB::bind_method(D_METHOD("get_allow_widen_host_only"), &MultiuserEditorPermissions::get_allow_widen_host_only);

	BIND_ENUM_CONSTANT(ROLE_NONE);
	BIND_ENUM_CONSTANT(ROLE_VIEWER);
	BIND_ENUM_CONSTANT(ROLE_EDITOR);
	BIND_ENUM_CONSTANT(ROLE_ADMIN);
}

int MultiuserEditorPermissions::role_from_string(const String &p_role) {
	const String r = p_role.strip_edges();
	if (r == multiuser_editor::kRoleViewer || r == "viewer") {
		return ROLE_VIEWER;
	}
	if (r == multiuser_editor::kRoleEditor || r == "editor") {
		return ROLE_EDITOR;
	}
	if (r == multiuser_editor::kRoleAdmin || r == "admin" || r == "Host" || r == "host") {
		return ROLE_ADMIN;
	}
	return ROLE_NONE;
}

String MultiuserEditorPermissions::role_to_string(int p_role_bit) {
	switch (p_role_bit) {
		case ROLE_VIEWER:
			return multiuser_editor::kRoleViewer;
		case ROLE_EDITOR:
			return multiuser_editor::kRoleEditor;
		case ROLE_ADMIN:
			return multiuser_editor::kRoleAdmin;
		default:
			return "None";
	}
}

void MultiuserEditorPermissions::_register_action(const String &p_action, int p_mask, bool p_host_only) {
	matrix[p_action] = p_mask;
	if (p_host_only) {
		host_only.insert(p_action);
	} else {
		host_only.erase(p_action);
	}
}

void MultiuserEditorPermissions::load_defaults() {
	matrix.clear();
	host_only.clear();

	const int VIEWER_PLUS = ROLE_VIEWER | ROLE_EDITOR | ROLE_ADMIN;
	const int EDITOR_PLUS = ROLE_EDITOR | ROLE_ADMIN;
	const int ADMIN_ONLY = ROLE_ADMIN;

	_register_action(multiuser_editor::kActionHandshake, VIEWER_PLUS, false);
	_register_action(multiuser_editor::kActionHandshakeAck, VIEWER_PLUS, true);
	_register_action(multiuser_editor::kActionAuthChallenge, VIEWER_PLUS, true);
	_register_action(multiuser_editor::kActionChat, VIEWER_PLUS, false);
	_register_action(multiuser_editor::kActionCursorUpdate, VIEWER_PLUS, false);
	_register_action(multiuser_editor::kActionSelect, VIEWER_PLUS, false);
	_register_action(multiuser_editor::kActionTelemetry, VIEWER_PLUS, false);
	_register_action(multiuser_editor::kActionFsSnapshotDone, VIEWER_PLUS, true);
	_register_action(multiuser_editor::kActionFileReject, VIEWER_PLUS, true);
	_register_action(multiuser_editor::kActionProjectSettingsSnapshot, VIEWER_PLUS, true);

	_register_action(multiuser_editor::kActionProperty, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionNodeAdd, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionNodeDelete, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionCrdt, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionCrdtSync, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionScriptAttach, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionScriptDetach, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileProposeBegin, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileProposeChunk, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileProposeEnd, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileProposeDelete, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileProposeMove, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileApplyBegin, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileApplyChunk, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileApplyEnd, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileApplyDelete, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionFileApplyMove, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionResourceSync, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionTileSync, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionVfxRestart, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionShaderAction, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionUnlockAll, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionMagicRepairRequest, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionGitRequest, EDITOR_PLUS, false);
	_register_action(multiuser_editor::kActionGitResponse, VIEWER_PLUS, true);

	_register_action(multiuser_editor::kActionProjectSetting, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionSceneSync, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionFsOp, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionFsMove, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionFsRemove, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionFsRefresh, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionTeamPlayStart, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionTeamPlayStop, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionMagicRepairStart, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionAutoworkTrigger, ADMIN_ONLY, true);
	_register_action(multiuser_editor::kActionGlobalUndo, ADMIN_ONLY, true);
}

int MultiuserEditorPermissions::_parse_role_mask(const String &p_csv) const {
	int mask = 0;
	const Vector<String> parts = p_csv.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		const String token = parts[i].strip_edges();
		if (token.is_empty()) {
			continue;
		}
		if (token == "*" || token.to_lower() == "all") {
			mask = ROLE_VIEWER | ROLE_EDITOR | ROLE_ADMIN;
			continue;
		}
		const int b = role_from_string(token);
		if (b != ROLE_NONE) {
			mask |= b;
		}
	}
	return mask;
}

void MultiuserEditorPermissions::apply_overrides(const String &p_overrides_setting) {
	const String trimmed = p_overrides_setting.strip_edges();
	if (trimmed.is_empty()) {
		return;
	}

	const Vector<String> entries = trimmed.split(";", false);
	for (int i = 0; i < entries.size(); i++) {
		const String entry = entries[i].strip_edges();
		if (entry.is_empty()) {
			continue;
		}
		const int eq = entry.find("=");
		if (eq <= 0) {
			continue;
		}
		const String action = entry.substr(0, eq).strip_edges();
		const String value = entry.substr(eq + 1).strip_edges();
		if (action.is_empty() || value.is_empty()) {
			continue;
		}

		if (!matrix.has(action)) {
			print_line(vformat("Multiuser editor permissions: ignoring override for unknown action '%s'", action));
			continue;
		}

		String role_part = value;
		bool host_only_flag = host_only.has(action);
		bool widening_host_only = false;
		const int at = value.find("@");
		if (at != -1) {
			role_part = value.substr(0, at).strip_edges();
			const String flags = value.substr(at + 1).strip_edges().to_lower();
			if (flags == "host_only" || flags == "host") {
				host_only_flag = true;
			} else if (flags == "any" || flags == "any_sender") {
				if (host_only.has(action) && !_allow_widen_host_only) {
					print_line(vformat("Multiuser editor permissions: refusing to widen host-only action '%s' to @any (set blazium/multiuser_editor/permissions/allow_widen_host_only=true to permit)", action));
					continue;
				}
				host_only_flag = false;
				widening_host_only = host_only.has(action);
			}
		}

		const int mask = _parse_role_mask(role_part);
		if (mask == 0) {
			continue;
		}

		print_line(vformat("Multiuser editor permissions: override applied action='%s' mask=0x%x host_only=%s%s",
				action, mask, host_only_flag ? "true" : "false",
				widening_host_only ? " (widen-host-only PERMITTED)" : ""));
		_register_action(action, mask, host_only_flag);
	}
}

bool MultiuserEditorPermissions::can_perform(const String &p_action, const String &p_role) const {
	const int role_bit = role_from_string(p_role);
	return can_perform_mask(p_action, role_bit);
}

bool MultiuserEditorPermissions::can_perform_mask(const String &p_action, int p_role_mask) const {
	if (p_role_mask == ROLE_NONE) {
		return false;
	}
	const HashMap<String, int>::ConstIterator it = matrix.find(p_action);
	if (!it) {
		return false;
	}
	return (it->value & p_role_mask) != 0;
}

bool MultiuserEditorPermissions::is_host_only(const String &p_action) const {
	return host_only.has(p_action);
}

bool MultiuserEditorPermissions::is_known_action(const String &p_action) const {
	return matrix.has(p_action);
}

int MultiuserEditorPermissions::get_action_mask(const String &p_action) const {
	const HashMap<String, int>::ConstIterator it = matrix.find(p_action);
	if (!it) {
		return 0;
	}
	return it->value;
}

#endif
