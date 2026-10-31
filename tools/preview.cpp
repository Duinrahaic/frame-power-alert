// Renders each alert state to a PNG, to check the look without a headset.
// Usage: preview <font.ttf> <out-dir>
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "pill.h"

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: preview <font.ttf> <out-dir>\n"); return 2; }
    PillRenderer renderer(argv[1]);
    const struct { Alert alert; const char* file; } states[] = {
        {Alert::NotCharging, "not-charging.png"},
        {Alert::LowBattery, "low-battery.png"},
        {Alert::Charging, "charging.png"}};
    for (const auto& s : states) {
        const auto px = renderer.composite(styleFor(s.alert), 62, 0.6f);
        std::string path = std::string(argv[2]) + "/" + s.file;
        stbi_write_png(path.c_str(), kTexW, kTexH, 4, px.data(), kTexW * 4);
    }
}
