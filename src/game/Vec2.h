#pragma once

#include <cmath>

namespace toupie {

/// Petit vecteur 2D en double précision (repère écran : Y vers le bas).
struct Vec2
{
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(double s) const { return {x / s, y / s}; }
    constexpr Vec2 &operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2 &operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2 &operator*=(double s) { x *= s; y *= s; return *this; }

    constexpr double dot(Vec2 o) const { return x * o.x + y * o.y; }
    double length() const { return std::hypot(x, y); }
    constexpr double lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const
    {
        const double len = length();
        return len > 1e-12 ? Vec2{x / len, y / len} : Vec2{};
    }

    /// Vecteur tourné de +90° (sens horaire à l'écran).
    constexpr Vec2 perpendicular() const { return {-y, x}; }

    static Vec2 fromAngle(double radians) { return {std::cos(radians), std::sin(radians)}; }
    double angle() const { return std::atan2(y, x); }
};

constexpr Vec2 operator*(double s, Vec2 v) { return v * s; }

} // namespace toupie
