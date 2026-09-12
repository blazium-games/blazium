def can_build(env, platform):
    return platform in ["windows", "linuxbsd"]


def configure(env):
    env.module_add_dependencies("discord_module", ["jwttool"], True)


def get_doc_classes():
    return [
        "Discord",
        "DiscordAuthResult",
    ]


def get_doc_path():
    return "doc_classes"
