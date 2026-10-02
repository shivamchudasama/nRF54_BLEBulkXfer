"""Main window: device sidebar on the left; on the right the feature tabs, the
application log and the toggleable BLE traffic pane. Light or dark theme
(ui/theme.py)."""

import time
import tkinter as tk
from tkinter import messagebox, ttk

from ..core.ble_link import LinkState
from ..core.event_bus import LINK_STATE, LOG
from . import theme as th
from .connection_panel import STATE_PILL, ConnectionPanel
from .traffic_view import TrafficView
from .widgets import Pill, card

APP_TITLE = "BLE Host"


class MainWindow:
    def __init__(self, root, ctx, feature_classes):
        self.root = root
        self.ctx = ctx
        root.title(APP_TITLE)
        k = root.winfo_fpixels("1i") / 96            # display scale (1.0 at 100 %)
        w = min(int(1220 * k), int(root.winfo_screenwidth() * 0.9))
        h = min(int(880 * k), int(root.winfo_screenheight() * 0.85))
        root.geometry(f"{w}x{h}")
        root.minsize(int(940 * k), int(620 * k))

        self.theme = ctx.theme = th.Theme(root, th.system_mode())
        self.theme.apply()
        self.mode_var = tk.StringVar(value=self.theme.mode)

        self.features = [cls(ctx) for cls in feature_classes]
        self.traffic_visible = tk.BooleanVar(value=False)

        self._build_toolbar()

        self.status_var = tk.StringVar(value="Idle")
        status = ttk.Frame(root, padding=(10, 4))
        status.pack(side="bottom", fill="x")
        self.status_pill = Pill(status, self.theme, "Idle", "idle")
        self.status_pill.pack(side="left")
        ttk.Label(status, textvariable=self.status_var, style="Caption.TLabel").pack(side="left", padx=8)
        self.theme_label = ttk.Label(status, style="Caption.TLabel")
        self.theme_label.pack(side="right")
        ttk.Separator(root, orient="horizontal").pack(side="bottom", fill="x")

        body = ttk.Frame(root)
        body.pack(fill="both", expand=True)
        self.conn = ConnectionPanel(body, ctx, is_busy=lambda: any(f.busy for f in self.features))
        self.conn.pack(side="left", fill="y")
        ttk.Separator(body, orient="vertical").pack(side="left", fill="y")

        self.panes = ttk.Panedwindow(body, orient="vertical")
        self.panes.pack(side="left", fill="both", expand=True, padx=(10, 12), pady=(8, 10))

        self.notebook = ttk.Notebook(self.panes)
        for f in self.features:
            self.notebook.add(f.build(self.notebook), text=f"  {f.title}  ")
        self.panes.add(self.notebook, weight=3)

        self.log_frame, self.log_text = self._build_log(self.panes)
        self.panes.add(self.log_frame, weight=1)

        self.traffic = TrafficView(self.panes, ctx)

        root.bind_all("<Control-t>", lambda e: self._toggle_traffic())
        root.protocol("WM_DELETE_WINDOW", self._on_close)
        ctx.bus.subscribe(LINK_STATE, self._on_link_state)
        ctx.bus.subscribe(LOG, self._on_log)
        self.theme.on_change(self._on_theme)

    # ---- construction ------------------------------------------------------
    def _build_menu(self, bar):
        """File and View as toolbar buttons: Windows draws a native menu bar
        light whatever the theme."""
        file_m = tk.Menu(bar, tearoff=False)
        file_m.add_command(label="Exit", command=self._on_close)
        view_m = tk.Menu(bar, tearoff=False)
        view_m.add_checkbutton(label="BLE Traffic", accelerator="Ctrl+T",
                               variable=self.traffic_visible, command=self._apply_traffic)
        theme_m = tk.Menu(view_m, tearoff=False)
        for mode, label in ((th.LIGHT, "Light"), (th.DARK, "Dark")):
            theme_m.add_radiobutton(label=label, value=mode, variable=self.mode_var,
                                    command=lambda: self._set_mode(self.mode_var.get()))
        if not self.theme.available:
            theme_m.entryconfigure("Dark", state="disabled")
        view_m.add_cascade(label="Theme", menu=theme_m)
        view_m.add_separator()
        view_m.add_command(label="Clear Log", command=lambda: self.log_text.delete("1.0", "end"))
        for label, menu in (("File", file_m), ("View", view_m)):
            b = ttk.Button(bar, text=label, style="Toolbutton", takefocus=False)
            b.configure(command=lambda b=b, menu=menu: menu.tk_popup(
                b.winfo_rootx(), b.winfo_rooty() + b.winfo_height()))
            b.pack(side="left")
            self.root.bind_all(f"<Alt-{label[0].lower()}>",
                               lambda e, b=b: b.invoke())
        self._menus = (file_m, view_m)

    def _build_toolbar(self):
        bar = ttk.Frame(self.root, padding=(8, 6, 12, 6))
        bar.pack(fill="x")
        self._build_menu(bar)
        ttk.Separator(bar, orient="vertical").pack(side="left", fill="y", padx=(6, 12), pady=4)
        ttk.Label(bar, text=APP_TITLE, style="Title.TLabel").pack(side="left")
        ttk.Label(bar, text="nRF54 BLE Bulk Transfer · hex upload · device provisioning",
                  style="Caption.TLabel").pack(side="left", padx=10)
        ttk.Checkbutton(bar, text="BLE traffic  (Ctrl+T)", style="Toggle.TButton",
                        variable=self.traffic_visible, command=self._apply_traffic).pack(side="right")
        self.mode_btn = ttk.Button(bar, command=self._toggle_mode)
        self.mode_btn.pack(side="right", padx=6)
        if not self.theme.available:
            self.mode_btn.state(["disabled"])
        ttk.Separator(self.root, orient="horizontal").pack(fill="x")

    def _build_log(self, parent):
        frame, head = card(parent, "Log", padding=(12, 8))
        ttk.Button(head, text="Clear", command=lambda: text.delete("1.0", "end")).grid(
            row=0, column=2, sticky="e")
        body = ttk.Frame(frame)
        body.pack(fill="both", expand=True)
        text = tk.Text(body, height=5, wrap="word", font=self.theme.fonts["mono"], state="normal",
                       padx=8, pady=6)
        sb = ttk.Scrollbar(body, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=sb.set)
        text.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        text.bind("<Key>", lambda e: None if (e.state & 0x4 and e.keysym.lower() == "c") else "break")
        self.theme.on_change(lambda p: th.style_text(text, p))
        return frame, text

    # ---- theme -------------------------------------------------------------
    def _toggle_mode(self):
        self._set_mode(th.DARK if self.theme.mode == th.LIGHT else th.LIGHT)

    def _set_mode(self, mode):
        self.theme.apply(mode)
        self.mode_var.set(self.theme.mode)

    def _on_theme(self, _palette):
        dark = self.theme.mode == th.DARK
        self.mode_btn.configure(text="☀  Light mode" if dark else "☾  Dark mode")
        self.theme_label.configure(text=f"{'Dark' if dark else 'Light'} theme"
                                   + ("" if self.theme.available else " (install sv-ttk for the full look)"))

    # ---- traffic pane ------------------------------------------------------
    def _toggle_traffic(self):
        self.traffic_visible.set(not self.traffic_visible.get())
        self._apply_traffic()

    def _apply_traffic(self):
        show = self.traffic_visible.get()
        shown = str(self.traffic) in [str(p) for p in self.panes.panes()]
        if show and not shown:
            self.panes.add(self.traffic, weight=3)
        elif not show and shown:
            self.panes.forget(self.traffic)
        self.ctx.tap.enabled = show
        self.ctx.log(f"BLE traffic monitor {'on' if show else 'off'}")

    # ---- events ------------------------------------------------------------
    def _on_log(self, payload):
        level, text = payload
        t = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"{t}  ", "time", f"{text}\n", level)
        lines = int(self.log_text.index("end-1c").split(".")[0])
        if lines > 2000:
            self.log_text.delete("1.0", f"{lines - 2000}.0")
        self.log_text.see("end")

    def _on_link_state(self, payload):
        state, info = payload
        self.conn.show_link(state, info)
        self.status_pill.set(*STATE_PILL.get(state, (state.value, "idle")))
        if state == LinkState.CONNECTED:
            self.status_var.set(f"Connected to {info.get('name') or '?'} [{info.get('address')}]"
                                f"  ·  ATT MTU {info.get('mtu')}")
            self.ctx.log(f"connected to {info.get('name') or info.get('address')}")
            for f in self.features:
                f.on_connected(info)
        elif state == LinkState.IDLE and "reason" in info:
            self.status_var.set(f"Idle ({info['reason']})")
            self.ctx.log(info["reason"], "warn" if info["reason"] == "link lost" else "info")
            for f in self.features:
                f.on_disconnected(info["reason"])
        elif state in (LinkState.CONNECTING, LinkState.SCANNING):
            self.status_var.set(f"{state.value}…")
        else:
            self.status_var.set(state.value)

    def _on_close(self):
        if any(f.busy for f in self.features) and not messagebox.askyesno(
                "Exit", "A transfer is in progress. Abort it and exit?"):
            return
        for f in self.features:
            try:
                f.shutdown()
            except Exception:
                pass
        try:
            self.ctx.runner.submit(self.ctx.link.disconnect()).result(timeout=3)
        except Exception:
            pass
        self.ctx.runner.stop()
        self.root.destroy()
