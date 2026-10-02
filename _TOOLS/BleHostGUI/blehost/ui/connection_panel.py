"""Scan / connect / disconnect / read CAPS: the window's left sidebar.

A connection card (link state, device, ATT MTU and CAPS) above the scan list,
with the base UUID in a collapsible section under it."""

import tkinter as tk
from tkinter import messagebox, ttk

from ..core.ble_link import LinkState
from ..protocols import bulkxfer
from ..protocols.bulkxfer import bx
from . import theme as th
from .widgets import Disclosure, Pill, card

# Link state -> (pill text, tone)
STATE_PILL = {
    LinkState.IDLE: ("Idle", "idle"),
    LinkState.SCANNING: ("Scanning…", "info"),
    LinkState.CONNECTING: ("Connecting…", "warn"),
    LinkState.CONNECTED: ("Connected", "ok"),
    LinkState.DISCONNECTING: ("Disconnecting…", "warn"),
}


class ConnectionPanel(ttk.Frame):
    def __init__(self, parent, ctx, is_busy):
        super().__init__(parent, padding=(12, 12, 10, 12))
        self.ctx = ctx
        self._is_busy = is_busy          # () -> True if a feature has work in progress
        self._results = []
        theme = th.of(ctx)
        s = ctx.settings

        # ---- connection card ----
        conn, head = card(self, "Device")
        conn.pack(fill="x")
        self.state_pill = Pill(head, theme, "Idle", "idle")
        self.state_pill.grid(row=0, column=2, sticky="e")
        self.name_var = tk.StringVar(value="Not connected")
        self.addr_var = tk.StringVar(value="Select a device below, then Connect.")
        ttk.Label(conn, textvariable=self.name_var, style="Title.TLabel").pack(anchor="w")
        ttk.Label(conn, textvariable=self.addr_var, style="MonoCaption.TLabel").pack(anchor="w")

        grid = ttk.Frame(conn)
        grid.pack(fill="x", pady=(8, 4))
        self._facts = {}
        for i, key in enumerate(("ATT MTU", "Protocol", "Window", "Max frame")):
            cell = ttk.Frame(grid)
            cell.grid(row=i // 2, column=i % 2, sticky="w", padx=(0, 18), pady=2)
            ttk.Label(cell, text=key.upper(), style="Caption.TLabel").pack(anchor="w")
            var = tk.StringVar(value="–")
            ttk.Label(cell, textvariable=var, style="Strong.TLabel").pack(anchor="w")
            self._facts[key] = var
        grid.columnconfigure((0, 1), weight=1, uniform="facts")
        self.caps_var = tk.StringVar(value="")
        ttk.Label(conn, textvariable=self.caps_var, style="Caption.TLabel",
                  wraplength=290, justify="left").pack(anchor="w")

        btns = ttk.Frame(conn)
        btns.pack(fill="x", pady=(8, 0))
        btns.columnconfigure((0, 1, 2), weight=1, uniform="btn")
        self.connect_btn = ttk.Button(btns, text="Connect", style="Accent.TButton", command=self._connect)
        self.connect_btn.grid(row=0, column=0, sticky="ew")
        self.disconnect_btn = ttk.Button(btns, text="Disconnect", command=self._disconnect)
        self.disconnect_btn.grid(row=0, column=1, sticky="ew", padx=6)
        self.caps_btn = ttk.Button(btns, text="Read Caps", command=self._read_caps)
        self.caps_btn.grid(row=0, column=2, sticky="ew")

        # ---- scan card ----
        scan, _ = card(self, "Nearby devices")
        scan.pack(fill="both", expand=True, pady=(10, 0))
        top = ttk.Frame(scan)
        top.pack(fill="x")
        ttk.Label(top, text="Name filter").pack(side="left")
        self.filter_var = tk.StringVar(value=s.name_filter)
        self.filter_var.trace_add("write", lambda *a: self._show_results())
        ttk.Entry(top, textvariable=self.filter_var).pack(side="left", fill="x", expand=True, padx=6)
        self.scan_btn = ttk.Button(top, text="Scan", command=self._scan)
        self.scan_btn.pack(side="left")

        cols = ("name", "address", "rssi", "svc")
        body = ttk.Frame(scan)
        body.pack(fill="both", expand=True, pady=(8, 0))
        self.tree = ttk.Treeview(body, columns=cols, show="headings", height=6, selectmode="browse")
        for c, text, w in (("name", "Name", 120), ("address", "Address", 130),
                           ("rssi", "RSSI", 46), ("svc", "BulkXfer", 62)):
            self.tree.heading(c, text=text, anchor="w")
            self.tree.column(c, width=w, minwidth=36, anchor="w", stretch=(c == "name"))
        sb = ttk.Scrollbar(body, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        self.tree.bind("<<TreeviewSelect>>", lambda e: self.update_state())
        self.tree.bind("<Double-1>", lambda e: self._connect())
        ttk.Label(scan, text="Double-click a device to connect.", style="Caption.TLabel").pack(
            anchor="w", pady=(6, 0))

        # ---- advanced ----
        adv = Disclosure(self, "Advanced: base UUID")
        adv.pack(fill="x", pady=(10, 0))
        row = ttk.Frame(adv.body)
        row.pack(fill="x")
        ttk.Label(row, text="B1C0xxxx-", style="Mono.TLabel").pack(side="left")
        self.base_var = tk.StringVar(value=s.base_uuid)
        self.base_entry = ttk.Entry(row, textvariable=self.base_var, width=30)
        self.base_entry.pack(side="left", fill="x", expand=True, padx=(4, 0))

        theme.on_change(self._recolour)
        self.update_state()

    def _recolour(self, p):
        self.tree.tag_configure("bulkxfer", foreground=p["ok"][0])

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
        self.state_pill.set(*STATE_PILL.get(st, (st.value, "idle")))
        if not con:
            self.caps_var.set("")
            for var in self._facts.values():
                var.set("–")

    def show_link(self, state, info: dict):
        """The link changed (MainWindow, Tk thread): the card's device lines."""
        self.update_state()
        if state == LinkState.CONNECTED:
            self.name_var.set(info.get("name") or "(no name)")
            self.addr_var.set(info.get("address") or "")
            self._facts["ATT MTU"].set(str(info.get("mtu") or "–"))
        elif state == LinkState.CONNECTING:
            self.name_var.set(info.get("name") or info.get("address") or "Connecting…")
            self.addr_var.set(info.get("address") or "")
        elif state == LinkState.IDLE:
            self.name_var.set("Not connected")
            self.addr_var.set(info.get("reason") or "Select a device below, then Connect.")

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
            self.tree.insert("", "end", iid=r.address, tags=("bulkxfer",) if has else (),
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
        for key, value in (("ATT MTU", str(mtu)), ("Protocol", f"v{ver}"),
                           ("Window", str(window)), ("Max frame", f"{max_frame} B")):
            self._facts[key].set(value)
        self.ctx.log(f"caps: v{ver}, max frame {max_frame}, window {window}, ATT MTU {mtu}")

    def _error(self, what):
        def show(exc):
            self.ctx.log(f"{what}: {type(exc).__name__}: {exc}", "error")
            messagebox.showerror(what, f"{type(exc).__name__}: {exc}")
            self.update_state()
        return show
