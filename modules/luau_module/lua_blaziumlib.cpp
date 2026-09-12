/**************************************************************************/
/*  lua_blaziumlib.cpp                                                    */
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

#include "lua_blaziumlib.h"

#include "bindings/array.h"
#include "bindings/luau_blazium_types.h"
#include "bindings/object.h"
#include "bindings/variant.h"
#include "helpers.h"

#include "core/error/error_macros.h"
#include "core/string/print_string.h"
#include "core/variant/variant_utility.h"
#include <lualib.h>

using namespace luau_module;

static int vector2_constructor(lua_State *L) {
	double x = luaL_checknumber(L, 1);
	double y = luaL_checknumber(L, 2);
	lua_pop(L, 2);

	push_typed_vector2(L, Vector2(x, y));
	return 1;
}

static int vector2i_constructor(lua_State *L) {
	int isnum = 0;
	int x = lua_tointegerx(L, 1, &isnum);
	if (isnum) {
		int y = luaL_checkinteger(L, 2);
		lua_pop(L, 2);

		push_variant(L, Vector2i(x, y));
	} else {
		Vector2 vec = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Vector2i(vec));
	}

	return 1;
}

static int rect2_constructor(lua_State *L) {
	int isnum = 0;
	double x = lua_tonumberx(L, 1, &isnum);
	if (isnum) {
		double y = luaL_checknumber(L, 2);
		double width = luaL_checknumber(L, 3);
		double height = luaL_checknumber(L, 4);
		lua_pop(L, 4);

		push_variant(L, Rect2(x, y, width, height));
	} else {
		Vector2 position = to_variant(L, 1);
		Vector2 size = to_variant(L, 2);
		lua_pop(L, 2);

		push_variant(L, Rect2(position, size));
	}

	return 1;
}

static int rect2i_constructor(lua_State *L) {
	int isnum = 0;
	int x = lua_tointegerx(L, 1, &isnum);
	if (isnum) {
		int y = luaL_checkinteger(L, 2);
		int width = luaL_checkinteger(L, 3);
		int height = luaL_checkinteger(L, 4);
		lua_pop(L, 4);

		push_variant(L, Rect2i(x, y, width, height));
	} else {
		Vector2i position = to_variant(L, 1);
		Vector2i size = to_variant(L, 2);
		lua_pop(L, 2);

		push_variant(L, Rect2i(position, size));
	}

	return 1;
}

static int vector3_constructor(lua_State *L) {
	double x = luaL_checknumber(L, 1);
	double y = luaL_checknumber(L, 2);
	double z = luaL_checknumber(L, 3);
	lua_pop(L, 3);

	push_typed_vector3(L, Vector3(x, y, z));
	return 1;
}

static int vector3i_constructor(lua_State *L) {
	int isnum = 0;
	int x = lua_tointegerx(L, 1, &isnum);
	if (isnum) {
		int y = luaL_checkinteger(L, 2);
		int z = luaL_checkinteger(L, 3);
		lua_pop(L, 3);

		push_variant(L, Vector3i(x, y, z));
	} else {
		Vector3 vec = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Vector3i(vec));
	}

	return 1;
}

static int transform2d_constructor(lua_State *L) {
	int isnum = 0;
	double rotation = lua_tonumberx(L, 1, &isnum);
	if (!isnum) {
		Vector2 x_axis = to_variant(L, 1);
		Vector2 y_axis = to_variant(L, 2);
		Vector2 origin = to_variant(L, 3);
		lua_pop(L, 3);

		push_variant(L, Transform2D(x_axis, y_axis, origin));
	} else if (lua_gettop(L) == 4) {
		Vector2 scale = to_variant(L, 2);
		double skew = luaL_checknumber(L, 3);
		Vector2 position = to_variant(L, 4);
		lua_pop(L, 4);

		push_variant(L, Transform2D(rotation, scale, skew, position));
	} else {
		Vector2 position = to_variant(L, 2);
		lua_pop(L, 2);

		push_variant(L, Transform2D(rotation, position));
	}

	return 1;
}

static int vector4_constructor(lua_State *L) {
	double x = luaL_checknumber(L, 1);
	double y = luaL_checknumber(L, 2);
	double z = luaL_checknumber(L, 3);
	double w = luaL_checknumber(L, 4);
	lua_pop(L, 4);

	push_variant(L, Vector4(x, y, z, w));
	return 1;
}

static int vector4i_constructor(lua_State *L) {
	int isnum = 0;
	int x = lua_tointegerx(L, 1, &isnum);
	if (isnum) {
		int y = luaL_checkinteger(L, 2);
		int z = luaL_checkinteger(L, 3);
		int w = luaL_checkinteger(L, 4);
		lua_pop(L, 4);

		push_variant(L, Vector4i(x, y, z, w));
	} else {
		Vector4 vec = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Vector4i(vec));
	}

	return 1;
}

static int plane_constructor(lua_State *L) {
	int isnum = 0;
	double a = lua_tonumberx(L, 1, &isnum);
	if (isnum) {
		double b = luaL_checknumber(L, 2);
		double c = luaL_checknumber(L, 3);
		double d = luaL_checknumber(L, 4);
		lua_pop(L, 4);

		push_variant(L, Plane(a, b, c, d));
	} else if (lua_gettop(L) == 2) {
		Vector3 normal = to_variant(L, 1);
		double d = lua_tonumberx(L, 2, &isnum);
		if (isnum) {
			lua_pop(L, 2);

			push_variant(L, Plane(normal, d));
		} else {
			Vector3 point = to_variant(L, 2);
			lua_pop(L, 2);

			push_variant(L, Plane(normal, point));
		}
	} else if (lua_gettop(L) == 3) {
		Vector3 point1 = to_variant(L, 1);
		Vector3 point2 = to_variant(L, 2);
		Vector3 point3 = to_variant(L, 3);
		lua_pop(L, 3);

		push_variant(L, Plane(point1, point2, point3));
	} else {
		Vector3 normal = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Plane(normal));
	}

	return 1;
}

static int quaternion_constructor(lua_State *L) {
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		Basis basis = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Quaternion(basis));
	} else if (nargs == 4) {
		double x = luaL_checknumber(L, 1);
		double y = luaL_checknumber(L, 2);
		double z = luaL_checknumber(L, 3);
		double w = luaL_checknumber(L, 4);
		lua_pop(L, 4);

		push_variant(L, Quaternion(x, y, z, w));
	} else {
		int isnum = 0;
		double angle = lua_tonumberx(L, 2, &isnum);
		if (isnum) {
			Vector3 axis = to_variant(L, 1);
			lua_pop(L, 2);

			push_variant(L, Quaternion(axis, angle));
		} else {
			Vector3 arc_from = to_variant(L, 1);
			Vector3 arc_to = to_variant(L, 2);
			lua_pop(L, 2);

			push_variant(L, Quaternion(arc_from, arc_to));
		}
	}

	return 1;
}

static int aabb_constructor(lua_State *L) {
	Vector3 position = to_variant(L, 1);
	Vector3 size = to_variant(L, 2);
	lua_pop(L, 2);

	push_variant(L, AABB(position, size));
	return 1;
}

static int basis_constructor(lua_State *L) {
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		Quaternion q = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Basis(q));
	} else if (nargs == 3) {
		Vector3 x = to_variant(L, 1);
		Vector3 y = to_variant(L, 2);
		Vector3 z = to_variant(L, 3);
		lua_pop(L, 3);

		push_variant(L, Basis(x, y, z));
	} else {
		Vector3 axis = to_variant(L, 1);
		double angle = luaL_checknumber(L, 2);
		lua_pop(L, 2);

		push_variant(L, Basis(axis, angle));
	}

	return 1;
}

static int transform3d_constructor(lua_State *L) {
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		Projection p = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Transform3D(p));
	} else if (nargs == 2) {
		Basis basis = to_variant(L, 1);
		Vector3 origin = to_variant(L, 2);
		lua_pop(L, 2);

		push_variant(L, Transform3D(basis, origin));
	} else {
		Vector3 x_axis = to_variant(L, 1);
		Vector3 y_axis = to_variant(L, 2);
		Vector3 z_axis = to_variant(L, 3);
		Vector3 origin = to_variant(L, 4);
		lua_pop(L, 4);

		push_variant(L, Transform3D(x_axis, y_axis, z_axis, origin));
	}

	return 1;
}

static int projection_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Transform3D transform = to_variant(L, 1);
		lua_pop(L, 1);

		push_variant(L, Projection(transform));
	} else {
		Vector4 x_axis = to_variant(L, 1);
		Vector4 y_axis = to_variant(L, 2);
		Vector4 z_axis = to_variant(L, 3);
		Vector4 w_axis = to_variant(L, 4);
		lua_pop(L, 4);

		push_variant(L, Projection(x_axis, y_axis, z_axis, w_axis));
	}

	return 1;
}

static int color_constructor(lua_State *L) {
	int nargs = lua_gettop(L);
	if (nargs == 1) {
		size_t len = 0;
		const char *cstr = luaL_checklstring(L, 1, &len);
		String code = String::utf8(cstr, len);
		lua_pop(L, 1);

		push_typed_color(L, Color(code));
	} else if (nargs == 2) {
		double alpha = luaL_checknumber(L, 2);

		if (lua_isstring(L, 1)) {
			size_t len = 0;
			const char *cstr = luaL_checklstring(L, 1, &len);
			String code = String::utf8(cstr, len);
			lua_pop(L, 2);

			push_typed_color(L, Color(code, alpha));
		} else {
			Color base = to_variant(L, 1);
			lua_pop(L, 2);

			push_typed_color(L, Color(base, alpha));
		}
	} else if (nargs == 3) {
		double r = luaL_checknumber(L, 1);
		double g = luaL_checknumber(L, 2);
		double b = luaL_checknumber(L, 3);
		lua_pop(L, 3);

		push_typed_color(L, Color(r, g, b));
	} else {
		double r = luaL_checknumber(L, 1);
		double g = luaL_checknumber(L, 2);
		double b = luaL_checknumber(L, 3);
		double a = luaL_checknumber(L, 4);
		lua_pop(L, 4);

		push_typed_color(L, Color(r, g, b, a));
	}

	return 1;
}

static int rid_constructor(lua_State *L) {
	push_variant(L, RID());
	return 1;
}

static int signal_constructor(lua_State *L) {
	Object *obj = to_object(L, 1);
	StringName signal = to_string_name(L, 2);
	lua_pop(L, 2);

	push_variant(L, Signal(obj, signal));
	return 1;
}

template <typename PackedT>
static void push_packed_from_array(lua_State *L, const Array &p_array) {
	PackedT packed;
	for (int i = 0; i < p_array.size(); i++) {
		packed.push_back(p_array[i]);
	}
	push_variant(L, packed);
}

static int packed_byte_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedByteArray>(L, array);
	} else {
		lua_newbuffer(L, 0);
	}

	return 1;
}

static int packed_int32_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedInt32Array>(L, array);
	} else {
		push_variant(L, PackedInt32Array());
	}

	return 1;
}

static int packed_int64_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedInt64Array>(L, array);
	} else {
		push_variant(L, PackedInt64Array());
	}

	return 1;
}

static int packed_float32_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedFloat32Array>(L, array);
	} else {
		push_variant(L, PackedFloat32Array());
	}

	return 1;
}

static int packed_float64_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedFloat64Array>(L, array);
	} else {
		push_variant(L, PackedFloat64Array());
	}

	return 1;
}

static int packed_string_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedStringArray>(L, array);
	} else {
		push_variant(L, PackedStringArray());
	}

	return 1;
}

static int packed_vector2_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedVector2Array>(L, array);
	} else {
		push_variant(L, PackedVector2Array());
	}

	return 1;
}

static int packed_vector3_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedVector3Array>(L, array);
	} else {
		push_variant(L, PackedVector3Array());
	}

	return 1;
}

static int packed_color_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedColorArray>(L, array);
	} else {
		push_variant(L, PackedColorArray());
	}

	return 1;
}

static int packed_vector4_array_constructor(lua_State *L) {
	if (lua_gettop(L) == 1) {
		Array array = to_array(L, 1);
		lua_pop(L, 1);

		push_packed_from_array<PackedVector4Array>(L, array);
	} else {
		push_variant(L, PackedVector4Array());
	}

	return 1;
}

static int blaziumlib_print(lua_State *L) {
	ERR_FAIL_COND_V_MSG(!lua_checkstack(L, 1), 0, "lua_blaziumlib.print(): Stack overflow. Cannot grow stack.");

	int nargs = lua_gettop(L);

	PackedStringArray s;
	s.resize(nargs);

	for (int i = 1; i <= nargs; i++) {
		size_t len;
		const char *str = luaL_tolstring(L, i, &len);
		if (str) {
			s.set(i - 1, String::utf8(str, len));
		}

		lua_pop(L, 1);
	}

	lua_pop(L, nargs);

	String joined = String("\t").join(s);
	print_line(joined);

	return 0;
}

int luaopen_blazium(lua_State *L) {
	luaL_checkstack(L, 3, "luaopen_blazium(): Stack overflow. Cannot grow stack.");

	install_blazium_type_metatables(L);

	luaL_Reg globals[] = {

		{ "Vector2", vector2_constructor },
		{ "Vector2i", vector2i_constructor },
		{ "Rect2", rect2_constructor },
		{ "Rect2i", rect2i_constructor },
		{ "Vector3", vector3_constructor },
		{ "Vector3i", vector3i_constructor },
		{ "Transform2D", transform2d_constructor },
		{ "Vector4", vector4_constructor },
		{ "Vector4i", vector4i_constructor },
		{ "Plane", plane_constructor },
		{ "Quaternion", quaternion_constructor },
		{ "AABB", aabb_constructor },
		{ "Basis", basis_constructor },
		{ "Transform3D", transform3d_constructor },
		{ "Projection", projection_constructor },
		{ "Color", color_constructor },
		{ "RID", rid_constructor },
		{ "Signal", signal_constructor },
		{ "PackedByteArray", packed_byte_array_constructor },
		{ "PackedInt32Array", packed_int32_array_constructor },
		{ "PackedInt64Array", packed_int64_array_constructor },
		{ "PackedFloat32Array", packed_float32_array_constructor },
		{ "PackedFloat64Array", packed_float64_array_constructor },
		{ "PackedStringArray", packed_string_array_constructor },
		{ "PackedVector2Array", packed_vector2_array_constructor },
		{ "PackedVector3Array", packed_vector3_array_constructor },
		{ "PackedColorArray", packed_color_array_constructor },
		{ "PackedVector4Array", packed_vector4_array_constructor },

		{ "print", blaziumlib_print },

		{ NULL, NULL }
	};

	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, NULL, globals);
	lua_pop(L, 1);

	luaL_Reg namespaced[] = {
		{ NULL, NULL }
	};
	luaL_register(L, "blazium", namespaced);
	return 1;
}
