# MemReport — memory usage per module

At the end of each link the linker prints one line per memory region for the whole image:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      331148 B      1524 KB     21.22%
             RAM:      191348 B       256 KB     72.99%
```

`mem_report.py` splits those figures by module of this repository. The firmware build runs it right after the linker's summary, and it prints:

```
Memory usage per module
Module                            FLASH (B)        %       RAM (B)        %
---------------------------------------------------------------------------
  _ASW/main.c                            30    0.00%             0    0.00%
  _ASW/_BLE                            3747    0.24%           271    0.10%
  ...
_ASW total                            35042    2.25%         91892   35.05%
  _LIB/BulkXfer                        9728    0.62%          9210    3.51%
  _LIB/GATT_CB                         1126    0.07%             0    0.00%
_LIB total                            10854    0.70%          9210    3.51%
Other (Zephyr, NCS, libc)            285252   18.28%         90246   34.43%
---------------------------------------------------------------------------
Total                                331148   21.22%        191348   72.99%
```

The percentages are of the region size, as in the linker's summary. `Total` equals the linker's figures. Regions nothing uses (`IDT_LIST`) are left out. The same table is written to `zephyr/mem_report.txt` in the image's build folder (`build/nRF54_BLEBulkXfer/zephyr/` with sysbuild).

## How it works

The tool reads only the linker map (`zephyr.map`), so it needs no toolchain binaries and no Python packages.

- **Modules.** The tool walks each scanned folder (`--dirs`, default `_ASW _LIB`). Each sub-folder (`_ASW/_PAIR`, `_LIB/BulkXfer`) is one module and takes in all its source files, including those in deeper folders. A source file directly in a scanned folder (`_ASW/main.c`) is a module of its own. Modules that use no memory (header-only ones such as `_BLE_GENERIX`) are hidden unless `--all` is given.
- **Objects.** Every input section in the map names its object file, such as `app/libapp.a(Pair.c.obj)`. The base name `Pair.c.obj` is matched to the source file `Pair.c`, and so to its module. Two source files with the same name in different modules cannot be told apart in the map: they count as Other, with a warning on stderr.
- **Regions.** An output section counts towards the region of its run-time address. When it has a load address in another region, it counts there too: initialised data lives in RAM but is copied from FLASH at boot. Sections that are not loaded (`.debug_*`, `.comment`, `.tbss`, …) are skipped. A region's total runs from its start to the end of its last section, as the linker counts it. The alignment gaps between sections go to Other.
- **Merged strings.** The linker merges string literals and constants (`.rodata.*.str1.1`, `.cst4`, …) from all objects into one pool and drops the duplicates. The map lists the whole pool under the first such input section, and every other input at its size before merging. The tool shares the pool out among those objects in proportion to that size. A module's string bytes are therefore an estimate. The error is at most the bytes that merging saved, and the totals stay exact.
- **Other** is everything outside the scanned folders: Zephyr, NCS, the BT controller, Mbed TLS, libc, padding.

## Usage

The build runs the tool by itself (root `CMakeLists.txt`, target `mem_report`). The target depends on the final ELF, so the table is printed again only when the image is relinked. It can also be run by hand on any build:

```sh
python _TOOLS/MemReport/mem_report.py --map build/nRF54_BLEBulkXfer/zephyr/zephyr.map
python _TOOLS/MemReport/mem_report.py --map <map> --root . --dirs _ASW _LIB --all --out report.txt
```

| Option | Meaning |
|---|---|
| `--map` | Linker map file (required) |
| `--root` | Repository root that `--dirs` are relative to (default `.`) |
| `--dirs` | Folders whose sub-folders are modules (default `_ASW _LIB`) |
| `--all` | Also list modules that use no memory |
| `--out` | Also write the table to this file |

The exit status is 0 on success. It is 1 when the map cannot be read or has no memory regions, and the build then fails. Adding a module needs no change here: a new folder under `_ASW` or `_LIB` shows up on the next build.

## Files

| File | Role |
|---|---|
| `mem_report.py` | Map parser (`parse_map`), module index (`module_index`), attribution (`usage`, `_input_shares`), table (`format_report`) and command line (`main`) |

Tests: [`_TEST/python/test_mem_report.py`](../../_TEST/python/test_mem_report.py). They run on a synthetic map that has each shape found in `zephyr.map`: one-line and two-line sections, a load address, NOLOAD and debug sections, fill, gaps and a merged string pool. The command line is tested too.
