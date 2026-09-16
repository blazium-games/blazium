/**************************************************************************/
/*  obfuscation.cpp                                                       */
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

#include "obfuscation.h"

#include "codec/seal_matrix.h"
#include "io/ctex_trailer.h"
#include "io/evidence_writer.h"
#include "io/pack_scan.h"
#include "seeds/comment_lattice.h"
#include "seeds/copyright_lattice.h"
#include "seeds/output_names.h"
#include "seeds/script_seed.h"
#include "seeds/source_map.h"
#include "stego/audio_mark.h"
#include "stego/image_mark.h"
#include "stego/spectro_embed.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/core_bind.h"
#include "core/crypto/crypto.h"
#include "core/crypto/crypto_core.h"
#include "core/crypto/hashing_context.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/hash_set.h"
#include "scene/resources/audio_stream_wav.h"

#include <cstring>

Obfuscation *Obfuscation::singleton = nullptr;
bool Obfuscation::import_audio_mark = false;

Obfuscation *Obfuscation::get_singleton() {
	return singleton;
}

Obfuscation::Obfuscation() {
	singleton = this;
}

Obfuscation::~Obfuscation() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

bool Obfuscation::is_enabled() const {
	return GLOBAL_GET("obfuscation/enabled");
}

String Obfuscation::_identity_dir() const {
	return OS::get_singleton()->get_user_data_dir().path_join("obfuscation");
}

PackedByteArray Obfuscation::_owner_id_from_key(const Ref<CryptoKey> &p_key) const {
	PackedByteArray out;
	out.resize(16);
	if (p_key.is_null()) {
		return out;
	}
	String pem = p_key->save_to_string(true);
	PackedByteArray utf = pem.to_utf8_buffer();
	uint8_t hash[32];
	CryptoCore::sha256(utf.ptr(), utf.size(), hash);
	memcpy(out.ptrw(), hash, 16);
	return out;
}

PackedByteArray Obfuscation::_default_payload() const {
	PackedByteArray p = get_owner_id();
	if (p.size() < 16) {
		p.resize(16);
	}
	return p;
}

bool Obfuscation::has_identity() const {
	return identity.is_valid() && identity->get_crypto_key().is_valid();
}

Error Obfuscation::generate_identity(const String &p_publisher_id, const String &p_project_id) {
	Ref<Crypto> crypto = Crypto::create();
	ERR_FAIL_COND_V(crypto.is_null(), ERR_UNAVAILABLE);
	Ref<CryptoKey> key = crypto->generate_rsa(2048);
	ERR_FAIL_COND_V(key.is_null(), ERR_CANT_CREATE);
	PackedByteArray hmac = crypto->generate_random_bytes(32);
	identity.instantiate();
	identity->set_key(key);
	identity->set_hmac_secret(hmac);
	identity->set_publisher_id(p_publisher_id);
	identity->set_project_id(p_project_id);
	String dir = _identity_dir();
	Error err = DirAccess::make_dir_recursive_absolute(dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	err = key->save(dir.path_join("secret.key"), false);
	ERR_FAIL_COND_V(err != OK, err);
	err = key->save(dir.path_join("public.key"), true);
	ERR_FAIL_COND_V(err != OK, err);
	Ref<FileAccess> hf = FileAccess::open(dir.path_join("hmac.bin"), FileAccess::WRITE);
	ERR_FAIL_COND_V(hf.is_null(), ERR_CANT_CREATE);
	hf->store_buffer(hmac);
	Ref<FileAccess> pf = FileAccess::open(dir.path_join("ids.txt"), FileAccess::WRITE);
	ERR_FAIL_COND_V(pf.is_null(), ERR_CANT_CREATE);
	pf->store_line(p_publisher_id);
	pf->store_line(p_project_id);
	return OK;
}

Error Obfuscation::load_identity() {
	String dir = _identity_dir();
	Ref<Crypto> crypto = Crypto::create();
	ERR_FAIL_COND_V(crypto.is_null(), ERR_UNAVAILABLE);
	Ref<CryptoKey> key = Ref<CryptoKey>(CryptoKey::create());
	ERR_FAIL_COND_V(key.is_null(), ERR_UNAVAILABLE);
	Error err = key->load(dir.path_join("secret.key"), false);
	if (err != OK) {
		err = key->load(dir.path_join("public.key"), true);
	}
	ERR_FAIL_COND_V(err != OK, err);
	identity.instantiate();
	identity->set_key(key);
	Ref<FileAccess> hf = FileAccess::open(dir.path_join("hmac.bin"), FileAccess::READ);
	if (hf.is_valid()) {
		identity->set_hmac_secret(hf->get_buffer(32));
	}
	Ref<FileAccess> pf = FileAccess::open(dir.path_join("ids.txt"), FileAccess::READ);
	if (pf.is_valid()) {
		identity->set_publisher_id(pf->get_line());
		identity->set_project_id(pf->get_line());
	}
	return OK;
}

void Obfuscation::clear_identity() {
	if (!OS::get_singleton()->has_feature("editor")) {
		ERR_FAIL_MSG("clear_identity is refused in packed builds.");
	}
	String dir = _identity_dir();
	DirAccess::remove_absolute(dir.path_join("secret.key"));
	DirAccess::remove_absolute(dir.path_join("public.key"));
	DirAccess::remove_absolute(dir.path_join("hmac.bin"));
	DirAccess::remove_absolute(dir.path_join("ids.txt"));
	identity.unref();
}

PackedByteArray Obfuscation::get_owner_id() const {
	if (!has_identity()) {
		PackedByteArray z;
		z.resize(16);
		return z;
	}
	return _owner_id_from_key(identity->get_crypto_key());
}

String Obfuscation::get_project_id() const {
	if (has_identity() && !identity->get_project_id().is_empty()) {
		return identity->get_project_id();
	}
	return GLOBAL_GET("obfuscation/project_id");
}

Ref<CryptoKey> Obfuscation::get_public_key() const {
	if (!has_identity()) {
		return Ref<CryptoKey>();
	}
	return identity->get_crypto_key();
}

Error Obfuscation::export_public_key(const String &p_path) const {
	ERR_FAIL_COND_V(!has_identity(), ERR_UNCONFIGURED);
	return identity->get_crypto_key()->save(p_path, true);
}

Error Obfuscation::import_public_key(const String &p_path) {
	Ref<CryptoKey> key = Ref<CryptoKey>(CryptoKey::create());
	ERR_FAIL_COND_V(key.is_null(), ERR_UNAVAILABLE);
	Error err = key->load(p_path, true);
	ERR_FAIL_COND_V(err != OK, err);
	if (identity.is_null()) {
		identity.instantiate();
	}
	identity->set_key(key);
	return OK;
}

PackedByteArray Obfuscation::copyright_hash() const {
	PackedByteArray out;
	out.resize(32);
	String b64 = get_copyright_base64();
	PackedByteArray utf = b64.to_utf8_buffer();
	if (!utf.is_empty()) {
		CryptoCore::sha256(utf.ptr(), utf.size(), out.ptrw());
	}
	return out;
}

Vector<uint8_t> Obfuscation::_build_seal_payload() const {
	Vector<uint8_t> p;
	p.resize(4 + 1 + 16 + 16 + 8 + 2 + 32 + 32);
	int o = 0;
	p.write[o++] = 'O';
	p.write[o++] = 'B';
	p.write[o++] = 'F';
	p.write[o++] = '1';
	p.write[o++] = 1;
	PackedByteArray owner = get_owner_id();
	for (int i = 0; i < 16; i++) {
		p.write[o++] = i < owner.size() ? owner[i] : 0;
	}
	PackedByteArray pid = get_project_id().to_utf8_buffer();
	uint8_t ph[32];
	CryptoCore::sha256(pid.ptr(), pid.size(), ph);
	for (int i = 0; i < 16; i++) {
		p.write[o++] = ph[i];
	}
	uint64_t ts = (uint64_t)Time::get_singleton()->get_unix_time_from_system();
	for (int i = 7; i >= 0; i--) {
		p.write[o++] = (uint8_t)((ts >> (i * 8)) & 0xff);
	}
	p.write[o++] = 0;
	p.write[o++] = 0;
	PackedByteArray ch = copyright_hash();
	for (int i = 0; i < 32; i++) {
		p.write[o++] = i < ch.size() ? ch[i] : 0;
	}
	Ref<CryptoKey> key = get_public_key();
	PackedByteArray pem;
	if (key.is_valid()) {
		pem = key->save_to_string(true).to_utf8_buffer();
	}
	uint8_t kh[32];
	CryptoCore::sha256(pem.ptr(), pem.size(), kh);
	for (int i = 0; i < 32; i++) {
		p.write[o++] = kh[i];
	}
	if (has_identity() && identity->has_private()) {
		Ref<Crypto> crypto = Crypto::create();
		if (crypto.is_valid()) {
			uint8_t sh[32];
			CryptoCore::sha256(p.ptr(), p.size(), sh);
			Vector<uint8_t> hv;
			hv.resize(32);
			memcpy(hv.ptrw(), sh, 32);
			Vector<uint8_t> sig = crypto->sign(HashingContext::HASH_SHA256, hv, identity->get_crypto_key());
			uint16_t sig_len = (uint16_t)CLAMP(sig.size(), 0, 65535);
			p.push_back((uint8_t)((sig_len >> 8) & 0xff));
			p.push_back((uint8_t)(sig_len & 0xff));
			for (int i = 0; i < sig_len; i++) {
				p.push_back(sig[i]);
			}
			uint32_t pem_len = pem.size();
			p.push_back((uint8_t)((pem_len >> 24) & 0xff));
			p.push_back((uint8_t)((pem_len >> 16) & 0xff));
			p.push_back((uint8_t)((pem_len >> 8) & 0xff));
			p.push_back((uint8_t)(pem_len & 0xff));
			for (int i = 0; i < pem.size(); i++) {
				p.push_back(pem[i]);
			}
		}
	}
	return p;
}

Ref<ObfuscationClaim> Obfuscation::_claim_from_payload(const Vector<uint8_t> &p_payload, const Ref<CryptoKey> &p_verify_key) const {
	Ref<ObfuscationClaim> claim;
	claim.instantiate();
	if (p_payload.size() < 111) {
		return claim;
	}
	if (!(p_payload[0] == 'O' && p_payload[1] == 'B' && p_payload[2] == 'F' && p_payload[3] == '1')) {
		return claim;
	}
	claim->set_magic("OBF1");
	claim->set_version(p_payload[4]);
	PackedByteArray owner;
	owner.resize(16);
	memcpy(owner.ptrw(), p_payload.ptr() + 5, 16);
	claim->set_owner_id(owner);
	PackedByteArray pidh;
	pidh.resize(16);
	memcpy(pidh.ptrw(), p_payload.ptr() + 21, 16);
	claim->set_project_id(String::hex_encode_buffer(pidh.ptr(), 16));
	uint64_t ts = 0;
	for (int i = 0; i < 8; i++) {
		ts = (ts << 8) | p_payload[37 + i];
	}
	claim->set_issued_unix((int64_t)ts);
	claim->set_flags(p_payload[45] | (p_payload[46] << 8));
	PackedByteArray ch;
	ch.resize(32);
	memcpy(ch.ptrw(), p_payload.ptr() + 47, 32);
	claim->set_copyright_hash(ch);
	Ref<CryptoKey> key = p_verify_key;
	int header = 111;
	if (p_payload.size() >= header + 2) {
		uint16_t sig_len = (uint16_t)((p_payload[header] << 8) | p_payload[header + 1]);
		int sig_off = header + 2;
		if (sig_len > 0 && p_payload.size() >= sig_off + (int)sig_len + 4) {
			Vector<uint8_t> sig;
			sig.resize(sig_len);
			memcpy(sig.ptrw(), p_payload.ptr() + sig_off, sig_len);
			int po = sig_off + sig_len;
			uint32_t pem_len = ((uint32_t)p_payload[po] << 24) | ((uint32_t)p_payload[po + 1] << 16) | ((uint32_t)p_payload[po + 2] << 8) | p_payload[po + 3];
			if (po + 4 + (int)pem_len <= p_payload.size()) {
				PackedByteArray pem;
				pem.resize(pem_len);
				memcpy(pem.ptrw(), p_payload.ptr() + po + 4, pem_len);
				if (key.is_null()) {
					key = Ref<CryptoKey>(CryptoKey::create());
					if (key.is_valid()) {
						String pems;
						pems.parse_utf8((const char *)pem.ptr(), pem.size());
						key->load_from_string(pems, true);
					}
				}
				claim->set_public_key(key);
				Ref<Crypto> crypto = Crypto::create();
				if (crypto.is_valid() && key.is_valid()) {
					uint8_t sh[32];
					CryptoCore::sha256(p_payload.ptr(), header, sh);
					Vector<uint8_t> hv;
					hv.resize(32);
					memcpy(hv.ptrw(), sh, 32);
					claim->set_signature_valid(crypto->verify(HashingContext::HASH_SHA256, hv, sig, key));
				}
			}
		}
	}
	return claim;
}

Ref<Image> Obfuscation::generate_seal_image() {
	if (!has_identity()) {
		load_identity();
	}
	Vector<uint8_t> payload = _build_seal_payload();
	return ObfuscationSealMatrix::encode(payload);
}

Error Obfuscation::save_seal_image(const String &p_path) {
	Ref<Image> img = generate_seal_image();
	ERR_FAIL_COND_V(img.is_null(), ERR_CANT_CREATE);
	return img->save_png(p_path);
}

Ref<ObfuscationClaim> Obfuscation::decode_seal_image(const Ref<Image> &p_image) {
	Vector<uint8_t> payload;
	if (!ObfuscationSealMatrix::decode(p_image, payload)) {
		Ref<ObfuscationClaim> empty;
		empty.instantiate();
		return empty;
	}
	return _claim_from_payload(payload, Ref<CryptoKey>());
}

Ref<ObfuscationClaim> Obfuscation::decode_seal_file(const String &p_path) {
	Ref<Image> img = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	Error err = img->load(p_path);
	if (err != OK) {
		Ref<ObfuscationClaim> empty;
		empty.instantiate();
		return empty;
	}
	return decode_seal_image(img);
}

bool Obfuscation::verify_seal(const Ref<Image> &p_image, const Ref<CryptoKey> &p_public_key) {
	Vector<uint8_t> payload;
	if (!ObfuscationSealMatrix::decode(p_image, payload)) {
		return false;
	}
	Ref<ObfuscationClaim> c = _claim_from_payload(payload, p_public_key);
	return c.is_valid() && c->get_signature_valid();
}

bool Obfuscation::can_watermark_image(const Ref<Image> &p_image) const {
	int min_size = GLOBAL_GET("obfuscation/textures/min_size");
	return ObfuscationImageMark::can_mark(p_image, min_size);
}

Ref<Image> Obfuscation::watermark_image(const Ref<Image> &p_image, const PackedByteArray &p_payload) {
	if (!can_watermark_image(p_image)) {
		return p_image;
	}
	PackedByteArray payload = p_payload.is_empty() ? _default_payload() : p_payload;
	bool gutter = GLOBAL_GET("obfuscation/textures/gutter");
	return ObfuscationImageMark::embed(p_image, payload, get_owner_id(), gutter);
}

PackedByteArray Obfuscation::extract_image_watermark(const Ref<Image> &p_image) {
	return ObfuscationImageMark::extract(p_image, get_owner_id(), 16 * 8);
}

float Obfuscation::watermark_strength() const {
	return 1.0f;
}

PackedByteArray Obfuscation::watermark_pcm(const PackedByteArray &p_pcm, int p_mix_rate, int p_channels, const PackedByteArray &p_payload) {
	PackedByteArray payload = p_payload.is_empty() ? _default_payload() : p_payload;
	return ObfuscationAudioMark::embed(p_pcm, p_mix_rate, p_channels, payload);
}

PackedByteArray Obfuscation::extract_pcm_watermark(const PackedByteArray &p_pcm, int p_mix_rate, int p_channels) {
	return ObfuscationAudioMark::extract(p_pcm, p_mix_rate, p_channels, 16);
}

Ref<AudioStream> Obfuscation::watermark_stream(const Ref<AudioStream> &p_stream, const PackedByteArray &p_payload) {
	Ref<AudioStreamWAV> wav = p_stream;
	if (wav.is_null() || wav->get_format() != AudioStreamWAV::FORMAT_16_BITS) {
		return p_stream;
	}
	PackedByteArray data = wav->get_data();
	int ch = wav->is_stereo() ? 2 : 1;
	PackedByteArray f32;
	f32.resize((data.size() / 2) * (int)sizeof(float));
	const int16_t *in = (const int16_t *)data.ptr();
	float *out = (float *)f32.ptrw();
	int n = data.size() / 2;
	for (int i = 0; i < n; i++) {
		out[i] = in[i] / 32768.0f;
	}
	PackedByteArray marked = watermark_pcm(f32, wav->get_mix_rate(), ch, p_payload);
	const float *mf = (const float *)marked.ptr();
	PackedByteArray out16;
	out16.resize(n * 2);
	int16_t *o16 = (int16_t *)out16.ptrw();
	for (int i = 0; i < n; i++) {
		o16[i] = (int16_t)CLAMP(mf[i] * 32767.0f, -32768.0f, 32767.0f);
	}
	wav->set_data(out16);
	return wav;
}

PackedByteArray Obfuscation::extract_stream_watermark(const Ref<AudioStream> &p_stream) {
	Ref<AudioStreamWAV> wav = p_stream;
	if (wav.is_null() || wav->get_format() != AudioStreamWAV::FORMAT_16_BITS) {
		return PackedByteArray();
	}
	PackedByteArray data = wav->get_data();
	int ch = wav->is_stereo() ? 2 : 1;
	PackedByteArray f32;
	f32.resize((data.size() / 2) * (int)sizeof(float));
	const int16_t *in = (const int16_t *)data.ptr();
	float *out = (float *)f32.ptrw();
	int n = data.size() / 2;
	for (int i = 0; i < n; i++) {
		out[i] = in[i] / 32768.0f;
	}
	return extract_pcm_watermark(f32, wav->get_mix_rate(), ch);
}

PackedByteArray Obfuscation::embed_spectrogram(const PackedByteArray &p_pcm, int p_mix_rate, const Ref<Image> &p_image) {
	return ObfuscationSpectroEmbed::embed(p_pcm, p_mix_rate, p_image);
}

Ref<Image> Obfuscation::preview_spectrogram(const PackedByteArray &p_pcm, int p_mix_rate, int p_height) {
	return ObfuscationSpectroEmbed::preview(p_pcm, p_mix_rate, p_height);
}

int64_t Obfuscation::derive_seed(const String &p_path) const {
	return ObfuscationScriptSeed::derive(_hmac_secret(), ObfuscationOutputNames::canonicalize_path(p_path));
}

PackedByteArray Obfuscation::_hmac_secret() const {
	if (has_identity()) {
		return identity->get_hmac_secret();
	}
	return PackedByteArray();
}

String Obfuscation::canonicalize_path(const String &p_path) {
	return ObfuscationOutputNames::canonicalize_path(p_path);
}

String Obfuscation::scramble_output_path(const String &p_logical) const {
	return ObfuscationOutputNames::scramble_output_path(_hmac_secret(), p_logical);
}

String Obfuscation::scramble_identifier(const String &p_name) const {
	return ObfuscationOutputNames::scramble_identifier(_hmac_secret(), p_name);
}

PackedStringArray Obfuscation::collect_script_identifiers(const String &p_source) const {
	HashSet<String> names;
	ObfuscationSourceMap::collect_identifiers(p_source, names);
	PackedStringArray out;
	for (const String &n : names) {
		out.push_back(n);
	}
	return out;
}

String Obfuscation::scramble_script_source(const String &p_source, const String &p_path) const {
	HashSet<String> funcs;
	HashSet<String> vars;
	const ObfuscationSourceMap::ScriptLang lang = ObfuscationSourceMap::script_lang_from_path(p_path);
	ObfuscationSourceMap::collect_split(p_source, funcs, vars, lang);
	HashSet<String> names = vars;
	for (const String &n : funcs) {
		names.insert(n);
	}
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(_hmac_secret(), names);
	return pack_script_source(p_source, p_path, output_artifact_path(p_path), idents, funcs, HashSet<String>());
}

static bool _obf_is_scene_text(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "tscn" || ext == "tres" || ext == "godot" || p_path.get_file() == "project.godot";
}

static String _obf_rel_path(const String &p_src_root, const String &p_path) {
	String path = p_path.replace("\\", "/").simplify_path();
	String root = p_src_root.replace("\\", "/").simplify_path();
	String rel = path;
	if (path.begins_with(root)) {
		rel = path.substr(root.length());
	}
	while (rel.begins_with("/")) {
		rel = rel.substr(1);
	}
	return rel;
}

void Obfuscation::build_pack_ident_maps(HashMap<String, String> &r_idents, HashSet<String> &r_funcs, HashSet<String> &r_scene_names) const {
	HashSet<String> vars;
	r_funcs.clear();
	r_scene_names.clear();
	Vector<String> files;
	const String root = ProjectSettings::get_singleton()->globalize_path("res://").replace("\\", "/").simplify_path();
	ObfuscationSourceMap::list_files(root, files);
	for (int i = 0; i < files.size(); i++) {
		const String rel = _obf_rel_path(root, files[i]);
		if (ObfuscationSourceMap::skip_relative_path(rel)) {
			continue;
		}
		const String ext = files[i].get_extension().to_lower();
		if (ObfuscationSourceMap::is_script_ext(ext)) {
			ObfuscationSourceMap::collect_split(FileAccess::get_file_as_string(files[i]), r_funcs, vars, ObfuscationSourceMap::script_lang_from_path(files[i]));
		} else if (_obf_is_scene_text(files[i])) {
			ObfuscationSourceMap::collect_scene_names(FileAccess::get_file_as_string(files[i]), r_scene_names);
		}
	}
	HashSet<String> names = vars;
	for (const String &n : r_funcs) {
		names.insert(n);
	}
	for (const String &n : r_scene_names) {
		names.insert(n);
	}
	r_idents = ObfuscationSourceMap::map_identifiers(_hmac_secret(), names);
}

String Obfuscation::rewrite_pack_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, const String &p_path) const {
	const bool scramble = GLOBAL_GET("obfuscation/pack/scramble_names");
	return ObfuscationSourceMap::rewrite_script(p_source, p_idents, p_funcs, p_scene_names, _hmac_secret(), scramble, ObfuscationSourceMap::script_lang_from_path(p_path));
}

String Obfuscation::rewrite_pack_paths(const String &p_text) const {
	return ObfuscationSourceMap::rewrite_paths(p_text, _hmac_secret());
}

String Obfuscation::rewrite_pack_scene(const String &p_text, const HashMap<String, String> &p_idents) const {
	return ObfuscationSourceMap::rewrite_scene(p_text, p_idents, _hmac_secret());
}

String Obfuscation::pack_script_source(const String &p_source, const String &p_logical, const String &p_packed, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names) const {
	const ObfuscationSourceMap::ScriptLang lang = ObfuscationSourceMap::script_lang_from_path(p_logical);
	const bool scramble = GLOBAL_GET("obfuscation/pack/scramble_names");
	const bool lattice = GLOBAL_GET("obfuscation/scripts/comment_lattice");
	const bool minify = GLOBAL_GET("obfuscation/scripts/minify");
	String src = p_source;
	if (lattice || (minify && scramble)) {
		src = ObfuscationSourceMap::strip_minify(src, lang);
	}
	if (lattice) {
		src = ObfuscationCommentLattice::inject_helpers(src, lang, GLOBAL_GET("obfuscation/scripts/inject_ck"), GLOBAL_GET("obfuscation/scripts/inject_copyright"));
	} else {
		src = inject_script_source(src, p_logical);
	}
	src = ObfuscationSourceMap::rewrite_script(src, p_idents, p_funcs, p_scene_names, _hmac_secret(), scramble, lang);
	if (!lattice) {
		return src;
	}
	Vector<String> lines;
	src = ObfuscationCommentLattice::apply_cref(src, _hmac_secret(), p_packed, lang, p_idents, p_funcs, p_scene_names, lines);
	if (GLOBAL_GET("obfuscation/scripts/inject_ck")) {
		lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_packed, "k", 'k', String::num_int64(derive_seed(p_logical)), lang));
	}
	if (GLOBAL_GET("obfuscation/scripts/inject_copyright")) {
		PackedStringArray shards = split_copyright_shards();
		if (!shards.is_empty()) {
			const int idx = (int)((uint64_t)derive_seed(p_logical) % (uint64_t)shards.size());
			lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_packed, "i", 'i', String::num_int64(idx), lang));
			lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_packed, "r", 'r', shards[idx], lang));
		}
	}
	const int decoys = GLOBAL_GET("obfuscation/scripts/comment_decoys");
	const uint64_t seed = (uint64_t)derive_seed(p_packed);
	for (int i = 0; i < decoys; i++) {
		lines.push_back(ObfuscationCommentLattice::make_decoy(seed, i, lang));
	}
	return ObfuscationCommentLattice::append_lines(src, lines);
}

void Obfuscation::scatter_pack_scripts(HashMap<String, String> &r_packed_to_source) const {
	const int copies = GLOBAL_GET("obfuscation/scripts/comment_decoys");
	ObfuscationCommentLattice::scatter_copies(r_packed_to_source, _hmac_secret(), copies);
}

Dictionary Obfuscation::parse_script_comments(const String &p_source, const String &p_packed_path) const {
	return ObfuscationCommentLattice::parse_with_fallbacks(p_source, p_packed_path, _hmac_secret());
}

Variant Obfuscation::comment_ref(const String &p_source, const String &p_packed_path, const String &p_slot) const {
	return ObfuscationCommentLattice::comment_ref(p_source, p_packed_path, p_slot, _hmac_secret());
}

String Obfuscation::output_artifact_path(const String &p_logical) const {
	if (bool(GLOBAL_GET("obfuscation/pack/scramble_names"))) {
		return scramble_output_path(p_logical);
	}
	return p_logical;
}

PackedByteArray Obfuscation::derive_seed_bytes(const String &p_path) const {
	int64_t v = derive_seed(p_path);
	PackedByteArray b;
	b.resize(8);
	for (int i = 7; i >= 0; i--) {
		b.write[7 - i] = (uint8_t)((v >> (i * 8)) & 0xff);
	}
	return b;
}

String Obfuscation::inject_script_source(const String &p_source, const String &p_path) const {
	String src = p_source;
	const ObfuscationSourceMap::ScriptLang lang = ObfuscationSourceMap::script_lang_from_path(p_path);
	const bool luau = lang == ObfuscationSourceMap::SCRIPT_LUAU;
	const bool lattice = GLOBAL_GET("obfuscation/scripts/comment_lattice");
	const bool minify = GLOBAL_GET("obfuscation/scripts/minify");
	const bool scramble = GLOBAL_GET("obfuscation/pack/scramble_names");
	if (lattice || (minify && scramble)) {
		src = ObfuscationSourceMap::strip_minify(src, lang);
	}
	if (lattice) {
		src = ObfuscationCommentLattice::inject_helpers(src, lang, GLOBAL_GET("obfuscation/scripts/inject_ck"), GLOBAL_GET("obfuscation/scripts/inject_copyright"));
		Vector<String> lines;
		if (GLOBAL_GET("obfuscation/scripts/inject_ck")) {
			lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_path, "k", 'k', String::num_int64(derive_seed(p_path)), lang));
		}
		if (GLOBAL_GET("obfuscation/scripts/inject_copyright")) {
			PackedStringArray shards = split_copyright_shards();
			if (!shards.is_empty()) {
				const int idx = (int)((uint64_t)derive_seed(p_path) % (uint64_t)shards.size());
				lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_path, "i", 'i', String::num_int64(idx), lang));
				lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_path, "r", 'r', shards[idx], lang));
			}
		}
		return ObfuscationCommentLattice::append_lines(src, lines);
	}
	if (GLOBAL_GET("obfuscation/scripts/inject_ck")) {
		src = ObfuscationScriptSeed::inject(src, derive_seed(p_path), luau);
	}
	if (GLOBAL_GET("obfuscation/scripts/inject_copyright")) {
		src = inject_copyright_source(src, p_path);
	}
	return src;
}

PackedInt64Array Obfuscation::extract_seeds_from_source(const String &p_source) const {
	PackedInt64Array out = ObfuscationScriptSeed::extract(p_source);
	Dictionary d = ObfuscationCommentLattice::decode_unverified(p_source);
	if (d.has("k")) {
		out.push_back((int64_t)d["k"]);
	}
	return out;
}

bool Obfuscation::seed_matches(const String &p_path, int64_t p_value) const {
	if (derive_seed(p_path) == p_value) {
		return true;
	}
	const String file_only = String("res://") + p_path.get_file();
	return derive_seed(file_only) == p_value;
}

String Obfuscation::get_copyright_canonical() const {
	return ObfuscationCopyrightLattice::canonical(GLOBAL_GET("obfuscation/copyright/author"), GLOBAL_GET("obfuscation/copyright/license"), GLOBAL_GET("obfuscation/copyright/text"));
}

String Obfuscation::get_copyright_base64() const {
	return ObfuscationCopyrightLattice::to_base64(get_copyright_canonical());
}

PackedStringArray Obfuscation::split_copyright_shards() const {
	int n = GLOBAL_GET("obfuscation/copyright/shard_count");
	return ObfuscationCopyrightLattice::split_shards(get_copyright_base64(), n);
}

String Obfuscation::inject_copyright_source(const String &p_source, const String &p_path) const {
	PackedStringArray shards = split_copyright_shards();
	if (shards.is_empty()) {
		return p_source;
	}
	int idx = (int)((uint64_t)derive_seed(p_path) % (uint64_t)shards.size());
	const ObfuscationSourceMap::ScriptLang lang = ObfuscationSourceMap::script_lang_from_path(p_path);
	if (GLOBAL_GET("obfuscation/scripts/comment_lattice")) {
		String src = p_source;
		if (!src.contains("_obfuscation_cr_mix")) {
			src = ObfuscationCommentLattice::inject_helpers(src, lang, false, true);
		}
		Vector<String> lines;
		lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_path, "i", 'i', String::num_int64(idx), lang));
		lines.push_back(ObfuscationCommentLattice::encode_line(_hmac_secret(), p_path, "r", 'r', shards[idx], lang));
		return ObfuscationCommentLattice::append_lines(src, lines);
	}
	return ObfuscationCopyrightLattice::inject(p_source, idx, shards[idx], lang == ObfuscationSourceMap::SCRIPT_LUAU);
}

Array Obfuscation::collect_copyright_shards(const PackedStringArray &p_sources) const {
	Array out;
	for (int i = 0; i < p_sources.size(); i++) {
		Dictionary d = ObfuscationCopyrightLattice::collect(p_sources[i]);
		if (d.is_empty()) {
			Dictionary u = ObfuscationCommentLattice::decode_unverified(p_sources[i]);
			if (u.has("i") && u.has("r")) {
				d["index"] = u["i"];
				d["shard"] = u["r"];
			}
		}
		if (!d.is_empty()) {
			out.push_back(d);
		}
	}
	return out;
}

String Obfuscation::reconstruct_copyright(const Array &p_collected) const {
	Dictionary map;
	for (int i = 0; i < p_collected.size(); i++) {
		Dictionary d = p_collected[i];
		int idx = d.get("index", -1);
		if (idx >= 0) {
			map[idx] = d.get("shard", "");
		}
	}
	int n = GLOBAL_GET("obfuscation/copyright/shard_count");
	String b64 = ObfuscationCopyrightLattice::reconstruct(map, n);
	core_bind::Marshalls *m = core_bind::Marshalls::get_singleton();
	if (!m) {
		return String();
	}
	PackedByteArray raw = m->base64_to_raw(b64);
	if (raw.is_empty() && !b64.is_empty()) {
		return String();
	}
	uint8_t h[32];
	PackedByteArray b64b = b64.to_utf8_buffer();
	CryptoCore::sha256(b64b.ptr(), b64b.size(), h);
	PackedByteArray expect = copyright_hash();
	if (expect.size() == 32 && memcmp(h, expect.ptr(), 32) != 0) {
		return String();
	}
	return String::utf8((const char *)raw.ptr(), raw.size());
}

bool Obfuscation::copyright_hash_matches(const PackedByteArray &p_hash) const {
	PackedByteArray expect = copyright_hash();
	if (expect.size() != p_hash.size()) {
		return false;
	}
	return memcmp(expect.ptr(), p_hash.ptr(), expect.size()) == 0;
}

Error Obfuscation::inject_into_pack_tree(const String &p_output_dir) {
	const String seal_res = output_artifact_path("res://.obfuscation/claimkey.png");
	const String man_res = output_artifact_path("res://.obfuscation/manifest.bin");
	const String seal_abs = ObfuscationOutputNames::to_absolute(p_output_dir, seal_res);
	const String man_abs = ObfuscationOutputNames::to_absolute(p_output_dir, man_res);
	Error err = DirAccess::make_dir_recursive_absolute(seal_abs.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	err = DirAccess::make_dir_recursive_absolute(man_abs.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	err = save_seal_image(seal_abs);
	ERR_FAIL_COND_V(err != OK, err);
	Dictionary man;
	man["owner_id"] = get_owner_id();
	man["project_id"] = get_project_id();
	man["copyright_hash"] = copyright_hash();
	Ref<FileAccess> f = FileAccess::open(man_abs, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	f->store_string(JSON::stringify(man));
	return OK;
}

Error Obfuscation::write_obfuscated_tree(const String &p_src_dir, const String &p_out_dir) {
	ERR_FAIL_COND_V(p_src_dir.is_empty() || p_out_dir.is_empty(), ERR_INVALID_PARAMETER);
	if (!has_identity()) {
		return ERR_UNCONFIGURED;
	}
	const PackedByteArray key = _hmac_secret();
	const bool scramble = GLOBAL_GET("obfuscation/pack/scramble_names");
	String src_root = p_src_dir.replace("\\", "/").simplify_path();
	while (src_root.ends_with("/")) {
		src_root = src_root.substr(0, src_root.length() - 1);
	}
	Vector<String> files;
	ObfuscationSourceMap::list_files(src_root, files);
	HashSet<String> names;
	HashSet<String> funcs;
	HashSet<String> scene_names;
	for (int i = 0; i < files.size(); i++) {
		const String rel = _obf_rel_path(src_root, files[i]);
		if (ObfuscationSourceMap::skip_relative_path(rel)) {
			continue;
		}
		const String ext = files[i].get_extension().to_lower();
		if (ObfuscationSourceMap::is_script_ext(ext)) {
			HashSet<String> vars;
			ObfuscationSourceMap::collect_split(FileAccess::get_file_as_string(files[i]), funcs, vars, ObfuscationSourceMap::script_lang_from_path(files[i]));
			for (const String &n : vars) {
				names.insert(n);
			}
		} else if (scramble && _obf_is_scene_text(files[i])) {
			ObfuscationSourceMap::collect_scene_names(FileAccess::get_file_as_string(files[i]), scene_names);
		}
	}
	for (const String &n : funcs) {
		names.insert(n);
	}
	for (const String &n : scene_names) {
		names.insert(n);
	}
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(key, names);
	Error mk = DirAccess::make_dir_recursive_absolute(p_out_dir);
	if (mk != OK && mk != ERR_ALREADY_EXISTS) {
		return mk;
	}
	HashMap<String, String> packed_scripts;
	HashMap<String, String> packed_to_abs;
	for (int i = 0; i < files.size(); i++) {
		const String rel = _obf_rel_path(src_root, files[i]);
		if (ObfuscationSourceMap::skip_relative_path(rel)) {
			continue;
		}
		const String logical = String("res://") + rel;
		const String dest_logical = (scramble && !ObfuscationSourceMap::keep_original_path(logical)) ? scramble_output_path(logical) : logical;
		const String dest_abs = ObfuscationOutputNames::to_absolute(p_out_dir, dest_logical);
		Error dir_err = DirAccess::make_dir_recursive_absolute(dest_abs.get_base_dir());
		if (dir_err != OK && dir_err != ERR_ALREADY_EXISTS) {
			return dir_err;
		}
		const String ext = files[i].get_extension().to_lower();
		if (ObfuscationSourceMap::is_script_ext(ext)) {
			packed_scripts[dest_logical] = pack_script_source(FileAccess::get_file_as_string(files[i]), logical, dest_logical, idents, funcs, scene_names);
			packed_to_abs[dest_logical] = dest_abs;
		} else if (_obf_is_scene_text(files[i])) {
			String src = FileAccess::get_file_as_string(files[i]);
			if (scramble) {
				src = ObfuscationSourceMap::rewrite_scene(src, idents, key);
			}
			Ref<FileAccess> wf = FileAccess::open(dest_abs, FileAccess::WRITE);
			ERR_FAIL_COND_V(wf.is_null(), ERR_CANT_CREATE);
			wf->store_string(src);
		} else if (ext == "png") {
			Ref<Image> img;
			img.instantiate();
			if (img->load(files[i]) == OK && can_watermark_image(img)) {
				img = watermark_image(img);
				img->save_png(dest_abs);
			} else {
				DirAccess::copy_absolute(files[i], dest_abs);
			}
		} else {
			DirAccess::copy_absolute(files[i], dest_abs);
		}
	}
	if (GLOBAL_GET("obfuscation/scripts/comment_lattice")) {
		scatter_pack_scripts(packed_scripts);
	}
	for (const KeyValue<String, String> &E : packed_scripts) {
		Ref<FileAccess> wf = FileAccess::open(packed_to_abs[E.key], FileAccess::WRITE);
		ERR_FAIL_COND_V(wf.is_null(), ERR_CANT_CREATE);
		wf->store_string(E.value);
	}
	return inject_into_pack_tree(p_out_dir);
}

Error Obfuscation::append_ctex_trailer(const String &p_path, const String &p_source_file) {
	PackedByteArray hmac;
	if (has_identity()) {
		Ref<Crypto> crypto = Crypto::create();
		if (crypto.is_valid()) {
			hmac = crypto->hmac_digest(HashingContext::HASH_SHA256, identity->get_hmac_secret(), ObfuscationOutputNames::canonicalize_path(p_source_file).to_utf8_buffer());
		}
	}
	PackedByteArray sig;
	return ObfuscationCtexTrailer::append(p_path, get_owner_id(), hmac, sig);
}

Ref<ObfuscationScan> Obfuscation::scan_image(const Ref<Image> &p_image) {
	Ref<ObfuscationScan> scan;
	scan.instantiate();
	Ref<ObfuscationClaim> claim = decode_seal_image(p_image);
	if (claim.is_valid() && claim->get_magic() == "OBF1") {
		scan->set_seal(claim);
		scan->set_signature_valid(claim->get_signature_valid());
		scan->set_owner_id(claim->get_owner_id());
		scan->add_mark("", MARK_SEAL, claim->get_signature_valid());
	}
	PackedByteArray wm = extract_image_watermark(p_image);
	bool match = wm.size() >= 16 && wm == get_owner_id();
	if (claim.is_valid() && claim->get_owner_id().size() == 16) {
		match = wm.size() >= 16 && wm == claim->get_owner_id();
	}
	if (!wm.is_empty()) {
		scan->add_mark("", MARK_IMAGE_DWT, match, wm.size() * 8);
	}
	scan->finalize_score();
	return scan;
}

Ref<ObfuscationScan> Obfuscation::scan_stream(const Ref<AudioStream> &p_stream) {
	Ref<ObfuscationScan> scan;
	scan.instantiate();
	PackedByteArray wm = extract_stream_watermark(p_stream);
	scan->add_mark("", MARK_AUDIO, wm.size() >= 16 && wm == get_owner_id(), wm.size() * 8);
	scan->finalize_score();
	return scan;
}

Ref<ObfuscationScan> Obfuscation::scan_script(const String &p_source, const String &p_path) {
	Ref<ObfuscationScan> scan;
	scan.instantiate();
	PackedInt64Array seeds = extract_seeds_from_source(p_source);
	bool seed_ok = false;
	for (int i = 0; i < seeds.size(); i++) {
		if (seed_matches(p_path, seeds[i])) {
			seed_ok = true;
			break;
		}
	}
	if (!seeds.is_empty()) {
		scan->add_mark(p_path, MARK_SEED, seed_ok);
	}
	Dictionary parsed = parse_script_comments(p_source, p_path);
	if (parsed.has("k") && seeds.is_empty()) {
		scan->add_mark(p_path, MARK_SEED, seed_matches(p_path, (int64_t)parsed["k"]));
	}
	Dictionary cr = ObfuscationCopyrightLattice::collect(p_source);
	if (cr.is_empty() && (parsed.has("i") || parsed.has("r"))) {
		cr["index"] = parsed.get("i", -1);
		cr["shard"] = parsed.get("r", "");
	}
	if (!cr.is_empty()) {
		scan->add_mark(p_path, MARK_COPYRIGHT, true);
	}
	scan->finalize_score();
	return scan;
}

Ref<ObfuscationScan> Obfuscation::scan_path(const String &p_path) {
	Ref<ObfuscationScan> scan;
	scan.instantiate();
	Vector<ObfuscationPackEntry> files = ObfuscationPackScan::list_files(p_path);
	Dictionary shard_map;
	int shard_count = GLOBAL_GET("obfuscation/copyright/shard_count");
	for (int i = 0; i < files.size(); i++) {
		const String path = files[i].path;
		const PackedByteArray &bytes = files[i].bytes;
		const String ext = path.get_extension().to_lower();
		const bool png_magic = bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
		const bool wav_magic = bytes.size() >= 12 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' && bytes[8] == 'W' && bytes[9] == 'A' && bytes[10] == 'V' && bytes[11] == 'E';
		if (png_magic || ext == "png") {
			Ref<Image> img;
			img.instantiate();
			img->load_png_from_buffer(bytes);
			if (img->get_width() == 256 && img->get_height() == 256) {
				Ref<ObfuscationClaim> claim = decode_seal_image(img);
				if (claim.is_valid() && claim->get_magic() == "OBF1") {
					scan->set_seal(claim);
					scan->set_signature_valid(claim->get_signature_valid());
					scan->set_owner_id(claim->get_owner_id());
					scan->set_project_id(claim->get_project_id());
					scan->add_mark(path, MARK_SEAL, claim->get_signature_valid());
					continue;
				}
			}
			if (img->get_width() >= 8) {
				PackedByteArray wm = extract_image_watermark(img);
				if (!wm.is_empty()) {
					const bool match = wm == get_owner_id() || (scan->get_seal().is_valid() && wm == scan->get_seal()->get_owner_id());
					scan->add_mark(path, MARK_IMAGE_DWT, match, wm.size() * 8);
				}
			}
			continue;
		}
		if (wav_magic || ext == "wav") {
			Dictionary opts;
			Ref<AudioStreamWAV> wav = AudioStreamWAV::load_from_buffer(bytes, opts);
			if (wav.is_valid()) {
				PackedByteArray wm = extract_stream_watermark(wav);
				if (!wm.is_empty()) {
					const bool match = wm.size() >= 16 && (wm == get_owner_id() || (scan->get_seal().is_valid() && wm == scan->get_seal()->get_owner_id()));
					scan->add_mark(path, MARK_AUDIO, match, wm.size() * 8);
				}
			}
			continue;
		}
		if (ext == "ctex") {
			PackedByteArray cid, hmac, sig;
			String tmp = OS::get_singleton()->get_cache_path().path_join("_obf_ctex");
			Ref<FileAccess> tf = FileAccess::open(tmp, FileAccess::WRITE);
			if (tf.is_valid()) {
				tf->store_buffer(bytes);
				tf.unref();
				if (ObfuscationCtexTrailer::read(tmp, cid, hmac, sig)) {
					scan->add_mark(path, MARK_CTEX_TRAILER, cid == get_owner_id() || (scan->get_seal().is_valid() && cid == scan->get_seal()->get_owner_id()));
				}
			}
			continue;
		}
		String src = String::utf8((const char *)bytes.ptr(), bytes.size());
		if (src.find("\"owner_id\"") >= 0 && src.find("\"copyright_hash\"") >= 0) {
			scan->add_mark(path, MARK_MANIFEST, true);
			continue;
		}
		if (ObfuscationSourceMap::is_script_ext(ext) || src.find("const _CK") >= 0 || src.find("const _CI") >= 0 || src.find("local _CK") >= 0 || src.find("local _CI") >= 0 || src.find("# ~ ") >= 0 || src.find("-- ~ ") >= 0) {
			Dictionary parsed = parse_script_comments(src, path);
			PackedInt64Array seeds = extract_seeds_from_source(src);
			if (parsed.has("k") || !seeds.is_empty()) {
				scan->add_mark(path, MARK_SEED, true);
			}
			Dictionary cr = ObfuscationCopyrightLattice::collect(src);
			if (cr.is_empty() && parsed.has("i") && parsed.has("r")) {
				cr["index"] = parsed["i"];
				cr["shard"] = parsed["r"];
			}
			if (!cr.is_empty()) {
				shard_map[(int)cr.get("index", -1)] = cr.get("shard", "");
				scan->add_mark(path, MARK_COPYRIGHT, true);
			}
		}
	}
	if (!shard_map.is_empty()) {
		String rec = ObfuscationCopyrightLattice::reconstruct(shard_map, shard_count);
		scan->set_reconstructed_copyright(rec);
	}
	scan->finalize_score();
	return scan;
}

Error Obfuscation::write_evidence(const Ref<ObfuscationScan> &p_scan, const String &p_out_path) {
	ERR_FAIL_COND_V(p_scan.is_null(), ERR_INVALID_PARAMETER);
	Dictionary d;
	d["signature_valid"] = p_scan->get_signature_valid();
	d["score"] = p_scan->get_score();
	d["matched"] = p_scan->get_matched();
	d["total"] = p_scan->get_total();
	d["owner_hex"] = String::hex_encode_buffer(p_scan->get_owner_id().ptr(), p_scan->get_owner_id().size());
	d["project_id"] = p_scan->get_project_id();
	d["marks"] = p_scan->get_marks();
	d["copyright"] = p_scan->get_reconstructed_copyright();
	return ObfuscationEvidenceWriter::write(d, p_out_path);
}

int Obfuscation::run_verify_cli(const String &p_key, const String &p_target, const String &p_out) {
	Obfuscation *ob = get_singleton();
	if (!ob) {
		return 1;
	}
	Ref<ObfuscationScan> scan;
	if (p_target.is_empty()) {
		scan = ob->scan_path(p_key);
	} else {
		ob->decode_seal_file(p_key);
		scan = ob->scan_path(p_target);
		Ref<ObfuscationClaim> claim = ob->decode_seal_file(p_key);
		if (claim.is_valid()) {
			scan->set_seal(claim);
			scan->set_signature_valid(claim->get_signature_valid());
			scan->set_owner_id(claim->get_owner_id());
		}
	}
	String out = p_out.is_empty() ? "evidence.json" : p_out;
	ob->write_evidence(scan, out);
	OS::get_singleton()->print("valid=%s score=%f matched=%d total=%d\n", scan->get_signature_valid() ? "true" : "false", scan->get_score(), scan->get_matched(), scan->get_total());
	return scan->get_signature_valid() ? 0 : 2;
}

void Obfuscation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_enabled"), &Obfuscation::is_enabled);
	ClassDB::bind_method(D_METHOD("has_identity"), &Obfuscation::has_identity);
	ClassDB::bind_method(D_METHOD("generate_identity", "publisher_id", "project_id"), &Obfuscation::generate_identity);
	ClassDB::bind_method(D_METHOD("load_identity"), &Obfuscation::load_identity);
	ClassDB::bind_method(D_METHOD("clear_identity"), &Obfuscation::clear_identity);
	ClassDB::bind_method(D_METHOD("get_owner_id"), &Obfuscation::get_owner_id);
	ClassDB::bind_method(D_METHOD("get_project_id"), &Obfuscation::get_project_id);
	ClassDB::bind_method(D_METHOD("get_public_key"), &Obfuscation::get_public_key);
	ClassDB::bind_method(D_METHOD("export_public_key", "path"), &Obfuscation::export_public_key);
	ClassDB::bind_method(D_METHOD("import_public_key", "path"), &Obfuscation::import_public_key);
	ClassDB::bind_method(D_METHOD("generate_seal_image"), &Obfuscation::generate_seal_image);
	ClassDB::bind_method(D_METHOD("save_seal_image", "path"), &Obfuscation::save_seal_image);
	ClassDB::bind_method(D_METHOD("scramble_identifier", "name"), &Obfuscation::scramble_identifier);
	ClassDB::bind_method(D_METHOD("scramble_output_path", "logical"), &Obfuscation::scramble_output_path);
	ClassDB::bind_method(D_METHOD("output_artifact_path", "logical"), &Obfuscation::output_artifact_path);
	ClassDB::bind_method(D_METHOD("collect_script_identifiers", "source"), &Obfuscation::collect_script_identifiers);
	ClassDB::bind_method(D_METHOD("scramble_script_source", "source", "path"), &Obfuscation::scramble_script_source);
	ClassDB::bind_method(D_METHOD("parse_script_comments", "source", "packed_path"), &Obfuscation::parse_script_comments);
	ClassDB::bind_method(D_METHOD("comment_ref", "source", "packed_path", "slot"), &Obfuscation::comment_ref);
	ClassDB::bind_method(D_METHOD("decode_seal_image", "image"), &Obfuscation::decode_seal_image);
	ClassDB::bind_method(D_METHOD("decode_seal_file", "path"), &Obfuscation::decode_seal_file);
	ClassDB::bind_method(D_METHOD("verify_seal", "image", "public_key"), &Obfuscation::verify_seal, DEFVAL(Ref<CryptoKey>()));
	ClassDB::bind_method(D_METHOD("can_watermark_image", "image"), &Obfuscation::can_watermark_image);
	ClassDB::bind_static_method("Obfuscation", D_METHOD("canonicalize_path", "path"), &Obfuscation::canonicalize_path);
	ClassDB::bind_method(D_METHOD("watermark_image", "image", "payload"), &Obfuscation::watermark_image, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("extract_image_watermark", "image"), &Obfuscation::extract_image_watermark);
	ClassDB::bind_method(D_METHOD("watermark_strength"), &Obfuscation::watermark_strength);
	ClassDB::bind_method(D_METHOD("watermark_pcm", "pcm", "mix_rate", "channels", "payload"), &Obfuscation::watermark_pcm, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("extract_pcm_watermark", "pcm", "mix_rate", "channels"), &Obfuscation::extract_pcm_watermark);
	ClassDB::bind_method(D_METHOD("watermark_stream", "stream", "payload"), &Obfuscation::watermark_stream, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("extract_stream_watermark", "stream"), &Obfuscation::extract_stream_watermark);
	ClassDB::bind_method(D_METHOD("embed_spectrogram", "pcm", "mix_rate", "image"), &Obfuscation::embed_spectrogram);
	ClassDB::bind_method(D_METHOD("preview_spectrogram", "pcm", "mix_rate", "height"), &Obfuscation::preview_spectrogram, DEFVAL(256));
	ClassDB::bind_method(D_METHOD("derive_seed", "path"), &Obfuscation::derive_seed);
	ClassDB::bind_method(D_METHOD("derive_seed_bytes", "path"), &Obfuscation::derive_seed_bytes);
	ClassDB::bind_method(D_METHOD("inject_script_source", "source", "path"), &Obfuscation::inject_script_source);
	ClassDB::bind_method(D_METHOD("extract_seeds_from_source", "source"), &Obfuscation::extract_seeds_from_source);
	ClassDB::bind_method(D_METHOD("seed_matches", "path", "value"), &Obfuscation::seed_matches);
	ClassDB::bind_method(D_METHOD("get_copyright_canonical"), &Obfuscation::get_copyright_canonical);
	ClassDB::bind_method(D_METHOD("get_copyright_base64"), &Obfuscation::get_copyright_base64);
	ClassDB::bind_method(D_METHOD("split_copyright_shards"), &Obfuscation::split_copyright_shards);
	ClassDB::bind_method(D_METHOD("inject_copyright_source", "source", "path"), &Obfuscation::inject_copyright_source);
	ClassDB::bind_method(D_METHOD("collect_copyright_shards", "sources"), &Obfuscation::collect_copyright_shards);
	ClassDB::bind_method(D_METHOD("reconstruct_copyright", "collected"), &Obfuscation::reconstruct_copyright);
	ClassDB::bind_method(D_METHOD("copyright_hash"), &Obfuscation::copyright_hash);
	ClassDB::bind_method(D_METHOD("copyright_hash_matches", "hash"), &Obfuscation::copyright_hash_matches);
	ClassDB::bind_method(D_METHOD("inject_into_pack_tree", "output_dir"), &Obfuscation::inject_into_pack_tree);
	ClassDB::bind_method(D_METHOD("write_obfuscated_tree", "src_dir", "out_dir"), &Obfuscation::write_obfuscated_tree);
	ClassDB::bind_method(D_METHOD("scan_path", "path"), &Obfuscation::scan_path);
	ClassDB::bind_method(D_METHOD("scan_image", "image"), &Obfuscation::scan_image);
	ClassDB::bind_method(D_METHOD("scan_stream", "stream"), &Obfuscation::scan_stream);
	ClassDB::bind_method(D_METHOD("scan_script", "source", "path"), &Obfuscation::scan_script);
	ClassDB::bind_method(D_METHOD("write_evidence", "scan", "out_path"), &Obfuscation::write_evidence);

	BIND_ENUM_CONSTANT(MARK_SEAL);
	BIND_ENUM_CONSTANT(MARK_IMAGE_DWT);
	BIND_ENUM_CONSTANT(MARK_IMAGE_GUTTER);
	BIND_ENUM_CONSTANT(MARK_AUDIO);
	BIND_ENUM_CONSTANT(MARK_SPECTRO);
	BIND_ENUM_CONSTANT(MARK_SEED);
	BIND_ENUM_CONSTANT(MARK_COPYRIGHT);
	BIND_ENUM_CONSTANT(MARK_MANIFEST);
	BIND_ENUM_CONSTANT(MARK_CTEX_TRAILER);
}
