def can_build(env, platform):
    # Mold and gold both fail once this module is linked into the huge GCC sanitizer
    # editor binary. Skip on that CI matrix (same as luau_module).
    if platform == "linuxbsd" and env.get("linker") == "mold":
        if env.get("use_asan") or env.get("use_ubsan"):
            print("trenchbroom: disabled for mold+sanitizer builds (linker relocation limit on large editor binaries)")
            return False

    return True


def configure(env):
    env.Append(CPPDEFINES=["TRENCHBROOM_MODULE_ENABLED"])


def get_doc_classes():
    return [
        "TrenchbroomMap",
        "TrenchbroomMapSettings",
        "TrenchbroomDefaults",
        "TrenchbroomLocalConfig",
        "BlaziumFGDFile",
        "BlaziumFGDEntityClass",
        "BlaziumFGDBaseClass",
        "BlaziumFGDSolidClass",
        "BlaziumFGDPointClass",
        "BlaziumFGDModelPointClass",
        "BlaziumFGDPointClassDisplayDescriptor",
        "TrenchbroomGameConfig",
        "TrenchbroomTag",
        "NetRadiantCustomShader",
        "NetRadiantCustomGamePackConfig",
        "QuakeMapFile",
        "QuakeWadFile",
        "QuakePaletteFile",
        "ResourceImporterQuakeMap",
        "ResourceImporterQuakeWad",
        "ResourceImporterQuakePalette",
        "ResourceImporterQuakeWal",
        "TrenchbroomEditorPlugin",
    ]


def get_doc_path():
    return "doc_classes"
