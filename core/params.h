#pragma once
#include <cstddef>
#include <cstdint>

namespace dev {

struct ParamDef {
    uint8_t id;
    const char* name;
    int32_t min;
    int32_t max;
    int32_t def;
    const char* unit;
};

inline constexpr ParamDef kParams[] = {
    {1, "SampleRate",  1,    1000, 100, "Hz"},
    {2, "Gain",        1,    64,   1,   "x"},
    {3, "Mode",        0,    2,    0,   "0=idle 1=run 2=cal"},
    {4, "Threshold",   -1000, 1000, 0,  "mV"},
    {5, "LedEnabled",  0,    1,    1,   "0/1"},
};

inline constexpr size_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);

inline const ParamDef* findParam(uint8_t id) {
    for (const auto& p : kParams)
        if (p.id == id) return &p;
    return nullptr;
}

}  // namespace dev
