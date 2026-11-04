#!/usr/bin/env python3
from __future__ import annotations

if __name__ != "__main__":
    raise ImportError(f"{__name__} should not be used as a module.")

import os
import sys
from typing import Any, Callable

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from gles3_builders import build_gles3_header
from glsl_builders import build_raw_header, build_rd_header

FUNC_PATH_KWARGS: list[tuple[Callable[..., None], str, dict[str, Any]]] = [
    (
        build_gles3_header,
        "tests/python_build/fixtures/gles3/vertex_fragment.glsl",
        {
            "include": "drivers/gles3/shader_gles3.h",
            "class_suffix": "GLES3",
            "optional_output_filename": "tests/python_build/fixtures/gles3/vertex_fragment.out",
        },
    ),
    (
        build_raw_header,
        "tests/python_build/fixtures/glsl/compute.glsl",
        {
            "optional_output_filename": "tests/python_build/fixtures/glsl/compute.out",
        },
    ),
    (
        build_raw_header,
        "tests/python_build/fixtures/glsl/vertex_fragment.glsl",
        {
            "optional_output_filename": "tests/python_build/fixtures/glsl/vertex_fragment.out",
        },
    ),
    (
        build_rd_header,
        "tests/python_build/fixtures/rd_glsl/compute.glsl",
        {
            "optional_output_filename": "tests/python_build/fixtures/rd_glsl/compute.out",
        },
    ),
    (
        build_rd_header,
        "tests/python_build/fixtures/rd_glsl/vertex_fragment.glsl",
        {
            "optional_output_filename": "tests/python_build/fixtures/rd_glsl/vertex_fragment.out",
        },
    ),
]


def main() -> int:
    ret = 0

    for func, path, kwargs in FUNC_PATH_KWARGS:
        if os.path.exists(out_path := os.path.abspath(path)):
            with open(out_path, "rb") as file:
                raw = file.read()
            func(path, **kwargs)
            with open(out_path, "rb") as file:
                if raw != file.read():
                    ret += 1
        else:
            func(path, **kwargs)
            ret += 1

    return ret


sys.exit(main())
