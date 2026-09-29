#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run one Unity test executable and write its results as JUnit XML.

    python run_unity.py <junit.xml> <suite name> <exe> [args...]

Unity prints one line per test case ("file:line:name:PASS|FAIL: msg|IGNORE: msg").
Each becomes a <testcase>; the lines printed before it are kept as its
system-out. A crash or sanitizer abort (no Unity summary, or a non-zero exit
without a FAIL) is reported as an extra failed test case, so it shows up in the
report instead of silently losing the remaining tests. The exit code is the
executable's, or 1 if the output shows a failure.
"""

import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

RESULT = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<name>[A-Za-z_]\w*):"
                    r"(?P<status>PASS|FAIL|IGNORE)(?::\s?(?P<msg>.*))?$")
SUMMARY = re.compile(r"^(\d+) Tests (\d+) Failures (\d+) Ignored")


def main() -> int:
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    junit_path, suite, cmd = sys.argv[1], sys.argv[2], sys.argv[3:]

    t0 = time.monotonic()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace")
    lines = []
    for line in proc.stdout:
        sys.stdout.write(line)
        lines.append(line.rstrip("\n"))
    rc = proc.wait()
    elapsed = time.monotonic() - t0

    cases, pending, summary = [], [], None
    for line in lines:
        m = RESULT.match(line)
        if m:
            cases.append((m, pending))
            pending = []
            continue
        s = SUMMARY.match(line)
        if s:
            summary = s
        elif line and not re.fullmatch(r"-+|OK|FAIL", line):
            pending.append(line)

    ts = ET.Element("testsuite", name=suite)
    failures = skipped = 0
    for m, out in cases:
        tc = ET.SubElement(ts, "testcase", classname=suite, name=m["name"],
                           file=m["file"], line=m["line"])
        if m["status"] == "FAIL":
            failures += 1
            ET.SubElement(tc, "failure", message=m["msg"] or "failed").text = \
                f"{m['file']}:{m['line']}: {m['msg'] or ''}"
        elif m["status"] == "IGNORE":
            skipped += 1
            ET.SubElement(tc, "skipped", message=m["msg"] or "ignored")
        if out:
            ET.SubElement(tc, "system-out").text = "\n".join(out)

    crashed = summary is None or (rc != 0 and failures == 0)
    if crashed:
        failures += 1
        tc = ET.SubElement(ts, "testcase", classname=suite, name="process_exit")
        what = "no Unity summary (crash or abort)" if summary is None else f"exit code {rc}"
        ET.SubElement(tc, "failure", message=what).text = "\n".join(pending[-60:] or lines[-60:])

    ts.set("tests", str(len(cases) + (1 if crashed else 0)))
    ts.set("failures", str(failures))
    ts.set("errors", "0")
    ts.set("skipped", str(skipped))
    ts.set("time", f"{elapsed:.3f}")
    root = ET.Element("testsuites")
    root.append(ts)
    ET.indent(root)
    ET.ElementTree(root).write(junit_path, encoding="utf-8", xml_declaration=True)

    return rc if rc != 0 else (1 if failures else 0)


if __name__ == "__main__":
    sys.exit(main())
