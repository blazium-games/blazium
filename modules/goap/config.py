def can_build(env, plat):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "BlaziumGoapAction",
        "BlaziumGoapActionPlanner",
        "BlaziumGoapAgent",
        "BlaziumGoapGoal",
        "BlaziumGoapWorldState",
    ]


def get_doc_path():
    return "doc_classes"
