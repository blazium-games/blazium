/**************************************************************************/
/*  joypad_ps3_state.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "joypad_ps3_state.h"

#include "core/os/memory.h"
#include "core/math/math_funcs.h"

#ifdef PS3_ENABLED
#include <io/pad.h>
#endif

JoypadPS3State *JoypadPS3State::singleton = nullptr;

JoypadPS3State *JoypadPS3State::get_singleton() {
	if (!singleton) {
		singleton = memnew(JoypadPS3State);
	}
	return singleton;
}

void JoypadPS3State::initialize(int p_port) {
#ifdef PS3_ENABLED
	if (!initialized) {
		ioPadInit(1);
		initialized = true;
	}
	port = p_port;
	ioPadSetPortSetting(port, PAD_SETTINGS_PRESS_ON);
#endif
}

bool JoypadPS3State::update() {
#ifdef PS3_ENABLED
	const s32 result = ioPadGetData(port, &pad_data);
	connected = result != 0;
	return connected;
#else
	connected = false;
	return false;
#endif
}

void JoypadPS3State::set_rumble(float p_weak, float p_strong) {
#ifdef PS3_ENABLED
	rumble.large_motor = (uint8_t)CLAMP(p_strong * 255.0f, 0.0f, 255.0f);
	rumble.small_motor = p_weak > 0.0f ? 1 : 0;
	ioPadSetActDirect(port, &rumble);
#endif
}
