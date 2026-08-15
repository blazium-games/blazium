def is_enabled():
    from SCons.Script import ARGUMENTS

    # Enabled by default on editor builds only.
    return ARGUMENTS.get("target", "editor") == "editor"


def can_build(env, platform):
    return env.editor_build


def configure(env):
    env.Append(CPPDEFINES=["ASSETTAGS_MODULE_ENABLED"])


def get_doc_classes():
    return [
        "AssetTagManager",
        "AssetTagRegistry",
        "AssetTagCoordinator",
        "AssetTagRuntime",
        "AssetTagsEditorPlugin",
        "AssetTagsContextMenuPlugin",
    ]


def get_doc_path():
    return "doc_classes"
