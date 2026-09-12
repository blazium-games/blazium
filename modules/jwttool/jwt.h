/**************************************************************************/
/*  jwt.h                                                                 */
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

#pragma once

#include "core/crypto/crypto.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/hash_set.h"

class DecodedJWT;

class JWT : public Object {
	GDCLASS(JWT, Object);
	static JWT *jwt_singleton;

private:
	HashSet<String> revoked_jtis;

protected:
	static void _bind_methods();

public:
	static JWT *get_singleton();

	Dictionary get_header(const String &p_jwt);
	Dictionary get_payload(const String &p_jwt);
	String get_signature(const String &p_jwt);

	Ref<DecodedJWT> parse(const String &p_jwt);
	bool is_expired(const String &p_jwt);

	Variant get_claim(const String &p_jwt, const String &p_claim);
	bool has_claim(const String &p_jwt, const String &p_claim);
	bool validate_claims(const String &p_jwt, const Dictionary &p_expected_claims);
	bool validate_header_claims(const String &p_jwt, const Dictionary &p_expected_claims);

	bool validate_timing(const String &p_jwt, double p_leeway_seconds = 0.0);

	String get_algorithm(const String &p_jwt);
	bool validate(const String &p_jwt, const Variant &p_key);
	String create_jwt(const Dictionary &p_header, const Dictionary &p_payload, const Variant &p_key);

	Dictionary decode(const String &p_jwt);

	String base64url_encode(const String &p_data);
	String base64url_decode(const String &p_str);

	String get_kid(const String &p_jwt);
	String create_jwt_timed(const Dictionary &p_header, const Dictionary &p_payload, const Variant &p_key, double p_validity_seconds);
	bool validate_any(const String &p_jwt, const Array &p_keys);
	bool validate_with_map(const String &p_jwt, const Dictionary &p_key_map);

	void revoke_jti(const String &p_jti);
	bool is_revoked(const String &p_jti) const;
	void clear_revoked();

	Dictionary validate_diagnostic(const String &p_jwt, const Variant &p_key);
	PackedStringArray validate_claims_diagnostic(const String &p_jwt, const Dictionary &p_expected_claims);

	bool validate_signature_hs256(const String &p_jwt, const String &p_secret);
	String create_jwt_hs256(const Dictionary &p_header, const Dictionary &p_payload, const String &p_secret);

	bool validate_signature_rs256(const String &p_jwt, Ref<CryptoKey> p_key);
	String create_jwt_rs256(const Dictionary &p_header, const Dictionary &p_payload, Ref<CryptoKey> p_key);

	JWT();
	~JWT();
};

class JWTBuilder : public RefCounted {
	GDCLASS(JWTBuilder, RefCounted);

protected:
	static void _bind_methods();

private:
	Dictionary header;
	Dictionary payload;

public:
	Ref<JWTBuilder> set_algorithm(const String &p_alg);
	Ref<JWTBuilder> set_subject(const String &p_sub);
	Ref<JWTBuilder> set_issuer(const String &p_iss);
	Ref<JWTBuilder> set_audience(const String &p_aud);
	Ref<JWTBuilder> set_expiration(double p_seconds_from_now);
	Ref<JWTBuilder> set_jwt_id(const String &p_jti);
	Ref<JWTBuilder> add_claim(const String &p_key, const Variant &p_val);
	String sign(const Variant &p_key);

	JWTBuilder();
};

class DecodedJWT : public RefCounted {
	GDCLASS(DecodedJWT, RefCounted);

protected:
	static void _bind_methods();

private:
	Dictionary header;
	Dictionary payload;
	String signature;

public:
	void parse(const String &p_jwt);
	Dictionary get_header() const;
	Dictionary get_payload() const;
	String get_signature() const;
	Variant get_claim(const String &p_key) const;

	Variant get_header_claim(const String &p_key) const;
	String get_header_claim_as_string(const String &p_key) const;
	bool has_header_claim(const String &p_key) const;

	String get_claim_as_string(const String &p_key) const;
	int get_claim_as_int(const String &p_key) const;
	bool get_claim_as_bool(const String &p_key) const;
	Array get_claim_as_array(const String &p_key) const;
	Dictionary get_claim_as_dictionary(const String &p_key) const;

	bool has_claim(const String &p_key) const;
	bool is_expired() const;
	String get_algorithm() const;

	String get_issuer() const;
	String get_subject() const;
	Variant get_audience() const;
	double get_expiration_time() const;
	double get_not_before() const;
	double get_issued_at() const;
	String get_jwt_id() const;
	String get_kid() const;

	DecodedJWT();
};
