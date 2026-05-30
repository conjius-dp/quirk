# quirk

<p align="center">
  <picture><img src="https://conjius-dp.github.io/quirk/screenshot.png" width="680" alt="Quirk"></picture>
</p>

<p align="center">
  <a href="https://github.com/conjius-dp/quirk/releases/latest/download/Quirk-macOS-VST3.zip"><img src="Assets/badges/download-vst3-macos.png" height="32" alt="Download VST3 for macOS"></a>
  &nbsp;
  <a href="https://github.com/conjius-dp/quirk/releases/latest/download/Quirk-macOS-AU.zip"><img src="Assets/badges/download-au-macos.png" height="32" alt="Download AU for macOS"></a>
  &nbsp;
  <a href="https://github.com/conjius-dp/quirk/releases/latest/download/Quirk-Windows-VST3.zip"><img src="Assets/badges/download-vst3-windows.png" height="32" alt="Download VST3 for Windows"></a>
</p>

<p align="center">
  <a href="https://github.com/conjius-dp/quirk/actions/workflows/ci.yml"><img src="https://github.com/conjius-dp/quirk/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/conjius-dp/quirk/releases/latest"><img src="https://img.shields.io/github/v/release/conjius-dp/quirk?label=stable&color=brightgreen" alt="Stable"></a>
  <a href="https://github.com/conjius-dp/quirk/releases"><img src="https://img.shields.io/github/v/release/conjius-dp/quirk?include_prereleases&label=nightly" alt="Nightly"></a>
  <a href="https://github.com/conjius-dp/quirk/releases"><img src="https://img.shields.io/github/downloads/conjius-dp/quirk/total?label=downloads&color=blue" alt="Total downloads"></a>
</p>

Waveshaping polysynth with an editable bezier curve waveform. The curve defines the oscillator's waveshape directly. Double-click the graph to add control points, drag points and handles to reshape. Up to 10 voices with oldest-voice stealing.

VST3 (macOS, Windows), AU + Standalone (macOS). Zero latency.

## Usage

- **Graph**: double-click to add/remove points. Drag points and handles to reshape the waveform.
- **ADSR**: attack, decay, sustain, release envelope per voice.
- **Voices**: 1-10 polyphonic voices with oldest-voice stealing.
- **Volume**: output level.
- **Power button** (top-right): hard bypass.

## Parameters

| Parameter | Range | Default |
|---|---|---|
| Volume | 0% - 100% | 50.0% |
| Attack | 1 - 5000 ms | 10 ms |
| Decay | 1 - 5000 ms | 300 ms |
| Sustain | 0% - 100% | 80.0% |
| Release | 1 - 5000 ms | 200 ms |
| Voices | 1 - 10 | 4 |
| Bypass | on / off | off |

## Build

```bash
git clone https://github.com/conjius-dp/quirk.git
cd quirk
git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build
```

VST3 / AU bundles are auto-copied to `~/Library/Audio/Plug-Ins/`.

Requires JUCE 8.0.12, CMake >= 3.22, a C++17 compiler.

## Tests

```bash
cmake --build build --target QuirkTests && ./build/QuirkTests
```

## macOS: plugin won't open after download

macOS quarantines anything downloaded from a browser and blocks unsigned plugins with a Gatekeeper dialog. Strip the quarantine flag once after dropping the plugin into its folder:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/quirk.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/quirk.component
```

Restart your DAW and the plugin loads silently.
