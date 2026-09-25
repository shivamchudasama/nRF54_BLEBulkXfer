"""Scan / connect / disconnect / read CAPS."""

import tkinter as tk
from tkinter import messagebox, ttk

from ..core.ble_link import LinkState
from ..protocols import bulkxfer
from ..protocols.bulkxfer import bx


class ConnectionPanel(ttk.LabelFrame):
    def __init__(self, parent, ctx, is_busy):
        super().__init__(parent, text="Device", padding=6)
        self.ctx = ctx
        self._is_busy = is_busy          # () -> True if a feature has work in progress
        self._results = []
        s = ctx.settings

        top = ttk.Frame(self)
        top.pack(fill="x")
        ttk.Label(top, text="Name filter").pack(side="left")
        self.filter_var = tk.StringVar(value=s.name_filter)
        self.filter_var.trace_add("write", lambda *a: self._show_results())
        ttk.Entry(top, textvariable=self.filter_var, width=22).pack(side="left", padx=4)
        self.scan_btn = ttk.Button(top, text="Scan", command=self._scan)
        self.scan_btn.pack(side="left", padx=(4, 12))
        ttk.Label(top, text="Base UUID  B1C0xxxx-").pack(side="left")
        self.base_var = tk.StringVar(value=s.base_uuid)
        self.base_entry = ttk.Entry(top, textvariable=self.base_var, width=30)
        self.base_entry.pack(side="left")

        cols = ("name", "address", "rssi", "svc")
        self.tree = ttk.Treeview(self, columns=cols, show="headings", height=4, selectmode="browse")
        for c, text, w in (("name", "Name", 200), ("address", "Address", 160),
                           ("rssi", "RSSI", 60), ("svc", "BulkXfer", 70)):
            self.tree.heading(c, text=text, anchor="w")
            self.tree.column(c, width=w, anchor="w", stretch=(c == "name"))
        self.tree.pack(fill="x", pady=4)
        self.tree.bind("<<TreeviewSelect>>", lambda e: self.update_state())
        self.tree.bind("<Double-1>", lambda e: self._connect())

        bottom = ttk.Frame(self)
        bottom.pack(fill="x")
        self.connect_btn = ttk.Button(bottom, text="Connect", command=self._connect)
        self.connect_btn.pack(side="left")
        self.disconnect_btn = ttk.Button(bottom, text="Disconnect", command=self._disconnect)
        self.disconnect_btn.pack(side="left", padx=4)
        self.caps_btn = ttk.Button(bottom, text="Read Caps", command=self._read_caps)
        self.caps_btn.pack(side="left")
        self.caps_var = tk.StringVar(value="")
        ttk.Label(bottom, textvariable=self.caps_var).pack(side="left", padx=8)

        self.update_state()

    # ---- state -------------------------------------------------------------
    def _selected(self):
        sel = self.tree.selection()
        return sel[0] if sel else None

    def update_state(self):
        st = self.ctx.link.state
        idle = st == LinkState.IDLE
        con = st == LinkState.CONNECTED
        self.scan_btn.state(["!disabled"] if idle else ["disabled"])
        self.connect_btn.state(["!disabled"] if idle and self._selected() else ["disabled"])
        self.disconnect_btn.state(["!disabled"] if con or st == LinkState.CONNECTING else ["disabled"])
        self.caps_btn.state(["!disabled"] if con else ["disabled"])
        self.base_entry.state(["!disabled"] if idle else ["disabled"])
        if not con:
            self.caps_var.set("")

    # ---- scan --------------------------------------------------------------
    def _scan(self):
        self.ctx.settings.name_filter = self.filter_var.get()
        self.ctx.run(self.ctx.link.scan(self.ctx.settings.scan_timeout),
                     on_done=self._scan_done, on_error=self._error("Scan"))

    def _scan_done(self, results):
        self._results = results
        self._show_results()
        self.ctx.log(f"scan: {len(results)} device(s)")

    def _show_results(self):
        svc = bx.char_uuid(0, self.base_var.get().strip())
        flt = self.filter_var.get().strip().lower()
        prev = self._selected()
        self.tree.delete(*self.tree.get_children())
        rows = []
        for r in self._results:
            has = svc in r.service_uuids
            if flt and flt not in r.name.lower() and not has:
                continue
            rows.append((not has, -r.rssi, r))
        for _, _, r in sorted(rows, key=lambda t: t[:2]):
            has = svc in r.service_uuids
            self.tree.insert("", "end", iid=r.address,
                             values=(r.name or "(no name)", r.address, r.rssi, "yes" if has else ""))
        if prev and self.tree.exists(prev):
            self.tree.selection_set(prev)
        elif self.tree.get_children():
            self.tree.selection_set(self.tree.get_children()[0])
        self.update_state()

    # ---- connect -----------------------------------------------------------
    def _connect(self):
        addr = self._selected()
        if not addr or self.ctx.link.state != LinkState.IDLE:
            return
        base = self.base_var.get().strip().lower()
        if len(base) != 27 or base.count("-") != 3:
            messagebox.showerror("Base UUID", "Expected the 96-bit base as xxxx-xxxx-xxxx-xxxxxxxxxxxx")
            return
        self.ctx.settings.base_uuid = base
        name = self.tree.set(addr, "name")
        self.ctx.run(self.ctx.link.connect(addr, name), on_error=self._error("Connect"))

    def _disconnect(self):
        if self._is_busy() and not messagebox.askyesno(
                "Disconnect", "A transfer is in progress. Abort it and disconnect?"):
            return
        self.ctx.run(self.ctx.link.disconnect(), on_error=self._error("Disconnect"))

    def _read_caps(self):
        svc = self.ctx.services[bulkxfer.BulkXferService.NAME]
        self.ctx.run(svc.read_caps(), on_done=self._caps_done, on_error=self._error("Read Caps"))

    def _caps_done(self, caps):
        ver, max_frame, window, mtu = caps
        frame = min(mtu - 3, max_frame)
        self.caps_var.set(f"protocol v{ver} · max frame {max_frame} B · window {window} · "
                          f"ATT MTU {mtu} → {frame - 4} B per DATA frame")
        self.ctx.log(f"caps: v{ver}, max frame {max_frame}, window {window}, ATT MTU {mtu}")

    def _error(self, what):
        def show(exc):
            self.ctx.log(f"{what}: {type(exc).__name__}: {exc}", "error")
            messagebox.showerror(what, f"{type(exc).__name__}: {exc}")
            self.update_state()
        return show
