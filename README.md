# quirk

Synth instrument with an editable bezier curve waveshaper. PolyBLEP sawtooth oscillator shaped through a user-defined transfer function. Double-click the graph to add points, drag to reshape. Symmetric or asymmetric curve editing.

VST3 (macOS, Windows), AU + Standalone (macOS).

## Usage

- **Gain**: how hard the oscillator output hits the curve (0 = clean, 100 = hard clip).
- **Graph**: double-click to add/remove points. Drag points and handles to reshape.
- **Symmetric / Asymmetric**: mirror the curve or edit each half independently.
- **Volume**: output level.
- **Power button** (top-right): hard bypass.

## Parameters

| Parameter | Range | Default |
|---|---|---|
| Gain | 0 - 100 | 0.0 |
| Volume | 0% - 100% | 50.0% |
| Symmetric | on / off | on |
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
