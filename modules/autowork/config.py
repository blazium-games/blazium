#!/usr/bin/env python
def is_enabled():
    from SCons.Script import ARGUMENTS

    # Enabled by default on editor builds only. template_debug can opt in via
    # module_autowork_enabled=yes; template_release is never built.
    return ARGUMENTS.get("target", "editor") == "editor"


def can_build(env, platform):
    # Editor always (when module enabled). template_debug only when opted in.
    # template_release is never built, even with module_autowork_enabled=yes.
    if env.editor_build:
        return True
    return env["target"] == "template_debug"


def configure(env):
    env.module_add_dependencies("autowork", ["gdscript"], True)


def get_doc_classes():
    return [
        "Autowork",
        "AutoworkCollector",
        "AutoworkConfig",
        "AutoworkDoubler",
        "AutoworkHookScript",
        "AutoworkInputSender",
        "AutoworkLogger",
        "AutoworkRuntimeUI",
        "AutoworkSignalHook",
        "AutoworkSignalWatcher",
        "AutoworkSpy",
        "AutoworkStubParams",
        "AutoworkStubber",
        "AutoworkTest",
        "AutoworkVSCodeDebugger",
        "AutoworkE2EConfig",
        "AutoworkE2EServer",
    ]


def get_doc_path():
    return "doc_classes"
