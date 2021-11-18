#pragma once

#include <stdlib.h>

#include "vec.h"


typedef enum path_type {
    PATHTYPE_line,
    PATHTYPE_bezier,
} PathType;

#define PATH_DEFAULT_CAPACITY 512
typedef struct path {
    PathType    type;
    Vec2        *nodes;
    size_t      node_cnt;
    size_t      capacity;
} Path;

Path* path_init(size_t count);
void path_deinit(Path *path);
void path_resize(Path *path, size_t new_capacity);
void path_addNode(Path *path, Vec2 node);
Vec2* path_getNode(Path *path, int index);
Path* path_fitBezier(Path *path, double scale);
