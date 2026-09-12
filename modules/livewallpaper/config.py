def get_opts(platform):
    from SCons.Variables import BoolVariable

    return [
        BoolVariable(
            "livewallpaper_template",
            "Build livewallpaper-flavored export templates (.livewallpaper suffix)",
            False,
        ),
    ]


def can_build(env, platform):
    if platform != "windows":
        return False
    if env.editor_build:
        return True
    return bool(env.get("livewallpaper_template", False))


def configure(env):
    if not env.editor_build:
        env.add_module_version_string("livewallpaper")


def get_doc_classes():
    return [
        "LiveWallpaper",
        "LiveWallpaperEditorPlugin",
        "EditorExportPlatformWindowsLiveWallpaper",
    ]


def get_doc_path():
    return "doc_classes"
