#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Header hygiene: every listed header must compile on its own, twice in a row
(include guard), with the test warning flags and -Werror.

    python check_headers.py <junit.xml> <known.txt> <cc> [cc flags / -I...] -- <header>...

known.txt lists headers with a known, reported problem ("path: reason", paths
relative to the repo root). Such a header is reported as skipped while it still
fails, and as a FAILURE once it compiles, so the entry gets removed with the fix
(like a strict xfail).
"""

import os
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET


def main() -> int:
    argv = sys.argv[1:]
    if "--" not in argv or len(argv) < 5:
        sys.exit(__doc__)
    sep = argv.index("--")
    junit_path, known_path, cc = argv[0], argv[1], argv[2]
    flags, headers = argv[3:sep], argv[sep + 1:]
    root = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

    known = {}
    with open(known_path) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                path, _, reason = line.partition(":")
                known[os.path.normpath(path.strip())] = reason.strip()

    ts = ET.Element("testsuite", name="header_hygiene")
    failures = skipped = 0
    t0 = time.monotonic()
    with tempfile.TemporaryDirectory() as tmp:
        probe = os.path.join(tmp, "probe.c")
        for hdr in headers:
            rel = os.path.normpath(os.path.relpath(hdr, root))
            inc = hdr.replace("\\", "/")
            with open(probe, "w") as f:
                f.write(f'#include "{inc}"\n#include "{inc}"\n')
            res = subprocess.run([cc, *flags, "-fsyntax-only", probe],
                                 capture_output=True, text=True)
            ok = res.returncode == 0
            out = (res.stdout + res.stderr).strip()
            tc = ET.SubElement(ts, "testcase", classname="header_hygiene",
                               name=rel.replace(os.sep, "/"))
            if rel in known:
                if ok:
                    failures += 1
                    ET.SubElement(tc, "failure", message="listed in known_header_issues.txt "
                                  "but compiles now: remove the entry").text = known[rel]
                    print(f"FIXED? {rel} compiles now; remove it from {os.path.basename(known_path)}")
                else:
                    skipped += 1
                    ET.SubElement(tc, "skipped", message=f"KNOWN BUG: {known[rel]}")
                    print(f"KNOWN  {rel}: {known[rel]}")
            elif ok:
                print(f"OK     {rel}")
            else:
                failures += 1
                ET.SubElement(tc, "failure", message="does not compile on its own").text = out
                print(f"FAIL   {rel}\n{out}")

    ts.set("tests", str(len(headers)))
    ts.set("failures", str(failures))
    ts.set("errors", "0")
    ts.set("skipped", str(skipped))
    ts.set("time", f"{time.monotonic() - t0:.3f}")
    root_el = ET.Element("testsuites")
    root_el.append(ts)
    ET.indent(root_el)
    ET.ElementTree(root_el).write(junit_path, encoding="utf-8", xml_declaration=True)
    print(f"{len(headers)} headers, {failures} failures, {skipped} known issues")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
