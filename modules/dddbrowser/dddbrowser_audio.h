/**************************************************************************/
/*  dddbrowser_audio.h                                                    */
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

#include "scene/3d/node_3d.h"

class DDDBrowserAudio : public Node3D {
	GDCLASS(DDDBrowserAudio, Node3D);

public:
	enum Format {
		FORMAT_WAV,
		FORMAT_OGG,
	};

private:
	String source_path;
	Format format = FORMAT_WAV;
	bool ambient = false;
	bool loop = false;
	float volume = 1.0f;
	bool auto_play = false;

protected:
	static void _bind_methods();

public:
	void set_source_path(const String &p_path);
	String get_source_path() const;
	void set_format(Format p_format);
	Format get_format() const;
	void set_ambient(bool p_ambient);
	bool get_ambient() const;
	void set_loop(bool p_loop);
	bool get_loop() const;
	void set_volume(float p_volume);
	float get_volume() const;
	void set_auto_play(bool p_auto);
	bool get_auto_play() const;
};

VARIANT_ENUM_CAST(DDDBrowserAudio::Format);
