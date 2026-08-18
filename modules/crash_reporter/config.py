def can_build(env, platform):
    if platform not in ("windows", "linuxbsd"):
        return False
    if env.editor_build:
        return True  # settings UI + console sink; Breakpad only if editor_crash_reporter=yes
    return bool(env.get("crash_reporter", False))


def configure(env):
    pass


def get_doc_classes():
    return [
        "CrashReporter",
    ]


def get_doc_path():
    return "doc_classes"
