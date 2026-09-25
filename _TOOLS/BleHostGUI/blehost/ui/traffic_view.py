"""BLE traffic monitor: a terminal-style view of every TX / RX on the link."""

import time
import tkinter as tk
from collections import deque
from tkinter import filedialog, ttk

from ..core.event_bus import TRAFFIC
from ..core.traffic import INFO, RX, TX

MAX_EVENTS = 5000          # history kept for re-filtering; older lines are dropped
FLUSH_MS = 100
HEX_SHORT = 32             # bytes of payload shown unless "Full payload" is on
FORMATS = ("hex + decoded", "hex", "decoded")


class TrafficView(ttk.Frame):
    def __init__(self, parent, ctx):
        super().__init__(parent)
        self.ctx = ctx
        self._events = deque(maxlen=MAX_EVENTS)
        self._pending = deque(maxlen=MAX_EVENTS)
        self._flush_scheduled = False

        bar = ttk.Frame(self, padding=(4, 2))
        bar.pack(fill="x")
        ttk.Label(bar, text="BLE traffic", font=("Segoe UI", 9, "bold")).pack(side="left", padx=(0, 8))
        self.paused = tk.BooleanVar(value=False)
        ttk.Checkbutton(bar, text="Pause", variable=self.paused, style="Toolbutton",
                        command=self._schedule_flush).pack(side="left", padx=2)
        ttk.Button(bar, text="Clear", command=self.clear).pack(side="left", padx=2)
        ttk.Button(bar, text="Save…", command=self._save).pack(side="left", padx=2)
        self.autoscroll = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="Autoscroll", variable=self.autoscroll).pack(side="left", padx=8)
        self.fmt = tk.StringVar(value=FORMATS[0])
        cb = ttk.Combobox(bar, textvariable=self.fmt, values=FORMATS, state="readonly", width=14)
        cb.pack(side="left", padx=2)
        cb.bind("<<ComboboxSelected>>", lambda e: self._rerender())

        flt = ttk.Frame(self, padding=(4, 0, 4, 2))
        flt.pack(fill="x")
        ttk.Label(flt, text="Show").pack(side="left", padx=(0, 4))
        self.show_tx = tk.BooleanVar(value=True)
        self.show_rx = tk.BooleanVar(value=True)
        self.show_info = tk.BooleanVar(value=True)
        self.hide_data = tk.BooleanVar(value=False)
        self.hide_ack = tk.BooleanVar(value=False)
        self.full = tk.BooleanVar(value=False)
        for text, var in (("TX", self.show_tx), ("RX", self.show_rx), ("Events", self.show_info),
                          ("Hide DATA", self.hide_data), ("Hide ACK", self.hide_ack),
                          ("Full payload", self.full)):
            ttk.Checkbutton(flt, text=text, variable=var, command=self._rerender).pack(side="left", padx=2)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True)
        self.text = tk.Text(body, height=10, wrap="none", font=("Consolas", 9),
                            background="#1e1e1e", foreground="#d4d4d4", insertbackground="#d4d4d4",
                            undo=False)
        ys = ttk.Scrollbar(body, orient="vertical", command=self.text.yview)
        xs = ttk.Scrollbar(body, orient="horizontal", command=self.text.xview)
        self.text.configure(yscrollcommand=ys.set, xscrollcommand=xs.set)
        self.text.grid(row=0, column=0, sticky="nsew")
        ys.grid(row=0, column=1, sticky="ns")
        xs.grid(row=1, column=0, sticky="ew")
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)
        self.text.tag_configure(TX, foreground="#4fc1ff")
        self.text.tag_configure(RX, foreground="#b5cea8")
        self.text.tag_configure(INFO, foreground="#9d9d9d")
        self.text.bind("<Key>", self._readonly_key)

        ctx.bus.subscribe(TRAFFIC, self._on_event)

    # ---- capture -----------------------------------------------------------
    def _on_event(self, ev):
        self._events.append(ev)
        self._pending.append(ev)
        self._schedule_flush()

    def _schedule_flush(self):
        if not self._flush_scheduled:
            self._flush_scheduled = True
            self.after(FLUSH_MS, self._flush)

    def _flush(self):
        self._flush_scheduled = False
        if self.paused.get() or not self._pending:
            return
        evs = list(self._pending)
        self._pending.clear()
        self._insert(evs)

    # ---- rendering ---------------------------------------------------------
    def _visible(self, ev, kind) -> bool:
        if ev.dir == TX and not self.show_tx.get():
            return False
        if ev.dir == RX and not self.show_rx.get():
            return False
        if ev.dir == INFO and not self.show_info.get():
            return False
        if kind == "DATA" and self.hide_data.get():
            return False
        if kind == "ACK" and self.hide_ack.get():
            return False
        return True

    def _format(self, ev, summary) -> str:
        t = time.strftime("%H:%M:%S", time.localtime(ev.ts)) + f".{int(ev.ts * 1000) % 1000:03d}"
        if ev.dir == INFO:
            return f"{t}  --  {ev.op:<10} {ev.note}\n"
        name = self.ctx.decoders.name(ev.uuid)
        fmt = self.fmt.get()
        data = ev.data
        parts = [f"{t}  {ev.dir}  {name:<5} {ev.op:<6} {len(data):>3}"]
        if fmt != "decoded" or not summary:
            shown = data if self.full.get() else data[:HEX_SHORT]
            h = shown.hex(" ")
            if len(shown) < len(data):
                h += f" … (+{len(data) - len(shown)})"
            parts.append(h)
        if fmt != "hex" and (summary or ev.note):
            parts.append("| " + (summary or ev.note))
        return "  ".join(parts) + "\n"

    def _insert(self, evs):
        args = []
        for ev in evs:
            kind, summary = self.ctx.decoders.decode(ev.uuid, ev.data) if ev.uuid else ("", "")
            if self._visible(ev, kind):
                args += [self._format(ev, summary), ev.dir]
        if not args:
            return
        self.text.insert("end", *args)
        lines = int(self.text.index("end-1c").split(".")[0])
        if lines > MAX_EVENTS:
            self.text.delete("1.0", f"{lines - MAX_EVENTS}.0")
        if self.autoscroll.get():
            self.text.see("end")

    def _rerender(self):
        self.text.delete("1.0", "end")
        self._pending.clear()
        self._insert(list(self._events))

    def clear(self):
        self._events.clear()
        self._pending.clear()
        self.text.delete("1.0", "end")

    def _save(self):
        path = filedialog.asksaveasfilename(title="Save BLE traffic", defaultextension=".txt",
                                            filetypes=[("Text", "*.txt"), ("All files", "*.*")])
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.text.get("1.0", "end-1c"))
            self.ctx.log(f"traffic saved to {path}")

    def _readonly_key(self, e):
        # allow copy / select-all / navigation, block editing
        if (e.state & 0x4) and e.keysym.lower() == "a":
            self.text.tag_add("sel", "1.0", "end")
            return "break"
        if (e.state & 0x4) and e.keysym.lower() == "c":
            return None
        if e.keysym in ("Up", "Down", "Left", "Right", "Prior", "Next", "Home", "End"):
            return None
        return "break"
