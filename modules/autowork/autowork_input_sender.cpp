/**************************************************************************/
/*  autowork_input_sender.cpp                                             */
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

#include "autowork_input_sender.h"
#include "core/input/input.h"
#include "core/os/keyboard.h"

void AutoworkInputSender::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mouse_down", "button_index", "position", "target"), &AutoworkInputSender::mouse_down, DEFVAL(Vector2()), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("mouse_up", "button_index", "position", "target"), &AutoworkInputSender::mouse_up, DEFVAL(Vector2()), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("mouse_motion", "position", "relative", "target"), &AutoworkInputSender::mouse_motion, DEFVAL(Vector2()), DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("key_down", "keycode", "target"), &AutoworkInputSender::key_down, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("key_up", "keycode", "target"), &AutoworkInputSender::key_up, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("action_down", "action", "strength", "target"), &AutoworkInputSender::action_down, DEFVAL(1.0), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("action_up", "action", "target"), &AutoworkInputSender::action_up, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("reset_inputs"), &AutoworkInputSender::reset_inputs);
}

AutoworkInputSender::AutoworkInputSender() {
}

AutoworkInputSender::~AutoworkInputSender() {
}

void AutoworkInputSender::_send_event(Ref<InputEvent> p_event, Node *p_target) {
	if (p_target) {
		if (p_target->is_class("Control")) {
			p_target->call("_gui_input", p_event);
		} else if (p_target->is_class("Window") || p_target->is_class("Viewport")) {
			p_target->call("push_input", p_event);
		} else {
			if (p_target->has_method("_input")) {
				p_target->call("_input", p_event);
			}
			if (p_target->has_method("_unhandled_input")) {
				p_target->call("_unhandled_input", p_event);
			}
		}
	} else {
		Input::get_singleton()->parse_input_event(p_event);
	}
}

void AutoworkInputSender::mouse_down(int p_button_index, const Vector2 &p_position, Node *p_target) {
	Ref<InputEventMouseButton> mb;
	mb.instantiate();
	mb->set_button_index((MouseButton)p_button_index);
	mb->set_pressed(true);
	mb->set_position(p_position);
	mb->set_global_position(p_position);
	_send_event(mb, p_target);
}

void AutoworkInputSender::mouse_up(int p_button_index, const Vector2 &p_position, Node *p_target) {
	Ref<InputEventMouseButton> mb;
	mb.instantiate();
	mb->set_button_index((MouseButton)p_button_index);
	mb->set_pressed(false);
	mb->set_position(p_position);
	mb->set_global_position(p_position);
	_send_event(mb, p_target);
}

void AutoworkInputSender::mouse_motion(const Vector2 &p_position, const Vector2 &p_relative, Node *p_target) {
	Ref<InputEventMouseMotion> mm;
	mm.instantiate();
	mm->set_position(p_position);
	mm->set_global_position(p_position);
	mm->set_relative(p_relative);
	_send_event(mm, p_target);
}

void AutoworkInputSender::key_down(int p_keycode, Node *p_target) {
	Ref<InputEventKey> k;
	k.instantiate();
	k->set_keycode((Key)p_keycode);
	k->set_pressed(true);
	_send_event(k, p_target);
}

void AutoworkInputSender::key_up(int p_keycode, Node *p_target) {
	Ref<InputEventKey> k;
	k.instantiate();
	k->set_keycode((Key)p_keycode);
	k->set_pressed(false);
	_send_event(k, p_target);
}

void AutoworkInputSender::action_down(const StringName &p_action, float p_strength, Node *p_target) {
	Ref<InputEventAction> ea;
	ea.instantiate();
	ea->set_action(p_action);
	ea->set_pressed(true);
	ea->set_strength(p_strength);
	_send_event(ea, p_target);
}

void AutoworkInputSender::action_up(const StringName &p_action, Node *p_target) {
	Ref<InputEventAction> ea;
	ea.instantiate();
	ea->set_action(p_action);
	ea->set_pressed(false);
	ea->set_strength(0.0);
	_send_event(ea, p_target);
}

void AutoworkInputSender::reset_inputs() {
	// A robust input cleanup logic could go here; flushing all pressed keys or buttons back to UP events.
	// For now, relies on the test explicitly calling mouse_up or action_up.
}
