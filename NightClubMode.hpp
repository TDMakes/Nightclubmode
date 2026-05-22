#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  NightClub Mode — include/NightClubMode.hpp
//  Beat Saber 1.40.8 Quest Mod
//
//  Features:
//    • Sabers cycle through the full HSV rainbow in sync with the song BPM
//    • On MISS  → full white environment strobe (3 rapid flashes)
//    • On COMBO milestone (x10, x25, x50, x100…) → colour explosion burst
//    • Ambient environment lights pulse at half-BPM to simulate club lighting
// ─────────────────────────────────────────────────────────────────────────────

#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "paper/shared/logger.hpp"

#include "GlobalNamespace/SaberModelController.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/ColorManager.hpp"
#include "GlobalNamespace/LightSwitchEventEffect.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/ScoreController.hpp"

#include "UnityEngine/Color.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Mathf.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Coroutine.hpp"
#include "UnityEngine/WaitForSeconds.hpp"
#include "UnityEngine/GameObject.hpp"

#include <cmath>
#include <cstdint>

// ── Module-level logger ───────────────────────────────────────────────────────
static constexpr auto NCM_TAG = "NightClubMode";
static Paper::ConstLoggerContext<13> NCM_LOGGER = Paper::Logger::WithContext<NCM_TAG>();

// ── Shared state (updated each frame / hook) ──────────────────────────────────
namespace NightClubMode {

    // Current BPM read from the beatmap; set once on song start
    inline float g_bpm          = 120.0f;
    // Elapsed song time in seconds (updated per frame from AudioTimeSyncController)
    inline float g_songTime     = 0.0f;
    // Current combo count
    inline int   g_combo        = 0;
    // Whether a strobe sequence is running
    inline bool  g_strobing     = false;
    // Toggle: mod is active
    inline bool  g_enabled      = true;

    // ── HSV → RGB ─────────────────────────────────────────────────────────────
    inline UnityEngine::Color HsvToRgb(float h, float s, float v) {
        h = std::fmod(h, 1.0f);
        if (h < 0.0f) h += 1.0f;

        int   i = static_cast<int>(h * 6.0f);
        float f = h * 6.0f - static_cast<float>(i);
        float p = v * (1.0f - s);
        float q = v * (1.0f - f * s);
        float t = v * (1.0f - (1.0f - f) * s);

        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return UnityEngine::Color(r, g, b, 1.0f);
    }

    // ── Current rainbow hue based on BPM beat phase ───────────────────────────
    //  Complete one full rainbow cycle every 4 beats
    inline float BeatHue() {
        if (g_bpm <= 0.0f) return 0.0f;
        float beatsPerSecond = g_bpm / 60.0f;
        float cycleSeconds   = 4.0f / beatsPerSecond;   // 4-beat cycle
        return std::fmod(g_songTime, cycleSeconds) / cycleSeconds;
    }

    // ── Beat pulse brightness (sine wave locked to BPM) ──────────────────────
    //  Returns 0.0 … 1.0; peaks once per beat
    inline float BeatPulse() {
        if (g_bpm <= 0.0f) return 1.0f;
        float beatsPerSecond = g_bpm / 60.0f;
        float phase = g_songTime * beatsPerSecond * 2.0f * 3.14159265f;
        // remap sin from [-1,1] to [0.4,1.0] so lights never go fully dark
        return 0.4f + 0.6f * (0.5f + 0.5f * std::sin(phase));
    }

} // namespace NightClubMode
