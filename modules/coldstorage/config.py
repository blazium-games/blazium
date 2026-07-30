import os


def get_opts(platform):
    from SCons.Variables import PathVariable

    return [
        PathVariable(
            "coldstorage_path",
            "Path to ColdStorage repo root (contains src/client/sdk); overrides vendored modules/coldstorage/sdk",
            "",
            PathVariable.PathAccept,
        ),
    ]


def resolve_coldstorage_sdk_path(env=None, engine_root=None, module_dir=None):
    """Return absolute ColdStorage repo root if src/client/sdk exists, else ''."""
    candidates = []
    if env is not None:
        configured = env.get("coldstorage_path", "")
        if configured:
            candidates.append(os.path.normpath(str(configured)))

    # Vendored copy inside the Blazium module (primary for CI / releases).
    if module_dir:
        candidates.append(os.path.normpath(module_dir))
        candidates.append(os.path.normpath(os.path.join(module_dir, "sdk")))

    if engine_root:
        engine_root = os.path.normpath(engine_root)
        candidates.append(os.path.normpath(os.path.join(engine_root, "modules", "coldstorage", "sdk")))
        candidates.append(os.path.normpath(os.path.join(engine_root, "..", "coldstorage")))
        candidates.append(os.path.normpath(os.path.join(engine_root, "coldstorage-sdk")))

    for path in candidates:
        if path and os.path.isdir(os.path.join(path, "src", "client", "sdk")):
            return path
    return ""


def can_build(env, platform):
    if not env.editor_build:
        return False

    engine_root = env.Dir("#").abspath
    module_dir = os.path.join(engine_root, "modules", "coldstorage")
    sdk = resolve_coldstorage_sdk_path(env, engine_root, module_dir)
    if not sdk:
        return False

    env.module_add_dependencies("coldstorage", ["mbedtls"], True)
    env["coldstorage_sdk_resolved"] = sdk
    return True


def configure(env):
    pass


def get_doc_classes():
    # JSON comes from engine core/io/json via sdk_shims/nlohmann/json.hpp (no vendored nlohmann).
    return [
        "ColdStorageVCS",
        "ColdStorageEditorPlugin",
        "ColdStorageSettingsInspectorPlugin",
        "ColdStorageSettingsUI",
    ]


def get_doc_path():
    return "doc_classes"
