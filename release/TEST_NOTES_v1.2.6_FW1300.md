# Common FPS v1.2.6 — FW 13.00 test

Not a public release. Stable v1.2.5 unchanged. User log supplied 2026-11-25: SDK 0x13000040; controller starts, finds eboot, DCE returns 60 FPS, ShellUI bootstrap succeeds. Hook probe backend=none status=-2 (UnsupportedFirmware), followed by quarantine. SDK v0.41 pinned by this project contains FW 13.00 support (crt/kernel.c case 0x13000000). Source compatibility does not prove runtime guard compatibility.

Candidate adds only family 0x13000000 to the existing protected ptrace probe and single-byte ShellUI guard path. Existing checked MDBG fallback is attempted on EFAULT. Expected-byte checks, restoration and quarantine remain. Native Application.Update patch remains unsupported for this family. Game-process memory reads/writes stay disabled; DCE, FPS limit, settings, prior firmware routes and SDK dependency pins unchanged. Media shortcut installer remains intentionally skipped on FW12+.

Hardware risk: the ShellUI guard transaction has not been tested on FW13.00; shell restart/console hang possible if runtime ABI differs. Save and close games, fully reboot, disable old autoload, manually load only candidate ELF (or one plugin, never both). Launch a game and check FPS, game stability and UI responsiveness. If failure, reboot, do not autoload candidate and send /data/CommonFPS/CommonFPS.log and /data/CommonFPS/CommonFPS_shellui.log; klog if crash. Hardware confirmation required before publication.

Expected log: fw1300_runtime=experimental; probe backend ptrace-io or ptrace-io+mdbg-read; successful read=0; guard with exact protection restoration, verified=1, detached=1, auth_restored=1; renderer online. If guard fails, collect logs.
