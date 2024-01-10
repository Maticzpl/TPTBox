#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <cmath>
namespace util {
    /**
     * @brief Round a float fast. WARNING: pass in only positive values
     * @param r A POSITIVE float
     * @return constexpr unsigned int 
     */
    constexpr unsigned int roundf(const float r) {
        #ifdef DEBUG
        if (r < 0.0f) throw std::invalid_argument("Input to util::roundf must be non-negative, got " + std::to_string(r));
        #endif
        return (unsigned int)(r + 0.5f);
    }

    /**
     * @brief Clamp a float between two values
     * 
     * @param val Value
     * @param min Min of range
     * @param max Max of range
     * @return constexpr float 
     */
    constexpr float clampf(const float val, const float min, const float max) {
        const float tmp = val < min ? min : val;
        return tmp > max ? max : tmp;
    }

    /**
     * @brief Ceil a float, but round negative values away from 0
     *        This is slightly different from std::ceil which always
     *        rounds in the positive direction
     * 
     * @param v Float value
     * @return constexpr float 
     */
    constexpr int ceil_proper(float v) {
        if (v < 0) return -std::ceil(-v);
        return std::ceil(v);
    }

    /**
     * @brief Return sqrt(x^2 + y^2), faster than std::hypot but
     *        no underflow/overflow protection guarantee
     * @param x 
     * @param y 
     * @return constexpr float 
     */
    constexpr float hypot(float x, float y) {
        return std::sqrt(x * x + y * y);
    }
    constexpr float hypot(float x, float y, float z) {
        return std::sqrt(x * x + y * y + z * z);
    }

    /**
     * @brief Return the index of the largest in [a, b, c]
     * @param a 
     * @param b 
     * @param c 
     * @return constexpr int 
     */
    constexpr int argmax3(float a, float b, float c) {
        a = std::abs(a);
        b = std::abs(b);
        c = std::abs(c);
        int idx = 0;

        if (b > a)
            idx = 1;
        if (c > b && c > a)
            idx = 2;
        return idx;
    }
}

#endif