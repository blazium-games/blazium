/**************************************************************************/
/*  obfuscation.h                                                         */
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

#include "claim.h"
#include "key.h"
#include "scan.h"

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"

class Image;
class AudioStream;

class Obfuscation : public Object {
	GDCLASS(Obfuscation, Object);

public:
	enum MarkKind {
		MARK_SEAL = 0,
		MARK_IMAGE_DWT,
		MARK_IMAGE_GUTTER,
		MARK_AUDIO,
		MARK_SPECTRO,
		MARK_SEED,
		MARK_COPYRIGHT,
		MARK_MANIFEST,
		MARK_CTEX_TRAILER,
	};

private:
	static Obfuscation *singleton;
	Ref<ObfuscationKey> identity;
	static bool import_audio_mark;

	String _identity_dir() const;
	PackedByteArray _default_payload() const;
	PackedByteArray _owner_id_from_key(const Ref<CryptoKey> &p_key) const;
	Vector<uint8_t> _build_seal_payload() const;
	Ref<ObfuscationClaim> _claim_from_payload(const Vector<uint8_t> &p_payload, const Ref<CryptoKey> &p_verify_key) const;
	PackedByteArray _hmac_secret() const;

protected:
	static void _bind_methods();

public:
	static Obfuscation *get_singleton();

	static void set_import_audio_mark(bool p_on) { import_audio_mark = p_on; }
	static bool is_import_audio_mark() { return import_audio_mark; }

	bool is_enabled() const;

	bool has_identity() const;
	Error generate_identity(const String &p_publisher_id, const String &p_project_id);
	Error load_identity();
	void clear_identity();
	PackedByteArray get_owner_id() const;
	String get_project_id() const;
	Ref<CryptoKey> get_public_key() const;
	Error export_public_key(const String &p_path) const;
	Error import_public_key(const String &p_path);

	Ref<Image> generate_seal_image();
	Error save_seal_image(const String &p_path);
	Ref<ObfuscationClaim> decode_seal_image(const Ref<Image> &p_image);
	Ref<ObfuscationClaim> decode_seal_file(const String &p_path);
	bool verify_seal(const Ref<Image> &p_image, const Ref<CryptoKey> &p_public_key = Ref<CryptoKey>());

	bool can_watermark_image(const Ref<Image> &p_image) const;
	Ref<Image> watermark_image(const Ref<Image> &p_image, const PackedByteArray &p_payload = PackedByteArray());
	PackedByteArray extract_image_watermark(const Ref<Image> &p_image);
	float watermark_strength() const;

	PackedByteArray watermark_pcm(const PackedByteArray &p_pcm, int p_mix_rate, int p_channels, const PackedByteArray &p_payload = PackedByteArray());
	PackedByteArray extract_pcm_watermark(const PackedByteArray &p_pcm, int p_mix_rate, int p_channels);
	Ref<AudioStream> watermark_stream(const Ref<AudioStream> &p_stream, const PackedByteArray &p_payload = PackedByteArray());
	PackedByteArray extract_stream_watermark(const Ref<AudioStream> &p_stream);
	PackedByteArray embed_spectrogram(const PackedByteArray &p_pcm, int p_mix_rate, const Ref<Image> &p_image);
	Ref<Image> preview_spectrogram(const PackedByteArray &p_pcm, int p_mix_rate, int p_height = 256);

	static String canonicalize_path(const String &p_path);
	String scramble_output_path(const String &p_logical) const;
	String scramble_identifier(const String &p_name) const;
	String output_artifact_path(const String &p_logical) const;
	PackedStringArray collect_script_identifiers(const String &p_source) const;
	String scramble_script_source(const String &p_source, const String &p_path) const;
	void build_pack_ident_maps(HashMap<String, String> &r_idents, HashSet<String> &r_funcs, HashSet<String> &r_scene_names) const;
	String rewrite_pack_script(const String &p_source, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names, const String &p_path = String()) const;
	String rewrite_pack_paths(const String &p_text) const;
	String rewrite_pack_scene(const String &p_text, const HashMap<String, String> &p_idents) const;
	String pack_script_source(const String &p_source, const String &p_logical, const String &p_packed, const HashMap<String, String> &p_idents, const HashSet<String> &p_funcs, const HashSet<String> &p_scene_names) const;
	void scatter_pack_scripts(HashMap<String, String> &r_packed_to_source) const;
	Dictionary parse_script_comments(const String &p_source, const String &p_packed_path) const;
	Variant comment_ref(const String &p_source, const String &p_packed_path, const String &p_slot) const;

	int64_t derive_seed(const String &p_path) const;
	PackedByteArray derive_seed_bytes(const String &p_path) const;
	String inject_script_source(const String &p_source, const String &p_path) const;
	PackedInt64Array extract_seeds_from_source(const String &p_source) const;
	bool seed_matches(const String &p_path, int64_t p_value) const;

	String get_copyright_canonical() const;
	String get_copyright_base64() const;
	PackedStringArray split_copyright_shards() const;
	String inject_copyright_source(const String &p_source, const String &p_path) const;
	Array collect_copyright_shards(const PackedStringArray &p_sources) const;
	String reconstruct_copyright(const Array &p_collected) const;
	bool copyright_hash_matches(const PackedByteArray &p_hash) const;
	PackedByteArray copyright_hash() const;

	Error inject_into_pack_tree(const String &p_output_dir);
	Error write_obfuscated_tree(const String &p_src_dir, const String &p_out_dir);
	Error append_ctex_trailer(const String &p_path, const String &p_source_file);
	Ref<ObfuscationScan> scan_path(const String &p_path);
	Ref<ObfuscationScan> scan_image(const Ref<Image> &p_image);
	Ref<ObfuscationScan> scan_stream(const Ref<AudioStream> &p_stream);
	Ref<ObfuscationScan> scan_script(const String &p_source, const String &p_path);
	Error write_evidence(const Ref<ObfuscationScan> &p_scan, const String &p_out_path);

	static int run_verify_cli(const String &p_key, const String &p_target, const String &p_out);

	Obfuscation();
	~Obfuscation();
};

VARIANT_ENUM_CAST(Obfuscation::MarkKind);
