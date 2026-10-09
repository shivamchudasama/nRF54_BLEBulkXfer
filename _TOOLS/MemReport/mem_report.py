#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Per-module FLASH and RAM usage of a Zephyr build, read from the linker map.

The linker's own summary (--print-memory-usage) gives one figure per memory
region for the whole image. This tool splits it by module of this repository:
every input section in the map names the object file it came from, and each
object file is traced back to its source file under the scanned folders
(_ASW, _LIB by default). A module is one sub-folder of a scanned folder
(_ASW/_BLE, _LIB/SETU, ...); a source file directly in a scanned folder
(_ASW/main.c) is a module of its own. Everything else (Zephyr, NCS, libc,
alignment padding) is the "Other" row, so the rows add up to the total.

Counting rules:
  - an output section counts towards the region that holds its run-time
    address, and also towards the region of its load address when it has one
    (initialised data: RAM at run time, copied from FLASH at boot);
  - sections that are not loaded (.debug_*, .comment, .tbss, ...) are left
    out. A region's total runs from its start to the end of its last
    section, as in the linker's summary; the gaps between sections count as
    Other;
  - the linker merges strings and constants (.rodata.str1.1, .cst4, ...) of
    all objects into one pool, dropping duplicates, and the map lists the
    whole pool under the first of those inputs. The pool is shared out among
    the objects by their size before merging, so a module's share of it is
    approximate (within the bytes duplicates saved).

Run by the firmware build after the final link (see the root CMakeLists.txt):

    mem_report.py --map build/.../zephyr/zephyr.map --root . --dirs _ASW _LIB
                  [--out zephyr/mem_report.txt] [--all]
"""

import argparse
import os
import re
import sys
from collections import OrderedDict

# Output sections that occupy no target memory.
NOT_LOADED = re.compile(
    r"^\.?(debug|comment|ARM\.attributes|stab|symtab|strtab|shstrtab|note"
    r"|gnu\.attributes|gnu_debug|tbss)")
# Input sections the linker merges (duplicates dropped): sizes in the map are
# the sizes before merging.
MERGEABLE = re.compile(r"\.(str\d+\.\d+|cst\d+)$")

HEX = r"0x([0-9a-fA-F]+)"
RE_REGION = re.compile(rf"^(\S+)\s+{HEX}\s+{HEX}")
# Output section: "name addr size [load address lma]", or "name" alone with
# "addr size [load address lma]" on the next line when the name is long.
RE_OUT_NAME = re.compile(r"^([^\s*]\S*)\s*$")
RE_OUT_FULL = re.compile(rf"^([^\s*]\S*)\s+{HEX}\s+{HEX}(?:\s+load address {HEX})?\s*$")
RE_OUT_CONT = re.compile(rf"^\s+{HEX}\s+{HEX}(?:\s+load address {HEX})?\s*$")
# Input section: " name addr size object", or " name" alone with
# "addr size object" on the next line.
RE_IN_NAME = re.compile(r"^ ([^\s*]\S*)\s*$")
RE_IN_FULL = re.compile(rf"^ ([^\s*]\S*)\s+{HEX}\s+{HEX}\s+([^\s0].*?)\s*$")
RE_IN_CONT = re.compile(rf"^\s+{HEX}\s+{HEX}\s+([^\s0].*?)\s*$")
# "app/libapp.a(Ble.c.obj)" -> "Ble.c.obj"
RE_ARCHIVE_MEMBER = re.compile(r"\(([^()]+)\)\s*$")

OTHER = "Other (Zephyr, NCS, libc)"


class Region:
    def __init__(self, name, origin, length):
        self.name, self.origin, self.length = name, origin, length

    def holds(self, addr):
        return self.origin <= addr < self.origin + self.length


class OutputSection:
    def __init__(self, name, vma, size, lma):
        self.name, self.vma, self.size, self.lma = name, vma, size, lma
        self.inputs = []                     # (input section, size, object)


def parse_map(text):
    """Return (regions, output_sections) from a GNU ld map file.

    Only loaded output sections are returned, each with the input sections
    the map lists inside it. lma is None when the section has no separate load
    address.
    """
    lines = text.splitlines()
    regions = []
    i = 0
    while i < len(lines) and not lines[i].startswith("Memory Configuration"):
        i += 1
    i += 1
    while i < len(lines) and not lines[i].startswith("Linker script and memory map"):
        m = RE_REGION.match(lines[i])
        if m and m.group(1) not in ("Name", "*default*"):
            regions.append(Region(m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
        i += 1

    outputs = []
    cur = None                # current output section, None while not loaded
    pending_out = None        # output section name waiting for its address line
    pending_in = None         # input section name waiting for its address line

    def open_section(name, vma, size, lma):
        if NOT_LOADED.match(name):
            return None
        sec = OutputSection(name, int(vma, 16), int(size, 16),
                            int(lma, 16) if lma else None)
        outputs.append(sec)
        return sec

    def add_input(name, size, obj):
        if cur is not None and size:
            cur.inputs.append((name, size, obj))

    for line in lines[i + 1:]:
        if line.startswith("OUTPUT("):
            break
        if pending_out is not None:
            name, pending_out = pending_out, None
            m = RE_OUT_CONT.match(line)
            if m:
                cur = open_section(name, *m.groups())
                continue
        if pending_in is not None:
            name, pending_in = pending_in, None
            m = RE_IN_CONT.match(line)
            if m:
                add_input(name, int(m.group(2), 16), m.group(3))
                continue
        if not line.strip():
            continue
        if not line[0].isspace():
            m = RE_OUT_FULL.match(line)
            if m:
                cur = open_section(*m.groups())
                continue
            m = RE_OUT_NAME.match(line)
            if m:
                pending_out = m.group(1)
            continue                         # LOAD, START GROUP, ...
        m = RE_IN_FULL.match(line)
        if m:
            add_input(m.group(1), int(m.group(3), 16), m.group(4))
            continue
        m = RE_IN_NAME.match(line)
        if m:
            pending_in = m.group(1)
    return regions, outputs


def object_name(obj):
    """Base name of the object file a map line names."""
    m = RE_ARCHIVE_MEMBER.search(obj)
    if m:
        obj = m.group(1)
    return os.path.basename(obj.replace("\\", "/"))


def module_index(root, dirs):
    """Map object base names ("Ble.c.obj") to modules ("_ASW/_BLE").

    Returns (index, modules, clashes). modules keeps the scan order (sorted
    paths). clashes lists object names built from two modules, which the map
    cannot tell apart; they are left out of the index and count as OTHER.
    """
    index, modules, clashes = {}, [], set()
    for d in dirs:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames.sort()
            parts = os.path.relpath(dirpath, root).replace("\\", "/").split("/")
            for f in sorted(filenames):
                if not f.endswith((".c", ".cpp", ".cc", ".S", ".s")):
                    continue
                mod = "/".join(parts[:2]) if len(parts) > 1 else f"{d}/{f}"
                key = f + ".obj"
                if key in index and index[key] != mod:
                    clashes.add(key)
                index[key] = mod
                if mod not in modules:
                    modules.append(mod)
    for key in clashes:
        del index[key]
    return index, modules, sorted(clashes)


def _input_shares(inputs):
    """(object, bytes) for each input section of one output section.

    ld merges the mergeable inputs of one kind (.str1.1, .cst4, ...) into one
    pool, which the map lists under the first of them at the pool's size; the
    others keep their size before merging. The pool is shared among the
    others in proportion to that size, scaled down when duplicates made it
    smaller than their sum, and the first input gets what remains.
    """
    pools = {}                               # kind -> [pool size, holder, others]
    shares = []
    for name, size, obj in inputs:
        m = MERGEABLE.search(name)
        if not m:
            shares.append((obj, float(size)))
        elif m.group(1) not in pools:
            pools[m.group(1)] = [size, obj, []]
        else:
            pools[m.group(1)][2].append((obj, size))
    for pool, holder, others in pools.values():
        listed = sum(size for _o, size in others)
        scale = min(1.0, pool / listed) if listed else 0.0
        shares.extend((obj, size * scale) for obj, size in others)
        shares.append((holder, pool - listed * scale))
    return shares


def usage(regions, outputs, index, modules):
    """Bytes per module and region: OrderedDict module -> {region: bytes},
    OTHER last. The rows of a region add up to what the linker's summary
    reports: from the region's start to the end of its last section, so
    alignment gaps between sections count as OTHER."""
    rows = OrderedDict((m, {r.name: 0 for r in regions}) for m in modules + [OTHER])
    end = {r.name: r.origin for r in regions}
    for sec in outputs:
        hit = []
        for addr in (sec.vma, sec.lma):
            for r in regions:
                if addr is not None and r.holds(addr) and r not in hit:
                    hit.append(r)
                    end[r.name] = max(end[r.name], addr + sec.size)
        if not hit or not sec.size:
            continue

        share = {}
        for obj, size in _input_shares(sec.inputs):
            mod = index.get(object_name(obj))
            if mod is not None:
                share[mod] = share.get(mod, 0.0) + size
        claimed = 0
        for mod, b in share.items():
            b = min(int(round(b)), sec.size - claimed)
            claimed += b
            for r in hit:
                rows[mod][r.name] += b
        for r in hit:
            rows[OTHER][r.name] += sec.size - claimed
    for r in regions:
        counted = sum(row[r.name] for row in rows.values())
        rows[OTHER][r.name] += max(0, end[r.name] - r.origin - counted)
    return rows


def format_report(regions, rows, dirs, show_empty=False):
    """The table printed after the build: one row per module, a subtotal per
    scanned folder, then OTHER and the total, each with its share of the
    region size."""
    shown = [r for r in regions if any(row[r.name] for row in rows.values())]
    width = max([len(m) + 2 for m in rows] + [len("Module")]) + 2

    head = f"{'Module':<{width}}" + "".join(
        f"{r.name + ' (B)':>14}{'%':>9}" for r in shown)
    rule = "-" * len(head)

    def line(label, vals):
        s = f"{label:<{width}}"
        for r in shown:
            pct = 100.0 * vals[r.name] / r.length if r.length else 0.0
            s += f"{vals[r.name]:>14}{pct:>8.2f}%"
        return s

    def total(names):
        return {r.name: sum(rows[n][r.name] for n in names) for r in shown}

    out = ["Memory usage per module", head, rule]
    for d in dirs:
        members = [m for m in rows if m.startswith(d + "/")]
        if not members:
            continue
        for m in members:
            if show_empty or any(rows[m][r.name] for r in shown):
                out.append(line("  " + m, rows[m]))
        out.append(line(d + " total", total(members)))
    out.append(line(OTHER, rows[OTHER]))
    out.append(rule)
    out.append(line("Total", total(list(rows))))
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.strip().split("\n\n")[0])
    ap.add_argument("--map", required=True, help="linker map file (zephyr.map)")
    ap.add_argument("--root", default=".", help="repository root")
    ap.add_argument("--dirs", nargs="+", default=["_ASW", "_LIB"],
                    help="folders whose sub-folders are modules")
    ap.add_argument("--all", action="store_true",
                    help="also list modules that use no memory")
    ap.add_argument("--out", help="also write the report to this file")
    args = ap.parse_args(argv)

    try:
        with open(args.map, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as e:
        print(f"mem_report: {e}", file=sys.stderr)
        return 1
    regions, outputs = parse_map(text)
    if not regions:
        print(f"mem_report: no memory regions in {args.map}", file=sys.stderr)
        return 1
    index, modules, clashes = module_index(args.root, args.dirs)
    report = format_report(regions, usage(regions, outputs, index, modules),
                           args.dirs, args.all)
    print(report)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(report + "\n")
    for key in clashes:
        print(f"mem_report: {key} is built from two modules; counted as Other",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
