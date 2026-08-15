def can_build(env, platform):
    return platform in ["windows", "linuxbsd"]


def configure(env):
    env.module_add_dependencies("steam", ["jwttool"], True)


def get_doc_classes():
    return [
        "Steam",
        "SteamAuthResult",
        "SteamAchievementInfo",
        "SteamInventoryItem",
        "SteamItemDefinition",
        "SteamEditorPlugin",
    ]


def get_doc_path():
    return "doc_classes"
