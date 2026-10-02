"""Provisioning: act as the Certificate Authority and provision the connected
device (_DOC/Provisioning/PROTOCOL.md).

The tab holds a CA (create or load a folder), reads the device's STATUS, and
runs the whole sequence: CSR from the device (over the PC's own BulkXfer
service, core/gatt_server.py), sign it, send the CA certificate and the device
certificate, each verified by the device, which then stores both. A provisioned
device refuses another run: "Remove provisioning" wipes it (DEPROVISION) and it
makes a fresh key and CSR.
"""

import os
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from cryptography.hazmat.primitives import serialization

from ..core.gatt_server import PcGattServer
from ..pki.authority import DEFAULT_FOLDER, CaError, CertificateAuthority
from ..protocols import bulkxfer, provisioning as prov
from ..ui import theme as th
from ..ui.widgets import Disclosure, Pill, Stepper, card
from .base import Feature

# The steps ProvisioningSession.provision() reports, and their labels in the list
STEP_TEXTS = ("reading device status", "requesting the CSR", "signing the CSR",
              "sending the CA certificate", "sending the device certificate", "provisioned")
STEPS = ("Read device status", "Request the CSR", "Sign the CSR",
         "Send the CA certificate", "Send the device certificate", "Provisioned")

# Device state (DeviceStatus.state_name) -> pill tone
STATE_TONE = {"PROVISIONED": "ok", "KEY_READY": "info", "CA_OK": "info", "NO_KEY": "warn"}


def step_position(text: str, current: int = 0):
    """(index, state) of the step list for the tab's step text."""
    if not text:
        return 0, "idle"
    if text in STEP_TEXTS:
        i = STEP_TEXTS.index(text)
        return i, "done" if i == len(STEP_TEXTS) - 1 else "running"
    if text in ("failed", "aborted"):
        return current, text
    return current, "running"


def device_pill(text: str):
    """(pill text, tone) for the tab's "Device: ..." line."""
    state = text.removeprefix("Device:").strip().split(",")[0].strip()
    if not state or state == "-":
        return "Unknown", "idle"
    return state, STATE_TONE.get(state, "idle")


class ProvisioningFeature(Feature):
    title = "Provisioning"

    def __init__(self, ctx):
        super().__init__(ctx)
        self.ca = None
        self._future = None
        self._outcome = None

    # ---- UI ----------------------------------------------------------------
    def build(self, parent):
        theme = th.of(self.ctx)
        f = ttk.Frame(parent, padding=(12, 12, 12, 10))

        # PC service (the device sends the CSR to it)
        pcc, _ = card(f, None, padding=(14, 8))
        pcc.pack(fill="x")
        ttk.Label(pcc, text="PC BulkXfer service", style="Strong.TLabel").pack(side="left")
        self.pc_pill = Pill(pcc, theme, "starting", "idle")
        self.pc_pill.pack(side="left", padx=10)
        self.pc_var = tk.StringVar()
        self._pc_note = tk.StringVar()
        ttk.Label(pcc, textvariable=self._pc_note, style="Caption.TLabel").pack(side="left")

        cols = ttk.Frame(f)
        cols.pack(fill="both", expand=True, pady=(10, 0))
        cols.columnconfigure((0, 1), weight=1, uniform="col")
        cols.rowconfigure(0, weight=1)

        # Certificate Authority
        ca, head = card(cols, "Certificate Authority (this PC)")
        ca.grid(row=0, column=0, sticky="nsew", padx=(0, 5))
        self.ca_pill = Pill(head, theme, "No CA", "warn")
        self.ca_pill.grid(row=0, column=2, sticky="e")
        self.ca_var = tk.StringVar(value="No CA loaded")
        self._ca_label = ttk.Label(ca, textvariable=self.ca_var, wraplength=360, justify="left")
        self._ca_label.pack(anchor="w", fill="x")
        ca.bind("<Configure>", lambda e: self._ca_label.configure(wraplength=max(200, e.width - 40)))
        ttk.Label(ca, text="Folder", style="Caption.TLabel").pack(anchor="w", pady=(10, 2))
        row = ttk.Frame(ca)
        row.pack(fill="x")
        self.folder_var = tk.StringVar(value=DEFAULT_FOLDER)
        ttk.Entry(row, textvariable=self.folder_var).pack(side="left", fill="x", expand=True)
        ttk.Button(row, text="Browse…", command=self._browse).pack(side="left", padx=(6, 0))
        ttk.Button(row, text="Load", command=self._load_ca).pack(side="left", padx=(6, 0))

        self._new_ca = Disclosure(ca, "Create a new CA")
        self._new_ca.pack(fill="x", pady=(10, 0))
        new = self._new_ca.body
        new.columnconfigure(1, weight=1)
        self.cn_var = tk.StringVar(value="BLE Host Provisioning CA")
        self.o_var = tk.StringVar(value="Bajaj Auto Technology Limited")
        self.c_var = tk.StringVar(value="IN")
        self.ca_days_var = tk.StringVar(value="3650")
        for i, (label, var, width) in enumerate((("CN", self.cn_var, 26), ("O", self.o_var, 26),
                                                 ("C", self.c_var, 4), ("Valid (days)", self.ca_days_var, 6))):
            ttk.Label(new, text=label).grid(row=i, column=0, sticky="w", pady=2)
            ttk.Entry(new, textvariable=var, width=width).grid(row=i, column=1, sticky="w" if width < 10 else "ew",
                                                               padx=(8, 0), pady=2)
        ttk.Button(new, text="Create CA", command=self._create_ca).grid(row=4, column=1, sticky="w",
                                                                        padx=(8, 0), pady=(6, 0))

        # Device
        dev, head = card(cols, "Device")
        dev.grid(row=0, column=1, sticky="nsew", padx=(5, 0))
        self.dev_pill = Pill(head, theme, "Unknown", "idle")
        self.dev_pill.grid(row=0, column=2, sticky="e")
        self.dev_var = tk.StringVar(value="Device: -")
        self._dev_label = ttk.Label(dev, textvariable=self.dev_var, style="Caption.TLabel",
                                    wraplength=360, justify="left")
        self._dev_label.pack(anchor="w", fill="x")
        dev.bind("<Configure>", lambda e: self._dev_label.configure(wraplength=max(200, e.width - 40)))
        self.stepper = Stepper(dev, theme, STEPS)
        self.stepper.pack(anchor="w", fill="x", pady=(10, 4))
        self.step_var = tk.StringVar()
        ttk.Label(dev, textvariable=self.step_var, style="Caption.TLabel").pack(anchor="w")

        days = ttk.Frame(dev)
        days.pack(fill="x", pady=(10, 0))
        ttk.Label(days, text="Device certificate valid (days)").pack(side="left")
        self.dev_days_var = tk.StringVar(value="365")
        ttk.Entry(days, textvariable=self.dev_days_var, width=6).pack(side="left", padx=(8, 0))
        btns = ttk.Frame(dev)
        btns.pack(fill="x", pady=(8, 0))
        self.prov_btn = ttk.Button(btns, text="Provision", style="Accent.TButton", command=self._provision)
        self.prov_btn.pack(side="left")
        self.status_btn = ttk.Button(btns, text="Get Status", command=self._get_status)
        self.status_btn.pack(side="left", padx=6)
        self.abort_btn = ttk.Button(btns, text="Abort", command=self.cancel)
        self.abort_btn.pack(side="left")
        btns2 = ttk.Frame(dev)
        btns2.pack(fill="x", pady=(8, 0))
        btns2.columnconfigure((0, 1), weight=1, uniform="b")
        self.save_btn = ttk.Button(btns2, text="Save device certificate…", command=self._save_cert)
        self.save_btn.grid(row=0, column=0, sticky="ew", padx=(0, 3))
        self.deprov_btn = ttk.Button(btns2, text="Remove provisioning…", style="Danger.TButton",
                                     command=self._deprovision)
        self.deprov_btn.grid(row=0, column=1, sticky="ew", padx=(3, 0))

        # Log
        logc, _ = card(f, "Provisioning log", padding=(12, 8))
        logc.pack(fill="both", expand=True, pady=(10, 0))
        body = ttk.Frame(logc)
        body.pack(fill="both", expand=True)
        self.log_text = tk.Text(body, height=5, wrap="word", state="disabled", font=theme.fonts["mono"],
                                padx=8, pady=6)
        self.log_text.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(body, orient="vertical", command=self.log_text.yview)
        sb.pack(side="right", fill="y")
        self.log_text.configure(yscrollcommand=sb.set)
        theme.on_change(lambda p: th.style_text(self.log_text, p))

        # The pills and the step list follow the text the logic writes
        self.pc_var.trace_add("write", lambda *a: self._show_pc_pill())
        self.ca_var.trace_add("write", lambda *a: self._show_ca_pill())
        self.dev_var.trace_add("write", lambda *a: self.dev_pill.set(*device_pill(self.dev_var.get())))
        self.step_var.trace_add("write", lambda *a: self.stepper.show(
            *step_position(self.step_var.get(), self.stepper.current)))

        if CertificateAuthority.exists(self.folder_var.get()):
            self._load_ca(quiet=True)
        self._show_ca_pill()
        self._update()
        f.after(1000, self._tick)
        return f

    # ---- display (pills, step list) -----------------------------------------
    def _show_pc_pill(self):
        text = self.pc_var.get()
        if text.startswith("PC BulkXfer service: published"):
            sub = "subscribed" in text
            self.pc_pill.set("device subscribed" if sub else "published", "ok")
            self._pc_note.set("The device sends its CSR to this service.")
        else:
            self.pc_pill.set("not available", "warn")
            self._pc_note.set(text.replace("PC BulkXfer service: not available ", ""))

    def _show_ca_pill(self):
        loaded = self.ca is not None
        self.ca_pill.set("Loaded" if loaded else "No CA", "ok" if loaded else "warn")
        if not loaded and not self._new_ca.is_open:
            self._new_ca.set_open(True)

    def _update(self):
        busy = self.busy
        ready = self.ctx.link.connected and self._bulkxfer().available and not busy
        self.status_btn.state(["!disabled"] if ready else ["disabled"])
        self.deprov_btn.state(["!disabled"] if ready else ["disabled"])
        self.prov_btn.state(["!disabled"] if ready and self.ca and self._pc().available else ["disabled"])
        self.abort_btn.state(["!disabled"] if busy else ["disabled"])
        self.save_btn.state(["!disabled"] if self._outcome and not busy else ["disabled"])
        self._show_pc()

    def _show_pc(self):
        pc = self._pc()
        if pc.available:
            subs = ", device subscribed" if pc.subscribers else ""
            self.pc_var.set(f"PC BulkXfer service: published{subs}")
        else:
            self.pc_var.set(f"PC BulkXfer service: not available ({pc.error or 'starting'}): "
                            "the CSR cannot be received")

    def _tick(self):
        """The PC service starts and the device subscribes asynchronously."""
        self._show_pc()
        self.prov_btn.after(1000, self._tick)

    def _log(self, text: str, level: str = "info"):
        self.ctx.log(f"provisioning: {text}", level)
        self.log_text.configure(state="normal")
        self.log_text.insert("end", text + "\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def _bulkxfer(self):
        return self.ctx.services[bulkxfer.BulkXferService.NAME]

    def _pc(self) -> PcGattServer:
        return self.ctx.services[PcGattServer.NAME]

    # ---- CA ----------------------------------------------------------------
    def _browse(self):
        path = filedialog.askdirectory(title="CA folder", initialdir=self.folder_var.get())
        if path:
            self.folder_var.set(path)

    def _show_ca(self):
        ca = self.ca
        self.ca_var.set(f"{ca.subject}\nSHA-256 {ca.fingerprint}\n"
                        f"valid until {ca.not_after:%Y-%m-%d}, {len(ca.issued())} certificate(s) issued")

    def _load_ca(self, quiet=False):
        try:
            self.ca = CertificateAuthority.load(self.folder_var.get())
        except (CaError, ValueError, OSError) as e:
            self.ca = None
            self.ca_var.set("No CA loaded")
            if not quiet:
                messagebox.showerror("Certificate Authority", str(e))
            self._update()
            return
        self._show_ca()
        self._log(f"CA loaded from {self.folder_var.get()}")
        self._update()

    def _create_ca(self):
        subject = {"CN": self.cn_var.get().strip(), "O": self.o_var.get().strip(),
                   "C": self.c_var.get().strip().upper()}
        try:
            days = int(self.ca_days_var.get())
            self.ca = CertificateAuthority.create(self.folder_var.get(), subject, days)
        except (CaError, ValueError, OSError) as e:
            messagebox.showerror("Certificate Authority", str(e))
            return
        self._show_ca()
        self._log(f"CA created in {self.folder_var.get()} (keep this folder private: it holds the CA key)")
        self._update()

    # ---- device ------------------------------------------------------------
    def _session(self) -> prov.ProvisioningSession:
        """On the BLE loop."""
        call = self.ctx.bus.call
        return prov.ProvisioningSession(self._bulkxfer().new_client(), self._pc().receiver,
                                        log=lambda t: call(self._log, t),
                                        step=lambda t: call(self.step_var.set, t))

    def _show_status(self, st: prov.DeviceStatus):
        busy = ", CSR transfer running" if st.csr_busy else ""
        self.dev_var.set(f"Device: {st.state_name}{busy}, CSR {st.csr_len} B, "
                         f"key SHA-256 {st.pubkey_sha256.hex()[:16]}…")

    def _get_status(self):
        async def go():
            return await self._session().get_status()
        self._run(go(), self._show_status)

    def _deprovision(self):
        if not messagebox.askyesno(
                "Remove provisioning",
                "Wipe the device's key, CSR, CA certificate and device certificate?\n\n"
                "The device then makes a new key and CSR and must be provisioned again; "
                "certificates issued for the old key no longer match it."):
            return
        self._outcome = None

        async def go():
            s = self._session()
            await s.deprovision()
            return await s.get_status()

        def done(st: prov.DeviceStatus):
            self._show_status(st)
            self._log(f"provisioning removed: device {st.state_name}, new key "
                      f"{st.pubkey_sha256.hex()[:16]}…")
        self._log("removing the device's provisioning")
        self._run(go(), done)

    def _provision(self):
        try:
            days = int(self.dev_days_var.get())
        except ValueError:
            messagebox.showerror("Provisioning", "validity must be a number of days")
            return
        self._outcome = None

        async def go():
            return await self._session().provision(self.ca, days)

        def done(out: prov.Outcome):
            self._outcome = out
            self._show_status(out.status)
            self.dev_var.set(f"Device: PROVISIONED, certificate serial {out.serial_hex}")
            self._log(f"device certificate issued to {out.device_cert.subject.rfc4514_string()}, "
                      f"serial {out.serial_hex}; the device logs both certificates on its UART")
            self._show_ca()
        self._log("provisioning started")
        self._run(go(), done)

    def _run(self, coro, on_done):
        def finished(result):
            self._future = None
            on_done(result)
            self._update()

        def failed(exc):
            self._future = None
            self.step_var.set("failed")
            self._log(f"failed: {exc}", "error")
            self._update()

        def aborted():
            self._future = None
            self.step_var.set("aborted")
            self._log("aborted", "warn")
            self._update()
        self._future = self.ctx.run(coro, on_done=finished, on_error=failed, on_cancel=aborted)
        self._update()

    def _save_cert(self):
        if not self._outcome:
            return
        path = filedialog.asksaveasfilename(
            title="Save device certificate", defaultextension=".pem",
            initialfile=f"device_{self._outcome.serial_hex}.pem",
            filetypes=[("PEM certificate", "*.pem"), ("DER certificate", "*.der")])
        if not path:
            return
        enc = serialization.Encoding.DER if path.lower().endswith(".der") else serialization.Encoding.PEM
        with open(path, "wb") as fh:
            fh.write(self._outcome.device_cert.public_bytes(enc))
        self._log(f"device certificate saved to {os.path.basename(path)}")

    # ---- Feature -----------------------------------------------------------
    @property
    def busy(self) -> bool:
        return self._future is not None

    def cancel(self):
        if self._future is not None:
            self._future.cancel()

    def on_connected(self, info):
        self.dev_var.set("Device: -")
        self._update()

    def on_disconnected(self, reason):
        self.cancel()
        self._update()
