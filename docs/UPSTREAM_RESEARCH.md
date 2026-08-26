# Upstream research used for the clean rewrite

## PS5 Payload SDK

Project:

`ps5-payload-dev/sdk`

The SDK uses the Prospero toolchain and documents normal PS5 payload builds via
`PS5_PAYLOAD_SDK/toolchain/prospero.mk`.

License note: most SDK files are GPLv3+ unless an individual file says otherwise.

## etaHEN

Reference commit used during research:

`5061a85312eb1d2da811269bcb9061e4b01302a1`

The published ShellUI source demonstrates a GPL source implementation of:

- Mono ShellUI hooks;
- access to `Scene.RootWidget`;
- runtime creation/removal of overlay labels;
- four position presets:
  - top left
  - top right
  - bottom left
  - bottom right

This is the preferred upstream source basis for the clean Common FPS renderer.

## etaHEN Plugin SDK

Project:

`etaHEN/etaHEN-Plugins`

The Plugin SDK is GPL-3.0 and is designed for persistent etaHEN plugins.
It depends on John Törnblom's PS5 payload SDK and provides libhijacker/process
integration facilities.

## Clean-room rule for Common FPS v1.1.x

Do not copy the PHU-derived binary renderer from v1.0.0.

Implement the new PS5 adapter from published GPL source and our documented
Common FPS behavior.
