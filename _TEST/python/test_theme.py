# SPDX-License-Identifier: MIT
"""The GUI's look: theme switching (_TOOLS/BleHostGUI/blehost/ui/theme.py), the
display widgets (ui/widgets.py) and the mappings the tabs use to colour their
state (hex segment status, provisioning steps and device state).

The mappings and palettes need no display. The tests that create Tk widgets
are skipped where Tk has no display (headless CI).
"""

import types

import pytest

from blehost.context import AppContext
from blehost.core.ble_link import LinkState
from blehost.features import hex_upload as hu
from blehost.features import provisioning as fp
from blehost.protocols import provisioning as prov
from blehost.ui import theme as th


# ---- palettes ---------------------------------------------------------------------
def _luminance(hex_colour):
    def channel(c):
        c = int(c, 16) / 255
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (channel(hex_colour[i:i + 2]) for i in (1, 3, 5))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((_luminance(a), _luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


@pytest.mark.parametrize("mode", [th.LIGHT, th.DARK])
def test_palettes_have_every_key_and_readable_tones(mode):
    p = th.PALETTES[mode]
    assert set(p) == set(th.PALETTES[th.LIGHT])
    assert contrast(p["fg"], p["bg"]) >= 7
    assert contrast(p["sub"], p["bg"]) >= 4.5
    assert contrast(p["log_fg"], p["log_bg"]) >= 7
    for tone in th.TONES:
        fg, bg = p[tone]
        assert contrast(fg, bg) >= 4.5, f"{mode} {tone} on its pill"
        assert contrast(fg, p["bg"]) >= 4.5, f"{mode} {tone} on the window"


def test_terminal_colours_are_readable():
    t = th.TERMINAL
    for key in ("fg", "tx", "rx", "info"):
        assert contrast(t[key], t["bg"]) >= 4.5, key


def test_system_mode_is_light_or_dark():
    assert th.system_mode() in (th.LIGHT, th.DARK)


def test_system_mode_off_windows(monkeypatch):
    monkeypatch.setattr(th.sys, "platform", "linux")
    assert th.system_mode() == th.LIGHT


# ---- passive theme (a tab built on its own) ----------------------------------------
def test_of_falls_back_to_the_passive_light_theme():
    passive = th.of(types.SimpleNamespace())
    assert passive is th.of(types.SimpleNamespace(theme=None))
    assert passive.mode == th.LIGHT and passive.root is None
    own = th.Theme()
    assert th.of(types.SimpleNamespace(theme=own)) is own


def test_on_change_calls_back_at_once_and_apply_without_root_does_nothing():
    t = th.Theme(mode=th.DARK)
    seen = []
    t.on_change(seen.append)
    assert seen == [th.PALETTES[th.DARK]]
    t.apply(th.LIGHT)                     # no root: mode only, no callback
    assert t.mode == th.LIGHT and len(seen) == 1


def test_without_sv_ttk_the_theme_stays_light(monkeypatch):
    monkeypatch.setattr(th, "sv_ttk", None)
    t = th.Theme(mode=th.DARK)
    assert not t.available
    t.apply(th.DARK)
    assert t.mode == th.LIGHT


def test_unknown_mode_and_tone_fall_back():
    t = th.Theme(mode="sepia")
    assert t.mode == th.LIGHT
    assert t.tone("nonsense") == t.palette["idle"]
    assert t.tone("err") == t.palette["err"]


# ---- what the tabs show --------------------------------------------------------------
@pytest.mark.parametrize("text, tone", [
    ("pending", "idle"),
    ("sending", "info"),
    ("stored, 195 kbit/s", "ok"),
    ("aborted", "warn"),
    ("failed: NACK", "err"),
    ("STORED mismatch: ERR 0x00000000 0 B", "err"),
    ("", "idle"),
])
def test_segment_status_tone(text, tone):
    assert hu.status_tone(text) == tone


def test_every_upload_phase_has_a_pill():
    assert set(hu.PHASE_PILL) == {"nofile", "error", "ready", "uploading", "done", "failed", "aborted"}
    assert all(tone in th.TONES for _, tone in hu.PHASE_PILL.values())


def test_step_texts_match_the_session():
    """The step list follows the strings ProvisioningSession.provision() reports."""
    import inspect
    src = inspect.getsource(prov.ProvisioningSession)
    for text in fp.STEP_TEXTS:
        assert f'self.step("{text}")' in src
    assert len(fp.STEPS) == len(fp.STEP_TEXTS)


@pytest.mark.parametrize("text, current, expected", [
    ("", 3, (0, "idle")),
    ("reading device status", 0, (0, "running")),
    ("signing the CSR", 0, (2, "running")),
    ("provisioned", 4, (5, "done")),
    ("failed", 3, (3, "failed")),
    ("aborted", 1, (1, "aborted")),
    ("something else", 2, (2, "running")),
])
def test_step_position(text, current, expected):
    assert fp.step_position(text, current) == expected


@pytest.mark.parametrize("text, expected", [
    ("Device: -", ("Unknown", "idle")),
    ("Device: KEY_READY, CSR 371 B, key SHA-256 0001…", ("KEY_READY", "info")),
    ("Device: PROVISIONED, certificate serial 1a2b", ("PROVISIONED", "ok")),
    ("Device: NO_KEY, CSR 0 B, key SHA-256 00…", ("NO_KEY", "warn")),
    ("Device: 0x07, CSR 0 B", ("0x07", "idle")),
])
def test_device_pill(text, expected):
    assert fp.device_pill(text) == expected


# ---- with a display ----------------------------------------------------------------------
@pytest.fixture
def root(tk_root):
    return tk_root


def test_switching_recolours_text_and_title(root):
    import tkinter as tk
    t = th.Theme(root, th.LIGHT)
    t.apply()
    log, term = tk.Text(root), tk.Text(root)
    t.on_change(lambda p: th.style_text(log, p))
    t.on_change(lambda p: th.style_text(term, p, terminal=True))
    assert log.cget("background") == th.PALETTES[th.LIGHT]["log_bg"]
    if not t.available:
        pytest.skip("sv-ttk not installed: light only")
    t.toggle()
    root.update()
    root.after(80)
    root.update()                          # past sv-ttk's own recolouring
    assert t.mode == th.DARK
    assert log.cget("background") == th.PALETTES[th.DARK]["log_bg"]
    assert term.cget("background") == th.TERMINAL["bg"], "the terminal stays dark"
    assert log.tag_cget("error", "foreground") == th.PALETTES[th.DARK]["err"][0]


def test_a_theme_file_that_does_not_load_falls_back_to_plain_ttk(root, monkeypatch):
    import tkinter as tk
    calls = []

    def set_theme(mode, root):
        calls.append(mode)
        raise tk.TclError('cannot read file "sv.tcl"')
    monkeypatch.setattr(th, "sv_ttk", types.SimpleNamespace(set_theme=set_theme))
    monkeypatch.setattr(th.time, "sleep", lambda s: None)
    t = th.Theme(root, th.DARK)
    t.apply()
    assert calls == [th.DARK] * 3, "three tries"
    assert not t.available and t.mode == th.LIGHT
    t.apply(th.DARK)
    assert t.mode == th.LIGHT and len(calls) == 3, "not tried again"


def test_pill_stepper_tile_and_disclosure(root):
    from blehost.ui import widgets
    t = th.Theme(root, th.LIGHT)
    pill = widgets.Pill(root, t, "Idle", "idle")
    pill.set("Connected", "ok")
    assert (pill.text, pill.tone) == ("Connected", "ok")
    assert int(pill.cget("width")) > 40 and pill.find_all()
    pill.set("Connected and more")                # keeps the tone, grows
    assert pill.tone == "ok"

    st = widgets.Stepper(root, t, fp.STEPS)
    st.show(*fp.step_position("signing the CSR"))
    assert (st.current, st.state) == (2, "running")
    st.show(99, "done")
    assert st.current == len(fp.STEPS) - 1

    tile = widgets.StatTile(root, "Rate", "–", "")
    tile.set("195 kbit/s", "current segment")
    assert tile.value_var.get() == "195 kbit/s" and tile.note_var.get() == "current segment"
    tile.set("200 kbit/s")
    assert tile.note_var.get() == "current segment"

    d = widgets.Disclosure(root, "Advanced")
    assert not d.is_open
    d.toggle()
    assert d.is_open and d.toggle_btn.cget("text").endswith("Advanced")
    d.set_open(False)
    assert not d.is_open


def test_main_window_builds_and_switches(root, monkeypatch):
    from blehost.features.hex_upload import HexUploadFeature
    from blehost.features.provisioning import ProvisioningFeature
    from blehost.protocols.bulkxfer import BulkXferService
    from blehost.ui import main_window as mw

    monkeypatch.setattr(th, "system_mode", lambda: th.LIGHT)
    monkeypatch.setattr(fp.CertificateAuthority, "exists", staticmethod(lambda folder: False))
    ctx = AppContext()
    ctx.services[BulkXferService.NAME] = BulkXferService(ctx)
    ctx.services["pc_server"] = types.SimpleNamespace(available=False, subscribers=0, error="test",
                                                      receiver=None)
    win = mw.MainWindow(root, ctx, [HexUploadFeature, ProvisioningFeature])
    assert ctx.theme is win.theme and win.theme.mode == th.LIGHT
    assert win.status_pill.text == "Idle"

    ctx.link.state = LinkState.CONNECTED
    win._on_link_state((LinkState.CONNECTED, {"name": "dev", "address": "AA", "mtu": 247}))
    assert win.status_pill.tone == "ok" and win.conn.name_var.get() == "dev"
    win.conn._caps_done((2, 244, 16, 247))
    assert win.conn._facts["Window"].get() == "16"

    prov_tab = win.features[1]
    assert prov_tab.pc_pill.text == "not available"
    prov_tab.dev_var.set("Device: PROVISIONED, certificate serial 1a2b")
    assert prov_tab.dev_pill.text == "PROVISIONED"
    prov_tab.step_var.set("requesting the CSR")
    assert prov_tab.stepper.current == 1

    if win.theme.available:
        win._toggle_mode()
        assert win.theme.mode == th.DARK and win.mode_var.get() == th.DARK
        assert "Light" in win.mode_btn.cget("text")

    ctx.link.state = LinkState.IDLE
    win._on_link_state((LinkState.IDLE, {"reason": "link lost"}))
    assert win.status_pill.text == "Idle" and win.conn._facts["Window"].get() == "–"
