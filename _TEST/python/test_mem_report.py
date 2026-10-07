# SPDX-License-Identifier: MIT
"""Tests for _TOOLS/MemReport/mem_report.py: map parsing, attribution of
sections to modules, merged-string pools, gaps, and the command line."""

import os
import sys

import pytest

from conftest import REPO

sys.path.insert(0, os.path.join(REPO, "_TOOLS", "MemReport"))

import mem_report as mr  # noqa: E402

# A trimmed GNU ld map with the shapes the real zephyr.map uses: one-line and
# two-line output and input sections, a load address, NOLOAD bss, a gap
# between sections, merged strings, fill, symbols and debug sections.
MAP = """\
Archive member included to satisfy reference by file (symbol)

Memory Configuration

Name             Origin             Length             Attributes
FLASH            0x00000000         0x00001000         xr
RAM              0x20000000         0x00000400         xw
IDT_LIST         0xffff7fff         0x00008000         xw
*default*        0x00000000         0xffffffff

Linker script and memory map

LOAD zephyr/CMakeFiles/zephyr_final.dir/misc/empty_file.c.obj
START GROUP
END GROUP
rom_start       0x00000000       0x40
 .exc_vector_table
                0x00000000       0x40 zephyr/arch/libarch.a(vector_table.S.obj)
                0x00000000                _vector_table

text            0x00000040      0x120
 .text.gv_Ble   0x00000040       0x30 app/libapp.a(Ble.c.obj)
                0x00000040                gv_Ble
 .text.gi_BLKS_Init
                0x00000070       0x50 app/libapp.a(BulkXfer_Server.c.obj)
 .text.main     0x000000c0        0x8 app/libapp.a(main.c.obj)
 *fill*         0x000000c8        0x8
 .text.k_sleep  0x000000d0       0x90 zephyr/kernel/libkernel.a(sched.c.obj)

tbss            0x00000160        0x8
 .tbss.errno    0x00000160        0x8 zephyr/libc/libc.a(errno.c.obj)

rodata          0x00000170       0x60
 .rodata.gst_Caps
                0x00000170       0x10 app/libapp.a(Ble.c.obj)
 .rodata.sv_Disconnected.str1.1
                0x00000180       0x40 app/libapp.a(Ble.c.obj)
 .rodata.gi_BLKS_Init.str1.1
                0x000001c0       0x20 app/libapp.a(BulkXfer_Server.c.obj)
 .rodata.str1.1
                0x000001e0       0x40 zephyr/kernel/libkernel.a(sched.c.obj)
 .rodata.k_cfg  0x000001c0       0x10 zephyr/kernel/libkernel.a(sched.c.obj)

a_very_long_output_section_name
                0x20000000       0x20 load address 0x000001d0
 .data.su8_State
                0x20000000       0x20 app/libapp.a(Ble.c.obj)

bss             0x20000030      0x100
 .bss.su8ar_Buf
                0x20000030       0xc0 app/libapp.a(Ble.c.obj)
 .bss.z_idle    0x200000f0       0x40 zephyr/kernel/libkernel.a(sched.c.obj)
 COMMON         0x20000130        0x0 app/libapp.a(Ble.c.obj)

.comment        0x00000000       0x1f
 .comment       0x00000000       0x1f app/libapp.a(Ble.c.obj)

.debug_info     0x00000000     0x4000
 .debug_info    0x00000000     0x4000 app/libapp.a(Ble.c.obj)
OUTPUT(zephyr/zephyr.elf elf32-littlearm)
LOAD linker stubs
"""


@pytest.fixture
def tree(tmp_path):
    """A repository with _ASW/_BLE, _ASW/main.c and _LIB/BulkXfer."""
    for rel in ("_ASW/main.c", "_ASW/_BLE/Ble.c", "_ASW/_BLE/Ble.h",
                "_ASW/_GENERIX/Helpers.h", "_LIB/BulkXfer/BulkXfer_Server.c",
                "_LIB/BulkXfer/sub/Deep.c"):
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("")
    return tmp_path


@pytest.fixture
def parsed():
    return mr.parse_map(MAP)


def by_name(outputs):
    return {s.name: s for s in outputs}


# ---------------------------------------------------------------- parse_map

def test_regions_skip_header_and_default(parsed):
    regions, _ = parsed
    assert [(r.name, r.origin, r.length) for r in regions] == [
        ("FLASH", 0, 0x1000), ("RAM", 0x20000000, 0x400),
        ("IDT_LIST", 0xffff7fff, 0x8000)]


def test_not_loaded_sections_are_dropped(parsed):
    names = by_name(parsed[1])
    assert "tbss" not in names
    assert ".comment" not in names
    assert ".debug_info" not in names


def test_one_line_and_two_line_sections(parsed):
    s = by_name(parsed[1])
    assert (s["text"].vma, s["text"].size, s["text"].lma) == (0x40, 0x120, None)
    long = s["a_very_long_output_section_name"]
    assert (long.vma, long.size, long.lma) == (0x20000000, 0x20, 0x1d0)
    assert [(n, sz) for n, sz, _o in s["text"].inputs] == [
        (".text.gv_Ble", 0x30), (".text.gi_BLKS_Init", 0x50),
        (".text.main", 0x8), (".text.k_sleep", 0x90)]


def test_fill_symbols_and_empty_inputs_are_not_inputs(parsed):
    s = by_name(parsed[1])
    objs = [o for _n, _s, o in s["text"].inputs]
    assert not any("fill" in o for o in objs)
    assert [n for n, _s, _o in s["bss"].inputs] == [".bss.su8ar_Buf", ".bss.z_idle"]


def test_parse_without_memory_configuration():
    assert mr.parse_map("nothing here\n") == ([], [])


# ---------------------------------------------------------------- helpers

@pytest.mark.parametrize("obj, name", [
    ("app/libapp.a(Ble.c.obj)", "Ble.c.obj"),
    (r"zephyr\CMakeFiles\x.dir\Ble.c.obj", "Ble.c.obj"),
    ("CMakeFiles/app.dir/_ASW/_BLE/Ble.c.obj", "Ble.c.obj"),
])
def test_object_name(obj, name):
    assert mr.object_name(obj) == name


def test_module_index(tree):
    index, modules, clashes = mr.module_index(str(tree), ["_ASW", "_LIB", "_NONE"])
    assert index == {"main.c.obj": "_ASW/main.c", "Ble.c.obj": "_ASW/_BLE",
                     "BulkXfer_Server.c.obj": "_LIB/BulkXfer",
                     "Deep.c.obj": "_LIB/BulkXfer"}
    assert modules == ["_ASW/main.c", "_ASW/_BLE", "_LIB/BulkXfer"]
    assert clashes == []


def test_module_index_clash_goes_to_other(tree):
    (tree / "_LIB" / "BulkXfer" / "Ble.c").write_text("")
    index, _modules, clashes = mr.module_index(str(tree), ["_ASW", "_LIB"])
    assert clashes == ["Ble.c.obj"]
    assert "Ble.c.obj" not in index


def test_input_shares_pool_is_shared_out():
    # Pool of 0x40 under the first .str1.1; the others listed 0x20 + 0x40
    # before merging, so they get 0x40 * (1/3, 2/3) and the holder nothing.
    shares = mr._input_shares([
        (".text.a", 8, "a"), (".rodata.f.str1.1", 0x40, "a"),
        (".rodata.g.str1.1", 0x20, "b"), (".rodata.str1.1", 0x40, "c")])
    got = {}
    for obj, b in shares:
        got[obj] = got.get(obj, 0) + b
    assert got == pytest.approx({"a": 8, "b": 0x40 / 3, "c": 0x80 / 3})


def test_input_shares_holder_keeps_remainder():
    shares = dict(mr._input_shares([
        (".rodata.f.str1.1", 0x50, "a"), (".rodata.g.str1.1", 0x10, "b")]))
    assert shares == {"a": 0x40, "b": 0x10}


def test_input_shares_kinds_are_separate_pools():
    shares = mr._input_shares([
        (".rodata.str1.1", 4, "a"), (".rodata.cst4", 8, "b"),
        (".rodata.x.cst4", 4, "c")])
    assert sorted(shares) == [("a", 4.0), ("b", 4.0), ("c", 4.0)]


# ---------------------------------------------------------------- usage

@pytest.fixture
def rows(tree, parsed):
    regions, outputs = parsed
    index, modules, _ = mr.module_index(str(tree), ["_ASW", "_LIB"])
    return mr.usage(regions, outputs, index, modules)


def test_usage_per_module(rows):
    # The string pool (0x40, listed under Ble.c) goes to the other 0x60 of
    # strings, scaled by 2/3: BulkXfer 0x20 -> 21, the kernel 43, Ble.c 0.
    # The data section counts in RAM and, by its load address, in FLASH.
    ble = rows["_ASW/_BLE"]
    assert ble["FLASH"] == 0x30 + 0x10 + 0 + 0x20
    assert ble["RAM"] == 0x20 + 0xc0
    blk = rows["_LIB/BulkXfer"]
    assert blk == {"FLASH": 0x50 + round(0x40 / 3), "RAM": 0, "IDT_LIST": 0}
    assert rows["_ASW/main.c"]["FLASH"] == 8


def test_usage_totals_match_region_extent(rows):
    # FLASH ends with the data's load image (0x1d0 + 0x20), RAM with bss
    # (0x30 + 0x100); the gaps before rodata and bss (0x10 each) are OTHER.
    flash = sum(r["FLASH"] for r in rows.values())
    ram = sum(r["RAM"] for r in rows.values())
    assert (flash, ram) == (0x1f0, 0x130)
    assert rows[mr.OTHER]["FLASH"] == 0x40 + 0x90 + 0x8 + 43 + 0x10 + 0x10
    assert sum(r["IDT_LIST"] for r in rows.values()) == 0
    assert list(rows)[-1] == mr.OTHER


def test_usage_section_outside_regions_is_ignored():
    regions = [mr.Region("FLASH", 0, 0x100)]
    sec = mr.OutputSection("far", 0x5000, 0x10, None)
    sec.inputs.append((".text.x", 0x10, "x.c.obj"))
    rows = mr.usage(regions, [sec], {"x.c.obj": "_ASW/_X"}, ["_ASW/_X"])
    assert rows["_ASW/_X"]["FLASH"] == 0 and rows[mr.OTHER]["FLASH"] == 0


def test_usage_module_share_capped_by_section_size():
    regions = [mr.Region("FLASH", 0, 0x100)]
    sec = mr.OutputSection("text", 0, 0x10, None)
    sec.inputs.append((".text.x", 0x20, "x.c.obj"))      # map says more
    rows = mr.usage(regions, [sec], {"x.c.obj": "_ASW/_X"}, ["_ASW/_X"])
    assert rows["_ASW/_X"]["FLASH"] == 0x10 and rows[mr.OTHER]["FLASH"] == 0


# ---------------------------------------------------------------- report

def test_format_report(rows, parsed):
    text = mr.format_report(parsed[0], rows, ["_ASW", "_LIB"])
    lines = text.splitlines()
    assert lines[0] == "Memory usage per module"
    assert "FLASH (B)" in lines[1] and "RAM (B)" in lines[1]
    assert "IDT_LIST" not in text                      # unused region hidden
    labels = [ln.split()[0] for ln in lines[3:] if not ln.startswith("-")]
    assert labels == ["_ASW/main.c", "_ASW/_BLE", "_ASW", "_LIB/BulkXfer",
                      "_LIB", "Other", "Total"]
    total = lines[-1].split()
    assert total[:2] == ["Total", str(0x1f0)]
    assert total[2] == f"{100 * 0x1f0 / 0x1000:.2f}%"


def test_format_report_hides_empty_unless_all(tree, parsed):
    regions, outputs = parsed
    (tree / "_ASW" / "_GAP").mkdir()
    (tree / "_ASW" / "_GAP" / "Gap.c").write_text("")
    index, modules, _ = mr.module_index(str(tree), ["_ASW", "_LIB"])
    rows = mr.usage(regions, outputs, index, modules)
    assert "_ASW/_GAP" not in mr.format_report(regions, rows, ["_ASW", "_LIB"])
    assert "_ASW/_GAP" in mr.format_report(regions, rows, ["_ASW", "_LIB"], True)


def test_format_report_skips_dir_without_modules(rows, parsed):
    text = mr.format_report(parsed[0], rows, ["_ASW", "_LIB", "_NONE"])
    assert "_NONE" not in text


# ---------------------------------------------------------------- command line

def test_main_prints_and_writes(tree, tmp_path, capsys):
    m = tmp_path / "zephyr.map"
    m.write_text(MAP)
    out = tmp_path / "report.txt"
    assert mr.main(["--map", str(m), "--root", str(tree), "--out", str(out)]) == 0
    printed = capsys.readouterr().out
    assert printed.startswith("Memory usage per module")
    assert out.read_text() == printed


def test_main_reports_clash(tree, tmp_path, capsys):
    (tree / "_LIB" / "BulkXfer" / "Ble.c").write_text("")
    m = tmp_path / "zephyr.map"
    m.write_text(MAP)
    assert mr.main(["--map", str(m), "--root", str(tree)]) == 0
    assert "Ble.c.obj is built from two modules" in capsys.readouterr().err


def test_main_missing_map(tmp_path, capsys):
    assert mr.main(["--map", str(tmp_path / "none.map")]) == 1
    assert "mem_report:" in capsys.readouterr().err


def test_main_map_without_regions(tmp_path, capsys):
    m = tmp_path / "zephyr.map"
    m.write_text("garbage\n")
    assert mr.main(["--map", str(m)]) == 1
    assert "no memory regions" in capsys.readouterr().err
