/**************************************************************************/
/*  joypad_ps3_state.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#ifdef PS3_ENABLED
#include <io/pad.h>
#endif

class JoypadPS3State {
	static JoypadPS3State *singleton;

#ifdef PS3_ENABLED
	padData pad_data;
	padActParam rumble;
#endif
	bool connected = false;
	bool initialized = false;
	int port = 0;

public:
	static JoypadPS3State *get_singleton();

	void initialize(int p_port = 0);
	bool update();
	bool is_connected() const { return connected; }
	int get_port() const { return port; }

#ifdef PS3_ENABLED
	const padData &get_data() const { return pad_data; }
#endif

	void set_rumble(float p_weak, float p_strong);
};
