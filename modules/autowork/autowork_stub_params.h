/**************************************************************************/
/*  autowork_stub_params.h                                                */
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

#include "autowork_stubber.h"
#include "core/object/ref_counted.h"

class AutoworkStubParams : public RefCounted {
	GDCLASS(AutoworkStubParams, RefCounted);

protected:
	static void _bind_methods();

	Ref<AutoworkStubber> stubber;
	Variant target_object;
	StringName target_method;
	Variant target_args; // Array of args, or null for wildcard

public:
	AutoworkStubParams();
	~AutoworkStubParams();

	void init(Ref<AutoworkStubber> p_stubber, const Variant &p_object, const StringName &p_method);

	Ref<AutoworkStubParams> to_return(const Variant &p_value);
	Ref<AutoworkStubParams> to_do_nothing();
	Ref<AutoworkStubParams> to_call_super();
	Ref<AutoworkStubParams> when_passed(const Variant &p_args);
};
