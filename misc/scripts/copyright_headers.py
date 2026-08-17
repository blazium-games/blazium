#!/usr/bin/env python3

import os
import sys

godot_header = """\
/**************************************************************************/
/*  $filename                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

blazium_header = """\
/**************************************************************************/
/*  $filename                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
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

use_blazium = "--blazium" in sys.argv
use_godot = "--godot" in sys.argv
if use_blazium:
    sys.argv.remove("--blazium")
if use_godot:
    sys.argv.remove("--godot")

if not use_blazium and not use_godot:
    print("Error: At least one of --blazium or --godot flag must be specified.")
    print("Usage: python copyright_headers.py [--blazium] [--godot] <file1> [file2] ...")
    print("  --blazium: Use the Blazium Engine copyright header")
    print("  --godot: Use the Godot Engine copyright header")
    sys.exit(1)

header = blazium_header if use_blazium else godot_header

if len(sys.argv) < 2:
    print("Invalid usage of copyright_headers.py, it should be called with a path to one or multiple files.")
    print("Usage: python copyright_headers.py [--blazium] [--godot] <file1> [file2] ...")
    print("  --blazium: Use the Blazium Engine copyright header")
    print("  --godot: Use the Godot Engine copyright header")
    sys.exit(1)

for f in sys.argv[1:]:
    fname = f

    if use_blazium:
        with open(fname.strip(), "r", encoding="utf-8") as fileread:
            file_content = fileread.read()
        if "BLAZIUM ENGINE" in file_content and "https://blazium.app" in file_content:
            continue

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
            # Maybe starting with a non-engine comment, abort header magic
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
