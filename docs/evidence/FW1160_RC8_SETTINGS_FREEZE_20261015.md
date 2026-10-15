# FW 11.60: RC8 settings confirmation and game freeze

The user reports the Media shortcut appears and works correctly. They request
an explicit palette instead of cycling colors, report Cross failing to save
while cursor clicks work, and report a complete game freeze after changing
color and corner.

`CommonFPS(5).log` identifies SDK 0x11600005 / RC8 and records:

- CFPS00060 registration succeeds; init/check/install 0, present=1,
  auth_restored=1.
- Settings service starts on loopback port 39029.
- Renderer bootstrap succeeds, 68 imports resolved, zero missing.
- Guard write succeeds and exact original protection 4 is restored.
- Renderer is online; the controller observes game PID 99, 279, 459 and 638.
- There are intermittent DCE errno=2 failures and later zeroed VideoOut
  candidate reads. These observations do not identify a hang stack or prove
  causality. The ShellUI log is not included in this attachment.

RC9 removes redundant initial style assignment, pins temporary boxed values
and strings, detects root replacement within the same scene, limits subsequent
updates to changed properties and removes background Width/Height queries.
These are source-level corrections and an isolation step, not a confirmed
root cause of the console failure. The sampler/injector are unchanged.

The new host presentation test compiles the actual renderer against a mock
Mono/PUI runtime that invalidates unrooted objects between calls. It checks
initial/custom styles, minimal updates, repeated FPS updates, root replacement,
setter failure recovery and handle balance. A fresh PS5 test remains required.
