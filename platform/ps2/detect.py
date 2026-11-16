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
        "PS2",
        "PS2Spawner",
        "PS2CameraVolume",
        "PS2Follow",
        "PS2Waypoint",
        "PS2Path",
        "PS2NavRegion",
        "PS2Vehicle",
        "PS2Particles",
        "PS2Tween",
        "PS2SpawnPoint",
        "PS2Checkpoint",
        "PS2Portal",
        "PS2SavePoint",
        "PS2MusicZone",
        "PS2LoadZone",
        "PS2UnloadZone",
        "PS2Hazard",
        "PS2Pickup",
        "PS2Talk",
        "PS2FlavorEditorPlugin",
    ]


def get_doc_path():
    return "export/doc_classes"
