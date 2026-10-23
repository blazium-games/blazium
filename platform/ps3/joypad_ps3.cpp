/**************************************************************************/
/*  joypad_ps3.cpp                                                        */
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

#include "joypad_ps3.h"
#include "joypad_ps3_state.h"

#include "platform/ps3/ps3_wrapper.h"

JoypadPS3::JoypadPS3() {
	input = Input::get_singleton();
	JoypadPS3State::get_singleton()->initialize(0);
	connected = JoypadPS3State::get_singleton()->is_connected();
	if (connected) {
		input->joy_connection_changed(0, true, "PS3 Controller");
	}
}

void JoypadPS3::process() {
	if (!input) {
		return;
	}

	JoypadPS3State *state = JoypadPS3State::get_singleton();
	if (!state->update()) {
		if (connected) {
			input->joy_connection_changed(0, false, "PS3 Controller");
			connected = false;
		}
		return;
	}

	if (!connected) {
		input->joy_connection_changed(0, true, "PS3 Controller");
		connected = true;
	}

#ifdef PS3_ENABLED
	const padData &data = state->get_data();
	input->joy_button(0, JoyButton::A, data.BTN_CROSS != 0);
	input->joy_button(0, JoyButton::B, data.BTN_CIRCLE != 0);
	input->joy_button(0, JoyButton::Y, data.BTN_TRIANGLE != 0);
	input->joy_button(0, JoyButton::X, data.BTN_SQUARE != 0);

	input->joy_button(0, JoyButton::DPAD_UP, data.BTN_UP != 0);
	input->joy_button(0, JoyButton::DPAD_DOWN, data.BTN_DOWN != 0);
	input->joy_button(0, JoyButton::DPAD_LEFT, data.BTN_LEFT != 0);
	input->joy_button(0, JoyButton::DPAD_RIGHT, data.BTN_RIGHT != 0);

	input->joy_button(0, JoyButton::LEFT_SHOULDER, data.BTN_L1 != 0);
	input->joy_button(0, JoyButton::RIGHT_SHOULDER, data.BTN_R1 != 0);
	input->joy_button(0, JoyButton::LEFT_STICK, data.BTN_L3 != 0);
	input->joy_button(0, JoyButton::RIGHT_STICK, data.BTN_R3 != 0);
	input->joy_button(0, JoyButton::START, data.BTN_START != 0);
	input->joy_button(0, JoyButton::BACK, data.BTN_SELECT != 0);

	input->joy_axis(0, JoyAxis::LEFT_X, (data.ANA_L_H - 128) / 128.0f);
	input->joy_axis(0, JoyAxis::LEFT_Y, (data.ANA_L_V - 128) / 128.0f);
	input->joy_axis(0, JoyAxis::RIGHT_X, (data.ANA_R_H - 128) / 128.0f);
	input->joy_axis(0, JoyAxis::RIGHT_Y, (data.ANA_R_V - 128) / 128.0f);
#endif
}
