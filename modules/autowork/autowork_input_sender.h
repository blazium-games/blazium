/**************************************************************************/
/*  autowork_input_sender.h                                               */
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

#include "core/input/input_event.h"
#include "core/object/ref_counted.h"
#include "scene/main/node.h"

class AutoworkInputSender : public RefCounted {
	GDCLASS(AutoworkInputSender, RefCounted);

	void _send_event(Ref<InputEvent> p_event, Node *p_target);

protected:
	static void _bind_methods();

public:
	AutoworkInputSender();
	~AutoworkInputSender();

	// Mouse
	void mouse_down(int p_button_index, const Vector2 &p_position = Vector2(), Node *p_target = nullptr);
	void mouse_up(int p_button_index, const Vector2 &p_position = Vector2(), Node *p_target = nullptr);
	void mouse_motion(const Vector2 &p_position, const Vector2 &p_relative = Vector2(), Node *p_target = nullptr);
	// Keyboard
	void key_down(int p_keycode, Node *p_target = nullptr);
	void key_up(int p_keycode, Node *p_target = nullptr);
	// Action
	void action_down(const StringName &p_action, float p_strength = 1.0, Node *p_target = nullptr);
	void action_up(const StringName &p_action, Node *p_target = nullptr);

	// Helpers
	void reset_inputs();
};
