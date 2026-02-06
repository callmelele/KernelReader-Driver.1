#pragma once
#include <numbers>
#include <cmath>
#include <algorithm>

struct Vector3 {
    float x, y, z;

    constexpr Vector3(float x = 0.f, float y = 0.f, float z = 0.f) noexcept : x(x), y(y), z(z) {}

    // Fixed: Return by value, not reference
    Vector3 operator-(const Vector3& other) const noexcept { return { x - other.x, y - other.y, z - other.z }; }
    Vector3 operator+(const Vector3& other) const noexcept { return { x + other.x, y + other.y, z + other.z }; }
    Vector3 operator*(float f) const noexcept { return { x * f, y * f, z * f }; }
    Vector3 operator/(float f) const noexcept { return { x / f, y / f, z / f }; }

    float DistTo(const Vector3& other) const {
        return std::sqrt(std::pow(other.x - x, 2) + std::pow(other.y - y, 2) + std::pow(other.z - z, 2));
    }

    bool IsZero() const noexcept { return x == 0.f && y == 0.f && z == 0.f; }
};

struct view_matrix_t {
    float matrix[4][4];
    float* operator[](int index) { return matrix[index]; }
};

// Global W2S function
inline bool WorldToScreen(const Vector3& world, Vector3& screen, view_matrix_t vm, int width = 1920, int height = 1080) {
    float w = vm[3][0] * world.x + vm[3][1] * world.y + vm[3][2] * world.z + vm[3][3];
    if (w < 0.001f) return false;

    float x = (vm[0][0] * world.x + vm[0][1] * world.y + vm[0][2] * world.z + vm[0][3]) / w;
    float y = (vm[1][0] * world.x + vm[1][1] * world.y + vm[1][2] * world.z + vm[1][3]) / w;

    screen.x = (width / 2.0f) + (x * width / 2.0f);
    screen.y = (height / 2.0f) - (y * height / 2.0f);
    return true;
}