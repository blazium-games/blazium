def can_build(env, platform):
    # Not usable on web, iOS, Android, macOS, or visionOS.
    if platform in ("android", "ios", "macos", "web", "visionos"):
        return False
    return platform in ("windows", "linuxbsd")


def configure(env):
    pass


def get_doc_classes():
    return [
        "Anticheat",
        "AnticheatEditorPlugin",
    ]


def get_doc_path():
    return "doc_classes"
