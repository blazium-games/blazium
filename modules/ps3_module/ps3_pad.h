/**************************************************************************/
/*  ps3_pad.h                                                             */
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

#include "core/object/class_db.h"

enum PS3PadButton {
	PS3_PAD_DPAD_LEFT = 0,
	PS3_PAD_DPAD_DOWN,
	PS3_PAD_DPAD_RIGHT,
	PS3_PAD_DPAD_UP,
	PS3_PAD_CROSS,
	PS3_PAD_CIRCLE,
	PS3_PAD_TRIANGLE,
	PS3_PAD_SQUARE,
	PS3_PAD_L1,
	PS3_PAD_R1,
	PS3_PAD_L2,
	PS3_PAD_R2,
	PS3_PAD_START,
	PS3_PAD_SELECT,
	PS3_PAD_L3,
	PS3_PAD_R3,
};

class PS3Pad : public Object {
	GDCLASS(PS3Pad, Object);

	int port = 0;

protected:
	static void _bind_methods();

public:
	void set_port(int p_port);
	int get_port() const;

	float get_pressure(int p_button) const;
	bool is_connected() const;
	Vector2 get_left_stick() const;
	Vector2 get_right_stick() const;
	void set_rumble(float p_weak, float p_strong);

	PS3Pad();
};
