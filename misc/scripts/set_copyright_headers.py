#!/usr/bin/env python
# -*- coding: utf-8 -*-

import glob
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

# File extensions to process
VALID_EXTENSIONS = {".cpp", ".h", ".hpp", ".c", ".cc", ".cxx", ".java", ".m", ".mm", ".glsl", ".inc"}

# Parse command line arguments
use_blazium = "--blazium" in sys.argv
use_godot = "--godot" in sys.argv
recursive = "--recursive" in sys.argv or "-r" in sys.argv

if use_blazium:
    sys.argv.remove("--blazium")
if use_godot:
    sys.argv.remove("--godot")
if "--recursive" in sys.argv:
    sys.argv.remove("--recursive")
if "-r" in sys.argv:
    sys.argv.remove("-r")

# Validate arguments
if len(sys.argv) < 2:
    print("Invalid usage of set_copyright_headers.py, it should be called with a path to one or multiple files.")
    print("Usage: python set_copyright_headers.py [--blazium] [--godot] [--recursive|-r] <path1> [path2] ...")
    print("  --blazium: Include Blazium Engine contributors copyright")
    print("  --godot: Include Godot Engine contributors copyright (includes Juan Linietsky, Ariel Manzur)")
    print("  --recursive, -r: Recursively process directories")
    print("  Both --blazium and --godot: Include all copyright lines")
    print("")
    print("Paths can be:")
    print("  - Individual files: file.cpp")
    print("  - Wildcards: *.cpp, src/*.h")
    print("  - Directories: src/ (processes all valid files in directory)")
    sys.exit(1)

if not use_blazium and not use_godot:
    print("Error: At least one of --blazium or --godot flag must be specified.")
    print("Usage: python set_copyright_headers.py [--blazium] [--godot] [--recursive|-r] <path1> [path2] ...")
    sys.exit(1)


# Function to collect files from various input types
def collect_files(paths, recursive=False):
    """Collect all files to process from given paths (files, wildcards, or directories)."""
    files_to_process = []

    for path_pattern in paths:
        path_pattern = path_pattern.strip()

        # Check if it's a wildcard pattern
        if "*" in path_pattern or "?" in path_pattern:
            # Use glob to expand wildcards
            if recursive:
                # For recursive, use ** pattern
                matched_files = glob.glob(path_pattern, recursive=True)
            else:
                matched_files = glob.glob(path_pattern)

            for matched in matched_files:
                if os.path.isfile(matched):
                    ext = os.path.splitext(matched)[1]
                    if ext in VALID_EXTENSIONS:
                        files_to_process.append(matched)

        # Check if it's a directory
        elif os.path.isdir(path_pattern):
            if recursive:
                # Walk through all subdirectories
                for root, dirs, files in os.walk(path_pattern):
                    for file in files:
                        ext = os.path.splitext(file)[1]
                        if ext in VALID_EXTENSIONS:
                            files_to_process.append(os.path.join(root, file))
            else:
                # Only process files in the immediate directory
                for item in os.listdir(path_pattern):
                    full_path = os.path.join(path_pattern, item)
                    if os.path.isfile(full_path):
                        ext = os.path.splitext(item)[1]
                        if ext in VALID_EXTENSIONS:
                            files_to_process.append(full_path)

        # It's a regular file
        elif os.path.isfile(path_pattern):
            files_to_process.append(path_pattern)
        else:
            print(f"Warning: Path not found: {path_pattern}")

    return files_to_process


# Build the header based on flags
def build_header():
    # Determine engine name and URL based on primary flag
    if use_blazium:
        engine_name = "BLAZIUM ENGINE"
        engine_url = "https://blazium.app"
    else:
        engine_name = "GODOT ENGINE"
        engine_url = "https://godotengine.org"

    # Pad engine_name and engine_url to maintain alignment
    # The line width is 76 chars total: "/*" (2) + "  " (2) + content (68) + "  " (2) + "*/" (2)
    # Total content area for text = 68 chars (between "/*  " and "  */")

    # Calculate padding for engine_name (centered)
    engine_name_padding = 68 - len(engine_name)
    left_padding_name = engine_name_padding // 2
    right_padding_name = engine_name_padding - left_padding_name
    engine_name_padded = " " * left_padding_name + engine_name + " " * right_padding_name

    # Calculate padding for engine_url (centered)
    engine_url_padding = 68 - len(engine_url)
    left_padding_url = engine_url_padding // 2
    right_padding_url = engine_url_padding - left_padding_url
    engine_url_padded = " " * left_padding_url + engine_url + " " * right_padding_url

    # Build header top
    header = header_top.format(engine_name=engine_name_padded, engine_url=engine_url_padded)
    header += "\n"

    # Add copyright lines based on flags
    if use_blazium and use_godot:
        # Both flags: add all copyright lines
        header += blazium_copyright + "\n"
        header += godot_copyright + "\n"
        header += juan_ariel_copyright + "\n"
    elif use_blazium:
        # Only Blazium flag
        header += blazium_copyright + "\n"
    elif use_godot:
        # Only Godot flag
        header += godot_copyright + "\n"
        header += juan_ariel_copyright + "\n"

    # Add license text
    header += license_text

    return header


header = build_header()

# Collect all files to process
files_to_process = collect_files(sys.argv[1:], recursive)

if not files_to_process:
    print("No files found to process.")
    sys.exit(0)

print(f"Processing {len(files_to_process)} file(s)...")

# Process files
for f in files_to_process:
    fname = f

    # Handle replacing $filename with actual filename and keep alignment
    fsingle = os.path.basename(fname.strip())
    rep_fl = "$filename"
    rep_fi = fsingle
    len_fl = len(rep_fl)
    len_fi = len(rep_fi)
    # Pad with spaces to keep alignment
    if len_fi < len_fl:
        for x in range(len_fl - len_fi):
            rep_fi += " "
    elif len_fl < len_fi:
        for x in range(len_fi - len_fl):
            rep_fl += " "
    if header.find(rep_fl) != -1:
        text = header.replace(rep_fl, rep_fi)
    else:
        text = header.replace("$filename", fsingle)
    text += "\n"

    # We now have the proper header, so we want to ignore the one in the original file
    # and potentially empty lines and badly formatted lines, while keeping comments that
    # come after the header, and then keep everything non-header unchanged.
    # To do so, we skip empty lines that may be at the top in a first pass.
    # In a second pass, we skip all consecutive comment lines starting with "/*",
    # then we can append the rest (step 2).

    with open(fname.strip(), "r", encoding="utf-8") as fileread:
        line = fileread.readline()
        header_done = False

        while line.strip() == "" and line != "":  # Skip empty lines at the top
            line = fileread.readline()

        if line.find("/**********") == -1:  # Godot/Blazium header starts this way
            # Maybe starting with a non-Godot/Blazium comment, abort header magic
            header_done = True

        while not header_done:  # Handle header now
            if line.find("/*") != 0:  # No more starting with a comment
                header_done = True
                if line.strip() != "":
                    text += line
            line = fileread.readline()

        while line != "":  # Dump everything until EOF
            text += line
            line = fileread.readline()

    # Write
    with open(fname.strip(), "w", encoding="utf-8", newline="\n") as filewrite:
        filewrite.write(text)

    print(f"  Processed: {fname}")

print(f"\nSuccessfully processed {len(files_to_process)} file(s).")
