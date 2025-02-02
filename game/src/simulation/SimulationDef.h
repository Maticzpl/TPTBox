#ifndef SIMULATION_DEF_H
#define SIMULATION_DEF_H

#include "stdint.h"

using coord_t = uint8_t;
using signed_coord_t = int16_t;
using ElementType = unsigned int;

// Must be signed, negative values are used for pfree
// and error codes in some functions
using part_id = int32_t;
using pmap_id = int32_t; // This is ID and type merged into 1 32 bit value

// Pmap and photons store positive ids in this format:
// [TYP 10 bits][ID 22 bits]
constexpr uint32_t PMAP_ID_BITS = 22;
constexpr uint16_t PT_NUM = 1 << (32 - PMAP_ID_BITS); // Max number of elements possible

#define ID(r) (r & ((1 << PMAP_ID_BITS) - 1))
#define TYP(r) (r >> PMAP_ID_BITS)
#define PMAP(t, i) (((uint32_t)t << PMAP_ID_BITS) | (uint32_t)i)

constexpr uint16_t ELEMENT_COUNT = __GLOBAL_ELEMENT_COUNT;

// Should be <= 256, larger values may break it especially for morton codes
// in util and the part fragment shader
constexpr unsigned int XRES = 200;
constexpr unsigned int YRES = 200;
constexpr unsigned int ZRES = 200;
constexpr unsigned int NPARTS = XRES * YRES * ZRES;

constexpr unsigned int SHADOW_MAP_SCALE = 1; // Mostly unused, needs to be set in shader as well
constexpr unsigned int SHADOW_MAP_X = (XRES + ZRES) / SHADOW_MAP_SCALE;
constexpr unsigned int SHADOW_MAP_Y = (YRES + ZRES) / SHADOW_MAP_SCALE;

constexpr float MAX_VELOCITY = 50.0f;

constexpr bool BOUNDS_CHECK(int x, int y, int z) {
    return x > 0 && x < XRES - 1 && y > 0 && y < YRES - 1 && z > 0 && z < ZRES - 1;
}
constexpr bool REVERSE_BOUNDS_CHECK(int x, int y, int z) {
    return x < 1 || x >= XRES - 1 || y < 1 || y >= YRES - 1 || z < 1 || z >= ZRES - 1;
}
constexpr uint32_t FLAT_IDX(coord_t x, coord_t y, coord_t z) {
    return x + y * XRES + z * (XRES * YRES);
}

enum class PartSwapBehavior: uint8_t {
    NOOP = 0,
    SWAP = 1,
    OCCUPY_SAME = 2,
    SPECIAL = 3,
    NOT_EVALED_YET = 4 // Sentinel value, do not use in normal cases
};

namespace PartErr {
    constexpr part_id ALREADY_OCCUPIED = -1;
    constexpr part_id PARTS_FULL = -3;
};

#endif