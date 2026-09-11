def is_enabled():
    from SCons.Script import ARGUMENTS

    # Enabled by default on editor builds only.
    return ARGUMENTS.get("target", "editor") == "editor"


def can_build(env, platform):
    if not env.editor_build:
        return False
    env.module_add_dependencies("navimesh_export", ["navigation"], True)
    env.module_add_dependencies("navimesh_export", ["justamcp"], False)
    env.module_add_dependencies("navimesh_export", ["remote_control"], False)
    return True


def configure(env):
    env.Append(CPPDEFINES=["NAVIMESH_EXPORT_MODULE_ENABLED"])


def get_doc_classes():
    return [
        "NavimeshExporter",
        "NavimeshExportTools",
    ]


def get_doc_path():
    return "doc_classes"
