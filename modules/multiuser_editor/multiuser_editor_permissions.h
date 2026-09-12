/**************************************************************************/
/*  multiuser_editor_permissions.h                                        */
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

#ifdef TOOLS_ENABLED

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

class MultiuserEditorPermissions : public RefCounted {
	GDCLASS(MultiuserEditorPermissions, RefCounted);

public:
	enum Role {
		ROLE_NONE = 0,
		ROLE_VIEWER = 1,
		ROLE_EDITOR = 2,
		ROLE_ADMIN = 4,
	};

	static int role_from_string(const String &p_role);
	static String role_to_string(int p_role_bit);

	void load_defaults();
	void apply_overrides(const String &p_overrides_setting);

	bool can_perform(const String &p_action, const String &p_role) const;
	bool can_perform_mask(const String &p_action, int p_role_mask) const;
	bool is_host_only(const String &p_action) const;
	bool is_known_action(const String &p_action) const;
	int get_action_mask(const String &p_action) const;

	void set_allow_widen_host_only(bool p_allow) { _allow_widen_host_only = p_allow; }
	bool get_allow_widen_host_only() const { return _allow_widen_host_only; }

protected:
	static void _bind_methods();

private:
	HashMap<String, int> matrix;
	HashSet<String> host_only;
	bool _allow_widen_host_only = false;

	void _register_action(const String &p_action, int p_mask, bool p_host_only);
	int _parse_role_mask(const String &p_csv) const;
};

VARIANT_ENUM_CAST(MultiuserEditorPermissions::Role);

#endif
