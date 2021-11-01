#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "path.h"
#include "vec.h"
#include "fit_bezier.h"

const double PI = 3.1415926535897932384626433832795;

Path* path_init(size_t count) {
    Path *path = calloc(1, sizeof(Path));
    assert(path != NULL);

    size_t capacity = count > 0 ? count : PATH_DEFAULT_CAPACITY;
    path->nodes = malloc(sizeof(Vec2) * capacity);
    path->timestamps = malloc(sizeof(double) * capacity);
    path->capacity = capacity;

    assert(path->nodes != NULL);
    assert(path->timestamps != NULL);

    return path;
}

void path_deinit(Path *path) {
    free(path);
}

void path_resize(Path *path, size_t new_capacity) {
    path->nodes = realloc(path->nodes, sizeof(Vec2) * new_capacity);
    path->timestamps = realloc(path->timestamps, sizeof(double) * new_capacity);
    path->capacity = new_capacity;

    assert(path->nodes != NULL);
    assert(path->timestamps != NULL);
}

void path_addNode(Path *path, Vec2 node, double timestamp) {
    if (path->node_cnt >= path->capacity) {
        path_resize(path, path->capacity * 2);
    }

    path->nodes[path->node_cnt] = node;
    path->timestamps[path->node_cnt] = timestamp < 0 ? glfwGetTime() : timestamp;
    path->node_cnt += 1;
}

Vec2* path_getNode(Path *path, int index) {
    int pos = (index < 0) ? (int)path->node_cnt + index : index;
    if (pos < (int)path->node_cnt) {
        return &path->nodes[pos];
    }
    return NULL;
}

Path* path_fitBezier(Path *path, double scale) {
    assert(path->node_cnt > 1);

    // TODO: These parameters will have to depend on how far we are zoomed in,
    // just like the frequency of sampling when drawing.
    BezierFitCtx *fit = fit_init(path->nodes, path->node_cnt);
    fit->timestamps = path->timestamps;
    fit->corner_thresh = PI / 6;
    fit->tangent_range = 20.0 / scale;
    fit->epsilon = 15.0 / scale;
    fit->psi = 60.0 / scale;
    fit->max_iter = 3;

    fitCurve(fit);

    Path *new = path_init(fit->new_cnt);
    memcpy(new->nodes, fit->new, fit->new_cnt * sizeof(Vec2));
    memcpy(new->timestamps, fit->new_ts, fit->new_cnt * sizeof(double));
    new->node_cnt = fit->new_cnt;
    new->capacity = fit->new_cnt;

    fit_deinit(fit);
    return new;
}
