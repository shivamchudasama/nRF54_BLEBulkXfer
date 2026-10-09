"""Scan / connect / disconnect / read CAPS: the window's Device page.

On the left the connection card (link state, device, Connect / Disconnect /
Read Caps) with the link's details (ATT MTU and CAPS) in a section that
opens after Read Caps, and the base UUID in a collapsible section under it;
on the right the scan list."""

import tkinter as tk
from tkinter import messagebox, ttk

from ..core.ble_link import LinkState
from ..protocols import setu
from ..protocols.setu import setu_client
from . import theme as th
from .widgets import Disclosure, Pill, card, set_icon, set_var

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
        theme = th.of(ctx)
        super().__init__(parent)
        self.ctx = ctx
        self._is_busy = is_busy          # () -> True if a feature has work in progress
        self._results = []
        s = ctx.settings
        self.columnconfigure(0, weight=2, uniform="col")
        self.columnconfigure(1, weight=3, uniform="col")
        self.rowconfigure(0, weight=1)
        left = ttk.Frame(self)
        left.grid(row=0, column=0, sticky="nsew", padx=(0, theme.sp("s")))

        # ---- connection card ----
        conn, head = card(left, "Device")
        conn.pack(fill="x")
        self.state_pill = Pill(head, theme, "Idle", "idle")
        self.state_pill.grid(row=0, column=2, sticky="e")
        self.name_var = tk.StringVar(value="Not connected")
        self.addr_var = tk.StringVar(value="Select a device, then Connect.")
        ttk.Label(conn, textvariable=self.name_var, style="Title.TLabel").pack(anchor="w")
        ttk.Label(conn, textvariable=self.addr_var, style="MonoCaption.TLabel").pack(anchor="w")

        btns = ttk.Frame(conn)
        btns.pack(fill="x", pady=(theme.sp("m"), 0))
        btns.columnconfigure((0, 1, 2), weight=1, uniform="btn")
        self.connect_btn = ttk.Button(btns, text="Connect", style="Accent.TButton", command=self._connect)
        self.connect_btn.grid(row=0, column=0, sticky="ew")
        self.disconnect_btn = ttk.Button(btns, text="Disconnect", command=self._disconnect)
        self.disconnect_btn.grid(row=0, column=1, sticky="ew", padx=theme.px(6))
        self.caps_btn = ttk.Button(btns, text="Read Caps", command=self._read_caps)
        self.caps_btn.grid(row=0, column=2, sticky="ew")
        set_icon(self.connect_btn, theme, "connect", accent=True)
        set_icon(self.disconnect_btn, theme, "disconnect")
        set_icon(self.caps_btn, theme, "info")

        # ---- link details (closed until Read Caps) ----
        self.details = Disclosure(conn, "Link details", theme=theme)
        self.details.pack(fill="x", pady=(theme.sp("m"), 0))
        grid = ttk.Frame(self.details.body)
        grid.pack(fill="x", pady=(0, theme.sp("xs")))
        self._facts = {}
        for i, key in enumerate(("ATT MTU", "Protocol", "Window", "Max frame")):
            cell = ttk.Frame(grid)
            cell.grid(row=i // 2, column=i % 2, sticky="w", padx=(0, theme.sp("l")), pady=theme.px(2))
            ttk.Label(cell, text=key.upper(), style="Caption.TLabel").pack(anchor="w")
            var = tk.StringVar(value="–")
            ttk.Label(cell, textvariable=var, style="Strong.TLabel").pack(anchor="w")
            self._facts[key] = var
        grid.columnconfigure((0, 1), weight=1, uniform="facts")
        self.caps_var = tk.StringVar(value="")
        ttk.Label(self.details.body, textvariable=self.caps_var, style="Caption.TLabel",
                  wraplength=theme.px(320), justify="left").pack(anchor="w")

        # ---- advanced ----
        adv = Disclosure(left, "Advanced: base UUID", theme=theme)
        adv.pack(fill="x", pady=(theme.sp("m"), 0))
        row = ttk.Frame(adv.body)
        row.pack(fill="x")
        ttk.Label(row, text="B1C0xxxx-", style="Mono.TLabel").pack(side="left")
        self.base_var = tk.StringVar(value=s.base_uuid)
        self.base_entry = ttk.Entry(row, textvariable=self.base_var, width=30)
        self.base_entry.pack(side="left", fill="x", expand=True, padx=(theme.sp("xs"), 0))

        # ---- scan card ----
        scan, _ = card(self, "Nearby devices")
        scan.grid(row=0, column=1, sticky="nsew", padx=(theme.sp("s"), 0))
        top = ttk.Frame(scan)
        top.pack(fill="x")
        ttk.Label(top, text="Name filter").pack(side="left")
        self.filter_var = tk.StringVar(value=s.name_filter)
        self.filter_var.trace_add("write", lambda *a: self._show_results())
        ttk.Entry(top, textvariable=self.filter_var).pack(side="left", fill="x", expand=True, padx=theme.px(6))
        self.scan_btn = ttk.Button(top, text="Scan", command=self._scan)
        self.scan_btn.pack(side="left")
        set_icon(self.scan_btn, theme, "search")

        cols = ("name", "address", "rssi", "svc")
        body = ttk.Frame(scan)
        body.pack(fill="both", expand=True, pady=(theme.sp("s"), 0))
        self.tree = ttk.Treeview(body, columns=cols, show="headings", height=6, selectmode="browse")
        for c, text, w in (("name", "Name", 140), ("address", "Address", 130),
                           ("rssi", "RSSI", 50), ("svc", "SETU", 70)):
            self.tree.heading(c, text=text, anchor="w")
            self.tree.column(c, width=theme.px(w), minwidth=theme.px(36), anchor="w", stretch=(c == "name"))
        sb = ttk.Scrollbar(body, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        self.tree.bind("<<TreeviewSelect>>", lambda e: self.update_state())
        self.tree.bind("<Double-1>", lambda e: self._connect())
        ttk.Label(scan, text="Double-click a device to connect.", style="Caption.TLabel").pack(
            anchor="w", pady=(theme.px(6), 0))

        theme.on_change(self._recolour)
        self.update_state()

    def _recolour(self, p):
        self.tree.tag_configure("setu", foreground=p["ok"][0])

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
            set_var(self.caps_var, "")
            for var in self._facts.values():
                set_var(var, "–")

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
            self.addr_var.set(info.get("reason") or "Select a device, then Connect.")

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
        svc = setu_client.char_uuid(0, self.base_var.get().strip())
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
            self.tree.insert("", "end", iid=r.address, tags=("setu",) if has else (),
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
        svc = self.ctx.services[setu.SETUService.NAME]
        self.ctx.run(svc.read_caps(), on_done=self._caps_done, on_error=self._error("Read Caps"))

    def _caps_done(self, caps):
        ver, max_frame, window, mtu = caps
        frame = min(mtu - 3, max_frame)
        self.caps_var.set(f"protocol v{ver} · max frame {max_frame} B · window {window} · "
                          f"ATT MTU {mtu} → {frame - 4} B per DATA frame")
        for key, value in (("ATT MTU", str(mtu)), ("Protocol", f"v{ver}"),
                           ("Window", str(window)), ("Max frame", f"{max_frame} B")):
            self._facts[key].set(value)
        self.details.set_open(True)
        self.ctx.log(f"caps: v{ver}, max frame {max_frame}, window {window}, ATT MTU {mtu}")

    def _error(self, what):
        def show(exc):
            self.ctx.log(f"{what}: {type(exc).__name__}: {exc}", "error")
            messagebox.showerror(what, f"{type(exc).__name__}: {exc}")
            self.update_state()
        return show
