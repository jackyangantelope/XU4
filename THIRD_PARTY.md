# Source attribution

- XU4 1.4.3 from [xu4-engine/u4](https://github.com/xu4-engine/u4/tree/cc92e9c918374712d0be5407daa5a81835e727fa), GPL-2.0-or-later. The original GPL text is in `deps/xu4/COPYING`. The altered snapshot contains native rendering, input, audio and scheduling integration.
- [pico_shared](https://github.com/fhoedemakers/pico_shared) HDMI and FatFs driver snapshots under `deps/pico_shared`. The GPLv3 and component notices remain with their sources.
- libxml2, zlib, libpng and libxmp sources and licenses are under `deps/libs`. The build selects statically linked components and disables optional libxmp depackers and ProWizard.
- stb_vorbis carries its upstream license in `platform/native/stb_vorbis.c`. Board and port code keep their per-file notices.
- The pinned SDK, TinyUSB and Pico-PIO-USB are fetched separately. The combined firmware source distribution uses GPLv3.

Game assets and saves are supplied separately by the user.
