"""Hex Upload: send an Intel HEX file segment by segment (_DOC/HexUpload/PROTOCOL.md).

With a name in "Store as", the device also stores the upload as the file
/FLASH_DISK:/FW/<name> on its external flash: BEGIN before the segments,
COMMIT after them, and the file's size and CRC-32 must match what was sent.
"""

import os
import struct
import time
import zlib
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from ..protocols import setu
from ..protocols.setu import setu_client
from ..ui import theme as th
from ..ui.widgets import Disclosure, Pill, StatTile, card, set_icon, set_var
from .base import Feature


def _result(p: bytes) -> str:
    if len(p) < 9:
        return p.hex(" ")
    st, addr, n = struct.unpack("<BII", p[:9])
    return f"{setu_client.STATUS.get(st, hex(st))} addr=0x{addr:08x} len={n}"


def _file(p: bytes) -> str:
    if len(p) < 10:
        return p.hex(" ")
    op, st, size, crc = struct.unpack("<BBII", p[:10])
    what = {setu_client.APP_TYPE_BEGIN: "BEGIN", setu_client.APP_TYPE_COMMIT: "COMMIT"}.get(op, f"0x{op:02x}")
    return f"{what} {setu_client.FS_STATUS.get(st, st)} size={size} crc=0x{crc:08x}"


setu.register_app_type(setu_client.APP_TYPE_SEGMENT, "SEGMENT")
setu.register_app_type(setu_client.APP_TYPE_RESULT, "RESULT", _result)
setu.register_app_type(setu_client.APP_TYPE_STORED, "STORED", _result)
setu.register_app_type(setu_client.APP_TYPE_BEGIN, "BEGIN", lambda p: p.decode("utf-8", "replace"))
setu.register_app_type(setu_client.APP_TYPE_COMMIT, "COMMIT", lambda p: "")
setu.register_app_type(setu_client.APP_TYPE_FILE, "FILE", _file)

# Progress reaches the tab once per ACK window; it is shown at most this often
PROGRESS_MS = 66

# Upload phase -> (pill text, tone)
PHASE_PILL = {
    "nofile": ("No file", "idle"), "error": ("Parse error", "err"), "ready": ("Ready", "ok"),
    "uploading": ("Uploading", "info"), "done": ("Done", "ok"),
    "failed": ("Failed", "err"), "aborted": ("Aborted", "warn"),
}


def segments_title(n: int) -> str:
    """The segment list's section title."""
    return f"Segments ({n})" if n else "Segments"


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
    icon = "upload"

    def __init__(self, ctx):
        super().__init__(ctx)
        self.segments = []
        self._parsed = None              # (path, seg_max) the segments came from
        self._future = None
        self._current = None             # index of the segment in flight
        self._pending_progress = None    # latest (i, done, total, rate) not shown yet
        self._progress_after = None

    # ---- UI ----------------------------------------------------------------
    def build(self, parent):
        theme = th.of(self.ctx)
        sp = theme.sp
        f = ttk.Frame(parent)

        filec, _ = card(f, "Hex file")
        filec.pack(fill="x")
        row = ttk.Frame(filec)
        row.pack(fill="x")
        self.path_var = tk.StringVar()
        ttk.Entry(row, textvariable=self.path_var, state="readonly").pack(side="left", fill="x", expand=True)
        self.browse_btn = ttk.Button(row, text="Browse…", command=self._browse)
        self.browse_btn.pack(side="left", padx=(sp("s"), 0))
        set_icon(self.browse_btn, theme, "folder")
        opt = ttk.Frame(filec)
        opt.pack(fill="x", pady=(sp("s"), 0))
        ttk.Label(opt, text="Max segment (bytes)").pack(side="left")
        self.seg_max_var = tk.StringVar(value=str(setu_client.SEG_MAX))
        self.seg_max_entry = ttk.Entry(opt, textvariable=self.seg_max_var, width=8)
        self.seg_max_entry.pack(side="left", padx=sp("s"))
        self.seg_max_entry.bind("<Return>", lambda e: self._load())
        self.seg_max_entry.bind("<FocusOut>", lambda e: self._load(quiet=True))
        self.summary_var = tk.StringVar(value="No file loaded")
        ttk.Label(opt, textvariable=self.summary_var, style="Caption.TLabel").pack(side="left", padx=sp("m"))
        store = ttk.Frame(filec)
        store.pack(fill="x", pady=(sp("s"), 0))
        ttk.Label(store, text="Store as").pack(side="left")
        self.store_var = tk.StringVar()
        self.store_entry = ttk.Entry(store, textvariable=self.store_var, width=24)
        self.store_entry.pack(side="left", padx=sp("s"))
        ttk.Label(store, text=f"file in {setu_client.FILE_DIR} on the device's flash (empty: not stored)",
                  style="Caption.TLabel").pack(side="left")

        tiles = ttk.Frame(f)
        tiles.pack(fill="x", pady=sp("m"))
        self.tile_segments = StatTile(tiles, "Segments", "–", "no file")
        self.tile_size = StatTile(tiles, "Size", "–", "")
        self.tile_sent = StatTile(tiles, "Sent", "–", "not started")
        self.tile_rate = StatTile(tiles, "Rate", "–", "")
        for i, t in enumerate((self.tile_segments, self.tile_size, self.tile_sent, self.tile_rate)):
            t.grid(row=0, column=i, sticky="ew", padx=(0 if i == 0 else sp("m"), 0))
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
                  style="MonoCaption.TLabel").pack(side="left", padx=(sp("m"), 0))
        btns = ttk.Frame(up)
        btns.pack(fill="x", pady=(sp("m"), 0))
        self.start_btn = ttk.Button(btns, text="Start Upload", style="Accent.TButton", command=self._start)
        self.start_btn.pack(side="left")
        self.abort_btn = ttk.Button(btns, text="Abort", style="Danger.TButton", command=self.cancel)
        self.abort_btn.pack(side="left", padx=sp("s"))
        set_icon(self.start_btn, theme, "upload", accent=True)
        set_icon(self.abort_btn, theme, "stop")

        self.seg_section = Disclosure(f, segments_title(0), theme=theme, fill="both")
        self.seg_section.pack(fill="both", expand=True, pady=(sp("m"), 0))
        body = ttk.Frame(self.seg_section.body)
        body.pack(fill="both", expand=True)
        cols = ("addr", "len", "status")
        self.tree = ttk.Treeview(body, columns=cols, show="headings", height=4)
        for c, text, w in (("addr", "Address", 120), ("len", "Length", 90), ("status", "Status", 320)):
            self.tree.heading(c, text=text, anchor="w")
            self.tree.column(c, width=theme.px(w), anchor="w", stretch=(c == "status"))
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
        for w in (self.browse_btn, self.seg_max_entry, getattr(self, "store_entry", None)):
            if w is not None:
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
        if not 1 <= v <= setu_client.SEG_MAX:
            raise ValueError(f"max segment must be 1…{setu_client.SEG_MAX}")
        return v

    def _load(self, quiet=False) -> bool:
        path = self.path_var.get()
        if not path or self.busy:
            return False
        try:
            seg_max = self._seg_max()
            if self._parsed == (path, seg_max):
                return True
            self.segments = setu_client.parse_ihex(path, seg_max)
        except (OSError, ValueError) as e:
            self.segments, self._parsed = [], None
            self.summary_var.set("Parse error")
            self.tree.delete(*self.tree.get_children())
            self.seg_section.set_title(segments_title(0))
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
        self.seg_section.set_title(segments_title(len(self.segments)))
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
    def _store_name(self):
        """The "Store as" name, None if empty; ValueError if the device would refuse it."""
        name = self.store_var.get().strip() if hasattr(self, "store_var") else ""
        if not name:
            return None
        if not setu_client.valid_file_name(name):
            raise ValueError(f"'{name}' is not a file name the device accepts: 1…{setu_client.FILE_NAME_MAX} "
                             "of A-Z a-z 0-9 . _ -")
        return name

    def _start(self):
        if self.busy or not self._load():
            return
        try:
            store = self._store_name()
        except ValueError as e:
            messagebox.showerror("Store as", str(e))
            return
        for i in range(len(self.segments)):
            self._set_status(i, "pending")
        self.progress.configure(value=0)
        self._t_start = time.perf_counter()
        self._phase = "uploading"
        self._future = self.ctx.run(self._upload(list(self.segments), store),
                                    on_done=self._finished, on_error=self._failed,
                                    on_cancel=self._aborted)
        self._update_buttons()

    async def _upload(self, segments, store=None) -> int:
        """Runs on the BLE loop. Returns the number of segments stored. With
        store, the device keeps them as the file FILE_DIR/store."""
        call = self.ctx.bus.call
        setu_cli = self.ctx.services[setu.SETUService.NAME].new_client()
        self.ctx.log(f"hex: upload started, ATT MTU {self.ctx.link.mtu_size}, "
                     f"{setu_cli.frame_cap - 4} B per DATA frame")
        if store:
            rep = await setu_cli.begin_file(store)
            if not rep.ok:
                raise RuntimeError(f"storing as {store} refused: {rep.status_name}")
            self.ctx.log(f"hex: storing as {setu_client.FILE_DIR}/{store}")
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

            status, stored = await setu_cli.upload_segment(addr, data, progress)
            if status != "OK":
                call(self._set_status, i, f"failed: {status}")
                raise RuntimeError(f"segment 0x{addr:08X}: {status}")
            st, s_addr, s_len = stored
            dt = t_xfer[0] or (time.perf_counter() - t0)
            ok = st == 0 and s_addr == addr and s_len == len(data)
            text = (f"stored, {len(data) * 8 / dt / 1000:.0f} kbit/s" if ok else
                    f"STORED mismatch: {setu_client.STATUS.get(st, st)} 0x{s_addr:08X} {s_len} B")
            call(self._set_status, i, text)
            self.ctx.log(f"hex: 0x{addr:08X} {len(data)} B {text}", "info" if ok else "warn")
            if not ok:
                raise RuntimeError(f"segment 0x{addr:08X}: {text}")
            done += len(data)
            call(self._progress, i, done, total, None)
        self._current = None
        if store:
            rep = await setu_cli.commit_file()
            if not rep.ok:
                raise RuntimeError(f"file {store} not stored: {rep.status_name}")
            image = setu_client.file_image(segments)
            if (rep.size, rep.crc) != (len(image), zlib.crc32(image)):
                raise RuntimeError(f"file {store}: device has {rep.size} B crc 0x{rep.crc:08X}, "
                                   f"expected {len(image)} B crc 0x{zlib.crc32(image):08X}")
            self.ctx.log(f"hex: stored as {setu_client.FILE_DIR}/{store}, {rep.size} B, crc 0x{rep.crc:08X}")
        return len(segments)

    def _progress(self, i, done, total, rate):
        """Progress from the upload (once per ACK window): keep the latest and
        show it at most every PROGRESS_MS, so the tab redraws a few times a
        second however fast the ACKs come."""
        last_rate = self._pending_progress[3] if self._pending_progress else None
        self._pending_progress = (i, done, total, rate or last_rate)
        if self._progress_after is None:
            self._progress_after = self.progress.after(PROGRESS_MS, self._flush_progress)

    def _flush_progress(self):
        """Show the pending progress now (the timer, and the end of an upload)."""
        if self._progress_after is not None:
            self.progress.after_cancel(self._progress_after)
            self._progress_after = None
        if self._pending_progress is None:
            return
        i, done, total, rate = self._pending_progress
        self._pending_progress = None
        self.progress.configure(maximum=max(total, 1), value=done)
        r = f", {rate * 8 / 1000:.0f} kbit/s" if rate else ""
        set_var(self.progress_var, f"segment {i + 1}/{len(self.segments)}  {done}/{total} B{r}")
        self.tile_segments.set(str(len(self.segments)), f"segment {i + 1} of {len(self.segments)}")
        self.tile_sent.set(f"{done:,} B", f"{100 * done // max(total, 1)} %")
        if rate:
            self.tile_rate.set(f"{rate * 8 / 1000:.0f} kbit/s", "current segment")

    def _finished(self, n):
        self._flush_progress()
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
        self._flush_progress()
        self._future = None
        if self._current is not None:
            self._set_status(self._current, f"failed: {exc}")
        self._current = None
        self.progress_var.set("failed")
        self._phase = "failed"
        self.ctx.log(f"hex: upload failed: {exc}", "error")
        self._update_buttons()

    def _aborted(self):
        self._flush_progress()
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
