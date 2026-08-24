# Common FPS for PS5

A lightweight real-time FPS counter for PlayStation 5 homebrew environments.

## Features

- Real FPS values from the native VideoOut counter
- Works with PS4 and PS5 games
- Standalone `.elf`
- etaHEN `.plugin`
- Fast asynchronous initialization
- Fixed bottom-left overlay
- Integer FPS display
- Automatic game detection
- Game switching without restarting the counter
- Stable lifecycle tested across closing one game and launching another

Display example:

```text
FPS: 60
```

`FPS:` is purple and the numeric value is white.

## Downloads

Public release files are stored in [`releases/v1.0.0/`](releases/v1.0.0/):

- `Common_FPS_PS5_v1.0.0.elf` — standalone ELF
- `Common_FPS_PS5_etaHEN_v1.0.0.plugin` — etaHEN plugin

## etaHEN installation

1. Copy `Common_FPS_PS5_etaHEN_v1.0.0.plugin` to:

   ```text
   /data/etaHEN/plugins/
   ```

2. Restart etaHEN.
3. Disable the built-in etaHEN FPS counter.
4. Use:

   ```text
   overlay_fps=0
   ```

5. Start the plugin from etaHEN Toolbox / Plugins.

## Standalone ELF

Load `Common_FPS_PS5_v1.0.0.elf` with a compatible ELF loader.

The standalone ELF and etaHEN plugin use the same FPS core.

## Tested

- PS5 firmware 9.60
- PS4 games
- PS5 games
- Closing one game and launching another without restarting the counter

## Version

### v1.0.0

First public stable release, based on the internally tested `v0.28b Async Stable Init` branch.

Highlights:

- fast activation
- stable real FPS overlay
- fixed bottom-left position
- font size 26
- integer FPS values
- stable game lifecycle

## SHA-256

```text
Common_FPS_PS5_v1.0.0.elf
6a66da88a99fa8757bf5c649e388d777a10768cd444d9f4cf82649e4592e292c

Common_FPS_PS5_etaHEN_v1.0.0.plugin
f1240511bfe3cb17a3db6f4d099cf3cd69be678d10901425a7b33911265195e1
```

## Русский

**Common FPS for PS5** — универсальный счётчик реального FPS для PS5.

Возможности:

- реальные значения FPS;
- поддержка PS4 и PS5 игр;
- standalone ELF;
- etaHEN plugin;
- быстрый асинхронный запуск;
- неподвижный счётчик в левом нижнем углу;
- целые значения FPS;
- автоматическое определение запущенной игры;
- можно закрыть одну игру и запустить другую без повторной активации счётчика.

Для etaHEN скопируйте plugin в:

```text
/data/etaHEN/plugins/
```

Отключите встроенный FPS Counter etaHEN и используйте `overlay_fps=0`.

## Disclaimer

This is homebrew software intended for modified PlayStation 5 systems. Use at your own risk.
