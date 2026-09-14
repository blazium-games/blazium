/**************************************************************************/
/*  blazium_fgd_point_class_display_descriptor.cpp                        */
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

#include "blazium_fgd_point_class_display_descriptor.h"

#include "core/object/class_db.h"

void BlaziumFGDPointClassDisplayDescriptor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_display_asset_path", "display_asset_path"), &BlaziumFGDPointClassDisplayDescriptor::set_display_asset_path);
	ClassDB::bind_method(D_METHOD("get_display_asset_path"), &BlaziumFGDPointClassDisplayDescriptor::get_display_asset_path);
	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &BlaziumFGDPointClassDisplayDescriptor::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &BlaziumFGDPointClassDisplayDescriptor::get_scale);
	ClassDB::bind_method(D_METHOD("set_skin", "skin"), &BlaziumFGDPointClassDisplayDescriptor::set_skin);
	ClassDB::bind_method(D_METHOD("get_skin"), &BlaziumFGDPointClassDisplayDescriptor::get_skin);
	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &BlaziumFGDPointClassDisplayDescriptor::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &BlaziumFGDPointClassDisplayDescriptor::get_frame);
	ClassDB::bind_method(D_METHOD("set_conditional", "conditional"), &BlaziumFGDPointClassDisplayDescriptor::set_conditional);
	ClassDB::bind_method(D_METHOD("get_conditional"), &BlaziumFGDPointClassDisplayDescriptor::get_conditional);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "display_asset_path"), "set_display_asset_path", "get_display_asset_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scale"), "set_scale", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "skin"), "set_skin", "get_skin");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "conditional"), "set_conditional", "get_conditional");
}
