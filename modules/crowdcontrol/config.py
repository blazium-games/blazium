def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "CrowdControl",
        "CrowdControlEffectParameter",
        "CrowdControlEffect",
        "CrowdControlGamePackMeta",
        "CrowdControlGamePack",
    ]


def get_doc_path():
    return "doc_classes"
