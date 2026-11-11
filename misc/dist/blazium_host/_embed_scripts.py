import os

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sources = [
    os.path.join(root, "misc", "dist", "html", "third_party_services"),
    os.path.join(root, "misc", "dist", "blazium_host"),
]
names = []
bodies = []
for folder in sources:
    for filename in sorted(os.listdir(folder)):
        if not filename.endswith(".js"):
            continue
        if filename.startswith("_"):
            continue
        path = os.path.join(folder, filename)
        if not os.path.isfile(path):
            continue
        with open(path, "r", encoding="utf-8", newline="\n") as handle:
            text = handle.read().replace("\r\n", "\n")
        if "BLZ_EMBED_9f3a" in text:
            raise SystemExit(f"delimiter collision in {filename}")
        names.append(filename)
        bodies.append(text)

out = os.path.join(root, "platform", "web", "export", "portal_scripts.inc")
header = """\
/**************************************************************************/
/*  portal_scripts.inc                                                    */
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
with open(out, "w", encoding="utf-8", newline="\n") as handle:
    handle.write(header)
    handle.write("/* Embedded portal and host scripts. Generated from misc/dist. */\n")
    handle.write(f"static const int portal_script_count = {len(names)};\n")
    handle.write("static const char *portal_script_names[] = {\n")
    for name in names:
        handle.write(f"\t\"{name}\",\n")
    handle.write("};\n")
    handle.write("static const char *portal_script_bodies[] = {\n")
    for body in bodies:
        handle.write("\tR\"BLZ_EMBED_9f3a(")
        handle.write(body)
        if not body.endswith("\n"):
            handle.write("\n")
        handle.write(")BLZ_EMBED_9f3a\",\n")
    handle.write("};\n")
print(f"wrote {len(names)} scripts")
