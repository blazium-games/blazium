"""Adapt dumped Godot 4.8 API so the blazium-dev blazium-cpp generator can build."""

import json
from pathlib import Path

CORRECT_TYPE_OLD = """    if meta is not None:
        if "int" in meta:
            return f"{meta}_t"
        elif "char" in meta:
            return f"{meta}_t"
        else:
            return meta
"""

CORRECT_TYPE_NEW = """    if meta is not None:
        if meta in ["int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64"]:
            return f"{meta}_t"
        elif meta in ["float", "double"]:
            return meta
        elif meta in ["char16", "char32"]:
            return f"{meta}_t"
"""

GLOBAL_CONSTANTS_OLD = '    if len(api["global_constants"]) > 0:\n        for constant in api["global_constants"]:'

GLOBAL_CONSTANTS_NEW = """    # Skip cstdint-overlapping integer limits (Godot 4.8+ @GlobalScope).
    limit_constants = {
        "UINT8_MAX",
        "UINT16_MAX",
        "UINT32_MAX",
        "INT8_MIN",
        "INT8_MAX",
        "INT16_MIN",
        "INT16_MAX",
        "INT32_MIN",
        "INT32_MAX",
        "INT64_MIN",
        "INT64_MAX",
    }
    global_constants = [c for c in api["global_constants"] if c["name"] not in limit_constants]
    if len(global_constants) > 0:
        for constant in global_constants:"""


def _adapt_api_node(node: object) -> tuple[int, int]:
    required_removed = 0
    dictionaries_rewritten = 0
    if isinstance(node, dict):
        if node.get("meta") == "required":
            del node["meta"]
            required_removed += 1
        for key in ("type", "return_type"):
            value = node.get(key)
            if isinstance(value, str) and value.startswith("typeddictionary::"):
                node[key] = "Dictionary"
                dictionaries_rewritten += 1
        for value in node.values():
            removed, rewritten = _adapt_api_node(value)
            required_removed += removed
            dictionaries_rewritten += rewritten
    elif isinstance(node, list):
        for value in node:
            removed, rewritten = _adapt_api_node(value)
            required_removed += removed
            dictionaries_rewritten += rewritten
    return required_removed, dictionaries_rewritten


def adapt_extension_api() -> None:
    path = Path("extension_api.json")
    if not path.is_file():
        raise SystemExit("extension_api.json not found")
    api = json.loads(path.read_text(encoding="utf-8"))
    required_removed, dictionaries_rewritten = _adapt_api_node(api)
    with path.open("w", encoding="utf-8", newline="\n") as out:
        json.dump(api, out, ensure_ascii=False)
        out.write("\n")
    print(f"Removed {required_removed} required meta entries from extension_api.json")
    print(f"Rewrote {dictionaries_rewritten} typeddictionary types to Dictionary")


def patch_binding_generator() -> None:
    path = Path("blazium-cpp/binding_generator.py")
    content = path.read_text(encoding="utf-8")
    original = content

    if "limit_constants" in content:
        print("binding_generator.py already filters integer limit constants")
    elif GLOBAL_CONSTANTS_OLD not in content:
        raise SystemExit("binding_generator.py global_constants loop not found")
    else:
        content = content.replace(GLOBAL_CONSTANTS_OLD, GLOBAL_CONSTANTS_NEW, 1)
        print("Patched binding_generator.py to skip cstdint limit constants")

    if CORRECT_TYPE_NEW in content:
        print("binding_generator.py already ignores unknown type meta")
    elif CORRECT_TYPE_OLD not in content:
        raise SystemExit("binding_generator.py correct_type meta fallback not found")
    else:
        content = content.replace(CORRECT_TYPE_OLD, CORRECT_TYPE_NEW, 1)
        print("Patched binding_generator.py to ignore Godot 4.8 required type meta")

    typed_array_old = (
        '    if type_name.startswith("typedarray::"):\n'
        '        return type_name.replace("typedarray::", "TypedArray<") + ">"\n'
    )
    typed_array_new = (
        typed_array_old + '    if type_name.startswith("typeddictionary::"):\n' + '        return "Dictionary"\n'
    )
    if 'startswith("typeddictionary::")' in content:
        print("binding_generator.py already maps typeddictionary to Dictionary")
    elif typed_array_old not in content:
        raise SystemExit("binding_generator.py typedarray correct_type branch not found")
    else:
        content = content.replace(typed_array_old, typed_array_new, 1)
        print("Patched binding_generator.py to map typeddictionary to Dictionary")

    if content != original:
        with path.open("w", encoding="utf-8", newline="\n") as out:
            out.write(content)


adapt_extension_api()
patch_binding_generator()
