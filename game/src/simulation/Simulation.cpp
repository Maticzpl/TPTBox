#include "Simulation.h"
#include "ElementClasses.h"
#include "ElementDefs.h"
#include "../util/vector_op.h"
#include "../util/util.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <limits>

Simulation::Simulation():
    paused(false)
{
    std::fill(&pmap[0][0][0], &pmap[ZRES][YRES][XRES], 0);
    // std::fill(&parts[0], &parts[NPARTS], 0);

    pfree = 1;
    maxId = 0;
}


void Simulation::create_part(uint x, uint y, uint z, ElementType type) {
    if (pmap[z][y][x]) return; // TODO


    // Create new part
    // Note: should it allow creation off screen? 
    parts[pfree].id = pfree;
    parts[pfree].type = type;
    parts[pfree].x = x;
    parts[pfree].y = y;
    parts[pfree].z = z;
    parts[pfree].vx = 0.0f;
    parts[pfree].vy = 0.0f;
    parts[pfree].vz = 0.0f;

    pmap[z][y][x] = pfree;
    pfree++;
    maxId = std::max(maxId, pfree);
}

void Simulation::update() {
    for (int i = 0; i <= maxId; i++)
    {
        auto &part = parts[i];
        if (!part.type) continue; // TODO: can probably be more efficient

        move_behavior(i);

    }
}

/**
 * @brief Perform a raycast starting at (x,y,z) with max displacement
 *        and direction indicated by (vx, vy, vz). The last empty voxel
 *        before colliding is written to (ox, oy, oz)
 * @see https://github.com/francisengelmann/fast_voxel_traversal/tree/master
 *        Licensed under LICENSES/
 */
void Simulation::raycast(uint x, uint y, uint z, float vx, float vy, float vz, uint &ox, uint &oy, uint &oz) {
    float dx = (vx >= 0) * 2 - 1;
    float dy = (vy >= 0) * 2 - 1;
    float dz = (vz >= 0) * 2 - 1;

    const Vector3T<short> last_voxel{ (short)(x + std::floor(vx)), (short)(y + std::floor(vy)), (short)(z + std::floor(vz)) };
    const Vector3 next_voxel_boundary{ x + dx, y + dy, z + dz };

    // tMaxX, tMaxY, tMaxZ -- distance until next intersection with voxel-border
    // the value of t at which the ray crosses the first vertical voxel boundary
    float tMaxX = (vx != 0) ? (next_voxel_boundary.x - x) / vx : std::numeric_limits<float>::max();
    float tMaxY = (vy != 0) ? (next_voxel_boundary.y - y) / vy : std::numeric_limits<float>::max();
    float tMaxZ = (vz != 0) ? (next_voxel_boundary.z - z) / vz : std::numeric_limits<float>::max();

    // tDeltaX, tDeltaY, tDeltaZ --
    // how far along the ray we must move for the horizontal component to equal the width of a voxel
    // the direction in which we traverse the grid
    // can only be FLT_MAX if we never go in that direction
    float tDeltaX = (vx != 0) ? 1.0f / vx * dx : std::numeric_limits<float>::max();
    float tDeltaY = (vy != 0) ? 1.0f / vy * dy : std::numeric_limits<float>::max();
    float tDeltaZ = (vz != 0) ? 1.0f / vz * dz : std::numeric_limits<float>::max();

    Vector3T<short> current_voxel{ (short)x, (short)y, (short)z };
    Vector3T<short> diff{ 0, 0, 0 };

    bool neg_ray = false;
    if (vx < 0 && current_voxel.x != last_voxel.x) { diff.x--; neg_ray = true; }
    if (vy < 0 && current_voxel.y != last_voxel.y) { diff.y--; neg_ray = true; }
    if (vz < 0 && current_voxel.z != last_voxel.z) { diff.z--; neg_ray = true; }

    auto pmapOccupied = [this](const Vector3T<short> &loc) -> bool {
        if (loc.y < 1 || loc.y >= YRES - 1 || loc.x < 1 || loc.x >= XRES - 1 || loc.z < 1 || loc.z >= ZRES - 1)
            return true;
        return pmap[loc.z][loc.y][loc.x] > 0;
    };

    if (neg_ray) {
        current_voxel += diff;
        if (pmapOccupied(current_voxel)) {
            ox = current_voxel.x;
            oy = current_voxel.y;
            oz = current_voxel.z;
            return;
        }
    }

    Vector3T<short> previous_voxel = current_voxel;

    while (current_voxel != last_voxel) {
        previous_voxel = current_voxel;

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                current_voxel.x += dx;
                tMaxX += tDeltaX;
            } else {
                current_voxel.z += dz;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                current_voxel.y += dy;
                tMaxY += tDeltaY;
            } else {
                current_voxel.z += dz;
                tMaxZ += tDeltaZ;
            }
        }
    
        if (pmapOccupied(current_voxel)) {
            ox = previous_voxel.x;
            oy = previous_voxel.y;
            oz = previous_voxel.z;
            return;
        }
    }
}


/**
 * @brief Perform default movement behaviors for different
 *        states of matter (ie powder, fluid, etc...)
 * 
 * @param idx 
 */
void Simulation::move_behavior(int idx) {
    const auto el = GetElements()[parts[idx].type];
    if (el.State == ElementState::TYPE_SOLID) return; // Solids can't move

    const auto part = parts[idx];
    // if (part.vx == 0.0f && part.vy == 0.0 && part.vz == 0.0) return; // No velocity

    int x = util::roundf(part.x);
    int y = util::roundf(part.y);
    int z = util::roundf(part.z);

    if (el.State == ElementState::TYPE_LIQUID || el.State == ElementState::TYPE_POWDER) {
        // TODO: gravity
        // If nothing below go straight down
        if (pmap[z][y - 1][x] == 0) {
            try_move(idx, x, y - 1, z);
            return;
        }

        // Check surroundings or below surroundings
        std::array<int, 2 * 8> next; // 8 neighboring spots it could go
        int c = 0;

        const int ylvl = el.State == ElementState::TYPE_LIQUID ? y : y - 1;

        if (y > 1 && pmap[z][y - 1][x] > 0) {
            for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dz) continue;
                if ((int)z + dz < 1 || (int)z + dz >= ZRES - 1 || (int)x + dx < 1 || (int)x + dx >= XRES - 1) continue;

                if (pmap[z + dz][y][x + dx] == 0 && pmap[z + dz][ylvl][x + dx] == 0) {
                    next[c] = x + dx;
                    next[c + 1] = z + dz;
                    c += 2;
                }
            }
            
            if (c) {
                int j = rand() % (c / 2);
                x = next[2 * j];
                y = ylvl;
                z = next[2 * j + 1];
                try_move(idx, x, y, z);
            }
        }
    }
    else if (el.State == ElementState::TYPE_GAS) {
        std::array<int, 3 * 26> next; // 26 neighboring spots it could go
        int c = 0;

        for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dz) continue;
            if ((int)y + dy < 1 || (int)y + dy >= YRES - 1 || (int)z + dz < 1 || z + dz >= ZRES - 1 || (int)x + dx < 1 || x + dx >= XRES - 1) continue;

            if (pmap[z + dz][y + dy][x + dx] == 0) {
                next[c] = x + dx;
                next[c + 1] = y + dy;
                next[c + 2] = z + dz;
                c += 3;
            }
        }
        
        if (c) {
            int j = rand() % (c / 3);
            x = next[j * 3];
            y = next[j * 3 + 1];
            z = next[j * 3 + 2];
            try_move(idx, x, y, z);
        }
    }
}

/**
 * @brief Try to move a particle to target location
 *        Updates the pmap and particle properties
 * 
 * @param idx Index in parts
 * @param x Target x
 * @param y Target y
 * @param z Target z
 */
void Simulation::try_move(int idx, uint x, uint y, uint z) {
    // TODO: consider edge mode
    if (x < 1 || y < 1 || z < 1 || x >= XRES - 1 || y >= YRES - 1 || z >= ZRES - 1)
        return;
    
    // TODO: can swap
    if (pmap[z][y][x]) return;
    
    int oldx = util::roundf(parts[idx].x);
    int oldy = util::roundf(parts[idx].y);
    int oldz = util::roundf(parts[idx].z);
    pmap[oldz][oldy][oldx] = 0;

    // TODO: suffers rounding issues
    parts[idx].x = x;
    parts[idx].y = y;
    parts[idx].z = z;
    pmap[z][y][x] = parts[idx].id;
}


