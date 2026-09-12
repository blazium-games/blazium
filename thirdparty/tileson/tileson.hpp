#include <algorithm>
#include <iterator>
///
/// T I L E S O N   V E R S I O N   1 . 4 . 0
/// ------------------------------------------------
/// BSD 2-Clause License
///
/// Copyright (c) 2022, Robin Berg Pettersen
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are met:
///
/// 1. Redistributions of source code must retain the above copyright notice, this
///    list of conditions and the following disclaimer.
///
/// 2. Redistributions in binary form must reproduce the above copyright notice,
///    this list of conditions and the following disclaimer in the documentation
///    and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
/// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
/// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
/// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
/// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
/// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
/// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
/// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef TILESON_TILESON_H
#define TILESON_TILESON_H


/*** Start of inlined file: json11.hpp ***/
/*** Start of inlined file: json11.cpp ***/

/*** Start of inlined file: json11.hpp ***/
/* Copyright (c) 2013 Dropbox, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>

#define JSON11_IS_DEFINED

#ifdef _MSC_VER
#if _MSC_VER <= 1800 // VS 2013
		#ifndef noexcept
			#define noexcept throw()
		#endif

		#ifndef snprintf
			#define snprintf _snprintf_s
		#endif
	#endif
#endif

namespace json11 {

	enum JsonParse {
		STANDARD, COMMENTS
	};

	class JsonValue;

	class Json final {
		public:
			// Types
			enum Type {
				NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
			};

			// Array and object typedefs
			typedef std::vector<Json> array;
			typedef std::map<std::string, Json> object;

			// Constructors for the various types of JSON value.
			inline Json() noexcept;                // NUL
			inline Json(std::nullptr_t) noexcept;  // NUL
			inline Json(double value);             // NUMBER
			inline Json(int value);                // NUMBER
			inline Json(bool value);               // BOOL
			inline Json(const std::string &value); // STRING
			inline Json(std::string &&value);      // STRING
			inline Json(const char * value);       // STRING
			inline Json(const array &values);      // ARRAY
			inline Json(array &&values);           // ARRAY
			inline Json(const object &values);     // OBJECT
			inline Json(object &&values);          // OBJECT

			// Implicit constructor: anything with a to_json() function.
			template <class T, class = decltype(&T::to_json)>
			inline Json(const T & t) : Json(t.to_json()) {}

			// Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
			template <class M, typename std::enable_if<
					std::is_constructible<std::string, decltype(std::declval<M>().begin()->first)>::value
					&& std::is_constructible<Json, decltype(std::declval<M>().begin()->second)>::value,
					int>::type = 0>
			inline Json(const M & m) : Json(object(m.begin(), m.end())) {}

			// Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
			template <class V, typename std::enable_if<
					std::is_constructible<Json, decltype(*std::declval<V>().begin())>::value,
					int>::type = 0>
			inline Json(const V & v) : Json(array(v.begin(), v.end())) {}

			// This prevents Json(some_pointer) from accidentally producing a bool. Use
			// Json(bool(some_pointer)) if that behavior is desired.
			Json(void *) = delete;

			// Accessors
			inline Type type() const;

			inline bool is_null()   const { return type() == NUL; }
			inline bool is_number() const { return type() == NUMBER; }
			inline bool is_bool()   const { return type() == BOOL; }
			inline bool is_string() const { return type() == STRING; }
			inline bool is_array()  const { return type() == ARRAY; }
			inline bool is_object() const { return type() == OBJECT; }

			// Return the enclosed value if this is a number, 0 otherwise. Note that json11 does not
			// distinguish between integer and non-integer numbers - number_value() and int_value()
			// can both be applied to a NUMBER-typed object.
			inline double number_value() const;
			inline int int_value() const;

			// Return the enclosed value if this is a boolean, false otherwise.
			inline bool bool_value() const;
			// Return the enclosed string if this is a string, "" otherwise.
			inline const std::string &string_value() const;
			// Return the enclosed std::vector if this is an array, or an empty vector otherwise.
			inline const array &array_items() const;
			// Return the enclosed std::map if this is an object, or an empty map otherwise.
			inline const object &object_items() const;

			// Return a reference to arr[i] if this is an array, Json() otherwise.
			inline const Json & operator[](size_t i) const;
			// Return a reference to obj[key] if this is an object, Json() otherwise.
			inline const Json & operator[](const std::string &key) const;

			// Serialize.
			inline void dump(std::string &out) const;
			inline std::string dump() const {
				std::string out;
				dump(out);
				return out;
			}

			// Parse. If parse fails, return Json() and assign an error message to err.
			static inline Json parse(const std::string & in,
							  std::string & err,
							  JsonParse strategy = JsonParse::STANDARD);
			static inline Json parse(const char * in,
							  std::string & err,
							  JsonParse strategy = JsonParse::STANDARD) {
				if (in) {
					return parse(std::string(in), err, strategy);
				} else {
					err = "null input";
					return nullptr;
				}
			}
			// Parse multiple objects, concatenated or separated by whitespace
			static inline std::vector<Json> parse_multi(
					const std::string & in,
					std::string::size_type & parser_stop_pos,
					std::string & err,
					JsonParse strategy = JsonParse::STANDARD);

			static inline std::vector<Json> parse_multi(
					const std::string & in,
					std::string & err,
					JsonParse strategy = JsonParse::STANDARD) {
				std::string::size_type parser_stop_pos;
				return parse_multi(in, parser_stop_pos, err, strategy);
			}

			inline bool operator== (const Json &rhs) const;
			inline bool operator<  (const Json &rhs) const;
			inline bool operator!= (const Json &rhs) const { return !(*this == rhs); }
			inline bool operator<= (const Json &rhs) const { return !(rhs < *this); }
			inline bool operator>  (const Json &rhs) const { return  (rhs < *this); }
			inline bool operator>= (const Json &rhs) const { return !(*this < rhs); }

			/* has_shape(types, err)
			 *
			 * Return true if this is a JSON object and, for each item in types, has a field of
			 * the given type. If not, return false and set err to a descriptive message.
			 */
			typedef std::initializer_list<std::pair<std::string, Type>> shape;
			inline bool has_shape(const shape & types, std::string & err) const;

		private:
			std::shared_ptr<JsonValue> m_ptr;
	};

// Internal class hierarchy - JsonValue objects are not exposed to users of this API.
	class JsonValue {
		protected:
			friend class Json;
			friend class JsonInt;
			friend class JsonDouble;
			virtual Json::Type type() const = 0;
			virtual bool equals(const JsonValue * other) const = 0;
			virtual bool less(const JsonValue * other) const = 0;
			virtual void dump(std::string &out) const = 0;
			virtual double number_value() const;
			virtual int int_value() const;
			virtual bool bool_value() const;
			virtual const std::string &string_value() const;
			virtual const Json::array &array_items() const;
			virtual const Json &operator[](size_t i) const;
			virtual const Json::object &object_items() const;
			virtual const Json &operator[](const std::string &key) const;
			virtual ~JsonValue() {}
	};

} // namespace json11

/*** End of inlined file: json11.hpp ***/

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

namespace json11 {

	static const int max_depth = 200;

	using std::string;
	using std::vector;
	using std::map;
	using std::make_shared;
	using std::initializer_list;
	using std::move;

/* Helper for representing null - just a do-nothing struct, plus comparison
 * operators so the helpers in JsonValue work. We can't use nullptr_t because
 * it may not be orderable.
 */
	struct NullStruct {
		bool operator==(NullStruct) const { return true; }
		bool operator<(NullStruct) const { return false; }
	};

/* * * * * * * * * * * * * * * * * * * *
 * Serialization
 */

	static void dump(NullStruct, string &out) {
		out += "null";
	}

	static void dump(double value, string &out) {
		if (std::isfinite(value)) {
			char buf[32];
			snprintf(buf, sizeof buf, "%.17g", value);
			out += buf;
		} else {
			out += "null";
		}
	}

	static void dump(int value, string &out) {
		char buf[32];
		snprintf(buf, sizeof buf, "%d", value);
		out += buf;
	}

	static void dump(bool value, string &out) {
		out += value ? "true" : "false";
	}

	static void dump(const string &value, string &out) {
		out += '"';
		for (size_t i = 0; i < value.length(); i++) {
			const char ch = value[i];
			if (ch == '\\') {
				out += "\\\\";
			} else if (ch == '"') {
				out += "\\\"";
			} else if (ch == '\b') {
				out += "\\b";
			} else if (ch == '\f') {
				out += "\\f";
			} else if (ch == '\n') {
				out += "\\n";
			} else if (ch == '\r') {
				out += "\\r";
			} else if (ch == '\t') {
				out += "\\t";
			} else if (static_cast<uint8_t>(ch) <= 0x1f) {
				char buf[8];
				snprintf(buf, sizeof buf, "\\u%04x", ch);
				out += buf;
			} else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1]) == 0x80
					   && static_cast<uint8_t>(value[i+2]) == 0xa8) {
				out += "\\u2028";
				i += 2;
			} else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1]) == 0x80
					   && static_cast<uint8_t>(value[i+2]) == 0xa9) {
				out += "\\u2029";
				i += 2;
			} else {
				out += ch;
			}
		}
		out += '"';
	}

	static void dump(const Json::array &values, string &out) {
		bool first = true;
		out += "[";
		for (const auto &value : values) {
			if (!first)
				out += ", ";
			value.dump(out);
			first = false;
		}
		out += "]";
	}

	static void dump(const Json::object &values, string &out) {
		bool first = true;
		out += "{";
		for (const auto &kv : values) {
			if (!first)
				out += ", ";
			dump(kv.first, out);
			out += ": ";
			kv.second.dump(out);
			first = false;
		}
		out += "}";
	}

	void Json::dump(string &out) const {
		m_ptr->dump(out);
	}

/* * * * * * * * * * * * * * * * * * * *
 * Value wrappers
 */

	template <Json::Type tag, typename T>
	class Value : public JsonValue {
		protected:

			// Constructors
			explicit Value(const T &value) : m_value(value) {}
			explicit Value(T &&value)      : m_value(std::move(value)) {}

			// Get type tag
			Json::Type type() const override {
				return tag;
			}

			// Comparisons
			bool equals(const JsonValue * other) const override {
				return m_value == static_cast<const Value<tag, T> *>(other)->m_value;
			}
			bool less(const JsonValue * other) const override {
				return m_value < static_cast<const Value<tag, T> *>(other)->m_value;
			}

			const T m_value;
			void dump(string &out) const override { json11::dump(m_value, out); }
	};

	class JsonDouble final : public Value<Json::NUMBER, double> {
			double number_value() const override { return m_value; }
			int int_value() const override { return static_cast<int>(m_value); }
			bool equals(const JsonValue * other) const override { return m_value == other->number_value(); }
			bool less(const JsonValue * other)   const override { return m_value <  other->number_value(); }
		public:
			explicit JsonDouble(double value) : Value(value) {}
	};

	class JsonInt final : public Value<Json::NUMBER, int> {
			double number_value() const override { return m_value; }
			int int_value() const override { return m_value; }
			bool equals(const JsonValue * other) const override { return m_value == other->number_value(); }
			bool less(const JsonValue * other)   const override { return m_value <  other->number_value(); }
		public:
			explicit JsonInt(int value) : Value(value) {}
	};

	class JsonBoolean final : public Value<Json::BOOL, bool> {
			bool bool_value() const override { return m_value; }
		public:
			explicit JsonBoolean(bool value) : Value(value) {}
	};

	class JsonString final : public Value<Json::STRING, string> {
			const string &string_value() const override { return m_value; }
		public:
			explicit JsonString(const string &value) : Value(value) {}
			explicit JsonString(string &&value)      : Value(std::move(value)) {}
	};

	class JsonArray final : public Value<Json::ARRAY, Json::array> {
			const Json::array &array_items() const override { return m_value; }
			const Json & operator[](size_t i) const override;
		public:
			explicit JsonArray(const Json::array &value) : Value(value) {}
			explicit JsonArray(Json::array &&value)      : Value(std::move(value)) {}
	};

	class JsonObject final : public Value<Json::OBJECT, Json::object> {
			const Json::object &object_items() const override { return m_value; }
			const Json & operator[](const string &key) const override;
		public:
			explicit JsonObject(const Json::object &value) : Value(value) {}
			explicit JsonObject(Json::object &&value)      : Value(std::move(value)) {}
	};

	class JsonNull final : public Value<Json::NUL, NullStruct> {
		public:
			JsonNull() : Value({}) {}
	};

/* * * * * * * * * * * * * * * * * * * *
 * Static globals - static-init-safe
 */
	struct Statics {
		const std::shared_ptr<JsonValue> null = make_shared<JsonNull>();
		const std::shared_ptr<JsonValue> t = make_shared<JsonBoolean>(true);
		const std::shared_ptr<JsonValue> f = make_shared<JsonBoolean>(false);
		const string empty_string;
		const vector<Json> empty_vector;
		const map<string, Json> empty_map;
		Statics() {}
	};

	static const Statics & statics() {
		static const Statics s {};
		return s;
	}

	static const Json & static_null() {
		// This has to be separate, not in Statics, because Json() accesses statics().null.
		static const Json json_null;
		return json_null;
	}

/* * * * * * * * * * * * * * * * * * * *
 * Constructors
 */

	Json::Json() noexcept                  : m_ptr(statics().null) {}
	Json::Json(std::nullptr_t) noexcept    : m_ptr(statics().null) {}
	Json::Json(double value)               : m_ptr(make_shared<JsonDouble>(value)) {}
	Json::Json(int value)                  : m_ptr(make_shared<JsonInt>(value)) {}
	Json::Json(bool value)                 : m_ptr(value ? statics().t : statics().f) {}
	Json::Json(const string &value)        : m_ptr(make_shared<JsonString>(value)) {}
	Json::Json(string &&value)             : m_ptr(make_shared<JsonString>(std::move(value))) {}
	Json::Json(const char * value)         : m_ptr(make_shared<JsonString>(value)) {}
	Json::Json(const Json::array &values)  : m_ptr(make_shared<JsonArray>(values)) {}
	Json::Json(Json::array &&values)       : m_ptr(make_shared<JsonArray>(std::move(values))) {}
	Json::Json(const Json::object &values) : m_ptr(make_shared<JsonObject>(values)) {}
	Json::Json(Json::object &&values)      : m_ptr(make_shared<JsonObject>(std::move(values))) {}

/* * * * * * * * * * * * * * * * * * * *
 * Accessors
 */

	inline Json::Type Json::type()                           const { return m_ptr->type();         }
	inline double Json::number_value()                       const { return m_ptr->number_value(); }
	inline int Json::int_value()                             const { return m_ptr->int_value();    }
	inline bool Json::bool_value()                           const { return m_ptr->bool_value();   }
	inline const string & Json::string_value()               const { return m_ptr->string_value(); }
	inline const vector<Json> & Json::array_items()          const { return m_ptr->array_items();  }
	inline const map<string, Json> & Json::object_items()    const { return m_ptr->object_items(); }
	inline const Json & Json::operator[] (size_t i)          const { return (*m_ptr)[i];           }
	inline const Json & Json::operator[] (const string &key) const { return (*m_ptr)[key];         }

	inline double                    JsonValue::number_value()              const { return 0; }
	inline int                       JsonValue::int_value()                 const { return 0; }
	inline bool                      JsonValue::bool_value()                const { return false; }
	inline const string &            JsonValue::string_value()              const { return statics().empty_string; }
	inline const vector<Json> &      JsonValue::array_items()               const { return statics().empty_vector; }
	inline const map<string, Json> & JsonValue::object_items()              const { return statics().empty_map; }
	inline const Json &              JsonValue::operator[] (size_t)         const { return static_null(); }
	inline const Json &              JsonValue::operator[] (const string &) const { return static_null(); }

	inline const Json & JsonObject::operator[] (const string &key) const {
		auto iter = m_value.find(key);
		return (iter == m_value.end()) ? static_null() : iter->second;
	}
	inline const Json & JsonArray::operator[] (size_t i) const {
		if (i >= m_value.size()) return static_null();
		else return m_value[i];
	}

/* * * * * * * * * * * * * * * * * * * *
 * Comparison
 */

	bool Json::operator== (const Json &other) const {
		if (m_ptr == other.m_ptr)
			return true;
		if (m_ptr->type() != other.m_ptr->type())
			return false;

		return m_ptr->equals(other.m_ptr.get());
	}

	bool Json::operator< (const Json &other) const {
		if (m_ptr == other.m_ptr)
			return false;
		if (m_ptr->type() != other.m_ptr->type())
			return m_ptr->type() < other.m_ptr->type();

		return m_ptr->less(other.m_ptr.get());
	}

/* * * * * * * * * * * * * * * * * * * *
 * Parsing
 */

/* esc(c)
 *
 * Format char c suitable for printing in an error message.
 */
	static inline string esc(char c) {
		char buf[12];
		if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
			snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
		} else {
			snprintf(buf, sizeof buf, "(%d)", c);
		}
		return string(buf);
	}

	static inline bool in_range(long x, long lower, long upper) {
		return (x >= lower && x <= upper);
	}

	namespace {
/* JsonParser
 *
 * Object that tracks all state of an in-progress parse.
 */
		struct JsonParser final {

			/* State
			 */
			const string &str;
			size_t i;
			string &err;
			bool failed;
			const JsonParse strategy;

			/* fail(msg, err_ret = Json())
			 *
			 * Mark this parse as failed.
			 */
			Json fail(string &&msg) {
				return fail(std::move(msg), Json());
			}

			template <typename T>
			T fail(string &&msg, const T err_ret) {
				if (!failed)
					err = std::move(msg);
				failed = true;
				return err_ret;
			}

			/* consume_whitespace()
			 *
			 * Advance until the current character is non-whitespace.
			 */
			void consume_whitespace() {
				while (str[i] == ' ' || str[i] == '\r' || str[i] == '\n' || str[i] == '\t')
					i++;
			}

			/* consume_comment()
			 *
			 * Advance comments (c-style inline and multiline).
			 */
			bool consume_comment() {
				bool comment_found = false;
				if (str[i] == '/') {
					i++;
					if (i == str.size())
						return fail("unexpected end of input after start of comment", false);
					if (str[i] == '/') { // inline comment
						i++;
						// advance until next line, or end of input
						while (i < str.size() && str[i] != '\n') {
							i++;
						}
						comment_found = true;
					}
					else if (str[i] == '*') { // multiline comment
						i++;
						if (i > str.size()-2)
							return fail("unexpected end of input inside multi-line comment", false);
						// advance until closing tokens
						while (!(str[i] == '*' && str[i+1] == '/')) {
							i++;
							if (i > str.size()-2)
								return fail(
										"unexpected end of input inside multi-line comment", false);
						}
						i += 2;
						comment_found = true;
					}
					else
						return fail("malformed comment", false);
				}
				return comment_found;
			}

			/* consume_garbage()
			 *
			 * Advance until the current character is non-whitespace and non-comment.
			 */
			void consume_garbage() {
				consume_whitespace();
				if(strategy == JsonParse::COMMENTS) {
					bool comment_found = false;
					do {
						comment_found = consume_comment();
						if (failed) return;
						consume_whitespace();
					}
					while(comment_found);
				}
			}

			/* get_next_token()
			 *
			 * Return the next non-whitespace character. If the end of the input is reached,
			 * flag an error and return 0.
			 */
			char get_next_token() {
				consume_garbage();
				if (failed) return static_cast<char>(0);
				if (i == str.size())
					return fail("unexpected end of input", static_cast<char>(0));

				return str[i++];
			}

			/* encode_utf8(pt, out)
			 *
			 * Encode pt as UTF-8 and add it to out.
			 */
			void encode_utf8(long pt, string & out) {
				if (pt < 0)
					return;

				if (pt < 0x80) {
					out += static_cast<char>(pt);
				} else if (pt < 0x800) {
					out += static_cast<char>((pt >> 6) | 0xC0);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				} else if (pt < 0x10000) {
					out += static_cast<char>((pt >> 12) | 0xE0);
					out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				} else {
					out += static_cast<char>((pt >> 18) | 0xF0);
					out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
					out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				}
			}

			/* parse_string()
			 *
			 * Parse a string, starting at the current position.
			 */
			string parse_string() {
				string out;
				long last_escaped_codepoint = -1;
				while (true) {
					if (i == str.size())
						return fail("unexpected end of input in string", "");

					char ch = str[i++];

					if (ch == '"') {
						encode_utf8(last_escaped_codepoint, out);
						return out;
					}

					if (in_range(ch, 0, 0x1f))
						return fail("unescaped " + esc(ch) + " in string", "");

					// The usual case: non-escaped characters
					if (ch != '\\') {
						encode_utf8(last_escaped_codepoint, out);
						last_escaped_codepoint = -1;
						out += ch;
						continue;
					}

					// Handle escapes
					if (i == str.size())
						return fail("unexpected end of input in string", "");

					ch = str[i++];

					if (ch == 'u') {
						// Extract 4-byte escape sequence
						string esc = str.substr(i, 4);
						// Explicitly check length of the substring. The following loop
						// relies on std::string returning the terminating NUL when
						// accessing str[length]. Checking here reduces brittleness.
						if (esc.length() < 4) {
							return fail("bad \\u escape: " + esc, "");
						}
						for (size_t j = 0; j < 4; j++) {
							if (!in_range(esc[j], 'a', 'f') && !in_range(esc[j], 'A', 'F')
								&& !in_range(esc[j], '0', '9'))
								return fail("bad \\u escape: " + esc, "");
						}

						long codepoint = strtol(esc.data(), nullptr, 16);

						// JSON specifies that characters outside the BMP shall be encoded as a pair
						// of 4-hex-digit \u escapes encoding their surrogate pair components. Check
						// whether we're in the middle of such a beast: the previous codepoint was an
						// escaped lead (high) surrogate, and this is a trail (low) surrogate.
						if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
							&& in_range(codepoint, 0xDC00, 0xDFFF)) {
							// Reassemble the two surrogate pairs into one astral-plane character, per
							// the UTF-16 algorithm.
							encode_utf8((((last_escaped_codepoint - 0xD800) << 10)
										 | (codepoint - 0xDC00)) + 0x10000, out);
							last_escaped_codepoint = -1;
						} else {
							encode_utf8(last_escaped_codepoint, out);
							last_escaped_codepoint = codepoint;
						}

						i += 4;
						continue;
					}

					encode_utf8(last_escaped_codepoint, out);
					last_escaped_codepoint = -1;

					if (ch == 'b') {
						out += '\b';
					} else if (ch == 'f') {
						out += '\f';
					} else if (ch == 'n') {
						out += '\n';
					} else if (ch == 'r') {
						out += '\r';
					} else if (ch == 't') {
						out += '\t';
					} else if (ch == '"' || ch == '\\' || ch == '/') {
						out += ch;
					} else {
						return fail("invalid escape character " + esc(ch), "");
					}
				}
			}

			/* parse_number()
			 *
			 * Parse a double.
			 */
			Json parse_number() {
				size_t start_pos = i;

				if (str[i] == '-')
					i++;

				// Integer part
				if (str[i] == '0') {
					i++;
					if (in_range(str[i], '0', '9'))
						return fail("leading 0s not permitted in numbers");
				} else if (in_range(str[i], '1', '9')) {
					i++;
					while (in_range(str[i], '0', '9'))
						i++;
				} else {
					return fail("invalid " + esc(str[i]) + " in number");
				}

				if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
					&& (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10)) {
					return std::atoi(str.c_str() + start_pos);
				}

				// Decimal part
				if (str[i] == '.') {
					i++;
					if (!in_range(str[i], '0', '9'))
						return fail("at least one digit required in fractional part");

					while (in_range(str[i], '0', '9'))
						i++;
				}

				// Exponent part
				if (str[i] == 'e' || str[i] == 'E') {
					i++;

					if (str[i] == '+' || str[i] == '-')
						i++;

					if (!in_range(str[i], '0', '9'))
						return fail("at least one digit required in exponent");

					while (in_range(str[i], '0', '9'))
						i++;
				}

				return std::strtod(str.c_str() + start_pos, nullptr);
			}

			/* expect(str, res)
			 *
			 * Expect that 'str' starts at the character that was just read. If it does, advance
			 * the input and return res. If not, flag an error.
			 */
			Json expect(const string &expected, Json res) {
				assert(i != 0);
				i--;
				if (str.compare(i, expected.length(), expected) == 0) {
					i += expected.length();
					return res;
				} else {
					return fail("parse error: expected " + expected + ", got " + str.substr(i, expected.length()));
				}
			}

			/* parse_json()
			 *
			 * Parse a JSON object.
			 */
			Json parse_json(int depth) {
				if (depth > max_depth) {
					return fail("exceeded maximum nesting depth");
				}

				char ch = get_next_token();
				if (failed)
					return Json();

				if (ch == '-' || (ch >= '0' && ch <= '9')) {
					i--;
					return parse_number();
				}

				if (ch == 't')
					return expect("true", true);

				if (ch == 'f')
					return expect("false", false);

				if (ch == 'n')
					return expect("null", Json());

				if (ch == '"')
					return parse_string();

				if (ch == '{') {
					map<string, Json> data;
					ch = get_next_token();
					if (ch == '}')
						return data;

					while (1) {
						if (ch != '"')
							return fail("expected '\"' in object, got " + esc(ch));

						string key = parse_string();
						if (failed)
							return Json();

						ch = get_next_token();
						if (ch != ':')
							return fail("expected ':' in object, got " + esc(ch));

						data[std::move(key)] = parse_json(depth + 1);
						if (failed)
							return Json();

						ch = get_next_token();
						if (ch == '}')
							break;
						if (ch != ',')
							return fail("expected ',' in object, got " + esc(ch));

						ch = get_next_token();
					}
					return data;
				}

				if (ch == '[') {
					vector<Json> data;
					ch = get_next_token();
					if (ch == ']')
						return data;

					while (1) {
						i--;
						data.push_back(parse_json(depth + 1));
						if (failed)
							return Json();

						ch = get_next_token();
						if (ch == ']')
							break;
						if (ch != ',')
							return fail("expected ',' in list, got " + esc(ch));

						ch = get_next_token();
						(void)ch;
					}
					return data;
				}

				return fail("expected value, got " + esc(ch));
			}
		};
	}//namespace {

	Json Json::parse(const string &in, string &err, JsonParse strategy) {
		JsonParser parser { in, 0, err, false, strategy };
		Json result = parser.parse_json(0);

		// Check for any trailing garbage
		parser.consume_garbage();
		if (parser.failed)
			return Json();
		if (parser.i != in.size() &&
			((parser.i + 1) != in.size() && in[parser.i] != 0)) //RBP: If there is only 1 character diff, it is probably just a terminating zero from a memory read.
		{
			return parser.fail("unexpected trailing " + esc(in[parser.i]));
		}
		return result;
	}

// Documented in json11.hpp
	vector<Json> Json::parse_multi(const string &in,
								   std::string::size_type &parser_stop_pos,
								   string &err,
								   JsonParse strategy) {
		JsonParser parser { in, 0, err, false, strategy };
		parser_stop_pos = 0;
		vector<Json> json_vec;
		while (parser.i != in.size() && !parser.failed) {
			json_vec.push_back(parser.parse_json(0));
			if (parser.failed)
				break;

			// Check for another object
			parser.consume_garbage();
			if (parser.failed)
				break;
			parser_stop_pos = parser.i;
		}
		return json_vec;
	}

/* * * * * * * * * * * * * * * * * * * *
 * Shape-checking
 */

	bool Json::has_shape(const shape & types, string & err) const {
		if (!is_object()) {
			err = "expected JSON object, got " + dump();
			return false;
		}

		const auto& obj_items = object_items();
		for (auto & item : types) {
			const auto it = obj_items.find(item.first);
			if (it == obj_items.cend() || it->second.type() != item.second) {
				err = "bad type for " + item.first + " in " + dump();
				return false;
			}
		}

		return true;
	}

} // namespace json11

/*** End of inlined file: json11.cpp ***/

/*** End of inlined file: json11.hpp ***/


/*** Start of inlined file: tileson_parser.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_TILESON_PARSER_HPP
#define TILESON_TILESON_PARSER_HPP

//RBP: FS-namespace is defined in tileson_parser now!
#if defined(IOS_ENABLED) || (defined(MACOS_ENABLED) && !defined(NO_MACOS_FS_STUB))
	#include <string>
	namespace fs {
		class path {
			std::string p;
		public:
			path() {}
			path(const char* c) : p(c ? c : "") {}
			path(const std::string& s) : p(s) {}
			std::string generic_string() const { return p; }
			std::string string() const { return p; }
			path parent_path() const { 
				size_t pos = p.find_last_of("/\\");
				return (pos == std::string::npos) ? path() : path(p.substr(0, pos));
			}
			std::string extension() const { return ""; }
			path filename() const { return path(); }
			bool empty() const { return p.empty(); }
			path operator/(const path& rhs) const { return path(p + "/" + rhs.p); }
			path operator/(const std::string& rhs) const { return path(p + "/" + rhs); }
			path operator/(const char* rhs) const { return path(p + "/" + std::string(rhs ? rhs : "")); }
		};
		inline bool exists(const path&) { return false; }
		inline bool is_regular_file(const path&) { return false; }
		inline bool is_directory(const path&) { return false; }
		
		struct directory_entry {
			path path() const { return fs::path(); }
		};
		struct directory_iterator {
			directory_iterator() {}
			directory_iterator(const path&) {}
			const directory_entry* begin() const { return nullptr; }
			const directory_entry* end() const { return nullptr; }
		};
	}
#else
#if _MSC_VER && !__INTEL_COMPILER
	#include <filesystem>
	namespace fs = std::filesystem;
#elif __MINGW64__
	#if __MINGW64_VERSION_MAJOR > 6
		#include <filesystem>
		namespace fs = std::filesystem;
	#else
		#include <experimental/filesystem>
		namespace fs = std::experimental::filesystem;
	#endif
#elif __clang__
	#if __clang_major__ < 8
		#include <experimental/filesystem>
		namespace fs = std::experimental::filesystem;
	#else
		#include <filesystem>
		namespace fs = std::filesystem;
	#endif
#else //Linux
	#if __GNUC__ < 8 //GCC major version less than 8
		#include <experimental/filesystem>
		namespace fs = std::experimental::filesystem;
	#else
		#include <filesystem>
		namespace fs = std::filesystem;
	#endif
#endif
#endif

#include <fstream>
#include <sstream>
#include <memory>


/*** Start of inlined file: Tools.hpp ***/
//
// Created by robin on 31.07.2020.
//

#ifndef TILESON_TOOLS_HPP
#define TILESON_TOOLS_HPP

#include <cstdint>
#include <vector>
#include <string_view>
namespace tson
{
	class Tools
	{

		public:
			Tools() = delete;
			~Tools() = delete;
			inline static std::vector<uint8_t> Base64DecodedStringToBytes(std::string_view str);
			inline static std::vector<uint32_t> BytesToUnsignedInts(const std::vector<uint8_t> &bytes);
			inline static std::vector<std::string> SplitString(const std::string &s, char delim);
			inline static bool Equal(float a, float b, float precision = 8192.f);

		private:
			template<typename Out>
			static void split(const std::string &s, char delim, Out result)
			{
				std::stringstream ss;
				ss.str(s);
				std::string item;

				while (std::getline(ss, item, delim))
				{
					*(result++) = item;
				}
			}
	};

	/*!
	 * When you have decoded a Base64 string, you'll get a string representing bytes. This function turns them into actual bytes.
	 * @param str
	 * @return
	 */
	std::vector<uint8_t> Tools::Base64DecodedStringToBytes(std::string_view str)
	{
		std::vector<uint8_t> bytes;
		for(size_t i = 0; i < str.size(); ++i)
		{
			uint8_t u8 = static_cast<uint8_t>(str[i]);
			bytes.push_back(u8);
		}
		return bytes;
	}

	/*!
	 * Converts bytes into unsigned int values. The bytes are converted in the Little Endian byte order to fit Tiled's specs.
	 * @param bytes A vector of bytes.
	 * @return Bytes converted to unsigned ints
	 */
	std::vector<uint32_t> Tools::BytesToUnsignedInts(const std::vector<uint8_t> &bytes)
	{
		std::vector<uint32_t> uints;
		std::vector<uint8_t> toConvert;
		//uint32_t size8 = (compressed[55] << 24) | (compressed[56] << 16) | (compressed[57] << 8) | compressed[58]; //Should be 66000

		for(size_t i = 0; i < bytes.size(); ++i)
		{
			toConvert.push_back(bytes[i]);
			if(toConvert.size() == 4)
			{
				uint32_t u32 = (toConvert[3] << 24) | (toConvert[2] << 16) | (toConvert[1] << 8) | toConvert[0];
				uints.push_back(u32);
				toConvert.clear();
			}
		}

		return uints;
	}

	std::vector<std::string> Tools::SplitString(const std::string &s, char delim)
	{
		std::vector<std::string> elems;
		split(s, delim, std::back_inserter(elems));
		return elems;
	}

	/*!
	 * Uses a threshold for comparing floats, as they are not precise in many cases.
	 * @param a
	 * @param b
	 * @return true if equal based on the currently defined precision
	 */
	bool Tools::Equal(float a, float b, float precision)
	{
		float threshold = 1.f / precision;
		float diff = fabsf(a - b);
		return diff <= threshold;
	}
}

#endif //TILESON_TOOLS_HPP

/*** End of inlined file: Tools.hpp ***/


/*** Start of inlined file: Base64Decompressor.hpp ***/
//
// Created by robin on 29.07.2020.
// The Base64 decoding logic is heavily based on: https://github.com/ReneNyffenegger/cpp-base64
//

#ifndef TILESON_BASE64DECOMPRESSOR_HPP
#define TILESON_BASE64DECOMPRESSOR_HPP


/*** Start of inlined file: IDecompressor.hpp ***/
//
// Created by robin on 29.07.2020.
//

#ifndef TILESON_IDECOMPRESSOR_HPP
#define TILESON_IDECOMPRESSOR_HPP

#include <string_view>

namespace tson
{
	template <class TIn, class TOut>
	class IDecompressor
	{
		public:
			/*!
			 * If the name matches with 'compression' or 'encoding' the decompress() function will
			 * be called automatically for the actual Layer. Encoding-related matching is handled first!
			 *
			 * Known values:
			 *
			 * compression: zlib, gzip, zstd (since Tiled 1.3) or empty (default) (tilelayer only).
			 * encoding: csv (default) or base64 (tilelayer only).
			 *
			 * @return
			 */
			[[nodiscard]] virtual const std::string &name() const = 0;

			/*!
			 * Used primarily for Tiled related decompression.
			 * @param input Input data
			 * @return Decompressed data
			 */
			virtual TOut decompress(const TIn &input) = 0;

			/*!
			 * Used for whole file decompression. Not related to Tiled
			 * @param path
			 * @return
			 */
			virtual TOut decompressFile(const fs::path &path) = 0;

			/*!
			 * Used for whole file decompression. Not related to Tiled
			 * @param path
			 * @return
			 */
			virtual TOut decompress(const void *data, size_t size) = 0;

			/*!
			 * Pure virtual class needs virtual destructor so derived classes can call their own destructors
			*/
			virtual ~IDecompressor() = default;
	};
}

#endif //TILESON_IDECOMPRESSOR_HPP

/*** End of inlined file: IDecompressor.hpp ***/

#include <string>

namespace tson
{
	class Base64Decompressor : public IDecompressor<std::string_view, std::string>
	{
		public:
			[[nodiscard]] inline const std::string &name() const override;

			inline std::string decompress(const std::string_view &s) override;

			inline std::string decompressFile(const fs::path &path) override;
			inline std::string decompress(const void *data, size_t size) override;

		private:
			inline unsigned int pos_of_char(const unsigned char chr);
			inline static const std::string NAME = "base64";
	};

	const std::string &Base64Decompressor::name() const
	{
		return NAME;
	}

	std::string Base64Decompressor::decompress(const std::string_view &s)
	{

		size_t length_of_string = s.length();
		if (!length_of_string) return std::string("");

		size_t in_len = length_of_string;
		size_t pos = 0;

		//
		// The approximate length (bytes) of the decoded string might be one ore
		// two bytes smaller, depending on the amount of trailing equal signs
		// in the encoded string. This approximation is needed to reserve
		// enough space in the string to be returned.
		//
		size_t approx_length_of_decoded_string = length_of_string / 4 * 3;
		std::string ret;
		ret.reserve(approx_length_of_decoded_string);

		while (pos < in_len) {

			unsigned int pos_of_char_1 = pos_of_char(s[pos+1] );

			ret.push_back(static_cast<std::string::value_type>( ( (pos_of_char(s[pos+0]) ) << 2 ) + ( (pos_of_char_1 & 0x30 ) >> 4)));

			if (s[pos+2] != '=' && s[pos+2] != '.') { // accept URL-safe base 64 strings, too, so check for '.' also.

				unsigned int pos_of_char_2 = pos_of_char(s[pos+2] );
				ret.push_back(static_cast<std::string::value_type>( (( pos_of_char_1 & 0x0f) << 4) + (( pos_of_char_2 & 0x3c) >> 2)));

				if (s[pos+3] != '=' && s[pos+3] != '.') {
					ret.push_back(static_cast<std::string::value_type>( ( (pos_of_char_2 & 0x03 ) << 6 ) + pos_of_char(s[pos+3])   ));
				}
			}

			pos += 4;
		}

		return ret;
	}

	unsigned int Base64Decompressor::pos_of_char(const unsigned char chr)
	{
		//
		// Return the position of chr within base64_encode()
		//

		if      (chr >= 'A' && chr <= 'Z') return chr - 'A';
		else if (chr >= 'a' && chr <= 'z') return chr - 'a' + ('Z' - 'A')               + 1;
		else if (chr >= '0' && chr <= '9') return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
		else if (chr == '+' || chr == '-') return 62; // Be liberal with input and accept both url ('-') and non-url ('+') base 64 characters (
		else if (chr == '/' || chr == '_') return 63; // Ditto for '/' and '_'

		#ifdef __cpp_exceptions
throw "If input is correct, this line should never be reached.";
#else
return 0;
#endif
	}

	/*!
	 * UNUSED! Does nothing
	 * @param path
	 * @return
	 */
	std::string Base64Decompressor::decompressFile(const fs::path &)
	{
		return std::string();
	}

	/*!
	 * UNUSED! Does nothing
	 * @param path
	 * @return
	 */
	std::string Base64Decompressor::decompress(const void *, size_t)
	{
		return std::string();
	}
}

#endif //TILESON_BASE64DECOMPRESSOR_HPP

/*** End of inlined file: Base64Decompressor.hpp ***/


/*** Start of inlined file: Lzma.hpp ***/
//
// Created by robin on 16.01.2021.
//
//#include "../../extras/pocketlzma.hpp"
#ifdef POCKETLZMA_POCKETLZMA_H

#ifndef TILESON_LZMA_HPP
#define TILESON_LZMA_HPP

namespace tson
{
	class Lzma : public IDecompressor<std::vector<uint8_t>, std::vector<uint8_t>>
	{
		public:
			inline const std::string &name() const override
			{
				return NAME;
			}

			inline std::vector<uint8_t> decompress(const std::vector<uint8_t> &input) override
			{
				std::vector<uint8_t> out;

				plz::PocketLzma p;
				plz::StatusCode status = p.decompress(input, out);

				if(status != plz::StatusCode::Ok)
					return std::vector<uint8_t>();

				return out;
			}

			inline std::vector<uint8_t> decompressFile(const fs::path &path) override
			{
				std::vector<uint8_t> in;
				std::vector<uint8_t> out;

				plz::PocketLzma p;
				plz::FileStatus fileStatus = plz::File::FromFile(path.generic_string(), in);
				if(fileStatus.status() != plz::FileStatus::Code::Ok)
					return std::vector<uint8_t>();

				plz::StatusCode status = p.decompress(in, out);

				if(status != plz::StatusCode::Ok)
					return std::vector<uint8_t>();

				return out;
			}

			inline std::vector<uint8_t> decompress(const void *data, size_t size) override
			{
				std::vector<uint8_t> out;

				plz::PocketLzma p;
				plz::StatusCode status = p.decompress((uint8_t*) data, size, out);

				if(status != plz::StatusCode::Ok)
					return std::vector<uint8_t>();

				return out;
			}

		private:
			inline static const std::string NAME {"lzma"};
	};
}

#endif //TILESON_LZMA_HPP

#endif
/*** End of inlined file: Lzma.hpp ***/


/*** Start of inlined file: DecompressorContainer.hpp ***/
//
// Created by robin on 30.07.2020.
//

#ifndef TILESON_DECOMPRESSORCONTAINER_HPP
#define TILESON_DECOMPRESSORCONTAINER_HPP

#include <memory>
#include <vector>
#include <string_view>
#include <functional>
namespace tson
{
	class DecompressorContainer
	{
		public:
			inline DecompressorContainer() = default;
			template <typename T, typename... Args>
			inline void add(Args &&... args);
			inline void remove(std::string_view name);
			inline bool contains(std::string_view name) const;
			inline bool empty() const;
			inline size_t size() const;
			inline void clear();

			inline IDecompressor<std::string_view, std::string> *get(std::string_view name);
		private:
			//Key: name,
			std::vector<std::unique_ptr<IDecompressor<std::string_view, std::string>>> m_decompressors;
	};

	template<typename T, typename... Args>
	void DecompressorContainer::add(Args &&... args)
	{
		m_decompressors.emplace_back(new T(args...));
	}

	/*!
	 *
	 * @param name The name of the decompressor to check whether exists.
	 * @return Whether a decompressor with the given name exists or not.
	 */
	bool DecompressorContainer::contains(std::string_view name) const
	{
		auto iter = std::find_if(m_decompressors.begin(), m_decompressors.end(), [&](const auto &item)
		{
			return item->name() == name;
		});

		return iter != m_decompressors.end();
	}

	/*!
	 * Removed an element with the given name.
	 * @param name The name of the decompressor
	 */
	void DecompressorContainer::remove(std::string_view name)
	{
		auto iter = std::remove_if(m_decompressors.begin(), m_decompressors.end(), [&](const auto &item)
		{
			return item->name() == name;
		});
		m_decompressors.erase(iter);
	}

	size_t DecompressorContainer::size() const
	{
		return m_decompressors.size();
	}

	/*!
	 *
	 * @param name The name of the container
	 * @return An ICompressor pointer if it exists. nullptr otherwise.
	 */
	IDecompressor<std::string_view, std::string> *DecompressorContainer::get(std::string_view name)
	{
		auto iter = std::find_if(m_decompressors.begin(), m_decompressors.end(), [&](const auto &item)
		{
			return item->name() == name;
		});

		return (iter != m_decompressors.end()) ? iter->get() : nullptr;
	}

	/*!
	 * Check if container is empty
	 * @return Whether or not the container is empty
	 */
	bool DecompressorContainer::empty() const
	{
		return m_decompressors.empty();
	}

	/*!
	 * Clears all IDecompressor elements in the container
	 */
	void DecompressorContainer::clear()
	{
		m_decompressors.clear();
	}
}
#endif //TILESON_DECOMPRESSORCONTAINER_HPP

/*** End of inlined file: DecompressorContainer.hpp ***/


/*** Start of inlined file: MemoryStream.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_MEMORYSTREAM_HPP
#define TILESON_MEMORYSTREAM_HPP


/*** Start of inlined file: MemoryBuffer.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_MEMORYBUFFER_HPP
#define TILESON_MEMORYBUFFER_HPP

#include <iostream>

namespace tson
{
	class MemoryBuffer : public std::basic_streambuf<char> {
		public:
			MemoryBuffer(const uint8_t *p, size_t l) {
				setg((char*)p, (char*)p, (char*)p + l);
			}
	};
}

#endif //TILESON_MEMORYBUFFER_HPP

/*** End of inlined file: MemoryBuffer.hpp ***/

namespace tson
{
	class MemoryStream : public std::istream {
		public:
			MemoryStream(const uint8_t *p, size_t l) :
					std::istream(&m_buffer),
					m_buffer(p, l) {
				rdbuf(&m_buffer);
			}

		private:
			MemoryBuffer m_buffer;
	};
}

#endif //TILESON_MEMORYSTREAM_HPP

/*** End of inlined file: MemoryStream.hpp ***/


/*** Start of inlined file: Map.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_MAP_HPP
#define TILESON_MAP_HPP

#include <functional>


/*** Start of inlined file: Color.hpp ***/
//
// Created by robin on 09.08.2019.
//

#ifndef TILESON_COLOR_HPP
#define TILESON_COLOR_HPP

#include <type_traits>
#include <cstdint>
#include <string>

namespace tson
{

	template<typename T>
	class Color
	{

		public:
			/*!
			 * Parses color from Tiled's own color format, which is #aarrggbb in hex format or optionally #rrggbb.
			 * @param color Color in "#rrggbbaa" hex format.
			 * @example "#ffaa07ff" and "#aa07ff". In cases where alpha is not a value, it is set to 255.
			*/
			inline explicit Color(const std::string &color)
			{
				parseHexString(color);
			}
			inline Color(T red, T green, T blue, T alpha);
			inline Color() { r = g = b = 0; a = 255; }

			inline bool operator==(const Color &rhs) const;
			inline bool operator==(const std::string &rhs) const;
			inline bool operator!=(const Color &rhs) const;

			inline Color<float> asFloat();
			inline Color<uint8_t> asInt();

			/*! Red */
			T r;
			/*! Green */
			T g;
			/*! Blue */
			T b;
			/*! Alpha */
			T a;

		private:
			void parseHexString(const std::string &color)
			{
				if constexpr (std::is_same<T, float>::value)
				{
					if (color.size() == 9)
					{
						a = (float) std::stoi(color.substr(1, 2), nullptr, 16) / 255;
						r = (float) std::stoi(color.substr(3, 2), nullptr, 16) / 255;
						g = (float) std::stoi(color.substr(5, 2), nullptr, 16) / 255;
						b = (float) std::stoi(color.substr(7, 2), nullptr, 16) / 255;
					}
					else if (color.size() == 7)
					{
						r = (float) std::stoi(color.substr(1, 2), nullptr, 16) / 255;
						g = (float) std::stoi(color.substr(3, 2), nullptr, 16) / 255;
						b = (float) std::stoi(color.substr(5, 2), nullptr, 16) / 255;
						a = 1.f;
					}
				}
				else if constexpr (std::is_same<T, uint8_t>::value)
				{
					if (color.size() == 9)
					{
						a = static_cast<uint8_t>(std::stoi(color.substr(1, 2), nullptr, 16));
						r = static_cast<uint8_t>(std::stoi(color.substr(3, 2), nullptr, 16));
						g = static_cast<uint8_t>(std::stoi(color.substr(5, 2), nullptr, 16));
						b = static_cast<uint8_t>(std::stoi(color.substr(7, 2), nullptr, 16));
					}
					else if (color.size() == 7)
					{
						r = static_cast<uint8_t>(std::stoi(color.substr(1, 2), nullptr, 16));
						g = static_cast<uint8_t>(std::stoi(color.substr(3, 2), nullptr, 16));
						b = static_cast<uint8_t>(std::stoi(color.substr(5, 2), nullptr, 16));
						a = 255;
					}
				}
			}

	};

	typedef Color<uint8_t> Colori;
	typedef Color<float> Colorf;

	/*!
	 * Gets the Color as a float. Only useful if the template related to the current color is NOT float
	 * @tparam T The template type
	 * @return If the T type is float, the value will be returned as a copy of itself. Else: All values will be divided by 255
	 * before returning.
	 */
	template<typename T>
	tson::Colorf Color<T>::asFloat()
	{
		if constexpr (std::is_same<T, float>::value)
			*this;
		else
			return tson::Colorf((float) r / 255, (float) g / 255, (float) b / 255, (float) a / 255);
	}

	/*!
	 * Gets the Color as an 32-bit variable, where each channel is 8-bit.
	 * Only useful if the template related to the current color is NOT already 8-bit int
	 * @tparam T The template type
	 * @return If the T type is float, the value of each channel will be multiplied by 255. Else: The value will be returned as a copy of itself.
	 */
	template<typename T>
	tson::Colori Color<T>::asInt()
	{
		if constexpr (std::is_same<T, float>::value)
			return tson::Colori(static_cast<std::uint8_t>((float) r * 255),
							  static_cast<std::uint8_t>((float) g * 255),
							  static_cast<std::uint8_t>((float) b * 255),
							  static_cast<std::uint8_t>((float) a * 255));
		else
			*this;
	}

	/*!
	 * Create a new color in rgba (red, green, blue, alpha) format
	 * @tparam T the template type for each channel. Usually uint8_t (8-bit int) or float.
	 * @param red Red channel
	 * @param green Green channel
	 * @param blue Blue channel
	 * @param alpha Alpha channel
	 */
	template<typename T>
	Color<T>::Color(T red, T green, T blue, T alpha)
	{
		r = red;
		g = green;
		b = blue;
		a = alpha;
	}

	template<typename T>
	bool Color<T>::operator==(const std::string &rhs) const {
		Color other {rhs};
		return *this == other;
	}

	template<typename T>
	bool Color<T>::operator==(const Color &rhs) const
	{
		return r == rhs.r &&
			   g == rhs.g &&
			   b == rhs.b &&
			   a == rhs.a;
	}

	template<typename T>
	bool Color<T>::operator!=(const Color &rhs) const
	{
		return !(rhs == *this);
	}

}

#endif //TILESON_COLOR_HPP

/*** End of inlined file: Color.hpp ***/


/*** Start of inlined file: Vector2.hpp ***/
//
// Created by robin on 31.07.2019.
//

#ifndef TILESON_VECTOR2_HPP
#define TILESON_VECTOR2_HPP

namespace tson
{
	template<typename T>
	class Vector2
	{

		public:
			inline Vector2(T xPos, T yPos);
			inline Vector2() { x = y = 0; }

			inline bool operator==(const Vector2 &rhs) const;
			inline bool operator!=(const Vector2 &rhs) const;

			T x;
			T y;
	};

	/*!
	 *
	 * @tparam T template type
	 * @param xPos x-position
	 * @param yPos y-position
	 */
	template<typename T>
	Vector2<T>::Vector2(T xPos, T yPos)
	{
		x = xPos;
		y = yPos;
	}

	template<typename T>
	bool Vector2<T>::operator==(const Vector2 &rhs) const
	{
		return x == rhs.x &&
			   y == rhs.y;
	}

	template<typename T>
	bool Vector2<T>::operator!=(const Vector2 &rhs) const
	{
		return !(rhs == *this);
	}

	typedef Vector2<int> Vector2i;
	typedef Vector2<float> Vector2f;
}

#endif //TILESON_VECTOR2_HPP

/*** End of inlined file: Vector2.hpp ***/

//#include "../external/json.hpp"

/*** Start of inlined file: IJson.hpp ***/
//
// Created by robin on 06.01.2021.
//

#ifndef TILESON_IJSON_HPP
#define TILESON_IJSON_HPP

namespace tson
{
	class IJson
	{
		public:

			virtual IJson& operator[](std::string_view key) = 0;
			virtual IJson &at(std::string_view key) = 0;
			virtual IJson &at(size_t pos) = 0;
			/*!
			 * If current json object is an array, this will get all elements of it!
			 * @return An array
			 */
			[[nodiscard]] virtual std::vector<std::unique_ptr<IJson>> array() = 0;
			[[nodiscard]] virtual std::vector<std::unique_ptr<IJson>> &array(std::string_view key) = 0;
			/*!
			 * Get the size of an object. This will be equal to the number of
			 * variables an object contains.
			 * @return
			 */
			[[nodiscard]] virtual size_t size() const = 0;
			[[nodiscard]] virtual bool parse(const fs::path &path) = 0;
			[[nodiscard]] virtual bool parse(const void *data, size_t size) = 0;

			template <typename T>
			[[nodiscard]] T get(std::string_view key);
			template <typename T>
			[[nodiscard]] T get();
			[[nodiscard]] virtual size_t count(std::string_view key) const = 0;
			[[nodiscard]] virtual bool any(std::string_view key) const = 0;
			[[nodiscard]] virtual bool isArray() const = 0;
			[[nodiscard]] virtual bool isObject() const = 0;
			[[nodiscard]] virtual bool isNull() const = 0;

			/*!
			 * Get the directory where the json was loaded.
			 * Only assigned if json is parsed by file.
			 * @return
			 */
			[[nodiscard]] virtual fs::path directory() const = 0;
			virtual void directory(const fs::path &directory) = 0;

			/*!
			 * Create a new empty instance using the same engine
			*/
			 virtual std::unique_ptr<IJson> create() = 0;

			/*!
			 * Pure virtual class needs virtual destructor so derived classes can call their own destructors
			*/
			virtual ~IJson() = default;

		protected:
			[[nodiscard]] virtual int32_t getInt32(std::string_view key) = 0;
			[[nodiscard]] virtual uint32_t getUInt32(std::string_view key) = 0;
			[[nodiscard]] virtual int64_t getInt64(std::string_view key) = 0;
			[[nodiscard]] virtual uint64_t getUInt64(std::string_view key) = 0;
			[[nodiscard]] virtual double getDouble(std::string_view key) = 0;
			[[nodiscard]] virtual float getFloat(std::string_view key) = 0;
			[[nodiscard]] virtual std::string getString(std::string_view key) = 0;
			[[nodiscard]] virtual bool getBool(std::string_view key) = 0;

			[[nodiscard]] virtual int32_t getInt32() = 0;
			[[nodiscard]] virtual uint32_t getUInt32() = 0;
			[[nodiscard]] virtual int64_t getInt64() = 0;
			[[nodiscard]] virtual uint64_t getUInt64() = 0;
			[[nodiscard]] virtual double getDouble() = 0;
			[[nodiscard]] virtual float getFloat() = 0;
			[[nodiscard]] virtual std::string getString() = 0;
			[[nodiscard]] virtual bool getBool() = 0;
	};

	template<typename T>
	T IJson::get(std::string_view key)
	{
		if constexpr (std::is_same<T, double>::value)
			return getDouble(key);
		if constexpr (std::is_same<T, float>::value)
			return getFloat(key);
		else if constexpr (std::is_same<T, int32_t>::value)
			return getInt32(key);
		else if constexpr (std::is_same<T, uint32_t>::value)
			return getUInt32(key);
		else if constexpr (std::is_same<T, int64_t>::value)
			return getInt64(key);
		else if constexpr (std::is_same<T, uint64_t>::value)
			return getUInt64(key);
		else if constexpr (std::is_same<T, std::string>::value)
			return getString(key);
		else if constexpr (std::is_same<T, bool>::value)
			return getBool(key);
		else
			return nullptr;
	}

	template<typename T>
	T IJson::get()
	{
		if constexpr (std::is_same<T, double>::value)
			return getDouble();
		if constexpr (std::is_same<T, float>::value)
			return getFloat();
		else if constexpr (std::is_same<T, int32_t>::value)
			return getInt32();
		else if constexpr (std::is_same<T, uint32_t>::value)
			return getUInt32();
		else if constexpr (std::is_same<T, int64_t>::value)
			return getInt64();
		else if constexpr (std::is_same<T, uint64_t>::value)
			return getUInt64();
		else if constexpr (std::is_same<T, std::string>::value)
			return getString();
		else if constexpr (std::is_same<T, bool>::value)
			return getBool();
		else
			return nullptr;
	}

}

#endif //TILESON_IJSON_HPP

/*** End of inlined file: IJson.hpp ***/



/*** Start of inlined file: NlohmannJson.hpp ***/
//
// Created by robin on 08.01.2021.
//

#ifdef INCLUDE_NLOHMANN_JSON_HPP_

#ifndef TILESON_NLOHMANNJSON_HPP
#define TILESON_NLOHMANNJSON_HPP

#include <memory>

namespace tson
{
	class NlohmannJson : public tson::IJson
	{
		public:
			inline NlohmannJson() = default;

			IJson &operator[](std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
					m_arrayCache[key.data()] = std::make_unique<NlohmannJson>(&m_json->operator[](key.data()));//.front());

				return *m_arrayCache[key.data()].get();
			}

			inline explicit NlohmannJson(nlohmann::json *json) : m_json {json}
			{

			}

			inline IJson& at(std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
					m_arrayCache[key.data()] = std::make_unique<NlohmannJson>(&m_json->operator[](key.data()));//.front());

				return *m_arrayCache[key.data()].get();
			}

			inline IJson& at(size_t pos) override
			{
				if(m_arrayPosCache.count(pos) == 0)
					m_arrayPosCache[pos] = std::make_unique<NlohmannJson>(&m_json->at(pos));

				return *m_arrayPosCache[pos];
			}

			std::vector<std::unique_ptr<IJson>> array() override
			{
				std::vector<std::unique_ptr<IJson>> vec;
				for(auto &item : *m_json)
				{
					nlohmann::json *ptr = &item;
					vec.emplace_back(std::make_unique<NlohmannJson>(ptr));
				}

				return vec;
			}

			inline std::vector<std::unique_ptr<IJson>> &array(std::string_view key) override
			{
				if(m_arrayListDataCache.count(key.data()) == 0)
				{
					if (m_json->count(key.data()) > 0 && m_json->operator[](key.data()).is_array())
					{
						std::for_each(m_json->operator[](key.data()).begin(), m_json->operator[](key.data()).end(), [&](nlohmann::json &item)
						{
							nlohmann::json *ptr = &item;
							m_arrayListDataCache[key.data()].emplace_back(std::make_unique<NlohmannJson>(ptr));
						});
					}
				}

				return m_arrayListDataCache[key.data()];
			}

			[[nodiscard]] inline size_t size() const override
			{
				return m_json->size();
			}

			inline bool parse(const fs::path &path) override
			{
				clearCache();
				m_data = nullptr;
				m_json = nullptr;
				if (fs::exists(path) && fs::is_regular_file(path))
				{
					m_path = path.parent_path();
					m_data = std::make_unique<nlohmann::json>();
					std::ifstream i(path.generic_string());
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
					{
						i >> *m_data;
						m_json = m_data.get();
					}
					catch (const nlohmann::json::parse_error &error)
					{
						std::string message = "Parse error: ";
						message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
						message += std::string("\n");
						std::cerr << message;
						return false;
					}
					return true;
				}
				return false;
			}

			inline bool parse(const void *data, size_t size) override
			{
				clearCache();
				m_json = nullptr;
				m_data = std::make_unique<nlohmann::json>();
				tson::MemoryStream mem{(uint8_t *) data, size};
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
				{
					mem >> *m_data;
					m_json = m_data.get();
				}
				catch (const nlohmann::json::parse_error &error)
				{
					std::string message = "Parse error: ";
					message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
					message += std::string("\n");
					std::cerr << message;
					return false;
				}
				return true;
			}

			[[nodiscard]] inline size_t count(std::string_view key) const override
			{
				return m_json->count(key);
			}

			[[nodiscard]] inline bool any(std::string_view key) const override
			{
				return count(key) > 0;
			}

			[[nodiscard]] inline bool isArray() const override
			{
				return m_json->is_array();
			}

			[[nodiscard]] inline bool isObject() const override
			{
				return m_json->is_object();
			}

			[[nodiscard]] inline bool isNull() const override
			{
				return m_json->is_null();
			}

			fs::path directory() const override
			{
				return m_path;
			}

			void directory(const fs::path &directory) override
			{
				m_path = directory;
			}

			std::unique_ptr<IJson> create() override
			{
				return std::make_unique<NlohmannJson>();
			}

		protected:
			[[nodiscard]] inline int32_t getInt32(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<int32_t>();
			}

			[[nodiscard]] inline uint32_t getUInt32(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<uint32_t>();
			}

			[[nodiscard]] inline int64_t getInt64(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<int64_t>();
			}

			[[nodiscard]] inline uint64_t getUInt64(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<uint64_t>();
			}

			[[nodiscard]] inline double getDouble(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<double>();
			}

			[[nodiscard]] inline std::string getString(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<std::string>();
			}

			[[nodiscard]] inline bool getBool(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<bool>();
			}

			[[nodiscard]] float getFloat(std::string_view key) override
			{
				return m_json->operator[](key.data()).get<float>();
			}

			[[nodiscard]] inline int32_t getInt32() override
			{
				return m_json->get<int32_t>();
			}

			[[nodiscard]] inline uint32_t getUInt32() override
			{
				return m_json->get<uint32_t>();
			}

			[[nodiscard]] inline int64_t getInt64() override
			{
				return m_json->get<int64_t>();
			}

			[[nodiscard]] inline uint64_t getUInt64() override
			{
				return m_json->get<uint64_t>();
			}

			[[nodiscard]] inline double getDouble() override
			{
				return m_json->get<double>();
			}

			[[nodiscard]] inline std::string getString() override
			{
				return m_json->get<std::string>();
			}

			[[nodiscard]] inline bool getBool() override
			{
				return m_json->get<bool>();
			}

			[[nodiscard]] float getFloat() override
			{
				return m_json->get<float>();
			}

		private:
			inline void clearCache()
			{
				m_arrayCache.clear();
				m_arrayPosCache.clear();
				m_arrayListDataCache.clear();
			}

			nlohmann::json *m_json = nullptr;
			std::unique_ptr<nlohmann::json> m_data = nullptr; //Only used if this is the owner json!
			fs::path m_path;

			//Cache!
			std::map<std::string, std::unique_ptr<IJson>> m_arrayCache;
			std::map<size_t, std::unique_ptr<IJson>> m_arrayPosCache;
			std::map<std::string, std::vector<std::unique_ptr<IJson>>> m_arrayListDataCache;

	};
}
#endif //TILESON_NLOHMANNJSON_HPP

#endif //INCLUDE_NLOHMANN_JSON_HPP_
/*** End of inlined file: NlohmannJson.hpp ***/


/*** Start of inlined file: PicoJson.hpp ***/
//
// Created by robin on 11.01.2021.
//

#ifdef picojson_h
#ifndef TILESON_PICOJSON_HPP
#define TILESON_PICOJSON_HPP

namespace tson
{
	class PicoJson : public tson::IJson
	{
		public:
			inline PicoJson() = default;

			IJson &operator[](std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
				{
					if(m_json->is<picojson::object>())
					{
						picojson::object &o = m_json->get<picojson::object>();
						m_arrayCache[key.data()] = std::make_unique<PicoJson>(&o[key.data()]);
					}
				}

				return *m_arrayCache[key.data()].get();
			}

			inline explicit PicoJson(picojson::value *json) : m_json {json}
			{

			}

			inline IJson& at(std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
				{
					if(m_json->is<picojson::object>())
					{
						picojson::object &o = m_json->get<picojson::object>();
						m_arrayCache[key.data()] = std::make_unique<PicoJson>(&o[key.data()]);
					}
				}
				return *m_arrayCache[key.data()].get();
			}

			inline IJson& at(size_t pos) override
			{
				if(m_arrayPosCache.count(pos) == 0)
				{
					picojson::array &a = m_json->get<picojson::array>();
					m_arrayPosCache[pos] = std::make_unique<PicoJson>(&a.at(pos));
				}

				return *m_arrayPosCache[pos];
			}

			std::vector<std::unique_ptr<IJson>> array() override
			{
				std::vector<std::unique_ptr<IJson>> vec;
				if(m_json->is<picojson::array>())
				{
					picojson::array &a = m_json->get<picojson::array>();
					for (auto &item : a)
					{
						picojson::value *ptr = &item;
						vec.emplace_back(std::make_unique<PicoJson>(ptr));
					}
				}

				return vec;
			}

			inline std::vector<std::unique_ptr<IJson>> &array(std::string_view key) override
			{
				if(m_arrayListDataCache.count(key.data()) == 0)
				{
					if(count(key.data()) > 0)
					{
						if (isObject())
						{
							picojson::object &obj = m_json->get<picojson::object>();
							picojson::value &v = obj.at(key.data());
							bool isArray = v.is<picojson::array>();
							if (isArray)
							{
								picojson::array &a = v.get<picojson::array>();

								std::for_each(a.begin(), a.end(), [&](picojson::value &item)
								{
									picojson::value *ptr = &item;
									m_arrayListDataCache[key.data()].emplace_back(std::make_unique<PicoJson>(ptr));
								});
							}
						}
					}
				}

				return m_arrayListDataCache[key.data()];
			}

			[[nodiscard]] inline size_t size() const override
			{
				if (m_json->is<picojson::object>())
				{
					picojson::object obj = m_json->get<picojson::object>();
					return obj.size();
				}
				return 0;
			}

			inline bool parse(const fs::path &path) override
			{
				clearCache();
				m_data = nullptr;
				m_json = nullptr;
				if (fs::exists(path) && fs::is_regular_file(path))
				{
					m_path = path.parent_path();
					m_data = std::make_unique<picojson::value>();
					std::ifstream i(path.generic_string());
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
					{
						std::string error = picojson::parse(*m_data, i);
						if(!error.empty())
						{
							std::cerr << "PicoJson parse error: " << error << "\n";
							return false;
						}
						//i >> *m_data;
						m_json = m_data.get();
					}

#ifdef __cpp_exceptions
catch(const std::exception &error)
#else
else if(false)
#endif
{
						std::string message = "Parse error: ";
						message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
						message += std::string("\n");
						std::cerr << message;
						return false;
					}
					return true;
				}
				return false;
			}

			inline bool parse(const void *data, size_t size) override
			{
				clearCache();
				m_json = nullptr;
				m_data = std::make_unique<picojson::value>();
				tson::MemoryStream mem{(uint8_t *) data, size};
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
				{
					std::string error = picojson::parse(*m_data, mem);
					if(!error.empty())
					{
						std::cerr << "PicoJson parse error: " << error << "\n";
						return false;
					}
					//mem >> *m_data;
					m_json = m_data.get();
				}

#ifdef __cpp_exceptions
catch(const std::exception &error)
#else
else if(false)
#endif
{
					std::string message = "Parse error: ";
					message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
					message += std::string("\n");
					std::cerr << message;
					return false;
				}
				return true;
			}

			[[nodiscard]] inline size_t count(std::string_view key) const override
			{
				if (isObject())
				{
					picojson::object obj = m_json->get<picojson::object>();
					return obj.count(key.data());
				}

				return m_json->contains(key.data()) ? 1 : 0;
			}

			[[nodiscard]] inline bool any(std::string_view key) const override
			{
				return count(key) > 0;
			}

			[[nodiscard]] inline bool isArray() const override
			{
				return m_json->is<picojson::array>();
			}

			[[nodiscard]] inline bool isObject() const override
			{
				return m_json->is<picojson::object>();
			}

			[[nodiscard]] inline bool isNull() const override
			{
				return m_json->is<picojson::null>();
			}

			fs::path directory() const override
			{
				return m_path;
			}

			void directory(const fs::path &directory) override
			{
				m_path = directory;
			}
			std::unique_ptr<IJson> create() override
			{
				return std::make_unique<PicoJson>();
			}

		protected:
			[[nodiscard]] inline int32_t getInt32(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return static_cast<int32_t>(getDouble(key));
			}

			[[nodiscard]] inline uint32_t getUInt32(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return static_cast<uint32_t>(getDouble(key));
			}

			[[nodiscard]] inline int64_t getInt64(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return static_cast<int64_t>(getDouble(key));
			}

			[[nodiscard]] inline uint64_t getUInt64(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return static_cast<uint64_t>(getDouble(key));
			}

			[[nodiscard]] inline double getDouble(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return obj[key.data()].get<double>();
			}

			[[nodiscard]] inline std::string getString(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return obj[key.data()].get<std::string>();
			}

			[[nodiscard]] inline bool getBool(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return obj[key.data()].get<bool>();
			}

			[[nodiscard]] float getFloat(std::string_view key) override
			{
				picojson::object obj = m_json->get<picojson::object>();
				return static_cast<float>(getDouble(key));
			}

			[[nodiscard]] inline int32_t getInt32() override
			{
				return static_cast<int32_t>(getDouble());
			}

			[[nodiscard]] inline uint32_t getUInt32() override
			{
				return static_cast<uint32_t>(getDouble());
			}

			[[nodiscard]] inline int64_t getInt64() override
			{
				return static_cast<int64_t>(getDouble());
			}

			[[nodiscard]] inline uint64_t getUInt64() override
			{
				return static_cast<uint64_t>(getDouble());
			}

			[[nodiscard]] inline double getDouble() override
			{
				return m_json->get<double>();
			}

			[[nodiscard]] inline std::string getString() override
			{
				return m_json->get<std::string>();
			}

			[[nodiscard]] inline bool getBool() override
			{
				return m_json->get<bool>();
			}

			[[nodiscard]] float getFloat() override
			{
				return static_cast<float>(getDouble());
			}

		private:
			inline void clearCache()
			{
				m_arrayCache.clear();
				m_arrayPosCache.clear();
				m_arrayListDataCache.clear();
			}

			picojson::value *m_json = nullptr;
			std::unique_ptr<picojson::value> m_data = nullptr; //Only used if this is the owner json!
			fs::path m_path;

			//Cache!
			std::map<std::string, std::unique_ptr<IJson>> m_arrayCache;
			std::map<size_t, std::unique_ptr<IJson>> m_arrayPosCache;
			std::map<std::string, std::vector<std::unique_ptr<IJson>>> m_arrayListDataCache;

	};
}
#endif //TILESON_PICOJSON_HPP
#endif

/*** End of inlined file: PicoJson.hpp ***/

//#include "../json/Gason.hpp" //Unsupported

/*** Start of inlined file: Json11.hpp ***/
//
// Created by robin on 16.01.2021.
//

#ifndef TILESON_JSON11_HPP
#define TILESON_JSON11_HPP

namespace tson
{
	class Json11 : public tson::IJson
	{
		public:
			inline Json11() = default;

			IJson &operator[](std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
				{
					if(m_json->is_object())
					{
						m_arrayCache[key.data()] = std::make_unique<Json11>(m_json->operator[](key.data()));
					}
				}

				return *m_arrayCache[key.data()].get();
			}

			inline explicit Json11(const json11::Json &json) : m_json {&json}
			{

			}

			inline IJson& at(std::string_view key) override
			{
				if(m_arrayCache.count(key.data()) == 0)
				{
					if(m_json->is_object())
					{
						m_arrayCache[key.data()] = std::make_unique<Json11>(m_json->operator[](key.data()));
					}
				}
				return *m_arrayCache[key.data()].get();
			}

			inline IJson& at(size_t pos) override
			{
				if(m_arrayPosCache.count(pos) == 0)
				{
					const std::vector<json11::Json> &a = m_json->array_items();
					m_arrayPosCache[pos] = std::make_unique<Json11>(a.at(pos));
				}

				return *m_arrayPosCache[pos];
			}

			std::vector<std::unique_ptr<IJson>> array() override
			{
				std::vector<std::unique_ptr<IJson>> vec;
				if(m_json->is_array())
				{
					for (const json11::Json &item : m_json->array_items())
					{
						vec.emplace_back(std::make_unique<Json11>(item));
					}
				}

				return vec;
			}

			inline std::vector<std::unique_ptr<IJson>> &array(std::string_view key) override
			{
				if(m_arrayListDataCache.count(key.data()) == 0)
				{
					if(count(key.data()) > 0)
					{
						if(isObject())
						{
							const json11::Json &v = m_json->operator[](key.data());
							if(v.is_array())
							{
								for (const json11::Json &item : v.array_items())
								{
									m_arrayListDataCache[key.data()].emplace_back(std::make_unique<Json11>(item));
								}
							}
						}
					}
				}

				return m_arrayListDataCache[key.data()];
			}

			[[nodiscard]] inline size_t size() const override
			{
				if(m_json->is_object())
					return m_json->object_items().size();
				else if(m_json->is_array())
					return m_json->array_items().size();

				return 0;
			}

			inline bool parse(const fs::path &path) override
			{
				clearCache();
				m_data = nullptr;
				m_json = nullptr;
				if (fs::exists(path) && fs::is_regular_file(path))
				{
					std::ifstream file(path.generic_string());
					std::string str;
					m_path = path.parent_path();

					file.seekg(0, std::ios::end);
					str.reserve(static_cast<size_t>(file.tellg()));
					file.seekg(0, std::ios::beg);

					str.assign((std::istreambuf_iterator<char>(file)),
							   std::istreambuf_iterator<char>());

					m_data = std::make_unique<json11::Json>();
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
					{
						std::string strError;
						*m_data = json11::Json::parse(str, strError);
						if(!strError.empty())
						{
							std::cerr << strError << "\n";
							return false;
						}
						m_json = m_data.get();
					}

#ifdef __cpp_exceptions
catch(const std::exception &error)
#else
else if(false)
#endif
{
						std::string message = "Json11 parse error: ";
						message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
						message += std::string("\n");
						std::cerr << message;
						return false;
					}
					return true;
				}
				return false;
			}

			inline bool parse(const void *data, size_t size) override
			{
				clearCache();
				m_json = nullptr;
				std::string str;

				str.reserve(size);

				tson::MemoryStream mem{(uint8_t *) data, size};

				str.assign((std::istreambuf_iterator<char>(mem)),
						   std::istreambuf_iterator<char>());

				m_data = std::make_unique<json11::Json>();
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
				{
					std::string strError;

					*m_data = json11::Json::parse(str, strError);
					if(!strError.empty())
					{
						std::cout << strError << "\n";
						return false;
					}
					m_json = m_data.get();
				}

#ifdef __cpp_exceptions
catch(const std::exception &error)
#else
else if(false)
#endif
{
					std::string message = "Json11 parse error: ";
					message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
					message += std::string("\n");
					std::cerr << message;
					return false;
				}
				return true;
			}

			[[nodiscard]] inline size_t count(std::string_view key) const override
			{
				if (isObject())
				{
					//const json11::Json &j = m_json->operator[](key.data());
					//size_t s1 = j.object_items().size();
					return m_json->object_items().count(key.data());
				}

				return 0;
			}

			[[nodiscard]] inline bool any(std::string_view key) const override
			{
				return count(key) > 0;
			}

			[[nodiscard]] inline bool isArray() const override
			{
				return m_json->is_array();
			}

			[[nodiscard]] inline bool isObject() const override
			{
				return m_json->is_object();
			}

			[[nodiscard]] inline bool isNull() const override
			{
				return m_json->is_null();
			}

			fs::path directory() const override
			{
				return m_path;
			}

			void directory(const fs::path &directory) override
			{
				m_path = directory;
			}

			std::unique_ptr<IJson> create() override
			{
				return std::make_unique<Json11>();
			}
		protected:
			[[nodiscard]] inline int32_t getInt32(std::string_view key) override
			{
				return static_cast<int32_t>(getDouble(key));
			}

			[[nodiscard]] inline uint32_t getUInt32(std::string_view key) override
			{
				return static_cast<uint32_t>(getDouble(key));
			}

			[[nodiscard]] inline int64_t getInt64(std::string_view key) override
			{
				return static_cast<int64_t>(getDouble(key));
			}

			[[nodiscard]] inline uint64_t getUInt64(std::string_view key) override
			{
				return static_cast<uint64_t>(getDouble(key));
			}

			[[nodiscard]] inline double getDouble(std::string_view key) override
			{
				return m_json->operator[](key.data()).number_value();
			}

			[[nodiscard]] inline std::string getString(std::string_view key) override
			{
				return m_json->operator[](key.data()).string_value(); // .get<std::string>();
			}

			[[nodiscard]] inline bool getBool(std::string_view key) override
			{
				return m_json->operator[](key.data()).bool_value();
			}

			[[nodiscard]] float getFloat(std::string_view key) override
			{
				return static_cast<float>(getDouble(key));
			}

			[[nodiscard]] inline int32_t getInt32() override
			{
				return static_cast<int32_t>(getDouble());
			}

			[[nodiscard]] inline uint32_t getUInt32() override
			{
				return static_cast<uint32_t>(getDouble());
			}

			[[nodiscard]] inline int64_t getInt64() override
			{
				return static_cast<int64_t>(getDouble());
			}

			[[nodiscard]] inline uint64_t getUInt64() override
			{
				return static_cast<uint64_t>(getDouble());
			}

			[[nodiscard]] inline double getDouble() override
			{
				return m_json->number_value();
			}

			[[nodiscard]] inline std::string getString() override
			{
				return m_json->string_value();
			}

			[[nodiscard]] inline bool getBool() override
			{
				return m_json->bool_value();
			}

			[[nodiscard]] float getFloat() override
			{
				return static_cast<float>(getDouble());
			}

		private:

			inline void clearCache()
			{
				m_arrayCache.clear();
				m_arrayPosCache.clear();
				m_arrayListDataCache.clear();
			}

			//Owner values
			std::unique_ptr<json11::Json> m_data = nullptr; //Only used if this is the owner json!

			const json11::Json *m_json = nullptr;
			fs::path m_path;

			//Cache!
			std::map<std::string, std::unique_ptr<IJson>> m_arrayCache;
			std::map<size_t, std::unique_ptr<IJson>> m_arrayPosCache;
			std::map<std::string, std::vector<std::unique_ptr<IJson>>> m_arrayListDataCache;

	};
}

#endif //TILESON_JSON11_HPP

/*** End of inlined file: Json11.hpp ***/



/*** Start of inlined file: Layer.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_LAYER_HPP
#define TILESON_LAYER_HPP

#include <set>
//#include "../external/json.hpp"


/*** Start of inlined file: Chunk.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_CHUNK_HPP
#define TILESON_CHUNK_HPP

//#include "../external/json.hpp"

namespace tson
{
	class Chunk
	{
		public:
			inline Chunk() = default;
			inline explicit Chunk(IJson &json);
			inline bool parse(IJson &json);

			[[nodiscard]] inline const std::vector<int> &getData() const;
			[[nodiscard]] inline const std::string &getBase64Data() const;
			[[nodiscard]] inline const Vector2i &getSize() const;
			[[nodiscard]] inline const Vector2i &getPosition() const;

		private:
			std::vector<int> m_data;        /*! 'data' (when uint array): Array of unsigned int (GIDs) or base64-encoded data. tilelayer only. */
			std::string      m_base64Data;  /*! 'data' (when string): Array of unsigned int (GIDs) or base64-encoded data. */
			tson::Vector2i   m_size;        /*!  x='width' (in tiles) and y='height' (in tiles): */
			tson::Vector2i   m_position;    /*! 'x' and 'y' position in tiles */
	};
}

#endif //TILESON_CHUNK_HPP

/*!
 * Parses 'chunk' data from Tiled json and stores the values in this class
 * @param json json-data
 */
tson::Chunk::Chunk(IJson &json)
{
	parse(json);
}

/*!
 * Parses 'chunk' data from Tiled json and stores the values in this class
 * @param json json-data
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Chunk::parse(IJson &json)
{
	bool allFound = true;

	if(json.count("width") > 0 && json.count("height") > 0)
		m_size = {json["width"].get<int>(), json["height"].get<int>()}; else allFound = false;
	if(json.count("x") > 0 && json.count("y") > 0)
		m_position = {json["x"].get<int>(), json["y"].get<int>()}; else allFound = false;

	//Handle DATA (Optional)
	if(json.count("data") > 0)
	{
		if(json["data"].isArray())
		{
			auto &data = json.array("data");
			std::for_each(data.begin(), data.end(), [&](std::unique_ptr<IJson> &item) { m_data.push_back(item->get<int>()); });
		}
		else
			m_base64Data = json["data"].get<std::string>();
	}

	return allFound;
}

/*!
 * 'data' (when uint array): Array of unsigned int (GIDs) or base64-encoded data. tilelayer only.
 * @return list of tile ids
 */
const std::vector<int> &tson::Chunk::getData() const
{
	return m_data;
}

/*!
 * 'data' (when string): Array of unsigned int (GIDs) or base64-encoded data.
 * @return base64 string
 */
const std::string &tson::Chunk::getBase64Data() const
{
	return m_base64Data;
}

/*!
 * x='width' (in tiles) and y='height' (in tiles).
 * @return Size (x and y), containing the values from the fields 'width' and 'height' in Tiled
 */
const tson::Vector2i &tson::Chunk::getSize() const
{
	return m_size;
}

/*!
 * 'x' and 'y' position in tiles
 * @return Position in int
 */
const tson::Vector2i &tson::Chunk::getPosition() const
{
	return m_position;
}
/*** End of inlined file: Chunk.hpp ***/


/*** Start of inlined file: Object.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_OBJECT_HPP
#define TILESON_OBJECT_HPP

//#include "../external/json.hpp"


/*** Start of inlined file: PropertyCollection.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_PROPERTYCOLLECTION_HPP
#define TILESON_PROPERTYCOLLECTION_HPP


/*** Start of inlined file: Property.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_PROPERTY_HPP
#define TILESON_PROPERTY_HPP

//#include "../../TilesonConfig.h"

//#if USE_CPP17_FILESYSTEM

#include <any>
#include <string>

/*** Start of inlined file: Enums.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_ENUMS_HPP
#define TILESON_ENUMS_HPP
#include <cstdint>

/*** Start of inlined file: EnumBitflags.hpp ***/
//
// Created by robin on 08.11.2020.
//

#ifndef TILESON_ENUMBITFLAGS_HPP
#define TILESON_ENUMBITFLAGS_HPP

#include <type_traits>
#include <iostream>

#define TILESON_ENABLE_BITMASK_OPERATORS(x)  \
namespace tson {                         \
	ENABLE_BITMASK_OPERATORS(x) \
}

namespace tson
{
	#define ENABLE_BITMASK_OPERATORS(x)  \
	template<>                           \
	struct EnableBitMaskOperators<x>     \
	{                                    \
		static const bool enable = true; \
	};

	template<typename Enum>
	struct EnableBitMaskOperators
	{
		static const bool enable = false;
	};

	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	operator |(Enum lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		return static_cast<Enum> (
				static_cast<underlying>(lhs) |
				static_cast<underlying>(rhs)
		);
	}

	//Permissions operator &(Permissions lhs, Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	operator &(Enum lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		return static_cast<Enum> (
				static_cast<underlying>(lhs) &
				static_cast<underlying>(rhs)
		);
	}

	//Permissions operator ^(Permissions lhs, Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	operator ^(Enum lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		return static_cast<Enum> (
				static_cast<underlying>(lhs) ^
				static_cast<underlying>(rhs)
		);
	}

	//Permissions operator ~(Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	operator ~(Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		return static_cast<Enum> (
				~static_cast<underlying>(rhs)
		);
	}

	//Permissions& operator |=(Permissions &lhs, Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	&operator |=(Enum &lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		lhs = static_cast<Enum> (
				static_cast<underlying>(lhs) |
				static_cast<underlying>(rhs)
		);

		return lhs;
	}

	//Permissions& operator &=(Permissions &lhs, Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	&operator &=(Enum &lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		lhs = static_cast<Enum> (
				static_cast<underlying>(lhs) &
				static_cast<underlying>(rhs)
		);

		return lhs;
	}

	//Permissions& operator ^=(Permissions &lhs, Permissions rhs)
	template<typename Enum>
	typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type
	&operator ^=(Enum &lhs, Enum rhs)
	{
		static_assert(std::is_enum<Enum>::value,
					  "template parameter is not an enum type");

		using underlying = typename std::underlying_type<Enum>::type;

		lhs = static_cast<Enum> (
				static_cast<underlying>(lhs) ^
				static_cast<underlying>(rhs)
		);

		return lhs;
	}
}

#endif //TILESON_ENUMBITFLAGS_HPP

/*** End of inlined file: EnumBitflags.hpp ***/


namespace tson
{
	/*!
	 * Type used in Property.hpp
	 */
	enum class Type : uint8_t
	{
			Undefined = 0,
			Color = 1, /*! color */
			File = 2, /*! file */
			Int = 3, /*! int */
			Boolean = 4, /*! bool */
			Float = 5, /*! float */
			String = 6, /*! string */
			Class = 7, /*! class */
			Enum = 8,  /*! 'string' or 'int' with a value in 'propertyType' */
			Object = 9 /*! object */
	};

	/*!
	 * Layer.hpp - LayerType
	 * //'type': tilelayer, objectgroup, imagelayer or group
	 */
	enum class LayerType : uint8_t
	{
			Undefined = 0,
			TileLayer = 1,
			ObjectGroup = 2,
			ImageLayer = 3,
			Group = 4
	};

	/*!
	 * Tileset.hpp - TilesetType
	 * //'type': imagetileset, imagecollectiontileset
	 */
	enum class TilesetType : uint8_t
	{
		Undefined = 0,
		ImageTileset = 1,
		ImageCollectionTileset = 2
	};

	/*!
	 * Map.hpp - ParseStatus
	 */
	enum class ParseStatus : uint8_t
	{
			OK = 0, //OK unless otherwise stated
			FileNotFound = 1,
			ParseError = 2,
			MissingData = 3,
			DecompressionError = 4
	};

	/*!
	 * Object.hpp - ObjectType
	 */
	enum class ObjectType : uint8_t
	{
			Undefined = 0,
			Object = 1,
			Ellipse = 2,
			Rectangle = 3,
			Point = 4,
			Polygon = 5,
			Polyline = 6,
			Text = 7,
			Template = 8
	};

	static constexpr uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
	static constexpr uint32_t FLIPPED_VERTICALLY_FLAG   = 0x40000000;
	static constexpr uint32_t FLIPPED_DIAGONALLY_FLAG   = 0x20000000;
	/*!
	 * Object.hpp - ObjectFlipFlags
	 */
	enum class TileFlipFlags : uint32_t
	{
			None = 0,
			Diagonally = FLIPPED_DIAGONALLY_FLAG,
			Vertically = FLIPPED_VERTICALLY_FLAG,
			Horizontally = FLIPPED_HORIZONTALLY_FLAG
	};

	/*!
	 * Tileset.hpp - ObjectAlignment
	 */
	enum class ObjectAlignment : uint8_t
	{
			Unspecified = 0,    //unspecified
			TopLeft = 1,        //topleft
			Top = 2,            //top
			TopRight = 3,       //topright
			Left = 4,           //left
			Center = 5,         //center
			Right = 6,          //right
			BottomLeft = 7,     //bottomleft
			Bottom = 8,         //bottom
			BottomRight = 9     //bottomright
	};

	/*!
	 * Text.hpp - TextAlignment
	 */
	enum class TextAlignment : uint8_t
	{
		Unresolved = 0,
		Left = 1,           //left
		Center = 2,         //center
		Right = 3,          //right
		Justify = 4,        //justify
		Top = 5,            //top
		Bottom = 6          //bottom
	};

	/*!
	 * Tileset.hpp - TileRenderSize - From Tiled v1.9
	 */
	enum class TileRenderSize : uint8_t
	{
		Undefined = 0,
		Tile = 1,  //tile (default)
		Grid = 2   //grid
	};

	/*!
	 * Tileset.hpp - FillMode - From Tiled v1.9
	 */
	enum class FillMode : uint8_t
	{
		Undefined = 0,
		Stretch = 1,            //stretch (default)
		PreserveAspectFit = 2   //preserve-aspect-fit
	};

	enum class EnumStorageType : uint8_t
	{
		Unspecified = 0,
		Int = 1,
		String = 2
	};

	ENABLE_BITMASK_OPERATORS(TileFlipFlags)
}

#endif //TILESON_ENUMS_HPP

/*** End of inlined file: Enums.hpp ***/


//#include "../external/json.hpp"

namespace tson
{
	class Project;

	class Property
	{
		public:
			inline Property();
			inline explicit Property(IJson &json, tson::Project *project = nullptr);
			inline Property(std::string name, std::any value, Type type);

			inline void setValue(const std::any &value);
			inline void setStrValue(const std::string &value);
			inline void setName(const std::string &name);

			[[nodiscard]] inline const std::type_info& getValueType() const;
			inline std::string getValueTypeInfo();
			[[nodiscard]]inline const std::any &getValue() const;
			template <typename T>
			inline T getValue() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline Type getType() const;
			[[nodiscard]] inline const std::string &getPropertyType() const;

			//Became public in v1.4.0
			inline void setValueByType(IJson &json); //Definition in tileson_forward.hpp

		protected:
			inline void setTypeByString(const std::string &str);

			tson::Project *m_project = nullptr; //Used for resolving 'enum' and 'class' objects
			Type m_type = Type::Undefined;
			std::string m_name {};
			std::string m_propertyType {};
			std::any m_value; //Using std::any to assign any type
	};

	template<typename T>
	T Property::getValue() const
	{
		bool isCorrectType = (m_value.type() == typeid(T));

		if(isCorrectType)
		{
			T value = std::any_cast<T>(m_value);
			return value;
		}
		else
		{
			static T defaultValue;
			return defaultValue;
		}
	}
}

tson::Property::Property() : m_name {"unnamed"}
{

}

tson::Property::Property(IJson &json, tson::Project *project) : m_project {project}
{
	m_name = json["name"].get<std::string>();
	if(json.count("propertytype") > 0)
		m_propertyType = json["propertytype"].get<std::string>();
	else if(json.count("propertyType") > 0) //Somehow Tiled's class objects uses propertyType with 'T'.
		m_propertyType = json["propertyType"].get<std::string>();

	setTypeByString(json["type"].get<std::string>());
	setValueByType(json["value"]);
}

tson::Property::Property(std::string name, std::any value, Type type) : m_type {type}, m_name { std::move(name) }, m_value { std::move(value) }
{

}

void tson::Property::setValue(const std::any &value)
{
	m_value = value;
}

/*!
 * Sets the value specifically as string.
 * When not specified as std::string, the default is that the value will be set as char * when adding a value like "test"
 * This function is to make sure the value is added as string.
 * @param value
 */
void tson::Property::setStrValue(const std::string &value)
{
	m_value = value;
}

const std::any &tson::Property::getValue() const
{
	return m_value;
}

void tson::Property::setName(const std::string &name)
{
	m_name = name;
}

const std::string &tson::Property::getName() const
{
	return m_name;
}

/*!
 * Gets the value type as std::value_info.
 * This can easily be compared to types like this:
 * Check if int: getValueType() == typeid(int)
 * @return
 */

const std::type_info &tson::Property::getValueType() const
{
	return m_value.type();
}

/*!
 * Gets the value type as std::string
 * Examples of known types:
 * "i" = int
 * "f" = float
 * "b" = bool
 * @return
 */
std::string tson::Property::getValueTypeInfo()
{
	return m_value.type().name();
}

tson::Type tson::Property::getType() const
{
	return m_type;
}

void tson::Property::setTypeByString(const std::string &str)
{
	if(str == "color")
		m_type = tson::Type::Color;
	else if(str == "file")
		m_type = tson::Type::File;
	else if(str == "int")
		m_type = tson::Type::Int;
	else if(str == "bool")
		m_type = tson::Type::Boolean;
	else if(str == "float")
		m_type = tson::Type::Float;
	else if(str == "string")
		m_type = tson::Type::String;
	else if(str == "class")
		m_type = tson::Type::Class;
	else if(str == "object")
		m_type = tson::Type::Object;
	else
		m_type = tson::Type::Undefined;
}

const std::string &tson::Property::getPropertyType() const
{
	return m_propertyType;
}

#endif //TILESON_PROPERTY_HPP

/*** End of inlined file: Property.hpp ***/

//#include "../external/json.hpp"
#include <map>

namespace tson
{
	class PropertyCollection
	{
		public:
			inline PropertyCollection() = default;

			inline explicit PropertyCollection(std::string id);

			inline tson::Property * add(const tson::Property &property);
			inline tson::Property * add(IJson &json, tson::Project *project = nullptr);
			inline tson::Property * add(const std::string &name, const std::any &value, tson::Type type);

			inline void remove(const std::string &name);

			inline void setValue(const std::string &name, const std::any &value);
			inline void setProperty(const std::string &name, const tson::Property &value);
			inline void setId(const std::string &id);

			inline bool hasProperty(const std::string &name);
			inline tson::Property * getProperty(const std::string &name);
			inline std::map<std::string, Property> &getProperties();
			inline std::vector<Property*> get();
			template <typename T>
			inline T getValue(const std::string &name);
			[[nodiscard]] inline const std::string &getId() const;
			[[nodiscard]] inline size_t getSize() const;

		protected:
			std::string m_id;
			std::map<std::string, tson::Property> m_properties;
	};
}

template<typename T>
T tson::PropertyCollection::getValue(const std::string &name)
{
	static T defaultT;
	return (m_properties.count(name) > 0) ? m_properties[name].getValue<T>() : defaultT;
}

tson::PropertyCollection::PropertyCollection(std::string id) : m_id {std::move(id)}
{

}

tson::Property *tson::PropertyCollection::add(const tson::Property &property)
{
	m_properties[property.getName()] = property;
	return &m_properties[property.getName()];
}

tson::Property *tson::PropertyCollection::add(IJson &json, tson::Project *project)
{
	tson::Property property = tson::Property(json, project);
	const std::string name = property.getName();
	m_properties[name] = std::move(property);
	return &m_properties[name];
}

tson::Property *tson::PropertyCollection::add(const std::string &name, const std::any &value, tson::Type type)
{
	m_properties[name] = {name, value, type};
	return &m_properties[name];
}

void tson::PropertyCollection::remove(const std::string &name)
{
	m_properties.erase(name);
}

/*!
 * Sets a value IF the property already exists. Does nothing otherwise.
 * See add() for adding new properties
 * @param name
 * @param value
 */
void tson::PropertyCollection::setValue(const std::string &name, const std::any &value)
{
	if(m_properties.count(name) > 0)
		m_properties[name].setValue(value);
}

/*!
 * Overwrites the current property if it exists, or adds it if it doesn't.
 * @param name
 * @param value
 */
void tson::PropertyCollection::setProperty(const std::string &name, const tson::Property &value)
{
	m_properties[name] = value;
}

void tson::PropertyCollection::setId(const std::string &id)
{
	m_id = id;
}

bool tson::PropertyCollection::hasProperty(const std::string &name)
{
	return m_properties.count(name) > 0;
}

tson::Property *tson::PropertyCollection::getProperty(const std::string &name)
{
	return (m_properties.count(name) > 0) ? &m_properties[name] : nullptr;
}

std::map<std::string, tson::Property> &tson::PropertyCollection::getProperties()
{
	return m_properties;
}

/*!
 * Gets vector of pointers to all the existing properties
 * @return
 */
std::vector<tson::Property *> tson::PropertyCollection::get()
{
	std::vector<tson::Property *> props;
	for(auto &i : m_properties)
		props.emplace_back(&i.second);

	return props;
}

const std::string &tson::PropertyCollection::getId() const
{
	return m_id;
}

size_t tson::PropertyCollection::getSize() const
{
	return m_properties.size();
}

#endif //TILESON_PROPERTYCOLLECTION_HPP

/*** End of inlined file: PropertyCollection.hpp ***/


/*** Start of inlined file: Text.hpp ***/
//
// Created by robin on 05.08.2019.
//

#ifndef TILESON_TEXT_HPP
#define TILESON_TEXT_HPP

#include <string>

namespace tson
{
	class Text
	{
		public:
			inline Text() = default;
			/*!
			 *
			 * @param _text Text
			 * @param _wrap If the text is marked as wrapped
			 */
			//inline Text(std::string _text, bool _wrap, tson::Colori _color) : text {std::move(_text)}, wrap {_wrap}, color {_color} {};
			inline explicit Text(IJson &json)
			{
				bool hasColor = json.count("color") > 0;
				tson::Color c = (hasColor) ? tson::Colori(json["color"].get<std::string>()) : tson::Colori();
				color = c;
				text = (json.count("text") > 0) ? json["text"].get<std::string>() : "";
				wrap = (json.count("wrap") > 0) ? json["wrap"].get<bool>() : false;

				//Previously missing properties
				bold = (json.count("bold") > 0) ? json["bold"].get<bool>() : false;
				fontFamily = (json.count("fontfamily") > 0) ? json["fontfamily"].get<std::string>() : "sans-serif";
				horizontalAlignment = (json.count("halign") > 0) ? resolveTextAlignmentByString(json["halign"].get<std::string>()) : TextAlignment::Left;
				italic = (json.count("italic") > 0) ? json["italic"].get<bool>() : false;
				kerning = (json.count("kerning") > 0) ? json["kerning"].get<bool>() : true;
				pixelSize = (json.count("pixelsize") > 0) ? json["pixelsize"].get<int32_t>() : 16;
				strikeout = (json.count("strikeout") > 0) ? json["strikeout"].get<bool>() : false;
				underline = (json.count("underline") > 0) ? json["underline"].get<bool>() : false;
				verticalAlignment = (json.count("valign") > 0) ? resolveTextAlignmentByString(json["valign"].get<std::string>()) : TextAlignment::Top;
			};

			//Just make it simple
			std::string text {};
			tson::Colori color {};
			bool wrap{};

			//Previously missing properties
			bool bold {false};
			std::string fontFamily {"sans-serif"};
			TextAlignment horizontalAlignment {TextAlignment::Left};
			bool italic {false};
			bool kerning {true};
			int pixelSize {16};
			bool strikeout {false};
			bool underline {false};
			TextAlignment verticalAlignment {TextAlignment::Top};

		private:
			[[nodiscard]] TextAlignment resolveTextAlignmentByString(const std::string &str) const
			{
				if(str == "left") return TextAlignment::Left;
				if(str == "center") return TextAlignment::Center;
				if(str == "right") return TextAlignment::Right;
				if(str == "justify") return TextAlignment::Justify;
				if(str == "top") return TextAlignment::Top;
				if(str == "bottom") return TextAlignment::Bottom;

				return TextAlignment::Unresolved;
			}
	};
}

#endif //TILESON_TEXT_HPP

/*** End of inlined file: Text.hpp ***/

#include <optional>

namespace tson
{
	class TiledClass;
	class Map;
	class Object
	{
		public:
			//enum class Type : uint8_t
			//{
			//        Undefined = 0,
			//        Object = 1,
			//        Ellipse = 2,
			//        Rectangle = 3,
			//        Point = 4,
			//        Polygon = 5,
			//        Polyline = 6,
			//        Text = 7,
			//        Template = 8
			//};

			inline Object() = default;
			inline explicit Object(IJson &json, tson::Map *map);
			inline bool parse(IJson &json, tson::Map *map);

			[[nodiscard]] inline ObjectType getObjectType() const;
			[[nodiscard]] inline bool isEllipse() const;
			[[nodiscard]] inline uint32_t getGid() const;
			[[nodiscard]] inline const Vector2i &getSize() const;
			[[nodiscard]] inline int getId() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline bool isPoint() const;
			[[nodiscard]] inline float getRotation() const;
			[[nodiscard]] inline const std::string &getTemplate() const;
			[[nodiscard]] inline const std::string &getType() const;
			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */
			[[nodiscard]] inline bool isVisible() const;
			[[nodiscard]] inline const Vector2i &getPosition() const;

			[[nodiscard]] inline const std::vector<tson::Vector2i> &getPolygons() const;
			[[nodiscard]] inline const std::vector<tson::Vector2i> &getPolylines() const;
			[[nodiscard]] inline PropertyCollection &getProperties();
			[[nodiscard]] inline const Text &getText() const;

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			//v1.2.0-stuff
			[[nodiscard]] inline TileFlipFlags getFlipFlags() const;
			inline bool hasFlipFlags(TileFlipFlags flags);

		private:
			inline void setObjectTypeByJson(IJson &json, IJson* templ);

			ObjectType                        m_objectType = ObjectType::Undefined;    /*! Says with object type this is */
			bool                              m_ellipse {};                            /*! 'ellipse': Used to mark an object as an ellipse */
			uint32_t                          m_gid {};                                /*! 'gid': GID, only if object comes from a Tilemap */
			tson::Vector2i                    m_size;                                  /*! x = 'width' (Width in pixels), y = 'height' (Height in pixels). Ignored if using a gid.)*/
			int                               m_id{};                                  /*! 'id': Incremental id - unique across all objects */
			std::string                       m_name;                                  /*! 'name':  String assigned to name field in editor*/
			bool                              m_point {};                              /*! 'point': Used to mark an object as a point */
			std::vector<tson::Vector2i>       m_polygon; 	                           /*! 'polygon': A list of x,y coordinates in pixels */
			std::vector<tson::Vector2i>       m_polyline; 	                           /*! 'polyline': A list of x,y coordinates in pixels */
			tson::PropertyCollection          m_properties; 	                       /*! 'properties': A list of properties (name, value, type). */
			float                             m_rotation {};                           /*! 'rotation': Angle in degrees clockwise */
			std::string                       m_template;                              /*! 'template': Reference to a template file, in case object is a template instance */
			tson::Text                        m_text; 	                               /*! first: 'text' second: 'wrap' */
			std::string                       m_type;                                  /*! 'type': String assigned to type field in editor */
			bool                              m_visible {};                            /*! 'visible': Whether object is shown in editor. */
			tson::Vector2i                    m_position;                              /*! 'x' and 'y': coordinate in pixels */

			//v1.2.0-stuff
			tson::TileFlipFlags               m_flipFlags = TileFlipFlags::None;       /*! Resolved using bit 32, 31 and 30 from gid */

			tson::Map *m_map {nullptr};
			std::shared_ptr<tson::TiledClass> m_class {};
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::Object::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}

	/*!
	* Returns the requested IJson object if it exists in the map file or in a related template file
	* @param fieldName The name of the field to check
	* @param main The main json file being parsed
	* @param templ The template file json, if present, nullptr otherwise.
	* @return the requested json object if found in the main json file, otherwise if it is found in the template and nullptr if not found anywhere
	*/
	inline IJson* readField(const std::string& fieldName,  IJson& main, IJson* templ = nullptr);

	/*!
	* Attempts to read a text field from main file or the template if not overriden
	* @param fieldName The name of the field to check
	* @param main The main json file being parsed
	* @param templ The template file json, if present, nullptr otherwise.
	* @return true if the field was found and parsed in any of the objects, false otherwise
	*/
   inline bool readField(Text& field, const std::string& fieldName,  IJson& main, IJson* templ = nullptr);

	/*!
	* Attempts to read a series of coordinates from main file or the template if not overriden
	* @param fieldName The name of the field to check
	* @param main The main json file being parsed
	* @param templ The template file json, if present, nullptr otherwise.
	* @return true if the field was found and parsed in any of the objects, false otherwise
	*/
	inline bool readField(std::vector<Vector2i>& field, const std::string& fieldName, IJson& main, IJson* templ = nullptr);

	/*!
	* Attempts to read a field from main file or the template if not overriden
	* @param fieldName The name of the field to check
	* @param main The main json file being parsed
	* @param templ The template file json, if present, nullptr otherwise.
	* @return true if the field was found and parsed in any of the objects, false otherwise
	*/
	template <typename T> bool readField(T& field, const std::string& fieldName,  IJson& main, IJson* templ = nullptr)
	{
		IJson* fieldJson = readField(fieldName, main, templ);
		if(fieldJson){
			field = fieldJson->get<T>();
			return true;
		}
		return false;
	}

	/*!
	* Attempts to read a vector from main file or the template if not overriden
	* @param field Target variable to fill
	* @param fieldNameX The name of the field to check for the x part of the vector
	* @param fieldNameY The name of the field to check for the y part of the vector
	* @param main The main json file being parsed
	* @param templ The template file json, if present, nullptr otherwise.
	* @return true if the field was found and parsed in any of the objects, false otherwise
	*/
	inline bool readVector(Vector2i& field, const std::string& fieldNameX, const std::string& fieldNameY, IJson& main, IJson* templ = nullptr);

	/*!
	* Reads all custom properties from the given json node
	* @param properties Target Properties collection to fill
	* @param json json node representing the map object
	* @param map Pointer to current map being parsed
	*/
	inline void readProperties(tson::PropertyCollection& properties, IJson& json, tson::Map* map);

	/*!
	* Reads a gid, parsing flip-flags
	* @param properties Target Properties collection to fill
	* @param json json node representing the map object
	* @param map Pointer to current map being parsed
	*/
	inline void readGid(uint32_t& gid, TileFlipFlags& flags, IJson& main, IJson* templ = nullptr);
}

/*!
 * Parses a json Tiled object
 * @param json
 */
tson::Object::Object(IJson &json, tson::Map *map)
{
	parse(json, map);
}

/*!
 * Sets an object type based on json data.
 * @param json
 */
void tson::Object::setObjectTypeByJson(IJson &json, IJson* templ)
{
	m_objectType = ObjectType::Undefined;
	if(m_ellipse)
		m_objectType = ObjectType::Ellipse;
	else if(m_point)
		m_objectType = ObjectType::Point;
	else if(readField("polygon", json, templ))
		m_objectType = ObjectType::Polygon;
	else if(readField("polyline", json, templ))
		m_objectType = ObjectType::Polyline;
	else if(readField("text", json, templ))
		m_objectType = ObjectType::Text;
	else if(readField("gid", json, templ))
		m_objectType = ObjectType::Object;
	else if(json.count("template") > 0)
		m_objectType = ObjectType::Template;
	else
		m_objectType = ObjectType::Rectangle;
}

/*!
 * Gets what type of object this is.
 * @return
 */

tson::ObjectType tson::Object::getObjectType() const
{
	return m_objectType;
}

/*!
 * 'ellipse': Used to mark an object as an ellipse
 * @return
 */
bool tson::Object::isEllipse() const
{
	return m_ellipse;
}

/*!
 * 'gid': GID, only if object comes from a Tilemap
 * @return
 */
uint32_t tson::Object::getGid() const
{
	return m_gid;
}

/*!
 * x = 'width' (Width in pixels), y = 'height' (Height in pixels). Ignored if using a gid.)
 * @return
 */
const tson::Vector2i &tson::Object::getSize() const
{
	return m_size;
}

/*!
 * 'id': Incremental id - unique across all objects
 * @return
 */
int tson::Object::getId() const
{
	return m_id;
}

/*!
 * 'name': String assigned to name field in editor
 * @return
 */
const std::string &tson::Object::getName() const
{
	return m_name;
}

/*!
 * 'point': Used to mark an object as a point
 * @return true if the object is of type point
 */
bool tson::Object::isPoint() const
{
	return m_point;
}

/*!
 * 'rotation': Angle in degrees clockwise
 * @return
 */
float tson::Object::getRotation() const
{
	return m_rotation;
}

/*!
 * 'template': Reference to a template file, in case object is a template instance
 * @return
 */
const std::string &tson::Object::getTemplate() const
{
	return m_template;
}

/*!
 * 'type': String assigned to type field in editor
 * This was renamed to 'class' in Tiled v1.9
 * @return
 */
const std::string &tson::Object::getType() const
{
	return m_type;
}

/*!
 * 'class': String assigned to class field in editor
 * This was renamed from 'type' to 'class' in Tiled v1.9
 * @return
 */
const std::string &tson::Object::getClassType() const
{
	return m_type;
}

/*!
 * 'visible': Whether object is shown in editor.
 * @return
 */
bool tson::Object::isVisible() const
{
	return m_visible;
}

/*!
 * 'x' and 'y': coordinate in pixels
 * @return
 */
const tson::Vector2i &tson::Object::getPosition() const
{
	return m_position;
}

/*!
 * 'polygon': A list of x,y coordinates in pixels.
 * If this is a Polygon type, this function will return the points used to create it
 * @return
 */
const std::vector<tson::Vector2i> &tson::Object::getPolygons() const
{
	return m_polygon;
}

/*!
 * 'polyline': A list of x,y coordinates in pixels
 * If this is a Polyline type, this function will return the points used to create it
 * @return
 */
const std::vector<tson::Vector2i> &tson::Object::getPolylines() const
{
	return m_polyline;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::Object::getProperties()
{
	return m_properties;
}

/*!
 * 'type': String assigned to type field in editor
 * @return
 */
const tson::Text &tson::Object::getText() const
{
	return m_text;
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Object::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);
	return nullptr;
}

/*!
 * Get all flip flags
 * @return
 */
tson::TileFlipFlags tson::Object::getFlipFlags() const
{
	return m_flipFlags;
}

/*!
 *
 * @param flags Which flags to check for. Several flags can be checked at once using the bitwise or operator.
 * Example:
 * hasFlipFlags(TileFlipFlags::Vertically | TileFlipFlags::Horizontally)
 *
 * @return true if the flag(s) specified are set
 */
bool tson::Object::hasFlipFlags(TileFlipFlags flags)
{
	return ((m_flipFlags & flags) == flags) ? true : false;
}

#endif //TILESON_OBJECT_HPP

/*** End of inlined file: Object.hpp ***/


/*** Start of inlined file: TileObject.hpp ***/
//
// Created by robin on 26.07.2020.
//

#ifndef TILESON_TILEOBJECT_HPP
#define TILESON_TILEOBJECT_HPP


/*** Start of inlined file: Rect.hpp ***/
//
// Created by robin on 24.07.2020.
//

#ifndef TILESON_RECT_HPP
#define TILESON_RECT_HPP

namespace tson
{
	class Rect
	{
		public:

			inline Rect() = default;
			inline Rect(int x_, int y_, int width_, int height_);

			inline bool operator==(const Rect &rhs) const;
			inline bool operator!=(const Rect &rhs) const;

			int x{};
			int y{};
			int width{};
			int height{};
	};

	Rect::Rect(int x_, int y_, int width_, int height_)
	{
		x = x_;
		y = y_;
		width = width_;
		height = height_;
	}

	bool Rect::operator==(const Rect &rhs) const
	{
		return x == rhs.x &&
			   y == rhs.y &&
			   width == rhs.width &&
			   height == rhs.height;
	}

	bool Rect::operator!=(const Rect &rhs) const
	{
		return !(rhs == *this);
	}
}

#endif //TILESON_RECT_HPP

/*** End of inlined file: Rect.hpp ***/

namespace tson
{
	class Tile;
	class TileObject
	{
		public:
			inline TileObject() = default;
			inline TileObject(const std::tuple<int, int> &posInTileUnits, tson::Tile *tile);

			inline void initialize(const std::tuple<int, int> &posInTileUnits, tson::Tile *tile); //Defined in tileson_forward.hpp

			inline Tile *getTile();
			inline const Vector2i &getPositionInTileUnits() const;
			inline const Vector2f &getPosition() const;
			inline const tson::Rect &getDrawingRect() const; //Defined in tileson_forward.hpp

		private:
			tson::Tile *m_tile;
			tson::Vector2i m_posInTileUnits;
			tson::Vector2f m_position;

	};

	TileObject::TileObject(const std::tuple<int, int> &posInTileUnits, tson::Tile *tile)
	{
		initialize(posInTileUnits, tile);
	}

	/*!
	 * Get a pointer to the related tile
	 * @return
	 */
	Tile *TileObject::getTile()
	{
		return m_tile;
	}

	/*!
	 * Gets the position of the tile in tile units
	 * @return
	 */
	const Vector2i &TileObject::getPositionInTileUnits() const
	{
		return m_posInTileUnits;
	}

	/*!
	 * Gets the position of the tile in pixels.
	 * @return
	 */
	const Vector2f &TileObject::getPosition() const
	{
		return m_position;
	}
}

#endif //TILESON_TILEOBJECT_HPP

/*** End of inlined file: TileObject.hpp ***/


/*** Start of inlined file: FlaggedTile.hpp ***/
//
// Created by robin on 13.11.2020.
//

#ifndef TILESON_FLAGGEDTILE_HPP
#define TILESON_FLAGGEDTILE_HPP

namespace tson
{
	class FlaggedTile
	{

		public:
			FlaggedTile(size_t x_, size_t y_, uint32_t id_, uint32_t tileId_) : x {x_}, y {y_}, id {id_}, tileId {tileId_}
			{

			}
			size_t x;
			size_t y;
			/*! Full ID, including flag */
			uint32_t id;
			/*! ID of the flagged tile */
			uint32_t tileId;
	};
}
#endif //TILESON_FLAGGEDTILE_HPP

/*** End of inlined file: FlaggedTile.hpp ***/

namespace tson
{
	class Tile;
	class Map;

	class Layer
	{
		public:
			inline Layer() = default;
			inline Layer(IJson &json, tson::Map *map);
			inline bool parse(IJson &json, tson::Map *map); //Defined in tileson_forward

			[[nodiscard]] inline const std::string &getCompression() const;
			[[nodiscard]] inline const std::vector<uint32_t> &getData() const;
			[[nodiscard]] inline const std::string &getBase64Data() const;
			[[nodiscard]] inline const std::string &getDrawOrder() const;
			[[nodiscard]] inline const std::string &getEncoding() const;
			[[nodiscard]] inline int getId() const;
			[[nodiscard]] inline const std::string &getImage() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline const Vector2f &getOffset() const;
			[[nodiscard]] inline float getOpacity() const;
			[[nodiscard]] inline const Vector2i &getSize() const;
			[[nodiscard]] inline const Colori &getTransparentColor() const;
			[[nodiscard]] inline const Vector2f &getParallax() const;
			[[nodiscard]] inline bool hasRepeatX() const;
			[[nodiscard]] inline bool hasRepeatY() const;

			[[nodiscard]] inline LayerType getType() const;
			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */

			[[nodiscard]] inline const std::string &getTypeStr() const;
			[[nodiscard]] inline bool isVisible() const;
			[[nodiscard]] inline int getX() const;
			[[nodiscard]] inline int getY() const;

			[[nodiscard]] inline std::vector<tson::Chunk> &getChunks();
			[[nodiscard]] inline std::vector<tson::Layer> &getLayers();
			[[nodiscard]] inline std::vector<tson::Object> &getObjects();
			[[nodiscard]] inline PropertyCollection &getProperties();

			inline tson::Object *getObj(int id);
			inline tson::Object *firstObj(const std::string &name);
			inline std::vector<tson::Object> getObjectsByName(const std::string &name);
			inline std::vector<tson::Object> getObjectsByType(tson::ObjectType type);

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			inline void assignTileMap(std::map<uint32_t, tson::Tile*> *tileMap);
			inline void createTileData(const Vector2i &mapSize, bool isInfiniteMap);

			[[nodiscard]] inline const std::map<std::tuple<int, int>, tson::Tile *> &getTileData() const;
			inline tson::Tile * getTileData(int x, int y);

			//v1.2.0-stuff
			[[nodiscard]] inline const Colori &getTintColor() const;
			[[nodiscard]] inline tson::Map *getMap() const;

			[[nodiscard]] inline std::map<std::tuple<int, int>, tson::TileObject> &getTileObjects();
			inline tson::TileObject * getTileObject(int x, int y);
			[[nodiscard]] inline const std::set<uint32_t> &getUniqueFlaggedTiles() const;
			inline void resolveFlaggedTiles();

		private:
			inline void setTypeByString();

			std::vector<tson::Chunk>                       m_chunks; 	                      /*! 'chunks': Array of chunks (optional). tilelayer only. */
			std::string                                    m_compression;                     /*! 'compression': zlib, gzip or empty (default). tilelayer only. */
			std::vector<uint32_t>                          m_data;                            /*! 'data' (when uint array): Array of unsigned int (GIDs) or base64-encoded
																							   *   data. tilelayer only. */
			std::string                                    m_base64Data;                      /*! 'data' (when string):     Array of unsigned int (GIDs) or base64-encoded
																							   *   data. tilelayer only. */
			std::string                                    m_drawOrder;                       /*! 'draworder': topdown (default) or index. objectgroup only. */
			std::string                                    m_encoding;                        /*! 'encoding': csv (default) or base64. tilelayer only. */
			int                                            m_id{};                            /*! 'id': Incremental id - unique across all layers */
			std::string                                    m_image;                           /*! 'image': Image used by this layer. imagelayer only. */
			std::vector<tson::Layer>                       m_layers; 	                      /*! 'layers': Array of layers. group on */
			std::string                                    m_name;                            /*! 'name': Name assigned to this layer */
			std::vector<tson::Object>                      m_objects;                         /*! 'objects': Array of objects. objectgroup only. */
			tson::Vector2f                                 m_offset;                          /*! 'offsetx' and 'offsety': Horizontal and Vertical layer offset in pixels
																							   *  (default: {0, 0}) */
			float                                          m_opacity{};                       /*! 'opacity': Value between 0 and 1 */
			tson::PropertyCollection                       m_properties; 	                  /*! 'properties': A list of properties (name, value, type). */
			tson::Vector2i                                 m_size;                            /*! x = 'width': (Column count. Same as map width for fixed-size maps.)
																								  y = 'height': Row count. Same as map height for fixed-size maps. */
			tson::Colori                                   m_transparentColor;                /*! 'transparentcolor': Hex-formatted color (#RRGGBB) (optional, imagelayer only */
			std::string                                    m_typeStr;                         /*! 'type': tilelayer, objectgroup, imagelayer or group */
			LayerType                                      m_type {LayerType::Undefined};     /*! Layer type as enum*/
			bool                                           m_visible{};                       /*! 'visible': Whether layer is shown or hidden in editor */
			int                                            m_x{};                             /*! 'x': Horizontal layer offset in tiles. Always 0. */
			int                                            m_y{};                             /*! 'y': Vertical layer offset in tiles. Always 0. */
			tson::Vector2f                                 m_parallax{1.f, 1.f};    /*! Tiled v1.5: parallax factor for this layer. Defaults to 1.
																								  x = 'parallaxx', y = 'parallaxy'*/
			bool                                           m_repeatX {};                         /*! 'repeatx': Whether the image drawn by this layer is repeated along the X axis. (since Tiled 1.8)*/
			bool                                           m_repeatY {};                         /*! 'repeaty': Whether the image drawn by this layer is repeated along the Y axis. (since Tiled 1.8)*/

			std::map<uint32_t, tson::Tile*>                *m_tileMap;
			std::map<std::tuple<int, int>, tson::Tile*>    m_tileData;                        /*! Key: Tuple of x and y pos in tile units. */

			//v1.2.0-stuff
			tson::Colori                                        m_tintColor;                  /*! 'tintcolor': Hex-formatted color (#RRGGBB or #AARRGGBB) that is multiplied with
																							   *        any graphics drawn by this layer or any child layers (optional). */
			inline void decompressData();                                                     /*! Defined in tileson_forward.hpp */
			inline void queueFlaggedTile(size_t x, size_t y, uint32_t id);                    /*! Queue a flagged tile */

			tson::Map *                                         m_map;                        /*! The map who owns this layer */
			std::map<std::tuple<int, int>, tson::TileObject>    m_tileObjects;
			std::set<uint32_t>                                  m_uniqueFlaggedTiles;
			std::vector<tson::FlaggedTile>                      m_flaggedTiles;

			std::string                                         m_classType{};              /*! 'class': The class of this map (since 1.9, defaults to “”). */
			std::shared_ptr<tson::TiledClass>                   m_class {};
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T Layer::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

/*!
 * Parses a Tiled layer from json
 * @param json
 */
tson::Layer::Layer(IJson &json, tson::Map *map)
{
	parse(json, map);
}

void tson::Layer::queueFlaggedTile(size_t x, size_t y, uint32_t id)
{
	uint32_t tileId = id;
	tileId &= ~(FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG);
	m_uniqueFlaggedTiles.insert(id);
	m_flaggedTiles.emplace_back(x, y, id, tileId);
}

/*!
 * Copies all objects with a name that equals the parameter
 * @param name Name of the objects to return
 * @return All objects with a matching name
 */
std::vector<tson::Object> tson::Layer::getObjectsByName(const std::string &name)
{
	std::vector<tson::Object> found;

	std::copy_if(m_objects.begin(), m_objects.end(), std::back_inserter(found), [&](const tson::Object &item)
	{
		return item.getName() == name;
	});

	return found;
}

/*!
 * Copies all objects with a type that equals the parameter
 * @param type LayerType of the objects to return
 * @return All objects with a matching type
 */
std::vector<tson::Object> tson::Layer::getObjectsByType(tson::ObjectType type)
{
	std::vector<tson::Object> found;

	std::copy_if(m_objects.begin(), m_objects.end(), std::back_inserter(found), [&](const tson::Object &item)
	{
		return item.getObjectType() == type;
	});

	return found;
}

/*!
 * Returns the first object with the given name
 * @param name Name of the object to find.
 * @return A pointer to the object if found. nullptr otherwise.
 */
tson::Object *tson::Layer::firstObj(const std::string &name)
{
	auto result = std::find_if(m_objects.begin(), m_objects.end(), [&](const tson::Object &obj){return obj.getName() == name; });
	if(result == m_objects.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Get an object by ID
 * @param id Unique ID of the object
 * @return A pointer to the object if found. nullptr otherwise.
 */
tson::Object *tson::Layer::getObj(int id)
{
	auto result = std::find_if(m_objects.begin(), m_objects.end(), [&](const tson::Object &obj){return obj.getId() == id; });
	if(result == m_objects.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Set type by string
 * tilelayer, objectgroup, imagelayer or group
 */
void tson::Layer::setTypeByString()
{
	if(m_typeStr == "tilelayer") m_type = LayerType::TileLayer;
	else if(m_typeStr == "objectgroup") m_type = LayerType::ObjectGroup;
	else if(m_typeStr == "imagelayer") m_type = LayerType::ImageLayer;
	else if(m_typeStr == "group") m_type = LayerType::Group;
	else m_type = LayerType::Undefined;
}

/*!
 * 'compression': zlib, gzip or empty (default). tilelayer only.
 * @return
 */
const std::string &tson::Layer::getCompression() const
{
	return m_compression;
}

/*!
 * 'data' (when uint array): Array of unsigned int (GIDs) or base64-encoded data. tilelayer only.
 * @return
 */
const std::vector<uint32_t> &tson::Layer::getData() const
{
	return m_data;
}

/*!
 * 'data' (when string): Array of unsigned int (GIDs) or base64-encoded data. tilelayer only.
 * @return
 */
const std::string &tson::Layer::getBase64Data() const
{
	return m_base64Data;
}

/*!
 * 'draworder': topdown (default) or index. objectgroup only.
 * @return
 */
const std::string &tson::Layer::getDrawOrder() const
{
	return m_drawOrder;
}

/*!
 * 'encoding': csv (default) or base64. tilelayer only.
 * @return
 */
const std::string &tson::Layer::getEncoding() const
{
	return m_encoding;
}

/*!
 * 'id': Incremental id - unique across all layers
 * @return
 */
int tson::Layer::getId() const
{
	return m_id;
}

/*!
 * 'image': Image used by this layer. imagelayer only.
 * @return
 */
const std::string &tson::Layer::getImage() const
{
	return m_image;
}

/*!
 * 'name': Name assigned to this layer
 * @return
 */
const std::string &tson::Layer::getName() const
{
	return m_name;
}

/*!
 * 'offsetx' and 'offsety': Horizontal and Vertical layer offset in pixels (default: {0, 0})
 * @return
 */
const tson::Vector2f &tson::Layer::getOffset() const
{
	return m_offset;
}

/*!
 * 'opacity': Value between 0 and 1
 * @return
 */
float tson::Layer::getOpacity() const
{
	return m_opacity;
}

/*!
 * x = 'width': (Column count. Same as map width for fixed-size maps.)
 * y = 'height': Row count. Same as map height for fixed-size maps.
 * @return width and height as a single size
 */
const tson::Vector2i &tson::Layer::getSize() const
{
	return m_size;
}

/*!
 * 'transparentcolor': Color created from a hex color (#RRGGBB) (optional, imagelayer only)
 * @return color as color object with rgba channel.
 */
const tson::Colori &tson::Layer::getTransparentColor() const
{
	return m_transparentColor;
}

/*!
 * 'type': tilelayer, objectgroup, imagelayer or group
 * @return string with the object type
 */
const std::string &tson::Layer::getTypeStr() const
{
	return m_typeStr;
}

/*!
 * 'visible': Whether layer is shown or hidden in editor
 * @return
 */
bool tson::Layer::isVisible() const
{
	return m_visible;
}

/*!
 * 'x': Horizontal layer offset in tiles. Always 0.
 * @return x value (always 0 for layer)
 */
int tson::Layer::getX() const
{
	return m_x;
}

/*!
 * 'y': Horizontal layer offset in tiles. Always 0.
 * @return y value (always 0 for layer)
 */
int tson::Layer::getY() const
{
	return m_y;
}

/*!
 * 'chunks': Array of chunks (optional). tilelayer only.
 * @return
 */
std::vector<tson::Chunk> &tson::Layer::getChunks()
{
	return m_chunks;
}

/*!
 * 'layers': Array of layers. group on
 * @return
 */
std::vector<tson::Layer> &tson::Layer::getLayers()
{
	return m_layers;
}

/*!
 * 'objects': Array of objects. objectgroup only.
 * @return
 */
std::vector<tson::Object> &tson::Layer::getObjects()
{
	return m_objects;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::Layer::getProperties()
{
	return m_properties;
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Layer::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);
	return nullptr;
}

/*!
 * Get layer type
 * @return Layer type as enum
 */
tson::LayerType tson::Layer::getType() const
{
	return m_type;
}

/*!
 * Assigns a tilemap of pointers to existing tiles.
 * @param tileMap The tilemap. key: tile id, value: pointer to Tile.
 */
void tson::Layer::assignTileMap(std::map<uint32_t, tson::Tile *> *tileMap)
{
	m_tileMap = tileMap;
}

/*!
 * Get tile data as some kind of map with x and y position with pointers to existing tiles.
 * Map only contains tiles that are not empty. x and y position is in tile units.
 *
 * Example of getting tile from the returned map:
 *
 * Tile *tile = tileData[{0, 4}];
 *
 * @return A map that represents the data returned from getData() in a 2D map with Tile pointers.
 */
const std::map<std::tuple<int, int>, tson::Tile *> &tson::Layer::getTileData() const
{
	return m_tileData;
}

/*!
 * A safe way to get tile data
 * Get tile data as some kind of map with x and y position with pointers to existing tiles.
 * Map only contains tiles that are not empty. x and y position is in tile units.
 *
 * Example of getting tile:
 * Tile *tile = layer->getTileData(0, 4)
 *
 * @param x X position in tile units
 * @param y Y position in tile units
 * @return pointer to tile, if it exists. nullptr otherwise.
 */
tson::Tile *tson::Layer::getTileData(int x, int y)
{
	return (m_tileData.count({x, y}) > 0) ? m_tileData[{x,y}] : nullptr;
}

/*!
 * Used for getting the tson::Map who is the parent of this Layer.
 * @return a pointer to the tson::Map where this layer is contained.
 */
tson::Map *tson::Layer::getMap() const
{
	return m_map;
}

/*!
 *
 * This is only supported for non-infinite maps!
 *
 * @param mapSize The size of the map
 * @param isInfiniteMap Whether or not the current map is infinte.
 */
void tson::Layer::createTileData(const Vector2i &mapSize, bool isInfiniteMap)
{
	size_t x = 0;
	size_t y = 0;
	if(!isInfiniteMap)
	{
		std::for_each(m_data.begin(), m_data.end(), [&](uint32_t tileId)
		{
			if (static_cast<int>(x) == mapSize.x)
			{
				++y;
				x = 0;
			}

			if (tileId > 0 && m_tileMap->count(tileId) > 0)
			{
				m_tileData[{static_cast<int>(x), static_cast<int>(y)}] = m_tileMap->at(tileId);
				m_tileObjects[{static_cast<int>(x), static_cast<int>(y)}] = {{static_cast<int>(x), static_cast<int>(y)}, m_tileData[{static_cast<int>(x), static_cast<int>(y)}]};
			}
			else if(tileId > 0 && m_tileMap->count(tileId) == 0) //Tile with flip flags!
			{
				queueFlaggedTile(x, y, tileId);
			}
			x++;
		});

	}
}

std::map<std::tuple<int, int>, tson::TileObject> &tson::Layer::getTileObjects()
{
	return m_tileObjects;
}

tson::TileObject *tson::Layer::getTileObject(int x, int y)
{
	return (m_tileObjects.count({x, y}) > 0) ? &m_tileObjects[{x,y}] : nullptr;
}

const std::set<uint32_t> &tson::Layer::getUniqueFlaggedTiles() const
{
	return m_uniqueFlaggedTiles;
}

void tson::Layer::resolveFlaggedTiles()
{
	std::for_each(m_flaggedTiles.begin(), m_flaggedTiles.end(), [&](const tson::FlaggedTile &tile)
	{
		if (tile.id > 0 && m_tileMap->count(tile.id) > 0)
		{
			m_tileData[{static_cast<int>(tile.x), static_cast<int>(tile.y)}] = m_tileMap->at(tile.id);
			m_tileObjects[{static_cast<int>(tile.x), static_cast<int>(tile.y)}] = {{static_cast<int>(tile.x), static_cast<int>(tile.y)}, m_tileData[{static_cast<int>(tile.x), static_cast<int>(tile.y)}]};
		}
	});
}

/*!
 * 'tintcolor': Hex-formatted color (#RRGGBB or #AARRGGBB) that is multiplied with any graphics drawn by this layer or any child layers (optional).
 *
 * @return tintcolor
 */
const tson::Colori &tson::Layer::getTintColor() const
{
	return m_tintColor;
}

/*!
 * New in Tiled v1.5
 * Gets the parallax factor for current layer. Defaults to 1.
 * @return A vector with the x and y values of the parallax factor.
 */
const tson::Vector2f &tson::Layer::getParallax() const
{
	return m_parallax;
}

/*!
 * New in Tiled v1.8
 * 'repeatx': Whether the image drawn by this layer is repeated along the X axis.
 * @return true if image layer is repeated along the X axis, false otherwise.
 */
bool tson::Layer::hasRepeatX() const
{
	return m_repeatX;
}

/*!
 * New in Tiled v1.8
 * 'repeatx': Whether the image drawn by this layer is repeated along the Y axis.
 * @return true if image layer is repeated along the Y axis, false otherwise.
 */
bool tson::Layer::hasRepeatY() const
{
	return m_repeatY;
}

const std::string &tson::Layer::getClassType() const
{
	return m_classType;
}

#endif //TILESON_LAYER_HPP

/*** End of inlined file: Layer.hpp ***/


/*** Start of inlined file: Tileset.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_TILESET_HPP
#define TILESON_TILESET_HPP

//#include "../external/json.hpp"


/*** Start of inlined file: Transformations.hpp ***/
//
// Created by robin on 04.04.2021.
//

#ifndef TILESON_TRANSFORMATIONS_HPP
#define TILESON_TRANSFORMATIONS_HPP

namespace tson
{
	class Transformations
	{
		public:
			inline Transformations() = default;
			inline explicit Transformations(IJson &json);
			inline bool parse(IJson &json);

			inline bool allowHflip() const;
			inline bool allowPreferuntransformed() const;
			inline bool allowRotation() const;
			inline bool allowVflip() const;

		private:
			bool m_hflip {};                /*! hflip: Whether the tiles in this set can be flipped horizontally (default false) */
			bool m_preferuntransformed {};  /*! preferuntransformed: Whether untransformed tiles remain preferred, otherwise transformed tiles are used to produce more variations (default false) */
			bool m_rotate {};               /*! rotate: Whether the tiles in this set can be rotated in 90 degree increments (default false) */
			bool m_vflip {};                /*! vflip: Whether the tiles in this set can be flipped vertically (default false) */
	};

	Transformations::Transformations(IJson &json)
	{
		parse(json);
	}

	bool Transformations::parse(IJson &json)
	{
		if(json.count("hflip") > 0) m_hflip = json["hflip"].get<bool>(); //Optional
		if(json.count("preferuntransformed") > 0) m_preferuntransformed = json["preferuntransformed"].get<bool>(); //Optional
		if(json.count("rotate") > 0) m_rotate = json["rotate"].get<bool>(); //Optional
		if(json.count("vflip") > 0) m_vflip = json["vflip"].get<bool>(); //Optional

		return true;
	}

	/*!
	 *
	 * @return hflip: Whether the tiles in this set can be flipped horizontally (default false)
	 */
	bool Transformations::allowHflip() const
	{
		return m_hflip;
	}

	/*!
	 *
	 * @return preferuntransformed: Whether untransformed tiles remain preferred, otherwise transformed tiles are used to produce more variations (default false)
	 */
	bool Transformations::allowPreferuntransformed() const
	{
		return m_preferuntransformed;
	}

	/*!
	 *
	 * @return rotate: Whether the tiles in this set can be rotated in 90 degree increments (default false)
	 */
	bool Transformations::allowRotation() const
	{
		return m_rotate;
	}

	/*!
	 *
	 * @return vflip: Whether the tiles in this set can be flipped vertically (default false)
	 */
	bool Transformations::allowVflip() const
	{
		return m_vflip;
	}
}

#endif //TILESON_TRANSFORMATIONS_HPP

/*** End of inlined file: Transformations.hpp ***/


/*** Start of inlined file: WangSet.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_WANGSET_HPP
#define TILESON_WANGSET_HPP

//#include "../external/json.hpp"

/*** Start of inlined file: WangColor.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_WANGCOLOR_HPP
#define TILESON_WANGCOLOR_HPP

//#include "../external/json.hpp"

namespace tson
{
	class WangColor
	{
		public:
			inline WangColor() = default;
			inline explicit WangColor(IJson &json, tson::Map *map);
			inline bool parse(IJson &json, tson::Map *map);

			[[nodiscard]] inline const Colori &getColor() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline float getProbability() const;
			[[nodiscard]] inline int getTile() const;

			inline PropertyCollection &getProperties();
			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */

		private:
			tson::Colori      m_color;              /*! 'color': Hex-formatted color (#RRGGBB or #AARRGGBB) */
			std::string       m_name;               /*! 'name': Name of the Wang color */
			float             m_probability{};      /*! 'probability': Probability used when randomizing */
			int               m_tile{};             /*! 'tile': Local ID of tile representing the Wang color */

			//New in Tiled v1.5
			tson::PropertyCollection     m_properties; 	  /*! 'properties': A list of properties (name, value, type). */
			tson::Map *                  m_map;
			std::string                  m_classType {};              /*! 'class': The class of this map (since 1.9, defaults to “”). */
			std::shared_ptr<tson::TiledClass> m_class {};

	};
}

tson::WangColor::WangColor(IJson &json, tson::Map *map)
{
	parse(json, map);
}

bool tson::WangColor::parse(IJson &json, tson::Map *map)
{
	m_map = map;
	bool allFound = true;

	if(json.count("color") > 0) m_color = tson::Colori(json["color"].get<std::string>()); else allFound = false;
	if(json.count("name") > 0) m_name = json["name"].get<std::string>(); else allFound = false;
	if(json.count("probability") > 0) m_probability = json["probability"].get<float>(); else allFound = false;
	if(json.count("tile") > 0) m_tile = json["tile"].get<int>(); else allFound = false;
	if(json.count("class") > 0) m_classType = json["class"].get<std::string>();                     //Optional

	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item); });
	}

	return allFound;
}

/*!
 * 'color': Color object created from hex-formatted string (#RRGGBB or #AARRGGBB)
 * @return
 */
const tson::Colori &tson::WangColor::getColor() const
{
	return m_color;
}

/*!
 * 'name': Name of the Wang color
 * @return
 */
const std::string &tson::WangColor::getName() const
{
	return m_name;
}

/*!
 * 'probability': Probability used when randomizing
 * @return
 */
float tson::WangColor::getProbability() const
{
	return m_probability;
}

/*!
 * 'tile': Local ID of tile representing the Wang color
 * @return
 */
int tson::WangColor::getTile() const
{
	return m_tile;
}

/*!
 * New property in Tiled v1.5 when data is contained in 'colors' of a wangset
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::WangColor::getProperties()
{
	return m_properties;
}

/*!
 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
 * @tparam T The template value
 * @param name Name of the property
 * @return The actual value, if it exists. Otherwise: The default value of the type.
 */
template<typename T>
T tson::WangColor::get(const std::string &name)
{
	return m_properties.getValue<T>(name);
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::WangColor::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);

	return nullptr;
}

const std::string &tson::WangColor::getClassType() const
{
	return m_classType;
}

#endif //TILESON_WANGCOLOR_HPP

/*** End of inlined file: WangColor.hpp ***/



/*** Start of inlined file: WangTile.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_WANGTILE_HPP
#define TILESON_WANGTILE_HPP

//#include "../external/json.hpp"

namespace tson
{
	class WangTile
	{
		public:
			inline WangTile() = default;
			inline explicit WangTile(IJson &json);
			inline bool parse(IJson &json);

			[[nodiscard]] inline bool hasDFlip() const;
			[[nodiscard]] inline bool hasHFlip() const;
			[[nodiscard]] inline uint32_t getTileid() const;
			[[nodiscard]] inline bool hasVFlip() const;

			[[nodiscard]] inline const std::vector<uint32_t> &getWangIds() const;

		private:
			bool                    m_dflip{};     /*! 'dflip': Tile is flipped diagonally */
			bool                    m_hflip{};     /*! 'hflip': Tile is flipped horizontally */
			uint32_t                m_tileid{};    /*! 'tileid': Local ID of tile */
			bool                    m_vflip{};     /*! 'vflip': Tile is flipped vertically */
			std::vector<uint32_t>   m_wangId;      /*! 'wangid': Array of Wang color indexes (uchar[8])*/
	};
}

tson::WangTile::WangTile(IJson &json)
{
	parse(json);
}

/*!
 * Parses a wang tile from Tiled json.
 * @param json A Tiled json file
 * @return true if all mandatory fields were found. False otherwise.
 */
bool tson::WangTile::parse(IJson &json)
{
	bool allFound = true;

	if(json.count("dflip") > 0) m_dflip = json["dflip"].get<bool>(); //Removed in Tiled v1.5 and is now optional
	if(json.count("hflip") > 0) m_hflip = json["hflip"].get<bool>(); //Removed in Tiled v1.5 and is now optional
	if(json.count("vflip") > 0) m_vflip = json["vflip"].get<bool>(); //Removed in Tiled v1.5 and is now optional

	if(json.count("tileid") > 0) m_tileid = json["tileid"].get<uint32_t>(); else allFound = false;
	if(json.count("wangid") > 0 && json["wangid"].isArray())
	{
		auto &wangid = json.array("wangid");
		std::for_each(wangid.begin(), wangid.end(), [&](std::unique_ptr<IJson> &item) { m_wangId.emplace_back(item->get<uint32_t>()); });
	}

	return allFound;
}

/*!
 * 'dflip': Tile is flipped diagonally
 *
 * NB! This property got removed in Tiled v1.5
 * @return
 */
bool tson::WangTile::hasDFlip() const
{
	return m_dflip;
}

/*!
 * 'hflip': Tile is flipped horizontally
 *
 * NB! This property got removed in Tiled v1.5
 * @return
 */
bool tson::WangTile::hasHFlip() const
{
	return m_hflip;
}

/*!
 * 'tileid': Local ID of tile
 * @return
 */
uint32_t tson::WangTile::getTileid() const
{
	return m_tileid;
}

/*!
 * 'vflip': Tile is flipped vertically
 *
 * NB! This property got removed in Tiled v1.5
 * @return
 */
bool tson::WangTile::hasVFlip() const
{
	return m_vflip;
}

/*!
 * 'wangid': Array of Wang color indexes (uchar[8])
 * @return
 */
const std::vector<uint32_t> &tson::WangTile::getWangIds() const
{
	return m_wangId;
}

#endif //TILESON_WANGTILE_HPP

/*** End of inlined file: WangTile.hpp ***/

namespace tson
{
	class WangSet
	{
		public:
			inline WangSet() = default;
			inline explicit WangSet(IJson &json, tson::Map *map);
			inline bool parse(IJson &json, tson::Map *map);

			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline int getTile() const;

			[[nodiscard]] inline const std::vector<tson::WangTile> &getWangTiles() const;
			[[nodiscard]] inline const std::vector<tson::WangColor> &getCornerColors() const;
			[[nodiscard]] inline const std::vector<tson::WangColor> &getEdgeColors() const;

			inline tson::WangColor * getColor(const std::string &name);
			inline const std::vector<tson::WangColor> &getColors() const;
			inline PropertyCollection &getProperties();

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */

		private:

			inline bool parseTiled15Props(IJson &json);

			std::string                  m_name;          /*! 'name': Name of the Wang set */
			int                          m_tile{};        /*! 'tile': Local ID of tile representing the Wang set */
			std::vector<tson::WangTile>  m_wangTiles;     /*! 'wangtiles': Array of Wang tiles */
			std::vector<tson::WangColor> m_cornerColors;  /*! 'cornercolors': Array of Wang colors */
			std::vector<tson::WangColor> m_edgeColors;    /*! 'edgecolors': Array of Wang colors */
			tson::PropertyCollection     m_properties; 	  /*! 'properties': A list of properties (name, value, type). */

			//Tiled v1.5
			std::vector<tson::WangColor> m_colors;        /*! 'colors': */

			tson::Map *                  m_map;
			std::string                  m_classType {};              /*! 'class': The class of this map (since 1.9, defaults to “”). */
			std::shared_ptr<tson::TiledClass> m_class {};

	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::WangSet::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

tson::WangSet::WangSet(IJson &json, tson::Map *map)
{
	parse(json, map);
}

bool tson::WangSet::parse(IJson &json, tson::Map *map)
{
	m_map = map;
	bool allFound = true;

	if(json.count("tile") > 0) m_tile = json["tile"].get<int>(); else allFound = false;
	if(json.count("name") > 0) m_name = json["name"].get<std::string>(); else allFound = false;

	//More advanced data
	if(json.count("wangtiles") > 0 && json["wangtiles"].isArray())
	{
		auto &wangtiles = json.array("wangtiles");
		std::for_each(wangtiles.begin(), wangtiles.end(), [&](std::unique_ptr<IJson> &item) { m_wangTiles.emplace_back(*item); });
	}
	if(json.count("cornercolors") > 0 && json["cornercolors"].isArray())
	{
		auto &cornercolors = json.array("cornercolors");
		std::for_each(cornercolors.begin(), cornercolors.end(), [&](std::unique_ptr<IJson> &item) { m_cornerColors.emplace_back(*item, m_map); });
	}
	if(json.count("edgecolors") > 0 && json["edgecolors"].isArray())
	{
		auto &edgecolors = json.array("edgecolors");
		std::for_each(edgecolors.begin(), edgecolors.end(), [&](std::unique_ptr<IJson> &item) { m_edgeColors.emplace_back(*item, m_map); });
	}
	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item); });
	}

	if(json.count("class") > 0) m_classType = json["class"].get<std::string>();                     //Optional

	if(!parseTiled15Props(json))
		allFound = false;

	return allFound;
}

/*!
 * Parsing data related to Tiled v1.5 changes
 * @param json The json containing data
 * @return Returns true if success
 */

bool tson::WangSet::parseTiled15Props(tson::IJson &json)
{
	if(json.count("colors") > 0 && json["colors"].isArray())
	{
		auto &colors = json.array("colors");
		std::for_each(colors.begin(), colors.end(), [&](std::unique_ptr<IJson> &item) { m_colors.emplace_back(*item, m_map); });
	}
	return true;
}

/*!
 * 'name': Name of the Wang set
 * @return
 */
const std::string &tson::WangSet::getName() const
{
	return m_name;
}

/*!
 * 'tile': Local ID of tile representing the Wang set
 * @return
 */
int tson::WangSet::getTile() const
{
	return m_tile;
}

/*!
 * 'wangtiles': Array of Wang tiles
 * @return
 */
const std::vector<tson::WangTile> &tson::WangSet::getWangTiles() const
{
	return m_wangTiles;
}

/*!
 * 'cornercolors': Array of Wang colors
 * @return
 */
const std::vector<tson::WangColor> &tson::WangSet::getCornerColors() const
{
	return m_cornerColors;
}

/*!
 * 'edgecolors': Array of Wang colors
 * @return
 */
const std::vector<tson::WangColor> &tson::WangSet::getEdgeColors() const
{
	return m_edgeColors;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::WangSet::getProperties()
{
	return m_properties;
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::WangSet::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);

	return nullptr;
}

/*!
 * Get Wangset colors (new in Tiled v1.5)
 * @return
 */
const std::vector<tson::WangColor> &tson::WangSet::getColors() const
{
	return m_colors;
}

/*!
 * NB! Will only work with maps created/modified by Tiled v1.5 or later!
 * Gets a color from a wangset by its name.
 *
 * @param name 'name' of WangColor
 * @return The WangColor with the given name or nullptr if it doesn't exist.
 */
tson::WangColor *tson::WangSet::getColor(const std::string &name)
{
	auto color = std::find_if(m_colors.begin(), m_colors.end(), [&](const auto &c) { return c.getName() == name; });

	if(color != m_colors.end())
		return &color.operator*();

	return nullptr;
}

const std::string &tson::WangSet::getClassType() const
{
	return m_classType;
}

#endif //TILESON_WANGSET_HPP

/*** End of inlined file: WangSet.hpp ***/


/*** Start of inlined file: Tile.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_TILE_HPP
#define TILESON_TILE_HPP

//#include "../external/json.hpp"


/*** Start of inlined file: Frame.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_FRAME_HPP
#define TILESON_FRAME_HPP

//#include "../external/json.hpp"

namespace tson
{
	class Frame
	{
		public:
			inline Frame() = default;
			inline Frame(int duration, uint32_t tileId);
			inline explicit Frame(IJson &json);

			inline bool parse(IJson &json);

			[[nodiscard]] inline int getDuration() const;
			[[nodiscard]] inline uint32_t getTileId() const;

		private:
			int m_duration {};  /*! 'duration': Frame duration in milliseconds */
			uint32_t m_tileId {};    /*! 'tileid': Local tile ID representing this frame */
	};
}

/*!
 *
 * @param duration duration in milliseconds
 * @param tileId TileId
 */
tson::Frame::Frame(int duration, uint32_t tileId) : m_duration {duration}, m_tileId {tileId}
{

}

/*!
 * Parses frame data from json
 * @param json
 */
tson::Frame::Frame(IJson &json)
{
	parse(json);
}

/*!
 * Parses frame data from json
 * @param json
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Frame::parse(IJson &json)
{
	bool allFound = true;

	if(json.count("duration") > 0) m_duration = json["duration"].get<int>(); else allFound = false;
	if(json.count("tileid") > 0) m_tileId = json["tileid"].get<uint32_t>() + 1; else allFound = false;

	return allFound;
}

/*!
 * 'duration': Frame duration in milliseconds
 * @return Duration in milliseconds
 */
int tson::Frame::getDuration() const
{
	return m_duration;
}

/*!
 * 'tileid': Local tile ID representing this frame
 * @return tile id
 */
uint32_t tson::Frame::getTileId() const
{
	return m_tileId;
}

#endif //TILESON_FRAME_HPP

/*** End of inlined file: Frame.hpp ***/


/*** Start of inlined file: Animation.hpp ***/
//
// Created by robin on 21.05.2021.
//

#ifndef TILESON_ANIMATION_HPP
#define TILESON_ANIMATION_HPP

namespace tson
{
	class Animation
	{
		public:
			inline Animation() = default;
			inline Animation(const std::vector<tson::Frame> &frames) : m_frames {frames} {};

			inline void update(float timeDeltaMs);
			inline void reset();

			inline void setFrames(const std::vector<tson::Frame> &frames);
			inline void setCurrentFrame(uint32_t currentFrame);
			inline void setTimeDelta(float timeDelta);

			inline const std::vector<tson::Frame> &getFrames() const;
			inline const tson::Frame *getCurrentFrame() const;
			inline uint32_t getCurrentFrameNumber() const;
			inline uint32_t getCurrentTileId() const;
			inline float getTimeDelta() const;

			inline bool any() const;
			inline size_t size() const;

		private:
			inline int nextFrame();
			std::vector<tson::Frame> m_frames;
			uint32_t m_currentFrame {0};
			float m_timeDelta {0};
	};

	const std::vector<tson::Frame> &Animation::getFrames() const
	{
		return m_frames;
	}

	/*!
	 * Resets the current frame and time delta to 0.
	 */
	void Animation::reset()
	{
		m_currentFrame = 0;
		m_timeDelta = 0.f;
	}

	/*!
	 * Gets the current frame or nullptr if no frame is found.
	 * @return
	 */
	const tson::Frame *Animation::getCurrentFrame() const
	{
		return (m_frames.size() == 0 || m_currentFrame >= m_frames.size()) ? nullptr : &m_frames.at(m_currentFrame);
	}

	size_t Animation::size() const
	{
		return m_frames.size();
	}

	/*!
	 * Update animation based on the fra
	 * @param timedeltaMs Time in milliseconds
	 */
	void Animation::update(float timeDeltaMs)
	{
		const tson::Frame *frame = getCurrentFrame();
		if(frame != nullptr)
		{
			m_timeDelta += timeDeltaMs;
			if(m_timeDelta >= frame->getDuration())
			{
				m_timeDelta = static_cast<float>((int32_t)m_timeDelta % frame->getDuration());
				m_currentFrame = nextFrame();
			}
		}
	}

	int Animation::nextFrame()
	{
		return (m_currentFrame+1 >= m_frames.size()) ? 0 : m_currentFrame + 1;
	}

	float Animation::getTimeDelta() const
	{
		return m_timeDelta;
	}

	uint32_t Animation::getCurrentFrameNumber() const
	{
		return m_currentFrame;
	}

	uint32_t Animation::getCurrentTileId() const
	{
		return (getCurrentFrame() != nullptr) ? getCurrentFrame()->getTileId() : 0;
	}

	void Animation::setFrames(const std::vector<tson::Frame> &frames)
	{
		m_frames = frames;
	}

	void Animation::setCurrentFrame(uint32_t currentFrame)
	{
		m_currentFrame = currentFrame;
	}

	void Animation::setTimeDelta(float timeDelta)
	{
		m_timeDelta = timeDelta;
	}

	/*!
	 * True if any frames exists, false otherwise
	 * @return
	 */
	bool Animation::any() const
	{
		return m_frames.size() > 0;
	}
}

#endif //TILESON_ANIMATION_HPP

/*** End of inlined file: Animation.hpp ***/

namespace tson
{
	class Tileset;
	class TiledClass;

	class Tile
	{
		public:
			inline Tile() = default;
			inline Tile(IJson &json, tson::Tileset *tileset, tson::Map *map);
			inline static Tile CreateFlippedTile(uint32_t gid, tson::Map *map); // v1.2.0
			inline static Tile CreateMissingTile(uint32_t id, tson::Tileset* tileset, tson::Map* map);
			inline bool parse(IJson &json, tson::Tileset *tileset, tson::Map *map);
			inline bool parseId(IJson &json);

			[[nodiscard]] inline uint32_t getId() const;
			[[nodiscard]] inline const fs::path &getImage() const;
			[[nodiscard]] inline const Vector2i &getImageSize() const;
			[[nodiscard]] inline const std::string &getType() const;
			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */

			//[[nodiscard]] inline const std::vector<tson::Frame> &getAnimation() const;
			[[nodiscard]] inline tson::Animation &getAnimation();
			[[nodiscard]] inline Layer &getObjectgroup();
			[[nodiscard]] inline PropertyCollection &getProperties();
			[[nodiscard]] inline const std::vector<int> &getTerrain() const;

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			//v1.2.0-stuff
			inline void setProperties(const tson::PropertyCollection &properties);

			[[nodiscard]] inline tson::Tileset * getTileset() const;
			[[nodiscard]] inline tson::Map * getMap() const;
			[[nodiscard]] inline const tson::Rect &getDrawingRect() const;
			[[nodiscard]] inline const Rect &getSubRectangle() const;

			inline const tson::Vector2f getPosition(const std::tuple<int, int> &tileDataPos);
			inline const tson::Vector2i getPositionInTileUnits(const std::tuple<int, int> &tileDataPos);
			[[nodiscard]] inline const tson::Vector2i getTileSize() const;                       /*! Declared in tileson_forward.hpp */

			[[nodiscard]] inline TileFlipFlags getFlipFlags() const;
			inline bool hasFlipFlags(TileFlipFlags flags);
			[[nodiscard]] inline uint32_t getGid() const;

			inline void addTilesetAndPerformCalculations(tson::Tileset *tileset); //v1.2.0

		private:
			tson::Animation                  m_animation{};      /*! 'animation': Array of Frames */
			uint32_t                         m_id {};            /*! 'id': Local ID of the tile */

			fs::path                         m_image;            /*! 'image': Image representing this tile (optional)*/

			tson::Vector2i                   m_imageSize;        /*! x = 'imagewidth' and y = 'imageheight': in pixels */
			tson::Layer                      m_objectgroup; 	 	/*! 'objectgroup': Layer with type objectgroup (optional) */
			tson::PropertyCollection         m_properties; 	    /*! 'properties': A list of properties (name, value, type). */
			std::vector<int>                 m_terrain;          /*! 'terrain': Index of terrain for each corner of tile */
			std::string                      m_type;             /*! 'type': The type of the tile (optional) */

			//v1.2.0-stuff
			uint32_t                    m_gid {};                                    /*! id without flip flags */
			tson::Tileset *             m_tileset;                                   /*! A pointer to the tileset where this Tile comes from */
			tson::Map *                 m_map;                                       /*! A pointer to the map where this tile is contained */
			tson::Rect                  m_drawingRect;                               /*! A rect that shows which part of the tileset that is used for this tile */
			tson::Rect                  m_subRect;                                   /*! Tiled 1.9: Contains the newly added sub-rectangle variables: 'x', 'y', 'width' and 'height'*/
			tson::TileFlipFlags         m_flipFlags = TileFlipFlags::None;           /*! Resolved using bit 32, 31 and 30 from gid */
			inline void performDataCalculations();                                   /*! Declared in tileson_forward.hpp - Calculate all the values used in the tile class. */
			inline void manageFlipFlagsByIdThenRemoveFlags(uint32_t &id);
			friend class Layer;
			std::shared_ptr<tson::TiledClass> m_class {};
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::Tile::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

tson::Tile::Tile(IJson &json, tson::Tileset *tileset, tson::Map *map)
{
	parse(json, tileset, map);
}

/*!
 * For flip flagged tiles, tilesets must be resolved later.
 * @param tileset
 */
void tson::Tile::addTilesetAndPerformCalculations(tson::Tileset *tileset)
{
	m_tileset = tileset;
	performDataCalculations();
}

/*!
 * 'id': Local ID of the tile
 * @return
 */
uint32_t tson::Tile::getId() const
{
	return m_id;
}

/*!
 * 'image': Image representing this tile (optional)
 * @return
 */

const fs::path &tson::Tile::getImage() const { return m_image; }

/*!
 * x = 'imagewidth' and y = 'imageheight': in pixels
 * @return
 */
const tson::Vector2i &tson::Tile::getImageSize() const
{
	return m_imageSize;
}

/*!
 * 'type': The type of the tile (optional)
 * This was renamed to 'class' in Tiled v1.9
 * @return
 */
const std::string &tson::Tile::getType() const
{
	return m_type;
}

/*!
  * 'class': String assigned to class field in editor (optional)
 * This was renamed from 'type' to 'class' in Tiled v1.9
 * @return
 */
const std::string &tson::Tile::getClassType() const
{
	return m_type;
}

/*!
 * 'animation': Array of Frames
 * @return
 */
tson::Animation &tson::Tile::getAnimation()
{
	return m_animation;
}

/*!
 * 'objectgroup': Layer with type objectgroup (optional)
 * @return
 */
tson::Layer &tson::Tile::getObjectgroup()
{
	return m_objectgroup;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::Tile::getProperties()
{
	return m_properties;
}

/*!
 * 'terrain': Index of terrain for each corner of tile
 * @return
 */
const std::vector<int> &tson::Tile::getTerrain() const
{
	return m_terrain;
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Tile::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);

	return nullptr;
}

/*!
 * Used for getting the tson::Tileset who is the parent of this Tile.
 * @return a pointer to the tson::Tileset where this tile is contained.
 */
tson::Tileset *tson::Tile::getTileset() const
{
	return m_tileset;
}

/*!
 * Used for getting the tson::Map who is the parent of this Tile.
 * @return a pointer to the tson::Map where this tile is contained.
 */
tson::Map *tson::Tile::getMap() const
{
	return m_map;
}

/*!
 * Get the information needed to draw the Tile based on its current tileset
 * @return a tson::Rect containing the information needed to draw the tile.
 */
const tson::Rect &tson::Tile::getDrawingRect() const
{
	return m_drawingRect;
}

/*!
 * Helper function.
 *
 * Get the position of the tile in tile units.
 * The size of each unit is determined by the tile size property of the map.
 * Example: If the tile size is 16x16 in the map, a tile unit of [2, 4] would be [32, 64] in pixels.
 * If you want the position in pixels: use getPosition() instead.
 *
 * @return Position of tile in tile units.
 */
const tson::Vector2i tson::Tile::getPositionInTileUnits(const std::tuple<int, int> &tileDataPos)
{
	return {std::get<0>(tileDataPos), std::get<1>(tileDataPos)};
}

void tson::Tile::manageFlipFlagsByIdThenRemoveFlags(uint32_t &id)
{
	if (id & FLIPPED_HORIZONTALLY_FLAG) m_flipFlags |= TileFlipFlags::Horizontally;
	if (id & FLIPPED_VERTICALLY_FLAG) m_flipFlags |= TileFlipFlags::Vertically;
	if (id & FLIPPED_DIAGONALLY_FLAG) m_flipFlags |= TileFlipFlags::Diagonally;

	id &= ~(FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG);
}

tson::TileFlipFlags tson::Tile::getFlipFlags() const
{
	return m_flipFlags;
}

/*!
 *
 * @param flags Which flags to check for. Several flags can be checked at once using the bitwise or operator.
 * Example:
 * hasFlipFlags(TileFlipFlags::Vertically | TileFlipFlags::Horizontally)
 *
 * @return true if the flag(s) specified are set
 */
bool tson::Tile::hasFlipFlags(tson::TileFlipFlags flags)
{
	return ((m_flipFlags & flags) == flags) ? true : false;
}

uint32_t tson::Tile::getGid() const
{
	return m_gid;
}

void tson::Tile::setProperties(const tson::PropertyCollection &properties)
{
	m_properties = properties;
}

/*!
 * Tiled 1.9: Contains the newly added sub-rectangle variables: 'x', 'y', 'width' and 'height'
 * @return A tson::Rect with the 'x', 'y', 'width' and 'height' values
 */
const tson::Rect &tson::Tile::getSubRectangle() const
{
	return m_subRect;
}

#endif //TILESON_TILE_HPP

/*** End of inlined file: Tile.hpp ***/


/*** Start of inlined file: Terrain.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_TERRAIN_HPP
#define TILESON_TERRAIN_HPP

//#include "../external/json.hpp"

namespace tson
{
	class Terrain
	{
		public:
			inline Terrain() = default;
			inline Terrain(std::string name, int tile);
			inline explicit Terrain(IJson &json);

			inline bool parse(IJson &json);

			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline int getTile() const;
			[[nodiscard]] inline PropertyCollection &getProperties();

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

		private:
			std::string                 m_name;        /*! 'name': Name of terrain */
			int                         m_tile {};     /*! 'tile': Local ID of tile representing terrain */
			tson::PropertyCollection    m_properties;  /*! 'properties': A list of properties (name, value, type). */
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::Terrain::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

tson::Terrain::Terrain(std::string name, int tile) : m_name {std::move(name)}, m_tile {tile}
{

}

tson::Terrain::Terrain(IJson &json)
{
	parse(json);
}

bool tson::Terrain::parse(IJson &json)
{
	bool allFound = true;

	if(json.count("name") > 0) m_name = json["name"].get<std::string>(); else allFound = false;
	if(json.count("tile") > 0) m_tile = json["tile"].get<int>(); else allFound = false;

	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item); });
	}

	return allFound;
}

/*!
 * 'name': Name of terrain
 * @return
 */
const std::string &tson::Terrain::getName() const
{
	return m_name;
}

/*!
 * 'tile': Local ID of tile representing terrain
 * @return
 */
int tson::Terrain::getTile() const
{
	return m_tile;
}

/*!
 * 'properties': A list of properties (name, value, type). *Missing from the official Tiled documentation...*
 * @return
 */
tson::PropertyCollection &tson::Terrain::getProperties()
{
	return m_properties;
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Terrain::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);
	return nullptr;
}

#endif //TILESON_TERRAIN_HPP

/*** End of inlined file: Terrain.hpp ***/


/*** Start of inlined file: Grid.hpp ***/
//
// Created by robin on 22.03.2020.
//

#ifndef TILESON_GRID_HPP
#define TILESON_GRID_HPP

#include <string>
//#include "../external/json.hpp"

namespace tson
{
	class Grid
	{
		public:
			inline Grid() = default;
			inline explicit Grid(IJson &json);

			inline bool parse(IJson &json);

			[[nodiscard]] inline const std::string &getOrientation() const;
			[[nodiscard]] inline const Vector2i &getSize() const;

		private:
			std::string m_orientation; /*! 'orientation': Orientation of the grid for the tiles in this tileset (orthogonal or isometric) */
			tson::Vector2i m_size; /*! 'width' and 'height': Size. */
	};
}

/*!
 * Parses Tiled grid data from json
 * @param json
 */
tson::Grid::Grid(IJson &json)
{
	parse(json);
}

/*!
 * Parses Tiled grid data from json
 * @param json
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Grid::parse(IJson &json)
{
	bool allFound = true;

	if(json.count("orientation") > 0) m_orientation = json["orientation"].get<std::string>(); //Optional

	if(json.count("width") > 0 && json.count("height") > 0)
		m_size = {json["width"].get<int>(), json["height"].get<int>()}; else allFound = false;

	return allFound;
}

/*!
 * 'orientation': Orientation of the grid for the tiles in this tileset (orthogonal or isometric)
 * @return orientation as string
 */
const std::string &tson::Grid::getOrientation() const
{
	return m_orientation;
}

/*!
 * 'width' and 'height': Size.
 * @return size as int
 */
const tson::Vector2i &tson::Grid::getSize() const
{
	return m_size;
}

#endif //TILESON_GRID_HPP

/*** End of inlined file: Grid.hpp ***/

#include <functional>

namespace tson
{
	class Map;
	class Project;
	class Tileset
	{
		public:
			inline Tileset() = default;
			inline explicit Tileset(IJson &json, tson::Map *map);
			inline bool parse(IJson &json, tson::Map *map);

			[[nodiscard]] inline int getColumns() const;
			[[nodiscard]] inline int getFirstgid() const;

			[[nodiscard]] inline const fs::path &getImagePath() const;
			[[nodiscard]] inline const fs::path &getFullImagePath() const;
			[[nodiscard]] inline const fs::path &getImage() const;
			[[nodiscard]] inline const Vector2i &getImageSize() const;
			[[nodiscard]] inline int getMargin() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline int getSpacing() const;
			[[nodiscard]] inline int getTileCount() const;
			[[nodiscard]] inline const Vector2i &getTileSize() const;
			[[nodiscard]] inline const Colori &getTransparentColor() const;
			[[nodiscard]] inline const std::string& getTypeStr() const;
			[[nodiscard]] inline TilesetType getType() const;
			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */
			[[nodiscard]] inline std::vector<tson::Tile> &getTiles();
			[[nodiscard]] inline const std::vector<tson::WangSet> &getWangsets() const;
			[[nodiscard]] inline PropertyCollection &getProperties();
			[[nodiscard]] inline const std::vector<tson::Terrain> &getTerrains() const;
			[[nodiscard]] inline const Vector2i &getTileOffset() const;
			[[nodiscard]] inline const Grid &getGrid() const;
			[[nodiscard]] inline TileRenderSize getTileRenderSize() const;
			[[nodiscard]] inline FillMode getFillMode() const;

			inline tson::Tile * getTile(uint32_t id);
			inline tson::Terrain * getTerrain(const std::string &name);

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			//v1.2.0-stuff
			[[nodiscard]] inline tson::Map *getMap() const;
			[[nodiscard]] inline ObjectAlignment getObjectAlignment() const;

			inline static tson::ObjectAlignment StringToAlignment(std::string_view str);

			//v1.3.0
			inline tson::Vector2i getMarginSpacingOffset(const tson::Vector2i &posInTileUnits);
			inline tson::WangSet * getWangset(const std::string &name);
			inline const Transformations &getTransformations() const;

			#ifndef TSON_TEST_ENABLED
		private:
			#endif

			[[nodiscard]] inline tson::Project *getProject() const;

			inline void generateMissingTiles();

			int                           m_columns {};       /*! 'columns': The number of tile columns in the tileset */
			int                           m_firstgid {};      /*! 'firstgid': GID corresponding to the first tile in the set */

			fs::path                      m_image;            /*! 'image': Image used for tiles in this set */
			fs::path                      m_imagePath;        /*!  Image path relative to the tileset json of this tileset */

			tson::Vector2i                m_imageSize;        /*! x = 'imagewidth' and y = 'imageheight': in pixels */
			int                           m_margin {};        /*! 'margin': Buffer between image edge and first tile (pixels)*/
			std::string                   m_name;             /*! 'name': Name given to this tileset */
			int                           m_spacing {};       /*! 'spacing': Spacing between adjacent tiles in image (pixels)*/
			int                           m_tileCount {};     /*! 'tilecount': The number of tiles in this tileset */
			tson::Vector2i                m_tileSize;         /*! x = 'tilewidth' and y = 'tileheight': Maximum size of tiles in this set */
			tson::Colori                  m_transparentColor; /*! 'transparentcolor': Hex-formatted color (#RRGGBB) (optional) */
			std::string                   m_typeStr;          /*! 'type': tileset (for tileset files, since 1.0) */
			TilesetType                   m_type{ TilesetType::Undefined }; /*! Tileset type as enum */

			std::vector<tson::Tile>       m_tiles;            /*! 'tiles': Array of Tiles (optional) */
			std::vector<tson::WangSet>    m_wangsets;         /*! 'wangsets':Array of Wang sets (since 1.1.5) */
			tson::PropertyCollection      m_properties; 	  /*! 'properties': A list of properties (name, value, type). */

			std::vector<tson::Terrain>    m_terrains;         /*! 'terrains': Array of Terrains (optional) */
			tson::Vector2i                m_tileOffset;       /*! 'x' and 'y': See <tileoffset> (optional) */
			tson::Grid                    m_grid;             /*! 'grid': This element is only used in case of isometric orientation, and determines
																   how tile overlays for terrain and collision information are rendered. */

			//v1.2.0-stuff
			tson::ObjectAlignment         m_objectAlignment{tson::ObjectAlignment::Unspecified};  /*! 'objectalignment': Alignment to use for tile objects. Tiled 1.4.*/
			tson::Map *                   m_map;              /*! The map who owns this tileset */

			//v1.3.0-stuff
			fs::path                      m_source {};           /*! 'source': exists only when tileset is contained in an external file*/
			fs::path                      m_path {};             /*! Has the full path to the tileset if 'source' has an existing value */
			Transformations               m_transformations {};  /*! New in Tiled v1.5 - This element is used to describe which transformations can be applied to
																	 the tiles (e.g. to extend a Wang set by transforming existing tiles).*/

			//v1.4.0-stuff
			TileRenderSize                m_tileRenderSize {};   /*! 'tilerendersize': The size to use when rendering tiles from this tileset on a tile layer. Valid values are 'tile' (the default) and 'grid'.
 *                                                                    When set to 'grid', the tile is drawn at the tile grid size of the map. (since 1.9)*/
			FillMode                      m_fillMode {};         /*! 'fillmode': The fill mode to use when rendering tiles from this tileset. Valid values are 'stretch' (the default) and 'preserve-aspect-fit'.
 *                                                                    Only relevant when the tiles are not rendered at their native size, so this applies to resized tile objects or in combination with 'tilerendersize' set to 'grid'. (since 1.9)*/

			std::string                   m_classType {};              /*! 'class': The class of this map (since 1.9, defaults to “”). */
			std::shared_ptr<tson::TiledClass> m_class {};
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::Tileset::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

tson::Tileset::Tileset(IJson &json, tson::Map *map)
{
	parse(json, map);
}

bool tson::Tileset::parse(IJson &json, tson::Map *map)
{
	m_map = map;
	bool allFound = true;

	if(json.count("firstgid") > 0) m_firstgid = json["firstgid"].get<int>(); else allFound = false;

	//Tileset is stored in external file if 'source' exists
	if(json.count("source") > 0)
	{
		if(!allFound)
			return allFound;

		std::string sourceStr = json["source"].get<std::string>();
		m_source = fs::path(sourceStr);
		m_path = json.directory() / m_source;

		if(!json.parse(m_path))
			return false;
	}

	if (json.count("columns") > 0)
	{
		m_columns = json["columns"].get<int>();
		if (m_columns == 0)
		{
			m_type = TilesetType::ImageCollectionTileset;
		}
		else if (m_columns > 0)
		{
			m_type = TilesetType::ImageTileset;
		}
	}
	else
	{
		allFound = false;
	}

	// Image tileset specific properties
	if (m_type == TilesetType::ImageTileset)
	{
		if (json.count("image") > 0)
		{
			m_image = fs::path(json["image"].get<std::string>());
			m_imagePath = m_path.parent_path() / m_image;
		}
		else
		{
			allFound = false;
		}

		if (json.count("imagewidth") > 0 && json.count("imageheight") > 0)
		{
			m_imageSize = { json["imagewidth"].get<int>(), json["imageheight"].get<int>() };
		}
		else
		{
			allFound = false;
		}
	}

	if(json.count("margin") > 0) m_margin = json["margin"].get<int>(); else allFound = false;
	if(json.count("name") > 0) m_name = json["name"].get<std::string>(); else allFound = false;
	if(json.count("spacing") > 0) m_spacing = json["spacing"].get<int>(); else allFound = false;
	if(json.count("tilecount") > 0) m_tileCount = json["tilecount"].get<int>(); else allFound = false;
	if(json.count("transparentcolor") > 0) m_transparentColor = tson::Colori(json["transparentcolor"].get<std::string>()); //Optional
	if (json.count("type") > 0) m_typeStr = json["type"].get<std::string>();
	if(json.count("grid") > 0) m_grid = tson::Grid(json["grid"]);
	if(json.count("class") > 0) m_classType = json["class"].get<std::string>();                     //Optional

	if(json.count("tilewidth") > 0 && json.count("tileheight") > 0)
		m_tileSize = {json["tilewidth"].get<int>(), json["tileheight"].get<int>()}; else allFound = false;
	if(json.count("tileoffset") > 0)
		m_tileOffset = {json["tileoffset"]["x"].get<int>(), json["tileoffset"]["y"].get<int>()};

	if(json.count("tilerendersize") > 0)
	{
		std::string tileRenderStr = json["tilerendersize"].get<std::string>();
		if(tileRenderStr == "tile") m_tileRenderSize = TileRenderSize::Tile;
		else if(tileRenderStr == "grid") m_tileRenderSize = TileRenderSize::Grid;
	}

	if(json.count("fillmode") > 0)
	{
		std::string fillmode = json["fillmode"].get<std::string>();
		if(fillmode == "stretch") m_fillMode = FillMode::Stretch;
		else if(fillmode == "preserve-aspect-fit") m_fillMode = FillMode::PreserveAspectFit;
	}

	//More advanced data
	if(json.count("wangsets") > 0 && json["wangsets"].isArray())
	{
		auto &wangsets = json.array("wangsets");
		std::for_each(wangsets.begin(), wangsets.end(), [&](std::unique_ptr<IJson> &item) { m_wangsets.emplace_back(*item, m_map); });
	}
	if(json.count("tiles") > 0 && json["tiles"].isArray())
	{
		auto &tiles = json.array("tiles");
		std::for_each(tiles.begin(), tiles.end(), [&](std::unique_ptr<IJson> &item) { m_tiles.emplace_back(*item, this, m_map); });
	}
	if(json.count("terrains") > 0 && json["terrains"].isArray())
	{
		auto &terrains = json.array("terrains");
		std::for_each(terrains.begin(), terrains.end(), [&](std::unique_ptr<IJson> &item) { m_terrains.emplace_back(*item); });
	}

	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item, getProject()); });
	}

	if(json.count("objectalignment") > 0)
	{
		std::string alignment = json["objectalignment"].get<std::string>();
		m_objectAlignment = StringToAlignment(alignment);
	}

	if(json.count("transformations") > 0)
	{
		m_transformations.parse(json["transformations"]);
	}

	generateMissingTiles();

	return allFound;
}

/*!
 * 'columns': The number of tile columns in the tileset
 * @return
 */
int tson::Tileset::getColumns() const
{
	return m_columns;
}

/*!
 * 'firstgid': GID corresponding to the first tile in the set
 * @return
 */
int tson::Tileset::getFirstgid() const
{
	return m_firstgid;
}

/*!
 * 'image': Image used for tiles in this set
 * @return
 */
const fs::path &tson::Tileset::getImagePath() const { return m_image; }

/*!
 * Gets a path to the image used in this tileset relative to the JSON
 * file defining this tileset.
 * @return A path that can be used to open the image file of this tileset.
 */
const fs::path &tson::Tileset::getFullImagePath() const { return m_imagePath; }

/*!
 * x = 'imagewidth' and y = 'imageheight': in pixels
 * @return
 */
const tson::Vector2i &tson::Tileset::getImageSize() const
{
	return m_imageSize;
}

/*!
 * 'margin': Buffer between image edge and first tile (pixels)
 * @return
 */
int tson::Tileset::getMargin() const
{
	return m_margin;
}

/*!
 * 'name': Name given to this tileset
 * @return
 */
const std::string &tson::Tileset::getName() const
{
	return m_name;
}

/*!
 * 'spacing': Spacing between adjacent tiles in image (pixels)
 * @return
 */
int tson::Tileset::getSpacing() const
{
	return m_spacing;
}

/*!
 * 'tilecount': The number of tiles in this tileset
 * @return
 */
int tson::Tileset::getTileCount() const
{
	return m_tileCount;
}

/*!
 * x = 'tilewidth' and y = 'tileheight': Maximum size of tiles in this set
 * @return
 */
const tson::Vector2i &tson::Tileset::getTileSize() const
{
	return m_tileSize;
}

/*!
 * 'transparentcolor': Color object created by hex-formatted color (#RRGGBB) (optional)
 * @return
 */
const tson::Colori &tson::Tileset::getTransparentColor() const
{
	return m_transparentColor;
}

/*!
 * 'type': tileset (for tileset files, since 1.0)
 * @return
 */
const std::string& tson::Tileset::getTypeStr() const
{
	return m_typeStr;
}

/*!
 * 'type': Tileset type as enum
 * @return
*/
tson::TilesetType tson::Tileset::getType() const
{
	return m_type;
}

/*!
 * 'image': Image used for tiles in this set
 * @return
 */

const fs::path &tson::Tileset::getImage() const { return m_image; }

/*!
 * 'tiles': Array of Tiles (optional)
 * @return
 */
std::vector<tson::Tile> &tson::Tileset::getTiles()
{
	return m_tiles;
}

/*!
 * 'wangsets':Array of Wang sets (since Tiled 1.1.5)
 * @return
 */
const std::vector<tson::WangSet> &tson::Tileset::getWangsets() const
{
	return m_wangsets;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::Tileset::getProperties()
{
	return m_properties;
}

/*!
 * 'terrains': Array of Terrains (optional)
 * @return
 */
const std::vector<tson::Terrain> &tson::Tileset::getTerrains() const
{
	return m_terrains;
}

/*!
 * 'x' and 'y': See <tileoffset> (optional)
 * @return
 */
const tson::Vector2i &tson::Tileset::getTileOffset() const
{
	return m_tileOffset;
}

/*!
 * 'grid': This element is only used in case of isometric orientation, and determines
 * how tile overlays for terrain and collision information are rendered.
 * @return
 */
const tson::Grid &tson::Tileset::getGrid() const
{
	return m_grid;
}

/*!
 * Gets a tile by ID (Tiled ID + 1)
 * @param id The ID of the tile stored in Tiled map + 1. Example: If ID was stored in Tiled map as 0, the corresponding value in Tileson is 1.
 * This is to make sure the IDs of tiles matches their references in containers.
 * @return A pointer to the Tile if found. nullptr otherwise.
 */
tson::Tile *tson::Tileset::getTile(uint32_t id)
{
	auto result = std::find_if(m_tiles.begin(), m_tiles.end(), [&](const tson::Tile & item) { return item.getId() == id;});
	if(result == m_tiles.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Get an existing Terrain object by name
 * @param name
 * @return A pointer to the Terrain if found. nullptr otherwise.
 */
tson::Terrain *tson::Tileset::getTerrain(const std::string &name)
{
	auto result = std::find_if(m_terrains.begin(), m_terrains.end(), [&](const tson::Terrain & item) { return item.getName() == name;});
	if(result == m_terrains.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Tileset::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);

	return nullptr;
}

/*!
 * Tiled only has tiles with a property stored in the map. This function makes sure even the ones with no properties will exist.
 */
void tson::Tileset::generateMissingTiles()
{
	std::vector<uint32_t> tileIds;
	for(auto &tile : m_tiles)
		tileIds.push_back(tile.getId());

	for (uint32_t i = 1; i < (uint32_t)m_tileCount + 1; ++i)
	{
		if (std::count(tileIds.begin(), tileIds.end(), i) == 0)
		{
			m_tiles.emplace_back(Tile::CreateMissingTile(i, this, m_map));
		}
	}
}

/*!
 * Used for getting the tson::Map who is the parent of this Tileset.
 * @return a pointer to the tson::Map where this tileset is contained.
 */
tson::Map *tson::Tileset::getMap() const
{
	return m_map;
}

/*!
 *
 * @param str The string you want to convert
 * @return Alignment enum based on the string from the input.
 */
tson::ObjectAlignment tson::Tileset::StringToAlignment(std::string_view str)
{
	if(str == "unspecified") return tson::ObjectAlignment::Unspecified;
	else if(str == "topleft") return tson::ObjectAlignment::TopLeft;
	else if(str == "top") return tson::ObjectAlignment::Top;
	else if(str == "topright") return tson::ObjectAlignment::TopRight;
	else if(str == "left") return tson::ObjectAlignment::Left;
	else if(str == "center") return tson::ObjectAlignment::Center;
	else if(str == "right") return tson::ObjectAlignment::Right;
	else if(str == "bottomleft") return tson::ObjectAlignment::BottomLeft;
	else if(str == "bottom") return tson::ObjectAlignment::Bottom;
	else if(str == "bottomright") return tson::ObjectAlignment::BottomRight;
	else
		return tson::ObjectAlignment::Unspecified;
}

tson::ObjectAlignment tson::Tileset::getObjectAlignment() const
{
	return m_objectAlignment;
}

/*!
 * Helper function to calculate the correct additional offset when margin and/or spacing is used in a tileset
 * Created to solve issue #31.
 *
 * @param posInTileUnits Position of the current tile in tile units.
 * @return Calculated additional offset in pixels.
 */
tson::Vector2i tson::Tileset::getMarginSpacingOffset(const tson::Vector2i &posInTileUnits)
{
	if(m_margin == 0 && m_spacing == 0)
		return {0,0};

	tson::Vector2i offset {(posInTileUnits.x * m_spacing) + m_margin, (posInTileUnits.y * m_spacing) + m_margin};
	return offset;
}

/*!
 * Get a wangset by name
 * @param name
 * @return
 */
tson::WangSet *tson::Tileset::getWangset(const std::string &name)
{
	auto wangset = std::find_if(m_wangsets.begin(), m_wangsets.end(), [&](const auto &w) { return w.getName() == name; });

	if(wangset != m_wangsets.end())
		return &wangset.operator*();

	return nullptr;
}

/*!
 * New in Tiled v1.5 - This element is used to describe which transformations can be applied to
 * the tiles (e.g. to extend a Wang set by transforming existing tiles).
 *
 * @return
 */
const tson::Transformations &tson::Tileset::getTransformations() const
{
	return m_transformations;
}

tson::TileRenderSize tson::Tileset::getTileRenderSize() const
{
	return m_tileRenderSize;
}

tson::FillMode tson::Tileset::getFillMode() const
{
	return m_fillMode;
}

const std::string &tson::Tileset::getClassType() const
{
	return m_classType;
}

#endif //TILESON_TILESET_HPP
/*** End of inlined file: Tileset.hpp ***/

namespace tson
{
	using LinkedFileParser = std::function<std::unique_ptr<IJson>(std::string relativePath)>;
	class Object;
	class Map
	{
		friend class Object;
		public:
			inline Map() = default;
			inline Map(ParseStatus status, std::string description);
			inline explicit Map(IJson &json, tson::DecompressorContainer *decompressors, tson::Project *project, tson::LinkedFileParser linkedFileParser = nullptr);
			inline bool parse(IJson &json, tson::DecompressorContainer *decompressors, tson::Project *project, tson::LinkedFileParser linkedFileParser = nullptr);

			[[nodiscard]] inline const Colori &getBackgroundColor() const;
			[[nodiscard]] inline const Vector2i &getSize() const;
			[[nodiscard]] inline int getHexsideLength() const;
			[[nodiscard]] inline bool isInfinite() const;
			[[nodiscard]] inline int getNextLayerId() const;
			[[nodiscard]] inline int getNextObjectId() const;
			[[nodiscard]] inline const std::string &getOrientation() const;
			[[nodiscard]] inline const std::string &getRenderOrder() const;
			[[nodiscard]] inline const std::string &getStaggerAxis() const;
			[[nodiscard]] inline const std::string &getStaggerIndex() const;
			[[nodiscard]] inline const std::string &getTiledVersion() const;
			[[nodiscard]] inline const Vector2i &getTileSize() const;
			[[nodiscard]] inline const std::string &getType() const;
			[[nodiscard]] inline const std::string &getClassType() const;
			[[nodiscard]] inline tson::TiledClass *getClass(); /*! Declared in tileson_forward.hpp */
			[[nodiscard]] inline const Vector2f &getParallaxOrigin() const;
			//[[nodiscard]] inline int getVersion() const; //Removed - Tileson v1.3.0

			[[nodiscard]] inline std::vector<tson::Layer> &getLayers();
			[[nodiscard]] inline PropertyCollection &getProperties();
			[[nodiscard]] inline std::vector<tson::Tileset> &getTilesets();

			[[nodiscard]] inline ParseStatus getStatus() const;
			[[nodiscard]] inline const std::string &getStatusMessage() const;
			[[nodiscard]] inline const std::map<uint32_t, tson::Tile *> &getTileMap() const;

			inline Layer * getLayer(const std::string &name);
			inline Tileset * getTileset(const std::string &name);

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property * getProp(const std::string &name);

			//v1.2.0
			[[nodiscard]] inline int getCompressionLevel() const;
			inline DecompressorContainer *getDecompressors();
			inline Project * getProject();
			inline Tileset * getTilesetByGid(uint32_t gid);

		private:
			inline IJson* parseLinkedFile(const std::string& path);
			inline bool createTilesetData(IJson &json);
			inline void processData();

			Colori                                 m_backgroundColor;   /*! 'backgroundcolor': Hex-formatted color (#RRGGBB or #AARRGGBB) (optional)*/;
			Vector2i                               m_size;              /*! 'width' and 'height' of a Tiled map */
			int                                    m_hexsideLength {};  /*! 'hexsidelength': Length of the side of a hex tile in pixels */
			bool                                   m_isInfinite {};     /*! 'infinite': Whether the map has infinite dimensions*/
			std::vector<tson::Layer>               m_layers; 	        /*! 'layers': Array of layers. group on */
			int                                    m_nextLayerId {};    /*! 'nextlayerid': Auto-increments for each layer */
			int                                    m_nextObjectId {};   /*! 'nextobjectid': Auto-increments for each placed object */
			std::string                            m_orientation;       /*! 'orientation': orthogonal, isometric, staggered or hexagonal */
			tson::PropertyCollection               m_properties; 	    /*! 'properties': A list of properties (name, value, type). */
			std::string                            m_renderOrder;       /*! 'renderorder': Rendering direction (orthogonal maps only) */
			std::string                            m_staggerAxis;       /*! 'staggeraxis': x or y (staggered / hexagonal maps only) */
			std::string                            m_staggerIndex;      /*! 'staggerindex': odd or even (staggered / hexagonal maps only) */
			std::string                            m_tiledVersion;      /*! 'tiledversion': The Tiled version used to save the file */
			Vector2i                               m_tileSize;          /*! 'tilewidth': and 'tileheight' of a map */
			std::vector<tson::Tileset>             m_tilesets;          /*! 'tilesets': Array of Tilesets */
			std::string                            m_type;              /*! 'type': map (since 1.0) */
			tson::Vector2f                         m_parallaxOrigin;    /*! Tiled v1.8: parallax origin in pixels. Defaults to 0. */
			//int                                    m_version{};       /*! 'version': The JSON format version - Removed in Tileson v1.3.0*/

			ParseStatus                            m_status {ParseStatus::OK};
			std::string                            m_statusMessage {"OK"};

			std::map<uint32_t, tson::Tile*>        m_tileMap{};           /*! key: Tile ID. Value: Pointer to Tile*/

			//v1.2.0
			int                                    m_compressionLevel {-1};  /*! 'compressionlevel': The compression level to use for tile layer
																			  *     data (defaults to -1, which means to use the algorithm default)
																			  *     Introduced in Tiled 1.3*/
			tson::DecompressorContainer *          m_decompressors {nullptr};
			tson::Project *                        m_project {nullptr};
			std::map<uint32_t, tson::Tile>         m_flaggedTileMap{};    /*! key: Tile ID. Value: Tile*/

			std::string                            m_classType{};              /*! 'class': The class of this map (since 1.9, defaults to “”). */
			std::shared_ptr<tson::TiledClass>      m_class {};

			tson::LinkedFileParser                 m_linkedFileParser;     /*! callback function to parse linked files */
			std::map<std::string, std::unique_ptr<IJson>> m_linkedFiles;  /*! key: relative path to linked file. Value: Pointer to loaded JSON */
	};

	/*!
	 * A shortcut for getting a property. Alternative to getProperties().getValue<T>("<name>")
	 * @tparam T The template value
	 * @param name Name of the property
	 * @return The actual value, if it exists. Otherwise: The default value of the type.
	 */
	template<typename T>
	T tson::Map::get(const std::string &name)
	{
		return m_properties.getValue<T>(name);
	}
}

/*!
 * When errors have happened before the map starts parsing, just keep the statuses
 * @param status The status
 * @param description Description of the status
 */
tson::Map::Map(tson::ParseStatus status, std::string description) : m_status {status}, m_statusMessage { std::move(description) }
{

}

/*!
 * Parses a json of a Tiled map.
 * @param json A json object with the format of Map
 * @param linkedFileParser A callback function that must return a IJson object when a linked file is found in the map
 * @return true if all mandatory fields was found. false otherwise.
 */
tson::Map::Map(IJson &json, tson::DecompressorContainer *decompressors, tson::Project *project, tson::LinkedFileParser linkedFileParser)
{
	parse(json, decompressors, project, linkedFileParser);
}

/*!
 * Parses a json of a Tiled map.
 * @param json A json object with the format of Map
 * @param linkedFileParser A callback function that must return a IJson object when a linked file is found in the map
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Map::parse(IJson &json, tson::DecompressorContainer *decompressors, tson::Project *project, tson::LinkedFileParser linkedFileParser)
{
	m_decompressors = decompressors;
	m_project = project;
	m_linkedFileParser = linkedFileParser;

	if(!m_linkedFileParser)
	{  // build a default linked file parser out of processing relative paths to
	   // the main json's location.
		m_linkedFileParser = [&json](std::string relativePath) -> std::unique_ptr<IJson>
		{
			if(json.directory().empty())
				return nullptr;

			std::unique_ptr<IJson> linkedFileJson = json.create();
			bool parseOk = linkedFileJson->parse(json.directory() / relativePath);
			if(parseOk)
				return linkedFileJson;
			else
				return nullptr;
		};
	}

	bool allFound = true;
	if(json.count("compressionlevel") > 0)
		m_compressionLevel = json["compressionlevel"].get<int>(); //Tiled 1.3 - Optional

	if(json.count("backgroundcolor") > 0) m_backgroundColor = Colori(json["backgroundcolor"].get<std::string>()); //Optional
	if(json.count("width") > 0 && json.count("height") > 0 )
		m_size = {json["width"].get<int>(), json["height"].get<int>()}; else allFound = false;
	if(json.count("hexsidelength") > 0) m_hexsideLength = json["hexsidelength"].get<int>();         //Optional
	if(json.count("infinite") > 0) m_isInfinite = json["infinite"].get<bool>();                     //Optional
	if(json.count("nextlayerid") > 0) m_nextLayerId = json["nextlayerid"].get<int>();               //Optional
	if(json.count("nextobjectid") > 0) m_nextObjectId = json["nextobjectid"].get<int>(); else allFound = false;
	if(json.count("orientation") > 0) m_orientation = json["orientation"].get<std::string>(); else allFound = false;
	if(json.count("renderorder") > 0) m_renderOrder = json["renderorder"].get<std::string>();       //Optional
	if(json.count("staggeraxis") > 0) m_staggerAxis = json["staggeraxis"].get<std::string>();       //Optional
	if(json.count("staggerindex") > 0) m_staggerIndex = json["staggerindex"].get<std::string>();    //Optional
	if(json.count("tiledversion") > 0) m_tiledVersion = json["tiledversion"].get<std::string>(); else allFound = false;
	if(json.count("tilewidth") > 0 && json.count("tileheight") > 0 )
		m_tileSize = {json["tilewidth"].get<int>(), json["tileheight"].get<int>()}; else allFound = false;
	if(json.count("type") > 0) m_type = json["type"].get<std::string>();                            //Optional
	if(json.count("class") > 0) m_classType = json["class"].get<std::string>();                     //Optional

	//Removed - Changed from a float to string in Tiled v1.6, and old spec said int.
	//Reason for removal is that it seems to have no real use, as TiledVersion is stored in another variable.
	//if(json.count("version") > 0) m_version = json["version"].get<int>(); else allFound = false;

	//More advanced data
	if(json.count("layers") > 0 && json["layers"].isArray())
	{
		auto &array = json.array("layers");
		std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item)
		{
			m_layers.emplace_back(*item, this);
		});
	}

	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &array = json.array("properties");
		std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item)
		{
			m_properties.add(*item, m_project);
		});
	}

	tson::Vector2f parallaxOrigin {0.f, 0.f};
	if(json.count("parallaxoriginx") > 0)
		parallaxOrigin.x = json["parallaxoriginx"].get<float>();
	if(json.count("parallaxoriginy") > 0)
		parallaxOrigin.y = json["parallaxoriginy"].get<float>();

	m_parallaxOrigin = parallaxOrigin;

	if(!createTilesetData(json))
		allFound = false;

	processData();

	m_linkedFiles.clear(); // close all open linked json files

	return allFound;
}

/*!
 * Attempts to parse a linked file found in the map.
 * @param relativePath Path to the linked file, relative to the map file.
 * @return a IJson pointer to the parsed file.
 */
tson::IJson* tson::Map::parseLinkedFile(const std::string& relativePath)
{
	auto it = m_linkedFiles.find(relativePath);
	if(it == m_linkedFiles.end())
	{
		if (!m_linkedFileParser) return nullptr;
		std::unique_ptr<IJson> linkedFileJson = m_linkedFileParser(relativePath);
		if(!linkedFileJson)
			return nullptr;

		auto result = m_linkedFiles.emplace(relativePath, std::move(linkedFileJson));
		return result.first->second.get();
	}
	else return it->second.get();
}

/*!
 * Tileset data must be created in two steps to prevent malformed tson::Tileset pointers inside tson::Tile
 */
bool tson::Map::createTilesetData(IJson &json)
{
	bool ok = true;
	if(json.count("tilesets") > 0 && json["tilesets"].isArray())
	{
		//First created tileset objects
		auto &tilesets = json.array("tilesets");
		std::for_each(tilesets.begin(), tilesets.end(), [&](std::unique_ptr<IJson> &)
		{
			m_tilesets.emplace_back();
		});

		int i = 0;
		//Then do the parsing
		std::for_each(tilesets.begin(), tilesets.end(), [&](std::unique_ptr<IJson> &item)
		{
			item->directory(json.directory());
			if(!m_tilesets[i].parse(*item, this))
				ok = false;

			++i;
		});
	}
	return ok;
}

/*!
 * Processes the parsed data and uses the data to create helpful objects, like tile maps.
 */
void tson::Map::processData()
{
	m_tileMap.clear();
	for(auto &tileset : m_tilesets)
	{
		  std::set<std::uint32_t> usedIds;
		  for(auto& tile : tileset.getTiles())
		  {
			  if (usedIds.count(tile.getGid()) != 0)
			  {
				  continue;
			  }
			  usedIds.insert(tile.getGid());
			  m_tileMap[tile.getGid()] = &tile;
		  }
	}
	std::for_each(m_layers.begin(), m_layers.end(), [&](tson::Layer &layer)
	{
		layer.assignTileMap(&m_tileMap);
		layer.createTileData(m_size, m_isInfinite);
		const std::set<uint32_t> &flaggedTiles = layer.getUniqueFlaggedTiles();
		for(uint32_t ftile : flaggedTiles)
		{
			tson::Tile tile = tson::Tile::CreateFlippedTile(ftile, layer.getMap());
			if(m_tileMap.count(tile.getGid()))
			{
				tson::Tile *originalTile = m_tileMap[tile.getGid()];
				tile.addTilesetAndPerformCalculations(originalTile->getTileset());
				tile.setProperties(originalTile->getProperties());
				m_flaggedTileMap[ftile] = tile;
				m_tileMap[ftile] = &m_flaggedTileMap[ftile];
			}
		}
		layer.resolveFlaggedTiles();
	});
}

/*!
 * 'backgroundcolor': Color created from a hex-formatted color string (#RRGGBB or #AARRGGBB) (optional)
 * @return string as color
 */
const tson::Colori &tson::Map::getBackgroundColor() const
{
	return m_backgroundColor;
}

/*!
 * 'width' and 'height' of a Tiled map
 * @return
 */
const tson::Vector2<int> &tson::Map::getSize() const
{
	return m_size;
}

/*!
 * 'hexsidelength': Length of the side of a hex tile in pixels
 * @return
 */
int tson::Map::getHexsideLength() const
{
	return m_hexsideLength;
}

/*!
 * 'infinite': Whether the map has infinite dimensions
 * @return
 */
bool tson::Map::isInfinite() const
{
	return m_isInfinite;
}

/*!
 * 'nextlayerid': Auto-increments for each layer
 * @return
 */
int tson::Map::getNextLayerId() const
{
	return m_nextLayerId;
}

/*!
 * 'nextobjectid': Auto-increments for each placed object
 * @return
 */
int tson::Map::getNextObjectId() const
{
	return m_nextObjectId;
}

/*!
 * 'orientation': orthogonal, isometric, staggered or hexagonal
 * @return
 */
const std::string &tson::Map::getOrientation() const
{
	return m_orientation;
}

/*!
 * 'renderorder': Rendering direction (orthogonal maps only)
 * @return
 */
const std::string &tson::Map::getRenderOrder() const
{
	return m_renderOrder;
}

/*!
 * 'staggeraxis': x or y (staggered / hexagonal maps only)
 * @return
 */
const std::string &tson::Map::getStaggerAxis() const
{
	return m_staggerAxis;
}

/*!
 * 'staggerindex': odd or even (staggered / hexagonal maps only)
 * @return
 */
const std::string &tson::Map::getStaggerIndex() const
{
	return m_staggerIndex;
}

/*!
 * 'tiledversion': The Tiled version used to save the file
 * @return
 */
const std::string &tson::Map::getTiledVersion() const
{
	return m_tiledVersion;
}

/*!
 * 'tilewidth': and 'tileheight' of a map
 * @return
 */
const tson::Vector2<int> &tson::Map::getTileSize() const
{
	return m_tileSize;
}

/*!
 * 'type': map (since 1.0)
 * @return
 */
const std::string &tson::Map::getType() const
{
	return m_type;
}

/*!
 * 'version': The JSON format version
 * @return
 */
//int tson::Map::getVersion() const
//{
//    return m_version;
//}

/*!
 * 'layers': Array of layers. group on
 * @return
 */
std::vector<tson::Layer> &tson::Map::getLayers()
{
	return m_layers;
}

/*!
 * 'properties': A list of properties (name, value, type).
 * @return
 */
tson::PropertyCollection &tson::Map::getProperties()
{
	return m_properties;
}

/*!
 * 'tilesets': Array of Tilesets
 * @return
 */
std::vector<tson::Tileset> &tson::Map::getTilesets()
{
	return m_tilesets;
}

tson::Layer *tson::Map::getLayer(const std::string &name)
{
	auto result = std::find_if(m_layers.begin(), m_layers.end(), [&](const tson::Layer &item) { return item.getName() == name; });
	if(result == m_layers.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Gets a tileset by name
 *
 * @param name Name of the tileset
 * @return tileset with the matching name
 */
tson::Tileset *tson::Map::getTileset(const std::string &name)
{
	auto result = std::find_if(m_tilesets.begin(), m_tilesets.end(), [&](const tson::Tileset &item) {return item.getName() == name; });
	if(result == m_tilesets.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Gets a tileset by gid (graphical ID of a tile). These are always unique, no matter how many tilesets you have
 *
 * @param gid Graphical ID of a tile
 * @return tileset related to the actual gid
 */
tson::Tileset *tson::Map::getTilesetByGid(uint32_t gid)
{
	auto result = std::find_if(m_tilesets.begin(), m_tilesets.end(), [&](const tson::Tileset &tileset)
	{
		auto const firstId = static_cast<uint32_t>(tileset.getFirstgid()); //First tile id of the tileset
		auto const lastId =  static_cast<uint32_t>((firstId + tileset.getTileCount()) - 1);

		return (gid >= firstId && gid <= lastId);
	});
	if(result == m_tilesets.end())
		return nullptr;

	return &result.operator*();
}

/*!
 * Shortcut for getting a property object. Alternative to getProperties().getProperty("<name>");
 * @param name Name of the property
 * @return
 */
tson::Property *tson::Map::getProp(const std::string &name)
{
	if(m_properties.hasProperty(name))
		return m_properties.getProperty(name);
	return nullptr;
}

tson::ParseStatus tson::Map::getStatus() const
{
	return m_status;
}

const std::string &tson::Map::getStatusMessage() const
{
	return m_statusMessage;
}

/*!
 * Get a tile map with pointers to every existing tile.
 * @return
 */
const std::map<uint32_t, tson::Tile *> &tson::Map::getTileMap() const
{
	return m_tileMap;
}

tson::DecompressorContainer *tson::Map::getDecompressors()
{
	return m_decompressors;
}

/*!
 * 'compressionlevel': The compression level to use for tile layer data (defaults to -1, which means to use the algorithm default)
 *
 * @return The compression level
 */
int tson::Map::getCompressionLevel() const
{
	return m_compressionLevel;
}

/*!
 * New in Tiled v1.8
 * Gets the parallax origin in pixels. Defaults to 0.
 * @return A vector with the x and y values of the parallax origin.
 */
const tson::Vector2f &tson::Map::getParallaxOrigin() const
{
	return m_parallaxOrigin;
}

tson::Project *tson::Map::getProject()
{
	return m_project;
}

const std::string &tson::Map::getClassType() const
{
	return m_classType;
}

#endif //TILESON_MAP_HPP

/*** End of inlined file: Map.hpp ***/


/*** Start of inlined file: TiledEnum.hpp ***/
//
// Created by robin on 06.06.22.
//

#ifndef TILESON_TILEDENUM_HPP
#define TILESON_TILEDENUM_HPP

namespace tson
{
	class EnumDefinition
	{
		public:
			inline explicit EnumDefinition(IJson &json);
			inline uint32_t getValue(const std::string &str);
			inline std::string getValue(uint32_t num);
			inline std::vector<std::string> getValues(uint32_t num);
			inline bool exists(const std::string &str);
			inline bool exists(uint32_t num);

			[[nodiscard]] inline uint32_t getId() const;
			[[nodiscard]] inline uint32_t getMaxValue() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline EnumStorageType getStorageType() const;
			[[nodiscard]] inline bool hasValuesAsFlags() const;

		private:
			inline bool hasFlag(uint32_t value, uint32_t flag) const;
			uint32_t m_id {};
			uint32_t m_maxValue {};
			std::string m_name {};
			std::map<uint32_t, std::string> m_values {};
			bool m_valuesAsFlags {false};
			EnumStorageType m_storageType { EnumStorageType::Unspecified };
	};

	EnumDefinition::EnumDefinition(IJson &json)
	{
		m_id = json.get<uint32_t>("id");
		m_name = json.get<std::string>("name");
		std::string type = json.get<std::string>("storageType");
		m_storageType = (type == "int") ? EnumStorageType::Int : (type == "string") ? EnumStorageType::String : EnumStorageType::Unspecified;
		m_valuesAsFlags = json.get<bool>("valuesAsFlags");

		if(json.count("values") > 0 && json["values"].isArray())
		{
			m_values[0] = "None";
			uint32_t valueCounter = (m_valuesAsFlags) ? 1 : 0;
			uint8_t flagBit = 1;
			auto &array = json.array("values");
			std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item)
			{
				std::string v = item->get<std::string>();
				m_values[valueCounter] = v;
				if(m_valuesAsFlags)
				{
					valueCounter = 1 << flagBit;
					++flagBit;
				}
				else
				{
					++valueCounter;
				}
			});

			m_maxValue = valueCounter;
		}
	}

	uint32_t EnumDefinition::getValue(const std::string &str)
	{
		auto result = std::find_if(m_values.begin(), m_values.end(), [&](const std::pair<uint32_t, std::string> &pair)
		{
			return pair.second == str;
		});

		if(result != m_values.end())
			return result->first;

		return 0;
	}

	std::string EnumDefinition::getValue(uint32_t num)
	{
		return (m_values.count(num) > 0) ? m_values[num] : "";
	}

	bool EnumDefinition::hasValuesAsFlags() const
	{
		return m_valuesAsFlags;
	}

	bool EnumDefinition::exists(const std::string &str)
	{
		auto result = std::find_if(m_values.begin(), m_values.end(), [&](const std::pair<uint32_t, std::string> &pair)
		{
			return pair.second == str;
		});

		if(result != m_values.end())
			return true;

		return false;
	}

	bool EnumDefinition::exists(uint32_t num) { return (m_values.count(num) > 0); }

	uint32_t EnumDefinition::getId() const
	{
		return m_id;
	}

	const std::string &EnumDefinition::getName() const
	{
		return m_name;
	}

	EnumStorageType EnumDefinition::getStorageType() const
	{
		return m_storageType;
	}

	uint32_t EnumDefinition::getMaxValue() const
	{
		return m_maxValue;
	}

	std::vector<std::string> EnumDefinition::getValues(uint32_t num)
	{
		std::vector<std::string> values;
		if(m_valuesAsFlags)
		{
			uint32_t flag = 0;
			uint32_t i = 0;
			while(flag < m_maxValue)
			{
				flag = 1 << i;
				++i;
				if(m_values.count(flag) > 0 && hasFlag(num, flag))
				{
					values.emplace_back(m_values[flag]);
				}
			}
		}
		else
		{
			std::string v = getValue(num);
			if(!v.empty())
				values.emplace_back();
		}

		return values;
	}

	bool EnumDefinition::hasFlag(uint32_t value, uint32_t flag) const
	{
		return ((value & flag) == flag);
	}

	class EnumValue
	{
		public:
			inline EnumValue() = default;
			inline EnumValue(uint32_t value, EnumDefinition *definition);
			inline EnumValue(const std::string &value, EnumDefinition *definition);

			[[nodiscard]] inline uint32_t getValue() const;
			inline std::string getValueName() const;
			[[nodiscard]] inline std::vector<std::string> getValueNames() const;
			[[nodiscard]] inline EnumDefinition *getDefinition() const;

			inline bool hasFlagValue(uint32_t flag) const;
			template <typename T>
			inline bool hasFlag(T flags) const;
			inline bool hasAnyFlagValue(uint32_t flags) const;
			template <typename T>
			inline bool hasAnyFlag(T flags) const;
			[[nodiscard]] inline bool containsValueName(const std::string &value) const;

		private:
			uint32_t m_value {0};
			EnumDefinition *m_definition = nullptr;
	};

	EnumValue::EnumValue(uint32_t value, EnumDefinition *definition) : m_value {value}, m_definition {definition}
	{

	}

	EnumValue::EnumValue(const std::string &value, EnumDefinition *definition) : m_definition {definition}
	{
		if(!value.empty() && definition != nullptr)
		{
			std::vector<std::string> values = Tools::SplitString(value, ',');
			for(auto &item : values)
			{
				uint32_t v = definition->getValue(item);
				m_value |= v;
			}
		}
	}

	/*!
	 * Checks if uint32 value contains a single flag. If 'valuesAsFlags' is not a part of the EnumDefinition, a simple equality comparison will be done
	 * instead.
	 * @param flags The uint32 value of the flag you want to check
	 * @return 'true' if EnumValue has the requested bit activated. 'false' otherwise.
	 */
	bool EnumValue::hasFlagValue(uint32_t flags) const
	{
		if(m_definition->hasValuesAsFlags())
			return ((m_value & flags) == flags) ? true : false;

		return m_value == flags;
	}

	/*!
	 * Checks if uint32 value contains one of several possible flags. If 'valuesAsFlags' is not a part of the EnumDefinition, a simple equality comparison will be done
	 * instead.
	 * @param flags The uint32 values of the flags you want to check
	 * @return 'true' if EnumValue has the requested bits activated. 'false' otherwise.
	 */
	bool EnumValue::hasAnyFlagValue(uint32_t flags) const
	{
		if(m_definition->hasValuesAsFlags())
			return ((m_value & flags) != 0);

		return m_value == flags;
	}

	uint32_t EnumValue::getValue() const
	{
		return m_value;
	}

	/*!
	 * Gets the single name of a value.
	 * This function is intended for enums that can only have one value (non-flag).
	 * If you want to get several value names (flags), use getValueNames() instead.
	 * @return A single name for a value
	 */
	std::string EnumValue::getValueName() const
	{
		return (m_definition == nullptr) ? "" : m_definition->getValue(m_value);
	}

	/*!
	 * Checks if T value contains a single flag. If 'valuesAsFlags' is not a part of the EnumDefinition, a simple equality comparison will be done
	 * instead.
	 * @tparam T A uint32_t compatible type
	 * @param flags One or more flags you want to verify is included.
	 * @return true is all flags presented are set. false otherwise.
	 */
	template<typename T>
	bool EnumValue::hasFlag(T flags) const
	{
		return hasFlagValue(static_cast<uint32_t>(flags));
	}

	/*!
	 * Checks if T value contains one of several possible flags. If 'valuesAsFlags' is not a part of the EnumDefinition, a simple equality comparison will be done
	 * instead.
	 * @tparam T A uint32_t compatible type
	 * @param flags One or more flags you want to verify is included.
	 * @return true is all flags presented are set. false otherwise.
	 */
	template<typename T>
	bool EnumValue::hasAnyFlag(T flags) const
	{
		return hasAnyFlagValue(static_cast<uint32_t>(flags));
	}

	EnumDefinition *EnumValue::getDefinition() const
	{
		return m_definition;
	}

	std::vector<std::string> EnumValue::getValueNames() const
	{
		return (m_definition == nullptr) ? std::vector<std::string>() : m_definition->getValues(m_value);
	}

	/*!
	 *
	 * @param value
	 * @return
	 */
	bool EnumValue::containsValueName(const std::string &value) const
	{
		if(m_definition != nullptr)
		{
			if(m_definition->hasValuesAsFlags())
			{
				std::vector<std::string> values = m_definition->getValues(m_value);
				auto it = std::find(values.begin(), values.end(), value);
				return it != values.end();
			}
			return m_definition->getValue(value) == m_value;
		}
		return false;
	}
}

#endif //TILESON_TILEDENUM_HPP

/*** End of inlined file: TiledEnum.hpp ***/


/*** Start of inlined file: TiledClass.hpp ***/
//
// Created by robin on 06.06.22.
//

#ifndef TILESON_TILEDCLASS_HPP
#define TILESON_TILEDCLASS_HPP

namespace tson
{
	class TiledClass
	{
		public:
			inline explicit TiledClass() = default;
			inline explicit TiledClass(IJson &json, tson::Project *project = nullptr);

			[[nodiscard]] inline uint32_t getId() const;
			[[nodiscard]] inline const std::string &getName() const;
			[[nodiscard]] inline const std::string &getType() const;
			[[nodiscard]] inline PropertyCollection &getMembers();
			inline void update(IJson &json);
			inline void update(PropertyCollection &properties);

			template <typename T>
			inline T get(const std::string &name);
			inline tson::Property *getMember(const std::string &name);

		private:
			uint32_t m_id {};
			std::string m_name {};
			std::string m_type {};
			PropertyCollection m_members {};

	};

	TiledClass::TiledClass(IJson &json, tson::Project *project)
	{
		if(json.count("id") > 0)
			m_id = json["id"].get<uint32_t>();

		if(json.count("name") > 0)
			m_name = json["name"].get<std::string>();
		if(json.count("type") > 0)
			m_type = json["type"].get<std::string>();

		if(json.count("members") > 0 && json["members"].isArray())
		{
			auto &array = json.array("members");
			std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item)
			{
				m_members.add(*item, project);
			});
		}
	}

	uint32_t TiledClass::getId() const
	{
		return m_id;
	}

	const std::string &TiledClass::getName() const
	{
		return m_name;
	}

	const std::string &TiledClass::getType() const
	{
		return m_type;
	}

	PropertyCollection &TiledClass::getMembers()
	{
		return m_members;
	}

	template<typename T>
	T TiledClass::get(const std::string &name)
	{
		return m_members.getValue<T>(name);
	}

	tson::Property *TiledClass::getMember(const std::string &name)
	{
		if(m_members.hasProperty(name))
			return m_members.getProperty(name);
		return nullptr;
	}

	/*!
	 * Takes a json object from a particular map top update values if they differ from the original values of the class
	 * @param json
	 */
	void TiledClass::update(IJson &json)
	{
		for(Property *property : m_members.get())
		{
			if(json.any(property->getName()))
			{
				property->setValueByType(json[property->getName()]);
			}
		}
	}

	void TiledClass::update(PropertyCollection &properties)
	{
		std::vector<Property *> toUpdate;
		for(Property *member : m_members.get())
		{
			if(properties.hasProperty(member->getName()))
			{
				Property *property = properties.getProperty(member->getName());
				if(member->getType() == property->getType())
				{
					toUpdate.push_back(property);
				}
			}
		}

		std::for_each(toUpdate.begin(), toUpdate.end(), [&](Property *p)
		{
		   m_members.setProperty(p->getName(), *p);
		});
	}
}

#endif //TILESON_TILEDCLASS_HPP

/*** End of inlined file: TiledClass.hpp ***/


/*** Start of inlined file: Project.hpp ***/
//
// Created by robin on 01.08.2020.
//

#ifndef TILESON_PROJECT_HPP
#define TILESON_PROJECT_HPP

#include <fstream>
#include <sstream>
#include <memory>

/*** Start of inlined file: World.hpp ***/
//
// Created by robin on 01.08.2020.
//

#ifndef TILESON_WORLD_HPP
#define TILESON_WORLD_HPP


/*** Start of inlined file: WorldMapData.hpp ***/
//
// Created by robin on 01.08.2020.
//

#ifndef TILESON_WORLDMAPDATA_HPP
#define TILESON_WORLDMAPDATA_HPP

namespace tson
{
	class WorldMapData
	{
		public:
			inline WorldMapData(const fs::path &folder_, IJson &json);
			inline void parse(const fs::path &folder_, IJson &json);
			//inline WorldMapData(fs::path folder_, std::string fileName_) : folder {std::move(folder_)}, fileName {fileName_}
			//{
			//    path = folder / fileName;
			//}

			fs::path folder;
			fs::path path;
			std::string fileName;
			tson::Vector2i size;
			tson::Vector2i position;
	};

	WorldMapData::WorldMapData(const fs::path &folder_, IJson &json)
	{
		parse(folder_, json);
	}

	void WorldMapData::parse(const fs::path &folder_, IJson &json)
	{
		folder = folder_;
		if(json.count("fileName") > 0) fileName = json["fileName"].get<std::string>();
		if(json.count("height") > 0) size = {json["width"].get<int>(), json["height"].get<int>()};
		if(json.count("x") > 0) position = {json["x"].get<int>(), json["y"].get<int>()};

		path = (!fileName.empty()) ? folder / fileName : folder;
	}
}

#endif //TILESON_WORLDMAPDATA_HPP
/*** End of inlined file: WorldMapData.hpp ***/

#include <memory>
namespace tson
{
	class Tileson;
	class World
	{
		public:
			#ifdef JSON11_IS_DEFINED
			inline explicit World(std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>()) : m_json {std::move(jsonParser)}
			{
			}

			inline explicit World(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>());
			#else
			inline explicit World(std::unique_ptr<tson::IJson> jsonParser) : m_json {std::move(jsonParser)}
			{
			}

			inline explicit World(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser);
			#endif
			inline bool parse(const fs::path &path);
			inline std::size_t loadMaps(tson::Tileson *parser); //tileson_forward.hpp
			inline bool contains(std::string_view filename);
			inline const WorldMapData *get(std::string_view filename) const;

			[[nodiscard]] inline const fs::path &getPath() const;
			[[nodiscard]] inline const fs::path &getFolder() const;
			[[nodiscard]] inline const std::vector<WorldMapData> &getMapData() const;
			[[nodiscard]] inline bool onlyShowAdjacentMaps() const;
			[[nodiscard]] inline const std::string &getType() const;
			[[nodiscard]] inline const std::vector<std::unique_ptr<tson::Map>> &getMaps() const;

		private:
			inline void parseJson(IJson &json);

			std::unique_ptr<IJson> m_json = nullptr;
			fs::path m_path;
			fs::path m_folder;
			std::vector<WorldMapData> m_mapData;
			std::vector<std::unique_ptr<tson::Map>> m_maps;
			bool m_onlyShowAdjacentMaps;
			std::string m_type;
	};

	World::World(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser) : m_json {std::move(jsonParser)}
	{
		parse(path);
	}

	bool World::parse(const fs::path &path)
	{
		m_path = path;
		m_folder = m_path.parent_path();

		if(!m_json->parse(path))
			return false;

		parseJson(*m_json);
		return true;
	}

	const fs::path &World::getPath() const
	{
		return m_path;
	}

	const std::vector<WorldMapData> &World::getMapData() const
	{
		return m_mapData;
	}

	bool World::onlyShowAdjacentMaps() const
	{
		return m_onlyShowAdjacentMaps;
	}

	const std::string &World::getType() const
	{
		return m_type;
	}

	void World::parseJson(IJson &json)
	{
		if(json.count("onlyShowAdjacentMaps") > 0) m_onlyShowAdjacentMaps = json["onlyShowAdjacentMaps"].get<bool>();
		if(json.count("type") > 0) m_type = json["type"].get<std::string>();

		if(json["maps"].isArray())
		{
			auto &maps = json.array("maps");
			std::for_each(maps.begin(), maps.end(), [&](std::unique_ptr<IJson> &item) { m_mapData.emplace_back(m_folder, *item); });
		}
	}

	const fs::path &World::getFolder() const
	{
		return m_folder;
	}

	/*!
	 * Check if there is WorldMapData in the world that contains the current filename.
	 * Filename = <file>.<extension>
	 * @param filename
	 * @return
	 */
	bool World::contains(std::string_view filename)
	{
		//Note: might be moved to std::ranges from C++20.
		return std::any_of(m_mapData.begin(), m_mapData.end(), [&](const auto &item) { return item.fileName == filename; });
	}

	/*!
	 * Get a map by its filename
	 * @param filename Filename (including extension) - (example: file.json)
	 * @return pointer to WorldMapData or nullptr if not exists
	 */
	const WorldMapData * World::get(std::string_view filename) const
	{
		auto iter = std::find_if(m_mapData.begin(), m_mapData.end(), [&](const auto &item) { return item.fileName == filename; });
		return (iter == m_mapData.end()) ? nullptr : iter.operator->();
	}

	/*!
	 * Get all maps that have been loaded by loadMaps().
	 * NOTE: This is untested, and was a last second addition to Tileson 1.2.0, as I had forgot about the loadMaps() functionality (also untested)
	 * If you find anything malfunctioning - please report.
	 * @return All maps loaded by loadMaps()
	 */
	const std::vector<std::unique_ptr<tson::Map>> &World::getMaps() const
	{
		return m_maps;
	}

}

#endif //TILESON_WORLD_HPP

/*** End of inlined file: World.hpp ***/



/*** Start of inlined file: ProjectPropertyTypes.hpp ***/
//
// Created by robin on 01.08.22.
//

#ifndef TILESON_PROJECTPROPERTYTYPES_HPP
#define TILESON_PROJECTPROPERTYTYPES_HPP

namespace tson
{
	class ProjectPropertyTypes
	{
		public:
			inline ProjectPropertyTypes() = default;
			inline bool parse(IJson &json, tson::Project *project);

			inline const std::vector<tson::EnumDefinition> &getEnums() const;
			inline const std::vector<tson::TiledClass> &getClasses() const;
			[[nodiscard]] inline tson::EnumDefinition* getEnumDefinition(std::string_view name);
			[[nodiscard]] inline tson::TiledClass* getClass(std::string_view name);
			inline bool isUnhandledContentFound() const;

		private:
			std::vector<tson::EnumDefinition> m_enums;
			std::vector<tson::TiledClass> m_classes;
			bool m_unhandledContentFound {false};

	};

	bool ProjectPropertyTypes::parse(IJson &json, tson::Project *project)
	{
		m_enums.clear();
		m_classes.clear();
		m_unhandledContentFound = false;

		if(json.count("propertyTypes") > 0 && json["propertyTypes"].isArray())
		{
			auto &array = json.array("propertyTypes");
			std::vector<tson::IJson*> classes; //Classes must be handled after enums
			std::vector<tson::IJson*> other; //Unhandled stuff - just to keep track if something is missing...
			std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item)
			{
				IJson &j = *item;
				if(j.count("type") > 0)
				{
					std::string t = j["type"].get<std::string>();
					if(t == "enum")
					{
						m_enums.emplace_back(j); //Can be resolved directly
					}
					else if(t == "class")
					{
						classes.push_back(item.get()); //Must be resolved later
					}
					else
						other.push_back(item.get()); //Only used to set flag for whether unhandled content was found.
				}
			});

			std::for_each(classes.begin(), classes.end(), [&](IJson *item)
			{
				m_classes.emplace_back(*item, project);
			});

			if(!other.empty())
				m_unhandledContentFound = true;

		}
		return false;
	}

	const std::vector<tson::EnumDefinition> &ProjectPropertyTypes::getEnums() const
	{
		return m_enums;
	}

	const std::vector<tson::TiledClass> &ProjectPropertyTypes::getClasses() const
	{
		return m_classes;
	}

	bool ProjectPropertyTypes::isUnhandledContentFound() const
	{
		return m_unhandledContentFound;
	}

	tson::EnumDefinition *ProjectPropertyTypes::getEnumDefinition(std::string_view name)
	{
		auto it = std::find_if(m_enums.begin(), m_enums.end(), [&](const EnumDefinition &def)
		{
			return def.getName() == name;
		});

		if(it != m_enums.end())
			return &it.operator*();

		return nullptr;
	}

	tson::TiledClass *ProjectPropertyTypes::getClass(std::string_view name)
	{
		auto it = std::find_if(m_classes.begin(), m_classes.end(), [&](const TiledClass &def)
		{
			return def.getName() == name;
		});

		if(it != m_classes.end())
			return &it.operator*();

		return nullptr;
	}
}

#endif //TILESON_PROJECTPROPERTYTYPES_HPP

/*** End of inlined file: ProjectPropertyTypes.hpp ***/


/*** Start of inlined file: ProjectFolder.hpp ***/
//
// Created by robin on 01.08.2020.
//

#ifndef TILESON_PROJECTFOLDER_HPP
#define TILESON_PROJECTFOLDER_HPP

namespace tson
{
	class ProjectFolder
	{
		public:
			inline ProjectFolder(const fs::path &path);

			inline const fs::path &getPath() const;
			inline bool hasWorldFile() const;
			inline const std::vector<ProjectFolder> &getSubFolders() const;
			inline const std::vector<fs::path> &getFiles() const;
			inline const World &getWorld() const;

		private:
			inline void loadData();
			fs::path                    m_path;
			bool                        m_hasWorldFile;
			tson::World                 m_world;
			std::vector<ProjectFolder>  m_subFolders;
			std::vector<fs::path>       m_files;

	};

	ProjectFolder::ProjectFolder(const fs::path &path) : m_path {path}
	{
		loadData();
	}

	void ProjectFolder::loadData()
	{
		m_hasWorldFile = false;
		m_subFolders.clear();
		m_files.clear();
		//Search and see if there is a World file .world file
		fs::path worldPath;
		for (const auto & entry : fs::directory_iterator(m_path))
		{
			if(fs::is_regular_file(entry.path()))
			{
				if(entry.path().extension() == ".world")
				{
					m_hasWorldFile = true;
					worldPath = entry.path();
				}
			}
		}

		if(m_hasWorldFile)
			m_world.parse(worldPath);

		for (const auto & entry : fs::directory_iterator(m_path))
		{
			if (fs::is_directory(entry.path()))
				m_subFolders.emplace_back(entry.path());//.loadData(); - loadData() is called in the constructor, so don't call again.
			else if (fs::is_regular_file(entry.path()))
			{
				if(m_hasWorldFile && m_world.contains(entry.path().filename().generic_string()))
					m_files.emplace_back(entry.path());
				else if(!m_hasWorldFile)
					m_files.emplace_back(entry.path());
			}
		}

	}

	const fs::path &ProjectFolder::getPath() const
	{
		return m_path;
	}

	bool ProjectFolder::hasWorldFile() const
	{
		return m_hasWorldFile;
	}

	const std::vector<ProjectFolder> &ProjectFolder::getSubFolders() const
	{
		return m_subFolders;
	}

	const std::vector<fs::path> &ProjectFolder::getFiles() const
	{
		return m_files;
	}

	/*!
	 * Only gives useful data if hasWorldFile() is true!
	 * @return
	 */
	const World &ProjectFolder::getWorld() const
	{
		return m_world;
	}
}

#endif //TILESON_PROJECTFOLDER_HPP
/*** End of inlined file: ProjectFolder.hpp ***/


/*** Start of inlined file: ProjectData.hpp ***/
//
// Created by robin on 01.08.2020.
//

#ifndef TILESON_PROJECTDATA_HPP
#define TILESON_PROJECTDATA_HPP

namespace tson
{
	class ProjectData
	{
		public:
			ProjectData() = default;
			std::string automappingRulesFile;
			std::vector<std::string> commands;
			std::string extensionsPath;
			std::vector<std::string> folders;
			std::string objectTypesFile;
			ProjectPropertyTypes projectPropertyTypes;

			//Tileson specific
			fs::path basePath;
			std::vector<tson::ProjectFolder> folderPaths;
	};
}

#endif //TILESON_PROJECTDATA_HPP
/*** End of inlined file: ProjectData.hpp ***/

namespace tson
{
	class Project
	{
		public:
			#ifdef JSON11_IS_DEFINED
			inline explicit Project(std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>()) : m_json {std::move(jsonParser)}
			{

			}
			inline explicit Project(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>());
			#else
			inline explicit Project(std::unique_ptr<tson::IJson> jsonParser) : m_json {std::move(jsonParser)}
			{

			}
			inline explicit Project(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser);
			#endif
			inline bool parse(const fs::path &path);
			inline void parse();

			[[nodiscard]] inline const ProjectData &getData() const;
			[[nodiscard]] inline const fs::path &getPath() const;
			[[nodiscard]] inline const std::vector<ProjectFolder> &getFolders() const;
			[[nodiscard]] inline tson::EnumDefinition* getEnumDefinition(std::string_view name);
			[[nodiscard]] inline tson::TiledClass* getClass(std::string_view name);

		private:
			inline void parseJson(IJson &json);
			fs::path m_path;
			std::vector<ProjectFolder> m_folders;
			ProjectData m_data;
			std::unique_ptr<IJson> m_json = nullptr;
	};

	Project::Project(const fs::path &path, std::unique_ptr<tson::IJson> jsonParser) : m_json {std::move(jsonParser)}
	{
		parse(path);
	}

	bool Project::parse(const fs::path &path)
	{
		m_path = path;
		std::ifstream i(m_path.generic_string());
#ifdef __cpp_exceptions
try
#else
if(true)
#endif
		{
			if(!m_json->parse(path))
				return false;
		}

#ifdef __cpp_exceptions
catch(const std::exception &error)
#else
else if(false)
#endif
{
			std::string message = "Parse error: ";
			message += std::string((
#ifdef __cpp_exceptions
error.what()
#else
"Exception disabled"
#endif
));
			message += std::string("\n");
			return false;
		}
		parseJson(*m_json);
		return true;
	}

	const ProjectData &Project::getData() const
	{
		return m_data;
	}

	void Project::parseJson(IJson &json)
	{
		m_data.basePath = (m_path.empty()) ? fs::path() : m_path.parent_path(); //The directory of the project file

		//Make sure these property types are read before any map is, so they can be resolved.
		if(json.count("propertyTypes") > 0)
		{
			m_data.projectPropertyTypes.parse(json, this);
		}

		if(json.count("automappingRulesFile") > 0) m_data.automappingRulesFile = json["automappingRulesFile"].get<std::string>();
		if(json.count("commands") > 0)
		{
			m_data.commands.clear();
			auto &commands = json.array("commands");
			std::for_each(commands.begin(), commands.end(), [&](std::unique_ptr<IJson> &item)
			{
				m_data.commands.emplace_back(item->get<std::string>());
			});
		}
		if(json.count("extensionsPath") > 0) m_data.extensionsPath = json["extensionsPath"].get<std::string>();
		if(json.count("folders") > 0)
		{
			m_data.folders.clear();
			m_data.folderPaths.clear();
			auto &folders = json.array("folders");
			std::for_each(folders.begin(), folders.end(), [&](std::unique_ptr<IJson> &item)
			{
				std::string folder = item->get<std::string>();
				m_data.folders.emplace_back(folder);
				m_data.folderPaths.emplace_back(m_data.basePath / folder);
				m_folders.emplace_back(m_data.basePath / folder);
			});
		}
		if(json.count("objectTypesFile") > 0) m_data.objectTypesFile = json["objectTypesFile"].get<std::string>();
	}

	const fs::path &Project::getPath() const
	{
		return m_path;
	}

	const std::vector<ProjectFolder> &Project::getFolders() const
	{
		return m_folders;
	}

	tson::EnumDefinition *Project::getEnumDefinition(std::string_view name)
	{
		return m_data.projectPropertyTypes.getEnumDefinition(name);
	}

	tson::TiledClass *Project::getClass(std::string_view name)
	{
		return m_data.projectPropertyTypes.getClass(name);
	}

	/*!
	 * Parses preloaded json data. Only used during tests involving project jsons not actually read from files
	 * @return
	 */
	void Project::parse()
	{
		parseJson(*m_json);
	}

}

#endif //TILESON_PROJECT_HPP

/*** End of inlined file: Project.hpp ***/

namespace tson
{
	class Tileson
	{
		public:
			#ifdef JSON11_IS_DEFINED
			inline explicit Tileson(std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>(), bool includeBase64Decoder = true);
			inline explicit Tileson(tson::Project *project, std::unique_ptr<tson::IJson> jsonParser = std::make_unique<tson::Json11>(), bool includeBase64Decoder = true);
			#else
			inline explicit Tileson(std::unique_ptr<tson::IJson> jsonParser, bool includeBase64Decoder = true);
			inline explicit Tileson(tson::Project *project, std::unique_ptr<tson::IJson> jsonParser, bool includeBase64Decoder = true);
			#endif

			inline std::unique_ptr<tson::Map> parse(const fs::path &path, std::unique_ptr<IDecompressor<std::vector<uint8_t>, std::vector<uint8_t>>> decompressor = nullptr);
			inline std::unique_ptr<tson::Map> parse(const void * data, size_t size, std::unique_ptr<IDecompressor<std::vector<uint8_t>, std::vector<uint8_t>>> decompressor = nullptr);
			inline tson::DecompressorContainer *decompressors();

		private:
			inline std::unique_ptr<tson::Map> parseJson();
			std::unique_ptr<tson::IJson> m_json;
			tson::DecompressorContainer m_decompressors;
			tson::Project *m_project {nullptr};
	};
}

/*!
 *
 * @param includeBase64Decoder Includes the base64-decoder from "Base64Decompressor.hpp" if true.
 * Otherwise no other decompressors/decoders than whatever the user itself have added will be used.
 */
tson::Tileson::Tileson(std::unique_ptr<tson::IJson> jsonParser, bool includeBase64Decoder) : m_json {std::move(jsonParser)}
{
	if(includeBase64Decoder)
		m_decompressors.add<Base64Decompressor>();
}

tson::Tileson::Tileson(tson::Project *project, std::unique_ptr<tson::IJson> jsonParser, bool includeBase64Decoder) : m_json {std::move(jsonParser)}
{
	m_project = project;
	if(includeBase64Decoder)
		m_decompressors.add<Base64Decompressor>();
}

/*!
 * Parses Tiled json map data by file
 * @param path path to file
 * @return parsed data as Map
 */
std::unique_ptr<tson::Map> tson::Tileson::parse(const fs::path &path, std::unique_ptr<IDecompressor<std::vector<uint8_t>, std::vector<uint8_t>>> decompressor)
{

	bool result = false;

	if(decompressor != nullptr)
	{
		std::vector<uint8_t> decompressed = decompressor->decompressFile(path);
		result = (decompressed.empty()) ? false : true;
		if(!result)
			return std::make_unique<tson::Map>(tson::ParseStatus::DecompressionError, "Error during decompression");

		result = m_json->parse(&decompressed[0], decompressed.size());

		if(result)
			return parseJson();
	}
	else if(m_json->parse(path))
	{
		return parseJson();
	}

	std::string msg = "File not found: ";
	msg += std::string(path.generic_string());
	return std::make_unique<tson::Map>(tson::ParseStatus::FileNotFound, msg);
}

/*!
 * Parses Tiled json map data by memory
 * @param data The data to parse
 * @param size The size of the data to parse
 * @return parsed data as Map
 */
std::unique_ptr<tson::Map> tson::Tileson::parse(const void *data, size_t size, std::unique_ptr<IDecompressor<std::vector<uint8_t>, std::vector<uint8_t>>> decompressor)
{
	bool result = false;

	if(decompressor != nullptr)
	{
		std::vector<uint8_t> decompressed = decompressor->decompress(data, size);
		result = (decompressed.empty()) ? false : true;
		if(!result)
			return std::make_unique<tson::Map>(tson::ParseStatus::DecompressionError, "Error during decompression");
		result = m_json->parse(&decompressed[0], decompressed.size());
	}
	else
		result = m_json->parse(data, size);

	if(!result)
		return std::make_unique<tson::Map>(tson::ParseStatus::ParseError, "Memory error");

	return parseJson();
}

/*!
 * Common parsing functionality for doing the json parsing
 * @param json Tiled json to parse
 * @return parsed data as Map
 */
std::unique_ptr<tson::Map> tson::Tileson::parseJson()
{
	std::unique_ptr<tson::Map> map = std::make_unique<tson::Map>();

	if(map->parse(*m_json, &m_decompressors, m_project))
		return map;

	return std::make_unique<tson::Map> (tson::ParseStatus::MissingData, "Missing map data...");
}

/*!
 * Gets the decompressor container used when something is either encoded or compressed (regardless: IDecompressor is used as base).
 * These are used specifically for tile layers, and are connected by checking the name of the IDecompressor. If the name of a decompressor
 * matches with an encoding or a compression, its decompress() function will be used.
 *
 * @return The container including all decompressors.
 */
tson::DecompressorContainer *tson::Tileson::decompressors()
{
	return &m_decompressors;
}

#endif //TILESON_TILESON_PARSER_HPP

/*** End of inlined file: tileson_parser.hpp ***/


/*** Start of inlined file: tileson_forward.hpp ***/
//
// Created by robin on 25.07.2020.
//

#ifndef TILESON_TILESON_FORWARD_HPP
#define TILESON_TILESON_FORWARD_HPP
/*!
 * T I L E S O N   F O R W A R D   D E C L A R A T I O N S
 * -------------------------------------------------------
 *
 * Due to cross-references we have forward declarations that cannot be resolved during the
 * implementation, thus the implementations must be done later when the class definition itself is known.
 *
 * All those forward declarations can be found below.
 */

// M a p . h p p
// ----------------

tson::TiledClass *tson::Map::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_project != nullptr) ? m_project->getClass(m_classType) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// T i l e . h p p
// ---------------------
/*!
 * Used in cases where you have a FLIP FLAGGED tile
 * @param id
 */
tson::Tile tson::Tile::CreateFlippedTile(uint32_t gid, tson::Map* map)
{
	// Parse flip flags and remove flip bits from global id
	tson::Tile tile;
	tile.manageFlipFlagsByIdThenRemoveFlags(gid);

	tile.m_map = map;
	tile.m_gid = gid;

	// Compute local id from global id
	tile.m_id = tile.m_gid - tile.m_map->getTilesetByGid(tile.m_gid)->getFirstgid() + 1;

	return tile;
}

/*!
 * Used in cases where you have a tile without any property
 * @param id
 */
tson::Tile tson::Tile::CreateMissingTile(uint32_t id, tson::Tileset* tileset, tson::Map* map)
{
	Tile tile;
	tile.m_tileset = tileset;
	tile.m_map = map;
	tile.m_id = id;
	tile.m_gid = tileset->getFirstgid() + id - 1;
	tile.performDataCalculations();

	return tile;
}

/*!
 * Parses a tile from a Tiled json. id on tile is store as id + 1 to match the references in data containers.
 * @param json
 * @return
 */
bool tson::Tile::parse(IJson &json, tson::Tileset *tileset, tson::Map *map)
{
	m_tileset = tileset;
	m_map = map;

	if(json.count("image") > 0) m_image = fs::path(json["image"].get<std::string>()); //Optional

	bool allFound = parseId(json);

	if(json.count("type") > 0) m_type = json["type"].get<std::string>(); //Optional
	else if(json.count("class") > 0) m_type = json["class"].get<std::string>(); //Tiled v1.9 renamed 'type' to 'class'

	if(json.count("objectgroup") > 0) m_objectgroup = tson::Layer(json["objectgroup"], m_map); //Optional

	if(json.count("imagewidth") > 0 && json.count("imageheight") > 0)
		m_imageSize = {json["imagewidth"].get<int>(), json["imageheight"].get<int>()}; //Optional

	m_subRect = {0,0, m_imageSize.x, m_imageSize.y};
	if(json.count("x") > 0) m_subRect.x = json["x"].get<int>(); //Optional
	if(json.count("y") > 0) m_subRect.y = json["y"].get<int>(); //Optional
	if(json.count("width") > 0) m_subRect.width = json["width"].get<int>(); //Optional
	if(json.count("height") > 0) m_subRect.height = json["height"].get<int>(); //Optional

	//More advanced data
	if(json.count("animation") > 0 && json["animation"].isArray())
	{
		auto &animation = json.array("animation");
		std::vector<tson::Frame> frames;
		std::for_each(animation.begin(), animation.end(), [&](std::unique_ptr<IJson> &item) { frames.emplace_back(*item); });
		if(frames.size() > 0)
		{
			m_animation.setFrames(frames);
		}
	}
	if(json.count("terrain") > 0 && json["terrain"].isArray())
	{
		auto &terrain = json.array("terrain");
		std::for_each(terrain.begin(), terrain.end(), [&](std::unique_ptr<IJson> &item) { m_terrain.emplace_back(item->get<int>()); });
	}

	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		tson::Project *project = (m_map != nullptr) ? m_map->getProject() : nullptr;
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item, project); });
	}

	performDataCalculations();

	return allFound;
}

/*!
 * Really just a shortcut to retrieve the tile size from the map.
 * @return TileSize based on the map property for tile size.
 */
const tson::Vector2i tson::Tile::getTileSize() const
{
	if(m_map != nullptr)
		return m_map->getTileSize();
	else
		return {0,0};
}

bool tson::Tile::parseId(IJson &json)
{
	if(json.count("id") > 0)
	{
		m_id = json["id"].get<uint32_t>() + 1;
		if (m_tileset != nullptr)
			m_gid = m_tileset->getFirstgid() + m_id - 1;
		else
			m_gid = m_id;
		manageFlipFlagsByIdThenRemoveFlags(m_gid);
		return true;
	}
	return false;
}

/*!
 * Uses tson::Tileset and tson::Map data to calculate related values for tson::Tile.
 * Added in v1.2.0
 */
void tson::Tile::performDataCalculations()
{
	if (m_tileset == nullptr || m_map == nullptr)
		return;

	m_drawingRect = { 0, 0, 0, 0 };

	if (m_tileset->getType() == TilesetType::ImageTileset)
	{
		int firstId = m_tileset->getFirstgid(); //First tile id of the tileset
		int columns = m_tileset->getColumns();
		int rows = m_tileset->getTileCount() / columns;
		int lastId = (m_tileset->getFirstgid() + m_tileset->getTileCount()) - 1;

		int const gid = static_cast<int>(getGid());
		if (gid >= firstId && gid <= lastId)
		{
			int const baseTilePosition = (gid - firstId);

			int const tileModX = (baseTilePosition % columns);
			int const currentRow = (baseTilePosition / columns);
			int const offsetX = (tileModX != 0) ? ((tileModX)*m_map->getTileSize().x) : (0 * m_map->getTileSize().x);
			int const offsetY = (currentRow < rows - 1) ? (currentRow * m_map->getTileSize().y) : ((rows - 1) * m_map->getTileSize().y);

			tson::Vector2i spacing = m_tileset->getMarginSpacingOffset({ tileModX, currentRow });
			m_drawingRect = { offsetX + spacing.x, offsetY + spacing.y, m_tileset->getTileSize().x, m_tileset->getTileSize().y };
		}
	}
	else if (m_tileset->getType() == TilesetType::ImageCollectionTileset)
	{
		tson::Vector2i imageSize = m_imageSize;

		// Tile in Image Collection Tileset contains image size
		if (getFlipFlags() != tson::TileFlipFlags::None)
		{
			uint32_t id = getGid() - m_tileset->getFirstgid() + 1;
			imageSize = m_tileset->getTile(id)->getImageSize();
		}

		m_drawingRect = { 0, 0, imageSize.x, imageSize.y };
	}
}

/*!
 * Get the position of the tile in pixels based on the tile data position from the current layer.
 * @return The position of the tile in Pixels
 */
const tson::Vector2f tson::Tile::getPosition(const std::tuple<int, int> &tileDataPos)
{
	tson::Vector2i tileSize = m_map->getTileSize();
	return {((float) std::get<0>(tileDataPos)) * tileSize.x, ((float) std::get<1>(tileDataPos)) * tileSize.y};
}

/*!
 * Gets the class information for the 'type'/'class'
 * This may only give a valid result if the map is loaded through a tson::Project
 * @return a tson::TiledClass object if related map was loaded through tson::Project
 */
tson::TiledClass *tson::Tile::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_type) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// T i l e s e t . h p p
// ------------------------

tson::Project *tson::Tileset::getProject() const {
	return m_map ? m_map->getProject() : nullptr;
}

tson::TiledClass *tson::Tileset::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_classType) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// T i l e O b j e c t . h p p
// ---------------------

/*!
 * In cases where the empty constructor is called, this must be called manually
 * for this class to make sense
 * @param posInTileUnits
 * @param tile
 */
void tson::TileObject::initialize(const std::tuple<int, int> &posInTileUnits, tson::Tile *tile)
{
	m_tile = tile;
	m_posInTileUnits = tile->getPositionInTileUnits(posInTileUnits);
	m_position = tile->getPosition(posInTileUnits);
}

const tson::Rect &tson::TileObject::getDrawingRect() const
{
	return m_tile->getDrawingRect();
}

// L a y e r . h p p
// -------------------

/*!
 * Decompresses data if there are matching decompressors
 */
void tson::Layer::decompressData()
{

	tson::DecompressorContainer *container = m_map->getDecompressors();
	if(container->empty())
		return;

	if(m_encoding.empty() && m_compression.empty())
		return;

	std::string data = m_base64Data;
	bool hasBeenDecoded = false;
	if(!m_encoding.empty() && container->contains(m_encoding))
	{
		data = container->get(m_encoding)->decompress(data);
		hasBeenDecoded = true;
	}

	if(!m_compression.empty() && container->contains(m_compression))
	{
		data = container->get(m_compression)->decompress(data);
	}

	if(hasBeenDecoded)
	{
		std::vector<uint8_t> bytes = tson::Tools::Base64DecodedStringToBytes(data);
		m_data = tson::Tools::BytesToUnsignedInts(bytes);
	}
}

/*!
 * Parses a Tiled layer from json
 * @param json
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Layer::parse(IJson &json, tson::Map *map)
{
	m_map = map;

	bool allFound = true;
	if(json.count("tintcolor") > 0) m_tintColor = tson::Colori(json["tintcolor"].get<std::string>()); //Optional
	if(json.count("compression") > 0) m_compression = json["compression"].get<std::string>(); //Optional
	if(json.count("draworder") > 0) m_drawOrder = json["draworder"].get<std::string>(); //Optional
	if(json.count("encoding") > 0) m_encoding = json["encoding"].get<std::string>(); //Optional
	if(json.count("id") > 0) m_id = json["id"].get<int>(); //Optional
	if(json.count("image") > 0) m_image = json["image"].get<std::string>(); //Optional
	if(json.count("name") > 0) m_name = json["name"].get<std::string>(); else allFound = false;
	if(json.count("offsetx") > 0 && json.count("offsety") > 0)
		m_offset = {json["offsetx"].get<float>(), json["offsety"].get<float>()}; //Optional
	if(json.count("opacity") > 0) m_opacity = json["opacity"].get<float>(); else allFound = false;
	if(json.count("width") > 0 && json.count("height") > 0)
		m_size = {json["width"].get<int>(), json["height"].get<int>()}; //else allFound = false; - Not mandatory for all layers!
	if(json.count("transparentcolor") > 0) m_transparentColor = tson::Colori(json["transparentcolor"].get<std::string>()); //Optional
	if(json.count("type") > 0) m_typeStr = json["type"].get<std::string>(); else allFound = false;
	if(json.count("class") > 0) m_classType = json["class"].get<std::string>();                     //Optional
	if(json.count("visible") > 0) m_visible = json["visible"].get<bool>(); else allFound = false;
	if(json.count("x") > 0) m_x = json["x"].get<int>(); else allFound = false;
	if(json.count("y") > 0) m_y = json["y"].get<int>(); else allFound = false;
	if(json.count("repeatx") > 0) m_repeatX = json["repeatx"].get<bool>(); //Optional
	if(json.count("repeaty") > 0) m_repeatY = json["repeaty"].get<bool>(); //Optional

	tson::Vector2f parallax {1.f, 1.f};
	if(json.count("parallaxx") > 0)
		parallax.x = json["parallaxx"].get<float>();
	if(json.count("parallaxy") > 0)
		parallax.y = json["parallaxy"].get<float>();

	m_parallax = parallax;

	//Handle DATA (Optional)
	if(json.count("data") > 0)
	{
		if(json["data"].isArray())
		{
			auto &array = json.array("data");
			std::for_each(array.begin(), array.end(), [&](std::unique_ptr<IJson> &item) { m_data.push_back(item->get<uint32_t>()); });
		}
		else
		{
			m_base64Data = json["data"].get<std::string>();
			decompressData();
		}
	}

	//More advanced data
	if(json.count("chunks") > 0 && json["chunks"].isArray())
	{
		auto &chunks = json.array("chunks");
		std::for_each(chunks.begin(), chunks.end(), [&](std::unique_ptr<IJson> &item) { m_chunks.emplace_back(*item); });
	}
	if(json.count("layers") > 0 && json["layers"].isArray())
	{
		auto &layers = json.array("layers");
		std::for_each(layers.begin(), layers.end(), [&](std::unique_ptr<IJson> &item) { m_layers.emplace_back(*item, m_map); });
	}
	if(json.count("objects") > 0 && json["objects"].isArray())
	{
		auto &objects = json.array("objects");
		std::for_each(objects.begin(), objects.end(), [&](std::unique_ptr<IJson> &item) { m_objects.emplace_back(*item, m_map); });
	}
	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &properties = json.array("properties");
		tson::Project *project = (m_map != nullptr) ? m_map->getProject() : nullptr;
		std::for_each(properties.begin(), properties.end(), [&](std::unique_ptr<IJson> &item) { m_properties.add(*item, project); });
	}

	setTypeByString();

	return allFound;
}

tson::TiledClass *tson::Layer::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_classType) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// O b j e c t . h p p
// --------------------

/*!
* Returns the requested IJson object if it exists in the map file or in a related template file
* @param fieldName The name of the field to check
* @param main The main json file being parsed
* @param templ The template file json, if present, nullptr otherwise.
* @return the requested json object if found in the main json file, otherwise if it is found in the template and nullptr if not found anywhere
*/
	tson::IJson* tson::readField(const std::string& fieldName,  IJson& main, IJson* templ)
{
	if(main.count(fieldName) > 0)
	{
		return &main[fieldName];
	} else if (templ && templ->count(fieldName) > 0)
	{
		return  &(*templ)[fieldName];
	}

	return nullptr;
}

/*!
* Attempts to read a text field from main file or the template if not overriden
* @param fieldName The name of the field to check
* @param main The main json file being parsed
* @param templ The template file json, if present, nullptr otherwise.
* @return true if the field was found and parsed in any of the objects, false otherwise
*/
bool tson::readField(Text& field, const std::string& fieldName,  IJson& main, IJson* templ)
{
	IJson* fieldJson = readField(fieldName, main, templ);
	if(fieldJson){
		field = tson::Text(*fieldJson);
		return true;
	}
	return false;
}

/*!
* Attempts to read a series of coordinates from main file or the template if not overriden
* @param fieldName The name of the field to check
* @param main The main json file being parsed
* @param templ The template file json, if present, nullptr otherwise.
* @return true if the field was found and parsed in any of the objects, false otherwise
*/
bool tson::readField(std::vector<Vector2i>& field, const std::string& fieldName, IJson& main, IJson* templ)
{
	IJson* fieldJson = readField(fieldName, main, templ);
	if(fieldJson && fieldJson->isArray())
	{
		auto polyline = fieldJson->array();
		std::for_each(polyline.begin(), polyline.end(),[&field](std::unique_ptr<IJson> &item)
		{
			IJson &j = *item;
			field.emplace_back(j["x"].get<int>(), j["y"].get<int>());
		});
		return true;
	}
	return false;
}

/*!
* Attempts to read a vector from main file or the template if not overriden
* @param field Target variable to fill
* @param fieldNameX The name of the field to check for the x part of the vector
* @param fieldNameY The name of the field to check for the y part of the vector
* @param main The main json file being parsed
* @param templ The template file json, if present, nullptr otherwise.
* @return true if the field was found and parsed in any of the objects, false otherwise
*/
bool tson::readVector(Vector2i& field, const std::string& fieldNameX, const std::string& fieldNameY, IJson& main, IJson* templ)
{
	int x = 0, y = 0;
	if(readField(x, fieldNameX, main, templ) && readField(y, fieldNameY, main, templ))
	{
		field = {x, y};
		return true;
	}
	return false;
}

/*!
* Reads all custom properties from the given json node
* @param properties Target Properties collection to fill
* @param json json node representing the map object
* @param map Pointer to current map being parsed
*/
void tson::readProperties(tson::PropertyCollection& properties, IJson& json, tson::Map* map)
{
	if(json.count("properties") > 0 && json["properties"].isArray())
	{
		auto &props = json.array("properties");
		tson::Project *project = (map != nullptr) ? map->getProject() : nullptr;
		std::for_each(props.begin(), props.end(), [&](std::unique_ptr<IJson> &item)
		{
			properties.add(*item, project);
		});
	}
}

/*!
* Reads a gid, parsing flip-flags
* @param gid Target gid to fill
* @param flags Target flip flags to fill
* @param main The main json file being parsed
* @param templ The template file json, if present, nullptr otherwise.
*/
void tson::readGid(uint32_t& gid, TileFlipFlags& flags, IJson& main, IJson* templ)
{
	uint32_t g;
	if(readField(g, "gid", main, templ))
	{
		if (g & FLIPPED_HORIZONTALLY_FLAG) flags |= TileFlipFlags::Horizontally;
		if (g & FLIPPED_VERTICALLY_FLAG) flags |= TileFlipFlags::Vertically;
		if (g & FLIPPED_DIAGONALLY_FLAG) flags |= TileFlipFlags::Diagonally;

		// Clear flags
		g &= ~(FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG);

		gid = g;
	}
}

/*!
 * Parses a json Tiled object and automatically determines the object type based on the data presented.
 * Call getObjectType() to see what object type it is.
 * @param json
 * @return true if all mandatory fields was found. false otherwise.
 */
bool tson::Object::parse(IJson &json, tson::Map *map)
{
	m_map = map;
	bool allFound = true;
	IJson* templateJson = nullptr;

	if(readField(m_template, "template", json) && map != nullptr)
	{
		IJson* tobjJsonFile = map->parseLinkedFile(m_template);
		if(tobjJsonFile)
			templateJson = readField("object", *tobjJsonFile);
	}

	readField(m_ellipse, "ellipse", json, templateJson); //Optional
	readField(m_point, "point", json, templateJson); // Optional
	readField(m_text, "text", json, templateJson);
	readGid(m_gid, m_flipFlags, json, templateJson);

	allFound &= readField(m_id, "id", json, templateJson);
	allFound &= readField(m_name, "name", json, templateJson);
	allFound &= readField(m_rotation, "rotation", json, templateJson);
	allFound &= readField(m_type, "type", json, templateJson) || readField(m_type, "class", json, templateJson); //Tiled v1.9 renamed 'type' to 'class'
	allFound &= readField(m_visible, "visible", json, templateJson);
	allFound &= readVector(m_size, "width", "height", json, templateJson);
	allFound &= readVector(m_position, "x", "y", json, templateJson);

	setObjectTypeByJson(json, templateJson);

	if(m_objectType == ObjectType::Template)
		allFound = true; //Just accept anything with this type

	//More advanced data
	readField(m_polygon, "polygon", json, templateJson);
	readField(m_polyline, "polyline", json, templateJson);

	// merge properties with template's.
	if(templateJson) readProperties(m_properties, *templateJson, m_map);
	readProperties(m_properties, json, m_map);

	return allFound;
}

// W a n g s e t . h p p
// ----------------------
tson::TiledClass *tson::WangSet::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_classType) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// W a n g c o l o r . h p p
// ----------------------
tson::TiledClass *tson::WangColor::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_classType) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

/*!
 * Gets the class information for the 'type'/'class'
 * This may only give a valid result if the map is loaded through a tson::Project
 * @return a tson::TiledClass object if related map was loaded through tson::Project
 */
tson::TiledClass *tson::Object::getClass()
{
	if(m_class == nullptr)
	{
		TiledClass* baseClass = (m_map != nullptr && m_map->getProject() != nullptr) ? m_map->getProject()->getClass(m_type) : nullptr;
		if(baseClass != nullptr)
		{
			m_class = std::make_shared<TiledClass>(*baseClass);//, m_map->getProject());
			m_class->update(m_properties);
		}
	}
	return (m_class != nullptr) ? m_class.get() : nullptr;
}

// W o r l d . h p p
// ------------------

/*!
 * Loads the actual maps based on the world data.
 * @param parser A Tileson object used for parsing the maps of the world.
 * @return How many maps who were parsed. Remember to call getStatus() for the actual map to find out if everything went okay.
 */

std::size_t tson::World::loadMaps(tson::Tileson *parser)
{
	m_maps.clear();
	std::for_each(m_mapData.begin(), m_mapData.end(), [&](const tson::WorldMapData &data)
	{
		if(fs::exists(data.path))
		{
			std::unique_ptr<tson::Map> map = parser->parse(data.path);
			m_maps.push_back(std::move(map));
		}
	});

	return m_maps.size();
}

// P r o p e r t y . h p p
// ------------------
void tson::Property::setValueByType(IJson &json)
{
	switch(m_type)
	{
		case Type::Color:
			m_value = Colori(json.get<std::string>());
			break;

		case Type::File:
			m_value = fs::path(json.get<std::string>());
			break;

		case Type::Int:
			if(!m_propertyType.empty())
			{
				m_type = Type::Enum;
				tson::EnumDefinition *def = (m_project != nullptr) ? m_project->getEnumDefinition(m_propertyType) : nullptr;
				if(def != nullptr)
				{
					uint32_t v = json.get<uint32_t>();
					m_value = tson::EnumValue(v, def);
				}
				else
					m_value = tson::EnumValue();
			}
			else
				m_value = json.get<int>();

			break;

		case Type::Boolean:
			m_value = json.get<bool>();
			break;

		case Type::Float:
			m_value = json.get<float>();
			break;

		case Type::String:
			if(!m_propertyType.empty())
			{
				m_type = Type::Enum;
				tson::EnumDefinition *def = (m_project != nullptr) ? m_project->getEnumDefinition(m_propertyType) : nullptr;
				if(def != nullptr)
				{
					std::string v = json.get<std::string>();
					m_value = tson::EnumValue(v, def);
				}
				else
					m_value = tson::EnumValue();
			}
			else
				setStrValue(json.get<std::string>());

			break;

		case Type::Class:
		{
			tson::TiledClass *baseClass = (m_project != nullptr) ? m_project->getClass(m_propertyType) : nullptr;
			if (baseClass != nullptr)
			{
				tson::TiledClass c = *baseClass;
				c.update(json);
				m_value = c;
			}
		}
		break;

		case Type::Object:
			m_value = json.get<uint32_t>();
			break;
		default:
			setStrValue(json.get<std::string>());
			break;
	}
}

#endif //TILESON_TILESON_FORWARD_HPP

/*** End of inlined file: tileson_forward.hpp ***/

#endif //TILESON_TILESON_H

