#ifndef SIMULATION_H
#define SIMULATION_H

#include "Particle.h"
#include "SimulationDef.h"

class Simulation {
public:
    bool paused;

    Particle parts[NPARTS];
    unsigned int pmap[ZRES][YRES][XRES];

    // TODO: have some sort of linked list to determine free particle spots
    int pfree;
    int maxId;

    uint32_t frame_count; // Monotomic frame counter, will overflow in ~824 days @ 60 FPS. Do not keep the program open for this long

    Simulation();
    int create_part(const coord_t x, const coord_t y, const coord_t z, const ElementType type);
    void kill_part(const int id);

    void update();

    void move_behavior(const int idx);
    void try_move(const int idx, const float x, const float y, const float z);
    void swap_part(const coord_t x1, const coord_t y1, const coord_t z1,
        const coord_t x2, const coord_t y2, const coord_t z2,
        const int id1, const int id2);

    bool raycast(const coord_t x, const coord_t y, const coord_t z, const float vx, const float vy, const float vz,
        coord_t &ox, coord_t &oy, coord_t &oz) const;

};

#endif