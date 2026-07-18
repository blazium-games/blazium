def is_enabled():
    from SCons.Script import ARGUMENTS

    return ARGUMENTS.get("target", "editor") == "editor"


def can_build(env, platform):
    env.module_add_dependencies("semanticsearch", ["assettags"], False)
    return env.editor_build


def configure(env):
    pass


def get_doc_classes():
    return [
        "SemanticAssetIndex",
        "SemanticSearchBackend",
        "LexicalTagBackend",
        "EmbeddingBackend",
        "SemanticAsyncSearchWorker",
        "SemanticAsyncEmbedWorker",
    ]


def get_doc_path():
    return "doc_classes"
