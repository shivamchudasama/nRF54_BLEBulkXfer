"""Feature: one tab of the main window.

To add a capability (pairing, provisioning, ...): subclass Feature in a new
module under features/, and list the class in ble_host_gui.py. A feature
  - builds its own tab in build(),
  - reaches the device only through ctx.link / ctx.services (never bleak),
  - runs BLE work with ctx.run(coro, on_done=..., ...) and touches Tk only
    from the Tk thread (callbacks from ctx.run and the EventBus already are).
"""

from abc import ABC, abstractmethod


class Feature(ABC):
    title = "Feature"

    def __init__(self, ctx):
        self.ctx = ctx

    @abstractmethod
    def build(self, parent):
        """Create and return the tab's top widget (child of `parent`)."""

    def on_connected(self, info: dict):
        """The link is up (Tk thread). info: address, name, mtu."""

    def on_disconnected(self, reason: str):
        """The link is gone (Tk thread). Running work has already failed or been cancelled."""

    @property
    def busy(self) -> bool:
        """True while the feature has BLE work in progress."""
        return False

    def cancel(self):
        """Stop any work in progress."""

    def shutdown(self):
        """The window is closing."""
        self.cancel()
