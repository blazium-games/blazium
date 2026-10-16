# PlayStation 2 is an export-only target. The editor is never built as
# `scons platform=ps2`. Guest binaries are produced at Export time by
# spawning blazium-toolchain (guest C++ lives in that CLI, not this tree).


def get_name():
    return "PlayStation 2"


def can_build():
    return False


def get_opts():
    return []


def get_flags():
    return {}


def get_doc_classes():
    return [
        "EditorExportPlatformPS2",
    ]


def get_doc_path():
    return "export/doc_classes"
