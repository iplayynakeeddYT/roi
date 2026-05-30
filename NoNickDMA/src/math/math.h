#pragma once
#include <numbers>
#include <cmath>
#include <Windows.h>
#include "../../Utils/SimpleMath.h"

using namespace DirectX::SimpleMath;
extern int width;
extern int height;

class Vec2 {
public:
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    const bool IsZero() const;
    Vec2 operator-(const Vec2& other) const;
    Vec2 operator+(const Vec2& other) const;
    Vec2 operator*(float scalar) const;
    float distance_to(const Vec2& other) const;
};

class Vec3 {
public:
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(const DirectX::SimpleMath::Vector3& other) : x(other.x), y(other.y), z(other.z) {}

    // FIXED: Made world_to_screen const-correct
    bool world_to_screen(DirectX::SimpleMath::Matrix& view_matrix, Vec2& out) const;
    const bool IsZero() const;
    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(float scalar) const;
    float distance_to(const Vec3& other) const;
};