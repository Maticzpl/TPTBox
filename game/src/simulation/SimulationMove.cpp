#include "Simulation.h"
#include "ElementClasses.h"
#include "ElementDefs.h"
#include "../util/vector_op.h"
#include "../util/util.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <string>

/**
 * @brief Perform a raycast starting at (x,y,z) with max displacement
 *        and direction indicated by (vx, vy, vz). The last empty voxel
 *        before colliding is written to (ox, oy, oz)
 * @see https://github.com/francisengelmann/fast_voxel_traversal/tree/master
 *        Licensed under LICENSES/
 * @return Whether it terminated because it hit a voxel (true if yes)
 */
bool Simulation::raycast(const RaycastInput &in,  RaycastOutput &out, const auto &pmapOccupied) const {
    // For raycasts that stop early, ie right on the next frame, we can simply check
    // if there's a particle in the direction of the greatest velocity. This saves
    // ~5ms per frame on a grid of 350k water particles
    // When the grid is that full, statistically most particles will not be able to move
    // hence the "optimization"
    int largest_axis = util::argmax3(in.vx, in.vy, in.vz);
    bool early_stop = false;

    if (largest_axis == 0 && PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ (signed_coord_t)(in.x + (in.vx < 0 ? -1 : 1)), (signed_coord_t)in.y, (signed_coord_t)in.z })) {
        early_stop = true;
        out.faces = RayCast::FACE_X;
    }
    else if (largest_axis == 1 && PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ (signed_coord_t)in.x, (signed_coord_t)(in.y + (in.vy < 0 ? -1 : 1)), (signed_coord_t)in.z })) {
        early_stop = true;
        out.faces = RayCast::FACE_Y;
    }
    else if (largest_axis == 2 && PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ (signed_coord_t)in.x, (signed_coord_t)in.y, (signed_coord_t)(in.z + (in.vz < 0 ? -1 : 1)) })) {
        early_stop = true;
        out.faces = RayCast::FACE_Z;
    }
    if (early_stop) {
        out.x = in.x;
        out.y = in.y;
        out.z = in.z;
        out.move = PartSwapBehavior::NOOP;
        return true;
    }

    // Actual raycast --------------
    Vector3T<signed_coord_t> current_voxel{ (signed_coord_t)in.x, (signed_coord_t)in.y, (signed_coord_t)in.z };
    const Vector3T<signed_coord_t> last_voxel{
        (signed_coord_t)((signed_coord_t)in.x + util::ceil_proper(in.vx)),
        (signed_coord_t)((signed_coord_t)in.y + util::ceil_proper(in.vy)),
        (signed_coord_t)((signed_coord_t)in.z + util::ceil_proper(in.vz))
    };
    Vector3T<signed_coord_t> diff{ 0, 0, 0 };
    Vector3T<signed_coord_t> previous_voxel = current_voxel;

    const Vector3T<signed_coord_t> ray = last_voxel - current_voxel;

    // Step to take per direction (+-1)
    const float dx = (ray.x >= 0) * 2 - 1;
    const float dy = (ray.y >= 0) * 2 - 1;
    const float dz = (ray.z >= 0) * 2 - 1;

    const Vector3 next_voxel_boundary{ in.x + dx, in.y + dy, in.z + dz };

    // tMaxX, tMaxY, tMaxZ -- distance until next intersection with voxel-border
    // the value of t at which the ray crosses the first vertical voxel boundary
    float tMaxX = (ray.x != 0) ? (next_voxel_boundary.x - in.x) / ray.x : std::numeric_limits<float>::max();
    float tMaxY = (ray.y != 0) ? (next_voxel_boundary.y - in.y) / ray.y : std::numeric_limits<float>::max();
    float tMaxZ = (ray.z != 0) ? (next_voxel_boundary.z - in.z) / ray.z : std::numeric_limits<float>::max();

    // tDeltaX, tDeltaY, tDeltaZ --
    // how far along the ray we must move for the horizontal component to equal the width of a voxel
    // the direction in which we traverse the grid
    // can only be FLT_MAX if we never go in that direction
    const float tDeltaX = (ray.x != 0) ? 1.0f / ray.x * dx : std::numeric_limits<float>::max();
    const float tDeltaY = (ray.y != 0) ? 1.0f / ray.y * dy : std::numeric_limits<float>::max();
    const float tDeltaZ = (ray.z != 0) ? 1.0f / ray.z * dz : std::numeric_limits<float>::max();

    bool neg_ray = false;
    if (ray.x < 0 && current_voxel.x != last_voxel.x) { diff.x--; neg_ray = true; }
    if (ray.y < 0 && current_voxel.y != last_voxel.y) { diff.y--; neg_ray = true; }
    if (ray.z < 0 && current_voxel.z != last_voxel.z) { diff.z--; neg_ray = true; }

    // Get which "faces" to bounce off of
    // prev is now, final is the voxel we will collide with if we continue
    // down our current trajectory
    // Precondition: prev_loc != final_loc
    auto getFaces = [this, &pmapOccupied](const Vector3T<signed_coord_t> &prev_loc, const Vector3T<signed_coord_t> &final_loc) -> RayCast::RayHitFace {
        RayCast::RayHitFace faces = 0;

        if ((prev_loc.x != final_loc.x) + (prev_loc.y != final_loc.y) + (prev_loc.z != final_loc.z) == 1) {
            if (prev_loc.x != final_loc.x)
                faces |= RayCast::FACE_X;
            if (prev_loc.y != final_loc.y)
                faces |= RayCast::FACE_Y;
            if (prev_loc.z != final_loc.z)
                faces |= RayCast::FACE_Z;
        } else {
            if (PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ final_loc.x, prev_loc.y, prev_loc.z }))
                faces |= RayCast::FACE_X;
            if (PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ prev_loc.x, final_loc.y, prev_loc.z }))
                faces |= RayCast::FACE_Y;
            if (PartSwapBehavior::NOOP == pmapOccupied(Vector3T<signed_coord_t>{ prev_loc.x, prev_loc.y, final_loc.z }))
                faces |= RayCast::FACE_Z;
        }
        return faces;
    };

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

        if (PartSwapBehavior::NOOP == pmapOccupied(current_voxel)) {
            out.x = previous_voxel.x;
            out.y = previous_voxel.y;
            out.z = previous_voxel.z;
            out.move = PartSwapBehavior::SWAP;

            if (in.compute_faces)
                out.faces = getFaces(previous_voxel, current_voxel);
            return true;
        }
    }

    out.x = current_voxel.x;
    out.y = current_voxel.y;
    out.z = current_voxel.z;
    out.faces = 0; // No faces to bounce off
    out.move = PartSwapBehavior::SWAP;
    return false;
}

/**
 * @brief Perform default movement behaviors for different
 *        states of matter (ie powder, fluid, etc...)
 * @param idx 
 */
void Simulation::move_behavior(const part_id idx) {
    const auto &el = GetElements()[parts[idx].type];
    if (el.State == ElementState::TYPE_SOLID || el.State == ElementState::TYPE_ENERGY)
        return; // Solids can't move, energy doesn't have custom behavior

    auto &part = parts[idx];
    const coord_t x = part.rx;
    const coord_t y = part.ry;
    const coord_t z = part.rz;

    // Apply gravity
    Vector3 gravity_force{0.0f, 0.0f, 0.0f};
    if (el.Gravity) {
        switch (gravity_mode) {
            case GravityMode::ZERO_G:
                break;
            case GravityMode::VERTICAL:
                if (eval_move(idx, x, y - 1, z) != PartSwapBehavior::NOOP)
                    part.vy -= el.Gravity;
                break;
            case GravityMode::RADIAL:
                gravity_force = Vector3{ XRES / 2 - part.x, YRES / 2 - part.y, ZRES / 2 - part.z };
                gravity_force = util::norm_vector(gravity_force);
                part.vx += gravity_force.x * el.Gravity;
                part.vy += gravity_force.y * el.Gravity;
                part.vz += gravity_force.z * el.Gravity;
                break;
        }
    }

    if (el.State == ElementState::TYPE_LIQUID || el.State == ElementState::TYPE_POWDER) {
        bool is_liquid = el.State == ElementState::TYPE_LIQUID;

        switch (gravity_mode) {
            case GravityMode::ZERO_G:
                return;
            case GravityMode::VERTICAL: {
                if (y > 1 && eval_move(idx, x, y - 1, z) != PartSwapBehavior::NOOP) // Particle can move down
                    return;
                float dx, dz;
                do {
                    dx = rng.uniform(-el.Diffusion, el.Diffusion);
                    dz = rng.uniform(-el.Diffusion, el.Diffusion);
                } while (!dx && !dz);

                const int newy = is_liquid ? y : y - 1;
                if (REVERSE_BOUNDS_CHECK(x + std::round(dx), newy, z + std::round(dz)))
                    return;

                const float newyf = is_liquid ? part.y : part.y - 1.0f;
                bool can_move_y_check = is_liquid || (eval_move(idx, x + std::round(dx), y, z + std::round(dz)) != PartSwapBehavior::NOOP);
    
                if (can_move_y_check) {
                    // If we raycast and collide with another voxel we can't
                    // do the naive try_move
                    bool hit = false;

                    if (std::abs(dx) > 1.0f || std::abs(dz) > 1.0f) {
                        auto pmapOccupied = [idx, this](const Vector3T<signed_coord_t> &loc) -> PartSwapBehavior {
                            if (REVERSE_BOUNDS_CHECK(loc.x, loc.y, loc.z))
                                return PartSwapBehavior::NOOP;
                            if (TYP(pmap[loc.z][loc.y][loc.x]) == parts[idx].type)
                                return PartSwapBehavior::SWAP;
                            return eval_move(idx, loc.x, loc.y, loc.z);
                        };

                        RaycastOutput out;
                        hit = raycast(RaycastInput {
                            .x = x, .y = y, .z = z,
                            .vx = dx, .vy = 0.0f, .vz = dz,
                            .compute_faces = false
                        }, out, pmapOccupied);

                        if (hit)
                            try_move(idx, out.x, newyf, out.z, out.move);
                    }
                    if (!hit) // Haven't already tried to move in raycast
                        try_move(idx, part.x + dx, newyf, part.z + dz);
                }
                break;
            }
            case GravityMode::RADIAL: {
                // Generate random unit vector orthogonal to gravity for wiggling around
                // We only wiggle if
                // a) the element is a fluid
                // b) the element is a powder (not a fluid)
                //    and the powder is not stable (there is nothing "below"
                //    it, where below is the direction of gravity)
                if (is_liquid ||
                    eval_move(idx,
                            x + util::sign(gravity_force.x),
                            y + util::sign(gravity_force.y),
                            z + util::sign(gravity_force.z)
                        ) != PartSwapBehavior::NOOP
                ) {
                    Vector3 randv = rng.rand_perpendicular_vector(gravity_force);
                    randv = el.Diffusion * util::norm_vector(randv);

                    auto nx = x + randv.x;
                    auto ny = y + randv.y;
                    auto nz = z + randv.z;

                    // Technically this allows us to jump through walls in radial gravity
                    // but this makes the best spheres; raycasting made weird non-sphere artifacts
                    // TODO
                    if (nx >= 0 && ny >= 0 && nz >= 0) [[likely]]
                        try_move(idx, nx, ny, nz);
                }
                return;
            }
        }
    }
    else if (el.State == ElementState::TYPE_GAS) {
        Vector3 randv = rng.rand_norm_vector();
        auto nx = x + el.Diffusion * randv.x;
        auto ny = y + el.Diffusion * randv.y;
        auto nz = z + el.Diffusion * randv.z;

        if (nx >= 0 && ny >= 0 && nz >= 0) [[likely]]
            try_move(idx, nx, ny, nz);
    }
}

/**
 * @brief Try to move a particle to target location
 *        Updates the pmap and particle properties
 * @param idx Index in parts
 * @param x Target x
 * @param y Target y
 * @param z Target z
 */
void Simulation::try_move(const part_id idx, const float tx, const float ty, const float tz, PartSwapBehavior behavior) {
    const coord_t x = util::roundf(tx);
    const coord_t y = util::roundf(ty);
    const coord_t z = util::roundf(tz);

    // TODO: consider edge mode
    if (REVERSE_BOUNDS_CHECK(x, y, z))
        return;

    const coord_t oldx = parts[idx].rx;
    const coord_t oldy = parts[idx].ry;
    const coord_t oldz = parts[idx].rz;
    
    // Particle did not move, but target pos could
    // potentially have changed (ie +0.1) but not
    // enough to shift the rounding, so assign anyways
    if (x == oldx && y == oldy && z == oldz) {
        parts[idx].x = tx;
        parts[idx].y = ty;
        parts[idx].z = tz;

        parts[idx].rx = x;
        parts[idx].ry = y;
        parts[idx].rz = z;
        return;
    }

    auto part_map = parts[idx].flag[PartFlags::IS_ENERGY] ? photons : pmap;
    auto old_pmap_val = part_map[oldz][oldy][oldx];

    if (behavior == PartSwapBehavior::NOT_EVALED_YET)
        behavior = eval_move(idx, x, y, z);

    switch (behavior) {
        case PartSwapBehavior::NOOP:
            return;
        case PartSwapBehavior::SWAP:
            swap_part(x, y, z, oldx, oldy, oldz, ID(part_map[z][y][x]), idx);
            break;
        case PartSwapBehavior::OCCUPY_SAME:
            part_map[oldz][oldy][oldx] = 0;
            part_map[z][y][x] = old_pmap_val;
            break;
        // The special behavior is resolved into one of the three
        // cases above by eval_move
        #ifdef DEBUG
        case PartSwapBehavior::SPECIAL:
            throw std::invalid_argument("Particle of type " + std::to_string(parts[idx].type) + " in try_move() has unresolved move behavior of SPECIAL");
            break;
        #endif
    }

    parts[idx].x = tx;
    parts[idx].y = ty;
    parts[idx].z = tz;

    parts[idx].rx = x;
    parts[idx].ry = y;
    parts[idx].rz = z;
}

/**
 * @brief Spatially swap the particles at the two ids
 */
void Simulation::swap_part(const coord_t x1, const coord_t y1, const coord_t z1,
        const coord_t x2, const coord_t y2, const coord_t z2, const part_id id1, const part_id id2) {
    std::swap(parts[id1].x, parts[id2].x);
    std::swap(parts[id1].y, parts[id2].y);
    std::swap(parts[id1].z, parts[id2].z);

    std::swap(parts[id1].rx, parts[id2].rx);
    std::swap(parts[id1].ry, parts[id2].ry);
    std::swap(parts[id1].rz, parts[id2].rz);

    auto part1_is_e = parts[id1].flag[PartFlags::IS_ENERGY]; 
    auto part2_is_e = parts[id2].flag[PartFlags::IS_ENERGY];

    if (!part1_is_e && !part2_is_e)
        std::swap(pmap[z1][y1][x1], pmap[z2][y2][x2]);
    else if (part1_is_e && part2_is_e)
        std::swap(photons[z1][y1][x1], photons[z2][y2][x2]);
    else {
        // Swapping energy with regular. May cause problems
        // if we displace a pmap onto something that can't normally
        // be displayed, but this option shouldn't be used anyways
        std::swap(pmap[z1][y1][x1], pmap[z2][y2][x2]);
        std::swap(photons[z1][y1][x1], photons[z2][y2][x2]);
    }
}


// Try to move a particle with velocity to new location
void Simulation::_raycast_movement(const part_id idx, const coord_t x, const coord_t y, const coord_t z) {
    auto &part = parts[idx];
    part.vx = util::clampf(part.vx, -MAX_VELOCITY, MAX_VELOCITY);
    part.vy = util::clampf(part.vy, -MAX_VELOCITY, MAX_VELOCITY);
    part.vz = util::clampf(part.vz, -MAX_VELOCITY, MAX_VELOCITY);

    RaycastOutput out;
    coord_t sx = x; // Starting point of raycast, can change with repeated casts
    coord_t sy = y;
    coord_t sz = z;
    bool hit, no_move; // Whether we hit an edge or voxel, whether we moved on the current raycast

    // Condition for early termination:
    // return true if it "hit" something (current spot is occupied or the next spot)
    // is outside of the simulation bounds
    auto pmapOccupied = [idx, this](const Vector3T<signed_coord_t> &loc) -> PartSwapBehavior {
        if (REVERSE_BOUNDS_CHECK(loc.x, loc.y, loc.z))
            return PartSwapBehavior::NOOP;
        return eval_move(idx, loc.x, loc.y, loc.z);
    };

    // Repeatedly ray cast until we "run out" of distance
    // Initial distance being the magnitude of the velocity vector
    float portion_velocity = 1.0f;
    float org_dis = util::hypot(part.vx, part.vy, part.vz);

    do {
        hit = raycast(RaycastInput {
            .x = sx, .y = sy, .z = sz,
            .vx = part.vx * portion_velocity,
            .vy = part.vy * portion_velocity,
            .vz = part.vz * portion_velocity,
            .compute_faces = true
        }, out, pmapOccupied);

        if (!hit) break;

        no_move = out.x == sx && out.y == sy && out.z == sz;
        sx = out.x; sy = out.y; sz = out.z;

        auto bounce = GetElements()[part.type].Collision;
        if ((out.faces & RayCast::FACE_X).any()) part.vx *= bounce;
        if ((out.faces & RayCast::FACE_Y).any()) part.vy *= bounce;
        if ((out.faces & RayCast::FACE_Z).any()) part.vz *= bounce;

        // The little velocity we have left is not enough to move to
        // another voxel, so the next raycast is always no_move, terminate early
        // Not exactly accurate but close enough
        if (fabsf(part.vy) < 0.1f && fabsf(part.vx) < 0.1f && fabsf(part.vx) < 0.1f) {
            no_move = true;
            break;
        }

        // We add a small offset since voxels are always 1.0f apart, so we add a small bit to prevent
        // rounding error (optimistically over-consuming distance to avoid extra unnecessary rays)
        portion_velocity -= util::hypot(out.x - sx, out.y - sy, out.z - sz) / org_dis + 0.001f;
    } while(!(hit || no_move || portion_velocity <= 0.01f));

    if (no_move || !hit) {
        try_move(idx,
            util::clampf(part.x + part.vx, 1.0f, XRES - 1.0f),
            util::clampf(part.y + part.vy, 1.0f, YRES - 1.0f),
            util::clampf(part.z + part.vz, 1.0f, ZRES - 1.0f));
    } else {
        try_move(idx, out.x, out.y, out.z, out.move);
    }
}

/**
 * @brief Evaluate the behavior when particle moves to new space
 * 
 * @param idx Particle id to move
 * @param nx New loc
 * @param ny New loc
 * @param nz New loc
 * @return part swap behavior, special cases are resolved
 */
PartSwapBehavior Simulation::eval_move(const part_id idx, const coord_t nx, const coord_t ny, const coord_t nz) const {
    auto other_type = TYP(pmap[nz][ny][nx]);
    if (!other_type) other_type = TYP(photons[nz][ny][nx]);
    if (!other_type) return PartSwapBehavior::SWAP;

    auto this_type = parts[idx].type;

    if (can_move[this_type][other_type] != PartSwapBehavior::SPECIAL)
        return can_move[this_type][other_type];

    // Deal with special cases:
    return PartSwapBehavior::NOOP; // TODO
}
