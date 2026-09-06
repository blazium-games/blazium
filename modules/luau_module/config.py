from SCons.Script import ARGUMENTS

from misc.utility.linker import override_mold_linker_with_gold


def _codegen_platform_supported(env, platform):
    arch = env.get("arch", "")
    if arch not in ("x86_64", "arm64"):
        return False
    if platform == "web":
        return False
    return platform in ("windows", "linuxbsd", "macos", "android", "ios")


def _default_analysis_enabled(env):
    return env.editor_build


def _default_codegen_enabled(env, platform):
    return _codegen_platform_supported(env, platform)


def _enable_web_luau_exceptions(env):
    # Luau requires C++ exceptions; Emscripten cannot link mixed -fno/-f exception TUs.
    if "-fno-exceptions" in env["CXXFLAGS"]:
        env["CXXFLAGS"].remove("-fno-exceptions")
    env.Append(CXXFLAGS=["-fexceptions"])
    env.Append(LINKFLAGS=["-fexceptions"])
    for flag_list_name in ("CCFLAGS", "LINKFLAGS"):
        env[flag_list_name] = [
            flag for flag in env[flag_list_name] if not (isinstance(flag, str) and "SUPPORT_LONGJMP" in flag)
        ]


def can_build(env, platform):
    if env.editor_build:
        env.module_add_dependencies("luau_module", ["jsonrpc"], False)

    # Mold and gold both fail to link luau_module into the huge GCC sanitizer editor
    # binary (relocation overflow). Skip luau on that CI matrix; trenchbroom applies
    # override_mold_linker_with_gold when enabled instead.
    if platform == "linuxbsd" and env.get("linker") == "mold":
        if env.get("use_asan") or env.get("use_ubsan"):
            print("luau_module: disabled for mold+sanitizer builds (linker relocation limit on large editor binaries)")
            return False

    return True


def configure(env):
    env.Append(CPPDEFINES=["LUAU_MODULE_ENABLED"])

    if "module_luau_module_analysis" not in ARGUMENTS:
        env["module_luau_module_analysis"] = _default_analysis_enabled(env)
    if "module_luau_module_codegen" not in ARGUMENTS:
        env["module_luau_module_codegen"] = _default_codegen_enabled(env, env["platform"])

    platform = env["platform"]
    codegen_requested = env["module_luau_module_codegen"]
    codegen_supported = _codegen_platform_supported(env, platform)
    if codegen_requested and not codegen_supported:
        env["module_luau_module_codegen"] = False
    elif codegen_requested and codegen_supported:
        env.Append(CPPDEFINES=["LUAU_MODULE_CODEGEN_ENABLED"])

    if env["module_luau_module_analysis"]:
        env.Append(CPPDEFINES=["LUAU_MODULE_ANALYSIS_ENABLED"])

    if platform == "web":
        env.Append(CPPDEFINES=["LUAU_MODULE_WEB_BRIDGE_ENABLED"])
        _enable_web_luau_exceptions(env)
    elif platform == "linuxbsd":
        override_mold_linker_with_gold(env, "luau_module")


def get_opts(platform):
    return [
        (
            "module_luau_module_codegen",
            "Enable Luau native CodeGen/JIT (default: on for x86_64/arm64 desktop/mobile)",
            False,
        ),
        (
            "module_luau_module_analysis",
            "Enable Luau Analysis for lint/typecheck (default: on in editor only)",
            False,
        ),
    ]


def get_doc_classes():
    return [
        "@Luau",
        "Luau",
        "LuauScript",
        "LuauScriptLanguage",
        "LuaState",
        "LuaCompileOptions",
        "LuaDebug",
        "LuauSignalWaiter",
        "ResourceFormatLoaderLuau",
        "ResourceFormatSaverLuau",
        "LuauSyntaxHighlighter",
        "LuauFormatter",
        "EditorExportLuau",
        "LuauEditorPlugin",
        "LuauLanguageServerPlugin",
        "LuauLanguageProtocol",
        "LuauTextDocument",
        "LuauWorkspace",
    ]


def get_doc_path():
    return "doc_classes"
