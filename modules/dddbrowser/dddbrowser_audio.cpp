/**************************************************************************/
/*  dddbrowser_audio.cpp                                                  */
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

#include "dddbrowser_audio.h"

void DDDBrowserAudio::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &DDDBrowserAudio::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &DDDBrowserAudio::get_source_path);
	ClassDB::bind_method(D_METHOD("set_format", "format"), &DDDBrowserAudio::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &DDDBrowserAudio::get_format);
	ClassDB::bind_method(D_METHOD("set_ambient", "ambient"), &DDDBrowserAudio::set_ambient);
	ClassDB::bind_method(D_METHOD("get_ambient"), &DDDBrowserAudio::get_ambient);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &DDDBrowserAudio::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &DDDBrowserAudio::get_loop);
	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &DDDBrowserAudio::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &DDDBrowserAudio::get_volume);
	ClassDB::bind_method(D_METHOD("set_auto_play", "auto_play"), &DDDBrowserAudio::set_auto_play);
	ClassDB::bind_method(D_METHOD("get_auto_play"), &DDDBrowserAudio::get_auto_play);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.wav,*.ogg"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_ENUM, "WAV,Ogg"), "set_format", "get_format");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ambient"), "set_ambient", "get_ambient");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");

	BIND_ENUM_CONSTANT(FORMAT_WAV);
	BIND_ENUM_CONSTANT(FORMAT_OGG);
}

void DDDBrowserAudio::set_source_path(const String &p_path) {
	source_path = p_path;
}
String DDDBrowserAudio::get_source_path() const {
	return source_path;
}
void DDDBrowserAudio::set_format(Format p_format) {
	format = p_format;
}
DDDBrowserAudio::Format DDDBrowserAudio::get_format() const {
	return format;
}
void DDDBrowserAudio::set_ambient(bool p_ambient) {
	ambient = p_ambient;
}
bool DDDBrowserAudio::get_ambient() const {
	return ambient;
}
void DDDBrowserAudio::set_loop(bool p_loop) {
	loop = p_loop;
}
bool DDDBrowserAudio::get_loop() const {
	return loop;
}
void DDDBrowserAudio::set_volume(float p_volume) {
	volume = p_volume;
}
float DDDBrowserAudio::get_volume() const {
	return volume;
}
void DDDBrowserAudio::set_auto_play(bool p_auto) {
	auto_play = p_auto;
}
bool DDDBrowserAudio::get_auto_play() const {
	return auto_play;
}
