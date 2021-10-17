#pragma once

typedef struct vec2 {
    double x;
    double y;
} Vec2;

Vec2 vec2_add(Vec2 v0, Vec2 v1);
Vec2 vec2_sub(Vec2 v0, Vec2 v1);
Vec2 vec2_mult(Vec2 v0, Vec2 v1);
Vec2 vec2_scalarMult(Vec2 v, double s);
double vec2_dot(Vec2 v1, Vec2 v2);
double vec2_cross(Vec2 v1, Vec2 v2);
double vec2_distSqr(Vec2 v1, Vec2 v2);
double vec2_dist(Vec2 v1, Vec2 v2);
double vec2_len(Vec2 v);
Vec2 vec2_norm(Vec2 v);
