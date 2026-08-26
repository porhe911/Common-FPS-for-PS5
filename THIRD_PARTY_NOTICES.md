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

Files carrying:

```text
SPDX-License-Identifier: GPL-3.0-or-later
```

are licensed by the Common FPS project under GPL-3.0-or-later.

Third-party files always retain their upstream notices and licenses.
