#ifndef UTIL_COLOR_H
#define UTIL_COLOR_H

#include <iostream>
#include <format>
#include "stdint.h"

class RGBA {
public:
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    RGBA(const uint32_t color = 0xFFFFFFF):
        r{ static_cast<uint8_t>((color & 0xFF000000) >> 24) },
        g{ static_cast<uint8_t>((color & 0xFF0000) >> 16) },
        b{ static_cast<uint8_t>((color & 0xFF00) >> 8) },
        a{ static_cast<uint8_t>(color & 0xFF) } {}
    RGBA(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a = 0xFF):
        r(r), g(g), b(b), a(a) {}
    RGBA(const RGBA &other): r(other.r), b(other.b), g(other.g), a(other.a) {}

    void darken(float amt) {
        r *= amt;
        g *= amt;
        b *= amt;
    }
};

inline std::ostream& operator<<(std::ostream& os, const RGBA& color) {
    os <<  std::format("#{:x} {:x} {:x} {:x}", color.r, color.g, color.b, color.a);
    return os;
}

#endif