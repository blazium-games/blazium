/**************************************************************************/
/*  crowd_control_effect.h                                                */
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

#include "core/io/resource.h"

class CrowdControlEffect : public Resource {
	GDCLASS(CrowdControlEffect, Resource);

private:
	String effect_id;
	Variant effect_name; // Can be String or Dictionary
	int price = 100;
	String description;
	int duration = 0;
	int quantity_min = 1;
	int quantity_max = 1;
	String note;
	bool inactive = false;
	PackedStringArray category;
	PackedStringArray group;
	Dictionary parameters;

protected:
	static void _bind_methods();

public:
	void set_effect_id(const String &p_id);
	String get_effect_id() const;

	void set_effect_name(const Variant &p_name);
	Variant get_effect_name() const;

	void set_price(int p_price);
	int get_price() const;

	void set_description(const String &p_description);
	String get_description() const;

	void set_duration(int p_duration);
	int get_duration() const;

	void set_quantity_min(int p_min);
	int get_quantity_min() const;

	void set_quantity_max(int p_max);
	int get_quantity_max() const;

	void set_note(const String &p_note);
	String get_note() const;

	void set_inactive(bool p_inactive);
	bool is_inactive() const;

	void set_category(const PackedStringArray &p_category);
	PackedStringArray get_category() const;

	void set_group(const PackedStringArray &p_group);
	PackedStringArray get_group() const;

	void set_parameters(const Dictionary &p_parameters);
	Dictionary get_parameters() const;

	Dictionary to_json() const;

	CrowdControlEffect();
	~CrowdControlEffect();
};
