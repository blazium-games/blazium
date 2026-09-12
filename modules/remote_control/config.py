def can_build(env, platform):
    env.module_add_dependencies("remote_control", ["httpserver"], True)
    env.module_add_dependencies("remote_control", ["luau_module"], False)
    # JustAMCP is a compile-time optional peer (mcp_status builtin) via
    # MODULE_JUSTAMCP_ENABLED — do not soft-dep here (circular with justamcp).
    if env.editor_build:
        return True
    # Export templates: only when building Hub-capable templates.
    return bool(env.get("hub_build", False))


def configure(env):
    pass


def get_doc_classes():
    return [
        "RemoteControlServer",
        "RemoteControlRegistry",
    ]


def get_doc_path():
    return "doc_classes"
