#ifndef SIMULATION_H
#define SIMULATION_H

#include "Particle.h"

using uint = unsigned int;
using ElementType = uint;

constexpr unsigned int XRES = 100;
constexpr unsigned int YRES = 100;
constexpr unsigned int ZRES = 100;
constexpr unsigned int NPARTS = XRES * YRES * ZRES;

class Simulation {
public:
    bool paused;

    Particle parts[NPARTS];
    unsigned int pmap[ZRES][YRES][XRES];

    // TODO: have some sort of linked list to determine free particle spots
    int pfree;
    int maxId;

    Simulation();
    void create_part(uint x, uint y, uint z, ElementType type);
    void kill_part(uint id);

    void update();
};

#endif