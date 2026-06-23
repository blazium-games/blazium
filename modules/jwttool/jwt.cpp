/**************************************************************************/
/*  jwt.cpp                                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2024 Dragos Daian, Randolph William Aarseth II.          */
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

#include "jwt.h"
#include "core/core_bind.h"
#include "core/crypto/crypto.h"
#include "core/io/json.h"
#include "core/os/time.h"

JWT *JWT::jwt_singleton = nullptr;

JWT *JWT::get_singleton() {
	return jwt_singleton;
}
void JWT::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_header", "jwt"), &JWT::get_header);
	ClassDB::bind_method(D_METHOD("get_payload", "jwt"), &JWT::get_payload);
	ClassDB::bind_method(D_METHOD("get_signature", "jwt"), &JWT::get_signature);
	ClassDB::bind_method(D_METHOD("parse", "jwt"), &JWT::parse);
	ClassDB::bind_method(D_METHOD("is_expired", "jwt"), &JWT::is_expired);
	ClassDB::bind_method(D_METHOD("get_claim", "jwt", "claim"), &JWT::get_claim);
	ClassDB::bind_method(D_METHOD("has_claim", "jwt", "claim"), &JWT::has_claim);
	ClassDB::bind_method(D_METHOD("validate_claims", "jwt", "expected_claims"), &JWT::validate_claims);
	ClassDB::bind_method(D_METHOD("validate_header_claims", "jwt", "expected_claims"), &JWT::validate_header_claims);
	ClassDB::bind_method(D_METHOD("validate_timing", "jwt", "leeway_seconds"), &JWT::validate_timing, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("get_algorithm", "jwt"), &JWT::get_algorithm);
	ClassDB::bind_method(D_METHOD("validate", "jwt", "key"), &JWT::validate);
	ClassDB::bind_method(D_METHOD("create_jwt", "header", "payload", "key"), &JWT::create_jwt);
	ClassDB::bind_method(D_METHOD("decode", "jwt"), &JWT::decode);
	ClassDB::bind_method(D_METHOD("base64url_encode", "str"), &JWT::base64url_encode);
	ClassDB::bind_method(D_METHOD("base64url_decode", "b64url"), &JWT::base64url_decode);
	ClassDB::bind_method(D_METHOD("get_kid", "jwt"), &JWT::get_kid);
	ClassDB::bind_method(D_METHOD("create_jwt_timed", "header", "payload", "key", "validity_seconds"), &JWT::create_jwt_timed);
	ClassDB::bind_method(D_METHOD("validate_any", "jwt", "keys"), &JWT::validate_any);
	ClassDB::bind_method(D_METHOD("validate_with_map", "jwt", "key_map"), &JWT::validate_with_map);
	ClassDB::bind_method(D_METHOD("revoke_jti", "jti"), &JWT::revoke_jti);
	ClassDB::bind_method(D_METHOD("is_revoked", "jti"), &JWT::is_revoked);
	ClassDB::bind_method(D_METHOD("clear_revoked"), &JWT::clear_revoked);
	ClassDB::bind_method(D_METHOD("validate_diagnostic", "jwt", "key"), &JWT::validate_diagnostic);
	ClassDB::bind_method(D_METHOD("validate_claims_diagnostic", "jwt", "expected_claims"), &JWT::validate_claims_diagnostic);
	ClassDB::bind_method(D_METHOD("validate_signature_hs256", "jwt", "secret"), &JWT::validate_signature_hs256);
	ClassDB::bind_method(D_METHOD("create_jwt_hs256", "header", "payload", "secret"), &JWT::create_jwt_hs256);
	ClassDB::bind_method(D_METHOD("validate_signature_rs256", "jwt", "key"), &JWT::validate_signature_rs256);
	ClassDB::bind_method(D_METHOD("create_jwt_rs256", "header", "payload", "key"), &JWT::create_jwt_rs256);
}

Dictionary JWT::get_header(const String &p_jwt) {
	Vector<String> split = p_jwt.split(".");
	if (split.size() < 2) {
		return {};
	}
	String json_utf8 = base64url_decode(split[0]);
	return JSON::parse_string(json_utf8);
}
Dictionary JWT::get_payload(const String &p_jwt) {
	Vector<String> split = p_jwt.split(".");
	if (split.size() < 2) {
		return {};
	}
	String json_utf8 = base64url_decode(split[1]);
	return JSON::parse_string(json_utf8);
}

String JWT::get_signature(const String &p_jwt) {
	Vector<String> split = p_jwt.split(".");
	if (split.size() < 3) {
		return "";
	}
	return split[2];
}

bool JWT::is_expired(const String &p_jwt) {
	Dictionary payload = get_payload(p_jwt);
	if (!payload.has("exp")) {
		return false;
	}
	double exp = payload["exp"];
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	return current_time >= exp;
}

Variant JWT::get_claim(const String &p_jwt, const String &p_claim) {
	Dictionary payload = get_payload(p_jwt);
	if (payload.has(p_claim)) {
		return payload[p_claim];
	}
	return Variant();
}

bool JWT::has_claim(const String &p_jwt, const String &p_claim) {
	Dictionary payload = get_payload(p_jwt);
	return payload.has(p_claim);
}

bool JWT::validate_claims(const String &p_jwt, const Dictionary &p_expected_claims) {
	Dictionary payload = get_payload(p_jwt);
	Array keys = p_expected_claims.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		if (!payload.has(key)) {
			return false;
		}
		Variant expected = p_expected_claims[key];
		Variant actual = payload[key];

		if (actual.get_type() == Variant::ARRAY && expected.get_type() != Variant::ARRAY) {
			Array actual_array = actual;
			if (!actual_array.has(expected)) {
				return false;
			}
		} else if (actual != expected) {
			return false;
		}
	}
	return true;
}

bool JWT::validate_header_claims(const String &p_jwt, const Dictionary &p_expected_claims) {
	Dictionary header = get_header(p_jwt);
	Array keys = p_expected_claims.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		if (!header.has(key)) {
			return false;
		}
		if (header[key] != p_expected_claims[key]) {
			return false;
		}
	}
	return true;
}

bool JWT::validate_timing(const String &p_jwt, double p_leeway_seconds) {
	Dictionary payload = get_payload(p_jwt);
	double current_time = Time::get_singleton()->get_unix_time_from_system();

	if (payload.has("exp")) {
		double exp = payload["exp"];
		if (current_time >= exp + p_leeway_seconds) {
			return false;
		}
	}

	if (payload.has("nbf")) {
		double nbf = payload["nbf"];
		if (current_time < nbf - p_leeway_seconds) {
			return false;
		}
	}

	return true;
}

String JWT::get_algorithm(const String &p_jwt) {
	Dictionary header = get_header(p_jwt);
	if (header.has("alg")) {
		return header["alg"];
	}
	return "";
}

bool JWT::validate_signature_hs256(const String &p_jwt, const String &p_secret) {
	Vector<String> split = p_jwt.split(".");
	if (split.size() < 3) {
		return false;
	}

	String msg = split[0] + "." + split[1];
	PackedByteArray msg_bytes = msg.to_utf8_buffer();
	PackedByteArray secret_bytes = p_secret.to_utf8_buffer();

	Ref<Crypto> crypto = Crypto::create();
	PackedByteArray hmac = crypto->hmac_digest(HashingContext::HASH_SHA256, secret_bytes, msg_bytes);

	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();
	String expected_sig = singleton->raw_to_base64(hmac).replace("+", "-").replace("/", "_").replace("=", "");

	return expected_sig == split[2];
}

String JWT::create_jwt_hs256(const Dictionary &p_header, const Dictionary &p_payload, const String &p_secret) {
	String header_str = JSON::stringify(p_header);
	String payload_str = JSON::stringify(p_payload);

	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();

	String header_b64 = singleton->utf8_to_base64(header_str).replace("+", "-").replace("/", "_").replace("=", "");
	String payload_b64 = singleton->utf8_to_base64(payload_str).replace("+", "-").replace("/", "_").replace("=", "");

	String msg = header_b64 + "." + payload_b64;
	PackedByteArray msg_bytes = msg.to_utf8_buffer();
	PackedByteArray secret_bytes = p_secret.to_utf8_buffer();

	Ref<Crypto> crypto = Crypto::create();
	PackedByteArray hmac = crypto->hmac_digest(HashingContext::HASH_SHA256, secret_bytes, msg_bytes);

	String sig_b64 = singleton->raw_to_base64(hmac).replace("+", "-").replace("/", "_").replace("=", "");

	return msg + "." + sig_b64;
}

bool JWT::validate_signature_rs256(const String &p_jwt, Ref<CryptoKey> p_key) {
	if (p_key.is_null()) {
		return false;
	}
	Vector<String> split = p_jwt.split(".");
	if (split.size() < 3) {
		return false;
	}

	String msg = split[0] + "." + split[1];
	PackedByteArray msg_bytes = msg.to_utf8_buffer();

	Ref<Crypto> crypto = Crypto::create();

	Ref<HashingContext> hash_ctx;
	hash_ctx.instantiate();
	hash_ctx->start(HashingContext::HASH_SHA256);
	hash_ctx->update(msg_bytes);
	PackedByteArray msg_hash = hash_ctx->finish();

	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();

	String b64_sig = split[2].replace("-", "+").replace("_", "/");
	while (b64_sig.length() % 4 != 0) {
		b64_sig += "=";
	}
	PackedByteArray sig_bytes = singleton->base64_to_raw(b64_sig);

	return crypto->verify(HashingContext::HASH_SHA256, msg_hash, sig_bytes, p_key);
}

String JWT::create_jwt_rs256(const Dictionary &p_header, const Dictionary &p_payload, Ref<CryptoKey> p_key) {
	if (p_key.is_null()) {
		return "";
	}
	String header_str = JSON::stringify(p_header);
	String payload_str = JSON::stringify(p_payload);

	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();

	String header_b64 = singleton->utf8_to_base64(header_str).replace("+", "-").replace("/", "_").replace("=", "");
	String payload_b64 = singleton->utf8_to_base64(payload_str).replace("+", "-").replace("/", "_").replace("=", "");

	String msg = header_b64 + "." + payload_b64;
	PackedByteArray msg_bytes = msg.to_utf8_buffer();

	Ref<Crypto> crypto = Crypto::create();

	Ref<HashingContext> hash_ctx;
	hash_ctx.instantiate();
	hash_ctx->start(HashingContext::HASH_SHA256);
	hash_ctx->update(msg_bytes);
	PackedByteArray msg_hash = hash_ctx->finish();

	PackedByteArray sig_bytes = crypto->sign(HashingContext::HASH_SHA256, msg_hash, p_key);

	String sig_b64 = singleton->raw_to_base64(sig_bytes).replace("+", "-").replace("/", "_").replace("=", "");

	return msg + "." + sig_b64;
}

bool JWT::validate(const String &p_jwt, const Variant &p_key) {
	if (is_expired(p_jwt)) {
		return false;
	}
	if (!validate_timing(p_jwt)) {
		return false;
	}
	Dictionary payload = get_payload(p_jwt);
	if (payload.has("jti") && is_revoked(payload["jti"])) {
		return false;
	}

	String alg = get_algorithm(p_jwt);
	if (alg == "HS256") {
		if (p_key.get_type() != Variant::STRING) {
			return false;
		}
		return validate_signature_hs256(p_jwt, p_key);
	} else if (alg == "RS256") {
		if (p_key.get_type() != Variant::OBJECT) {
			return false;
		}
		Ref<CryptoKey> key = p_key;
		return validate_signature_rs256(p_jwt, key);
	}
	return false;
}

String JWT::create_jwt(const Dictionary &p_header, const Dictionary &p_payload, const Variant &p_key) {
	if (!p_header.has("alg")) {
		return "";
	}
	String alg = p_header["alg"];
	if (alg == "HS256") {
		if (p_key.get_type() != Variant::STRING) {
			return "";
		}
		return create_jwt_hs256(p_header, p_payload, p_key);
	} else if (alg == "RS256") {
		if (p_key.get_type() != Variant::OBJECT) {
			return "";
		}
		Ref<CryptoKey> key = p_key;
		return create_jwt_rs256(p_header, p_payload, key);
	}
	return "";
}

Dictionary JWT::decode(const String &p_jwt) {
	Dictionary result;
	result["header"] = get_header(p_jwt);
	result["payload"] = get_payload(p_jwt);
	result["signature"] = get_signature(p_jwt);
	return result;
}

String JWT::base64url_encode(const String &p_str) {
	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();
	return singleton->utf8_to_base64(p_str).replace("+", "-").replace("/", "_").replace("=", "");
}

String JWT::base64url_decode(const String &p_b64url) {
	core_bind::Marshalls *singleton = core_bind::Marshalls::get_singleton();
	String b64 = p_b64url.replace("-", "+").replace("_", "/");
	while (b64.length() % 4 != 0) {
		b64 += "=";
	}
	return singleton->base64_to_utf8(b64);
}

String JWT::get_kid(const String &p_jwt) {
	Dictionary header = get_header(p_jwt);
	if (header.has("kid")) {
		return header["kid"];
	}
	return "";
}

String JWT::create_jwt_timed(const Dictionary &p_header, const Dictionary &p_payload, const Variant &p_key, double p_validity_seconds) {
	Dictionary payload = p_payload.duplicate();
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	payload["iat"] = current_time;
	payload["exp"] = current_time + p_validity_seconds;
	return create_jwt(p_header, payload, p_key);
}

bool JWT::validate_any(const String &p_jwt, const Array &p_keys) {
	for (int i = 0; i < p_keys.size(); i++) {
		if (validate(p_jwt, p_keys[i])) {
			return true;
		}
	}
	return false;
}

bool JWT::validate_with_map(const String &p_jwt, const Dictionary &p_key_map) {
	String kid = get_kid(p_jwt);
	if (kid.is_empty() || !p_key_map.has(kid)) {
		return false;
	}
	return validate(p_jwt, p_key_map[kid]);
}

void JWT::revoke_jti(const String &p_jti) {
	if (!p_jti.is_empty()) {
		revoked_jtis.insert(p_jti);
	}
}

bool JWT::is_revoked(const String &p_jti) const {
	return revoked_jtis.has(p_jti);
}

void JWT::clear_revoked() {
	revoked_jtis.clear();
}

Dictionary JWT::validate_diagnostic(const String &p_jwt, const Variant &p_key) {
	Dictionary result;
	result["valid"] = false;

	if (is_expired(p_jwt)) {
		result["error"] = "Token has expired.";
		return result;
	}

	if (!validate_timing(p_jwt)) {
		result["error"] = "Token timing violation (NBF constraint).";
		return result;
	}

	Dictionary payload = get_payload(p_jwt);
	if (payload.has("jti") && is_revoked(payload["jti"])) {
		result["error"] = "Token has been revoked (JTI match).";
		return result;
	}

	String alg = get_algorithm(p_jwt);
	if (alg.is_empty()) {
		result["error"] = "Missing alg header.";
		return result;
	}

	if (alg == "HS256") {
		if (p_key.get_type() != Variant::STRING) {
			result["error"] = "HS256 requires String secret.";
			return result;
		}
		if (validate_signature_hs256(p_jwt, p_key)) {
			result["valid"] = true;
			result["error"] = "";
		} else {
			result["error"] = "Invalid HS256 signature.";
		}
	} else if (alg == "RS256") {
		if (p_key.get_type() != Variant::OBJECT) {
			result["error"] = "RS256 requires CryptoKey object.";
			return result;
		}
		Ref<CryptoKey> key = p_key;
		if (validate_signature_rs256(p_jwt, key)) {
			result["valid"] = true;
			result["error"] = "";
		} else {
			result["error"] = "Invalid RS256 signature.";
		}
	} else {
		result["error"] = "Unsupported algorithm: " + alg;
	}

	return result;
}

PackedStringArray JWT::validate_claims_diagnostic(const String &p_jwt, const Dictionary &p_expected_claims) {
	PackedStringArray errors;
	Dictionary payload = get_payload(p_jwt);
	Array keys = p_expected_claims.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant key = keys[i];
		if (!payload.has(key)) {
			errors.push_back("Missing claim: " + String(key));
		} else if (payload[key] != p_expected_claims[key]) {
			errors.push_back("Mismatched claim: " + String(key));
		}
	}
	return errors;
}

JWT::JWT() {
	jwt_singleton = this;
}
JWT::~JWT() {
	jwt_singleton = nullptr;
}

void JWTBuilder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_algorithm", "alg"), &JWTBuilder::set_algorithm);
	ClassDB::bind_method(D_METHOD("set_subject", "sub"), &JWTBuilder::set_subject);
	ClassDB::bind_method(D_METHOD("set_issuer", "iss"), &JWTBuilder::set_issuer);
	ClassDB::bind_method(D_METHOD("set_audience", "aud"), &JWTBuilder::set_audience);
	ClassDB::bind_method(D_METHOD("set_expiration", "seconds_from_now"), &JWTBuilder::set_expiration);
	ClassDB::bind_method(D_METHOD("set_jwt_id", "jti"), &JWTBuilder::set_jwt_id);
	ClassDB::bind_method(D_METHOD("add_claim", "key", "val"), &JWTBuilder::add_claim);
	ClassDB::bind_method(D_METHOD("sign", "key"), &JWTBuilder::sign);
}

Ref<JWTBuilder> JWTBuilder::set_algorithm(const String &p_alg) {
	header["alg"] = p_alg;
	header["typ"] = "JWT";
	return this;
}

Ref<JWTBuilder> JWTBuilder::set_subject(const String &p_sub) {
	payload["sub"] = p_sub;
	return this;
}

Ref<JWTBuilder> JWTBuilder::set_issuer(const String &p_iss) {
	payload["iss"] = p_iss;
	return this;
}

Ref<JWTBuilder> JWTBuilder::set_audience(const String &p_aud) {
	payload["aud"] = p_aud;
	return this;
}

Ref<JWTBuilder> JWTBuilder::set_expiration(double p_seconds_from_now) {
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	payload["iat"] = current_time;
	payload["exp"] = current_time + p_seconds_from_now;
	return this;
}

Ref<JWTBuilder> JWTBuilder::set_jwt_id(const String &p_jti) {
	payload["jti"] = p_jti;
	return this;
}

Ref<JWTBuilder> JWTBuilder::add_claim(const String &p_key, const Variant &p_val) {
	payload[p_key] = p_val;
	return this;
}

String JWTBuilder::sign(const Variant &p_key) {
	return JWT::get_singleton()->create_jwt(header, payload, p_key);
}

JWTBuilder::JWTBuilder() {
	header["typ"] = "JWT";
}

Ref<DecodedJWT> JWT::parse(const String &p_jwt) {
	Ref<DecodedJWT> decoded;
	decoded.instantiate();
	decoded->parse(p_jwt);
	return decoded;
}

void DecodedJWT::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "jwt"), &DecodedJWT::parse);
	ClassDB::bind_method(D_METHOD("get_header"), &DecodedJWT::get_header);
	ClassDB::bind_method(D_METHOD("get_payload"), &DecodedJWT::get_payload);
	ClassDB::bind_method(D_METHOD("get_signature"), &DecodedJWT::get_signature);
	ClassDB::bind_method(D_METHOD("get_claim", "key"), &DecodedJWT::get_claim);

	ClassDB::bind_method(D_METHOD("get_header_claim", "key"), &DecodedJWT::get_header_claim);
	ClassDB::bind_method(D_METHOD("get_header_claim_as_string", "key"), &DecodedJWT::get_header_claim_as_string);
	ClassDB::bind_method(D_METHOD("has_header_claim", "key"), &DecodedJWT::has_header_claim);

	ClassDB::bind_method(D_METHOD("get_claim_as_string", "key"), &DecodedJWT::get_claim_as_string);
	ClassDB::bind_method(D_METHOD("get_claim_as_int", "key"), &DecodedJWT::get_claim_as_int);
	ClassDB::bind_method(D_METHOD("get_claim_as_bool", "key"), &DecodedJWT::get_claim_as_bool);
	ClassDB::bind_method(D_METHOD("get_claim_as_array", "key"), &DecodedJWT::get_claim_as_array);
	ClassDB::bind_method(D_METHOD("get_claim_as_dictionary", "key"), &DecodedJWT::get_claim_as_dictionary);
	ClassDB::bind_method(D_METHOD("has_claim", "key"), &DecodedJWT::has_claim);
	ClassDB::bind_method(D_METHOD("is_expired"), &DecodedJWT::is_expired);
	ClassDB::bind_method(D_METHOD("get_algorithm"), &DecodedJWT::get_algorithm);

	ClassDB::bind_method(D_METHOD("get_issuer"), &DecodedJWT::get_issuer);
	ClassDB::bind_method(D_METHOD("get_subject"), &DecodedJWT::get_subject);
	ClassDB::bind_method(D_METHOD("get_audience"), &DecodedJWT::get_audience);
	ClassDB::bind_method(D_METHOD("get_expiration_time"), &DecodedJWT::get_expiration_time);
	ClassDB::bind_method(D_METHOD("get_not_before"), &DecodedJWT::get_not_before);
	ClassDB::bind_method(D_METHOD("get_issued_at"), &DecodedJWT::get_issued_at);
	ClassDB::bind_method(D_METHOD("get_jwt_id"), &DecodedJWT::get_jwt_id);

	ClassDB::bind_method(D_METHOD("get_kid"), &DecodedJWT::get_kid);
}

void DecodedJWT::parse(const String &p_jwt) {
	JWT *jwt_tool = JWT::get_singleton();
	if (jwt_tool) {
		header = jwt_tool->get_header(p_jwt);
		payload = jwt_tool->get_payload(p_jwt);
		signature = jwt_tool->get_signature(p_jwt);
	}
}

Dictionary DecodedJWT::get_header() const {
	return header;
}
Dictionary DecodedJWT::get_payload() const {
	return payload;
}
String DecodedJWT::get_signature() const {
	return signature;
}

Variant DecodedJWT::get_claim(const String &p_key) const {
	return payload.get(p_key, Variant());
}

String DecodedJWT::get_claim_as_string(const String &p_key) const {
	Variant c = get_claim(p_key);
	if (c.get_type() == Variant::STRING) {
		return c;
	}
	return "";
}

int DecodedJWT::get_claim_as_int(const String &p_key) const {
	Variant c = get_claim(p_key);
	if (c.get_type() == Variant::INT || c.get_type() == Variant::FLOAT) {
		return (int)c;
	}
	return 0;
}

bool DecodedJWT::get_claim_as_bool(const String &p_key) const {
	Variant c = get_claim(p_key);
	if (c.get_type() == Variant::BOOL) {
		return (bool)c;
	}
	return false;
}

Array DecodedJWT::get_claim_as_array(const String &p_key) const {
	Variant c = get_claim(p_key);
	if (c.get_type() == Variant::ARRAY) {
		return c;
	}
	return Array();
}

Dictionary DecodedJWT::get_claim_as_dictionary(const String &p_key) const {
	Variant c = get_claim(p_key);
	if (c.get_type() == Variant::DICTIONARY) {
		return c;
	}
	return Dictionary();
}

bool DecodedJWT::has_claim(const String &p_key) const {
	return payload.has(p_key);
}
bool DecodedJWT::is_expired() const {
	if (!payload.has("exp")) {
		return false;
	}
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	return (double)payload["exp"] < current_time;
}
String DecodedJWT::get_algorithm() const {
	return header.get("alg", "");
}

Variant DecodedJWT::get_header_claim(const String &p_key) const {
	return header.get(p_key, Variant());
}
String DecodedJWT::get_header_claim_as_string(const String &p_key) const {
	Variant c = get_header_claim(p_key);
	if (c.get_type() == Variant::STRING) {
		return c;
	}
	return "";
}
bool DecodedJWT::has_header_claim(const String &p_key) const {
	return header.has(p_key);
}

String DecodedJWT::get_issuer() const {
	return payload.get("iss", "");
}
String DecodedJWT::get_subject() const {
	return payload.get("sub", "");
}
Variant DecodedJWT::get_audience() const {
	return payload.get("aud", Variant());
}
double DecodedJWT::get_expiration_time() const {
	return payload.get("exp", 0.0);
}
double DecodedJWT::get_not_before() const {
	return payload.get("nbf", 0.0);
}
double DecodedJWT::get_issued_at() const {
	return payload.get("iat", 0.0);
}
String DecodedJWT::get_jwt_id() const {
	return payload.get("jti", "");
}

String DecodedJWT::get_kid() const {
	return header.get("kid", "");
}

DecodedJWT::DecodedJWT() {}
