def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "CSV",
        "CSVExporter",
        "CSVDialect",
        "CSVAsyncTask",
        "CSVChunkProcessor",
        "CSVRowModel",
        "CSVImporter",
        "CSVTable",
        "CSVIndex",
        "CSVWriter",
        "CSVReader",
        "DSVExporter",
        "DSVImporter",
        "DSVWriter",
        "DSVReader",
        "ResourceImporterCSV",
    ]


def get_doc_path():
    return "doc_classes"
