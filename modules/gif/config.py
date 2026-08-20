def is_enabled():
    return True


def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "GIFAnimation",
        "GIFTexture",
        "GIFSprite2D",
        "GIFRecorder",
        "ResourceImporterGIF",
    ]


def get_doc_path():
    return "doc_classes"
