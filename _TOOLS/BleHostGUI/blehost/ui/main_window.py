"""Main window: device panel, feature tabs, application log and the toggleable
BLE traffic pane."""

import time
import tkinter as tk
from tkinter import font as tkfont
from tkinter import messagebox, ttk

from ..core.ble_link import LinkState
from ..core.event_bus import LINK_STATE, LOG
from .connection_panel import ConnectionPanel
from .traffic_view import TrafficView

APP_TITLE = "BLE Host"


class MainWindow:
    def __init__(self, root, ctx, feature_classes):
        self.root = root
        self.ctx = ctx
        root.title(APP_TITLE)
        k = root.winfo_fpixels("1i") / 96            # display scale (1.0 at 100 %)
        w = min(int(1100 * k), int(root.winfo_screenwidth() * 0.9))
        h = min(int(820 * k), int(root.winfo_screenheight() * 0.85))
        root.geometry(f"{w}x{h}")
        root.minsize(int(800 * k), int(560 * k))
        style = ttk.Style(root)
        style.configure("Treeview", rowheight=int(tkfont.nametofont("TkDefaultFont").metrics("linespace") * 1.4))

        self.features = [cls(ctx) for cls in feature_classes]
        self.traffic_visible = tk.BooleanVar(value=False)

        self._build_menu()

        toolbar = ttk.Frame(root, padding=(6, 4, 6, 0))
        toolbar.pack(fill="x")
        ttk.Checkbutton(toolbar, text="BLE traffic  (Ctrl+T)", style="Toolbutton",
                        variable=self.traffic_visible, command=self._apply_traffic).pack(side="right")

        self.conn = ConnectionPanel(root, ctx, is_busy=lambda: any(f.busy for f in self.features))
        self.conn.pack(fill="x", padx=6, pady=4)

        self.status_var = tk.StringVar(value="Idle")
        ttk.Label(root, textvariable=self.status_var, relief="sunken", anchor="w",
                  padding=(6, 2)).pack(side="bottom", fill="x")

        self.panes = ttk.Panedwindow(root, orient="vertical")
        self.panes.pack(fill="both", expand=True, padx=6, pady=(0, 4))

        self.notebook = ttk.Notebook(self.panes)
        for f in self.features:
            self.notebook.add(f.build(self.notebook), text=f.title)
        self.panes.add(self.notebook, weight=3)

        self.log_text = self._build_log(self.panes)
        self.panes.add(self.log_text.master, weight=1)

        self.traffic = TrafficView(self.panes, ctx)

        root.bind_all("<Control-t>", lambda e: self._toggle_traffic())
        root.protocol("WM_DELETE_WINDOW", self._on_close)
        ctx.bus.subscribe(LINK_STATE, self._on_link_state)
        ctx.bus.subscribe(LOG, self._on_log)

    # ---- construction ------------------------------------------------------
    def _build_menu(self):
        m = tk.Menu(self.root)
        file_m = tk.Menu(m, tearoff=False)
        file_m.add_command(label="Exit", command=self._on_close)
        m.add_cascade(label="File", menu=file_m)
        view_m = tk.Menu(m, tearoff=False)
        view_m.add_checkbutton(label="BLE Traffic", accelerator="Ctrl+T",
                               variable=self.traffic_visible, command=self._apply_traffic)
        view_m.add_command(label="Clear Log", command=lambda: self.log_text.delete("1.0", "end"))
        m.add_cascade(label="View", menu=view_m)
        self.root.config(menu=m)

    def _build_log(self, parent):
        frame = ttk.Frame(parent)
        text = tk.Text(frame, height=6, wrap="word", font=("Consolas", 9), state="normal")
        sb = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=sb.set)
        text.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        text.tag_configure("warn", foreground="#b36b00")
        text.tag_configure("error", foreground="#c00000")
        text.bind("<Key>", lambda e: None if (e.state & 0x4 and e.keysym.lower() == "c") else "break")
        return text

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
        self.log_text.insert("end", f"{t}  {text}\n", level)
        lines = int(self.log_text.index("end-1c").split(".")[0])
        if lines > 2000:
            self.log_text.delete("1.0", f"{lines - 2000}.0")
        self.log_text.see("end")

    def _on_link_state(self, payload):
        state, info = payload
        self.conn.update_state()
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
