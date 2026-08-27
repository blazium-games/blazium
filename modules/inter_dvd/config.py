def can_build(env, platform):
    return env.editor_build


def configure(env):
    pass


def get_doc_classes():
    return [
        "InterDVDInstruction",
        "InterDVDMachine",
        "InterDVDCell",
        "InterDVDButton",
        "InterDVDMenu",
        "InterDVDPGC",
        "InterDVDProject",
        "InterDVDIfoWriter",
        "InterDVDSceneBaker",
        "InterDVDEditorPlugin",
        "InterDVDExportProgress",
        "InterDVDStream",
        "EditorExportPlatformWindowsInterDVD",
    ]


def get_doc_path():
    return "doc_classes"
