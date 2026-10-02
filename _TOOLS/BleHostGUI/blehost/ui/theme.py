"""Look and feel: the sv-ttk (Sun Valley, Windows 11) theme in light or dark mode,
plus the few styles and colours the panels share.

    theme = Theme(root)          # MainWindow does this and puts it in ctx.theme
    theme.apply("dark")          # or theme.toggle()
    theme.on_change(fn)          # fn(palette) now and after every switch

ttk widgets follow the theme by themselves. Classic Tk widgets (Text, Canvas)
and Treeview row tags do not: a panel registers an on_change callback that
recolours them. Without sv-ttk installed the GUI still runs on the platform's
ttk theme, in light mode only.

A panel built without a window theme (a test building one tab) gets the
passive light theme from of(ctx): on_change still calls back once, apply does
nothing.
"""

import sys
import time
import tkinter as tk
from tkinter import font as tkfont
from tkinter import ttk

try:
    import sv_ttk
except ImportError:                                  # optional: plain ttk without it
    sv_ttk = None

LIGHT, DARK = "light", "dark"

# Status tones: (foreground, background) per mode. Foregrounds keep 4.5:1 on
# their background and on the window background.
PALETTES = {
    LIGHT: {
        "bg": "#fafafa", "fg": "#1c1c1c", "sub": "#5d5d5d", "border": "#e5e5e5",
        "accent": "#005fb8", "on_accent": "#ffffff", "track": "#d6d6d6",
        "log_bg": "#ffffff", "log_fg": "#1c1c1c",
        "ok": ("#0e700e", "#dff6dd"), "info": ("#005fb8", "#e5f0fb"),
        "warn": ("#8a5300", "#fff4ce"), "err": ("#c42b1c", "#fde7e9"),
        "idle": ("#5d5d5d", "#ebebeb"),
    },
    DARK: {
        "bg": "#1c1c1c", "fg": "#fafafa", "sub": "#c5c5c5", "border": "#3a3a3a",
        "accent": "#57c8ff", "on_accent": "#000000", "track": "#4a4a4a",
        "log_bg": "#232323", "log_fg": "#e6e6e6",
        "ok": ("#6ccb5f", "#1f3a1d"), "info": ("#99ebff", "#0e3a4f"),
        "warn": ("#fce100", "#433519"), "err": ("#ff99a4", "#442726"),
        "idle": ("#c5c5c5", "#2d2d2d"),
    },
}
TONES = ("ok", "info", "warn", "err", "idle")

# The traffic monitor stays a dark terminal in both modes
TERMINAL = {"bg": "#1e1e1e", "fg": "#d4d4d4", "tx": "#4fc1ff", "rx": "#b5cea8", "info": "#9d9d9d"}

UI = ("Segoe UI Variable Text", "Segoe UI", "Cantarell", "DejaVu Sans")
MONO = ("Cascadia Mono", "Consolas", "DejaVu Sans Mono", "Courier New")


def system_mode() -> str:
    """Windows' app mode (Settings > Personalization > Colors); light elsewhere."""
    if sys.platform != "win32":
        return LIGHT
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                            r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize") as k:
            return LIGHT if winreg.QueryValueEx(k, "AppsUseLightTheme")[0] else DARK
    except OSError:
        return LIGHT


def _family(root, names, default):
    try:
        have = set(tkfont.families(root))
    except tk.TclError:
        return default
    return next((n for n in names if n in have), default)


class Theme:
    def __init__(self, root=None, mode: str = LIGHT):
        self.root = root
        self.mode = mode if mode in PALETTES else LIGHT
        self._listeners = []
        self._sv_failed = False
        ui = _family(root, UI, "TkDefaultFont") if root else "Segoe UI"
        mono = _family(root, MONO, "TkFixedFont") if root else "Consolas"
        self.fonts = {
            "body": (ui, 10), "caption": (ui, 9), "strong": (ui, 10, "bold"),
            "title": (ui, 12, "bold"), "value": (ui, 15, "bold"), "mono": (mono, 9),
        }

    @property
    def available(self) -> bool:
        """True when sv-ttk is installed and loads (dark mode needs it)."""
        return sv_ttk is not None and not self._sv_failed

    @property
    def palette(self) -> dict:
        return PALETTES[self.mode]

    def tone(self, name: str):
        """(foreground, background) of a status tone."""
        return self.palette.get(name, self.palette["idle"])

    # ---- switching -----------------------------------------------------------
    def apply(self, mode: str = None):
        """Switch to mode (default: the current one). Tk thread."""
        if mode in PALETTES:
            self.mode = mode
        if not self.available:
            self.mode = LIGHT
        if self.root is None:
            return
        if self.available:
            self._set_sv_theme()
        self._configure_styles()
        self._title_bar()
        self._notify()
        # sv-ttk recolours classic widgets (tk_setPalette) when Tk delivers
        # <<ThemeChanged>>, after this returns: paint them again afterwards
        self.root.after(50, self._notify)

    def _set_sv_theme(self):
        """Tcl sometimes fails to read the theme file on a first try (Windows
        reports "couldn't read file ... No error"); after three tries the GUI
        carries on with the platform's ttk theme, in light mode."""
        for attempt in range(3):
            try:
                sv_ttk.set_theme(self.mode, self.root)
                return
            except tk.TclError as e:
                error = e
                time.sleep(0.05 * (attempt + 1))
        self._sv_failed = True
        self.mode = LIGHT
        print(f"sv-ttk theme not loaded ({error}); using the plain ttk theme", file=sys.stderr)

    def toggle(self):
        self.apply(DARK if self.mode == LIGHT else LIGHT)

    def on_change(self, fn):
        """fn(palette) now and after every switch. Tk thread."""
        self._listeners.append(fn)
        fn(self.palette)

    def _notify(self):
        for fn in list(self._listeners):
            try:
                fn(self.palette)
            except tk.TclError:                      # widget already destroyed
                self._listeners.remove(fn)

    # ---- styles ----------------------------------------------------------------
    def _configure_styles(self):
        """ttk styles are per theme, so this runs after every switch."""
        p, f = self.palette, self.fonts
        s = ttk.Style(self.root)
        if not self.available:
            s.configure("Card.TFrame", borderwidth=1, relief="solid")
        s.configure("Caption.TLabel", foreground=p["sub"], font=f["caption"])
        s.configure("Strong.TLabel", font=f["strong"])
        s.configure("Title.TLabel", font=f["title"])
        s.configure("Value.TLabel", font=f["value"])
        s.configure("Mono.TLabel", font=f["mono"])
        s.configure("MonoCaption.TLabel", foreground=p["sub"], font=f["mono"])
        s.configure("Link.TLabel", foreground=p["accent"], font=f["body"])
        s.configure("Danger.TButton", foreground=p["err"][0])
        line = tkfont.Font(root=self.root, font=f["body"]).metrics("linespace")
        s.configure("Treeview", rowheight=int(line * 1.9))

    def _title_bar(self):
        """Windows 10 20H1+ / 11: dark caption bar in dark mode."""
        if sys.platform != "win32":
            return
        try:
            import ctypes
            self.root.update_idletasks()
            hwnd = ctypes.windll.user32.GetParent(self.root.winfo_id())
            value = ctypes.c_int(1 if self.mode == DARK else 0)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(value),
                                                       ctypes.sizeof(value))
        except (AttributeError, OSError, tk.TclError):
            pass


_PASSIVE = Theme()


def of(ctx) -> Theme:
    """The window's theme, or the passive light one (a tab built on its own)."""
    return getattr(ctx, "theme", None) or _PASSIVE


def style_text(widget: tk.Text, palette: dict, terminal: bool = False):
    """Colours of a read-only log Text, and its warn / error tags."""
    if terminal:
        widget.configure(background=TERMINAL["bg"], foreground=TERMINAL["fg"],
                         insertbackground=TERMINAL["fg"], selectbackground="#264f78",
                         highlightthickness=0, borderwidth=0)
        return
    widget.configure(background=palette["log_bg"], foreground=palette["log_fg"],
                     insertbackground=palette["log_fg"], highlightthickness=1,
                     highlightbackground=palette["border"], highlightcolor=palette["border"],
                     borderwidth=0)
    widget.tag_configure("warn", foreground=palette["warn"][0])
    widget.tag_configure("error", foreground=palette["err"][0])
    widget.tag_configure("time", foreground=palette["sub"])
