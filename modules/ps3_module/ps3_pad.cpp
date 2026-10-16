#include "ps3_pad.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "joypad_ps3_state.h"

#ifdef PS3_ENABLED
#include <io/pad.h>
#endif

void PS3Pad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_port", "port"), &PS3Pad::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &PS3Pad::get_port);
	ClassDB::bind_method(D_METHOD("get_pressure", "button"), &PS3Pad::get_pressure);
	ClassDB::bind_method(D_METHOD("is_connected"), &PS3Pad::is_connected);
	ClassDB::bind_method(D_METHOD("get_left_stick"), &PS3Pad::get_left_stick);
	ClassDB::bind_method(D_METHOD("get_right_stick"), &PS3Pad::get_right_stick);
	ClassDB::bind_method(D_METHOD("set_rumble", "weak", "strong"), &PS3Pad::set_rumble);

	BIND_CONSTANT(PS3_PAD_DPAD_LEFT);
	BIND_CONSTANT(PS3_PAD_DPAD_DOWN);
	BIND_CONSTANT(PS3_PAD_DPAD_RIGHT);
	BIND_CONSTANT(PS3_PAD_DPAD_UP);
	BIND_CONSTANT(PS3_PAD_CROSS);
	BIND_CONSTANT(PS3_PAD_CIRCLE);
	BIND_CONSTANT(PS3_PAD_TRIANGLE);
	BIND_CONSTANT(PS3_PAD_SQUARE);
	BIND_CONSTANT(PS3_PAD_L1);
	BIND_CONSTANT(PS3_PAD_R1);
	BIND_CONSTANT(PS3_PAD_L2);
	BIND_CONSTANT(PS3_PAD_R2);
	BIND_CONSTANT(PS3_PAD_START);
	BIND_CONSTANT(PS3_PAD_SELECT);
	BIND_CONSTANT(PS3_PAD_L3);
	BIND_CONSTANT(PS3_PAD_R3);
}

void PS3Pad::set_port(int p_port) {
	port = p_port;
}

int PS3Pad::get_port() const {
	return port;
}

bool PS3Pad::is_connected() const {
#ifdef PS3_ENABLED
	JoypadPS3State *state = JoypadPS3State::get_singleton();
	state->initialize(port);
	return state->update();
#else
	return false;
#endif
}

Vector2 PS3Pad::get_left_stick() const {
#ifdef PS3_ENABLED
	if (!is_connected()) {
		return Vector2();
	}
	const padData &data = JoypadPS3State::get_singleton()->get_data();
	return Vector2((data.ANA_L_H - 128) / 128.0f, (data.ANA_L_V - 128) / 128.0f);
#else
	return Vector2();
#endif
}

Vector2 PS3Pad::get_right_stick() const {
#ifdef PS3_ENABLED
	if (!is_connected()) {
		return Vector2();
	}
	const padData &data = JoypadPS3State::get_singleton()->get_data();
	return Vector2((data.ANA_R_H - 128) / 128.0f, (data.ANA_R_V - 128) / 128.0f);
#else
	return Vector2();
#endif
}

float PS3Pad::get_pressure(int p_button) const {
#ifdef PS3_ENABLED
	if (!is_connected()) {
		return 0.0f;
	}
	const padData &data = JoypadPS3State::get_singleton()->get_data();

	uint16_t pressure = 0;
	switch (p_button) {
		case PS3_PAD_CROSS:
			pressure = data.PRE_CROSS;
			break;
		case PS3_PAD_CIRCLE:
			pressure = data.PRE_CIRCLE;
			break;
		case PS3_PAD_TRIANGLE:
			pressure = data.PRE_TRIANGLE;
			break;
		case PS3_PAD_SQUARE:
			pressure = data.PRE_SQUARE;
			break;
		case PS3_PAD_L1:
			pressure = data.PRE_L1;
			break;
		case PS3_PAD_R1:
			pressure = data.PRE_R1;
			break;
		case PS3_PAD_L2:
			pressure = data.PRE_L2;
			break;
		case PS3_PAD_R2:
			pressure = data.PRE_R2;
			break;
		case PS3_PAD_DPAD_UP:
			pressure = data.PRE_UP;
			break;
		case PS3_PAD_DPAD_DOWN:
			pressure = data.PRE_DOWN;
			break;
		case PS3_PAD_DPAD_LEFT:
			pressure = data.PRE_LEFT;
			break;
		case PS3_PAD_DPAD_RIGHT:
			pressure = data.PRE_RIGHT;
			break;
		default:
			return 0.0f;
	}

	if (pressure > 0) {
		return pressure / 255.0f;
	}

	switch (p_button) {
		case PS3_PAD_CROSS:
			return data.BTN_CROSS ? 1.0f : 0.0f;
		case PS3_PAD_CIRCLE:
			return data.BTN_CIRCLE ? 1.0f : 0.0f;
		case PS3_PAD_TRIANGLE:
			return data.BTN_TRIANGLE ? 1.0f : 0.0f;
		case PS3_PAD_SQUARE:
			return data.BTN_SQUARE ? 1.0f : 0.0f;
		case PS3_PAD_L1:
			return data.BTN_L1 ? 1.0f : 0.0f;
		case PS3_PAD_R1:
			return data.BTN_R1 ? 1.0f : 0.0f;
		case PS3_PAD_L2:
			return data.BTN_L2 ? 1.0f : 0.0f;
		case PS3_PAD_R2:
			return data.BTN_R2 ? 1.0f : 0.0f;
		case PS3_PAD_DPAD_UP:
			return data.BTN_UP ? 1.0f : 0.0f;
		case PS3_PAD_DPAD_DOWN:
			return data.BTN_DOWN ? 1.0f : 0.0f;
		case PS3_PAD_DPAD_LEFT:
			return data.BTN_LEFT ? 1.0f : 0.0f;
		case PS3_PAD_DPAD_RIGHT:
			return data.BTN_RIGHT ? 1.0f : 0.0f;
		case PS3_PAD_START:
			return data.BTN_START ? 1.0f : 0.0f;
		case PS3_PAD_SELECT:
			return data.BTN_SELECT ? 1.0f : 0.0f;
		case PS3_PAD_L3:
			return data.BTN_L3 ? 1.0f : 0.0f;
		case PS3_PAD_R3:
			return data.BTN_R3 ? 1.0f : 0.0f;
		default:
			return 0.0f;
	}
#else
	return 0.0f;
#endif
}

void PS3Pad::set_rumble(float p_weak, float p_strong) {
#ifdef PS3_ENABLED
	JoypadPS3State::get_singleton()->set_rumble(p_weak, p_strong);
#endif
}

PS3Pad::PS3Pad() {
}
