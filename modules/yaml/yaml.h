#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

class YAML : public RefCounted {
    GDCLASS(YAML, RefCounted);

protected:
    static void _bind_methods();

public:
    // Parse a YAML string → Variant (Dictionary / Array / primitive)
    static Variant parse_string(const String &p_yaml);

    // Convert a Variant → YAML string
    static String stringify(const Variant &p_value, int p_indent = 2);

    // Convenience: parse a file
    static Variant parse_file(const String &p_path);
};