/**************************************************************************/
/*  cooker.cpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "cooker.h"

#include "inp_writer.h"
#include "mesh_writer.h"
#include "ntex_writer.h"
#include "pack_writer.h"
#include "script_writer.h"
#include "wav_writer.h"

#include <cstring>

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "core/object/script_language.h"

namespace n64_cook {

struct NodeRow {
	int parent = -1;
	uint8_t kind = KIND_OTHER;
	Vector3 pos;
	Vector3 rot;
	Vector3 scale = Vector3(1, 1, 1);
	String name;
};

struct Gather {
	Vector<NodeRow> nodes;
	Vector<Vector3> verts;
	Vector<String> errors;
	Ref<AudioStreamWAV> sfx;
	Ref<AudioStreamWAV> music;
	Vector<String> scripts;
};

static void gather_mesh(MeshInstance3D *p_mi, Gather &r_g) {
	if (!p_mi) {
		return;
	}
	const Ref<Mesh> mesh = p_mi->get_mesh();
	if (mesh.is_null()) {
		return;
	}
	const Transform3D xf = p_mi->get_transform();
	const int surfaces = mesh->get_surface_count();
	for (int s = 0; s < surfaces; s++) {
		const Array arrays = mesh->surface_get_arrays(s);
		if (arrays.size() <= Mesh::ARRAY_VERTEX) {
			continue;
		}
		const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
		PackedInt32Array idx;
		if (arrays.size() > Mesh::ARRAY_INDEX) {
			idx = arrays[Mesh::ARRAY_INDEX];
		}
		if (idx.size() >= 3) {
			for (int i = 0; i + 2 < idx.size(); i += 3) {
				if (r_g.verts.size() / 3 >= kMaxTris) {
					r_g.errors.push_back(vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris));
					return;
				}
				for (int k = 0; k < 3; k++) {
					const int vi = idx[i + k];
					if (vi < 0 || vi >= verts.size()) {
						r_g.errors.push_back("MESH index out of range.");
						return;
					}
					r_g.verts.push_back(xf.xform(verts[vi]));
				}
			}
		} else {
			for (int i = 0; i + 2 < verts.size(); i += 3) {
				if (r_g.verts.size() / 3 >= kMaxTris) {
					r_g.errors.push_back(vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris));
					return;
				}
				r_g.verts.push_back(xf.xform(verts[i]));
				r_g.verts.push_back(xf.xform(verts[i + 1]));
				r_g.verts.push_back(xf.xform(verts[i + 2]));
			}
		}
	}
}

static void append_u16(Vector<uint8_t> &r_out, uint16_t p_v) {
	r_out.push_back(uint8_t(p_v & 0xFF));
	r_out.push_back(uint8_t((p_v >> 8) & 0xFF));
}

static void append_i16(Vector<uint8_t> &r_out, int16_t p_v) {
	append_u16(r_out, uint16_t(p_v));
}

static void append_f32(Vector<uint8_t> &r_out, float p_v) {
	uint32_t bits = 0;
	memcpy(&bits, &p_v, sizeof(bits));
	r_out.push_back(uint8_t(bits & 0xFF));
	r_out.push_back(uint8_t((bits >> 8) & 0xFF));
	r_out.push_back(uint8_t((bits >> 16) & 0xFF));
	r_out.push_back(uint8_t((bits >> 24) & 0xFF));
}

static uint8_t kind_of(Node *p_node) {
	if (Object::cast_to<Camera3D>(p_node)) {
		return KIND_CAMERA3D;
	}
	if (Object::cast_to<MeshInstance3D>(p_node)) {
		return KIND_MESH_INSTANCE3D;
	}
	if (Object::cast_to<Node3D>(p_node)) {
		return KIND_NODE3D;
	}
	return KIND_OTHER;
}

static void walk_scene(Node *p_node, Gather &r_g, int p_parent) {
	if (!p_node) {
		return;
	}
	if (r_g.nodes.size() >= kMaxNodes) {
		r_g.errors.push_back(vformat("N64 cook refuses more than %d nodes (no silent drop).", kMaxNodes));
		return;
	}

	NodeRow row;
	row.parent = p_parent;
	row.kind = kind_of(p_node);
	row.name = p_node->get_name();
	if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
		const Transform3D xf = n3->get_transform();
		row.pos = xf.origin;
		row.rot = xf.basis.get_euler();
		row.scale = xf.basis.get_scale();
	}
	const int my = r_g.nodes.size();
	r_g.nodes.push_back(row);
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		gather_mesh(mi, r_g);
	}
	Ref<AudioStream> stream;
	if (AudioStreamPlayer *ap = Object::cast_to<AudioStreamPlayer>(p_node)) {
		stream = ap->get_stream();
	} else if (AudioStreamPlayer3D *ap3 = Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		stream = ap3->get_stream();
	} else if (AudioStreamPlayer2D *ap2 = Object::cast_to<AudioStreamPlayer2D>(p_node)) {
		stream = ap2->get_stream();
	}
	if (stream.is_valid()) {
		const Ref<AudioStreamWAV> wav = stream;
		if (wav.is_valid() && (wav->get_format() == AudioStreamWAV::FORMAT_8_BITS || wav->get_format() == AudioStreamWAV::FORMAT_16_BITS)) {
			if (r_g.sfx.is_null()) {
				r_g.sfx = wav;
			} else if (r_g.music.is_null()) {
				r_g.music = wav;
			}
			if (r_g.music.is_null() && wav->get_loop_mode() != AudioStreamWAV::LOOP_DISABLED) {
				r_g.music = wav;
			}
		}
	}
	if (p_node->get_script()) {
		const Ref<Script> sc = p_node->get_script();
		if (sc.is_valid()) {
			String src = sc->get_source_code();
			if (src.is_empty() && !sc->get_path().is_empty()) {
				src = FileAccess::get_file_as_string(sc->get_path());
			}
			if (!src.is_empty()) {
				r_g.scripts.push_back(src);
			}
		}
	}

	const int child_count = p_node->get_child_count();
	for (int i = 0; i < child_count; i++) {
		if (!r_g.errors.is_empty()) {
			return;
		}
		walk_scene(p_node->get_child(i), r_g, my);
	}
}

static Error pcm_from_wav(const Ref<AudioStreamWAV> &p_wav, Vector<int16_t> &r_pcm, int &r_rate) {
	if (p_wav.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	r_rate = p_wav->get_mix_rate() > 0 ? p_wav->get_mix_rate() : 22050;
	const Vector<uint8_t> data = p_wav->get_data();
	const bool stereo = p_wav->is_stereo();
	r_pcm.clear();
	if (p_wav->get_format() == AudioStreamWAV::FORMAT_16_BITS) {
		const int step = stereo ? 4 : 2;
		for (int i = 0; i + 1 < data.size(); i += step) {
			r_pcm.push_back(int16_t(int(data[i]) | (int(data[i + 1]) << 8)));
		}
	} else {
		const int step = stereo ? 2 : 1;
		for (int i = 0; i < data.size(); i += step) {
			r_pcm.push_back(int16_t((int(data[i]) - 128) * 256));
		}
	}
	return r_pcm.is_empty() ? ERR_INVALID_DATA : OK;
}

static Error write_cooked_wav(const Ref<AudioStreamWAV> &p_wav, int p_beep_hz, int p_beep_ms, Vector<uint8_t> &r_blob) {
	if (p_wav.is_valid()) {
		Vector<int16_t> pcm;
		int rate = 22050;
		if (pcm_from_wav(p_wav, pcm, rate) == OK) {
			const Error err = write_wav_pcm(pcm, rate, r_blob);
			if (err == OK) {
				return OK;
			}
			if (err == ERR_OUT_OF_MEMORY) {
				return err;
			}
		}
	}
	return write_wav_beep(p_beep_hz, p_beep_ms, 22050, r_blob);
}

static Vector<uint8_t> write_node_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('N');
	out.push_back('O');
	out.push_back('D');
	out.push_back('E');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.nodes.size()));
	for (int i = 0; i < p_g.nodes.size(); i++) {
		const NodeRow &n = p_g.nodes[i];
		append_u16(out, uint16_t(i));
		append_i16(out, int16_t(n.parent));
		out.push_back(n.kind);
		out.push_back(0);
		append_f32(out, n.pos.x);
		append_f32(out, n.pos.y);
		append_f32(out, n.pos.z);
		append_f32(out, n.rot.x);
		append_f32(out, n.rot.y);
		append_f32(out, n.rot.z);
		append_f32(out, n.scale.x);
		append_f32(out, n.scale.y);
		append_f32(out, n.scale.z);
		const CharString utf = n.name.utf8();
		for (int c = 0; c < 32; c++) {
			out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
	}
	return out;
}

Error cook_project(const String &p_export_dir, CookResult &r_out) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const Error mk = da->make_dir_recursive(p_export_dir);
	ERR_FAIL_COND_V(mk != OK && mk != ERR_ALREADY_EXISTS, mk);

	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_out.errors.push_back("N64 cook requires application/run/main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	if (!ResourceLoader::exists(main_scene)) {
		r_out.errors.push_back("main_scene does not exist: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Ref<PackedScene> ps = ResourceLoader::load(main_scene);
	if (ps.is_null()) {
		r_out.errors.push_back("failed to load main_scene: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Node *root = ps->instantiate();
	if (!root) {
		r_out.errors.push_back("failed to instantiate main_scene: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Gather gathered;
	walk_scene(root, gathered, -1);
	memdelete(root);

	if (!gathered.errors.is_empty()) {
		r_out.errors = gathered.errors;
		r_out.report = gathered.errors[0];
		return FAILED;
	}
	if (gathered.nodes.is_empty()) {
		r_out.errors.push_back("N64 cook found no nodes in main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	const Vector<uint8_t> blob = write_node_blob(gathered);
	const String node_path = p_export_dir.path_join("NODE00.bin");
	Ref<FileAccess> f = FileAccess::open(node_path, FileAccess::WRITE);
	if (f.is_null()) {
		r_out.errors.push_back("cannot write " + node_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	f->store_buffer(blob);

	r_out.node_path = node_path;
	r_out.nodes = gathered.nodes.size();

	if (gathered.verts.is_empty()) {
		r_out.errors.push_back("N64 cook requires at least one MeshInstance3D triangle.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	Vector<uint8_t> mesh_blob;
	int tris = 0;
	const Error mesh_err = write_mesh_blob(gathered.verts, mesh_blob, tris);
	if (mesh_err != OK) {
		r_out.errors.push_back(mesh_err == ERR_OUT_OF_MEMORY ? vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris) : String("MESH cook failed."));
		r_out.report = r_out.errors[0];
		return mesh_err;
	}
	const String mesh_path = p_export_dir.path_join("MESH00.bin");
	Ref<FileAccess> mf = FileAccess::open(mesh_path, FileAccess::WRITE);
	if (mf.is_null()) {
		r_out.errors.push_back("cannot write " + mesh_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	mf->store_buffer(mesh_blob);
	r_out.mesh_path = mesh_path;
	r_out.tris = tris;

	Ref<Image> albedo = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 16; x++) {
			const bool cell = (((x / 4) + (y / 4)) & 1) != 0;
			albedo->set_pixel(x, y, cell ? Color(0.2f, 0.45f, 0.85f) : Color(0.85f, 0.85f, 0.95f));
		}
	}
	Vector<uint8_t> ntex_blob;
	const Error ntex_err = write_ntex_blob(albedo, ntex_blob);
	if (ntex_err != OK) {
		r_out.errors.push_back(ntex_err == ERR_OUT_OF_MEMORY ? String("NTEX exceeds 4 KiB TMEM.") : String("NTEX cook failed."));
		r_out.report = r_out.errors[0];
		return ntex_err;
	}
	const String ntex_path = p_export_dir.path_join("NTEX00.bin");
	Ref<FileAccess> tf = FileAccess::open(ntex_path, FileAccess::WRITE);
	if (tf.is_null()) {
		r_out.errors.push_back("cannot write " + ntex_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	tf->store_buffer(ntex_blob);
	r_out.ntex_path = ntex_path;

	Vector<uint8_t> inp_blob;
	int actions = 0;
	const Error inp_err = write_inp_blob(inp_blob, actions);
	if (inp_err != OK) {
		r_out.errors.push_back(inp_err == ERR_OUT_OF_MEMORY ? vformat("N64 cook refuses more than %d input actions (no silent drop).", kMaxActions) : String("INP6 cook failed."));
		r_out.report = r_out.errors[0];
		return inp_err;
	}
	const String inp_path = p_export_dir.path_join("INP600.bin");
	Ref<FileAccess> inf = FileAccess::open(inp_path, FileAccess::WRITE);
	if (inf.is_null()) {
		r_out.errors.push_back("cannot write " + inp_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	inf->store_buffer(inp_blob);
	r_out.inp_path = inp_path;
	r_out.actions = actions;

	Vector<uint8_t> sfx_blob;
	const Error sfx_err = write_cooked_wav(gathered.sfx, 880, 120, sfx_blob);
	if (sfx_err != OK) {
		r_out.errors.push_back(sfx_err == ERR_OUT_OF_MEMORY ? String("N64 cook refuses SFX WAV over 512 KiB (no silent drop).") : String("SFX WAV cook failed."));
		r_out.report = r_out.errors[0];
		return sfx_err;
	}
	const String sfx_path = p_export_dir.path_join("SFX00.wav");
	Ref<FileAccess> sxf = FileAccess::open(sfx_path, FileAccess::WRITE);
	if (sxf.is_null()) {
		r_out.errors.push_back("cannot write " + sfx_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	sxf->store_buffer(sfx_blob);
	r_out.sfx_path = sfx_path;

	Vector<uint8_t> music_blob;
	const Error mus_err = write_cooked_wav(gathered.music, 440, 400, music_blob);
	if (mus_err != OK) {
		r_out.errors.push_back(mus_err == ERR_OUT_OF_MEMORY ? String("N64 cook refuses MUSIC WAV over 512 KiB (no silent drop).") : String("MUSIC WAV cook failed."));
		r_out.report = r_out.errors[0];
		return mus_err;
	}
	const String music_path = p_export_dir.path_join("MUSIC00.wav");
	Ref<FileAccess> muf = FileAccess::open(music_path, FileAccess::WRITE);
	if (muf.is_null()) {
		r_out.errors.push_back("cannot write " + music_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	muf->store_buffer(music_blob);
	r_out.music_path = music_path;

	Vector<uint8_t> pack_blob;
	const Error pack_err = write_pack_blob(1, "pack01", pack_blob);
	if (pack_err != OK) {
		r_out.errors.push_back(pack_err == ERR_OUT_OF_MEMORY ? vformat("N64 cook refuses pack id outside 1..%d (no silent drop).", kMaxPacks - 1) : String("PACK cook failed."));
		r_out.report = r_out.errors[0];
		return pack_err;
	}
	const String pack_path = p_export_dir.path_join("PACK01.bin");
	Ref<FileAccess> pkf = FileAccess::open(pack_path, FileAccess::WRITE);
	if (pkf.is_null()) {
		r_out.errors.push_back("cannot write " + pack_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	pkf->store_buffer(pack_blob);
	r_out.pack_path = pack_path;

	Vector<uint8_t> script_blob;
	const Error scr_err = write_script_blob(gathered.scripts, script_blob);
	if (scr_err != OK) {
		r_out.errors.push_back("N64 cook refuses attached script that is not on the allow-list (no silent drop).");
		r_out.report = r_out.errors[0];
		return scr_err;
	}
	const String script_path = p_export_dir.path_join("SCRP00.bin");
	Ref<FileAccess> scf = FileAccess::open(script_path, FileAccess::WRITE);
	if (scf.is_null()) {
		r_out.errors.push_back("cannot write " + script_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	scf->store_buffer(script_blob);
	r_out.script_path = script_path;

	String ir_text = "IR rotate_y\n";
	Ref<DirAccess> examples = DirAccess::open("res://examples");
	if (examples.is_valid()) {
		examples->list_dir_begin();
		String name = examples->get_next();
		while (!name.is_empty()) {
			if (!examples->current_is_dir() && name.ends_with(".gd")) {
				const String path = String("res://examples/").path_join(name);
				const String src = FileAccess::get_file_as_string(path);
				if (source_is_forbidden(src)) {
					ir_text += "REJECT " + path + "\n";
				}
			}
			name = examples->get_next();
		}
		examples->list_dir_end();
	}
	const String ir_path = p_export_dir.path_join("SCRIPT_IR.txt");
	Ref<FileAccess> irf = FileAccess::open(ir_path, FileAccess::WRITE);
	if (irf.is_null()) {
		r_out.errors.push_back("cannot write " + ir_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	irf->store_string(ir_text);

	const String names_path = p_export_dir.path_join("NAMES.MAP");
	Ref<FileAccess> names = FileAccess::open(names_path, FileAccess::WRITE);
	if (names.is_null()) {
		r_out.errors.push_back("cannot write " + names_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	names->store_line(main_scene + " -> NODE00.bin MESH00.bin NTEX00.bin INP600.bin SFX00.wav MUSIC00.wav PACK01.bin SCRP00.bin");
	for (int i = 0; i < gathered.nodes.size(); i++) {
		names->store_line(vformat("node %d %s", i, gathered.nodes[i].name));
	}
	r_out.names_path = names_path;

	const int tmem = albedo->get_width() * albedo->get_height() * 2;
	const int rdram = kFbZBytes + kGuestReserve + blob.size() + mesh_blob.size() + ntex_blob.size() + inp_blob.size() + sfx_blob.size() + music_blob.size() + pack_blob.size() + script_blob.size();
	const int rom = blob.size() + mesh_blob.size() + ntex_blob.size() + inp_blob.size() + sfx_blob.size() + music_blob.size() + pack_blob.size() + script_blob.size() + kRomPad;
	r_out.tmem_bytes = tmem;
	r_out.rdram_bytes = rdram;
	r_out.rom_bytes = rom;
	r_out.over_budget = tmem > kTmemBytes || rdram > kRdramBudget || rom > kRomBudget;

	const String budget_path = p_export_dir.path_join("BUDGET.txt");
	Ref<FileAccess> budget = FileAccess::open(budget_path, FileAccess::WRITE);
	if (budget.is_null()) {
		r_out.errors.push_back("cannot write " + budget_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	budget->store_line(vformat("ABI %d", kCookABI));
	budget->store_line(vformat("nodes %d/%d", r_out.nodes, kMaxNodes));
	budget->store_line(vformat("tris %d/%d", r_out.tris, kMaxTris));
	budget->store_line(vformat("tmem %d/%d", tmem, kTmemBytes));
	budget->store_line(vformat("rdram %d/%d", rdram, kRdramBudget));
	budget->store_line(vformat("rom %d/%d", rom, kRomBudget));
	budget->store_line(vformat("display %dx%d", kDisplayW, kDisplayH));
	r_out.budget_path = budget_path;

	if (r_out.over_budget) {
		r_out.errors.push_back("N64 cook exceeded TMEM, RDRAM, or ROM budget.");
		r_out.report = r_out.errors[0];
		return ERR_OUT_OF_MEMORY;
	}

	r_out.report = vformat("cooked %d nodes, %d tris, %d actions + SFX/MUSIC + PACK01 + SCRP to NODE/MESH/NTEX/INP6 + NAMES.MAP + BUDGET.txt", r_out.nodes, r_out.tris, r_out.actions);
	return OK;
}

} // namespace n64_cook
