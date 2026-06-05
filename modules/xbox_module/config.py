import os


def is_enabled():
    # Disabled by default. Enable with: module_xbox_module_enabled=yes
    return False


def get_opts(platform):
    from SCons.Variables import PathVariable

    return [
        PathVariable(
            "gdk_path",
            "Path to Microsoft GDK installation (edition or windows layout)",
            "",
            PathVariable.PathAccept,
        ),
    ]


def can_build(env, platform):
    return platform == "windows"


def _normalize_gdk_windows_path(path):
    if not path:
        return None

    path = os.path.normpath(path)

    if os.path.isfile(os.path.join(path, "include", "XGameRuntime.h")):
        return path

    windows_subdir = os.path.join(path, "windows")
    if os.path.isfile(os.path.join(windows_subdir, "include", "XGameRuntime.h")):
        return windows_subdir

    grdk_sibling = os.path.join(os.path.dirname(path), "windows")
    if os.path.isfile(os.path.join(grdk_sibling, "include", "XGameRuntime.h")):
        return grdk_sibling

    return None


def _pick_latest_version_dir(root):
    if not root or not os.path.isdir(root):
        return None

    best = None
    best_num = -1
    for entry in os.listdir(root):
        full = os.path.join(root, entry)
        if not os.path.isdir(full):
            continue
        if not entry.isdigit():
            continue
        version_num = int(entry)
        if version_num > best_num:
            candidate = _normalize_gdk_windows_path(full)
            if candidate:
                best = candidate
                best_num = version_num

    return best


def _discover_default_gdk_install():
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    default_root = os.path.join(program_files_x86, "Microsoft GDK")
    if not os.path.isdir(default_root):
        return None
    return _pick_latest_version_dir(default_root)


def _discover_gdk_path(env):
    explicit = str(env.get("gdk_path", "")).strip()
    if not explicit:
        explicit = os.environ.get("gdk_path", "").strip()
    if explicit:
        normalized = _normalize_gdk_windows_path(explicit)
        if normalized:
            return normalized
        latest = _pick_latest_version_dir(explicit)
        if latest:
            return latest

    for env_name in ("GameDKCoreLatest", "GameDKLatest", "GRDKLatest"):
        env_value = os.environ.get(env_name, "").strip()
        if not env_value:
            continue
        normalized = _normalize_gdk_windows_path(env_value)
        if normalized:
            return normalized

    for env_name in ("GameDK", "GRDK"):
        env_value = os.environ.get(env_name, "").strip()
        if not env_value:
            continue
        latest = _pick_latest_version_dir(env_value)
        if latest:
            return latest

    return _discover_default_gdk_install()


def _copy_gdk_runtime_dlls(env, gdk_windows):
    bin_dir = os.path.join(gdk_windows, "bin", "x64")
    if not os.path.isdir(bin_dir):
        return

    dll_names = [
        "libHttpClient.dll",
        "XCurl.dll",
        "Microsoft.Xbox.Services.C.Thunks.dll",
    ]
    if env["target"] in ("editor", "template_debug"):
        dll_names.append("Microsoft.Xbox.Services.C.Thunks.Debug.dll")

    output_dir = os.path.join(env.Dir("#").abspath, "bin")
    os.makedirs(output_dir, exist_ok=True)

    for dll_name in dll_names:
        src = os.path.join(bin_dir, dll_name)
        if not os.path.isfile(src):
            continue
        dst = os.path.join(output_dir, dll_name)
        if os.path.isfile(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
            continue
        try:
            import shutil

            shutil.copy2(src, dst)
        except OSError as exc:
            print(f"xbox_module: failed to copy GDK runtime DLL {dll_name}: {exc}")
            return

    print(f"xbox_module: staged GDK runtime DLLs in {output_dir}")


def configure(env):
    gdk_windows = _discover_gdk_path(env)
    env["xbox_module_gdk"] = gdk_windows is not None

    if not gdk_windows:
        print(
            "xbox_module: Microsoft GDK not found (set gdk_path or GameDKCoreLatest/GameDKLatest). "
            "Building stub implementation without XBOX_MODULE_GDK_ENABLED."
        )
        return

    include_dir = os.path.join(gdk_windows, "include")
    lib_dir = os.path.join(gdk_windows, "lib", "x64")

    if not os.path.isdir(include_dir):
        print(f"xbox_module: GDK include directory not found at {include_dir}")
        env["xbox_module_gdk"] = False
        return

    if not os.path.isdir(lib_dir):
        print(f"xbox_module: GDK lib directory not found at {lib_dir}")
        env["xbox_module_gdk"] = False
        return

    env.Append(
        CPPDEFINES=[
            "XBOX_MODULE_GDK_ENABLED",
            "_GAMING_DESKTOP",
            ("WINAPI_FAMILY", "WINAPI_FAMILY_DESKTOP_APP"),
        ]
    )
    env.Append(CPPPATH=[include_dir])
    env.Append(LIBPATH=[lib_dir])

    # Custom-engine PC integration: GRTS via xgameruntime.lib; XSAPI via Thunks
    # import lib + runtime DLL (see Microsoft GDK custom-engine get-started guide).
    gdk_libs = [
        "xgameruntime",
        "Microsoft.Xbox.Services.C.Thunks",
        "libHttpClient",
        "XCurl",
        "GameChat2",
    ]
    for lib_name in gdk_libs:
        lib_file = os.path.join(lib_dir, lib_name + ".lib")
        if os.path.isfile(lib_file):
            env.Append(LIBS=[env.File(lib_file)])
        else:
            print(f"xbox_module: GDK library not found: {lib_file}")
            env["xbox_module_gdk"] = False
            return

    _copy_gdk_runtime_dlls(env, gdk_windows)

    print(f"xbox_module: Microsoft GDK enabled from {gdk_windows} (XSAPI via Thunks.dll)")


def get_doc_classes():
    return [
        "GDK",
        "GDKResult",
        "GDKPendingSignal",
        "GDKUser",
        "GDKUsers",
        "GDKGameUI",
        "GDKAccessibility",
        "GDKAchievement",
        "GDKAchievements",
        "GDKPackage",
        "GDKStats",
        "GDKLeaderboards",
        "GDKPrivacy",
        "GDKPresence",
        "GDKSocial",
        "GDKStore",
        "GDKProfile",
        "GDKStringVerify",
        "GDKTitleStorage",
        "GDKErrorReporting",
        "GDKLauncher",
        "GDKMultiplayerActivity",
        "GDKCapture",
        "GDKSystem",
        "GDKDisplay",
        "GDKActivation",
        "XboxEditorPlugin",
    ]


def get_doc_path():
    return "doc_classes"
