/**************************************************************************/
/*  gdir_ops.h                                                            */
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

// GDIR: the compiled GDScript subset for the PS2 guest (ABI 1, inside SCRP).
// Must match blazium-toolchain guest/ps2/runtime/gdir_ops.h (same values).
//
// The cooker compiles each script attached to a cooked node from the GDScript
// parser/analyzer tree into this stack bytecode. It is not the Godot VM: no
// Variant, no ClassDB, no await. Values are nil/bool/int/float/string/
// Vector2/Vector3/node/array; nodes are the pack's cooked node indices.

#define GDIR_VERSION 1

enum GdirConst {
	GDIR_K_NIL = 0,
	GDIR_K_BOOL = 1,
	GDIR_K_INT = 2,
	GDIR_K_FLOAT = 3,
	GDIR_K_STR = 4,
	GDIR_K_VEC2 = 5,
	GDIR_K_VEC3 = 6,
	GDIR_K_ARRAY = 7, // u16 n, u16 const index * n (read-only const array)
};

enum GdirOp {
	GOP_NOP = 0,
	GOP_CONST = 1, // u16 const
	GOP_NIL = 2,
	GOP_TRUE = 3,
	GOP_FALSE = 4,
	GOP_LLOAD = 5, // u8 local
	GOP_LSTORE = 6, // u8 local (pops)
	GOP_MLOAD = 7, // u8 member
	GOP_MSTORE = 8, // u8 member (pops)
	GOP_SELF = 9,
	GOP_NODEREF = 10, // u8 class node-ref slot
	GOP_GETP = 11, // u8 prop: [obj] -> [value]
	GOP_SETP = 12, // u8 prop: [value obj] -> [obj'] (vectors return the edited copy)
	GOP_ADD = 13,
	GOP_SUB = 14,
	GOP_MUL = 15,
	GOP_DIV = 16,
	GOP_MOD = 17, // also "fmt" % value for strings
	GOP_NEG = 18,
	GOP_EQ = 19,
	GOP_NE = 20,
	GOP_LT = 21,
	GOP_LE = 22,
	GOP_GT = 23,
	GOP_GE = 24,
	GOP_NOT = 25,
	GOP_JMP = 26, // u32 absolute code offset
	GOP_JZ = 27, // u32 (pops)
	GOP_JNZ = 28, // u32 (pops)
	GOP_JZK = 29, // u32: falsy -> jump keeping value, else pop ("and")
	GOP_JNZK = 30, // u32: truthy -> jump keeping value, else pop ("or")
	GOP_POP = 31,
	GOP_DUP = 32,
	GOP_CALL = 33, // u16 function, u8 argc
	GOP_NATIVE = 34, // u8 native, u8 argc (pushes a result, nil if none)
	GOP_RET = 35,
	GOP_RETNIL = 36,
	GOP_ARRAY = 37, // u8 count
	GOP_INDEX = 38, // [base index] -> [value]
	GOP_SETINDEX = 39, // [value base index] -> []
	GOP_BAND = 40,
	GOP_BOR = 41,
	GOP_IN = 42, // [value container] -> [bool]
};

enum GdirProp {
	GP_POSITION = 0,
	GP_ROTATION = 1,
	GP_ROTATION_DEGREES = 2,
	GP_SCALE = 3,
	GP_VISIBLE = 4,
	GP_TEXT = 5,
	GP_X = 6,
	GP_Y = 7,
	GP_Z = 8,
	GP_GLOBAL_POSITION = 9,
	GP_NAME = 10,
	GP_COLOR = 11, // ColorRect.color / Label font colour (rgb)
	GP_R = 12,
	GP_G = 13,
	GP_B = 14,
	GP_SIZE = 15, // read only
};

enum GdirNative {
	GN_PRINT = 0,
	GN_STR = 1,
	GN_INT = 2,
	GN_FLOAT = 3,
	GN_ABS = 4,
	GN_MIN = 5,
	GN_MAX = 6,
	GN_CLAMP = 7,
	GN_LERP = 8,
	GN_SIN = 9,
	GN_COS = 10,
	GN_ATAN2 = 11,
	GN_SQRT = 12,
	GN_FLOOR = 13,
	GN_CEIL = 14,
	GN_ROUND = 15,
	GN_RANDF = 16,
	GN_RANDI = 17,
	GN_RANDF_RANGE = 18,
	GN_RANDI_RANGE = 19,
	GN_SIGN = 20,
	GN_FMOD = 21,
	GN_WRAPI = 22,
	GN_WRAPF = 23,
	GN_DEG_TO_RAD = 24,
	GN_RAD_TO_DEG = 25,
	GN_LEN = 26,
	GN_VEC2 = 27,
	GN_VEC3 = 28,
	GN_COLOR = 29,
	GN_POW = 30,
	GN_POSMOD = 31,
	GN_TAN = 32,
	GN_BOOL = 33,
	GN_MOVE_TOWARD = 34,
	GN_ASIN = 35,
	GN_ACOS = 36,
	GN_EXP = 37,
	GN_IS_EQUAL_APPROX = 38,
	GN_STR_LENGTH = 39, // s.length()
	GN_STR_UPPER = 40,
	GN_STR_LOWER = 41,
	GN_STR_BEGINS = 42,
	GN_STR_ENDS = 43,
	GN_STR_SUBSTR = 44,
	GN_STR_PAD_ZEROS = 45,
	GN_ARR_SIZE = 46,
	GN_ARR_APPEND = 47,
	GN_ARR_POP_BACK = 48,
	GN_ARR_CLEAR = 49,
	GN_ARR_HAS = 50,
	GN_ARR_FIND = 51,
	GN_ARR_IS_EMPTY = 52,
	GN_ARR_FRONT = 53,
	GN_ARR_BACK = 54,
	GN_ARR_ERASE = 55,
	GN_ARR_INSERT = 56,
	GN_VEC_LENGTH = 57,
	GN_VEC_NORMALIZED = 58,
	GN_VEC_DOT = 59,
	GN_VEC_LERP = 60,
	GN_VEC_DISTANCE = 61,
	GN_INPUT_PRESSED = 64, // action name
	GN_INPUT_JUST_PRESSED = 65,
	GN_INPUT_JUST_RELEASED = 66,
	GN_INPUT_AXIS = 67, // neg, pos
	GN_INPUT_VECTOR = 68, // nx, px, ny, py
	GN_INPUT_STRENGTH = 69,
	GN_HAS_FEATURE = 72,
	GN_CHANGE_SCENE = 73, // path
	GN_CHANGE_SCENE_FADE = 74, // path, seconds
	GN_SET_FADE = 75, // alpha 0..1 [, Color]
	GN_QUIT = 76,
	GN_GET_DELTA_TIME = 77,
	GN_TICKS_MSEC = 78,
	GN_NODE_ROTATE_X = 80,
	GN_NODE_ROTATE_Y = 81,
	GN_NODE_ROTATE_Z = 82,
	GN_NODE_TRANSLATE = 83,
	GN_NODE_HIDE = 84,
	GN_NODE_SHOW = 85,
	GN_NODE_IS_VISIBLE = 86,
	GN_NODE_GRAB_FOCUS = 87,
	GN_NODE_HAS_FOCUS = 88,
	GN_NODE_LOOK_AT = 89,
	GN_NODE_CHILD_COUNT = 90,
	GN_NODE_GET_CHILD = 91,
	GN_NODE_SET_VISIBLE = 92,
	GN_PS2_PLAY_SFX = 96,
	GN_PS2_SET_RUMBLE = 97,
	GN_PS2_STOP_RUMBLE = 98,
	GN_PS2_SET_HUD_TEXT = 99, // node or slot, text
	GN_PS2_SHAKE_CAMERA = 100,
	GN_PS2_GET_DISPLAY_W = 101,
	GN_PS2_GET_DISPLAY_H = 102,
	GN_PS2_GET_ABI = 103,
	GN_PS2_EE_USED = 104,
	GN_PS2_EE_FREE = 105,
	GN_PS2_EE_LIMIT = 106,
	GN_PS2_GS_USED = 107,
	GN_PS2_GS_FREE = 108,
	GN_PS2_GS_LIMIT = 109,
	GN_PS2_LOAD_MUSIC = 110,
	GN_PS2_STOP_MUSIC = 111,
	GN_PS2_SET_MUSIC_VOLUME = 112,
	GN_PS2_SAY = 113,
	GN_PS2_POKE = 114, // off, byte: 24 KiB that survives change_scene
	GN_PS2_PEEK = 115, // off
	GN_PS2_SET_FOG = 116, // Color, near, far (no args, or far <= near: off)
	GN_PS2_SAVE_SLOT = 117, // slot: the poke bytes (up to the last non-zero one) to the memory card
	GN_PS2_LOAD_SLOT = 118, // slot: the memory card save back into the poke bytes; bytes, -1 none, -2 no card
	GN_PS2_MEMCARD_PRESENT = 119, // a formatted PS2 memory card is in slot 1
	GN_PS2_SET_VIEWS = 120, // count, camera nodes: split screen (2: halves, 3-4: quarters); 0 or 1: one view
	GN_PS2_PAD_COUNT = 121, // pads plugged in (a multitap's slots count)
	GN_PS2_PAD_PRESSED = 122, // pad, button name ("cross", "left", "start", ...)
	GN_PS2_PAD_JUST_PRESSED = 123, // pad, button name
	GN_PS2_PAD_AXIS = 124, // pad: steering -1..1 (d-pad, else the left stick)
	GN_COUNT = 128,
};
