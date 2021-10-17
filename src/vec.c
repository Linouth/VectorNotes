#include <math.h>

#include "vec.h"

Vec2 vec2_add(Vec2 v0, Vec2 v1) {
    Vec2 vec = {
        .x = v0.x + v1.x,
        .y = v0.y + v1.y,
    };
    return vec;
}

Vec2 vec2_sub(Vec2 v0, Vec2 v1) {
    Vec2 vec = {
        .x = v0.x - v1.x,
        .y = v0.y - v1.y,
    };
    return vec;
}

Vec2 vec2_mult(Vec2 v0, Vec2 v1) {
    Vec2 vec = {
        .x = v0.x * v1.x,
        .y = v0.y * v1.y,
    };
    return vec;
}

Vec2 vec2_scalarMult(Vec2 v, double s) {
    Vec2 vec = {
        .x = v.x * s,
        .y = v.y * s,
    };
    return vec;
}

double vec2_dot(Vec2 v1, Vec2 v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

double vec2_cross(Vec2 v1, Vec2 v2) {
    return v1.x*v2.y - v1.y*v2.x;
}

double vec2_distSqr(Vec2 v1, Vec2 v2) {
    double x = v1.x - v2.x;
    double y = v1.y - v2.y;
    return x*x + y*y;
}

double vec2_dist(Vec2 v1, Vec2 v2) {
    return sqrt(vec2_distSqr(v1, v2));
}

double vec2_len(Vec2 v) {
    return sqrt(vec2_dot(v, v));
}

Vec2 vec2_norm(Vec2 v) {
    double len = vec2_len(v);
    Vec2 out = {
        .x = v.x / len,
        .y = v.y / len,
    };
    return out;
}
