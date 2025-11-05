def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "KickAPI",
        "KickHTTPClient",
        "KickRequestBase",
        "KickCategoriesRequests",
        "KickChannelsRequests",
        "KickChatRequests",
        "KickEventsRequests",
        "KickLivestreamsRequests",
        "KickModerationRequests",
        "KickOAuthRequests",
        "KickUsersRequests",
    ]


def get_doc_path():
    return "doc_classes"
