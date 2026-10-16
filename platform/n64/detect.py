# Nintendo 64 is an export-only target. The editor is never built as
# `scons platform=n64`. Guest binaries are produced at Export time by
# spawning blazium-toolchain (guest C++ lives in that CLI, not this tree).


def get_name():
    return "Nintendo 64"


def can_build():
    return False


def get_opts():
    return []


def get_flags():
    return {}


def get_doc_classes():
    return [
        "EditorExportPlatformN64",
    ]


def get_doc_path():
    return "export/doc_classes"
