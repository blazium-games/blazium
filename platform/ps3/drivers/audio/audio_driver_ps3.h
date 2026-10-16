/**************************************************************************/
/*  audio_driver_ps3.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio_server.h"

class AudioDriverPS3 : public AudioDriver {
	Mutex mutex;
	int mix_rate = 44100;
	uint32_t buffer_frames = 1024;
	Vector<int32_t> mix_buffer;
	SafeFlag active;

public:
	virtual const char *get_name() const override { return "PS3"; }

	virtual Error init() override;
	virtual void start() override;
	virtual int get_mix_rate() const override { return mix_rate; }
	virtual SpeakerMode get_speaker_mode() const override { return SPEAKER_MODE_STEREO; }

	virtual void lock() override { mutex.lock(); }
	virtual void unlock() override { mutex.unlock(); }
	virtual void finish() override;

	void mix_audio();
};
