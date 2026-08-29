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
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include <psxpress.h>
#include <psxsn.h>
#include <psxspu.h>

#include "fmv_play.h"
#include "script_vm.h"

#ifndef BLAZIUM_PS1_COOK_ABI
#define BLAZIUM_PS1_COOK_ABI 9
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
static int16_t g_spr_x = 140;
static int16_t g_spr_y = 100;
static int16_t g_spr_vx = 2;
static int16_t g_spr_vy = 1;
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
			p += sz;
			left -= sz;
		}
		return;
	}
	upload_one_tim(cooked_tim, cooked_tim_size);
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
	SpuSetKey(0, 1);
	SpuSetVoiceVolume(0, 0x3fff, 0x3fff);
	SpuSetVoicePitch(0, getSPUSampleRate(int(rate ? rate : 22050)));
	SpuSetVoiceStartAddr(0, addr);
	SPU_CH_ADSR1(0) = 0x00ff;
	SPU_CH_ADSR2(0) = 0x0000;
	SpuSetKey(1, 1);
	g_vag_on = 1;
}

static void stop_cooked_vag() {
	SpuSetKey(0, 1);
	g_vag_on = 0;
}

static int cooked_vag_playing() {
	return g_vag_on;
}
#endif

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
typedef struct {
	int16_t x, y;
	uint16_t w, h;
	uint8_t u, v;
	uint16_t pad;
} CookedSprite;

static uint16_t g_spr_flags = 1;

static void aabb_step() {
	g_spr_x += g_spr_vx;
	g_spr_y += g_spr_vy;
	if (g_spr_x < 0 || g_spr_x > SCREEN_W - 16) {
		g_spr_vx = int16_t(-g_spr_vx);
		g_spr_x += g_spr_vx;
	}
	if (g_spr_y < 24 || g_spr_y > SCREEN_H - 16) {
		g_spr_vy = int16_t(-g_spr_vy);
		g_spr_y += g_spr_vy;
	}
}

static void draw_cooked_sprites(uint16_t clut) {
	if (cooked_sprite_size < 4) {
		return;
	}
	const uint16_t n = uint16_t(cooked_sprite[0] | (cooked_sprite[1] << 8));
	g_spr_flags = uint16_t(cooked_sprite[2] | (cooked_sprite[3] << 8));
	const CookedSprite *spr = (const CookedSprite *)(cooked_sprite + 4);
	SPRT *p = (SPRT *)g_pri;
	for (uint16_t i = 0; i < n; i++) {
		if ((uint8_t *)(p + 1) > g_fb[g_active].packet + PACKET_LEN) {
			break;
		}
		setSprt(p);
		setRGB0(p, 255, 255, 255);
		int16_t x = spr[i].x;
		int16_t y = spr[i].y;
		if (i == 1 && (g_spr_flags & 1)) {
			x = g_spr_x;
			y = g_spr_y;
		}
		setXY0(p, x, y);
		setWH(p, spr[i].w, spr[i].h);
		setUV0(p, spr[i].u, spr[i].v);
		p->clut = clut;
		addPrim(&g_fb[g_active].ot[1], p);
		p++;
	}
	g_pri = (uint8_t *)p;
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
	for (int i = 0; i < n; i++) {
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
			const int fog = otz * 128 / OT_LEN;
			poly->r0 = uint8_t((int(poly->r0) * (128 - fog) + 32 * fog) >> 7);
			poly->g0 = uint8_t((int(poly->g0) * (128 - fog)) >> 7);
			poly->b0 = uint8_t((int(poly->b0) * (128 - fog) + 48 * fog) >> 7);
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
	if (cooked_mesh_size < 4) {
		return 0;
	}
	const uint16_t tri_n = uint16_t(cooked_mesh[0] | (cooked_mesh[1] << 8));
	const CookedTri *tris = (const CookedTri *)(cooked_mesh + 4);
	const size_t need = 4 + size_t(tri_n) * sizeof(CookedTri);
	if (need > cooked_mesh_size) {
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
	}
#ifdef BLAZIUM_PS1_HAS_NODE
	if (cooked_node_size >= 8 && cooked_node[0] == 'N' && cooked_node[1] == 'O' && cooked_node[2] == 'D' && cooked_node[3] == 'E') {
		const uint16_t ver = uint16_t(cooked_node[4] | (cooked_node[5] << 8));
		const uint16_t nc = uint16_t(cooked_node[6] | (cooked_node[7] << 8));
		if (ver == 9) {
			ScriptVMNode parsed[PS1_MAX_NODES];
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
				if (p + 20 > end) {
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
				parsed[got++] = n;
			}
			script_vm_set_nodes(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_HUD
	if (cooked_hud_size >= 8 && cooked_hud[0] == 'H' && cooked_hud[1] == 'U' && cooked_hud[2] == 'D' && cooked_hud[3] == '0') {
		const uint16_t ver = uint16_t(cooked_hud[4] | (cooked_hud[5] << 8));
		const uint16_t nc = uint16_t(cooked_hud[6] | (cooked_hud[7] << 8));
		if (ver == 9) {
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
				uint8_t cpy = tl < 31 ? tl : 31;
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
		if (ver == 9) {
			ScriptVMTile parsed[PS1_MAX_TILES];
			const uint8_t *p = cooked_tile + 10;
			const uint8_t *end = cooked_tile + cooked_tile_size;
			int got = 0;
			for (uint16_t i = 0; i < nc && i < PS1_MAX_TILES && p + 8 <= end; i++) {
				ScriptVMTile t{};
				t.x = int16_t(p[0] | (p[1] << 8));
				t.y = int16_t(p[2] | (p[3] << 8));
				t.u = p[4];
				t.v = p[5];
				t.tex = p[6];
				t.node_id = p[7];
				p += 8;
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
		if (ver == 9) {
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
				if (p + 17 + 12 > end) {
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
				parsed[got++] = pack;
			}
			script_vm_set_packs(parsed, got);
		}
	}
#endif
#ifdef BLAZIUM_PS1_HAS_ANIM
	if (cooked_anim_size >= 8 && cooked_anim[0] == 'A' && cooked_anim[1] == 'N' && cooked_anim[2] == 'I' && cooked_anim[3] == 'M') {
		const uint16_t ver = uint16_t(cooked_anim[4] | (cooked_anim[5] << 8));
		const uint16_t nc = uint16_t(cooked_anim[6] | (cooked_anim[7] << 8));
		if (ver == 9) {
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
				}
				parsed[got++] = clip;
			}
			script_vm_set_anims(parsed, got);
		}
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
	const uint16_t clut = cluts[0];

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
		{
			ScriptVMHost host;
			host.rot_x = &rot.vx;
			host.rot_y = &rot.vy;
			host.rot_z = &rot.vz;
			host.pos_x = &pos.vx;
			host.pos_y = &pos.vy;
			host.pos_z = &pos.vz;
			host.pad34 = g_pad[0];
			host.hud_focus_blocks_cam = 0;
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
			if (!script_vm_process(1.0f / 60.0f, &host)) {
				rot.vy += rot_step;
			}
			block_cam = host.hud_focus_blocks_cam;
		}
		{
			const PADTYPE *pad = (const PADTYPE *)g_pad[0];
			if (pad->stat == 0 && !block_cam) {
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
		if (g_spr_flags & 1) {
			aabb_step();
		}
		draw_cooked_sprites(clut);
#endif
		{
			uint16_t dummy_tp[TIM_SLOTS] = {};
			draw_cooked_tiles(dummy_tp, cluts);
			draw_cooked_hud(cluts);
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
