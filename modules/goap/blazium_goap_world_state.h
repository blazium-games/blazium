/**************************************************************************/
/*  blazium_goap_world_state.h                                            */
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

#include "core/object/ref_counted.h"

class BlaziumGoapWorldState : public RefCounted {
	GDCLASS(BlaziumGoapWorldState, RefCounted);

private:
	Dictionary state;

protected:
	static void _bind_methods();

public:
	Array get_elements() const;
	Variant get_state(const StringName &p_state_name, const Variant &p_default = Variant()) const;
	void set_state(const StringName &p_state_name, const Variant &p_value);
	void erase_state(const StringName &p_state_name);
	void clear_state();

	const Dictionary &get_state_dictionary() const { return state; }
	void set_state_dictionary(const Dictionary &p_state);

	static bool is_satisfied(const Variant &p_blackboard_value, const Variant &p_desired_value);

	BlaziumGoapWorldState();
	~BlaziumGoapWorldState();
};
