#include "Simulation.h"
#include "ElementClasses.h"
#include "ElementDefs.h"
#include "../util/vector_op.h"
#include "../util/math.h"

#include <omp.h>
#include <algorithm>
#include <iostream>
#include <tuple>
#include <vector>
#include <cmath>
#include <limits>
#include <cstring>

Simulation::Simulation():
    paused(false),
    air(*this)
{
    std::fill(&pmap[0][0][0], &pmap[ZRES][YRES][XRES], 0);
    std::fill(&photons[0][0][0], &photons[ZRES][YRES][XRES], 0);

    std::fill(&max_y_per_zslice[0], &max_y_per_zslice[ZRES - 2], YRES - 1);
    std::fill(&min_y_per_zslice[0], &min_y_per_zslice[ZRES - 2], 1);
    // std::fill(&parts[0], &parts[NPARTS], 0);

    pfree = 1;
    maxId = 0;
    frame_count = 0;
    parts_count = 0;
    gravity_mode = GravityMode::VERTICAL;

    // gravity_mode = GravityMode::RADIAL; // TODO
    

    // ---- Threads ------
    constexpr int MIN_CASUALITY_RADIUS = 4; // Width of each slice = 2 * this
    constexpr int MAX_THREADS = ZRES / (4 * MIN_CASUALITY_RADIUS); // Threads = number of slices / 2

    sim_thread_count = std::min(omp_get_max_threads(), MAX_THREADS);
    max_ok_causality_range = ZRES / (sim_thread_count * 4);
    actual_thread_count = 0;

    // TODO: singleton?
    _init_can_move();
}

Simulation::~Simulation() {}


void Simulation::_init_can_move() {
    ElementType movingType, destinationType;
    const auto &elements = GetElements();

    for (movingType = 1; movingType <= ELEMENT_COUNT; movingType++) {
        // All elements swap with NONE
        can_move[movingType][PT_NONE] = PartSwapBehavior::SWAP;
        can_move[PT_NONE][movingType] = PartSwapBehavior::SWAP;

		for (destinationType = 1; destinationType <= ELEMENT_COUNT; destinationType++) {
            // Heavier elements can swap with lighter ones
			if (elements[movingType].Weight > elements[destinationType].Weight)
				can_move[movingType][destinationType] = PartSwapBehavior::SWAP;

            // All energy particles can occupy same space
            if (elements[movingType].State == ElementState::TYPE_ENERGY && elements[destinationType].State == ElementState::TYPE_ENERGY)
                can_move[movingType][destinationType] = PartSwapBehavior::OCCUPY_SAME;
        }
    }
}

void Simulation::cycle_gravity_mode() {
    gravity_mode = static_cast<GravityMode>( ((int)gravity_mode + 1) % 3 );
}

part_id Simulation::create_part(const coord_t x, const coord_t y, const coord_t z, const ElementType type) {
    #ifdef DEBUG
    if (REVERSE_BOUNDS_CHECK(x, y, z))
        throw std::invalid_argument("Input to sim.create_part must be in bounds, got " +
            std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z));
    #endif

    auto is_energy = GetElements()[type].State == ElementState::TYPE_ENERGY;
    const auto part_map = is_energy ? photons : pmap;

    if (part_map[z][y][x]) return PartErr::ALREADY_OCCUPIED;
    if (pfree >= NPARTS) return PartErr::PARTS_FULL;

    // Create new part
    // Note: should it allow creation off screen? TODO
    const part_id next_pfree = parts[pfree].id < 0 ? -parts[pfree].id : pfree + 1;
    const part_id old_pfree = pfree;

    parts[pfree].flag[PartFlags::UPDATE_FRAME] = 1 - (frame_count & 1);
    parts[pfree].flag[PartFlags::MOVE_FRAME]   = 1 - (frame_count & 1);
    parts[pfree].flag[PartFlags::IS_ENERGY]    = is_energy;

    parts[pfree].id = pfree;
    parts[pfree].type = type;
    parts[pfree].x = x;
    parts[pfree].y = y;
    parts[pfree].z = z;
    parts[pfree].rx = x;
    parts[pfree].ry = y;
    parts[pfree].rz = z;
    parts[pfree].vx = 0.0f;
    parts[pfree].vy = 0.0f;
    parts[pfree].vz = 0.0f;
    if (paused) {
        if (_should_do_lighting(parts[pfree])) {
            graphics.ao_blocks[AO_FLAT_IDX(x, y, z)]++;
            _update_shadow_map(x, y, z);
        }
        parts_count++;
    }

    part_map[z][y][x] = PMAP(type, pfree);
    _set_color_data_at(x, y, z, &parts[pfree]);

    maxId = std::max(maxId, pfree + 1);
    pfree = next_pfree;
    return old_pfree;
}

void Simulation::kill_part(const part_id i) {
    auto &part = parts[i];
    if (part.type <= 0) return;

    coord_t x = part.rx;
    coord_t y = part.ry;
    coord_t z = part.rz;

    if (pmap[z][y][x] && ID(pmap[z][y][x]) == i)
        pmap[z][y][x] = 0;
    else if (photons[z][y][x] && ID(photons[z][y][x]) == i)
        photons[z][y][x] = 0;

    part.type = PT_NONE;
    part.flag[PartFlags::IS_ENERGY] = 0;

    if (i == maxId && i > 0)
        maxId--;

    _set_color_data_at(x, y, z, nullptr);
    if (paused) {
        if (_should_do_lighting(part))
            graphics.ao_blocks[AO_FLAT_IDX(x, y, z)]--;
        graphics.shadows_force_update = true;
        parts_count--;
    }
    part.id = -pfree;
    pfree = i;
}

void Simulation::update_zslice(const coord_t pz) {
    if (pz < 1 || pz >= ZRES - 1)
        return;

    // Dirty rect does not have any impact on performance
    // for these sizes of YRES / XRES (could slow/speed up by a factor of a few ns)
    for (coord_t py = min_y_per_zslice[pz - 1]; py < max_y_per_zslice[pz - 1]; py++)
    for (coord_t px = 1; px < XRES - 1; px++) {
        if (pmap[pz][py][px])
            update_part(ID(pmap[pz][py][px]));
        if (photons[pz][py][px])
            update_part(ID(photons[pz][py][px]));
    }
}

void Simulation::update_part(const part_id i, const bool consider_causality) {
    auto &part = parts[i];

    // Since a particle might move we might update it again
    // if it moves in the direction of scanning the pmap array
    // To avoid this, it stores the parity of the frame count
    // and only updates the particle if the last frame it was updated
    // has the same parity
    const auto frame_count_parity = frame_count & 1;
    const coord_t x = part.rx;
    const coord_t y = part.ry;
    const coord_t z = part.rz;

    // Update causality constraint: depends on move_behavior and update step
    // Velocity can be set but the particle cannot move beyond its causality radius
    if (part.flag[PartFlags::UPDATE_FRAME] != frame_count_parity) { // Need to update
        const auto &el = GetElements()[part.type];

        if (consider_causality && el.Causality > max_ok_causality_range)
            return;
        part.flag[PartFlags::UPDATE_FRAME] = frame_count_parity > 0;

        // Air acceleration
        part.vx *= el.Loss;
        part.vy *= el.Loss;
        part.vz *= el.Loss;

        if (el.Advection) {
            const auto &airCell = air.cells[z / AIR_CELL_SIZE][y / AIR_CELL_SIZE][x / AIR_CELL_SIZE];
            part.vx += el.Advection * airCell.data[VX_IDX];
            part.vy += el.Advection * airCell.data[VY_IDX];
            part.vz += el.Advection * airCell.data[VZ_IDX];
        }

        if (el.Update) {
            const auto result = el.Update(*this, i, x, y, z, parts, pmap);
            if (result == -1) return;
        }
        move_behavior(i); // Apply element specific movement, like powder / liquid spread
    }

    // Movement causality constraint: depends on velocity
    if (part.flag[PartFlags::MOVE_FRAME] != frame_count_parity) { // Need to move
        // Causality check only needs to consider Z direction
        // since threads operate on XY slices, so only z velocity will
        // move a part into another thread domain
        if (consider_causality && fabs(part.vz) > max_ok_causality_range)
            return;
        part.flag[PartFlags::MOVE_FRAME] = frame_count_parity > 0;

        if (part.vx || part.vy || part.vz)
            _raycast_movement(i, x, y, z); // Apply velocity to displacement
    }
}

void Simulation::update() {
    if (paused) {
        if (graphics.shadows_force_update)
            _force_update_all_shadows();
        return;
    }

    // air.update(); // TODO

    #pragma omp parallel num_threads(sim_thread_count)
    { 
        const int thread_count = omp_get_num_threads();
        const int z_chunk_size = (ZRES - 2) / (2 * thread_count) + 1;
        const int tid = omp_get_thread_num();
        coord_t z_start = z_chunk_size * (2 * tid);

        if (tid == 0)
            actual_thread_count = thread_count;

        for (coord_t z = z_start; z < z_chunk_size + z_start; z++)
            update_zslice(z);

        #pragma omp barrier // Synchronize threads before processing 2nd chunk

        z_start = z_chunk_size * (2 * tid + 1);
        for (coord_t z = z_start; z < z_chunk_size + z_start; z++)
            update_zslice(z);
    }

    recalc_free_particles();
    frame_count++;
}

void Simulation::recalc_free_particles() {
    parts_count = 0;
    part_id newMaxId = 0;
    std::fill(&max_y_per_zslice[0], &max_y_per_zslice[ZRES - 2], 0);
    std::fill(&min_y_per_zslice[0], &min_y_per_zslice[ZRES - 2], YRES - 1);
    std::fill(&graphics.shadow_map[0][0], &graphics.shadow_map[SHADOW_MAP_Y][SHADOW_MAP_X], 0);
    graphics.ao_blocks.fill(0);

    for (part_id i = 0; i <= maxId; i++) {
        auto &part = parts[i];
        if (!part.type) continue;

        parts_count++;
        newMaxId = i;

        const coord_t x = part.rx;
        const coord_t y = part.ry;
        const coord_t z = part.rz;

        // Ambient occlusion and shadow rules
        if (part.id == ID(pmap[z][y][x]) && _should_do_lighting(part)) {
            graphics.ao_blocks[AO_FLAT_IDX(x, y, z)]++;
            _update_shadow_map(x, y, z);
        }

        // Pmap / other cache
        min_y_per_zslice[z - 1] = std::min(y, min_y_per_zslice[z - 1]);
        max_y_per_zslice[z - 1] = std::max(y, max_y_per_zslice[z - 1]);

        // Pmap and graphics
        auto &map = part.flag[PartFlags::IS_ENERGY] ? photons : pmap;
        if (GetElements()[part.type].Graphics)
            _set_color_data_at(part.rx, part.ry, part.rz, &part);
        if (!map[z][y][x]) {
            map[z][y][x] = PMAP(part.type, i);
            _set_color_data_at(x, y, z, &part);
        }

        update_part(i, false);
    }
    maxId = newMaxId + 1;
}


// Octree & color data updates
// ------------------------
void Simulation::_set_color_data_at(const coord_t x, const coord_t y, const coord_t z, const Particle * part) {
    uint32_t new_color = 0;
    util::Bitset8 new_flags = 0;

    if (part != nullptr) {
        const auto &el = GetElements()[part->type];
        new_color = el.Color.as_ABGR();
        new_flags = util::Bitset8(el.GraphicsFlags);

        if (el.Graphics) {
            RGBA color_out;
            el.Graphics(*this, *part, part->rx, part->ry, part->rz, color_out, new_flags);
            new_color = color_out.as_ABGR();
        }
    }

    unsigned int idx = FLAT_IDX(x, y, z);
    if (graphics.color_data[idx] == new_color && graphics.color_flags[idx] == new_flags) return; // Color did not actually change

    auto &tree = graphics.octree_blocks[
        (x / OCTREE_BLOCK_DIM) + (y / OCTREE_BLOCK_DIM) * X_BLOCKS +
        (z / OCTREE_BLOCK_DIM) * X_BLOCKS * Y_BLOCKS];

    graphics.color_data_modified[idx / COLOR_DATA_CHUNK_SIZE] = 0xFF;
    graphics.color_data[idx] = new_color;
    graphics.color_flags[idx] = new_flags;
    tree.modified = 0xFF;

    if (new_color)
        tree.insert(x & (OCTREE_BLOCK_DIM - 1), y & (OCTREE_BLOCK_DIM - 1), z & (OCTREE_BLOCK_DIM - 1));
    else
        tree.remove(x & (OCTREE_BLOCK_DIM - 1), y & (OCTREE_BLOCK_DIM - 1), z & (OCTREE_BLOCK_DIM - 1));
}

void Simulation::_update_shadow_map(const coord_t x, const coord_t y, const coord_t z) {
    unsigned int proj_x = (static_cast<unsigned int>(x) + (ZRES - z)) / SHADOW_MAP_SCALE;
    unsigned int proj_y = (static_cast<unsigned int>(y) + (ZRES - z)) / SHADOW_MAP_SCALE;
    graphics.shadow_map[proj_y][proj_x] = std::max(graphics.shadow_map[proj_y][proj_x], static_cast<uint8_t>(z));
}

bool Simulation::_should_do_lighting(const Particle &part) {
    return !GetElements()[part.type].GraphicsFlags[GraphicsFlagsIdx::NO_LIGHTING];
}

void Simulation::_force_update_all_shadows() {
    std::fill(&graphics.shadow_map[0][0], &graphics.shadow_map[SHADOW_MAP_Y][SHADOW_MAP_X], 0);
    graphics.shadows_force_update = false;

    #pragma parallel for
    for (part_id i = 0; i <= maxId; i++) {
        auto &part = parts[i];
        if (!part.type) continue;
        if (part.id == ID(pmap[part.rz][part.ry][part.rx]) && _should_do_lighting(part))
            _update_shadow_map(part.rx, part.ry, part.rz);
    }
}
