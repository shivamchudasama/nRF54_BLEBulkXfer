# SPDX-License-Identifier: MIT
"""The Pairing page (_TOOLS/BleHostGUI/blehost/features/pairing.py).

The page's logic (scan for the Pairing service and clear the progress pane,
choose the roles, check the chosen devices, pair, refuse devices paired with
each other, cancel, unpair, which buttons are enabled, what the pills and step
lists show) runs against stand-in Tk variables and buttons, a fake AppContext
whose run() executes the coroutine at once, and the simulated devices of
test_pairing.py behind fake links (the orchestrator itself is tested there).
One test builds the real Tk page on conftest's shared Tk root.
"""

import asyncio
import functools

import pytest

from blehost.core.ble_link import LinkState, ScanResult
from blehost.core.decoders import DecoderRegistry
from blehost.features import pairing as fpair
from blehost.protocols import pairing as pp
from test_pairing import A, B, FakeLink, SimDevice, World

C = ("D0:11:22:33:44:55", 1)


# ---- stand-ins for Tk and the context -------------------------------------------------
class Var:
    def __init__(self, value=""):
        self.value = value
        self.sets = 0

    def get(self):
        return self.value

    def set(self, value):
        self.value = value
        self.sets += 1


class Button:
    def __init__(self):
        self.enabled = True

    def state(self, spec):
        self.enabled = spec == ["!disabled"]


class Bus:
    def call(self, fn, *args):
        fn(*args)


class Future:
    def __init__(self, on_cancel=None):
        self._on_cancel = on_cancel

    def cancel(self):
        if self._on_cancel:
            self._on_cancel()


class Ctx:
    def __init__(self):
        self.bus = Bus()
        self.link = type("Link", (), {"state": LinkState.IDLE})()
        self.decoders = DecoderRegistry()
        self.settings = type("Settings", (), {"scan_timeout": 0.1})()
        self.logs = []
        self.hold = False
        self.held = None
        self.pending = []

    def log(self, text, level="info"):
        self.logs.append((level, text))

    def run(self, coro, on_done=None, on_error=None, on_cancel=None):
        if self.hold:
            coro.close()
            self.held = Future(on_cancel)
            return self.held
        try:
            result = asyncio.run(coro)
        except Exception as e:                 # noqa: BLE001 - mirrors AppContext.run
            self.pending.append(lambda exc=e: on_error(exc))
        else:
            self.pending.append(lambda: on_done(result))
        return Future()

    def flush(self):
        while self.pending:
            self.pending.pop(0)()


class ScanWorld(World):
    """World whose "scan" link reports the devices, plus one without the service."""

    def make_link(self, label):
        link = super().make_link(label)
        if label == "scan":
            devices = self.devices

            async def scan(timeout):
                found = [ScanResult(a, f"dev-{a[:2]}", -40 - i, [pp.SERVICE_UUID.upper()])
                         for i, a in enumerate(devices)]
                return found + [ScanResult("AA:AA:AA:AA:AA:AA", "other", -30, ["180a"])]
            link.scan = scan
        return link


@pytest.fixture
def fast(monkeypatch):
    """The page's orchestrator with test timings."""
    monkeypatch.setattr(fpair.pairing, "PairingOrchestrator", functools.partial(
        pp.PairingOrchestrator, timeout=1.0, fail_grace=0.1, poll=0.02, start_settle=0.0))


@pytest.fixture
def dialogs(monkeypatch):
    seen = {"errors": [], "ask": True}
    monkeypatch.setattr(fpair.messagebox, "showerror", lambda t, m: seen["errors"].append(m))
    monkeypatch.setattr(fpair.messagebox, "askyesno", lambda t, m: seen["ask"])
    return seen


def make_page(world):
    """A PairingFeature with stand-in widgets, as build() would leave it."""
    f = fpair.PairingFeature(Ctx(), make_link=world.make_link)
    for name in ("central_var", "peripheral_var", "result_var", "hint_var"):
        setattr(f, name, Var())
    f.role_vars = {"central": Var(), "peripheral": Var()}
    for name in ("scan_btn", "check_btn", "swap_btn", "pair_btn", "cancel_btn",
                 "unpair_c_btn", "unpair_p_btn"):
        setattr(f, name, Button())
    return f


@pytest.fixture
def page(fast, dialogs):
    c, p = SimDevice(A[0]), SimDevice(B[0])
    world = ScanWorld(c, p)
    f = make_page(world)
    f.world = world
    return f


def scanned(f):
    f._scan()
    f.ctx.flush()
    return f


def logged(f) -> str:
    return "\n".join(text for _, text in f.ctx.logs)


def enabled(f):
    return {n for n in ("scan", "check", "swap", "pair", "cancel", "unpair_c", "unpair_p")
            if getattr(f, f"{n}_btn").enabled}


# ---- display helpers ------------------------------------------------------------------
def test_choice_texts():
    text = fpair.choice_text("dev", A[0])
    assert text == f"dev [{A[0]}]"
    assert fpair.choice_address(text) == A[0] and fpair.choice_name(text) == "dev"
    assert fpair.choice_address(fpair.choice_text("", A[0].lower())) == A[0]
    assert fpair.choice_text("", A[0]).startswith("(no name)")
    assert fpair.choice_address("") == "" and fpair.choice_address("dev") == ""


def test_row_status():
    st = lambda **kw: pp.PairStatus(**{**dict(state=0, error=0, detail=0, prov_state=3, role=0,
                                               own=A[0], own_type=1, peer="", peer_type=0), **kw})
    assert fpair.row_status(None) == ""
    assert fpair.row_status(OSError("gone")) == "error: gone"
    assert fpair.row_status(st(prov_state=1)) == "not provisioned"
    assert fpair.row_status(st(state=7, peer=B[0])) == f"paired with {B[0]}"
    assert fpair.row_status(st(state=7)) == "paired"
    assert fpair.row_status(st(state=3)) == "pairing…"
    assert fpair.row_status(st(state=8, error=7)) == "ready (last run failed: PEER_CERT)"
    assert fpair.row_status(st()) == "ready"


@pytest.mark.parametrize("text, pill, step", [
    ("", ("Not started", "idle"), (0, "idle")),
    ("IDLE", ("Idle", "idle"), (0, "idle")),
    ("ARMED as central, peer x", ("Armed", "info"), (0, "running")),
    ("CERT_VERIFIED as central", ("Cert Verified", "info"), (3, "running")),
    ("PAIRING as peripheral", ("Pairing", "info"), (5, "running")),
    ("PAIRED as central, peer x", ("Paired", "ok"), (6, "done")),
    ("FAILED as central: cancelled", ("Failed", "err"), (2, "failed")),
])
def test_role_pill_and_steps(text, pill, step):
    assert fpair.role_pill(text) == pill
    assert fpair.step_position(text, current=2) == step


def test_result_pill():
    assert fpair.result_pill("") == ("Idle", "idle")
    assert fpair.result_pill("Running…") == ("Running", "info")
    assert fpair.result_pill("Paired: both") == ("Paired", "ok")
    assert fpair.result_pill("Cancelled (…)") == ("Cancelled", "warn")
    assert fpair.result_pill("Failed: x") == ("Failed", "err")
    assert fpair.result_pill("Not started: x") == ("Failed", "err")
    assert fpair.result_pill("Checked: central ready") == ("Checked", "info")


# ---- scan, check, roles ----------------------------------------------------------------
def test_decoders_are_registered(page):
    assert page.ctx.decoders.name(pp.STATUS_UUID) == "PAIR STATUS"


def test_scan_keeps_pairing_devices_and_proposes_roles(page):
    scanned(page)
    assert set(page._found) == {A[0], B[0]}, "only devices advertising the Pairing service"
    assert fpair.choice_address(page.central_var.get()) == A[0]
    assert fpair.choice_address(page.peripheral_var.get()) == B[0]
    assert "2 device(s) advertise the Pairing service" in logged(page)
    # A second scan keeps the user's choice
    page.central_var.set(fpair.choice_text("dev", B[0]))
    scanned(page)
    assert fpair.choice_address(page.central_var.get()) == B[0]


def test_check_reads_the_chosen_devices(page):
    page.world.devices[C[0]] = SimDevice(C[0])
    scanned(page)
    page.world.devices[B[0]].prov_state = 1
    page._check()
    page.ctx.flush()
    assert set(page._checked) == {A[0], B[0]}, "only the chosen central and peripheral"
    assert page._checked[A[0]].provisioned
    assert fpair.row_status(page._checked[B[0]]) == "not provisioned"
    assert "peripheral dev-E1: not provisioned" in logged(page)
    # Shown in the progress pane
    assert page.role_vars["central"].get() == "IDLE"
    assert page.role_vars["peripheral"].get() == "IDLE"
    assert page.result_var.get() == "Checked: central ready; peripheral not provisioned"
    assert fpair.result_pill(page.result_var.get()) == ("Checked", "info")
    # An unreachable device shows its error, the other its status
    page._found["AA:BB:CC:DD:EE:FF"] = ScanResult("AA:BB:CC:DD:EE:FF", "gone", -90, [])
    page.peripheral_var.set(fpair.choice_text("gone", "AA:BB:CC:DD:EE:FF"))
    page._check()
    page.ctx.flush()
    assert fpair.row_status(page._checked["AA:BB:CC:DD:EE:FF"]).startswith("error:")
    assert page.role_vars["peripheral"].get() == ""
    assert "peripheral error:" in page.result_var.get()
    # One role chosen: only that one
    page.peripheral_var.set("")
    page._checked.clear()
    page._check()
    page.ctx.flush()
    assert set(page._checked) == {A[0]} and page.result_var.get() == "Checked: central ready"


def test_check_shows_devices_paired_with_each_other(page):
    scanned(page)
    a, b = page.world.devices[A[0]], page.world.devices[B[0]]
    a.state, a.peer, a.peer_type, a.role = pp.State.PAIRED, B[0], 1, 1
    b.state, b.peer, b.peer_type, b.role = pp.State.PAIRED, A[0], 1, 2
    page._check()
    page.ctx.flush()
    assert page.role_vars["central"].get().startswith("PAIRED as central")
    assert page.role_vars["peripheral"].get().startswith("PAIRED as peripheral")
    assert page.result_var.get().startswith("Paired with each other already")
    assert fpair.result_pill(page.result_var.get()) == ("Paired", "ok")


def test_scan_clears_the_progress_pane(page):
    scanned(page)
    page._pair()
    page.ctx.flush()
    assert page.result_var.get().startswith("Paired")
    scanned(page)
    assert page.result_var.get() == ""
    assert page.role_vars["central"].get() == "" and page.role_vars["peripheral"].get() == ""
    assert page._outcome is None


def test_paired_devices_are_not_paired_again(page, dialogs):
    scanned(page)
    page._pair()
    page.ctx.flush()
    assert "pair" not in enabled(page), "paired with each other: unpair one first"
    assert "paired with each other already" in page.hint_var.get()
    controls = list(page.world.devices[A[0]].controls)
    page._pair()
    assert dialogs["errors"] and "paired with each other already" in dialogs["errors"][0]
    assert page.world.devices[A[0]].controls == controls, "no START"
    # Swapped roles are the same two devices
    page._swap()
    page._update()
    assert "pair" not in enabled(page)
    # Unpairing one of them allows a new run
    page._unpair("central")
    page.ctx.flush()
    assert "pair" in enabled(page) and page.hint_var.get() == ""
    page._pair()
    page.ctx.flush()
    assert page.result_var.get().startswith("Paired: both devices paired")


def test_paired_devices_found_by_another_tool_are_refused(page):
    """The page's statuses may be stale: the orchestrator checks again."""
    scanned(page)
    a, b = page.world.devices[A[0]], page.world.devices[B[0]]
    a.state, a.peer, a.peer_type = pp.State.PAIRED, B[0], 1
    b.state, b.peer, b.peer_type = pp.State.PAIRED, A[0], 1
    page._update()
    assert "pair" in enabled(page), "not checked since"
    page._pair()
    page.ctx.flush()
    assert page.result_var.get().startswith("Not started: the devices are paired with each other")
    assert a.controls == [] and b.controls == []


def test_swap(page):
    scanned(page)
    page._swap()
    assert fpair.choice_address(page.central_var.get()) == B[0]
    assert fpair.choice_address(page.peripheral_var.get()) == A[0]


def test_buttons(page):
    page._update()
    assert enabled(page) == {"scan", "swap"}, "nothing to check or pair yet"
    scanned(page)
    page._update()
    assert enabled(page) == {"scan", "check", "swap", "pair", "unpair_c", "unpair_p"}

    # The Device page's link must be idle: this page makes its own links
    page.ctx.link.state = LinkState.CONNECTED
    page._update()
    assert enabled(page) == set()
    assert "Disconnect on the Device page first" in page.hint_var.get()
    page.ctx.link.state = LinkState.IDLE

    # The same device twice
    page.peripheral_var.set(page.central_var.get())
    page._update()
    assert "pair" not in enabled(page) and page.hint_var.get() == "Choose two different devices."

    # Busy: only Cancel
    page._swap()
    page.peripheral_var.set(fpair.choice_text("dev", B[0]))
    page.ctx.hold = True
    page._pair()
    assert enabled(page) == {"cancel"}


# ---- pair, cancel, unpair ----------------------------------------------------------------
def test_pair(page):
    scanned(page)
    page._pair()
    assert page.result_var.get() == "Running…"
    page.ctx.flush()
    assert page.result_var.get().startswith("Paired: both devices paired")
    assert page.role_vars["central"].get().startswith("PAIRED as central")
    assert page.role_vars["peripheral"].get().startswith("PAIRED as peripheral")
    assert page._checked[A[0]].paired and page._checked[B[0]].paired
    assert "pairing dev-C4 (central) with dev-E1 (peripheral)" in logged(page)
    assert "paired; the devices keep their encrypted link" in logged(page)
    assert not page.busy


def test_pair_shows_both_devices_live(page):
    """Each STATUS notification reaches its role's step list while pairing."""
    scanned(page)
    seen = []
    page.role_vars["central"].set = lambda v: seen.append(v)
    page._pair()
    page.ctx.flush()
    states = [v.split(" ")[0] for v in seen]
    assert states[0] == "" and "ARMED" in states and "CERT_EXCHANGE" in states and states[-1] == "PAIRED"


def test_pair_failure(page):
    scanned(page)
    c, p = page.world.devices[A[0]], page.world.devices[B[0]]
    c.outcome, p.outcome = ("fail", pp.Error.PEER_CERT, 5), "lost"
    c.peer_dev, p.peer_dev = p, c
    page._pair()
    page.ctx.flush()
    assert page.result_var.get().startswith("Failed: central: the peer's certificate was rejected")
    assert page.role_vars["central"].get().startswith("FAILED")
    assert ("error", "pairing: failed: central: the peer's certificate was rejected (BAD_SIG); "
                     "peripheral: the link between the devices dropped (detail 0x08)") in page.ctx.logs


def test_pair_not_started(page):
    scanned(page)
    page.world.devices[B[0]].prov_state = 2
    page._pair()
    page.ctx.flush()
    assert page.result_var.get() == "Not started: the peripheral device is not provisioned"
    assert "pair failed: the peripheral device is not provisioned" in logged(page)
    assert not page.busy


def test_pair_needs_two_devices(page, dialogs):
    page._pair()
    assert dialogs["errors"] and "two different devices" in dialogs["errors"][0]
    scanned(page)
    page.peripheral_var.set(page.central_var.get())
    page._pair()
    assert len(dialogs["errors"]) == 2


def test_cancel(page):
    scanned(page)
    page.ctx.hold = True
    page._pair()
    assert page.busy
    page.cancel()
    assert page.result_var.get().startswith("Cancelled")
    assert not page.busy
    page.cancel()                                   # nothing running: no effect


def test_unpair(page, dialogs):
    scanned(page)
    dev = page.world.devices[B[0]]
    dev.state, dev.peer, dev.peer_type = pp.State.PAIRED, A[0], 1
    dialogs["ask"] = False
    page._unpair("peripheral")
    page.ctx.flush()
    assert dev.controls == [], "not confirmed"
    dialogs["ask"] = True
    page._unpair("peripheral")
    page.ctx.flush()
    assert dev.controls == [pp.encode_unpair()]
    assert page.role_vars["peripheral"].get() == "IDLE"
    assert "dev-E1 unpaired: IDLE" in logged(page)
    page.central_var.set("")
    page._unpair("central")                         # no role chosen: nothing
    assert page.world.devices[A[0]].controls == []


def test_link_hooks_refresh_the_buttons(page):
    scanned(page)
    page.ctx.link.state = LinkState.CONNECTED
    page.on_connected({})
    assert "pair" not in enabled(page)
    page.ctx.link.state = LinkState.IDLE
    page.on_disconnected("gone")
    assert "pair" in enabled(page)


# ---- the real page --------------------------------------------------------------------------
def test_build_real_page(tk_root, fast):
    world = ScanWorld(SimDevice(A[0]), SimDevice(B[0]), SimDevice(C[0]))
    f = fpair.PairingFeature(Ctx(), make_link=world.make_link)
    frame = f.build(tk_root)                        # passive theme (theme.of)
    assert frame.winfo_children()
    assert "disabled" in f.pair_btn.state() and "disabled" in f.check_btn.state()
    scanned(f)
    assert len(f.tree.get_children()) == 3
    assert len(f._combos[0].cget("values")) == 3
    assert f.found_pill.text == "3 found"
    assert "disabled" not in f.pair_btn.state()
    f.role_vars["central"].set("CERT_EXCHANGE as central, peer x")
    assert f.role_pills["central"].text == "Cert Exchange"
    assert (f.steppers["central"].current, f.steppers["central"].state) == (2, "running")
    f.result_var.set("Paired: both")
    assert f.result_pill.text == "Paired"
