/**************************************************************************/
/*  json.hpp                                                              */
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

#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/variant/variant.h"

#include <cstdint>
#include <initializer_list>
#include <istream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nlohmann {

class json {
public:
	class exception : public std::runtime_error {
	public:
		explicit exception(const char *what_arg) :
				std::runtime_error(what_arg) {}
		explicit exception(const std::string &what_arg) :
				std::runtime_error(what_arg) {}
	};

	json() :
			data_(Variant()) {}
	json(std::nullptr_t) :
			data_(Variant()) {}
	json(bool v) :
			data_(v) {}
	json(int v) :
			data_(v) {}
	json(int64_t v) :
			data_(v) {}
	json(size_t v) :
			data_((int64_t)v) {}
	json(double v) :
			data_(v) {}
	json(const char *v) :
			data_(String::utf8(v ? v : "")) {}
	json(const std::string &v) :
			data_(String::utf8(v.c_str())) {}
	json(const String &v) :
			data_(v) {}
	json(const Dictionary &v) :
			data_(v) {}
	json(const Array &v) :
			data_(v) {}
	json(const Variant &v) :
			data_(v) {}
	json(const std::vector<std::string> &v) {
		Array a;
		a.resize(v.size());
		for (size_t i = 0; i < v.size(); i++) {
			a[(int)i] = String::utf8(v[i].c_str());
		}
		data_ = a;
	}
	json(const std::vector<json> &v) {
		Array a;
		a.resize(v.size());
		for (size_t i = 0; i < v.size(); i++) {
			a[(int)i] = v[i].data_;
		}
		data_ = a;
	}

	json(std::initializer_list<json> init) {
		bool as_object = init.size() > 0;
		for (const json &el : init) {
			if (!el.is_array() || el.size() != 2 || !el[0].is_string()) {
				as_object = false;
				break;
			}
		}
		if (as_object) {
			Dictionary d;
			for (const json &el : init) {
				d[_to_godot_key(el[0])] = el[1].data_;
			}
			data_ = d;
		} else {
			Array a;
			a.resize(init.size());
			int i = 0;
			for (const json &el : init) {
				a[i++] = el.data_;
			}
			data_ = a;
		}
	}

	static json object() {
		return json(Dictionary());
	}
	static json array() {
		return json(Array());
	}

	static Variant _coerce_exact_ints(const Variant &v) {
		// Godot JSON may parse all numbers as FLOAT when
		// filesystem/import/json/always_parse_numbers_as_double is true.
		// Restore exact integers within the float mantissa for protocol fields.
		switch (v.get_type()) {
			case Variant::FLOAT: {
				const double d = (double)v;
				if (Math::is_finite(d) && d >= (double)INT64_MIN && d <= (double)INT64_MAX) {
					const double nearest = Math::round(d);
					if (d == nearest && Math::abs(d) <= 9007199254740992.0) { // 2^53
						return (int64_t)nearest;
					}
				}
				return v;
			}
			case Variant::ARRAY: {
				Array a = v;
				Array out;
				out.resize(a.size());
				for (int i = 0; i < a.size(); i++) {
					out[i] = _coerce_exact_ints(a[i]);
				}
				return out;
			}
			case Variant::DICTIONARY: {
				Dictionary d = v;
				Dictionary out;
				const Array keys = d.keys();
				for (int i = 0; i < keys.size(); i++) {
					const Variant key = keys[i];
					out[key] = _coerce_exact_ints(d[key]);
				}
				return out;
			}
			default:
				return v;
		}
	}

	static json parse(const std::string &s) {
		Ref<JSON> parser;
		parser.instantiate();
		const Error e = parser->parse(String::utf8(s.c_str()));
		if (e != OK) {
			throw exception(std::string(parser->get_error_message().utf8().get_data()));
		}
		return json(_coerce_exact_ints(parser->get_data()));
	}

	const Variant &variant() const { return data_; }
	Variant &variant() { return data_; }

	bool is_null() const { return data_.get_type() == Variant::NIL; }
	bool is_object() const { return data_.get_type() == Variant::DICTIONARY; }
	bool is_array() const { return data_.get_type() == Variant::ARRAY; }
	bool is_string() const { return data_.get_type() == Variant::STRING; }
	bool is_boolean() const { return data_.get_type() == Variant::BOOL; }
	bool is_number() const {
		return data_.get_type() == Variant::INT || data_.get_type() == Variant::FLOAT;
	}
	bool empty() const {
		if (is_null()) {
			return true;
		}
		if (is_object()) {
			return Dictionary(data_).is_empty();
		}
		if (is_array()) {
			return Array(data_).is_empty();
		}
		if (is_string()) {
			return String(data_).is_empty();
		}
		return false;
	}
	size_t size() const {
		if (is_object()) {
			return Dictionary(data_).size();
		}
		if (is_array()) {
			return Array(data_).size();
		}
		return 0;
	}

	bool contains(const char *key) const {
		if (!is_object()) {
			return false;
		}
		return Dictionary(data_).has(String::utf8(key));
	}
	bool contains(const std::string &key) const { return contains(key.c_str()); }

	std::string dump(int indent = -1) const {
		String out;
		if (indent >= 0) {
			out = JSON::stringify(data_, String(" ").repeat(indent), false, true);
		} else {
			out = JSON::stringify(data_, "", false, true);
		}
		return std::string(out.utf8().get_data());
	}

	json at(const char *key) const {
		if (!is_object() || !contains(key)) {
			throw exception(std::string("key not found: ") + key);
		}
		return json(Dictionary(data_)[String::utf8(key)]);
	}
	json at(const std::string &key) const { return at(key.c_str()); }

	json operator[](int index) const {
		if (!is_array()) {
			throw exception("not an array");
		}
		Array a = data_;
		if (index < 0 || index >= a.size()) {
			throw exception("array index out of range");
		}
		return json(a[index]);
	}

	json operator[](const char *key) {
		_ensure_object();
		Dictionary d = data_;
		const String k = String::utf8(key);
		if (!d.has(k)) {
			d[k] = Variant();
		}
		json child(d[k]);
		child.parent_dict_ = d;
		child.parent_key_ = k;
		child.has_parent_ = true;
		return child;
	}
	json operator[](const std::string &key) { return (*this)[key.c_str()]; }

	json operator[](const char *key) const {
		if (!is_object()) {
			return json();
		}
		Dictionary d = data_;
		const String k = String::utf8(key);
		if (!d.has(k)) {
			return json();
		}
		return json(d[k]);
	}
	json operator[](const std::string &key) const { return (*this)[key.c_str()]; }

	json(const json &other) :
			data_(other.data_) {}

	json &operator=(const json &other) {
		data_ = other.data_;
		_write_through();
		return *this;
	}
	json &operator=(bool v) {
		data_ = v;
		_write_through();
		return *this;
	}
	json &operator=(int v) {
		data_ = v;
		_write_through();
		return *this;
	}
	json &operator=(int64_t v) {
		data_ = v;
		_write_through();
		return *this;
	}
	json &operator=(size_t v) {
		data_ = (int64_t)v;
		_write_through();
		return *this;
	}
	json &operator=(double v) {
		data_ = v;
		_write_through();
		return *this;
	}
	json &operator=(const char *v) {
		data_ = String::utf8(v ? v : "");
		_write_through();
		return *this;
	}
	json &operator=(const std::string &v) {
		data_ = String::utf8(v.c_str());
		_write_through();
		return *this;
	}
	json &operator=(std::initializer_list<json> init) {
		*this = json(init);
		_write_through();
		return *this;
	}
	json &operator=(const std::vector<std::string> &v) {
		*this = json(v);
		_write_through();
		return *this;
	}
	json &operator=(const std::vector<json> &v) {
		*this = json(v);
		_write_through();
		return *this;
	}

	std::string value(const char *key, const char *default_value) const {
		if (!contains(key)) {
			return default_value ? default_value : "";
		}
		return (*this)[key].get<std::string>();
	}
	std::string value(const char *key, const std::string &default_value) const {
		if (!contains(key)) {
			return default_value;
		}
		return (*this)[key].get<std::string>();
	}
	json value(const char *key, const json &default_value) const {
		if (!contains(key)) {
			return default_value;
		}
		return (*this)[key];
	}
	template <typename T>
	T value(const char *key, T default_value) const {
		if (!contains(key)) {
			return default_value;
		}
		return (*this)[key].get<T>();
	}
	template <typename T>
	T value(const std::string &key, T default_value) const {
		return value(key.c_str(), default_value);
	}

	template <typename T>
	T get() const {
		return _get_impl(static_cast<T *>(nullptr));
	}

	class iterator {
		friend class json;
		const json *owner_ = nullptr;
		int index_ = 0;
		bool is_obj_ = false;

		iterator(const json *owner, int index, bool is_obj) :
				owner_(owner), index_(index), is_obj_(is_obj) {}

	public:
		iterator() = default;

		std::string key() const {
			if (!owner_ || !is_obj_) {
				return std::to_string(index_);
			}
			return _variant_to_std_string(Dictionary(owner_->data_).get_key_at_index(index_));
		}
		json value() const {
			if (!owner_) {
				return json();
			}
			if (is_obj_) {
				Dictionary d = owner_->data_;
				return json(d[d.get_key_at_index(index_)]);
			}
			return json(Array(owner_->data_)[index_]);
		}
		json operator*() const { return value(); }

		iterator &operator++() {
			++index_;
			return *this;
		}
		bool operator!=(const iterator &other) const {
			return index_ != other.index_ || owner_ != other.owner_;
		}
		bool operator==(const iterator &other) const { return !(*this != other); }
	};

	iterator begin() const {
		if (is_object()) {
			return iterator(this, 0, true);
		}
		if (is_array()) {
			return iterator(this, 0, false);
		}
		return end();
	}
	iterator end() const {
		if (is_object()) {
			return iterator(this, Dictionary(data_).size(), true);
		}
		if (is_array()) {
			return iterator(this, Array(data_).size(), false);
		}
		return iterator(this, 0, false);
	}

	friend std::istream &operator>>(std::istream &in, json &j) {
		std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		j = json::parse(s);
		return in;
	}

private:
	Variant data_;
	Dictionary parent_dict_;
	String parent_key_;
	bool has_parent_ = false;

	void _ensure_object() {
		if (!is_object()) {
			data_ = Dictionary();
			_write_through();
		}
	}

	void _write_through() {
		if (has_parent_) {
			parent_dict_[parent_key_] = data_;
		}
	}

	static String _to_godot_key(const json &j) {
		if (j.is_string()) {
			return String(j.data_);
		}
		return String::utf8(j.get<std::string>().c_str());
	}

	static std::string _variant_to_std_string(const Variant &v) {
		if (v.get_type() == Variant::STRING) {
			return std::string(String(v).utf8().get_data());
		}
		return std::string(String(v).utf8().get_data());
	}

	std::string _get_impl(std::string *) const {
		if (is_string()) {
			return _variant_to_std_string(data_);
		}
		if (is_null()) {
			return {};
		}
		return _variant_to_std_string(data_);
	}
	bool _get_impl(bool *) const { return (bool)data_; }
	int _get_impl(int *) const {
		if (data_.get_type() == Variant::INT) {
			return (int)(int64_t)data_;
		}
		if (data_.get_type() == Variant::FLOAT) {
			return (int)Math::round((double)data_);
		}
		return (int)data_;
	}
	int64_t _get_impl(int64_t *) const {
		if (data_.get_type() == Variant::INT) {
			return (int64_t)data_;
		}
		if (data_.get_type() == Variant::FLOAT) {
			return (int64_t)Math::round((double)data_);
		}
		return (int64_t)data_;
	}
	size_t _get_impl(size_t *) const {
		int64_t v = _get_impl(static_cast<int64_t *>(nullptr));
		return v < 0 ? 0 : (size_t)v;
	}
	double _get_impl(double *) const { return (double)data_; }
	json _get_impl(json *) const { return *this; }

	std::vector<std::string> _get_impl(std::vector<std::string> *) const {
		std::vector<std::string> out;
		if (!is_array()) {
			return out;
		}
		Array a = data_;
		out.reserve(a.size());
		for (int i = 0; i < a.size(); i++) {
			out.push_back(_variant_to_std_string(a[i]));
		}
		return out;
	}
	std::vector<json> _get_impl(std::vector<json> *) const {
		std::vector<json> out;
		if (!is_array()) {
			return out;
		}
		Array a = data_;
		out.reserve(a.size());
		for (int i = 0; i < a.size(); i++) {
			out.push_back(json(a[i]));
		}
		return out;
	}
};

} //namespace nlohmann
