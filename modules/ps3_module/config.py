import os


def is_enabled():
    return True


def can_build(env, platform):
    return platform == "ps3"


def configure(env):
    env.Append(CPPDEFINES=["PS3_MODULE_ENABLED"])


def get_doc_classes():
    return [
        "PS3Module",
        "PS3Pad",
        "PS3SystemUtil",
    ]


def get_doc_path():
    return "doc_classes"
