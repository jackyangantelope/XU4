# XU4 for RP2350

[XU4](https://github.com/xu4-engine/u4) recreates the Ultima IV engine.
This source port targets Adafruit Fruit Jam with 8 MiB PSRAM, HDMI
audio/video, SD storage and one USB keyboard host port.

Provide your own compatible Ultima IV data and XU4 supplementary resources.
Under `/roms/XU4`, the dataset picker checks for `conf/config.xml`,
`ultima4/AVATAR.EXE` and `ultima4/WORLD.MAP`. Keep the complete dataset
with its original directory structure.

The title and gate sounds use PCM sidecars. MIDI playback uses rendered
PCM sidecars and a General MIDI soundfont. On Linux or WSL with
FluidSynth development headers, generate them beside your own files:

```sh
cc scripts/render-midi.c $(pkg-config --cflags --libs fluidsynth) -o /tmp/xu4-render-midi
cc scripts/decode-ogg.c platform/native/stb_vorbis.c -lm -o /tmp/xu4-decode-ogg
python3 scripts/prepare-audio.py /path/to/dataset --midi-renderer /tmp/xu4-render-midi --ogg-decoder /tmp/xu4-decode-ogg --soundfont /path/to/gm.sf2
```

Other effects are decoded from the dataset's original OGG files and cached
as needed. Game assets and generated PCM stay on the SD card.

The November 11, 2026 physical test of `0.1.3-native-dev04-waitexit1`
recorded 845 movement frames over 35.207737 seconds (24.0004 FPS).
The user confirmed sound, picture and movement. The Fruit Jam port
has separate board wiring and awaits hardware validation.

## Build

Use Git, CMake 3.20+, Ninja, Python 3 and Arm GNU Toolchain 14.2.Rel1.
Pico SDK, TinyUSB and Pico-PIO-USB revisions are in `dependencies.lock.json`.

```powershell
./scripts/fetch-dependencies.ps1 -Destination <outside-source-directory>
./scripts/build.ps1 -PicoSdkPath <pico-sdk> -PicoPioUsbPath <Pico-PIO-USB> `
  -ToolchainPath <arm-toolchain> -NinjaPath <ninja.exe>
```

The build writes `build/fruit-jam/picoXu4Native.uf2` for a compatible
16 MiB flash loader at address `0x10080000`. For offline builds pass
`-PioasmDir` and `-PicotoolDir`.

See [THIRD_PARTY.md](THIRD_PARTY.md) and [VALIDATION.md](VALIDATION.md).

## License

The engine is based on XU4 1.4.3 under GPL-2.0-or-later. The combined
firmware source uses GPLv3. Component notices are retained.

Run the scheduling and diagnostic regression on Linux or WSL with
`bash scripts/test-host.sh` and a C++ compiler with ASan/UBSan support.
