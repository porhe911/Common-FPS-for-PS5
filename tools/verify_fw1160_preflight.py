#!/usr/bin/env python3
"""Verify that the observation-only test does not contain counter adapters."""
import hashlib
import pathlib
import subprocess
import sys

elf = pathlib.Path(sys.argv[1]).read_bytes()
plugin = pathlib.Path(sys.argv[2]).read_bytes()
header = b"etaHEN_PLUGIN\0CFPS00061\0" + b"0.10\0"
assert elf.startswith(b"\x7fELF")
assert plugin.startswith(header) and plugin[len(header):] == elf
for marker in [b"mode=process-observation-only", b"overlay=disabled",
               b"dce=disabled mdbg=disabled", b"ptrace=disabled auth_changes=disabled",
               b"/data/CommonFPS_FW1160_Preflight.log", b"observe_seconds=120",
               b"stage=sysctl-data-before", b"stage=sysctl-data-after"]:
    assert marker in elf, marker
symbols = subprocess.check_output(["nm", sys.argv[1]], text=True)
main = subprocess.check_output(["objdump", "-d", "--disassemble=main", sys.argv[1]], text=True)
assert "<fork>" not in main
for name in ["mdbg_copyout", "mdbg_copyin", "DceFpsSampler", "FpsSampler",
             "inject_shellui_stable", "pt_attach", "pt_detach", "ioctl",
             "kernel_set_ucred_authid", "kernel_mprotect"]:
    assert name not in symbols, name
assert b"/dev/dce" not in elf
assert b"id_commonfps_value" not in elf
print("FW 11.60 observation-only boundary: PASS")
print("ELF SHA256:", hashlib.sha256(elf).hexdigest())
print("plugin SHA256:", hashlib.sha256(plugin).hexdigest())
