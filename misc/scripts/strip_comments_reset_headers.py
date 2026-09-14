#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Strip C-family comments from source bodies and rewrite standard Godot/Blazium license headers.

Limitations: does not model every preprocessor edge case; Objective-C @"..." strings may be
altered if they contain sequences that look like comments outside string rules."""

from __future__ import annotations

import glob
import io
import os
import sys

# Header template structure (placeholders will be replaced with centered text)
header_top = """\
/**************************************************************************/
/*  $filename                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*  {engine_name}  */
/*  {engine_url}  */
/**************************************************************************/"""

# Copyright line templates
blazium_copyright = "/* Copyright (c) 2024-present Blazium Engine contributors.                */"
godot_copyright = "/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */"
juan_ariel_copyright = "/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */"

# License text (common to all)
license_text = """\
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/
"""

VALID_EXTENSIONS = {".cpp", ".h", ".hpp", ".c", ".cc", ".cxx", ".java", ".m", ".mm", ".glsl", ".inc"}


def collect_files(paths, recursive=False):
    """Collect all files to process from given paths (files, wildcards, or directories)."""
    files_to_process = []

    for path_pattern in paths:
        path_pattern = path_pattern.strip()

        if "*" in path_pattern or "?" in path_pattern:
            if recursive:
                matched_files = glob.glob(path_pattern, recursive=True)
            else:
                matched_files = glob.glob(path_pattern)

            for matched in matched_files:
                if os.path.isfile(matched):
                    ext = os.path.splitext(matched)[1]
                    if ext in VALID_EXTENSIONS:
                        files_to_process.append(matched)

        elif os.path.isdir(path_pattern):
            if recursive:
                for root, _dirs, files in os.walk(path_pattern):
                    for file in files:
                        ext = os.path.splitext(file)[1]
                        if ext in VALID_EXTENSIONS:
                            files_to_process.append(os.path.join(root, file))
            else:
                for item in os.listdir(path_pattern):
                    full_path = os.path.join(path_pattern, item)
                    if os.path.isfile(full_path):
                        ext = os.path.splitext(item)[1]
                        if ext in VALID_EXTENSIONS:
                            files_to_process.append(full_path)

        elif os.path.isfile(path_pattern):
            files_to_process.append(path_pattern)
        else:
            print(f"Warning: Path not found: {path_pattern}")

    return files_to_process


def build_header(use_blazium, use_godot):
    if use_blazium:
        engine_name = "BLAZIUM ENGINE"
        engine_url = "https://blazium.app"
    else:
        engine_name = "GODOT ENGINE"
        engine_url = "https://godotengine.org"

    engine_name_padding = 68 - len(engine_name)
    left_padding_name = engine_name_padding // 2
    right_padding_name = engine_name_padding - left_padding_name
    engine_name_padded = " " * left_padding_name + engine_name + " " * right_padding_name

    engine_url_padding = 68 - len(engine_url)
    left_padding_url = engine_url_padding // 2
    right_padding_url = engine_url_padding - left_padding_url
    engine_url_padded = " " * left_padding_url + engine_url + " " * right_padding_url

    header = header_top.format(engine_name=engine_name_padded, engine_url=engine_url_padded)
    header += "\n"

    if use_blazium and use_godot:
        header += blazium_copyright + "\n"
        header += godot_copyright + "\n"
        header += juan_ariel_copyright + "\n"
    elif use_blazium:
        header += blazium_copyright + "\n"
    elif use_godot:
        header += godot_copyright + "\n"
        header += juan_ariel_copyright + "\n"

    header += license_text
    return header


def detach_license_header(content: str) -> str:
    """Match set_copyright_headers.py: drop leading blank lines, optional /* block starting with /**********."""
    fileread = io.StringIO(content)
    line = fileread.readline()
    header_done = False
    text_parts = []

    while line.strip() == "" and line != "":
        line = fileread.readline()

    if line.find("/**********") == -1:
        header_done = True

    while not header_done:
        if line.find("/*") != 0:
            header_done = True
            if line.strip() != "":
                text_parts.append(line)
        line = fileread.readline()

    while line != "":
        text_parts.append(line)
        line = fileread.readline()

    return "".join(text_parts)


def _raw_string_prefix_start(s: str, i: int) -> int | None:
    """Return index of 'R' if s[i:] begins an optional-prefix raw string (u8R, LR, UR, uR, R)."""
    if s.startswith('u8R"', i):
        return i + 2
    if s.startswith('LR"', i) or s.startswith('UR"', i) or s.startswith('uR"', i):
        return i + 1
    if s.startswith('R"', i):
        return i
    return None


def _try_consume_raw_string(s: str, i: int, out: list[str]) -> int | None:
    r_pos = _raw_string_prefix_start(s, i)
    if r_pos is None:
        return None
    if not s.startswith('R"', r_pos):
        return None
    j = r_pos + 2
    open_paren = s.find("(", j)
    if open_paren == -1:
        return None
    delim = s[j:open_paren]
    closing = ")" + delim + '"'
    close_idx = s.find(closing, open_paren + 1)
    if close_idx == -1:
        return None
    end = close_idx + len(closing)
    out.append(s[i:end])
    return end


def strip_c_family_comments(body: str, path_hint: str = "") -> str:
    """Remove // and /* */ outside strings and character literals; preserve newlines for //."""
    out: list[str] = []
    n = len(body)
    i = 0
    mode = "code"

    while i < n:
        c = body[i]

        if mode == "code":
            raw_end = _try_consume_raw_string(body, i, out)
            if raw_end is not None:
                i = raw_end
                continue
            if i + 1 < n and body[i : i + 2] == "//":
                i += 2
                while i < n and body[i] != "\n":
                    i += 1
                continue
            if i + 1 < n and body[i : i + 2] == "/*":
                i += 2
                close = body.find("*/", i)
                if close == -1:
                    if path_hint:
                        print(f"Warning: unterminated block comment in {path_hint}, stripping to EOF", file=sys.stderr)
                    i = n
                    break
                i = close + 2
                continue
            if c == '"':
                mode = "string"
                out.append(c)
                i += 1
                continue
            if c == "'":
                mode = "char"
                out.append(c)
                i += 1
                continue
            out.append(c)
            i += 1
            continue

        if mode == "string":
            out.append(c)
            if c == "\\":
                if i + 1 < n:
                    out.append(body[i + 1])
                    i += 2
                else:
                    i += 1
                continue
            if c == '"':
                mode = "code"
            i += 1
            continue

        if mode == "char":
            out.append(c)
            if c == "\\":
                if i + 1 < n:
                    out.append(body[i + 1])
                    i += 2
                else:
                    i += 1
                continue
            if c == "'":
                mode = "code"
            i += 1
            continue

    return "".join(out)


def header_text_for_file(header_template: str, fname: str) -> str:
    fsingle = os.path.basename(fname.strip())
    rep_fl = "$filename"
    rep_fi = fsingle
    len_fl = len(rep_fl)
    len_fi = len(rep_fi)
    if len_fi < len_fl:
        rep_fi += " " * (len_fl - len_fi)
    elif len_fl < len_fi:
        rep_fl += " " * (len_fi - len_fl)
    if header_template.find(rep_fl) != -1:
        text = header_template.replace(rep_fl, rep_fi)
    else:
        text = header_template.replace("$filename", fsingle)
    return text + "\n"


def main() -> int:
    argv = sys.argv[:]
    use_blazium = "--blazium" in argv
    use_godot = "--godot" in argv
    recursive = "--recursive" in argv or "-r" in argv

    if use_blazium:
        argv.remove("--blazium")
    if use_godot:
        argv.remove("--godot")
    if "--recursive" in argv:
        argv.remove("--recursive")
    if "-r" in argv:
        argv.remove("-r")

    if len(argv) < 2:
        print(
            "Invalid usage of strip_comments_reset_headers.py; pass one or more paths.",
            file=sys.stderr,
        )
        print(
            "Usage: python strip_comments_reset_headers.py [--blazium] [--godot] [--recursive|-r] <path1> [path2] ...",
            file=sys.stderr,
        )
        print("  Strips // and /* */ comments from file bodies, then applies standard license header.", file=sys.stderr)
        print("  --blazium / --godot: same as set_copyright_headers.py", file=sys.stderr)
        print("  --recursive, -r: recurse into directories", file=sys.stderr)
        return 1

    if not use_blazium and not use_godot:
        print("Error: At least one of --blazium or --godot flag must be specified.", file=sys.stderr)
        return 1

    header_template = build_header(use_blazium, use_godot)
    files_to_process = collect_files(argv[1:], recursive)

    if not files_to_process:
        print("No files found to process.")
        return 0

    print(f"Processing {len(files_to_process)} file(s)...")

    for fname in files_to_process:
        path = fname.strip()
        with open(path, "r", encoding="utf-8") as f:
            raw = f.read()

        body = detach_license_header(raw)
        stripped = strip_c_family_comments(body, path_hint=path)
        text = header_text_for_file(header_template, path) + stripped

        with open(path, "w", encoding="utf-8", newline="\n") as filewrite:
            filewrite.write(text)

        print(f"  Processed: {fname}")

    print(f"\nSuccessfully processed {len(files_to_process)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
