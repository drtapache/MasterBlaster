# MasterBlaster VST3 — Build & Distribution Guide

---

## For end users (the only thing they need)

End users receive **one file**: `MasterBlaster-1.1.0-Setup.exe`

1. Double-click it
2. Click **Install**
3. Open your DAW and rescan plugins

Zero prerequisites. Nothing else required. The installer handles everything automatically.

---

## For developers — how the installer is built

The source code in this repository is **developer tooling only**. End users never see it, never run any batch file, and never need any tool listed below.

### Recommended: GitHub Actions (zero local tools needed)

Push this repository to GitHub once. Every push to `main` or `master` automatically builds the installer and uploads it as a GitHub Actions artifact.

```
git init
git add .
git commit -m "initial"
git remote add origin https://github.com/YOUR_USERNAME/masterblaster.git
git push -u origin main
```

**To get the installer:**
- GitHub → **Actions** → latest run → **Artifacts** → download `MasterBlaster-1.1.0-Setup`

**To publish a release with the installer attached:**
1. GitHub → **Releases** → **Draft a new release**
2. Create tag `v1.1.0` and publish
3. The CI workflow auto-attaches `MasterBlaster-1.1.0-Setup.exe` to the release page

Users download the `.exe` directly from the release page. That is the entire distribution — one file.

---

### Optional: local build (developer machines only)

Only needed if you want to build without pushing to GitHub.

| Tool | Where to get |
|------|-------------|
| **Visual Studio 2022** | visualstudio.microsoft.com — "Desktop development with C++" workload |
| **Git** | git-scm.com |
| Internet (first build only) | CMake auto-downloads JUCE 7.0.9 |

CMake is bundled with Visual Studio — no separate install needed.

Run the developer build script:
```bat
dev_build.bat
```

Output: `installer\dist\MasterBlaster-1.1.0-Setup.exe`

Distribute only that `.exe` file. Everything else stays on the developer machine.

---

## Plugin overview

### Signal chain
```
INPUT → Linear Phase EQ → 4-Band Multiband Comp → Harmonic Saturation
      → Stereo Widener → True Peak Limiter → Dithering → LUFS Meter → OUTPUT
```

### Platform targets
| Platform | LUFS target | Ceiling | Character |
|----------|-------------|---------|-----------|
| SoundCloud | −9 LUFS | −0.3 dBFS | Punchy, slight HF push |
| Spotify | −14 LUFS | −1.0 dBFS | Balanced, normalised |
| Apple Music | −16 LUFS | −1.5 dBFS | Wide dynamics, cleanest |

### OPEN GUTS panel
- **EQ Bands** — 8-band linear phase (zero phase smear)
- **Compressor ratios** — per band (Sub / LowMid / HighMid / High)
- **Module bypass row** — toggle EQ / COMP / SAT / WIDTH / LIM / DITHER individually
- **Saturation drive** — asymmetric waveshaper amount
- **Stereo width** — 0 = mono, 1 = unity, 2 = maximum
- **Limiter ceiling** — true peak ceiling in dBFS
- **Dither** — 24-bit or 16-bit TPDF

### A/B comparison
Click **A/B** to instantly compare processed (A) vs dry input (B).

### Reference track
Drag any audio file onto **DROP REF TRACK** to auto-tune EQ bands 6–8 to match its tonal balance.

---

## Ableton Live 12 notes

- Plugin latency: **2048 samples** (linear phase FFT hop) — enable delay compensation (on by default)
- Works at 44.1 / 48 / 88.2 / 96 kHz
- Category: `Audio Effects → VST3 → Fx`
- After install: **Preferences → Plug-Ins → Rescan**

---

*Circuit Burn Audio — MasterBlaster v1.1*
