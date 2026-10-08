# SPDX-License-Identifier: MIT
"""Replace one top-level section of _TEST/vectors/wire.json in place.

The generators (gen_pair_vector.py, gen_fs_vector.py) each own a section. Only
that section's text is replaced (or appended at the end); the other sections
keep their hand formatting and their order.
"""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
WIRE = os.path.join(HERE, "..", "vectors", "wire.json")


def replace_section(name: str, value, path: str = WIRE) -> None:
    with open(path, encoding="utf-8") as f:
        text = f.read()
    body = json.dumps(value, indent=2)
    body = "\n".join("  " + line for line in body.splitlines()).lstrip()

    key = f'\n  "{name}": '
    pos = text.find(key)
    if pos >= 0:
        # The value runs from after the key to where a JSON decoder stops
        start = pos + len(key)
        _, end = json.JSONDecoder().raw_decode(text, start)
        text = text[:start] + body + text[end:]
    else:
        assert text.rstrip().endswith("}"), "wire.json must end with its closing brace"
        head = text.rstrip()[:-1].rstrip()
        text = head + "," + key + body + "\n}\n"

    json.loads(text)                          # still valid JSON
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
