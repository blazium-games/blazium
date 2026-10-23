/**************************************************************************/
/*  audio_driver_ps3.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "audio_driver_ps3.h"

#include "core/string/print_string.h"

Error AudioDriverPS3::init() {
	mix_rate = _get_configured_mix_rate();
	buffer_frames = 1024;
	mix_buffer.resize(buffer_frames * 2);
	active.clear();
	print_line("PS3: audio driver initialized (main-thread mixing).");
	return OK;
}

void AudioDriverPS3::start() {
	active.set();
}

void AudioDriverPS3::finish() {
	active.clear();
}

void AudioDriverPS3::mix_audio() {
	if (!active.is_set()) {
		return;
	}

	lock();
	audio_server_process(buffer_frames, mix_buffer.ptrw());
	unlock();
}
