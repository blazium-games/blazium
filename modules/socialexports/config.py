def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "ThirdPartyClient",
        "ReactClient",
        "DiscordEmbeddedAppClient",
        "DiscordEmbeddedAppResponse",
        "DiscordEmbeddedAppResult",
        "YoutubePlayablesClient",
        "YoutubePlayablesResponse",
        "YoutubePlayablesResult",
    ]


def get_doc_path():
    return "doc_classes"
