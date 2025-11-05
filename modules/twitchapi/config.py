def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "TwitchAPI",
        "TwitchHTTPClient",
        "TwitchRequestBase",
        "TwitchAdsRequests",
        "TwitchAnalyticsRequests",
        "TwitchBitsRequests",
        "TwitchChannelPointsRequests",
        "TwitchChannelsRequests",
        "TwitchChatRequests",
        "TwitchClipsRequests",
        "TwitchGamesRequests",
        "TwitchModerationRequests",
        "TwitchStreamsRequests",
        "TwitchUsersRequests",
    ]


def get_doc_path():
    return "doc_classes"
