def can_build(env, platform):
    # HTTP-only; all platforms. Editor always compiles (settings + singleton).
    if env.editor_build:
        return True
    return bool(env.get("analytics", False))


def configure(env):
    pass


def get_doc_classes():
    return [
        "Analytics",
    ]


def get_doc_path():
    return "doc_classes"
