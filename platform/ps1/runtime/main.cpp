/**************************************************************************/
/*  main.cpp                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
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

#include <cstddef>
#include <cstdint>
#include <inline_c.h>
#include <psxapi.h>
#include <psxcd.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include <psxpress.h>
#include <psxsn.h>
#include <psxspu.h>
#include <string.h>

#include "fmv_play.h"
#include "script_vm.h"

#ifndef BLAZIUM_PS1_COOK_ABI
#define BLAZIUM_PS1_COOK_ABI 19
#endif

#define OT_LEN 2048
#define PACKET_LEN 368640
#define TIM_SLOTS 16
#define SCREEN_W g_screen_w
#define SCREEN_H g_screen_h

static int g_screen_w = 320;
static int g_screen_h = 240;
static int g_region = 0;

#ifdef BLAZIUM_PS1_HAS_TIM
extern const uint8_t cooked_tim[];
extern const size_t cooked_tim_size;
#endif

#ifdef BLAZIUM_PS1_HAS_MESH
extern const uint8_t cooked_mesh[];
extern const size_t cooked_mesh_size;
#endif

#ifdef BLAZIUM_PS1_HAS_VAG
extern const uint8_t cooked_vag[];
extern const size_t cooked_vag_size;
#endif

#ifdef BLAZIUM_PS1_HAS_SPRITE
extern const uint8_t cooked_sprite[];
extern const size_t cooked_sprite_size;
#endif

#ifdef BLAZIUM_PS1_HAS_SCRIPT
extern const uint8_t cooked_script[];
extern const size_t cooked_script_size;
#endif

#ifdef BLAZIUM_PS1_HAS_GDBC
extern const uint8_t cooked_gdbc[];
extern const size_t cooked_gdbc_size;
#endif

#ifdef BLAZIUM_PS1_HAS_LUAU
extern const uint8_t cooked_luau[];
extern const size_t cooked_luau_size;
#endif

#ifdef BLAZIUM_PS1_HAS_STR
extern const uint8_t cooked_str[];
extern const size_t cooked_str_size;
#endif

#ifdef BLAZIUM_PS1_HAS_XA
extern const uint8_t cooked_xa[];
extern const size_t cooked_xa_size;
#endif

#ifdef BLAZIUM_PS1_HAS_NODE
extern const uint8_t cooked_node[];
extern const size_t cooked_node_size;
#endif

#ifdef BLAZIUM_PS1_HAS_HUD
extern const uint8_t cooked_hud[];
extern const size_t cooked_hud_size;
#endif

#ifdef BLAZIUM_PS1_HAS_TILE
extern const uint8_t cooked_tile[];
extern const size_t cooked_tile_size;
#endif

#ifdef BLAZIUM_PS1_HAS_SCENE
extern const uint8_t cooked_scene[];
extern const size_t cooked_scene_size;
#endif

#ifdef BLAZIUM_PS1_HAS_ANIM
extern const uint8_t cooked_anim[];
extern const size_t cooked_anim_size;
#endif

#ifdef BLAZIUM_PS1_HAS_CAM
extern const uint8_t cooked_cam[];
extern const size_t cooked_cam_size;
#endif

#ifdef BLAZIUM_PS1_HAS_HIT
extern const uint8_t cooked_hit[];
extern const size_t cooked_hit_size;
#endif

static MATRIX g_color_mtx = {
	ONE * 3 / 4, ONE / 2, ONE / 4,
	ONE * 3 / 4, ONE / 2, ONE / 4,
	ONE * 3 / 4, ONE / 2, ONE / 4
};
static MATRIX g_light_mtx = {
	-2048, -2048, -2048,
	2048, -1024, 0,
	0, 2048, -1024
};
static uint8_t g_tile_w = 16;
static uint8_t g_tile_h = 16;

static uint8_t g_pad[2][34];

typedef struct {
	DISPENV disp;
	DRAWENV draw;
	uint32_t ot[OT_LEN];
	uint8_t packet[PACKET_LEN];
} FrameBuf;

static FrameBuf g_fb[2];
static int g_active;
static uint8_t *g_pri;

static uint8_t g_tim_used[TIM_SLOTS];
#ifdef BLAZIUM_PS1_HAS_TIM
static int16_t g_tp_x[TIM_SLOTS] = {
	640, 704, 768, 832, 640, 704, 768, 832, 640, 704, 768, 832, 640, 704, 768, 832
};
static int16_t g_tp_y[TIM_SLOTS] = {
	0, 0, 0, 0, 64, 64, 64, 64, 128, 128, 128, 128, 192, 192, 192, 192
};
static int16_t g_cl_x[TIM_SLOTS] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int16_t g_cl_y[TIM_SLOTS] = {
	480, 479, 478, 477, 476, 475, 474, 473, 472, 471, 470, 469, 468, 467, 466, 465
};

static void record_tim_dest(int slot, const uint8_t *tim, size_t remain) {
	if (slot < 0 || slot >= TIM_SLOTS || remain < 20 || tim[0] != 0x10) {
		return;
	}
	const uint8_t *p = tim + 8;
	const uint32_t flags = uint32_t(tim[4]) | (uint32_t(tim[5]) << 8) | (uint32_t(tim[6]) << 16) | (uint32_t(tim[7]) << 24);
	if (flags & 8) {
		g_cl_x[slot] = int16_t(p[4] | (p[5] << 8));
		g_cl_y[slot] = int16_t(p[6] | (p[7] << 8));
		const uint32_t csz = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		p += csz;
	}
	g_tp_x[slot] = int16_t(p[4] | (p[5] << 8));
	g_tp_y[slot] = int16_t(p[6] | (p[7] << 8));
}

static size_t tim_blob_size(const uint8_t *tim, size_t remain) {
	if (remain < 20 || tim[0] != 0x10) {
		return 0;
	}
	size_t used = 8;
	const uint8_t *p = tim + 8;
	if (tim[4] & 8) {
		const uint32_t csz = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		if (csz < 12 || used + csz > remain) {
			return 0;
		}
		p += csz;
		used += csz;
	}
	const uint32_t isz = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
	if (isz < 12 || used + isz > remain) {
		return 0;
	}
	return used + isz;
}

static void upload_one_tim(const uint8_t *tim, size_t remain) {
	if (remain < 20 || tim[0] != 0x10) {
		return;
	}
	const uint8_t *p = tim + 8;
	const uint32_t flags = uint32_t(tim[4]) | (uint32_t(tim[5]) << 8) | (uint32_t(tim[6]) << 16) | (uint32_t(tim[7]) << 24);
	if (flags & 8) {
		const uint16_t cx = uint16_t(p[4] | (p[5] << 8));
		const uint16_t cy = uint16_t(p[6] | (p[7] << 8));
		const uint16_t cw = uint16_t(p[8] | (p[9] << 8));
		const uint16_t ch = uint16_t(p[10] | (p[11] << 8));
		const uint32_t csz = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		RECT cr{ short(cx), short(cy), short(cw), short(ch) };
		LoadImage(&cr, (const uint32_t *)(p + 12));
		DrawSync(0);
		p += csz;
	}
	const uint16_t ix = uint16_t(p[4] | (p[5] << 8));
	const uint16_t iy = uint16_t(p[6] | (p[7] << 8));
	const uint16_t iw = uint16_t(p[8] | (p[9] << 8));
	const uint16_t ih = uint16_t(p[10] | (p[11] << 8));
	RECT ir{ short(ix), short(iy), short(iw), short(ih) };
	LoadImage(&ir, (const uint32_t *)(p + 12));
	DrawSync(0);
}

static void upload_cooked_tim() {
	if (cooked_tim_size >= 8 && cooked_tim[0] == 'T' && cooked_tim[1] == 'P' && cooked_tim[2] == 'A' && cooked_tim[3] == 'K') {
		const uint16_t n = uint16_t(cooked_tim[4] | (cooked_tim[5] << 8));
		const uint8_t *p = cooked_tim + 8;
		size_t left = cooked_tim_size - 8;
		for (uint16_t i = 0; i < n; i++) {
			const size_t sz = tim_blob_size(p, left);
			if (sz == 0) {
				break;
			}
			record_tim_dest(int(i), p, sz);
			upload_one_tim(p, sz);
			if (i < TIM_SLOTS) {
				g_tim_used[i] = 1;
			}
			p += sz;
			left -= sz;
		}
		return;
	}
	upload_one_tim(cooked_tim, cooked_tim_size);
	g_tim_used[0] = 1;
}
#endif

static void flip_frame() {
	DrawSync(0);
	VSync(0);
	FrameBuf *draw = &g_fb[g_active];
	FrameBuf *disp = &g_fb[g_active ^ 1];
	PutDispEnv(&disp->disp);
	DrawOTagEnv(&draw->ot[OT_LEN - 1], &draw->draw);
	g_active ^= 1;
	g_pri = g_fb[g_active].packet;
	ClearOTagR(g_fb[g_active].ot, OT_LEN);
}

#ifdef BLAZIUM_PS1_HAS_VAG
static int g_vag_on = 0;

static void play_cooked_vag() {
	if (cooked_vag_size < 64) {
		return;
	}
	SpuInit();
	SpuSetCommonMasterVolume(0x3fff, 0x3fff);
	const uint32_t data_size = (uint32_t(cooked_vag[12]) << 24) | (uint32_t(cooked_vag[13]) << 16) | (uint32_t(cooked_vag[14]) << 8) | uint32_t(cooked_vag[15]);
	const uint32_t rate = (uint32_t(cooked_vag[16]) << 24) | (uint32_t(cooked_vag[17]) << 16) | (uint32_t(cooked_vag[18]) << 8) | uint32_t(cooked_vag[19]);
	const uint32_t addr = 0x1010;
	uint32_t xfer = (data_size + 63) & ~uint32_t(63);
	if (48 + xfer > cooked_vag_size) {
		xfer = cooked_vag_size - 48;
	}
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(addr);
	SpuWrite((const uint32_t *)(cooked_vag + 48), xfer);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
	SpuSetKey(0, 1 << 0);
	SpuSetVoiceVolume(0, 0x3fff, 0x3fff);
	SpuSetVoicePitch(0, getSPUSampleRate(int(rate ? rate : 22050)));
	SpuSetVoiceStartAddr(0, addr);
	SPU_CH_LOOP_ADDR(0) = getSPUAddr(addr);
	SPU_CH_ADSR1(0) = 0x00ff;
	SPU_CH_ADSR2(0) = 0x0000;
	SpuSetKey(1, 1 << 0);
	g_vag_on = 1;
}

static void stop_cooked_vag() {
	SpuSetKey(0, 1 << 0);
	SpuSetVoiceVolume(0, 0, 0);
	g_vag_on = 0;
}

static int cooked_vag_playing() {
	return g_vag_on;
}
#endif

static uint8_t g_rumble_mot[2][2];
static int g_pad_sio = 0;
#define JOY_TXRX (*(volatile uint8_t *)0x1F801040)
#define JOY_STAT (*(volatile uint16_t *)0x1F801044)
#define JOY_MODE (*(volatile uint16_t *)0x1F801048)
#define JOY_CTRL (*(volatile uint16_t *)0x1F80104A)
#define JOY_BAUD (*(volatile uint16_t *)0x1F80104E)

static int joy_spin(uint16_t mask, int want, int n) {
	while (n--) {
		if (((JOY_STAT & mask) != 0) == want) {
			return 1;
		}
	}
	return 0;
}

static uint8_t joy_xfer(uint8_t tx) {
	joy_spin(1, 1, 4000);
	JOY_TXRX = tx;
	joy_spin(2, 1, 4000);
	return JOY_TXRX;
}

static int pad_exchange(int port, const uint8_t *tx, int txn, uint8_t *rx, int rxn) {
	JOY_CTRL = 0x40;
	for (int i = 0; i < 20; i++) {
		(void)JOY_STAT;
	}
	JOY_BAUD = 0x88;
	JOY_MODE = 0x000D;
	JOY_CTRL = uint16_t(0x1003 | (port ? 0x2000 : 0));
	if (!joy_spin(2, 0, 2000)) {
		JOY_CTRL = 0;
		return 0;
	}
	int ok = 1;
	for (int i = 0; i < txn || i < rxn; i++) {
		const uint8_t out = (i < txn) ? tx[i] : 0x00;
		const uint8_t in = joy_xfer(out);
		if (i < rxn && rx) {
			rx[i] = in;
		}
		if (i + 1 < txn || i + 1 < rxn) {
			if (!joy_spin(0x80, 1, 2000)) {
				ok = 0;
				break;
			}
			JOY_CTRL |= 0x10;
		}
	}
	JOY_CTRL = 0;
	return ok;
}

static int analog_enter_port(int port) {
	uint8_t rx[9];
	const uint8_t enter[] = { 0x01, uint8_t(PAD_CMD_CONFIG_MODE), 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t analog[] = { 0x01, uint8_t(PAD_CMD_SET_ANALOG), 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00 };
	const uint8_t motors[] = { 0x01, uint8_t(PAD_CMD_REQUEST_CONFIG), 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
	const uint8_t leave[] = { 0x01, uint8_t(PAD_CMD_CONFIG_MODE), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	if (!pad_exchange(port, enter, 9, rx, 9)) {
		return 0;
	}
	pad_exchange(port, analog, 9, rx, 9);
	pad_exchange(port, motors, 9, rx, 9);
	pad_exchange(port, leave, 9, rx, 9);
	return 1;
}

static void pad_poll_motors() {
	if (!g_pad_sio) {
		return;
	}
	for (int port = 0; port < 2; port++) {
		uint8_t tx[9] = { uint8_t(port ? 0x02 : 0x01), uint8_t(PAD_CMD_READ), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
		tx[3] = g_rumble_mot[port][0] ? 0xFF : 0x00;
		tx[4] = g_rumble_mot[port][1];
		uint8_t rx[9];
		if (pad_exchange(port, tx, 9, rx, 9) && rx[1] != 0xFF) {
			g_pad[port][0] = 0;
			g_pad[port][1] = rx[1];
			for (int i = 2; i < 9 && i < 34; i++) {
				g_pad[port][i] = rx[i];
			}
		}
	}
}

static void try_analog_enter() {
	StopPAD();
	ChangeClearPAD(0);
	int ok = analog_enter_port(0);
	analog_enter_port(1);
	if (ok) {
		g_pad_sio = 1;
		pad_poll_motors();
	} else {
		g_pad_sio = 0;
		InitPAD(g_pad[0], 34, g_pad[1], 34);
		StartPAD();
		ChangeClearPAD(0);
	}
}

static void host_set_rumble(int device, int small, int large) {
	if (device != 0 && device != 1) {
		return;
	}
	g_rumble_mot[device][0] = uint8_t(small < 0 ? 0 : (small > 255 ? 255 : small));
	g_rumble_mot[device][1] = uint8_t(large < 0 ? 0 : (large > 255 ? 255 : large));
}

#ifdef BLAZIUM_PS1_HAS_TIM
static int host_upload_tpak(const uint8_t *blob, int size, int *lo, int *hi) {
	if (!blob || size < 8 || blob[0] != 'T' || blob[1] != 'P' || blob[2] != 'A' || blob[3] != 'K') {
		return 0;
	}
	const uint16_t n = uint16_t(blob[4] | (blob[5] << 8));
	int free_n = 0;
	int slots[TIM_SLOTS];
	int ns = 0;
	for (int i = 0; i < TIM_SLOTS; i++) {
		if (!g_tim_used[i]) {
			if (ns < TIM_SLOTS) {
				slots[ns++] = i;
			}
			free_n++;
		}
	}
	if (int(n) > free_n || int(n) > ns) {
		return 0;
	}
	const uint8_t *p = blob + 8;
	size_t left = size_t(size - 8);
	const uint8_t *ents[TIM_SLOTS];
	for (uint16_t i = 0; i < n; i++) {
		const size_t sz = tim_blob_size(p, left);
		if (sz == 0) {
			return 0;
		}
		ents[i] = p;
		p += sz;
		left -= sz;
	}
	int first = -1, last = -1;
	for (uint16_t i = 0; i < n; i++) {
		const int slot = slots[i];
		const uint8_t *tim = ents[i];
		RECT ir{ g_tp_x[slot], g_tp_y[slot], 64, 64 };
		const uint8_t *tp = tim + 8;
		if (tim[4] & 8) {
			const uint32_t csz = uint32_t(tp[0] | (tp[1] << 8) | (tp[2] << 16) | (tp[3] << 24));
			RECT cr{ g_cl_x[slot], g_cl_y[slot], short(tp[8] | (tp[9] << 8)), short(tp[10] | (tp[11] << 8)) };
			LoadImage(&cr, (const uint32_t *)(tp + 12));
			DrawSync(0);
			tp += csz;
		}
		ir.w = short(tp[8] | (tp[9] << 8));
		ir.h = short(tp[10] | (tp[11] << 8));
		LoadImage(&ir, (const uint32_t *)(tp + 12));
		DrawSync(0);
		g_tim_used[slot] = 1;
		if (first < 0) {
			first = slot;
		}
		last = slot;
	}
	if (lo) {
		*lo = first < 0 ? 0 : first;
	}
	if (hi) {
		*hi = last < 0 ? 0 : last + 1;
	}
	return 1;
}

static void host_evict_tpak(int lo, int hi) {
	if (lo < 0) {
		lo = 0;
	}
	if (hi > TIM_SLOTS) {
		hi = TIM_SLOTS;
	}
	for (int i = lo; i < hi; i++) {
		g_tim_used[i] = 0;
	}
}
#else
static int host_upload_tpak(const uint8_t *blob, int size, int *lo, int *hi) {
	(void)blob;
	(void)size;
	if (lo) {
		*lo = 0;
	}
	if (hi) {
		*hi = 0;
	}
	return 0;
}
static void host_evict_tpak(int lo, int hi) {
	(void)lo;
	(void)hi;
}
#endif

struct SfxClip {
	char name[16];
	uint32_t addr;
	uint32_t size;
	uint32_t rate;
	int voice;
	uint32_t age;
	int vol;
};
static SfxClip g_sfx[16];
static int g_nsfx = 0;
static uint32_t g_sfx_tick = 1;
static int g_music_vol = 0x3fff;

static int host_load_sfx(const uint8_t *blob, int size) {
	if (!blob || size < 8 || blob[0] != 'S' || blob[1] != 'F' || blob[2] != 'X' || blob[3] != '0') {
		return 0;
	}
#ifdef BLAZIUM_PS1_HAS_VAG
	const int nc = int(blob[6] | (blob[7] << 8));
	if (nc <= 0 || nc > 16) {
		return 0;
	}
	SfxClip parsed[16];
	uint32_t xfers[16];
	uint32_t cursor = 0x1010 + 0x8000;
	for (int i = 0; i < nc; i++) {
		const uint8_t *row = blob + 8 + i * 28;
		SfxClip c{};
		for (int k = 0; k < 16; k++) {
			c.name[k] = char(row[k]);
		}
		c.name[15] = 0;
		const uint32_t off = uint32_t(row[16] | (row[17] << 8) | (row[18] << 16) | (row[19] << 24));
		c.size = uint32_t(row[20] | (row[21] << 8) | (row[22] << 16) | (row[23] << 24));
		c.rate = uint32_t(row[24] | (row[25] << 8));
		if (!c.rate) {
			c.rate = 22050;
		}
		if (off + c.size > uint32_t(size) || c.size < 48) {
			return 0;
		}
		uint32_t xfer = (c.size + 63) & ~uint32_t(63);
		if (off + xfer > uint32_t(size)) {
			xfer = c.size;
		}
		c.addr = cursor;
		c.voice = -1;
		c.vol = 0x3fff;
		parsed[i] = c;
		xfers[i] = xfer;
		cursor += xfer;
	}
	g_nsfx = 0;
	SpuInit();
	for (int i = 0; i < nc; i++) {
		const uint8_t *row = blob + 8 + i * 28;
		const uint32_t off = uint32_t(row[16] | (row[17] << 8) | (row[18] << 16) | (row[19] << 24));
		SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
		SpuSetTransferStartAddr(parsed[i].addr);
		SpuWrite((const uint32_t *)(blob + off), xfers[i]);
		SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
		g_sfx[g_nsfx++] = parsed[i];
	}
	return 1;
#else
	(void)size;
	return 0;
#endif
}

static void host_unload_sfx() {
#ifdef BLAZIUM_PS1_HAS_VAG
	for (int i = 0; i < g_nsfx; i++) {
		if (g_sfx[i].voice >= 1) {
			SpuSetKey(0, 1 << g_sfx[i].voice);
		}
	}
#endif
	g_nsfx = 0;
}

static int host_play_sfx(const char *name) {
#ifdef BLAZIUM_PS1_HAS_VAG
	if (!name || !name[0] || !g_nsfx) {
		return 0;
	}
	int clip = -1;
	for (int i = 0; i < g_nsfx; i++) {
		int same = 1;
		for (int k = 0; k < 16 && (name[k] || g_sfx[i].name[k]); k++) {
			if (name[k] != g_sfx[i].name[k]) {
				same = 0;
				break;
			}
		}
		if (same) {
			clip = i;
			break;
		}
	}
	if (clip < 0) {
		return 0;
	}
	int voice = -1;
	uint32_t oldest = 0xFFFFFFFFu;
	int oldv = 1;
	for (int v = 1; v < 8; v++) {
		int used = 0;
		for (int i = 0; i < g_nsfx; i++) {
			if (g_sfx[i].voice == v) {
				used = 1;
				if (g_sfx[i].age < oldest) {
					oldest = g_sfx[i].age;
					oldv = v;
				}
			}
		}
		if (!used) {
			voice = v;
			break;
		}
	}
	if (voice < 0) {
		voice = oldv;
		for (int i = 0; i < g_nsfx; i++) {
			if (g_sfx[i].voice == voice) {
				g_sfx[i].voice = -1;
			}
		}
	}
	SpuSetKey(0, 1 << voice);
	SpuSetVoiceVolume(voice, int16_t(g_sfx[clip].vol), int16_t(g_sfx[clip].vol));
	SpuSetVoicePitch(voice, getSPUSampleRate(int(g_sfx[clip].rate)));
	SpuSetVoiceStartAddr(voice, g_sfx[clip].addr);
	SPU_CH_ADSR1(voice) = 0x00ff;
	SPU_CH_ADSR2(voice) = 0x0000;
	SpuSetKey(1, 1 << voice);
	g_sfx[clip].voice = voice;
	g_sfx[clip].age = g_sfx_tick++;
	return 1;
#else
	(void)name;
	return 0;
#endif
}

static void host_stop_sfx(const char *name) {
#ifdef BLAZIUM_PS1_HAS_VAG
	for (int i = 0; i < g_nsfx; i++) {
		int same = 1;
		if (name) {
			for (int k = 0; k < 16 && (name[k] || g_sfx[i].name[k]); k++) {
				if (name[k] != g_sfx[i].name[k]) {
					same = 0;
					break;
				}
			}
		}
		if (same && g_sfx[i].voice >= 1) {
			SpuSetKey(0, 1 << g_sfx[i].voice);
			g_sfx[i].voice = -1;
		}
	}
#else
	(void)name;
#endif
}

static void host_set_sfx_vol(const char *name, int vol) {
#ifdef BLAZIUM_PS1_HAS_VAG
	if (vol < 0) {
		vol = 0;
	}
	if (vol > 0x3fff) {
		vol = 0x3fff;
	}
	for (int i = 0; i < g_nsfx; i++) {
		int same = 1;
		if (name) {
			for (int k = 0; k < 16 && (name[k] || g_sfx[i].name[k]); k++) {
				if (name[k] != g_sfx[i].name[k]) {
					same = 0;
					break;
				}
			}
		}
		if (same) {
			g_sfx[i].vol = vol;
			if (g_sfx[i].voice >= 1) {
				SpuSetVoiceVolume(g_sfx[i].voice, int16_t(vol), int16_t(vol));
			}
		}
	}
#else
	(void)name;
	(void)vol;
#endif
}

static int host_load_music(const uint8_t *blob, int size) {
	if (!blob || size < 64) {
		return 0;
	}
	SpuInit();
	SpuSetCommonMasterVolume(0x3fff, 0x3fff);
	const uint32_t data_size = (uint32_t(blob[12]) << 24) | (uint32_t(blob[13]) << 16) | (uint32_t(blob[14]) << 8) | uint32_t(blob[15]);
	const uint32_t rate = (uint32_t(blob[16]) << 24) | (uint32_t(blob[17]) << 16) | (uint32_t(blob[18]) << 8) | uint32_t(blob[19]);
	const uint32_t addr = 0x1010;
	uint32_t xfer = (data_size + 63) & ~uint32_t(63);
	if (48 + int(xfer) > size) {
		xfer = uint32_t(size - 48);
	}
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(addr);
	SpuWrite((const uint32_t *)(blob + 48), xfer);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
	SpuSetKey(0, 1 << 0);
	SpuSetVoiceVolume(0, int16_t(g_music_vol), int16_t(g_music_vol));
	SpuSetVoicePitch(0, getSPUSampleRate(int(rate ? rate : 22050)));
	SpuSetVoiceStartAddr(0, addr);
	SPU_CH_LOOP_ADDR(0) = getSPUAddr(addr);
	SPU_CH_ADSR1(0) = 0x00ff;
	SPU_CH_ADSR2(0) = 0x0000;
	SpuSetKey(1, 1 << 0);
#ifdef BLAZIUM_PS1_HAS_VAG
	g_vag_on = 1;
#endif
	return 1;
}

static void host_unload_music() {
	SpuSetKey(0, 1 << 0);
	SpuSetVoiceVolume(0, 0, 0);
#ifdef BLAZIUM_PS1_HAS_VAG
	g_vag_on = 0;
	play_cooked_vag();
#endif
}

static void host_play_fmv_blob(const uint8_t *blob, int size) {
	if (!blob || size <= 0) {
		return;
	}
	fmv_play_embedded(blob, size_t(size), SCREEN_W, SCREEN_H, g_pad[0], nullptr, 0);
}

static int g_cd_ok = 0;

static int cd_ready() {
	if (g_cd_ok) {
		return 1;
	}
	if (CdInit()) {
		g_cd_ok = 1;
		return 1;
	}
	return 0;
}

static void cd_make_path(char *dst, const char *iso_name) {
	dst[0] = '\\';
	int i = 0;
	while (iso_name && iso_name[i] && i < 20) {
		dst[1 + i] = iso_name[i];
		i++;
	}
	dst[1 + i] = ';';
	dst[2 + i] = '1';
	dst[3 + i] = 0;
}

static int host_play_xa(const char *iso_name, int file, int chan) {
	if (!iso_name || !iso_name[0] || !cd_ready()) {
		return 0;
	}
	char path[32];
	cd_make_path(path, iso_name);
	CdlFILE fp;
	if (!CdSearchFile(&fp, path)) {
		return 0;
	}
	CdlFILTER filt;
	filt.file = uint8_t(file > 0 ? file : 1);
	filt.chan = uint8_t(chan < 0 ? 0 : chan);
	filt.pad = 0;
	CdControl(CdlSetfilter, (uint8_t *)&filt, 0);
	uint8_t mode = uint8_t(CdlModeRT | CdlModeSF | CdlModeSpeed);
	CdControl(CdlSetmode, &mode, 0);
	CdControl(CdlReadS, (uint8_t *)&fp.pos, 0);
	return 1;
}

static void host_stop_xa() {
	if (g_cd_ok) {
		CdControl(CdlPause, 0, 0);
	}
}

static int host_play_fmv_cd(const char *iso_name) {
	if (!iso_name || !cd_ready()) {
		return 0;
	}
	char path[32];
	cd_make_path(path, iso_name);
	CdlFILE fp;
	if (!CdSearchFile(&fp, path)) {
		return 0;
	}
	return fmv_play_cd(iso_name, SCREEN_W, SCREEN_H, g_pad[0]);
}

static void host_set_music_vol(int vol) {
#ifdef BLAZIUM_PS1_HAS_VAG
	if (vol < 0) {
		vol = 0;
	}
	if (vol > 0x3fff) {
		vol = 0x3fff;
	}
	g_music_vol = vol;
	SpuSetVoiceVolume(0, int16_t(vol), int16_t(vol));
#else
	(void)vol;
#endif
}

static void host_set_light(int index, int dx, int dy, int dz, int r, int g, int b) {
	if (index < 0 || index > 2) {
		return;
	}
	g_light_mtx.m[0][index] = int16_t(dx);
	g_light_mtx.m[1][index] = int16_t(dy);
	g_light_mtx.m[2][index] = int16_t(dz);
	g_color_mtx.m[0][index] = int16_t((4096 * (r < 0 ? 0 : (r > 255 ? 255 : r))) / 255);
	g_color_mtx.m[1][index] = int16_t((4096 * (g < 0 ? 0 : (g > 255 ? 255 : g))) / 255);
	g_color_mtx.m[2][index] = int16_t((4096 * (b < 0 ? 0 : (b > 255 ? 255 : b))) / 255);
	gte_SetColorMatrix(&g_color_mtx);
}

#ifdef BLAZIUM_PS1_HAS_STR
static void replay_cooked_fmv() {
	const uint8_t *xa = nullptr;
	size_t xa_n = 0;
#ifdef BLAZIUM_PS1_HAS_XA
	xa = cooked_xa;
	xa_n = cooked_xa_size;
#endif
	fmv_play_embedded(cooked_str, cooked_str_size, SCREEN_W, SCREEN_H, g_pad[0], xa, xa_n);
}
#endif

static void try_pcdrv_tim() {
	if (PCinit() != 0) {
		return;
	}
	const int fd = PCopen("TEX00.TIM", PCDRV_MODE_READ);
	if (fd < 0) {
		return;
	}
	uint8_t buf[2112];
	const int n = PCread(fd, buf, sizeof(buf));
	PCclose(fd);
	if (n < 20 || buf[0] != 0x10) {
		return;
	}
	const uint8_t *p = buf + 8;
	if (buf[4] & 8) {
		const uint32_t csz = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		RECT cr{ short(p[4] | (p[5] << 8)), short(p[6] | (p[7] << 8)), short(p[8] | (p[9] << 8)), short(p[10] | (p[11] << 8)) };
		LoadImage(&cr, (const uint32_t *)(p + 12));
		DrawSync(0);
		p += csz;
	}
	RECT ir{ short(p[4] | (p[5] << 8)), short(p[6] | (p[7] << 8)), short(p[8] | (p[9] << 8)), short(p[10] | (p[11] << 8)) };
	LoadImage(&ir, (const uint32_t *)(p + 12));
	DrawSync(0);
}

#ifdef BLAZIUM_PS1_HAS_SPRITE
static void draw_cooked_sprites(uint16_t *tpages, uint16_t *cluts) {
	const ScriptVMSprite *spr = script_vm_sprites();
	const int nspr = script_vm_sprite_count();
	const ScriptVMNode *nodes = script_vm_nodes();
	const int nn = script_vm_node_count();
	for (int ni = 0; ni < nn; ni++) {
		const int si = int(nodes[ni].sprite);
		if (si < 0 || si >= nspr || !(nodes[ni].flags & 1)) {
			continue;
		}
		const uint8_t rgb = spr[si].rgb ? spr[si].rgb : 255;
			const uint8_t u = uint8_t(spr[si].u + spr[si].frame * spr[si].w);
			const uint8_t u1 = spr[si].flip_h ? u : uint8_t(u + spr[si].w);
			const uint8_t u0 = spr[si].flip_h ? uint8_t(u + spr[si].w) : u;
			const uint8_t v0 = spr[si].flip_v ? uint8_t(spr[si].v + spr[si].h) : spr[si].v;
			const uint8_t v1 = spr[si].flip_v ? spr[si].v : uint8_t(spr[si].v + spr[si].h);
		const int16_t x = spr[si].x ? spr[si].x : nodes[ni].px;
		const int16_t y = spr[si].y ? spr[si].y : int16_t(-nodes[ni].py);
		const uint8_t tex = uint8_t(spr[si].tex & 15);
		if (spr[si].billboard) {
			if ((uint8_t *)((POLY_FT4 *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
				break;
			}
			POLY_FT4 *q = (POLY_FT4 *)g_pri;
			setPolyFT4(q);
			setRGB0(q, rgb, rgb, rgb);
			const int16_t hw = int16_t(spr[si].w / 2);
			const int16_t hh = int16_t(spr[si].h / 2);
			setXY4(q, int16_t(x - hw), int16_t(y - hh), int16_t(x + hw), int16_t(y - hh), int16_t(x - hw), int16_t(y + hh), int16_t(x + hw), int16_t(y + hh));
			setUV4(q, u0, v0, u1, v0, u0, v1, u1, v1);
			q->tpage = tpages[tex];
			q->clut = cluts[tex];
			addPrim(&g_fb[g_active].ot[1], q);
			g_pri = (uint8_t *)(q + 1);
		} else {
			if ((uint8_t *)((SPRT *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
				break;
			}
			SPRT *p = (SPRT *)g_pri;
			setSprt(p);
			setRGB0(p, rgb, rgb, rgb);
			setXY0(p, x, y);
			setWH(p, spr[si].w, spr[si].h);
			setUV0(p, u0, v0);
			p->clut = cluts[tex];
			addPrim(&g_fb[g_active].ot[1], p);
			g_pri = (uint8_t *)(p + 1);
		}
	}
	(void)tpages;
}
#endif

static void unpack_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b) {
	*r = uint8_t(((packed >> 5) & 7) * 36);
	*g = uint8_t(((packed >> 2) & 7) * 36);
	*b = uint8_t((packed & 3) * 85);
}

static void draw_tile_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
	if ((uint8_t *)((TILE *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
		return;
	}
	TILE *t = (TILE *)g_pri;
	setTile(t);
	setXY0(t, x, y);
	setWH(t, w, h);
	setRGB0(t, r, g, b);
	addPrim(&g_fb[g_active].ot[1], t);
	g_pri = (uint8_t *)(t + 1);
}

static void draw_cooked_tiles(uint16_t *tpages, uint16_t *cluts) {
	const int n = script_vm_tile_count();
	const ScriptVMTile *tiles = script_vm_tiles();
	const uint8_t tw = g_tile_w ? g_tile_w : 16;
	const uint8_t th = g_tile_h ? g_tile_h : 16;
	const ScriptVMNode *nodes = script_vm_nodes();
	const int nn = script_vm_node_count();
	for (int i = 0; i < n; i++) {
		const int nid = int(tiles[i].node_id);
		if (nid >= 0 && nid < nn && !(nodes[nid].flags & 1)) {
			continue;
		}
		if ((uint8_t *)((SPRT *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
			break;
		}
		SPRT *p = (SPRT *)g_pri;
		setSprt(p);
		setRGB0(p, 255, 255, 255);
		setXY0(p, tiles[i].x, tiles[i].y);
		setWH(p, tw, th);
		setUV0(p, tiles[i].u, tiles[i].v);
		const int slot = tiles[i].tex & 15;
		p->clut = cluts[slot];
		addPrim(&g_fb[g_active].ot[1], p);
		g_pri = (uint8_t *)(p + 1);
	}
	(void)tpages;
}

static void draw_cooked_hud(uint16_t *cluts) {
	const int n = script_vm_hud_count();
	ScriptVMHud *hud = script_vm_hud();
	const int focus = script_vm_hud_focus();
	for (int i = 0; i < n; i++) {
		if (!(hud[i].flags & 1)) {
			continue;
		}
		uint8_t r, g, b;
		unpack_rgb(hud[i].rgb, &r, &g, &b);
		if (hud[i].rgb == 128 && r == 0) {
			r = 128;
			g = 128;
			b = 128;
		}
		if (hud[i].flags & 2) {
			r = uint8_t(r / 2);
			g = uint8_t(g / 2);
			b = uint8_t(b / 2);
		}
		if (i == focus) {
			r = uint8_t(r > 200 ? 255 : r + 55);
			g = uint8_t(g > 200 ? 255 : g + 55);
			b = uint8_t(b > 200 ? 255 : b + 55);
			draw_tile_rect(int16_t(hud[i].x - 1), int16_t(hud[i].y - 1), int16_t(hud[i].w + 2), int16_t(hud[i].h + 2), 255, 255, 80);
		}
		if (hud[i].tex != 0xff) {
			if ((uint8_t *)((SPRT *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
				break;
			}
			SPRT *p = (SPRT *)g_pri;
			setSprt(p);
			setRGB0(p, r, g, b);
			setXY0(p, hud[i].x, hud[i].y);
			setWH(p, hud[i].w, hud[i].h);
			setUV0(p, 0, 0);
			p->clut = cluts[hud[i].tex & 15];
			addPrim(&g_fb[g_active].ot[1], p);
			g_pri = (uint8_t *)(p + 1);
		} else {
			draw_tile_rect(hud[i].x, hud[i].y, hud[i].w, hud[i].h, r, g, b);
		}
		if (hud[i].kind == 10 || hud[i].kind == 11 || hud[i].kind == 12) {
			int span = int(hud[i].vmax) - int(hud[i].vmin);
			if (span < 1) {
				span = 1;
			}
			int fill = (int(hud[i].value) - int(hud[i].vmin)) * int(hud[i].w) / span;
			if (fill < 0) {
				fill = 0;
			}
			if (fill > hud[i].w) {
				fill = hud[i].w;
			}
			if (fill > 0) {
				draw_tile_rect(hud[i].x, hud[i].y, int16_t(fill), hud[i].h, 80, 200, 80);
			}
		}
		char line[40];
		int nch = 0;
		if (hud[i].kind == 5 || hud[i].kind == 6) {
			line[nch++] = '[';
			line[nch++] = (hud[i].flags & 8) ? 'x' : ' ';
			line[nch++] = ']';
			line[nch++] = ' ';
		}
		for (int k = 0; hud[i].text[k] && nch < 38; k++) {
			line[nch++] = hud[i].text[k];
		}
		line[nch] = 0;
		if (nch) {
			g_pri = (uint8_t *)FntSort(&g_fb[g_active].ot[1], g_pri, hud[i].x + 2, hud[i].y + 2, line);
		}
	}
}

#ifdef BLAZIUM_PS1_HAS_MESH
typedef struct {
	SVECTOR v[3];
	uint8_t u0, v0, u1, v1, u2, v2;
	uint8_t tex;
	uint8_t node_id;
} CookedTri;

static int draw_tri_range(const CookedTri *tris, uint16_t tri_n, uint16_t lo, uint16_t hi, const uint16_t *tpages, const uint16_t *cluts) {
	if (lo >= tri_n || lo >= hi) {
		return 0;
	}
	if (hi > tri_n) {
		hi = tri_n;
	}
	POLY_FT3 *poly = (POLY_FT3 *)g_pri;
	int drawn = 0;
	for (uint16_t i = lo; i < hi; i++) {
		if ((uint8_t *)(poly + 1) > g_fb[g_active].packet + PACKET_LEN) {
			break;
		}
		gte_ldv3(&tris[i].v[0], &tris[i].v[1], &tris[i].v[2]);
		gte_rtpt();
		int otz = 0;
		gte_avsz3();
		gte_stotz(&otz);
		otz >>= 2;
		if (otz <= 0 || otz >= OT_LEN) {
			continue;
		}
		setPolyFT3(poly);
		setRGB0(poly, 128, 128, 128);
		{
			SVECTOR n;
			const int32_t ux = tris[i].v[1].vx - tris[i].v[0].vx;
			const int32_t uy = tris[i].v[1].vy - tris[i].v[0].vy;
			const int32_t uz = tris[i].v[1].vz - tris[i].v[0].vz;
			const int32_t vx = tris[i].v[2].vx - tris[i].v[0].vx;
			const int32_t vy = tris[i].v[2].vy - tris[i].v[0].vy;
			const int32_t vz = tris[i].v[2].vz - tris[i].v[0].vz;
			int32_t nx = uy * vz - uz * vy;
			int32_t ny = uz * vx - ux * vz;
			int32_t nz = ux * vy - uy * vx;
			while (nx > 4096 || nx < -4096 || ny > 4096 || ny < -4096 || nz > 4096 || nz < -4096) {
				nx >>= 1;
				ny >>= 1;
				nz >>= 1;
			}
			n.vx = int16_t(nx);
			n.vy = int16_t(ny);
			n.vz = int16_t(nz);
			n.pad = 0;
			gte_ldrgb(&poly->r0);
			gte_ldv0(&n);
			gte_ncs();
			gte_strgb(&poly->r0);
		}
		{
			int fog_on = 1, fog_s = 0, fog_e = OT_LEN;
			uint8_t fr = 32, fg = 0, fb = 48;
			script_vm_get_fog(&fog_on, &fog_s, &fog_e, &fr, &fg, &fb);
			if (fog_on) {
				if (fog_e <= fog_s) {
					fog_e = fog_s + 1;
				}
				int fog = (otz - fog_s) * 128 / (fog_e - fog_s);
				if (fog < 0) {
					fog = 0;
				}
				if (fog > 128) {
					fog = 128;
				}
				poly->r0 = uint8_t((int(poly->r0) * (128 - fog) + int(fr) * fog) >> 7);
				poly->g0 = uint8_t((int(poly->g0) * (128 - fog) + int(fg) * fog) >> 7);
				poly->b0 = uint8_t((int(poly->b0) * (128 - fog) + int(fb) * fog) >> 7);
			}
		}
		gte_stsxy0(&poly->x0);
		gte_stsxy1(&poly->x1);
		gte_stsxy2(&poly->x2);
		setUV3(poly, tris[i].u0, tris[i].v0, tris[i].u1, tris[i].v1, tris[i].u2, tris[i].v2);
		{
			const uint8_t slot = uint8_t(tris[i].tex & 15);
			poly->tpage = tpages[slot];
			poly->clut = cluts[slot];
		}
		addPrim(&g_fb[g_active].ot[otz], poly);
		poly++;
		drawn++;
	}
	g_pri = (uint8_t *)poly;
	return drawn;
}

static void compose_node_mtx(int idx, MATRIX *out) {
	const ScriptVMNode *nodes = script_vm_nodes();
	const int n = script_vm_node_count();
	if (idx < 0 || idx >= n || !nodes) {
		for (int r = 0; r < 3; r++) {
			for (int c = 0; c < 3; c++) {
				out->m[r][c] = (r == c) ? ONE : 0;
			}
			out->t[r] = 0;
		}
		return;
	}
	SVECTOR r = { nodes[idx].rx, nodes[idx].ry, nodes[idx].rz, 0 };
	VECTOR t = { nodes[idx].px, nodes[idx].py, nodes[idx].pz };
	MATRIX local;
	RotMatrix(&r, &local);
	TransMatrix(&local, &t);
	if (nodes[idx].parent >= 0 && nodes[idx].parent < n && nodes[idx].parent != idx) {
		MATRIX parent;
		compose_node_mtx(nodes[idx].parent, &parent);
		CompMatrixLV(&parent, &local, out);
	} else {
		*out = local;
	}
}

static int draw_cooked_mesh(const MATRIX *view, const uint16_t *tpages, const uint16_t *cluts) {
	const uint8_t *mesh = script_vm_tris();
	int mesh_n = script_vm_tri_count();
	size_t mesh_sz = mesh ? (4 + size_t(mesh_n) * sizeof(CookedTri)) : 0;
	if (!mesh || mesh_n <= 0) {
		if (cooked_mesh_size < 4) {
			return 0;
		}
		mesh = cooked_mesh;
		mesh_sz = cooked_mesh_size;
		mesh_n = int(cooked_mesh[0] | (cooked_mesh[1] << 8));
	}
	if (mesh_sz < 4) {
		return 0;
	}
	const uint16_t tri_n = uint16_t(mesh_n);
	const CookedTri *tris = (const CookedTri *)(mesh + 4);
	const size_t need = 4 + size_t(tri_n) * sizeof(CookedTri);
	if (need > mesh_sz) {
		return 0;
	}
	const int nnode = script_vm_node_count();
	const ScriptVMNode *nodes = script_vm_nodes();
	if (nnode > 0 && nodes && view) {
		int drawn = 0;
		for (int i = 0; i < nnode; i++) {
			if (!(nodes[i].flags & 1)) {
				continue;
			}
			MATRIX world, mv;
			compose_node_mtx(i, &world);
			CompMatrixLV(const_cast<MATRIX *>(view), &world, &mv);
			gte_SetRotMatrix(&mv);
			gte_SetTransMatrix(&mv);
			{
				MATRIX lmtx;
				MulMatrix0(&g_light_mtx, &mv, &lmtx);
				gte_SetLightMatrix(&lmtx);
			}
			drawn += draw_tri_range(tris, tri_n, nodes[i].tri_lo, nodes[i].tri_hi, tpages, cluts);
		}
		return drawn;
	}
	return draw_tri_range(tris, tri_n, 0, tri_n, tpages, cluts);
}
#endif

int main(int argc, const char **argv) {
	(void)argc;
	(void)argv;

	ResetGraph(0);
	DecDCTReset(0);
	InitGeom();
	int16_t geom_screen = 160;
	int16_t rot_step = 12;
	SVECTOR rot = { 0, 0, 0, 0 };
	VECTOR pos = { 0, 0, 512 };
#ifdef BLAZIUM_PS1_HAS_SCRIPT
	if (cooked_script_size >= 14) {
		union {
			float f;
			uint32_t u;
		} ry;
		ry.u = uint32_t(cooked_script[0]) | (uint32_t(cooked_script[1]) << 8) | (uint32_t(cooked_script[2]) << 16) | (uint32_t(cooked_script[3]) << 24);
		pos.vx = int16_t(cooked_script[4] | (cooked_script[5] << 8));
		pos.vy = int16_t(cooked_script[6] | (cooked_script[7] << 8));
		pos.vz = int16_t(cooked_script[8] | (cooked_script[9] << 8));
		geom_screen = int16_t(cooked_script[10] | (cooked_script[11] << 8));
		if (geom_screen < 80) {
			geom_screen = 80;
		}
		if (geom_screen > 256) {
			geom_screen = 256;
		}
		int step = int(ry.f * 4096.0f / (2.0f * 3.14159265f) / 60.0f);
		if (step == 0 && ry.f != 0.0f) {
			step = ry.f > 0.0f ? 1 : -1;
		}
		rot_step = int16_t(step);
		if (cooked_script_size >= 46) {
			const uint16_t light_n = uint16_t(cooked_script[14] | (cooked_script[15] << 8));
			if (light_n > 0) {
				int16_t dx[3] = { 0, 0, 0 };
				int16_t dy[3] = { 0, 0, 0 };
				int16_t dz[3] = { 0, 0, 0 };
				int cr[3] = { 0, 0, 0 };
				int cg[3] = { 0, 0, 0 };
				int cb[3] = { 0, 0, 0 };
				const int n = light_n > 3 ? 3 : int(light_n);
				for (int i = 0; i < n; i++) {
					const uint8_t *L = cooked_script + 16 + i * 10;
					dx[i] = int16_t(L[0] | (L[1] << 8));
					dy[i] = int16_t(L[2] | (L[3] << 8));
					dz[i] = int16_t(L[4] | (L[5] << 8));
					cr[i] = int(L[6]);
					cg[i] = int(L[7]);
					cb[i] = int(L[8]);
				}
				g_light_mtx.m[0][0] = dx[0];
				g_light_mtx.m[0][1] = dx[1];
				g_light_mtx.m[0][2] = dx[2];
				g_light_mtx.m[1][0] = dy[0];
				g_light_mtx.m[1][1] = dy[1];
				g_light_mtx.m[1][2] = dy[2];
				g_light_mtx.m[2][0] = dz[0];
				g_light_mtx.m[2][1] = dz[1];
				g_light_mtx.m[2][2] = dz[2];
				g_color_mtx.m[0][0] = (ONE * cr[0]) / 255;
				g_color_mtx.m[0][1] = (ONE * cr[1]) / 255;
				g_color_mtx.m[0][2] = (ONE * cr[2]) / 255;
				g_color_mtx.m[1][0] = (ONE * cg[0]) / 255;
				g_color_mtx.m[1][1] = (ONE * cg[1]) / 255;
				g_color_mtx.m[1][2] = (ONE * cg[2]) / 255;
				g_color_mtx.m[2][0] = (ONE * cb[0]) / 255;
				g_color_mtx.m[2][1] = (ONE * cb[1]) / 255;
				g_color_mtx.m[2][2] = (ONE * cb[2]) / 255;
			}
		}
		if (cooked_script_size >= 60) {
			const uint16_t dw = uint16_t(cooked_script[46] | (cooked_script[47] << 8));
			const uint16_t dh = uint16_t(cooked_script[48] | (cooked_script[49] << 8));
			if (dw == 256 || dw == 320 || dw == 512 || dw == 640) {
				g_screen_w = int(dw);
			}
			if (dh == 240 || dh == 256) {
				g_screen_h = int(dh);
			}
			g_region = cooked_script[50] ? 1 : 0;
			rot.vx = int16_t(cooked_script[56] | (cooked_script[57] << 8));
			rot.vy = int16_t(cooked_script[58] | (cooked_script[59] << 8));
			if (cooked_script_size >= 62) {
				rot.vz = int16_t(cooked_script[60] | (cooked_script[61] << 8));
			}
		}
	}
#endif
	if (g_region) {
		SetVideoMode(MODE_PAL);
	} else {
		SetVideoMode(MODE_NTSC);
	}
	gte_SetGeomOffset(SCREEN_W / 2, SCREEN_H / 2);
	gte_SetGeomScreen(geom_screen);
	gte_SetBackColor(32, 32, 32);
	gte_SetFarColor(32, 0, 48);
	gte_SetColorMatrix(&g_color_mtx);

	SetDefDispEnv(&g_fb[0].disp, 0, 0, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&g_fb[0].draw, 0, SCREEN_H, SCREEN_W, SCREEN_H);
	SetDefDispEnv(&g_fb[1].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&g_fb[1].draw, 0, 0, SCREEN_W, SCREEN_H);
	for (int i = 0; i < 2; i++) {
		setRGB0(&g_fb[i].draw, 32, 0, 48);
		g_fb[i].draw.isbg = 1;
		ClearOTagR(g_fb[i].ot, OT_LEN);
	}
	g_active = 0;
	g_pri = g_fb[0].packet;
	SetDispMask(1);

	FntLoad(960, 0);
#ifdef BLAZIUM_PS1_HAS_TIM
	upload_cooked_tim();
#endif
	try_pcdrv_tim();
#ifdef BLAZIUM_PS1_HAS_VAG
	play_cooked_vag();
#endif
	InitPAD(g_pad[0], 34, g_pad[1], 34);
	StartPAD();
	ChangeClearPAD(0);
	try_analog_enter();
#ifdef BLAZIUM_PS1_HAS_STR
	{
		const uint8_t *xa = nullptr;
		size_t xa_n = 0;
#ifdef BLAZIUM_PS1_HAS_XA
		xa = cooked_xa;
		xa_n = cooked_xa_size;
#endif
		fmv_play_embedded(cooked_str, cooked_str_size, SCREEN_W, SCREEN_H, g_pad[0], xa, xa_n);
	}
#endif
	{
		const uint8_t *gdbc = nullptr;
		size_t gdbc_n = 0;
		const uint8_t *luau = nullptr;
		size_t luau_n = 0;
#ifdef BLAZIUM_PS1_HAS_GDBC
		gdbc = cooked_gdbc;
		gdbc_n = cooked_gdbc_size;
#endif
#ifdef BLAZIUM_PS1_HAS_LUAU
		luau = cooked_luau;
		luau_n = cooked_luau_size;
#endif
		script_vm_init(gdbc, gdbc_n, luau, luau_n);
#ifdef BLAZIUM_PS1_HAS_SCRIPT
		if (cooked_script_size >= 67 && cooked_script[62] == 'I' && cooked_script[63] == 'N' && cooked_script[64] == 'P' && cooked_script[65] == '1') {
			const int nc = int(cooked_script[66]);
			ScriptVMAction acts[PS1_MAX_ACTIONS];
			const uint8_t *p = cooked_script + 67;
			const uint8_t *end = cooked_script + cooked_script_size;
			int got = 0;
			for (int i = 0; i < nc && i < PS1_MAX_ACTIONS && p < end; i++) {
				const uint8_t nl = *p++;
				ScriptVMAction a{};
				uint8_t cpy = nl < 15 ? nl : 15;
				for (uint8_t k = 0; k < cpy && p < end; k++) {
					a.name[k] = char(*p++);
				}
				if (nl > cpy) {
					p += (nl - cpy);
				}
				a.name[cpy] = 0;
				if (p + 2 > end) {
					break;
				}
				a.mask = uint16_t(p[0] | (p[1] << 8));
				p += 2;
				if (p < end) {
					a.axis = *p++;
				}
				acts[got++] = a;
			}
			script_vm_set_actions(acts, got);
		}
#endif
	}
#ifdef BLAZIUM_PS1_HAS_NODE
	if (cooked_node_size >= 8 && cooked_node[0] == 'N' && cooked_node[1] == 'O' && cooked_node[2] == 'D' && cooked_node[3] == 'E') {
		const uint16_t ver = uint16_t(cooked_node[4] | (cooked_node[5] << 8));
		const uint16_t nc = uint16_t(cooked_node[6] | (cooked_node[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMNode parsed[PS1_MAX_NODES];
			uint8_t bits[PS1_MAX_NODES];
			char names[8][16];
			for (int g = 0; g < 8; g++) {
				names[g][0] = 0;
			}
			const uint8_t *p = cooked_node + 8;
			const uint8_t *end = cooked_node + cooked_node_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_NODES && p + 4 <= end; i++) {
				ScriptVMNode n{};
				n.parent = int16_t(p[0] | (p[1] << 8));
				p += 2;
				const uint8_t nl = *p++;
				uint8_t cpy = nl < 31 ? nl : 31;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					n.name[k] = char(p[k]);
				}
				n.name[cpy] = 0;
				p += nl;
				if (p + 21 > end) {
					break;
				}
				n.type = *p++;
				n.flags = *p++;
				n.px = int16_t(p[0] | (p[1] << 8));
				n.py = int16_t(p[2] | (p[3] << 8));
				n.pz = int16_t(p[4] | (p[5] << 8));
				n.rx = int16_t(p[6] | (p[7] << 8));
				n.ry = int16_t(p[8] | (p[9] << 8));
				n.rz = int16_t(p[10] | (p[11] << 8));
				n.tri_lo = uint16_t(p[12] | (p[13] << 8));
				n.tri_hi = uint16_t(p[14] | (p[15] << 8));
				n.sprite = int16_t(p[16] | (p[17] << 8));
				n.script = int16_t(p[18] | (p[19] << 8));
				p += 20;
				bits[got] = *p++;
				parsed[got++] = n;
			}
			if (p + 128 <= end) {
				for (int g = 0; g < 8; g++) {
					int k = 0;
					while (k < 15 && p[g * 16 + k]) {
						names[g][k] = char(p[g * 16 + k]);
						k++;
					}
					names[g][k] = 0;
				}
				p += 128;
			}
			uint8_t scroll[PS1_MAX_NODES];
			for (int i = 0; i < PS1_MAX_NODES; i++) {
				scroll[i] = 0;
			}
			if (p + 128 <= end) {
				for (int i = 0; i < 128 && i < PS1_MAX_NODES; i++) {
					scroll[i] = p[i];
				}
				p += 128;
			}
			script_vm_set_nodes(parsed, got);
			script_vm_set_group_bits(bits, got);
			script_vm_set_group_names(names);
			script_vm_set_scroll(scroll, got);
			if (p + 10 <= end) {
				script_vm_set_fog(p[0], int(p[2] | (p[3] << 8)), int(p[4] | (p[5] << 8)), p[6], p[7], p[8]);
			}
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_HUD
	if (cooked_hud_size >= 8 && cooked_hud[0] == 'H' && cooked_hud[1] == 'U' && cooked_hud[2] == 'D' && cooked_hud[3] == '0') {
		const uint16_t ver = uint16_t(cooked_hud[4] | (cooked_hud[5] << 8));
		const uint16_t nc = uint16_t(cooked_hud[6] | (cooked_hud[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMHud parsed[PS1_MAX_HUD];
			const uint8_t *p = cooked_hud + 8;
			const uint8_t *end = cooked_hud + cooked_hud_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_HUD && p + 17 <= end; i++) {
				ScriptVMHud h{};
				h.x = int16_t(p[0] | (p[1] << 8));
				h.y = int16_t(p[2] | (p[3] << 8));
				h.w = int16_t(p[4] | (p[5] << 8));
				h.h = int16_t(p[6] | (p[7] << 8));
				p += 8;
				h.kind = *p++;
				h.tex = *p++;
				h.rgb = *p++;
				h.node_id = *p++;
				h.flags = *p++;
				h.value = int16_t(p[0] | (p[1] << 8));
				h.vmin = int16_t(p[2] | (p[3] << 8));
				h.vmax = int16_t(p[4] | (p[5] << 8));
				p += 6;
				if (p >= end) {
					break;
				}
				const uint8_t tl = *p++;
				uint8_t cpy = tl < 63 ? tl : 63;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					h.text[k] = char(p[k]);
				}
				h.text[cpy] = 0;
				p += tl;
				if (p >= end) {
					break;
				}
				h.nitems = *p++;
				if (h.nitems > 8) {
					h.nitems = 8;
				}
				for (uint8_t it = 0; it < h.nitems && p < end; it++) {
					const uint8_t il = *p++;
					uint8_t ic = il < 15 ? il : 15;
					for (uint8_t k = 0; k < ic && p + k < end; k++) {
						h.items[it][k] = char(p[k]);
					}
					h.items[it][ic] = 0;
					p += il;
				}
				parsed[got++] = h;
			}
			script_vm_set_hud(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_TILE
	if (cooked_tile_size >= 10 && cooked_tile[0] == 'T' && cooked_tile[1] == 'I' && cooked_tile[2] == 'L' && cooked_tile[3] == 'E') {
		const uint16_t ver = uint16_t(cooked_tile[4] | (cooked_tile[5] << 8));
		const uint16_t nc = uint16_t(cooked_tile[6] | (cooked_tile[7] << 8));
		g_tile_w = cooked_tile[8];
		g_tile_h = cooked_tile[9];
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMTile parsed[PS1_MAX_TILES];
			const uint8_t *p = cooked_tile + 10;
			const uint8_t *end = cooked_tile + cooked_tile_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_TILES && p + 9 <= end; i++) {
				ScriptVMTile t{};
				t.x = int16_t(p[0] | (p[1] << 8));
				t.y = int16_t(p[2] | (p[3] << 8));
				t.u = p[4];
				t.v = p[5];
				t.tex = p[6];
				t.node_id = p[7];
				t.flags = p[8];
				p += 9;
				parsed[got++] = t;
			}
			script_vm_set_tiles(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_SCENE
	if (cooked_scene_size >= 8 && cooked_scene[0] == 'S' && cooked_scene[1] == 'C' && cooked_scene[2] == 'E' && cooked_scene[3] == 'N') {
		const uint16_t ver = uint16_t(cooked_scene[4] | (cooked_scene[5] << 8));
		const uint16_t nc = uint16_t(cooked_scene[6] | (cooked_scene[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMPack parsed[PS1_MAX_PACKS];
			const uint8_t *p = cooked_scene + 8;
			const uint8_t *end = cooked_scene + cooked_scene_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_PACKS && p < end; i++) {
				ScriptVMPack pack{};
				const uint8_t sl = *p++;
				uint8_t cpy = sl < 63 ? sl : 63;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					pack.path[k] = char(p[k]);
				}
				pack.path[cpy] = 0;
				p += sl;
				if (p + 16 + 1 + 12 + 30 > end) {
					break;
				}
				pack.node_lo = uint16_t(p[0] | (p[1] << 8));
				pack.node_hi = uint16_t(p[2] | (p[3] << 8));
				pack.tri_lo = uint16_t(p[4] | (p[5] << 8));
				pack.tri_hi = uint16_t(p[6] | (p[7] << 8));
				pack.hud_lo = uint16_t(p[8] | (p[9] << 8));
				pack.hud_hi = uint16_t(p[10] | (p[11] << 8));
				pack.tile_lo = uint16_t(p[12] | (p[13] << 8));
				pack.tile_hi = uint16_t(p[14] | (p[15] << 8));
				p += 16;
				pack.has_cam = *p++;
				pack.cam_px = int16_t(p[0] | (p[1] << 8));
				pack.cam_py = int16_t(p[2] | (p[3] << 8));
				pack.cam_pz = int16_t(p[4] | (p[5] << 8));
				pack.cam_rx = int16_t(p[6] | (p[7] << 8));
				pack.cam_ry = int16_t(p[8] | (p[9] << 8));
				pack.cam_rz = int16_t(p[10] | (p[11] << 8));
				p += 12;
				if (p + 30 > end) {
					break;
				}
				pack.node_count = uint16_t(p[0] | (p[1] << 8));
				pack.tri_count = uint16_t(p[2] | (p[3] << 8));
				pack.hud_count = uint16_t(p[4] | (p[5] << 8));
				pack.tile_count = uint16_t(p[6] | (p[7] << 8));
				pack.tim_count = uint16_t(p[8] | (p[9] << 8));
				p += 10;
				pack.ram_bytes = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
				p += 20;
				parsed[got++] = pack;
			}
			script_vm_set_packs(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_MESH
	script_vm_set_mesh(cooked_mesh, cooked_mesh_size);
#endif
#ifdef BLAZIUM_PS1_HAS_ANIM
	if (cooked_anim_size >= 8 && cooked_anim[0] == 'A' && cooked_anim[1] == 'N' && cooked_anim[2] == 'I' && cooked_anim[3] == 'M') {
		const uint16_t ver = uint16_t(cooked_anim[4] | (cooked_anim[5] << 8));
		const uint16_t nc = uint16_t(cooked_anim[6] | (cooked_anim[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMAnimClip parsed[PS1_MAX_CLIPS];
			const uint8_t *p = cooked_anim + 8;
			const uint8_t *end = cooked_anim + cooked_anim_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_CLIPS && p < end; i++) {
				ScriptVMAnimClip clip{};
				const uint8_t sl = *p++;
				uint8_t cpy = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					clip.name[k] = char(p[k]);
				}
				clip.name[cpy] = 0;
				p += sl;
				if (p >= end) {
					break;
				}
				clip.nkeys = *p++;
				if (clip.nkeys > PS1_MAX_KEYS) {
					clip.nkeys = PS1_MAX_KEYS;
				}
				for (uint8_t k = 0; k < clip.nkeys && p + 16 <= end; k++) {
					ScriptVMAnimKey key{};
					key.t_ms = uint16_t(p[0] | (p[1] << 8));
					key.node_id = p[2];
					key.flags = p[3];
					key.px = int16_t(p[4] | (p[5] << 8));
					key.py = int16_t(p[6] | (p[7] << 8));
					key.pz = int16_t(p[8] | (p[9] << 8));
					key.rx = int16_t(p[10] | (p[11] << 8));
					key.ry = int16_t(p[12] | (p[13] << 8));
					key.rz = int16_t(p[14] | (p[15] << 8));
					p += 16;
					clip.keys[k] = key;
					if (key.flags & 2) {
						clip.loop = 1;
					}
				}
				parsed[got++] = clip;
			}
			script_vm_set_anims(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_CAM
	if (cooked_cam_size >= 8 && cooked_cam[0] == 'C' && cooked_cam[1] == 'A' && cooked_cam[2] == 'M' && cooked_cam[3] == '0') {
		const uint16_t ver = uint16_t(cooked_cam[4] | (cooked_cam[5] << 8));
		const uint16_t nc = uint16_t(cooked_cam[6] | (cooked_cam[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMCam parsed[PS1_MAX_CAMS];
			const uint8_t *p = cooked_cam + 8;
			const uint8_t *end = cooked_cam + cooked_cam_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_CAMS && p < end; i++) {
				ScriptVMCam c{};
				c.node_id = int16_t(p[0] | (p[1] << 8));
				p += 2;
				const uint8_t sl = *p++;
				uint8_t cpy = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < cpy && p + k < end; k++) {
					c.name[k] = char(p[k]);
				}
				c.name[cpy] = 0;
				p += sl;
				if (p + 24 > end) {
					break;
				}
				c.is_default = *p++;
				c.px = int16_t(p[0] | (p[1] << 8));
				c.py = int16_t(p[2] | (p[3] << 8));
				c.pz = int16_t(p[4] | (p[5] << 8));
				c.rx = int16_t(p[6] | (p[7] << 8));
				c.ry = int16_t(p[8] | (p[9] << 8));
				c.rz = int16_t(p[10] | (p[11] << 8));
				p += 12;
				c.dim = *p++;
				c.drag = *p++;
				c.dead = *p++;
				c.lim_l = int16_t(p[0] | (p[1] << 8));
				c.lim_t = int16_t(p[2] | (p[3] << 8));
				c.lim_r = int16_t(p[4] | (p[5] << 8));
				c.lim_b = int16_t(p[6] | (p[7] << 8));
				p += 8;
				c.pack = 0;
				parsed[got++] = c;
			}
			script_vm_set_cams(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_HIT
	if (cooked_hit_size >= 8 && cooked_hit[0] == 'H' && cooked_hit[1] == 'I' && cooked_hit[2] == 'T' && cooked_hit[3] == '0') {
		const uint16_t ver = uint16_t(cooked_hit[4] | (cooked_hit[5] << 8));
		const uint16_t nc = uint16_t(cooked_hit[6] | (cooked_hit[7] << 8));
		if (ver == BLAZIUM_PS1_COOK_ABI) {
			ScriptVMHit parsed[PS1_MAX_HITS];
			const uint8_t *p = cooked_hit + 8;
			const uint8_t *end = cooked_hit + cooked_hit_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_HITS && p + 18 <= end; i++) {
				ScriptVMHit h{};
				h.node_id = int16_t(p[0] | (p[1] << 8));
				h.dim = p[2];
				h.kind = p[3];
				h.flags = p[4];
				h.layer = p[5] ? p[5] : 1;
				h.min_x = int16_t(p[6] | (p[7] << 8));
				h.min_y = int16_t(p[8] | (p[9] << 8));
				h.min_z = int16_t(p[10] | (p[11] << 8));
				h.max_x = int16_t(p[12] | (p[13] << 8));
				h.max_y = int16_t(p[14] | (p[15] << 8));
				h.max_z = int16_t(p[16] | (p[17] << 8));
				h.pack = 0;
				p += 18;
				parsed[got++] = h;
			}
			script_vm_set_hits(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_SPRITE
	if (cooked_sprite_size >= 4) {
		const uint16_t n = uint16_t(cooked_sprite[0] | (cooked_sprite[1] << 8));
		const uint8_t *p = cooked_sprite + 4;
		ScriptVMSprite parsed[PS1_MAX_SPRITES];
		int got = 0;
		for (uint16_t i = 0; i < n && got < PS1_MAX_SPRITES && p + 16 <= cooked_sprite + cooked_sprite_size; i++) {
			ScriptVMSprite s{};
			s.x = int16_t(p[0] | (p[1] << 8));
			s.y = int16_t(p[2] | (p[3] << 8));
			s.w = uint16_t(p[4] | (p[5] << 8));
			s.h = uint16_t(p[6] | (p[7] << 8));
			s.u = p[8];
			s.v = p[9];
			s.tex = uint16_t(p[10] | (p[11] << 8));
			s.frame = p[12];
			s.nframes = p[13] ? p[13] : 1;
			s.fps = p[14];
			s.billboard = p[15];
			s.rgb = 255;
			p += 16;
			parsed[got++] = s;
		}
		script_vm_set_sprites(parsed, got);
	}
#endif
	const int font = FntOpen(8, 16, SCREEN_W - 16, SCREEN_H - 40, 0, 256);
	uint16_t tpages[TIM_SLOTS];
	uint16_t cluts[TIM_SLOTS];
	for (int i = 0; i < TIM_SLOTS; i++) {
#ifdef BLAZIUM_PS1_HAS_TIM
		tpages[i] = getTPage(0, 0, g_tp_x[i], g_tp_y[i]);
		cluts[i] = getClut(g_cl_x[i], g_cl_y[i]);
#else
		tpages[i] = getTPage(0, 0, 640 + 64 * (i % 4), 64 * (i / 4));
		cluts[i] = getClut(0, 480 - i);
#endif
	}
	int frames = 0;

	for (;;) {
		MATRIX mtx;
		RotMatrix(&rot, &mtx);
		TransMatrix(&mtx, &pos);
		gte_SetRotMatrix(&mtx);
		gte_SetTransMatrix(&mtx);
		{
			MATRIX lmtx;
			MulMatrix0(&g_light_mtx, &mtx, &lmtx);
			gte_SetLightMatrix(&lmtx);
		}
		int block_cam = 0;
		static int g_script_cam_flag = 0;
		static int g_cam_scale = 0;
		{
			ScriptVMHost host{};
			memset(&host, 0, sizeof(host));
			host.rot_x = &rot.vx;
			host.rot_y = &rot.vy;
			host.rot_z = &rot.vz;
			host.pos_x = &pos.vx;
			host.pos_y = &pos.vy;
			host.pos_z = &pos.vz;
			host.pad34 = g_pad[0];
			host.pad34_1 = g_pad[1];
			host.hud_focus_blocks_cam = 0;
			host.script_drives_cam = &g_script_cam_flag;
			host.cam_scale = &g_cam_scale;
#ifdef BLAZIUM_PS1_HAS_VAG
			host.play_vag = play_cooked_vag;
			host.stop_vag = stop_cooked_vag;
			host.vag_playing = cooked_vag_playing;
#else
			host.play_vag = nullptr;
			host.stop_vag = nullptr;
			host.vag_playing = nullptr;
#endif
#ifdef BLAZIUM_PS1_HAS_STR
			host.play_fmv = replay_cooked_fmv;
#else
			host.play_fmv = nullptr;
#endif
			host.region = g_region;
			host.set_rumble = host_set_rumble;
			host.upload_tpak = host_upload_tpak;
			host.evict_tpak = host_evict_tpak;
			host.load_sfx_bank = host_load_sfx;
			host.unload_sfx_bank = host_unload_sfx;
			host.play_sfx = host_play_sfx;
			host.stop_sfx = host_stop_sfx;
			host.set_sfx_volume = host_set_sfx_vol;
			host.set_music_volume = host_set_music_vol;
			host.set_light = host_set_light;
			host.load_music = host_load_music;
			host.unload_music = host_unload_music;
			host.play_fmv_blob = host_play_fmv_blob;
			host.play_xa = host_play_xa;
			host.stop_xa = host_stop_xa;
			host.play_fmv_cd = host_play_fmv_cd;
			pad_poll_motors();
			if (!script_vm_process(g_region ? 1.0f / 50.0f : 1.0f / 60.0f, &host)) {
				rot.vy += rot_step;
			}
			block_cam = host.hud_focus_blocks_cam;
			if (g_cam_scale > 0) {
				gte_SetGeomScreen(g_cam_scale);
			}
		}
		{
			const PADTYPE *pad = (const PADTYPE *)g_pad[0];
			if (pad->stat == 0 && !block_cam && !g_script_cam_flag && !script_vm_script_cam()) {
				if (!(pad->btn & PAD_LEFT)) {
					rot.vy -= 24;
				}
				if (!(pad->btn & PAD_RIGHT)) {
					rot.vy += 24;
				}
				if (!(pad->btn & PAD_UP)) {
					pos.vz -= 8;
				}
				if (!(pad->btn & PAD_DOWN)) {
					pos.vz += 8;
				}
			}
		}

#ifdef BLAZIUM_PS1_HAS_MESH
		draw_cooked_mesh(&mtx, tpages, cluts);
#endif
#ifdef BLAZIUM_PS1_HAS_SPRITE
		draw_cooked_sprites(tpages, cluts);
#endif
		{
			uint16_t dummy_tp[TIM_SLOTS] = {};
			draw_cooked_tiles(dummy_tp, cluts);
			draw_cooked_hud(cluts);
		}
		{
			int fade_a = 0;
			uint8_t fr = 0, fg = 0, fb = 0;
			script_vm_get_fade(&fade_a, &fr, &fg, &fb);
			if (fade_a > 0 && (uint8_t *)((POLY_F4 *)g_pri + 1) <= g_fb[g_active].packet + PACKET_LEN) {
				POLY_F4 *f = (POLY_F4 *)g_pri;
				setPolyF4(f);
				setSemiTrans(f, 1);
				setRGB0(f, fr, fg, fb);
				setXY4(f, 0, 0, SCREEN_W, 0, 0, SCREEN_H, SCREEN_W, SCREEN_H);
				addPrim(&g_fb[g_active].ot[0], f);
				g_pri = (uint8_t *)(f + 1);
			}
			const int np = script_vm_particle_count();
			const ScriptVMParticle *parts = script_vm_particles();
			for (int i = 0; i < np; i++) {
				if (!parts[i].life) {
					continue;
				}
				const int dx = int(parts[i].x) - int(pos.vx);
				const int dy = int(parts[i].y) - int(pos.vy);
				const int dz = int(parts[i].z) - int(pos.vz);
				const int cy = icos(rot.vy);
				const int ysin = isin(rot.vy);
				const int lx = (dx * cy - dz * ysin) >> 12;
				const int lz = (dx * ysin + dz * cy) >> 12;
				const int cp = icos(rot.vx);
				const int sp = isin(rot.vx);
				const int ly = (dy * cp - lz * sp) >> 12;
				const int lz2 = (dy * sp + lz * cp) >> 12;
				if (lz2 <= 1) {
					continue;
				}
				const int fov = g_cam_scale > 0 ? g_cam_scale : 160;
				const int16_t sx = int16_t(SCREEN_W / 2 + lx * fov / lz2);
				const int16_t sy = int16_t(SCREEN_H / 2 - ly * fov / lz2);
				const uint8_t pr = parts[i].r ? parts[i].r : 255;
				const uint8_t pg = parts[i].g ? parts[i].g : 220;
				const uint8_t pb = parts[i].b ? parts[i].b : 80;
				const int sz = parts[i].size ? int(parts[i].size) : 4;
				const uint8_t mode = parts[i].mode;
				if (mode == 2) {
					if ((uint8_t *)((POLY_F4 *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
						break;
					}
					const int pdx = int(parts[i].px) - int(pos.vx);
					const int pdy = int(parts[i].py) - int(pos.vy);
					const int pdz = int(parts[i].pz) - int(pos.vz);
					const int plx = (pdx * cy - pdz * ysin) >> 12;
					const int plz = (pdx * ysin + pdz * cy) >> 12;
					const int ply = (pdy * cp - plz * sp) >> 12;
					const int plz2 = (pdy * sp + plz * cp) >> 12;
					if (plz2 <= 1) {
						continue;
					}
					const int16_t ox = int16_t(SCREEN_W / 2 + plx * fov / plz2);
					const int16_t oy = int16_t(SCREEN_H / 2 - ply * fov / plz2);
					POLY_F4 *q = (POLY_F4 *)g_pri;
					setPolyF4(q);
					setRGB0(q, pr, pg, pb);
					setXY4(q, ox, int16_t(oy - sz), sx, int16_t(sy - sz), ox, int16_t(oy + sz), sx, int16_t(sy + sz));
					addPrim(&g_fb[g_active].ot[1], q);
					g_pri = (uint8_t *)(q + 1);
					continue;
				}
				if (mode == 3 || mode == 4) {
					const int tw = sz > 1 ? 2 : 1;
					draw_tile_rect(sx, sy, int16_t(tw), int16_t(tw), pr, pg, pb);
					continue;
				}
				if ((uint8_t *)((POLY_FT4 *)g_pri + 1) > g_fb[g_active].packet + PACKET_LEN) {
					break;
				}
				POLY_FT4 *q = (POLY_FT4 *)g_pri;
				setPolyFT4(q);
				setRGB0(q, pr, pg, pb);
				setXY4(q, int16_t(sx - sz), int16_t(sy - sz), int16_t(sx + sz), int16_t(sy - sz), int16_t(sx - sz), int16_t(sy + sz), int16_t(sx + sz), int16_t(sy + sz));
				uint8_t u0 = 0, u1 = 8;
				if (mode == 1 && parts[i].nframes > 1) {
					const uint8_t fw = uint8_t(256 / parts[i].nframes);
					u0 = uint8_t(parts[i].frame * fw);
					u1 = uint8_t(u0 + fw);
				}
				setUV4(q, u0, 0, u1, 0, u0, 8, u1, 8);
				q->tpage = tpages[parts[i].tex & 15];
				q->clut = cluts[parts[i].tex & 15];
				addPrim(&g_fb[g_active].ot[1], q);
				g_pri = (uint8_t *)(q + 1);
			}
		}
		{
			const char *err = script_vm_last_error();
			g_pri = (uint8_t *)FntSort(&g_fb[g_active].ot[1], g_pri, 8, 200, (err && err[0]) ? err : "HUD SPRT AABB L3 FOG MDEC");
		}
		(void)font;
		(void)frames;
		flip_frame();
		frames++;
	}
}
