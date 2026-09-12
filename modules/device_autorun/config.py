def can_build(env, platform):
    return env.editor_build


def configure(env):
    pass


def get_doc_classes():
    return [
        "AutorunInf",
        "DeviceAutorunEditorPlugin",
        "EditorExportDeviceAutorun",
    ]


def get_doc_path():
    return "doc_classes"
