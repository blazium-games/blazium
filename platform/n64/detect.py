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
        "N64",
        "N64Checkpoint",
        "N64CameraVolume",
        "N64Follow",
        "N64Path",
        "N64Waypoint",
        "N64NavRegion",
        "N64Particles",
        "N64Vehicle",
        "N64Tween",
        "N64Spawner",
        "N64LoadZone",
        "N64UnloadZone",
        "N64FlavorEditorPlugin",
    ]


def get_doc_path():
    return "export/doc_classes"
