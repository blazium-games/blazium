/**************************************************************************/
/*  crowd_control_game_pack.h                                             */
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
#include "core/variant/typed_array.h"
#include "crowd_control_effect.h"
#include "crowd_control_game_pack_meta.h"

class CrowdControlGamePack : public Resource {
	GDCLASS(CrowdControlGamePack, Resource);

private:
	Ref<CrowdControlGamePackMeta> pack_meta;
	TypedArray<CrowdControlEffect> effects;

protected:
	static void _bind_methods();

public:
	void set_pack_meta(const Ref<CrowdControlGamePackMeta> &p_meta);
	Ref<CrowdControlGamePackMeta> get_pack_meta() const;

	void set_effects(const TypedArray<CrowdControlEffect> &p_effects);
	TypedArray<CrowdControlEffect> get_effects() const;

	void add_effect(const Ref<CrowdControlEffect> &p_effect);
	void remove_effect(const String &p_effect_id);
	Ref<CrowdControlEffect> get_effect(const String &p_effect_id) const;

	Error export_to_json(const String &p_path);
	Dictionary to_json() const;

	CrowdControlGamePack();
	~CrowdControlGamePack();
};
