def can_build(env, platform):
    """
    RCON module builds on all platforms.
    RCONServer/RCONClient availability is determined at runtime by feature tags.
    """
    return True


def configure(env):
    """
    Configure the RCON module build environment.
    """
    pass


def get_doc_classes():
    """
    Returns list of classes that should have documentation generated.
    """
    return [
        "RCONPacket",
        "RCONServer",
        "RCONClient",
    ]


def get_doc_path():
    """
    Returns the path to the documentation directory.
    """
    return "doc_classes"
