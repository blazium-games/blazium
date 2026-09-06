def can_build(env, platform):
    return platform == "windows" and env.editor_build


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
        "InterDVDHotspot",
        "InterDVDChapter",
        "InterDVDTitle",
        "InterDVDTitleSet",
        "InterDVDMenuPage",
        "InterDVDDisc",
        "InterDVDIfoWriter",
        "InterDVDSceneBaker",
        "InterDVDEditorPlugin",
        "InterDVDExportProgress",
        "InterDVDStream",
        "EditorExportPlatformWindowsInterDVD",
    ]


def get_doc_path():
    return "doc_classes"
