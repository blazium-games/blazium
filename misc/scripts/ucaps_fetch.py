#!/usr/bin/env python3

# Script used to dump case mappings from
# the Unicode Character Database to the `ucaps_data.h` file.
# NOTE: This script is deliberately not integrated into the build system;
# you should run it manually whenever you want to update the data.

import os
import random
import sys
from typing import Dict, Final, List, Tuple
from urllib.request import urlopen

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(os.path.dirname(__file__), "../../"))

from methods import generate_copyright_header

URL: Final[str] = "https://www.unicode.org/Public/16.0.0/ucd/UnicodeData.txt"


lower_to_upper: List[Tuple[str, str]] = []
upper_to_lower: List[Tuple[str, str]] = []
local_rand = None


def parse_unicode_data() -> None:
    lines: List[str] = [line.decode("utf-8") for line in urlopen(URL)]

    for line in lines:
        split_line: List[str] = line.split(";")

        code_value: str = split_line[0].strip()
        uppercase_mapping: str = split_line[12].strip()
        lowercase_mapping: str = split_line[13].strip()

        if uppercase_mapping:
            lower_to_upper.append((f"0x{code_value}", f"0x{uppercase_mapping}"))
        if lowercase_mapping:
            upper_to_lower.append((f"0x{code_value}", f"0x{lowercase_mapping}"))


def make_cap_table(table_name: str, len_name: str, table: List[Tuple[str, str]]) -> str:
    result: str = f"static constexpr int {table_name}[{len_name}][2] = {{\n"

    for first, second in table:
        result += f"\t{{ {first}, {second} }},\n"

    result += "};\n\n"

    return result


def get_next_rand_uint32() -> int:
    global local_rand

    # Init random with our seed and make sure to use version 2
    if local_rand is None:
        local_rand = random.Random()
        seed = 25232848942
        local_rand.seed(seed, version=2)

    # Get random number with random.random(), as it is guaranteed to return the same values in future python versions
    rand: int = int(local_rand.random() * (2**32))

    # Always use an odd multiplier because it is coprime to any power of 2 sized table, and won't fill bottom bits with zeros
    odd_rand: int = rand | 1

    return odd_rand


def find_perfect_hash_multiplier(table: List[Tuple[str, str]], bits: int) -> int:
    keys = list([int(entry[0], 16) for entry in table])
    shift: int = 32 - bits

    is_perfect = False
    while not is_perfect:
        mult = get_next_rand_uint32()
        seen = set()
        is_collision = False
        for key in keys:
            idx = ((key * mult) & 0xFFFFFFFF) >> shift
            if idx in seen:
                is_collision = True
                break
            seen.add(idx)

        if not is_collision:
            is_perfect = True
            return mult

    return 0


def generate_perfect_hash_multipliers() -> Dict[str, int]:
    bits: int = 13
    table_len: int = 8192

    caps_multiplier: int = find_perfect_hash_multiplier(lower_to_upper, bits)
    reverse_caps_multiplier: int = find_perfect_hash_multiplier(upper_to_lower, bits)

    # Make sure multipliers are valid and actually perfect
    verify_perfect_hash(bits, caps_multiplier, lower_to_upper)
    verify_perfect_hash(bits, reverse_caps_multiplier, upper_to_lower)

    data: Dict[str, int] = {
        "bits": bits,
        "table_len": table_len,
        "caps_multiplier": caps_multiplier,
        "reverse_caps_multiplier": reverse_caps_multiplier,
    }
    return data


def verify_perfect_hash(bits, multiplier: int, table: List[Tuple[str, str]]) -> None:
    if multiplier <= 0:
        raise RuntimeError("Invalid hash multiplier generated: {0}".format(multiplier))

    keys = list([int(entry[0], 16) for entry in table])
    shift = 32 - bits
    used_keys = set()
    for key in keys:
        idx = ((key * multiplier) & 0xFFFFFFFF) >> shift
        if idx in used_keys:
            raise RuntimeError("Invalid hash multiplier {0}. It generated a collision".format(multiplier))
        used_keys.add(idx)


def generate_ucaps_fetch(data: Dict[str, int]) -> None:
    source: str = generate_copyright_header("ucaps_data.h")

    source += f"""
#pragma once

// This file was generated using the `misc/scripts/ucaps_fetch.py` script.

namespace UcapsInternal {{
static constexpr uint32_t BITS = {data["bits"]};
static constexpr uint32_t TABLE_LEN = {data["table_len"]};
static constexpr uint32_t CAPS_MULTIPLIER = {data["caps_multiplier"]};
static constexpr uint32_t REVERSE_CAPS_MULTIPLIER = {data["reverse_caps_multiplier"]};

#define LTU_LEN {len(lower_to_upper)}
#define UTL_LEN {len(upper_to_lower)}\n\n"""

    source += make_cap_table("caps_table", "LTU_LEN", lower_to_upper)
    source += make_cap_table("reverse_caps_table", "UTL_LEN", upper_to_lower)

    source += """
} // namespace UcapsInternal
"""

    ucaps_path: str = os.path.join(os.path.dirname(__file__), "../../core/string/ucaps_data.h")
    with open(ucaps_path, "w", newline="\n") as f:
        f.write(source)

    print("`ucaps_data.h` generated successfully.")


if __name__ == "__main__":
    parse_unicode_data()
    data = generate_perfect_hash_multipliers()
    generate_ucaps_fetch(data)
