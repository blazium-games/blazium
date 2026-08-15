/**************************************************************************/
/*  justamcp_runtime_query_serialize.cpp                                  */
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

#include "justamcp_runtime.h"

#include "core/object/script_language.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

Dictionary JustAMCPRuntime::_serialize_node_tree(Node *p_node, int p_depth, int p_max_depth, bool p_include_properties) {
	Dictionary result = _serialize_node(p_node, p_include_properties);
	if (p_depth < p_max_depth) {
		Array children;
		for (int i = 0; i < p_node->get_child_count(); i++) {
			children.push_back(_serialize_node_tree(p_node->get_child(i), p_depth + 1, p_max_depth, p_include_properties));
		}
		result["children"] = children;
	}
	return result;
}

Dictionary JustAMCPRuntime::_serialize_node(Node *p_node, bool p_include_properties) {
	Dictionary result;
	result["name"] = p_node->get_name();
	result["type"] = p_node->get_class();
	result["path"] = String(p_node->get_path());

	if (p_include_properties) {
		Dictionary props;
		List<PropertyInfo> list;
		p_node->get_property_list(&list);
		for (const PropertyInfo &E : list) {
			if (E.usage & PROPERTY_USAGE_STORAGE) {
				if (!E.name.begins_with("_")) {
					props[E.name] = _serialize_value(p_node->get(E.name));
				}
			}
		}
		result["properties"] = props;
	}
	return result;
}

Variant JustAMCPRuntime::_serialize_value(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return p_value;
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
			return p_value;
		case Variant::VECTOR2: {
			Vector2 v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			return d;
		}
		case Variant::VECTOR2I: {
			Vector2i v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			return d;
		}
		case Variant::RECT2: {
			Rect2 r = p_value;
			Dictionary d;
			d["x"] = r.position.x;
			d["y"] = r.position.y;
			d["w"] = r.size.x;
			d["h"] = r.size.y;
			return d;
		}
		case Variant::RECT2I: {
			Rect2i r = p_value;
			Dictionary d;
			d["x"] = r.position.x;
			d["y"] = r.position.y;
			d["w"] = r.size.x;
			d["h"] = r.size.y;
			return d;
		}
		case Variant::VECTOR3: {
			Vector3 v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			return d;
		}
		case Variant::VECTOR3I: {
			Vector3i v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			return d;
		}
		case Variant::TRANSFORM2D: {
			Transform2D t = p_value;
			Array a;
			a.push_back(_serialize_value(t.get_origin()));
			a.push_back(_serialize_value(t[0]));
			a.push_back(_serialize_value(t[1]));
			return a;
		}
		case Variant::VECTOR4: {
			Vector4 v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			d["w"] = v.w;
			return d;
		}
		case Variant::VECTOR4I: {
			Vector4i v = p_value;
			Dictionary d;
			d["x"] = v.x;
			d["y"] = v.y;
			d["z"] = v.z;
			d["w"] = v.w;
			return d;
		}
		case Variant::PLANE: {
			Plane p = p_value;
			Dictionary d;
			d["normal"] = _serialize_value(p.normal);
			d["d"] = p.d;
			return d;
		}
		case Variant::QUATERNION: {
			Quaternion q = p_value;
			Dictionary d;
			d["x"] = q.x;
			d["y"] = q.y;
			d["z"] = q.z;
			d["w"] = q.w;
			return d;
		}
		case Variant::AABB: {
			AABB a = p_value;
			Dictionary d;
			d["position"] = _serialize_value(a.position);
			d["size"] = _serialize_value(a.size);
			return d;
		}
		case Variant::BASIS: {
			Basis b = p_value;
			Array a;
			a.push_back(_serialize_value(b.get_column(0)));
			a.push_back(_serialize_value(b.get_column(1)));
			a.push_back(_serialize_value(b.get_column(2)));
			return a;
		}
		case Variant::TRANSFORM3D: {
			Transform3D t = p_value;
			Dictionary d;
			d["basis"] = _serialize_value(t.basis);
			d["origin"] = _serialize_value(t.origin);
			return d;
		}
		case Variant::PROJECTION: {
			Projection p = p_value;
			Array a;
			a.push_back(_serialize_value(p.columns[0]));
			a.push_back(_serialize_value(p.columns[1]));
			a.push_back(_serialize_value(p.columns[2]));
			a.push_back(_serialize_value(p.columns[3]));
			return a;
		}
		case Variant::COLOR: {
			Color c = p_value;
			return c.to_html(true);
		}
		case Variant::NODE_PATH:
			return String(p_value);
		case Variant::RID: {
			RID rid = p_value;
			return (int64_t)rid.get_id();
		}
		case Variant::OBJECT: {
			Object *obj = p_value;
			if (!obj) {
				return Variant();
			}
			Node *n = Object::cast_to<Node>(obj);
			if (n) {
				return String(n->get_path());
			}
			return obj->get_class();
		}
		case Variant::DICTIONARY: {
			Dictionary d = p_value;
			Dictionary res;
			for (const Variant &k : d.get_key_list()) {
				res[String(k)] = _serialize_value(d[k]);
			}
			return res;
		}
		case Variant::ARRAY: {
			Array a = p_value;
			Array res;
			for (int i = 0; i < a.size(); i++) {
				res.push_back(_serialize_value(a[i]));
			}
			return res;
		}
		default:
			return String(p_value);
	}
}

Variant JustAMCPRuntime::_deserialize_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_value;
		if (d.has("x") && d.has("y") && d.size() == 2) {
			return Vector2(float(d["x"]), float(d["y"]));
		}
		if (d.has("x") && d.has("y") && d.has("z") && d.size() == 3) {
			return Vector3(float(d["x"]), float(d["y"]), float(d["z"]));
		}
		if (d.has("x") && d.has("y") && d.has("z") && d.has("w") && d.size() == 4) {
			return Vector4(float(d["x"]), float(d["y"]), float(d["z"]), float(d["w"]));
		}
		if (d.has("r") && d.has("g") && d.has("b")) {
			float a = d.has("a") ? float(d["a"]) : 1.0f;
			return Color(float(d["r"]), float(d["g"]), float(d["b"]), a);
		}

		Dictionary res;
		for (const Variant &k : d.get_key_list()) {
			res[k] = _deserialize_value(d[k]);
		}
		return res;
	} else if (p_value.get_type() == Variant::ARRAY) {
		Array a = p_value;
		Array res;
		for (int i = 0; i < a.size(); i++) {
			res.push_back(_deserialize_value(a[i]));
		}
		return res;
	} else if (p_value.get_type() == Variant::STRING) {
		String s = p_value;
		if (s.begins_with("#") && (s.length() == 7 || s.length() == 9)) {
			return Color::html(s);
		}
	}
	return p_value;
}
