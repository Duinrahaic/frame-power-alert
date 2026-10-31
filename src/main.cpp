// frame-power-alert: a head-locked pill on the Steam Frame when its USB-C input stops charging it.
#include "openvr.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "chime.h"
#include "pill.h"

#include <glob.h>
#include <signal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* kFontPath = "/usr/share/fonts/noto/NotoSans-Bold.ttf";
constexpr const char* kInputGlob = "/sys/class/power_supply/tcpm-source-psy-*";
constexpr const char* kBatteryGlob = "/sys/class/power_supply/max1720x_bat*";

constexpr long kMinChargingMicrovolts = 9'000'000;  // PD100 falls back to 5 V as a battery runs flat; that counts as dead
constexpr int kLowBatteryPercent = 25;
constexpr int kStableReads = 3;                     // ≥ 0.5 s unchanged; rides out the PD renegotiation blip
constexpr auto kPollInterval = std::chrono::milliseconds(250);
constexpr auto kIdleTick = std::chrono::milliseconds(50);
constexpr auto kAnimationTick = std::chrono::milliseconds(11);  // ~90 Hz

constexpr float kAlertSecs = 10, kChargingSecs = 4;  // ring runs out, then the pill drops away
constexpr float kEnterSecs = 0.45f, kExitSecs = 0.3f;
constexpr float kRiseMeters = 0.06f, kMinScale = 0.85f;

constexpr float kWidthMeters = 0.3f, kDrop = 0.38f, kDistance = 0.9f;

enum class Power { Charging, NotCharging };

std::string globFirst(const char* pattern) {
    glob_t g;
    std::string path;
    if (glob(pattern, 0, nullptr, &g) == 0 && g.gl_pathc > 0) path = g.gl_pathv[0];
    globfree(&g);
    return path;
}

long readLong(const std::string& path) {
    std::ifstream f(path);
    long v = -1;
    f >> v;
    return v;
}

Power readPower() {
    std::string input = globFirst(kInputGlob);
    bool charging = !input.empty() && readLong(input + "/online") == 1 &&
                    readLong(input + "/voltage_now") >= kMinChargingMicrovolts;
    return charging ? Power::Charging : Power::NotCharging;
}

int readBatteryPercent() { return static_cast<int>(readLong(globFirst(kBatteryGlob) + "/capacity")); }


bool isLowBattery(int percent) { return percent >= 0 && percent <= kLowBatteryPercent; }

float secondsSince(Clock::time_point t) { return std::chrono::duration<float>(Clock::now() - t).count(); }

float easeOutBack(float x) {
    constexpr float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * std::pow(x - 1, 3.0f) + c1 * std::pow(x - 1, 2.0f);
}

// Position along the pop-up: 0 = hidden below, 1 = in place. Overshoots slightly on the way in.
struct Motion {
    float progress, alpha;
    bool operator==(const Motion& o) const { return progress == o.progress && alpha == o.alpha; }
};

Motion motionAt(float t, float showFor) {
    if (t < kEnterSecs) return {easeOutBack(t / kEnterSecs), std::min(1.0f, t / (kEnterSecs / 2))};
    if (t < showFor) return {1, 1};
    float q = std::min(1.0f, (t - showFor) / kExitSecs);
    return {1 - q * q * q, 1 - q};
}

// Pill pixels to meters at a given pop-up scale.
float pillMetersPerPixel(Motion m) { return kWidthMeters * (kMinScale + (1 - kMinScale) * m.progress) / kTexW; }

// Below eye level, tilted up to face the eye; rises into place from kRiseMeters lower. xOffset is along the
// pill's own horizontal axis; spin turns the overlay clockwise (radians) in its own plane.
void place(vr::VROverlayHandle_t overlay, Motion m, float widthMeters, float xOffset, float spin = 0) {
    float tilt = std::atan(kDrop / kDistance), c = std::cos(tilt), s = std::sin(tilt);
    float cs = std::cos(spin), sn = std::sin(spin);
    float y = -kDrop - (1 - m.progress) * kRiseMeters;
    vr::HmdMatrix34_t pose = {{{cs, sn, 0, xOffset}, {-c * sn, c * cs, s, y}, {s * sn, -s * cs, c, -kDistance}}};
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &pose);
    vr::VROverlay()->SetOverlayWidthInMeters(overlay, widthMeters);
    vr::VROverlay()->SetOverlayAlpha(overlay, std::clamp(m.alpha, 0.0f, 1.0f));
}

vr::VROverlayHandle_t createOverlay(const std::string& key, uint32_t sortOrder) {
    vr::VROverlayHandle_t handle;
    vr::EVROverlayError e = vr::VROverlay()->CreateOverlay(key.c_str(), "Frame power alert", &handle);
    if (e != vr::VROverlayError_None) {
        std::fprintf(stderr, "CreateOverlay %s: %s\n", key.c_str(), vr::VROverlay()->GetOverlayErrorNameFromEnum(e));
        std::exit(1);
    }
    vr::VROverlay()->SetOverlaySortOrder(handle, sortOrder);
    return handle;
}

void upload(vr::VROverlayHandle_t overlay, const std::vector<uint8_t>& pixels, int w, int h) {
    vr::VROverlay()->SetOverlayRaw(overlay, const_cast<uint8_t*>(pixels.data()), w, h, 4);
}

// Shows or hides an overlay, calling into SteamVR only on a change.
struct Layer {
    vr::VROverlayHandle_t overlay;
    bool shown = false;

    void set(bool show) {
        if (show == shown) return;
        shown = show;
        if (show) vr::VROverlay()->ShowOverlay(overlay);
        else vr::VROverlay()->HideOverlay(overlay);
    }
};

}  // namespace

// --demo plays each alert once, alongside the running service, then exits.
int main(int argc, char** argv) {
    const bool demo = argc > 1 && std::strcmp(argv[1], "--demo") == 0;
    std::deque<Alert> demoQueue;
    if (demo) demoQueue = {Alert::NotCharging, Alert::LowBattery, Alert::Charging};

    signal(SIGCHLD, SIG_IGN);  // reap chime players

    const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
    if (!runtimeDir) {
        std::fprintf(stderr, "XDG_RUNTIME_DIR is not set\n");
        return 1;
    }
    const Chime alertChime(runtimeDir, "frame-power-alert-down", 880, 660);
    const Chime lowChime(runtimeDir, "frame-power-alert-low", 660, 440);
    const Chime chargingChime(runtimeDir, "frame-power-alert-up", 660, 880);

    vr::EVRInitError err;
    vr::VR_Init(&err, vr::VRApplication_Background);
    if (err != vr::VRInitError_None) {
        std::fprintf(stderr, "VR_Init: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(err));
        return 1;
    }

    const std::string key = demo ? "frame-power-alert.demo" : "frame-power-alert";
    PillRenderer renderer(kFontPath);

    // Content is uploaded once per alert while hidden; re-uploading a visible overlay flashes.
    vr::VROverlayHandle_t pill = createOverlay(key + ".pill", 0);

    // The countdown ring as a pie timer (see PillRenderer::ringSpinner). Textures are uploaded per alert while hidden.
    Layer filler{createOverlay(key + ".ring-filler", 1)};
    Layer spinner{createOverlay(key + ".ring-spinner", 2)};
    Layer mask{createOverlay(key + ".ring-mask", 3)};

    // Only transitions alert; the state at startup is taken as-is.
    Power stable = readPower(), pending = stable;
    int pendingReads = kStableReads;
    bool lowBattery = isLowBattery(readBatteryPercent());
    auto lastPoll = Clock::now(), shownAt = Clock::now();
    bool visible = false;
    float showFor = 0;
    Motion placed{-1, -1};
    float placedRemaining = -1;

    auto placeAll = [&](Motion m, float remaining) {
        if (m == placed && remaining == placedRemaining) return;
        float mpp = pillMetersPerPixel(m);
        float ringW = kRingTex / kRingTexScale * mpp, ringX = (kCircleX - kTexW / 2.0f) * mpp;
        if (!(m == placed)) {
            place(pill, m, kTexW * mpp, 0);
            place(filler.overlay, m, ringW, ringX);
            place(mask.overlay, m, ringW, ringX);
        }
        place(spinner.overlay, m, ringW, ringX, (remaining * 2 - 1) * static_cast<float>(M_PI));
        placed = m;
        placedRemaining = remaining;
        spinner.set(remaining > 0);
        filler.set(remaining >= 0.5f);
        mask.set(remaining > 0 && remaining < 0.5f);
    };

    auto hideAll = [&] {
        vr::VROverlay()->HideOverlay(pill);
        for (Layer* l : {&filler, &spinner, &mask}) l->set(false);
    };

    auto show = [&](Alert a) {
        hideAll();
        showFor = a == Alert::Charging ? kChargingSecs : kAlertSecs;
        // Starts fully transparent, so the fresh upload can be shown straight away.
        upload(pill, renderer.content(styleFor(a), readBatteryPercent()), kTexW, kTexH);
        const Rgb accent = styleFor(a).accent;
        upload(filler.overlay, renderer.ringFiller(accent), kRingTex, kRingTex);
        upload(spinner.overlay, renderer.ringSpinner(accent), kRingTex, kRingTex);
        upload(mask.overlay, renderer.ringMask(accent), kRingTex, kRingTex);
        shownAt = Clock::now();
        visible = true;
        placed = {-1, -1};
        placeAll(motionAt(0, showFor), 1);
        vr::VROverlay()->ShowOverlay(pill);
        (a == Alert::Charging ? chargingChime : a == Alert::LowBattery ? lowChime : alertChime).play();
    };

    while (true) {
        vr::VREvent_t event;
        while (vr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
            if (event.eventType == vr::VREvent_Quit) {
                vr::VRSystem()->AcknowledgeQuit_Exiting();
                vr::VR_Shutdown();
                return 0;
            }
        }

        if (demo && !visible) {
            if (demoQueue.empty()) break;
            show(demoQueue.front());
            demoQueue.pop_front();
        }

        if (!demo && Clock::now() - lastPoll >= kPollInterval) {
            lastPoll = Clock::now();
            Power reading = readPower();
            pendingReads = reading == pending ? pendingReads + 1 : 1;
            pending = reading;
            if (pendingReads == kStableReads && pending != stable) {
                stable = pending;
                show(stable == Power::Charging ? Alert::Charging : Alert::NotCharging);
            }

            bool low = isLowBattery(readBatteryPercent());
            if (low && !lowBattery) show(Alert::LowBattery);
            lowBattery = low;
        }

        if (visible) {
            float t = secondsSince(shownAt);
            if (t >= showFor + kExitSecs) {
                hideAll();
                visible = false;
            } else {
                placeAll(motionAt(t, showFor), std::max(0.0f, 1 - t / showFor));
            }
        }

        std::this_thread::sleep_for(visible ? kAnimationTick : kIdleTick);
    }
    vr::VR_Shutdown();
}
