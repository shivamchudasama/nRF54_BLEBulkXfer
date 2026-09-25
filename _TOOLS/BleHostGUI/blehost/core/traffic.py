"""BLE traffic capture for the traffic monitor pane.

BleLink reports every write, read, notification and link event to a TrafficTap.
When the pane is hidden the tap is disabled and does nothing, so transfers pay
no cost for it.
"""

import time
from dataclasses import dataclass

from .event_bus import TRAFFIC

TX, RX, INFO = "TX", "RX", "--"


@dataclass(frozen=True)
class TrafficEvent:
    ts: float          # time.time()
    dir: str           # TX | RX | INFO
    op: str            # WNR, WRITE, READ, NOTIFY, CCCD, or an INFO keyword
    uuid: str          # characteristic UUID (lower case), "" for INFO
    data: bytes
    note: str = ""


class TrafficTap:
    def __init__(self, bus):
        self._bus = bus
        self.enabled = False

    def emit(self, direction: str, op: str, uuid: str = "", data: bytes = b"", note: str = ""):
        if self.enabled:
            self._bus.post(TRAFFIC, TrafficEvent(time.time(), direction, op, str(uuid).lower(),
                                                 bytes(data), note))

    def tx(self, op, uuid, data, note=""):
        self.emit(TX, op, uuid, data, note)

    def rx(self, op, uuid, data, note=""):
        self.emit(RX, op, uuid, data, note)

    def info(self, op, note):
        self.emit(INFO, op, "", b"", note)
