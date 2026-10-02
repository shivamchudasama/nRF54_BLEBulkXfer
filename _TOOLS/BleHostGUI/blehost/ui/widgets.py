"""Small display widgets shared by the panels: cards, status pills, stat tiles,
a step list and a collapsible section. Display only: they hold no state the
features depend on, and they follow the theme (ui/theme.py)."""

import tkinter as tk
from tkinter import font as tkfont
from tkinter import ttk


def card(parent, title=None, padding=(14, 10)):
    """A bordered section. Returns (frame, header); header is None without a
    title, else a frame whose column 2 is free for widgets on the right."""
    frame = ttk.Frame(parent, style="Card.TFrame", padding=padding)
    header = None
    if title:
        header = ttk.Frame(frame)
        header.pack(fill="x", pady=(0, 6))
        header.columnconfigure(1, weight=1)
        ttk.Label(header, text=title, style="Strong.TLabel").grid(row=0, column=0, sticky="w")
    return frame, header


class Pill(tk.Canvas):
    """A rounded status label: pill.set("Connected", "ok"). Tones: theme.TONES."""

    def __init__(self, parent, theme, text="", tone="idle", dot=True):
        super().__init__(parent, height=1, width=1, highlightthickness=0, borderwidth=0)
        self._theme = theme
        self._text, self._tone, self._dot = text, tone, dot
        self._font = tkfont.Font(root=self, font=theme.fonts["caption"] + ("bold",))
        theme.on_change(lambda _p: self._draw())

    @property
    def text(self) -> str:
        return self._text

    @property
    def tone(self) -> str:
        return self._tone

    def set(self, text: str, tone: str = None):
        self._text = text
        if tone:
            self._tone = tone
        self._draw()

    def _draw(self):
        fg, bg = self._theme.tone(self._tone)
        self.configure(background=self._theme.palette["bg"])
        self.delete("all")
        h = self._font.metrics("linespace") + 8
        dot = h // 3 if self._dot else 0
        pad = h // 2
        w = pad * 2 + self._font.measure(self._text) + (dot + 6 if dot else 0)
        self.configure(width=w, height=h)
        r = h // 2
        self.create_oval(0, 0, h, h, fill=bg, outline=bg)
        self.create_oval(w - h, 0, w, h, fill=bg, outline=bg)
        self.create_rectangle(r, 0, w - r, h, fill=bg, outline=bg)
        x = pad
        if dot:
            y = (h - dot) // 2
            self.create_oval(x, y, x + dot, y + dot, fill=fg, outline=fg)
            x += dot + 6
        self.create_text(x, h // 2, text=self._text, anchor="w", fill=fg, font=self._font)


class StatTile(ttk.Frame):
    """A card with a caption, a large value and a note."""

    def __init__(self, parent, caption: str, value: str = "–", note: str = ""):
        super().__init__(parent, style="Card.TFrame", padding=(14, 8))
        self.value_var = tk.StringVar(value=value)
        self.note_var = tk.StringVar(value=note)
        ttk.Label(self, text=caption.upper(), style="Caption.TLabel").pack(anchor="w")
        ttk.Label(self, textvariable=self.value_var, style="Value.TLabel").pack(anchor="w")
        ttk.Label(self, textvariable=self.note_var, style="Caption.TLabel").pack(anchor="w")

    def set(self, value: str, note: str = None):
        self.value_var.set(value)
        if note is not None:
            self.note_var.set(note)


class Stepper(tk.Canvas):
    """A vertical list of steps: done ones ticked, the current one ringed, a
    failed one crossed. stepper.show(current, state) with state one of
    "idle" (nothing run), "running", "done", "failed", "aborted"."""

    ROW = 28

    def __init__(self, parent, theme, steps):
        super().__init__(parent, highlightthickness=0, borderwidth=0,
                         height=self.ROW * len(steps), width=260)
        self._theme = theme
        self.steps = list(steps)
        self.current, self.state = 0, "idle"
        self._font = tkfont.Font(root=self, font=theme.fonts["body"])
        self._bold = tkfont.Font(root=self, font=theme.fonts["strong"])
        theme.on_change(lambda _p: self._draw())

    def show(self, current: int, state: str):
        self.current, self.state = max(0, min(current, len(self.steps) - 1)), state
        self._draw()

    def _draw(self):
        p = self._theme.palette
        self.configure(background=p["bg"])
        self.delete("all")
        d, row = 18, self.ROW
        for i, label in enumerate(self.steps):
            y = i * row + (row - d) // 2
            done = self.state == "done" or (self.state != "idle" and i < self.current)
            here = self.state != "idle" and i == self.current and self.state != "done"
            if i < len(self.steps) - 1:                  # connector to the next step
                c = p["accent"] if done else p["track"]
                self.create_line(d // 2 + 1, y + d, d // 2 + 1, y + row, fill=c, width=2)
            if done:
                self.create_oval(1, y, d + 1, y + d, fill=p["accent"], outline=p["accent"])
                self.create_line(6, y + 9, 9, y + 12, 14, y + 6, fill=p["on_accent"], width=2)
            elif here and self.state == "failed":
                fg = p["err"][0]
                self.create_oval(1, y, d + 1, y + d, fill=fg, outline=fg)
                self.create_line(6, y + 5, 14, y + 13, fill=p["bg"], width=2)
                self.create_line(14, y + 5, 6, y + 13, fill=p["bg"], width=2)
            elif here:
                c = p["warn"][0] if self.state == "aborted" else p["accent"]
                self.create_oval(1, y, d + 1, y + d, outline=c, width=2)
                self.create_oval(6, y + 5, d - 4, y + d - 5, fill=c, outline=c)
            else:
                self.create_oval(1, y, d + 1, y + d, outline=p["track"], width=2)
            self.create_text(d + 12, y + d // 2, anchor="w", text=label,
                             font=self._bold if here else self._font,
                             fill=p["fg"] if done or here else p["sub"])


class Disclosure(ttk.Frame):
    """A titled section that opens and closes: put widgets in .body."""

    def __init__(self, parent, title: str, open_: bool = False):
        super().__init__(parent)
        self._title = title
        self._open = tk.BooleanVar(value=open_)
        self.toggle_btn = ttk.Button(self, style="Link.TLabel", command=self.toggle,
                                     takefocus=True, cursor="hand2")
        self.toggle_btn.pack(anchor="w")
        self.body = ttk.Frame(self)
        self._apply()

    @property
    def is_open(self) -> bool:
        return self._open.get()

    def toggle(self):
        self._open.set(not self._open.get())
        self._apply()

    def set_open(self, open_: bool):
        self._open.set(open_)
        self._apply()

    def _apply(self):
        self.toggle_btn.configure(text=("▾  " if self.is_open else "▸  ") + self._title)
        if self.is_open:
            self.body.pack(fill="x", pady=(6, 0))
        else:
            self.body.pack_forget()
