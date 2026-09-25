"""asyncio event loop on a background thread.

All BLE work (bleak) runs on this loop. The Tk main thread hands it coroutines
with submit() and never blocks on them; results come back through the EventBus.
Keeping bleak off the Tk thread also keeps its WinRT backend out of the STA
apartment that Tk initialises, which is where bleak can hang on Windows.
"""

import asyncio
import concurrent.futures
import threading


class AsyncRunner:
    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._run, name="ble-asyncio", daemon=True)

    def start(self):
        self._thread.start()

    def _run(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def submit(self, coro) -> concurrent.futures.Future:
        """Schedule a coroutine from any thread. Cancelling the returned future
        cancels the task on the loop."""
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def call_soon(self, fn, *args):
        self.loop.call_soon_threadsafe(fn, *args)

    def stop(self, timeout: float = 2.0):
        if self._thread.is_alive():
            self.loop.call_soon_threadsafe(self.loop.stop)
            self._thread.join(timeout)
