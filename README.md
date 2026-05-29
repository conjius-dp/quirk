# quirk

Synth instrument with an editable bezier curve waveform. The curve defines the oscillator's waveshape directly. Double-click the graph to add control points, drag points and handles to reshape.

VST3 (macOS, Windows), AU + Standalone (macOS).

## Usage

- **Graph**: double-click to add/remove points. Drag points and handles to reshape the waveform.
- **Volume**: output level.
- **Power button** (top-right): hard bypass.

## Parameters

| Parameter | Range | Default |
|---|---|---|
| Volume | 0% - 100% | 50.0% |
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
