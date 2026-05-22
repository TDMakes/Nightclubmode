# 🕺 NightClub Mode — Beat Saber Quest Mod
**Version:** 1.0.0 | **Game:** Beat Saber 1.40.8 (Quest)

Turns every Beat Saber level into a full-on nightclub experience:

| Feature | Detail |
|---|---|
| 🌈 Rainbow Sabers | Left & right sabers cycle through complementary hues locked to song BPM |
| 💡 Beat-Pulsing Lights | All environment lights throb in sync with the beat |
| ⚡ Miss Strobe | Three rapid white strobes fire whenever you miss a note or hit a wall |
| 🎨 Seamless | Works with every environment; doesn't break scoring or replays |

---

## Prerequisites

| Tool | Where to get it |
|---|---|
| **BMBF** (installed on Quest) | https://bmbf.dev |
| **qpm** (Quest Package Manager) | `winget install sc2ad.qpm` or https://github.com/sc2ad/QuestPackageManager |
| **Android NDK r25+** | Android Studio SDK Manager or https://developer.android.com/ndk/downloads |
| **CMake 3.22+** | https://cmake.org/download |
| **Ninja** | `winget install Ninja-build.Ninja` |
| **PowerShell 7+** | `winget install Microsoft.PowerShell` |

---

## Building from Source

```powershell
# 1. Clone / extract this repo
cd NightClubMode

# 2. Install dependencies (downloads headers from GitHub)
qpm restore

# 3. Build the shared library
pwsh ./build.ps1

# 4. Package into a .qmod file
pwsh ./package.ps1
```

The output file `NightClubMode-1.0.0.qmod` will appear in the project root.

---

## Installing

1. Connect your Quest to your PC (or use SideQuest wireless ADB)
2. Open **BMBF** on the headset
3. In BMBF → **Tools** tab → **Upload .qmod**
4. Select `NightClubMode-1.0.0.qmod`
5. Hit **Sync to Beat Saber** in BMBF
6. Relaunch Beat Saber — the mod loads automatically

---

## How It Works (Technical)

```
AudioTimeSyncController::Update      → tracks song time each frame
BeatmapObjectSpawnController::Start  → captures BPM on level load
ColorManager::ColorForSaberType      → replaces saber colors with HSV rainbow
LightSwitchEventEffect::SetColor     → blends environment lights to beat pulse
NoteController::HandleNoteWasMissed  → triggers strobe state machine on miss
ComboController::HandlePlayerHead…   → triggers strobe state machine on wall hit
```

The strobe state machine lives in `StrobeRunner` inside `src/main.cpp` and is
ticked from the `AudioTimeSyncController::Update` hook so it runs on Unity's
main thread without needing a coroutine host.

---

## Tweaking

Open `include/NightClubMode.hpp` and adjust:

```cpp
// How many beats per rainbow cycle (default: 4)
float cycleSeconds = 4.0f / beatsPerSecond;

// Strobe flashes on miss (default: 6 half-cycles = 3 blinks)
static const int TOTAL_FLASHES = 6;

// Time between each flash half-cycle in seconds (default: 70 ms)
static const float FLASH_INTERVAL = 0.07f;

// Min/max brightness of the beat pulse (default: 0.4 → 1.0)
return 0.4f + 0.6f * (0.5f + 0.5f * std::sin(phase));
```

Then rebuild and re-package.

---

## Compatibility Notes

- Tested against **Beat Saber 1.40.8** IL2CPP metadata
- Uses `beatsaber-hook >=3.8.0` and `codegen >=0.24.0`
- Compatible with **ModAssistant** / **BMBF** install workflows
- Does **not** modify note timing, score, or replay data — safe for ranked play

---

## License

MIT — do whatever you want, just don't claim you wrote it alone. 🤝
