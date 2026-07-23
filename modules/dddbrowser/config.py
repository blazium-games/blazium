def can_build(env, platform):
    # Soft deps: preview uses httpserver; DDD Luau compile-check uses luau_module when present.
    env.module_add_dependencies("dddbrowser", ["httpserver", "luau_module"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "DDDBrowserLevel",
        "DDDBrowserSpawn",
        "DDDBrowserPortal",
        "DDDBrowserVolume",
        "DDDBrowserModel",
        "DDDBrowserTextbox",
        "DDDBrowserPicturebox",
        "DDDBrowserAudio",
        "DDDBrowserFont",
        "DDDBrowserScript",
        "DDDBrowserExporter",
        "DDDBrowserPreviewServer",
    ]


def get_doc_path():
    return "doc_classes"
