# What "source-built" means here

A Common FPS release is source-built only when the distributed PS5 artifacts
are created from:

```text
this repository's source
+ declared open-source build dependencies
+ PS5 Payload SDK
```

with no binary-patching of the historical Common FPS/PHU payload.

A host unit-test build alone does not qualify.

A successful GitHub `PS5 Source Build` workflow is the compiler-side gate.

FW 9.60 hardware testing is a separate runtime gate.
