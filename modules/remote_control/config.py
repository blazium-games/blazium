def can_build(env, platform):
    env.module_add_dependencies("remote_control", ["httpserver"], True)
    env.module_add_dependencies("remote_control", ["luau_module"], False)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "RemoteControlServer",
        "RemoteControlRegistry",
    ]


def get_doc_path():
    return "doc_classes"
