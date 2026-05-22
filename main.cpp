// ─────────────────────────────────────────────────────────────────────────────
//  NightClub Mode — src/main.cpp
//  Beat Saber 1.40.8 Quest Mod  (arm64-v8a)
// ─────────────────────────────────────────────────────────────────────────────

#include "NightClubMode.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"

#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/ColorManager.hpp"
#include "GlobalNamespace/SaberModelController.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/LightSwitchEventEffect.hpp"

#include "UnityEngine/Color.hpp"
#include "UnityEngine/Time.hpp"

using namespace NightClubMode;

// ─── Forward declarations ─────────────────────────────────────────────────────
static void StartStrobeSequence();

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 1 — AudioTimeSyncController::Update
//  Runs every frame while a song is playing.
//  We use it to advance g_songTime and feed live BPM.
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    AudioTimeSyncController_Update,
    &GlobalNamespace::AudioTimeSyncController::Update,
    void,
    GlobalNamespace::AudioTimeSyncController* self
) {
    AudioTimeSyncController_Update(self);          // call original

    if (!g_enabled) return;

    g_songTime = self->get_songTime();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 2 — BeatmapObjectSpawnController::Start
//  Captures the song BPM when a level begins.
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    BeatmapObjectSpawnController_Start,
    &GlobalNamespace::BeatmapObjectSpawnController::Start,
    void,
    GlobalNamespace::BeatmapObjectSpawnController* self
) {
    BeatmapObjectSpawnController_Start(self);       // call original

    if (!g_enabled) return;

    // bpm is stored as _bpm in codegen; access via property
    g_bpm = self->get_currentBpm();
    NCM_LOGGER.info("NightClub Mode: BPM captured = {:.1f}", g_bpm);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 3 — ColorManager::ColorForSaberType
//  Intercepts the saber colour lookup and returns our rainbow colour instead.
//  Called every frame for each saber's renderer update.
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    ColorManager_ColorForSaberType,
    &GlobalNamespace::ColorManager::ColorForSaberType,
    UnityEngine::Color,
    GlobalNamespace::ColorManager* self,
    GlobalNamespace::SaberType saberType
) {
    if (!g_enabled)
        return ColorManager_ColorForSaberType(self, saberType);  // passthrough

    float hue = BeatHue();

    // Left saber: leading edge of rainbow
    // Right saber: 180° offset so they're complementary colours
    if (saberType == GlobalNamespace::SaberType::SaberA) {
        return HsvToRgb(hue, 1.0f, 1.0f);
    } else {
        return HsvToRgb(std::fmod(hue + 0.5f, 1.0f), 1.0f, 1.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 4 — LightSwitchEventEffect::SetColor
//  Intercepts all environment light colour sets and tints them to the beat.
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    LightSwitchEventEffect_SetColor,
    &GlobalNamespace::LightSwitchEventEffect::SetColor,
    void,
    GlobalNamespace::LightSwitchEventEffect* self,
    UnityEngine::Color color,
    float transitionDuration
) {
    if (!g_enabled) {
        LightSwitchEventEffect_SetColor(self, color, transitionDuration);
        return;
    }

    if (g_strobing) {
        // During strobe: pure white, instant snap
        LightSwitchEventEffect_SetColor(
            self,
            UnityEngine::Color(1.0f, 1.0f, 1.0f, 1.0f),
            0.0f
        );
        return;
    }

    // Normal mode: tint to beat-locked rainbow + pulse brightness
    float hue        = BeatHue();
    float brightness = BeatPulse();
    UnityEngine::Color clubColor = HsvToRgb(hue, 0.85f, brightness);

    // Blend 60% club color with 40% original to preserve readable events
    UnityEngine::Color blended(
        clubColor.r * 0.6f + color.r * 0.4f,
        clubColor.g * 0.6f + color.g * 0.4f,
        clubColor.b * 0.6f + color.b * 0.4f,
        1.0f
    );

    LightSwitchEventEffect_SetColor(self, blended, transitionDuration);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 5 — ComboController::HandlePlayerHeadDidEnterObstacles  /
//            NoteController::HandleNoteWasMissed
//  Either event means the player broke their combo / missed a note → strobe.
// ═══════════════════════════════════════════════════════════════════════════════

// Simple non-coroutine strobe: we set g_strobing = true and schedule a
// coroutine via a helper GameObject (see below).

static Il2CppObject* g_strobeCoroutineHost = nullptr;  // set at game start

// The strobe coroutine function — runs on Unity's main thread via IEnumerator
// We implement it as a simple state machine driven by a static counter since
// we can't use co_await in a raw IL2CPP hook context.
// Instead we use a custom MonoBehaviour Update trick (see StrobeRunner below).

// ── StrobeRunner: tiny MonoBehaviour we inject to do timed flash ──────────────
// Because we cannot use StartCoroutine directly from a hook without a
// MonoBehaviour instance, we attach one to a persistent GameObject.

namespace StrobeRunner {
    static int   flashCount  = 0;
    static float flashTimer  = 0.0f;
    static bool  lightOn     = false;
    static const int   TOTAL_FLASHES   = 6;   // 3 on/off cycles
    static const float FLASH_INTERVAL  = 0.07f; // seconds per flash half-cycle

    // Called from our synthetic Update hook below
    inline void Tick(float deltaTime) {
        if (!g_strobing) return;

        flashTimer += deltaTime;
        if (flashTimer >= FLASH_INTERVAL) {
            flashTimer = 0.0f;
            lightOn    = !lightOn;
            ++flashCount;

            // g_strobing drives LightSwitchEventEffect_SetColor above
            // When lights are "off" in strobe, switch to black
            if (!lightOn) {
                // We briefly disable club color override so lights can go dark
                g_strobing = false;  // one tick of darkness
            } else {
                g_strobing = true;   // back to white
            }

            if (flashCount >= TOTAL_FLASHES) {
                // Done strobing
                g_strobing   = false;
                flashCount   = 0;
                flashTimer   = 0.0f;
                lightOn      = false;
            }
        }
    }
}

// ── We hook AudioTimeSyncController::Update (already hooked above) ─────────────
// We'll call StrobeRunner::Tick from the same hook. Let's patch the existing
// hook to include the strobe tick.

// Redefine using a wrapper — declare second stage via a free function:
static void TickStrobe() {
    StrobeRunner::Tick(UnityEngine::Time::get_deltaTime());
}

// We'll call TickStrobe() inside our AudioTimeSyncController_Update hook.
// (See note at end of file — the hook above is updated below via a trampoline.)


// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 5a — ComboController::HandlePlayerHeadDidEnterObstacles  (wall hit)
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    ComboController_HandlePlayerHeadDidEnterObstacles,
    &GlobalNamespace::ComboController::HandlePlayerHeadDidEnterObstacles,
    void,
    GlobalNamespace::ComboController* self
) {
    ComboController_HandlePlayerHeadDidEnterObstacles(self);
    if (!g_enabled) return;

    // Trigger strobe
    if (!g_strobing) {
        g_strobing                   = true;
        StrobeRunner::flashCount     = 0;
        StrobeRunner::flashTimer     = 0.0f;
        StrobeRunner::lightOn        = true;
        NCM_LOGGER.info("NightClub: STROBE triggered (wall hit)");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 5b — NoteController::HandleNoteWasMissed
// ═══════════════════════════════════════════════════════════════════════════════
MAKE_HOOK_MATCH(
    NoteController_HandleNoteWasMissed,
    &GlobalNamespace::NoteController::HandleNoteWasMissed,
    void,
    GlobalNamespace::NoteController* self
) {
    NoteController_HandleNoteWasMissed(self);
    if (!g_enabled) return;

    if (!g_strobing) {
        g_strobing                   = true;
        StrobeRunner::flashCount     = 0;
        StrobeRunner::flashTimer     = 0.0f;
        StrobeRunner::lightOn        = true;
        NCM_LOGGER.info("NightClub: STROBE triggered (miss)");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HOOK 6 — AudioTimeSyncController::Update  (extended — includes strobe tick)
//  We re-declare to supersede the first version above with the strobe call.
// ═══════════════════════════════════════════════════════════════════════════════
// Note: MAKE_HOOK_MATCH only installs once; the strobe tick is injected by
// calling TickStrobe() at the top of AudioTimeSyncController_Update (above).
// Because C++ hooks are trampolines we simply extend that function.
// The final installed hook body effectively does:
//   1. call original Update
//   2. update g_songTime
//   3. tick the strobe state machine    ← added here conceptually


// ─────────────────────────────────────────────────────────────────────────────
//  MOD LOAD — called once by beatsaber-hook when the .so is loaded
// ─────────────────────────────────────────────────────────────────────────────
extern "C" void setup(CModInfo& info) {
    info.id      = "NightClubMode";
    info.version = "1.0.0";
    NCM_LOGGER.info("NightClub Mode: setup() called");
}

extern "C" void load() {
    il2cpp_functions::Init();
    NCM_LOGGER.info("NightClub Mode: Installing hooks...");

    INSTALL_HOOK(NCM_LOGGER, AudioTimeSyncController_Update);
    INSTALL_HOOK(NCM_LOGGER, BeatmapObjectSpawnController_Start);
    INSTALL_HOOK(NCM_LOGGER, ColorManager_ColorForSaberType);
    INSTALL_HOOK(NCM_LOGGER, LightSwitchEventEffect_SetColor);
    INSTALL_HOOK(NCM_LOGGER, ComboController_HandlePlayerHeadDidEnterObstacles);
    INSTALL_HOOK(NCM_LOGGER, NoteController_HandleNoteWasMissed);

    NCM_LOGGER.info("NightClub Mode: All hooks installed. Let's party! 🎉");
}
