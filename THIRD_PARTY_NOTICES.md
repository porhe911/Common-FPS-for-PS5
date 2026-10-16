# Third-Party Notices

Common FPS for PS5 interoperates with and derives implementation knowledge from
open-source PS5 homebrew projects.

## PS5 Payload SDK

Project:

```text
https://github.com/ps5-payload-dev/sdk
```

Used as the PS5 payload toolchain/system SDK. The upstream project retains its
copyright notices and per-file licensing terms.

## etaHEN

Project:

```text
https://github.com/etaHEN/etaHEN
```

etaHEN publishes its source under GPLv3. Common FPS uses etaHEN as an
open-source reference/source dependency for PS5 process and ShellUI/PUI
integration.

## ps5-payload-dev/shsrv

Project:

```text
https://github.com/ps5-payload-dev/shsrv
```

GPLv3+ source used as the reference for the ptrace ELF loader and the
payload-args execution contract.

## Historical PHU-derived work

The historical Common FPS `v1.0.0` binary was developed through binary analysis
and patching of third-party PS5 homebrew renderer/loader mechanisms, including
PHU-derived material.

Common FPS does not claim ownership of that third-party code.

The source-built `v1.1.x` path intentionally does **not** embed the historical
PHU ELF/SO renderer blob.

## Common FPS-owned files

The Media launcher uses the parameter format, media category, deeplink approach
and direct AppInstUtil registration demonstrated by drakmor/ShadowMountPlus,
GPLv3, commit `98c524890176c7b474fcb03387bec88c4a559861` (2026-10-15 snapshot):
`src/sm_shell_icon.c`, `src/sm_install.c`, `assets/shell_icon_param.json`.
Reference: https://github.com/drakmor/ShadowMountPlus
Common FPS uses its own icon, settings page, ownership checks, bounded atomic
asset writes and scoped credential restoration. No upstream icon is copied.


Files carrying:

```text
SPDX-License-Identifier: GPL-3.0-or-later
```

are licensed by the Common FPS project under GPL-3.0-or-later.

Third-party files always retain their upstream notices and licenses.
