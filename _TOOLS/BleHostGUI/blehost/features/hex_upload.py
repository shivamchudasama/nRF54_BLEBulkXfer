"""Hex Upload: send an Intel HEX file segment by segment (_DOC/HexUpload/PROTOCOL.md)."""

import os
import struct
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from ..protocols import bulkxfer
from ..protocols.bulkxfer import bx
from ..ui import theme as th
from ..ui.widgets import Pill, StatTile, card
from .base import Feature


def _result(p: bytes) -> str:
    if len(p) < 9:
        return p.hex(" ")
    st, addr, n = struct.unpack("<BII", p[:9])
    return f"{bx.STATUS.get(st, hex(st))} addr=0x{addr:08x} len={n}"


bulkxfer.register_app_type(bx.APP_TYPE_SEGMENT, "SEGMENT")
bulkxfer.register_app_type(bx.APP_TYPE_RESULT, "RESULT", _result)
bulkxfer.register_app_type(bx.APP_TYPE_STORED, "STORED", _result)

# Upload phase -> (pill text, tone)
PHASE_PILL = {
    "nofile": ("No file", "idle"), "error": ("Parse error", "err"), "ready": ("Ready", "ok"),
    "uploading": ("Uploading", "info"), "done": ("Done", "ok"),
    "failed": ("Failed", "err"), "aborted": ("Aborted", "warn"),
}


def status_tone(text: str) -> str:
    """Theme tone of a segment's status text."""
    if text.startswith("stored"):
        return "ok"
    if text.startswith("sending"):
        return "info"
    if text.startswith("aborted"):
        return "warn"
    if text.startswith(("failed", "STORED mismatch")):
        return "err"
    return "idle"


class HexUploadFeature(Feature):
    title = "Hex Upload"

    def __init__(self, ctx):
        super().__init__(ctx)
        self.segments = []
        self._parsed = None              # (path, seg_max) the segments came from
        self._future = None
        self._current = None             # index of the segment in flight

    # ---- UI ----------------------------------------------------------------
    def build(self, parent):
        theme = th.of(self.ctx)
        f = ttk.Frame(parent, padding=(12, 12, 12, 10))

        filec, _ = card(f, "Hex file")
        filec.pack(fill="x")
        row = ttk.Frame(filec)
        row.pack(fill="x")
        self.path_var = tk.StringVar()
        ttk.Entry(row, textvariable=self.path_var, state="readonly").pack(side="left", fill="x", expand=True)
        self.browse_btn = ttk.Button(row, text="Browse…", command=self._browse)
        self.browse_btn.pack(side="left", padx=(6, 0))
        opt = ttk.Frame(filec)
        opt.pack(fill="x", pady=(8, 0))
        ttk.Label(opt, text="Max segment (bytes)").pack(side="left")
        self.seg_max_var = tk.StringVar(value=str(bx.SEG_MAX))
        self.seg_max_entry = ttk.Entry(opt, textvariable=self.seg_max_var, width=8)
        self.seg_max_entry.pack(side="left", padx=6)
        self.seg_max_entry.bind("<Return>", lambda e: self._load())
        self.seg_max_entry.bind("<FocusOut>", lambda e: self._load(quiet=True))
        self.summary_var = tk.StringVar(value="No file loaded")
        ttk.Label(opt, textvariable=self.summary_var, style="Caption.TLabel").pack(side="left", padx=12)

        tiles = ttk.Frame(f)
        tiles.pack(fill="x", pady=10)
        self.tile_segments = StatTile(tiles, "Segments", "–", "no file")
        self.tile_size = StatTile(tiles, "Size", "–", "")
        self.tile_sent = StatTile(tiles, "Sent", "–", "not started")
        self.tile_rate = StatTile(tiles, "Rate", "–", "")
        for i, t in enumerate((self.tile_segments, self.tile_size, self.tile_sent, self.tile_rate)):
            t.grid(row=0, column=i, sticky="ew", padx=(0 if i == 0 else 10, 0))
            tiles.columnconfigure(i, weight=1, uniform="tile")

        up, head = card(f, "Upload")
        up.pack(fill="x")
        self.state_pill = Pill(head, theme, "No file", "idle")
        self.state_pill.grid(row=0, column=2, sticky="e")
        prow = ttk.Frame(up)
        prow.pack(fill="x")
        self.progress = ttk.Progressbar(prow, mode="determinate", length=240)
        self.progress.pack(side="left", fill="x", expand=True)
        self.progress_var = tk.StringVar()
        ttk.Label(prow, textvariable=self.progress_var, width=38, anchor="e",
                  style="MonoCaption.TLabel").pack(side="left", padx=(10, 0))
        btns = ttk.Frame(up)
        btns.pack(fill="x", pady=(10, 0))
        self.start_btn = ttk.Button(btns, text="Start Upload", style="Accent.TButton", command=self._start)
        self.start_btn.pack(side="left")
        self.abort_btn = ttk.Button(btns, text="Abort", style="Danger.TButton", command=self.cancel)
        self.abort_btn.pack(side="left", padx=6)

        seg, _ = card(f, "Segments")
        seg.pack(fill="both", expand=True, pady=(10, 0))
        body = ttk.Frame(seg)
        body.pack(fill="both", expand=True)
        cols = ("addr", "len", "status")
        self.tree = ttk.Treeview(body, columns=cols, show="headings", height=4)
        for c, text, w in (("addr", "Address", 120), ("len", "Length", 90), ("status", "Status", 320)):
            self.tree.heading(c, text=text, anchor="w")
            self.tree.column(c, width=w, anchor="w", stretch=(c == "status"))
        self.tree.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(body, orient="vertical", command=self.tree.yview)
        sb.pack(side="right", fill="y")
        self.tree.configure(yscrollcommand=sb.set)

        self._phase = "nofile"
        theme.on_change(self._recolour)
        self._update_buttons()
        return f

    def _recolour(self, p):
        for tone in th.TONES:
            self.tree.tag_configure(tone, foreground=p[tone][0])

    def _update_buttons(self):
        busy = self.busy
        can_start = self.ctx.link.connected and bool(self.segments) and not busy
        self.start_btn.state(["!disabled"] if can_start else ["disabled"])
        self.abort_btn.state(["!disabled"] if busy else ["disabled"])
        for w in (self.browse_btn, self.seg_max_entry):
            w.state(["disabled"] if busy else ["!disabled"])
        self._show_phase()

    # ---- display (tiles, pill) ----------------------------------------------
    def _show_phase(self, phase: str = None):
        if phase:
            self._phase = phase
        ph = self._phase
        if ph == "ready" and not self.ctx.link.connected:
            self.state_pill.set("Waiting for link", "warn")
        else:
            self.state_pill.set(*PHASE_PILL[ph])

    def _show_loaded(self, path, total):
        n = len(self.segments)
        self.tile_segments.set(str(n), "ready")
        self.tile_size.set(f"{total:,} B", os.path.basename(path))
        self.tile_sent.set("–", "not started")
        self.tile_rate.set("–", "")

    # ---- file --------------------------------------------------------------
    def _browse(self):
        path = filedialog.askopenfilename(title="Select Intel HEX file",
                                          filetypes=[("Intel HEX", "*.hex *.ihex"), ("All files", "*.*")])
        if path:
            self.path_var.set(path)
            self._parsed = None
            self._load()

    def _seg_max(self) -> int:
        try:
            v = int(self.seg_max_var.get(), 0)
        except ValueError:
            v = 0
        if not 1 <= v <= bx.SEG_MAX:
            raise ValueError(f"max segment must be 1…{bx.SEG_MAX}")
        return v

    def _load(self, quiet=False) -> bool:
        path = self.path_var.get()
        if not path or self.busy:
            return False
        try:
            seg_max = self._seg_max()
            if self._parsed == (path, seg_max):
                return True
            self.segments = bx.parse_ihex(path, seg_max)
        except (OSError, ValueError) as e:
            self.segments, self._parsed = [], None
            self.summary_var.set("Parse error")
            self.tree.delete(*self.tree.get_children())
            self.tile_segments.set("–", "parse error")
            self._phase = "error"
            self._update_buttons()
            if not quiet:
                messagebox.showerror("Hex file", str(e))
            self.ctx.log(f"hex: {e}", "error")
            return False
        self._parsed = (path, seg_max)
        total = sum(len(d) for _, d in self.segments)
        self.summary_var.set(f"{len(self.segments)} segment(s), {total} bytes")
        self.tree.delete(*self.tree.get_children())
        for i, (addr, data) in enumerate(self.segments):
            self.tree.insert("", "end", iid=str(i), values=(f"0x{addr:08X}", len(data), "pending"),
                             tags=("idle",))
        self.progress.configure(maximum=max(total, 1), value=0)
        self.progress_var.set("")
        self._show_loaded(path, total)
        self._phase = "ready"
        self.ctx.log(f"hex: {os.path.basename(path)}: {len(self.segments)} segment(s), {total} bytes")
        self._update_buttons()
        return True

    def _set_status(self, i: int, text: str):
        if self.tree.exists(str(i)):
            self.tree.set(str(i), "status", text)
            self.tree.item(str(i), tags=(status_tone(text),))
            self.tree.see(str(i))

    # ---- upload ------------------------------------------------------------
    def _start(self):
        if self.busy or not self._load():
            return
        for i in range(len(self.segments)):
            self._set_status(i, "pending")
        self.progress.configure(value=0)
        self._t_start = time.perf_counter()
        self._phase = "uploading"
        self._future = self.ctx.run(self._upload(list(self.segments)),
                                    on_done=self._finished, on_error=self._failed,
                                    on_cancel=self._aborted)
        self._update_buttons()

    async def _upload(self, segments) -> int:
        """Runs on the BLE loop. Returns the number of segments stored."""
        call = self.ctx.bus.call
        blk = self.ctx.services[bulkxfer.BulkXferService.NAME].new_client()
        self.ctx.log(f"hex: upload started, ATT MTU {self.ctx.link.mtu_size}, "
                     f"{blk.frame_cap - 4} B per DATA frame")
        total = sum(len(d) for _, d in segments)
        done = 0
        for i, (addr, data) in enumerate(segments):
            self._current = i
            call(self._set_status, i, "sending")
            t0 = time.perf_counter()
            t_xfer = [None]

            def progress(acked, obj_len, base=done, t0=t0):
                sent = max(0, acked - 4)                  # object = 4-byte address + data
                if acked == obj_len:
                    t_xfer[0] = time.perf_counter() - t0
                call(self._progress, i, base + sent, total, sent / max(time.perf_counter() - t0, 1e-3))

            status, stored = await blk.upload_segment(addr, data, progress)
            if status != "OK":
                call(self._set_status, i, f"failed: {status}")
                raise RuntimeError(f"segment 0x{addr:08X}: {status}")
            st, s_addr, s_len = stored
            dt = t_xfer[0] or (time.perf_counter() - t0)
            ok = st == 0 and s_addr == addr and s_len == len(data)
            text = (f"stored, {len(data) * 8 / dt / 1000:.0f} kbit/s" if ok else
                    f"STORED mismatch: {bx.STATUS.get(st, st)} 0x{s_addr:08X} {s_len} B")
            call(self._set_status, i, text)
            self.ctx.log(f"hex: 0x{addr:08X} {len(data)} B {text}", "info" if ok else "warn")
            if not ok:
                raise RuntimeError(f"segment 0x{addr:08X}: {text}")
            done += len(data)
            call(self._progress, i, done, total, None)
        self._current = None
        return len(segments)

    def _progress(self, i, done, total, rate):
        self.progress.configure(maximum=max(total, 1), value=done)
        r = f", {rate * 8 / 1000:.0f} kbit/s" if rate else ""
        self.progress_var.set(f"segment {i + 1}/{len(self.segments)}  {done}/{total} B{r}")
        self.tile_segments.set(str(len(self.segments)), f"segment {i + 1} of {len(self.segments)}")
        self.tile_sent.set(f"{done:,} B", f"{100 * done // max(total, 1)} %")
        if rate:
            self.tile_rate.set(f"{rate * 8 / 1000:.0f} kbit/s", "current segment")

    def _finished(self, n):
        self._future = None
        dt = time.perf_counter() - self._t_start
        self.progress_var.set(f"done: {n} segment(s) in {dt:.1f} s")
        total = sum(len(d) for _, d in self.segments)
        self.tile_segments.set(str(n), "all stored")
        self.tile_rate.set(f"{total * 8 / max(dt, 1e-3) / 1000:.0f} kbit/s", f"average, {dt:.1f} s")
        self._phase = "done"
        self.ctx.log(f"hex: upload complete, {n} segment(s) in {dt:.1f} s")
        self._update_buttons()

    def _failed(self, exc):
        self._future = None
        if self._current is not None:
            self._set_status(self._current, f"failed: {exc}")
        self._current = None
        self.progress_var.set("failed")
        self._phase = "failed"
        self.ctx.log(f"hex: upload failed: {exc}", "error")
        self._update_buttons()

    def _aborted(self):
        self._future = None
        if self._current is not None:
            self._set_status(self._current, "aborted")
        self._current = None
        self.progress_var.set("aborted")
        self._phase = "aborted"
        self.ctx.log("hex: upload aborted", "warn")
        self._update_buttons()

    # ---- Feature -----------------------------------------------------------
    @property
    def busy(self) -> bool:
        return self._future is not None

    def cancel(self):
        if self._future is not None:
            self._future.cancel()

    def on_connected(self, info):
        self._update_buttons()

    def on_disconnected(self, reason):
        self.cancel()
        self._update_buttons()
