def can_build(env, platform):
    return True


def configure(env):
    # Public headers use turnbattle/*; tests compile outside this module's SCsub.
    env.Prepend(CPPPATH=["#modules/town_sdk/include"])


def get_doc_classes():
    return ["TownSdkClient"]


def get_doc_path():
    return "doc_classes"
