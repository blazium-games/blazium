/**************************************************************************/
/*  dddbrowser_portal.cpp                                                 */
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

#include "core/object/class_db.h"
#include "dddbrowser_portal.h"

void DDDBrowserPortal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_destination_url", "url"), &DDDBrowserPortal::set_destination_url);
	ClassDB::bind_method(D_METHOD("get_destination_url"), &DDDBrowserPortal::get_destination_url);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &DDDBrowserPortal::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &DDDBrowserPortal::get_radius);
	ClassDB::bind_method(D_METHOD("set_trigger_mode", "mode"), &DDDBrowserPortal::set_trigger_mode);
	ClassDB::bind_method(D_METHOD("get_trigger_mode"), &DDDBrowserPortal::get_trigger_mode);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "destination_url"), "set_destination_url", "get_destination_url");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0,100,0.01"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "trigger_mode", PROPERTY_HINT_ENUM, "Auto,Manual,Script"), "set_trigger_mode", "get_trigger_mode");

	BIND_ENUM_CONSTANT(TRIGGER_AUTO);
	BIND_ENUM_CONSTANT(TRIGGER_MANUAL);
	BIND_ENUM_CONSTANT(TRIGGER_SCRIPT);
}

void DDDBrowserPortal::set_destination_url(const String &p_url) {
	destination_url = p_url;
}
String DDDBrowserPortal::get_destination_url() const {
	return destination_url;
}
void DDDBrowserPortal::set_radius(float p_radius) {
	radius = p_radius;
}
float DDDBrowserPortal::get_radius() const {
	return radius;
}
void DDDBrowserPortal::set_trigger_mode(TriggerMode p_mode) {
	trigger_mode = p_mode;
}
DDDBrowserPortal::TriggerMode DDDBrowserPortal::get_trigger_mode() const {
	return trigger_mode;
}
