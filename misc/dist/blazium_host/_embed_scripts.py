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
with open(out, "w", encoding="utf-8", newline="\n") as handle:
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
