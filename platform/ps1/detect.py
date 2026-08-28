# PlayStation 1 is an export-only target. The editor is never built as
# `scons platform=ps1`. Guest binaries are produced at Export time by
# spawning blazium-toolchain (see platform/ps1/runtime/).


def get_name():
    return "PlayStation 1"


def can_build():
    return False


def get_opts():
    return []


def get_flags():
    return {}


def get_doc_classes():
    return [
        "EditorExportPlatformPS1",
    ]


def get_doc_path():
    return "export/doc_classes"
