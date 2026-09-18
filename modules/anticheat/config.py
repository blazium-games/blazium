def can_build(env, platform):
    return platform in ["windows", "linuxbsd"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "Anticheat",
        "AnticheatEditorPlugin",
    ]


def get_doc_path():
    return "doc_classes"
