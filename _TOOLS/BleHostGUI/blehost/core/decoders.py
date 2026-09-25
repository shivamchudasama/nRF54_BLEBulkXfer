"""Characteristic names and payload decoders for the traffic monitor.

A protocol module registers its characteristics here; the traffic pane then
shows a short name instead of the UUID and a one-line summary of each payload.
"""


class DecoderRegistry:
    def __init__(self):
        self._names = {}
        self._decoders = {}

    def register(self, uuid: str, name: str, decoder=None):
        """decoder(data: bytes) -> (kind, summary). `kind` is a short frame class
        (e.g. "DATA", "ACK") the traffic pane can filter on; "" if none."""
        uuid = uuid.lower()
        self._names[uuid] = name
        if decoder:
            self._decoders[uuid] = decoder

    def name(self, uuid: str) -> str:
        if not uuid:
            return ""
        return self._names.get(uuid, uuid[4:8] if len(uuid) == 36 else uuid)

    def decode(self, uuid: str, data: bytes) -> tuple:
        fn = self._decoders.get(uuid)
        if not fn:
            return "", ""
        try:
            return fn(data)
        except Exception as e:
            return "", f"<decode error: {e}>"
