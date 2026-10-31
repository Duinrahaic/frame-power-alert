// Two-note "bo-boop" chimes, synthesized once into WAV files and played through PipeWire.
#pragma once
#include <spawn.h>
#include <unistd.h>  // environ

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

class Chime {
public:
    // Writes the chime to <dir>/<name>.wav; notes are in Hz.
    Chime(const std::string& dir, const char* name, float firstHz, float secondHz)
        : path_(dir + "/" + name + ".wav") {
        std::vector<int16_t> samples;
        appendNote(samples, firstHz, kNoteSecs);
        samples.resize(samples.size() + static_cast<size_t>(kGapSecs * kRate));
        appendNote(samples, secondHz, kNoteSecs * 1.6f);
        writeWav(samples);
    }

    // Fire and forget; children are reaped automatically because main ignores SIGCHLD.
    void play() const {
        const char* argv[] = {"pw-play", "--volume", kVolume, path_.c_str(), nullptr};
        pid_t pid;
        if (posix_spawnp(&pid, "pw-play", nullptr, nullptr, const_cast<char**>(argv), environ) != 0)
            std::perror("pw-play");
    }

private:
    static constexpr int kRate = 48000;
    static constexpr float kNoteSecs = 0.09f, kGapSecs = 0.02f;
    static constexpr const char* kVolume = "0.5";

    // A sine with a soft second harmonic, quick attack and exponential decay.
    static void appendNote(std::vector<int16_t>& out, float hz, float secs) {
        const int n = static_cast<int>(secs * kRate);
        for (int i = 0; i < n; i++) {
            float t = static_cast<float>(i) / kRate;
            float env = std::min(1.0f, t / 0.005f) * std::exp(-t * 5 / secs);
            float s = std::sin(2 * static_cast<float>(M_PI) * hz * t) + 0.25f * std::sin(4 * static_cast<float>(M_PI) * hz * t);
            out.push_back(static_cast<int16_t>(s / 1.25f * env * 32767 * 0.8f));
        }
    }

    void writeWav(const std::vector<int16_t>& samples) const {
        FILE* f = std::fopen(path_.c_str(), "wb");
        if (!f) { std::perror(path_.c_str()); std::exit(1); }
        auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
        const uint32_t bytes = static_cast<uint32_t>(samples.size() * 2);
        std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
        std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
        std::fwrite("data", 1, 4, f); u32(bytes);
        std::fwrite(samples.data(), 2, samples.size(), f);
        std::fclose(f);
    }

    std::string path_;
};
