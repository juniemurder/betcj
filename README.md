# SHEETZ! ☁️

A cloud-and-baby-blue-sky shaping plugin (VST3 + Standalone) inspired by ShaperBox.
Draw a curve, and it modulates six effects in sync with your host tempo.

**Modules:** Volume · Filter (LP/HP + resonance) · Pan · Drive · Crush · Width
Each has its own curve, on/off, and depth. Global: Rate (1/32 → 4 bars, incl. triplets), Phase, Smooth, Mix.

## Editor
- Double-click: add point · drag: move · drag hollow circle: bend segment
- Right-click point: delete · right-click empty: shape menu (sine, saw, steps, random, invert, reverse)
- Hold **Alt** to bypass grid snapping

## Presets
- 26 factory presets (Sidechain, Gates, Filter, Pan & Width, Lo-Fi & Drive, Combos)
- **Save** stores to `Documents/SHEETZ/Presets/User`
- **Import** a `.sheetz` file(s), or **drag & drop** files or whole folders onto the plugin window
- **Export** shares the current sound as a `.sheetz` file (plain JSON, easy to hand-edit or share)

## Build on GitHub
1. Create a repo and push this folder.
2. Open the **Actions** tab: "Build SHEETZ!" runs for Windows, macOS, Linux.
3. Download the zip under *Artifacts*. Tag a release (`git tag v1.0.0 && git push --tags`) to attach them to a GitHub Release.

## Install
- Windows: copy `SHEETZ!.vst3` to `C:\Program Files\Common Files\VST3`
- macOS: copy to `~/Library/Audio/Plug-Ins/VST3`, then `xattr -cr "SHEETZ!.vst3"` (builds are unsigned)
- Linux: copy to `~/.vst3`

## Local build
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
