/**************************************************************************/
/*  justamcp_blazium_resource_registry.cpp                                */
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

#ifdef TOOLS_ENABLED

#include "justamcp_blazium_resource_registry.h"

#include "justamcp_docs_resource_provider.h"
#include "justamcp_guides_resource_provider.h"
#include "justamcp_logs_resource_provider.h"
#include "justamcp_materials_resource_provider.h"
#include "justamcp_node_resource_provider.h"
#include "justamcp_project_resource_provider.h"
#include "justamcp_scene_resource_provider.h"
#include "justamcp_script_resource_provider.h"
#include "justamcp_selection_resource_provider.h"
#include "justamcp_semantic_resource_provider.h"
#include "justamcp_sessions_resource_provider.h"
#include "justamcp_tags_resource_provider.h"

struct BlaziumResourceProviderEntry {
	bool (*can_read)(const String &);
	Dictionary (*read)(const String &, const String &);
};

static const BlaziumResourceProviderEntry g_blazium_resource_providers[] = {
	{ JustAMCPSceneResourceProvider::can_read, JustAMCPSceneResourceProvider::read },
	{ JustAMCPLogsResourceProvider::can_read, JustAMCPLogsResourceProvider::read },
	{ JustAMCPDocsResourceProvider::can_read, JustAMCPDocsResourceProvider::read },
	{ JustAMCPGuidesResourceProvider::can_read, JustAMCPGuidesResourceProvider::read },
	{ JustAMCPNodeResourceProvider::can_read, JustAMCPNodeResourceProvider::read },
	{ JustAMCPScriptResourceProvider::can_read, JustAMCPScriptResourceProvider::read },
	{ JustAMCPTagsResourceProvider::can_read, JustAMCPTagsResourceProvider::read },
	{ JustAMCPSemanticResourceProvider::can_read, JustAMCPSemanticResourceProvider::read },
	{ JustAMCPProjectResourceProvider::can_read, JustAMCPProjectResourceProvider::read },
	{ JustAMCPSelectionResourceProvider::can_read, JustAMCPSelectionResourceProvider::read },
	{ JustAMCPMaterialsResourceProvider::can_read, JustAMCPMaterialsResourceProvider::read },
	{ JustAMCPSessionsResourceProvider::can_read, JustAMCPSessionsResourceProvider::read },
};

bool JustAMCPBlaziumResourceRegistry::can_read(const String &p_canonical_uri) {
	for (int i = 0; i < (int)(sizeof(g_blazium_resource_providers) / sizeof(g_blazium_resource_providers[0])); i++) {
		if (g_blazium_resource_providers[i].can_read(p_canonical_uri)) {
			return true;
		}
	}
	return false;
}

Dictionary JustAMCPBlaziumResourceRegistry::read(const String &p_uri, const String &p_canonical_uri) {
	for (int i = 0; i < (int)(sizeof(g_blazium_resource_providers) / sizeof(g_blazium_resource_providers[0])); i++) {
		if (g_blazium_resource_providers[i].can_read(p_canonical_uri)) {
			return g_blazium_resource_providers[i].read(p_uri, p_canonical_uri);
		}
	}
	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = "Unknown blazium resource URI: " + p_uri;
	result["uri"] = p_uri;
	return result;
}

#endif
