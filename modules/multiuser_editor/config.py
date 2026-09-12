def can_build(env, platform):
    env.module_add_dependencies("multiuser_editor", ["enet"], True)
    return env.editor_build


def configure(env):
    pass


def get_doc_classes():
    return [
        "MultiuserEditorPlugin",
        "MultiuserEditorPermissions",
        "MultiuserEditorDock",
        "MultiuserEditorGhostCursorOverlay",
        "MultiuserEditorSettingsInspectorPlugin",
        "MultiuserEditorSettingsUI",
    ]


def get_doc_path():
    return "doc_classes"
