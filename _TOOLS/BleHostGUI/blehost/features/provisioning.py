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
from .base import Feature


class ProvisioningFeature(Feature):
    title = "Provisioning"

    def __init__(self, ctx):
        super().__init__(ctx)
        self.ca = None
        self._future = None
        self._outcome = None

    # ---- UI ----------------------------------------------------------------
    def build(self, parent):
        f = ttk.Frame(parent, padding=8)
        f.columnconfigure(1, weight=1)
        f.rowconfigure(6, weight=1)

        # Certificate Authority
        ca = ttk.LabelFrame(f, text="Certificate Authority (this PC)", padding=6)
        ca.grid(row=0, column=0, columnspan=3, sticky="ew")
        ca.columnconfigure(1, weight=1)
        ttk.Label(ca, text="Folder").grid(row=0, column=0, sticky="w")
        self.folder_var = tk.StringVar(value=DEFAULT_FOLDER)
        ttk.Entry(ca, textvariable=self.folder_var).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(ca, text="Browse…", command=self._browse).grid(row=0, column=2)
        ttk.Button(ca, text="Load", command=self._load_ca).grid(row=0, column=3, padx=(4, 0))

        new = ttk.Frame(ca)
        new.grid(row=1, column=0, columnspan=4, sticky="ew", pady=(6, 0))
        self.cn_var = tk.StringVar(value="BLE Host Provisioning CA")
        self.o_var = tk.StringVar(value="Bajaj Auto Technology Limited")
        self.c_var = tk.StringVar(value="IN")
        self.ca_days_var = tk.StringVar(value="3650")
        for label, var, width in (("CN", self.cn_var, 26), ("O", self.o_var, 26), ("C", self.c_var, 4),
                                  ("Valid (days)", self.ca_days_var, 6)):
            ttk.Label(new, text=label).pack(side="left", padx=(0, 2))
            ttk.Entry(new, textvariable=var, width=width).pack(side="left", padx=(0, 8))
        ttk.Button(new, text="Create CA", command=self._create_ca).pack(side="left")

        self.ca_var = tk.StringVar(value="No CA loaded")
        ttk.Label(ca, textvariable=self.ca_var, wraplength=700, justify="left").grid(
            row=2, column=0, columnspan=4, sticky="w", pady=(6, 0))

        # PC service (the device sends the CSR to it)
        self.pc_var = tk.StringVar()
        ttk.Label(f, textvariable=self.pc_var).grid(row=1, column=0, columnspan=3, sticky="w", pady=(6, 0))

        # Device
        btns = ttk.Frame(f)
        btns.grid(row=2, column=0, columnspan=3, sticky="ew", pady=6)
        self.status_btn = ttk.Button(btns, text="Get Status", command=self._get_status)
        self.status_btn.pack(side="left")
        self.prov_btn = ttk.Button(btns, text="Provision", command=self._provision)
        self.prov_btn.pack(side="left", padx=4)
        self.abort_btn = ttk.Button(btns, text="Abort", command=self.cancel)
        self.abort_btn.pack(side="left")
        self.deprov_btn = ttk.Button(btns, text="Remove provisioning…", command=self._deprovision)
        self.deprov_btn.pack(side="left", padx=4)
        ttk.Label(btns, text="Device cert valid (days)").pack(side="left", padx=(12, 2))
        self.dev_days_var = tk.StringVar(value="365")
        ttk.Entry(btns, textvariable=self.dev_days_var, width=6).pack(side="left")
        self.save_btn = ttk.Button(btns, text="Save device certificate…", command=self._save_cert)
        self.save_btn.pack(side="right")

        self.dev_var = tk.StringVar(value="Device: -")
        ttk.Label(f, textvariable=self.dev_var).grid(row=3, column=0, columnspan=3, sticky="w")
        self.step_var = tk.StringVar()
        ttk.Label(f, textvariable=self.step_var).grid(row=4, column=0, columnspan=3, sticky="w")

        ttk.Label(f, text="Log").grid(row=5, column=0, sticky="w", pady=(6, 0))
        self.log_text = tk.Text(f, height=10, wrap="word", state="disabled")
        self.log_text.grid(row=6, column=0, columnspan=3, sticky="nsew")
        sb = ttk.Scrollbar(f, orient="vertical", command=self.log_text.yview)
        sb.grid(row=6, column=3, sticky="ns")
        self.log_text.configure(yscrollcommand=sb.set)

        if CertificateAuthority.exists(self.folder_var.get()):
            self._load_ca(quiet=True)
        self._update()
        f.after(1000, self._tick)
        return f

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
