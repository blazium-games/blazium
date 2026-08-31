def can_build(env, platform):
    # Depending on websocket and mbedtls for networking
    env.module_add_dependencies("justamcp", ["websocket", "mbedtls"], True)
    env.module_add_dependencies("justamcp", ["httpserver"], False)
    # Editor-only modules: required in the editor catalog, optional in export templates.
    env.module_add_dependencies("justamcp", ["assettags"], True)
    env.module_add_dependencies("justamcp", ["semanticsearch"], True)
    env.module_add_dependencies("justamcp", ["remote_control"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "JustAMCPRuntime",
        "JustAMCPServer",
        "JustAMCPToolExecutor",
        "JustAMCPSceneTools",
        "JustAMCPResourceTools",
        "JustAMCPAnimationTools",
        "JustAMCPAnalysisTools",
        "JustAMCPAudioTools",
        "JustAMCPBatchTools",
        "JustAMCPDocumentationTools",
        "JustAMCPExportTools",
        "JustAMCPInputTools",
        "JustAMCPMultiuserTools",
        "JustAMCPNodeTools",
        "JustAMCPParticleTools",
        "JustAMCPPhysicsTools",
        "JustAMCPProfilingTools",
        "JustAMCPProjectTools",
        "JustAMCPScene3DTools",
        "JustAMCPScriptTools",
        "JustAMCPShaderTools",
        "JustAMCPThemeTools",
        "JustAMCPTileMapTools",
        "JustAMCPPrompt",
        "JustAMCPPromptBlaziumContext",
        "JustAMCPPromptBlaziumWorkflow",
        "JustAMCPPromptEditorState",
        "JustAMCPPromptExecutor",
        "JustAMCPPromptProjectInfo",
        "JustAMCPResource",
        "JustAMCPResourceExecutor",
        "JustAMCPResourceProjectFile",
        "JustAMCPResourceUI",
        "JustAMCPTaskManager",
        "JustAMCPAssetTagsTools",
        "JustAMCPToolsetRegistry",
        "JustAMCPPromptAssetTaggingWorkflow",
        "JustAMCPToolCategoryBridge",
        "JustAMCPMCPAppsHost",
        "JustAMCPMCPClient",
        "JustAMCPMCPClientBridge",
        "JustAMCPSemanticSearchTools",
    ]


def get_doc_path():
    return "doc_classes"
