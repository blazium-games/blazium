/**************************************************************************/
/*  ddd_luau_check.cpp                                                    */
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

#include "ddd_luau_check.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"

#ifdef MODULE_LUAU_MODULE_ENABLED
#include "modules/luau_module/luau.h"
#endif

String DDDLuauCheck::stub_preamble() {
	return R"LUAU(-- DDDBrowser API stubs (editor check only; stripped at export)
local Engine = {
	getActionState = function(_name) return false end,
	consumeAction = function(_name) return false end,
	setEntityPosition = function(_id, _x, _y, _z) end,
	getEntityPosition = function(_id) return { x = 0, y = 0, z = 0 } end,
	setEntityRotation = function(_id, _x, _y, _z) end,
	getEntityRotation = function(_id) return { x = 0, y = 0, z = 0 } end,
	setEntityScale = function(_id, _x, _y, _z) end,
	getEntityScale = function(_id) return { x = 1, y = 1, z = 1 } end,
	setEntityVisible = function(_id, _v) end,
	getEntityVisible = function(_id) return true end,
	getCamera = function() return { position = { x = 0, y = 0, z = 0 }, forward = { x = 0, y = 0, z = -1 } } end,
	setCamera = function(_pos, _fwd) end,
	triggerPortal = function(_id) return false end,
	spawnEntity = function(_req) return nil end,
	destroyEntity = function(_id) return false end,
	raycast = function(_o, _d, _max) return { hit = false } end,
	sphereOverlap = function(_c, _r) return {} end,
	httpRequest = function(_url, _cb) end,
	openTextBox = function(_id, _title, _text, _buttons, _checks, _cb) end,
	closeTextBox = function(_id) end,
	openImGuiTextBox = function(_title, _text, _buttons, _checks, _cb) end,
	closeImGuiTextBox = function() end,
	playAudio = function(_id, _asset, _pos, _ambient, _loop, _vol) end,
	stopAudio = function(_id) end,
	openExternalUrl = function(_url, _cb) end,
	setLightColor = function(_id, _r, _g, _b) end,
	getLightColor = function(_id) return { x = 1, y = 1, z = 1 } end,
	setLightIntensity = function(_id, _v) end,
	getLightIntensity = function(_id) return 1 end,
	setLightEnabled = function(_id, _v) end,
	getLightEnabled = function(_id) return true end,
	setLightRange = function(_id, _v) end,
	getLightRange = function(_id) return 10 end,
}
local Scene = {
	getId = function() return "" end,
	getName = function() return "" end,
	getDescription = function() return "" end,
	getAuthor = function() return "" end,
	getRating = function() return "GENERAL" end,
	getThumbnail = function() return "" end,
	getManifestUrl = function() return "" end,
	getWorldId = function() return "" end,
	getWorldPosition = function() return { x = 0, y = 0, z = 0 } end,
	TravelTo = function(_url) end,
}
local Gamemode = {
	getState = function(_k) return nil end,
	setState = function(_k, _v) end,
	triggerEvent = function(_name, _payload) end,
	onEvent = function(_name, _cb) end,
	saveGame = function() return false end,
}
local localStorage = {
	get = function(_k) return nil end,
	set = function(_k, _v) end,
	remove = function(_k) end,
	clear = function() end,
	getAll = function() return {} end,
}
)LUAU";
}

String DDDLuauCheck::entity_template() {
	return R"LUAU(-- DDDBrowser entity script (not a Blazium gdclass / LuauScript).
-- Lifecycle table returned below runs in DDDBrowser with Engine/Scene/Gamemode/localStorage.
-- Tunables from the instance script.data field are available as self.data.

local Script = {}

function Script:on_start()
	-- self.entity (number), self.asset_id (string), self.data (table)
end

function Script:on_update(dt)
end

function Script:on_interact(actorId)
end

function Script:on_save()
	return {}
end

function Script:on_load(state)
end

function Script:on_shutdown()
end

return Script
)LUAU";
}

String DDDLuauCheck::gamemode_template() {
	return R"LUAU(-- DDDBrowser gamemode script (scene-wide). Declared via Level.gamemode_file.
-- Export emits gamemode.file as this .luau basename (not an asset id).

local GamemodeScript = {}

function GamemodeScript:on_start()
end

function GamemodeScript:on_update(dt)
end

function GamemodeScript:on_save()
	return {}
end

function GamemodeScript:on_load(state)
end

return GamemodeScript
)LUAU";
}

Dictionary DDDLuauCheck::check_source(const String &p_source) {
	Dictionary result;
#ifdef MODULE_LUAU_MODULE_ENABLED
	const String wrapped = stub_preamble() + "\n" + p_source;
	const luau_module::LuauCompileResult compile = luau_module::Luau::compile_with_diagnostics(wrapped);
	if (compile.succeeded()) {
		result["ok"] = true;
		result["message"] = "DDD Luau compile check passed.";
	} else {
		result["ok"] = false;
		String msg = compile.error_message;
		if (compile.error_line >= 0) {
			msg = vformat("Line %d: %s", compile.error_line, compile.error_message);
		}
		result["message"] = msg;
	}
#else
	result["ok"] = false;
	result["message"] = "luau_module is disabled; cannot compile-check DDD Luau.";
#endif
	return result;
}

Dictionary DDDLuauCheck::check_file(const String &p_path) {
	String path = p_path;
	if (path.begins_with("res://") || path.begins_with("user://")) {
		path = ProjectSettings::get_singleton()->globalize_path(path);
	}
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	Dictionary result;
	if (f.is_null()) {
		result["ok"] = false;
		result["message"] = vformat("Cannot read script: %s", p_path);
		return result;
	}
	return check_source(f->get_as_text());
}
