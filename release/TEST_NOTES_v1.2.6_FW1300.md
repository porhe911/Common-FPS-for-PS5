# Common FPS v1.2.6 — FW 13.00 test

Hardware confirmed on FW 13.0; promotion to v1.2.6 stable authorized by the user on 2026-11-25. Previous v1.2.5 remains intact. User log supplied 2026-11-25: SDK 0x13000040; controller starts, finds eboot, DCE returns 60 FPS, ShellUI bootstrap succeeds. Hook probe backend=none status=-2 (UnsupportedFirmware), followed by quarantine. SDK v0.41 pinned by this project contains FW 13.00 support (crt/kernel.c case 0x13000000). Source compatibility does not prove runtime guard compatibility.

Candidate adds only family 0x13000000 to the existing protected ptrace probe and single-byte ShellUI guard path. Existing checked MDBG fallback is attempted on EFAULT. Expected-byte checks, restoration and quarantine remain. Native Application.Update patch remains unsupported for this family. Game-process memory reads/writes stay disabled; DCE, FPS limit, settings, prior firmware routes and SDK dependency pins unchanged. Media shortcut installer remains intentionally skipped on FW12+.

Initial test procedure (retained for regression testing): Save and close games, fully reboot, disable old autoload, manually load only candidate ELF (or one plugin, never both). Launch a game and check FPS, game stability and UI responsiveness. If failure, reboot, do not autoload candidate and send /data/CommonFPS/CommonFPS.log and /data/CommonFPS/CommonFPS_shellui.log; klog if crash. Initial hardware confirmation completed.

Expected log: fw1300_runtime=experimental; probe backend ptrace-io or ptrace-io+mdbg-read; successful read=0; guard with exact protection restoration, verified=1, detached=1, auth_restored=1; renderer online. If guard fails, collect logs.

## Hardware result (2026-11-25)

User confirmed the overlay now appears and authorized publication. The v1.2.6 log shows:

- SDK 0x13000040; successful bootstrap, probe read primary EFAULT (14), checked MDBG fallback 0.
- Guard: status=0, verified=1, detached=1, auth_restored=1, original_protection=4, protect_rc=0, ptrace_write_rc=0, restore_protection_rc=0, protection_restored=1.
- Renderer online in 748 ms; DCE returns 60 FPS for PID 94 and later PID 120.
- One DCE ioctl error (errno=2) during the game transition; sampling resumed on the next game PID.
- Game memory reads and writes remain zero. Media installer skipped on FW12+.

PASS: reported FW 13.0 overlay startup and observed game PID transition. NOT TESTED: 120 Hz output, etaHEN plugin autoload, Rest Mode, older or other firmware regression. The test binary retains its original diagnostic marker; it describes the initial test build state, not the subsequent hardware result. No runtime source changes were needed for promotion.
