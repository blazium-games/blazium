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

#include "crowd_control.h"
#include "crowd_control_effect.h"
#include "crowd_control_effect_parameter.h"
#include "crowd_control_game_pack.h"
#include "crowd_control_game_pack_meta.h"

#ifdef TOOLS_ENABLED
#include "editor/crowd_control_game_pack_editor_plugin.h"
#endif

static CrowdControl *crowd_control_singleton = nullptr;

void initialize_crowdcontrol_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Register singleton
		crowd_control_singleton = memnew(CrowdControl);
		GDREGISTER_CLASS(CrowdControl);
		Engine::get_singleton()->add_singleton(Engine::Singleton("CrowdControl", crowd_control_singleton));

		// Register resource classes
		GDREGISTER_CLASS(CrowdControlEffectParameter);
		GDREGISTER_CLASS(CrowdControlEffect);
		GDREGISTER_CLASS(CrowdControlGamePackMeta);
		GDREGISTER_CLASS(CrowdControlGamePack);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<CrowdControlGamePackEditorPlugin>();
	}
#endif
}

void uninitialize_crowdcontrol_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (crowd_control_singleton) {
			Engine::get_singleton()->remove_singleton("CrowdControl");
			memdelete(crowd_control_singleton);
			crowd_control_singleton = nullptr;
		}
	}
}
