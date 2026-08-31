def get_opts(platform):
    from SCons.Variables import BoolVariable

    return [
        BoolVariable(
            "screensaver_template",
            "Build screensaver-flavored export templates (.screensaver suffix)",
            False,
        ),
    ]


def can_build(env, platform):
    if platform != "windows":
        return False
    if env.editor_build:
        return True
    return bool(env.get("screensaver_template", False))


def configure(env):
    if not env.editor_build:
        env.add_module_version_string("screensaver")


def get_doc_classes():
    return [
        "Screensaver",
        "ScreensaverEditorPlugin",
        "EditorExportPlatformWindowsScreensaver",
    ]


def get_doc_path():
    return "doc_classes"
