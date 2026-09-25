#!/usr/bin/env python3
"""BLE Host: PC-side GUI for the nRF54 BLE Bulk Transfer firmware.

    pip install -r requirements.txt
    python ble_host_gui.py

See README.md for the layout and how to add a feature.
"""

import tkinter as tk

from blehost.context import AppContext
from blehost.features.hex_upload import HexUploadFeature
from blehost.protocols.bulkxfer import BulkXferService
from blehost.ui.main_window import MainWindow

# Feature tabs, in display order. Add new features here.
FEATURES = [HexUploadFeature]


def _dpi_aware():
    """Crisp text on scaled Windows displays (Tk otherwise gets bitmap-stretched)."""
    try:
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except (AttributeError, OSError):
        pass


def main():
    _dpi_aware()
    ctx = AppContext()
    ctx.services[BulkXferService.NAME] = BulkXferService(ctx)

    root = tk.Tk()
    MainWindow(root, ctx, FEATURES)
    ctx.bus.attach(root)
    ctx.runner.start()
    root.mainloop()


if __name__ == "__main__":
    main()
