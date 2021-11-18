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
    unsigned    node_cnt;
    unsigned    capacity;
} Path;

Path* path_init(unsigned count);
void path_deinit(Path *path);
void path_resize(Path *path, unsigned new_capacity);
void path_addNode(Path *path, Vec2 node);
Vec2* path_getNode(Path *path, int index);
Path* path_fitBezier(Path *path, double scale);
