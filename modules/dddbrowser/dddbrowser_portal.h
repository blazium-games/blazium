/**************************************************************************/
/*  dddbrowser_portal.h                                                   */
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

class DDDBrowserPortal : public Node3D {
	GDCLASS(DDDBrowserPortal, Node3D);

public:
	enum TriggerMode {
		TRIGGER_AUTO,
		TRIGGER_MANUAL,
		TRIGGER_SCRIPT,
	};

private:
	String destination_url;
	float radius = 1.0f;
	TriggerMode trigger_mode = TRIGGER_MANUAL;

protected:
	static void _bind_methods();

public:
	void set_destination_url(const String &p_url);
	String get_destination_url() const;
	void set_radius(float p_radius);
	float get_radius() const;
	void set_trigger_mode(TriggerMode p_mode);
	TriggerMode get_trigger_mode() const;
};

VARIANT_ENUM_CAST(DDDBrowserPortal::TriggerMode);
