#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Merge the JUnit XML files of all test tiers into one report.

    python report.py <junit dir> [--md summary.md] [--html report.html] [--title T]

Writes a Markdown summary (also used as the GitHub Actions job summary) and a
self-contained HTML page: totals per suite, then every failure with its
message, then the known bugs (xfail / skipped with "KNOWN BUG"). Exit code 0;
the test runners own the pass/fail decision.
"""

import argparse
import glob
import html
import os
import sys
import xml.etree.ElementTree as ET


def load(junit_dir):
    suites = []
    for path in sorted(glob.glob(os.path.join(junit_dir, "*.xml"))):
        root = ET.parse(path).getroot()
        for ts in ([root] if root.tag == "testsuite" else root.iter("testsuite")):
            cases = []
            for tc in ts.iter("testcase"):
                status, msg, detail = "passed", "", ""
                for tag in ("failure", "error"):
                    el = tc.find(tag)
                    if el is not None:
                        status, msg, detail = "failed", el.get("message", ""), (el.text or "")
                sk = tc.find("skipped")
                if status == "passed" and sk is not None:
                    msg = sk.get("message", "") or (sk.text or "")
                    status = "known bug" if ("KNOWN BUG" in msg or "xfail" in sk.get("type", "")) \
                        else "skipped"
                cases.append({"name": tc.get("name", "?"), "cls": tc.get("classname", ""),
                              "status": status, "msg": msg.strip(), "detail": detail.strip(),
                              "file": tc.get("file", ""), "line": tc.get("line", "")})
            if cases:
                suites.append({"name": ts.get("name") or os.path.basename(path), "cases": cases})
    return suites


def count(cases, status):
    return sum(1 for c in cases if c["status"] == status)


def to_markdown(suites, title):
    allc = [c for s in suites for c in s["cases"]]
    failed = count(allc, "failed")
    icon = "❌" if failed else "✅"
    o = [f"## {icon} {title}", "",
         f"**{len(allc)} tests: {count(allc, 'passed')} passed, {failed} failed, "
         f"{count(allc, 'known bug')} known bugs, {count(allc, 'skipped')} skipped**", "",
         "| Suite | Tests | Passed | Failed | Known bugs | Skipped |", "|---|---:|---:|---:|---:|---:|"]
    for s in suites:
        c = s["cases"]
        mark = " ❌" if count(c, "failed") else ""
        o.append(f"| {s['name']}{mark} | {len(c)} | {count(c, 'passed')} | {count(c, 'failed')} | "
                 f"{count(c, 'known bug')} | {count(c, 'skipped')} |")
    fails = [(s, c) for s in suites for c in s["cases"] if c["status"] == "failed"]
    if fails:
        o += ["", "### Failures", ""]
        for s, c in fails:
            where = f" ({c['file']}:{c['line']})" if c["file"] else ""
            o.append(f"- **{s['name']} › {c['name']}**{where}: {c['msg'] or 'failed'}")
    known = {}                                   # message -> test names (parametrized ones share it)
    for s in suites:
        for c in s["cases"]:
            if c["status"] == "known bug":
                known.setdefault(c["msg"].replace("KNOWN BUG: ", ""), []).append(f"{s['name']} › {c['name']}")
    if known:
        o += ["", "### Known bugs (tests that document an open defect)", ""]
        for msg, names in known.items():
            o.append(f"- **{names[0]}**{f' (+{len(names) - 1} more)' if len(names) > 1 else ''}: {msg}")
    return "\n".join(o) + "\n"


def to_html(suites, title):
    allc = [c for s in suites for c in s["cases"]]
    failed = count(allc, "failed")
    colour = {"passed": "#1a7f37", "failed": "#cf222e", "known bug": "#9a6700", "skipped": "#6e7781"}
    rows = []
    for s in suites:
        c = s["cases"]
        rows.append(f"<details{' open' if count(c, 'failed') else ''}><summary><b>{html.escape(s['name'])}</b>"
                    f" — {len(c)} tests, {count(c, 'failed')} failed, {count(c, 'known bug')} known bugs"
                    f"</summary><table>")
        for t in c:
            info = html.escape(t["msg"]) + (f"<pre>{html.escape(t['detail'])}</pre>" if t["detail"] else "")
            rows.append(f"<tr><td style='color:{colour[t['status']]}'>{t['status']}</td>"
                        f"<td>{html.escape(t['name'])}</td><td>{info}</td></tr>")
        rows.append("</table></details>")
    return f"""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(title)}</title>
<style>
:root {{ color-scheme: light dark; }}
body {{ font: 14px/1.45 system-ui, sans-serif; margin: 16px; max-width: 1100px; }}
table {{ border-collapse: collapse; width: 100%; margin: 6px 0 14px; }}
td {{ border-top: 1px solid #8884; padding: 3px 8px; vertical-align: top; }}
td:first-child {{ white-space: nowrap; font-weight: 600; }}
pre {{ white-space: pre-wrap; margin: 4px 0; font-size: 12px; }}
summary {{ cursor: pointer; padding: 4px 0; }}
.banner {{ padding: 10px 14px; border-radius: 6px; color: #fff;
          background: {'#cf222e' if failed else '#1a7f37'}; }}
</style></head><body>
<h1>{html.escape(title)}</h1>
<p class="banner">{len(allc)} tests: {count(allc, 'passed')} passed, {failed} failed,
{count(allc, 'known bug')} known bugs, {count(allc, 'skipped')} skipped</p>
{''.join(rows)}
</body></html>
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("junit_dir")
    ap.add_argument("--md")
    ap.add_argument("--html")
    ap.add_argument("--title", default="Test report")
    a = ap.parse_args()
    suites = load(a.junit_dir)
    md = to_markdown(suites, a.title)
    if a.md:
        with open(a.md, "w", encoding="utf-8") as f:
            f.write(md)
    if a.html:
        with open(a.html, "w", encoding="utf-8") as f:
            f.write(to_html(suites, a.title))
    # Windows consoles are often cp1252: never fail on the status emoji
    sys.stdout.reconfigure(errors="replace")
    print(md)


if __name__ == "__main__":
    main()
