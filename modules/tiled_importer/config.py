def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "TiledAnimation",
        "TiledChunk",
        "TiledEnumDefinition",
        "TiledEnumValue",
        "TiledFrame",
        "TiledGrid",
        "TiledLayer",
        "TiledMap",
        "TiledObject",
        "TiledProject",
        "TiledProjectData",
        "TiledProjectFolder",
        "TiledProjectPropertyTypes",
        "TiledProperty",
        "TiledTerrain",
        "TiledText",
        "TiledTile",
        "TiledClass",
        "TiledTileObject",
        "TiledTileset",
        "TiledTileson",
        "TiledTransformations",
        "TiledWangColor",
        "TiledWangSet",
        "TiledWangTile",
        "TiledWorld",
        "TiledWorldMapData",
    ]


def get_doc_path():
    return "doc_classes"
