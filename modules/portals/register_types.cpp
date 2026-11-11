/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "html_portals.h"
#include "minigame_host.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#ifdef TESTS_ENABLED
#include "tests/test_portals.h"
#endif

static Vector<Object *> portal_singletons;

static void add_portal_singleton(const char *p_name, Object *p_object) {
	Engine::get_singleton()->add_singleton(Engine::Singleton(p_name, p_object));
	portal_singletons.push_back(p_object);
}

void initialize_portals_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(Betacade);
	GDREGISTER_CLASS(YouTube);
	GDREGISTER_CLASS(Telegram);
	GDREGISTER_CLASS(Reddit);
	GDREGISTER_CLASS(Newgrounds);
	GDREGISTER_CLASS(CrazyGames);
	GDREGISTER_CLASS(Poki);
	GDREGISTER_CLASS(Meta);
	GDREGISTER_CLASS(GameDistribution);
	GDREGISTER_CLASS(WeChat);
	GDREGISTER_CLASS(Douyin);
	GDREGISTER_CLASS(TikTok);

	add_portal_singleton("Betacade", memnew(Betacade));
	add_portal_singleton("YouTube", memnew(YouTube));
	add_portal_singleton("Telegram", memnew(Telegram));
	add_portal_singleton("Reddit", memnew(Reddit));
	add_portal_singleton("Newgrounds", memnew(Newgrounds));
	add_portal_singleton("CrazyGames", memnew(CrazyGames));
	add_portal_singleton("Poki", memnew(Poki));
	add_portal_singleton("Meta", memnew(Meta));
	add_portal_singleton("GameDistribution", memnew(GameDistribution));
	add_portal_singleton("WeChat", memnew(WeChat));
	add_portal_singleton("Douyin", memnew(Douyin));
	add_portal_singleton("TikTok", memnew(TikTok));
}

void uninitialize_portals_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	for (int i = portal_singletons.size() - 1; i >= 0; i--) {
		Object *object = portal_singletons[i];
		Engine::get_singleton()->remove_singleton(object->get_class());
		memdelete(object);
	}
	portal_singletons.clear();
}
