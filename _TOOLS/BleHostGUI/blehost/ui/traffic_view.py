"""BLE traffic monitor: a terminal-style view of every TX / RX on the link.

Capture (ctx.tap.enabled) is switched here and stays on while other pages are
shown, so a transfer can be watched afterwards."""

import time
import tkinter as tk
from collections import deque
from tkinter import filedialog, ttk

from ..core.event_bus import TRAFFIC
from ..core.traffic import INFO, RX, TX
from . import theme as th
from .widgets import set_icon

MAX_EVENTS = 5000          # history kept for re-filtering; older lines are dropped
FLUSH_MS = 100
HEX_SHORT = 32             # bytes of payload shown unless "Full payload" is on
FORMATS = ("hex + decoded", "hex", "decoded")


class TrafficView(ttk.Frame):
    def __init__(self, parent, ctx, on_capture=None):
        """on_capture(on): called when capture is switched (the window's rail badge)."""
        theme = th.of(ctx)
        super().__init__(parent)
        self.ctx = ctx
        self._on_capture = on_capture
        self._events = deque(maxlen=MAX_EVENTS)
        self._pending = deque(maxlen=MAX_EVENTS)
        self._flush_scheduled = False

        bar = ttk.Frame(self)
        bar.pack(fill="x")
        self.capture = tk.BooleanVar(value=ctx.tap.enabled)
        ttk.Checkbutton(bar, text="Capture", variable=self.capture, style="Switch.TCheckbutton",
                        command=self._apply_capture).pack(side="left", padx=(0, theme.sp("m")))
        self.paused = tk.BooleanVar(value=False)
        pause = ttk.Checkbutton(bar, text="Pause", variable=self.paused, style="Toggle.TButton",
                                command=self._schedule_flush)
        clear = ttk.Button(bar, text="Clear", command=self.clear)
        save = ttk.Button(bar, text="Save…", command=self._save)
        for b, icon in ((pause, "pause"), (clear, "clear"), (save, "save")):
            b.pack(side="left", padx=theme.px(2))
            set_icon(b, theme, icon)
        self.fmt = tk.StringVar(value=FORMATS[0])
        cb = ttk.Combobox(bar, textvariable=self.fmt, values=FORMATS, state="readonly", width=14)
        cb.pack(side="right", padx=(6, 0))
        cb.bind("<<ComboboxSelected>>", lambda e: self._rerender())
        theme.on_change(lambda p: th.style_combobox(cb, p))
        ttk.Label(bar, text="Format", style="Caption.TLabel").pack(side="right")
        self.autoscroll = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="Autoscroll", variable=self.autoscroll,
                        style="Switch.TCheckbutton").pack(side="right", padx=12)

        flt = ttk.Frame(self)
        flt.pack(fill="x", pady=(6, 6))
        ttk.Label(flt, text="Show", style="Caption.TLabel").pack(side="left", padx=(0, 6))
        self.show_tx = tk.BooleanVar(value=True)
        self.show_rx = tk.BooleanVar(value=True)
        self.show_info = tk.BooleanVar(value=True)
        self.hide_data = tk.BooleanVar(value=False)
        self.hide_ack = tk.BooleanVar(value=False)
        self.full = tk.BooleanVar(value=False)
        for i, (text, var) in enumerate((("TX", self.show_tx), ("RX", self.show_rx), ("Events", self.show_info),
                                         ("Hide DATA", self.hide_data), ("Hide ACK", self.hide_ack),
                                         ("Full payload", self.full))):
            if i == 3:
                ttk.Separator(flt, orient="vertical").pack(side="left", fill="y", padx=8, pady=4)
            ttk.Checkbutton(flt, text=text, variable=var, style="Toggle.TButton",
                            command=self._rerender).pack(side="left", padx=2)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True)
        self.text = tk.Text(body, height=10, wrap="none", font=theme.fonts["mono"],
                            padx=8, pady=6, undo=False)
        ys = ttk.Scrollbar(body, orient="vertical", command=self.text.yview)
        xs = ttk.Scrollbar(body, orient="horizontal", command=self.text.xview)
        self.text.configure(yscrollcommand=ys.set, xscrollcommand=xs.set)
        self.text.grid(row=0, column=0, sticky="nsew")
        ys.grid(row=0, column=1, sticky="ns")
        xs.grid(row=1, column=0, sticky="ew")
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)
        self.text.tag_configure(TX, foreground=th.TERMINAL["tx"])
        self.text.tag_configure(RX, foreground=th.TERMINAL["rx"])
        self.text.tag_configure(INFO, foreground=th.TERMINAL["info"])
        self.text.bind("<Key>", self._readonly_key)
        theme.on_change(lambda p: th.style_text(self.text, p, terminal=True))

        ctx.bus.subscribe(TRAFFIC, self._on_event)

    # ---- capture -----------------------------------------------------------
    def set_capture(self, on: bool):
        if self.capture.get() != on:
            self.capture.set(on)
            self._apply_capture()

    def _apply_capture(self):
        on = self.capture.get()
        self.ctx.tap.enabled = on
        self.ctx.log(f"BLE traffic capture {'on' if on else 'off'}")
        if self._on_capture:
            self._on_capture(on)

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
            # A CCCD write carries the descriptor value, not a characteristic payload,
            # so it keeps its note ("subscribe" / "unsubscribe") instead of a decode
            decode = ev.uuid and ev.op != "CCCD"
            kind, summary = self.ctx.decoders.decode(ev.uuid, ev.data) if decode else ("", "")
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
