// Lucide icons (https://lucide.dev, ISC license), 24×24 viewBox, stroke-width 2, round caps and joins.
// Path data is copied from the SVGs; each <rect x="2" y="6" width="16" height="12" rx="2"/> is written as the
// equivalent path.
#pragma once

namespace icons {

constexpr const char* kBatteryRect = "M4 6h12a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2z";

constexpr const char* kBatteryFull[] = {"M10 10v4", "M14 10v4", "M22 14v-4", "M6 10v4", kBatteryRect};

constexpr const char* kBatteryLow[] = {"M22 14v-4", "M6 14v-4", kBatteryRect};

constexpr const char* kBatteryCharging[] = {
    "m11 7-3 5h4l-3 5",
    "M14.856 6H16a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2h-2.935",
    "M22 14v-4",
    "M5.14 18H4a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h2.936",
};

}  // namespace icons
